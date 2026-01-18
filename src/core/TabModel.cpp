#include "TabModel.h"

#include "AppPaths.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUrlQuery>

#include <algorithm>

namespace
{
bool isUnderDataRoot(const QString& absoluteFilePath)
{
  if (absoluteFilePath.trimmed().isEmpty()) {
    return false;
  }

  const QString rootPath = QDir::fromNativeSeparators(QDir(xbrowser::appDataRoot()).absolutePath());
  QString root = QDir::cleanPath(rootPath);
  if (!root.endsWith('/')) {
    root += '/';
  }

  const QString candidate =
    QDir::cleanPath(QDir::fromNativeSeparators(QFileInfo(absoluteFilePath).absoluteFilePath()));
  return candidate.startsWith(root, Qt::CaseInsensitive);
}

void removeFileIfSafe(const QString& path)
{
  if (path.trimmed().isEmpty()) {
    return;
  }
  if (!isUnderDataRoot(path)) {
    return;
  }
  QFile::remove(path);
}

QUrl thumbnailUrlFor(const QString& path, int version)
{
  if (path.trimmed().isEmpty()) {
    return {};
  }

  QUrl url = QUrl::fromLocalFile(path);
  QUrlQuery query;
  query.addQueryItem(QStringLiteral("t"), QString::number(qMax(0, version)));
  url.setQuery(query);
  return url;
}
}

TabModel::TabModel(QObject* parent)
  : QAbstractListModel(parent)
{
}

int TabModel::rowCount(const QModelIndex& parent) const
{
  if (parent.isValid()) {
    return 0;
  }
  return m_tabs.size();
}

QVariant TabModel::data(const QModelIndex& index, int role) const
{
  if (!index.isValid()) {
    return {};
  }

  const int row = index.row();
  if (row < 0 || row >= m_tabs.size()) {
    return {};
  }

  const auto& tab = m_tabs[row];
  switch (role) {
    case TabIdRole:
      return tab.id;
    case TitleRole:
      return tab.customTitle.isEmpty() ? tab.pageTitle : tab.customTitle;
    case CustomTitleRole:
      return tab.customTitle;
    case UrlRole:
      return tab.url;
    case IsActiveRole:
      return row == m_activeIndex;
    case IsEssentialRole:
      return tab.essential;
    case GroupIdRole:
      return tab.groupId;
    case FaviconUrlRole:
      return tab.faviconUrl;
    case ThumbnailUrlRole:
      return thumbnailUrlFor(tab.thumbnailPath, tab.thumbnailVersion);
    case IsLoadingRole:
      return tab.isLoading;
    case IsAudioPlayingRole:
      return tab.isAudioPlaying;
    case IsMutedRole:
      return tab.isMuted;
    case LastActivatedMsRole:
      return tab.lastActivatedMs;
    case IsSelectedRole:
      return m_selectedTabIds.contains(tab.id);
    default:
      return {};
  }
}

QHash<int, QByteArray> TabModel::roleNames() const
{
  return {
    {TabIdRole, "tabId"},
    {TitleRole, "title"},
    {CustomTitleRole, "customTitle"},
    {UrlRole, "url"},
    {IsActiveRole, "isActive"},
    {IsEssentialRole, "isEssential"},
    {GroupIdRole, "groupId"},
    {FaviconUrlRole, "faviconUrl"},
    {ThumbnailUrlRole, "thumbnailUrl"},
    {IsLoadingRole, "isLoading"},
    {IsAudioPlayingRole, "isAudioPlaying"},
    {IsMutedRole, "isMuted"},
    {LastActivatedMsRole, "lastActivatedMs"},
    {IsSelectedRole, "isSelected"},
  };
}

int TabModel::activeIndex() const
{
  return m_activeIndex;
}

void TabModel::setActiveIndex(int index)
{
  if (index < 0 || index >= m_tabs.size()) {
    index = m_tabs.isEmpty() ? -1 : 0;
  }

  if (m_activeIndex == index) {
    return;
  }

  const int oldIndex = m_activeIndex;
  m_activeIndex = index;
  if (m_activeIndex >= 0 && m_activeIndex < m_tabs.size()) {
    m_tabs[m_activeIndex].lastActivatedMs = QDateTime::currentMSecsSinceEpoch();
  }
  emit activeIndexChanged();

  if (oldIndex >= 0 && oldIndex < m_tabs.size()) {
    emit dataChanged(this->index(oldIndex), this->index(oldIndex), {IsActiveRole});
  }
  if (m_activeIndex >= 0 && m_activeIndex < m_tabs.size()) {
    emit dataChanged(this->index(m_activeIndex), this->index(m_activeIndex), {IsActiveRole, LastActivatedMsRole});
  }
}

int TabModel::selectedCount() const
{
  return m_selectedTabIds.size();
}

bool TabModel::hasSelection() const
{
  return !m_selectedTabIds.isEmpty();
}

int TabModel::count() const
{
  return m_tabs.size();
}

int TabModel::addTab(const QUrl& url)
{
  return addTabWithId(0, url, QStringLiteral("New Tab"), true);
}

int TabModel::addTabWithId(int tabId, const QUrl& url, const QString& title, bool makeActive)
{
  const int index = m_tabs.size();
  beginInsertRows(QModelIndex(), index, index);

  TabEntry entry;
  entry.id = tabId > 0 ? tabId : m_nextId++;
  entry.url = url;
  entry.initialUrl = url;
  const QString trimmed = title.trimmed();
  entry.pageTitle = trimmed.isEmpty() ? QStringLiteral("New Tab") : trimmed;
  entry.lastActivatedMs = makeActive ? QDateTime::currentMSecsSinceEpoch() : 0;
  m_tabs.push_back(entry);

  if (entry.id >= m_nextId) {
    m_nextId = entry.id + 1;
  }

  endInsertRows();

  if (makeActive) {
    setActiveIndex(index);
  }

  return index;
}

void TabModel::closeTab(int index)
{
  removeTabInternal(index, true);
}

void TabModel::removeTab(int index)
{
  removeTabInternal(index, false);
}

void TabModel::clear()
{
  if (m_tabs.isEmpty() && m_activeIndex == -1 && m_nextId == 1 && m_selectedTabIds.isEmpty()) {
    return;
  }

  const bool hadSelection = !m_selectedTabIds.isEmpty();

  beginResetModel();
  for (const auto& tab : m_tabs) {
    removeFileIfSafe(tab.thumbnailPath);
  }
  for (const auto& tab : m_closedTabs) {
    removeFileIfSafe(tab.thumbnailPath);
  }
  m_tabs.clear();
  m_closedTabs.clear();
  m_selectedTabIds.clear();
  m_activeIndex = -1;
  m_nextId = 1;
  endResetModel();
  emit activeIndexChanged();
  if (hadSelection) {
    emit selectionChanged();
  }
}

void TabModel::moveTab(int fromIndex, int toIndex)
{
  if (fromIndex < 0 || fromIndex >= m_tabs.size() || toIndex < 0 || toIndex >= m_tabs.size()) {
    return;
  }
  if (fromIndex == toIndex) {
    return;
  }

  const int activeTabId = (m_activeIndex >= 0 && m_activeIndex < m_tabs.size()) ? m_tabs[m_activeIndex].id : 0;

  const int destination = fromIndex < toIndex ? toIndex + 1 : toIndex;
  beginMoveRows(QModelIndex(), fromIndex, fromIndex, QModelIndex(), destination);
  m_tabs.move(fromIndex, toIndex);
  endMoveRows();

  if (activeTabId > 0) {
    const int nextActive = indexOfTabId(activeTabId);
    if (nextActive != m_activeIndex) {
      m_activeIndex = nextActive;
      emit activeIndexChanged();
    }

    if (!m_tabs.isEmpty()) {
      emit dataChanged(index(0), index(m_tabs.size() - 1), {IsActiveRole});
    }
  }
}

int TabModel::tabIdAt(int index) const
{
  if (index < 0 || index >= m_tabs.size()) {
    return 0;
  }
  return m_tabs[index].id;
}

int TabModel::indexOfTabId(int tabId) const
{
  if (tabId <= 0) {
    return -1;
  }

  for (int i = 0; i < m_tabs.size(); ++i) {
    if (m_tabs[i].id == tabId) {
      return i;
    }
  }
  return -1;
}

bool TabModel::isSelectedById(int tabId) const
{
  if (tabId <= 0 || indexOfTabId(tabId) < 0) {
    return false;
  }
  return m_selectedTabIds.contains(tabId);
}

void TabModel::setSelectedById(int tabId, bool selected)
{
  if (tabId <= 0) {
    return;
  }

  const int row = indexOfTabId(tabId);
  if (row < 0) {
    return;
  }

  const bool has = m_selectedTabIds.contains(tabId);
  if (has == selected) {
    return;
  }

  if (selected) {
    m_selectedTabIds.insert(tabId);
  } else {
    m_selectedTabIds.remove(tabId);
  }

  emit dataChanged(index(row), index(row), {IsSelectedRole});
  emit selectionChanged();
}

void TabModel::toggleSelectedById(int tabId)
{
  setSelectedById(tabId, !isSelectedById(tabId));
}

void TabModel::clearSelection()
{
  if (m_selectedTabIds.isEmpty()) {
    return;
  }

  m_selectedTabIds.clear();
  if (!m_tabs.isEmpty()) {
    emit dataChanged(index(0), index(m_tabs.size() - 1), {IsSelectedRole});
  }
  emit selectionChanged();
}

void TabModel::selectOnlyById(int tabId)
{
  if (tabId <= 0) {
    return;
  }
  if (indexOfTabId(tabId) < 0) {
    return;
  }

  if (m_selectedTabIds.size() == 1 && m_selectedTabIds.contains(tabId)) {
    return;
  }

  m_selectedTabIds.clear();
  m_selectedTabIds.insert(tabId);

  if (!m_tabs.isEmpty()) {
    emit dataChanged(index(0), index(m_tabs.size() - 1), {IsSelectedRole});
  }
  emit selectionChanged();
}

void TabModel::setSelectionByIds(const QVariantList& tabIds, bool clearOthers)
{
  QSet<int> next = clearOthers ? QSet<int>() : m_selectedTabIds;

  for (const QVariant& v : tabIds) {
    bool ok = false;
    const int tabId = v.toInt(&ok);
    if (!ok || tabId <= 0) {
      continue;
    }
    if (indexOfTabId(tabId) < 0) {
      continue;
    }
    next.insert(tabId);
  }

  if (next == m_selectedTabIds) {
    return;
  }

  m_selectedTabIds = next;
  if (!m_tabs.isEmpty()) {
    emit dataChanged(index(0), index(m_tabs.size() - 1), {IsSelectedRole});
  }
  emit selectionChanged();
}

QVariantList TabModel::selectedTabIds() const
{
  QVariantList ids;
  ids.reserve(m_selectedTabIds.size());

  for (const auto& tab : m_tabs) {
    if (m_selectedTabIds.contains(tab.id)) {
      ids.push_back(tab.id);
    }
  }

  return ids;
}

QUrl TabModel::urlAt(int index) const
{
  if (index < 0 || index >= m_tabs.size()) {
    return {};
  }
  return m_tabs[index].url;
}

QUrl TabModel::initialUrlAt(int index) const
{
  if (index < 0 || index >= m_tabs.size()) {
    return {};
  }
  return m_tabs[index].initialUrl;
}

QString TabModel::titleAt(int index) const
{
  if (index < 0 || index >= m_tabs.size()) {
    return {};
  }
  const auto& tab = m_tabs[index];
  return tab.customTitle.isEmpty() ? tab.pageTitle : tab.customTitle;
}

QString TabModel::pageTitleAt(int index) const
{
  if (index < 0 || index >= m_tabs.size()) {
    return {};
  }
  return m_tabs[index].pageTitle;
}

QString TabModel::customTitleAt(int index) const
{
  if (index < 0 || index >= m_tabs.size()) {
    return {};
  }
  return m_tabs[index].customTitle;
}

bool TabModel::isEssentialAt(int index) const
{
  if (index < 0 || index >= m_tabs.size()) {
    return false;
  }
  return m_tabs[index].essential;
}

void TabModel::setEssentialAt(int index, bool essential)
{
  if (index < 0 || index >= m_tabs.size()) {
    return;
  }

  auto& tab = m_tabs[index];
  if (tab.essential == essential) {
    return;
  }

  tab.essential = essential;
  if (tab.essential && tab.initialUrl.isEmpty()) {
    tab.initialUrl = tab.url;
  }
  emit dataChanged(this->index(index), this->index(index), {IsEssentialRole});
}

int TabModel::groupIdAt(int index) const
{
  if (index < 0 || index >= m_tabs.size()) {
    return 0;
  }
  return m_tabs[index].groupId;
}

void TabModel::setGroupIdAt(int index, int groupId)
{
  if (index < 0 || index >= m_tabs.size()) {
    return;
  }

  const int next = qMax(0, groupId);

  auto& tab = m_tabs[index];
  if (tab.groupId == next) {
    return;
  }

  tab.groupId = next;
  emit dataChanged(this->index(index), this->index(index), {GroupIdRole});
}

QUrl TabModel::faviconUrlAt(int index) const
{
  if (index < 0 || index >= m_tabs.size()) {
    return {};
  }
  return m_tabs[index].faviconUrl;
}

void TabModel::setFaviconUrlAt(int index, const QUrl& url)
{
  if (index < 0 || index >= m_tabs.size()) {
    return;
  }

  auto& tab = m_tabs[index];
  if (tab.faviconUrl == url) {
    return;
  }

  tab.faviconUrl = url;
  emit dataChanged(this->index(index), this->index(index), {FaviconUrlRole});
}

QUrl TabModel::thumbnailUrlAt(int index) const
{
  if (index < 0 || index >= m_tabs.size()) {
    return {};
  }

  const auto& tab = m_tabs[index];
  return thumbnailUrlFor(tab.thumbnailPath, tab.thumbnailVersion);
}

void TabModel::setThumbnailPathById(int tabId, const QString& filePath)
{
  if (tabId <= 0) {
    return;
  }

  const int index = indexOfTabId(tabId);
  if (index < 0 || index >= m_tabs.size()) {
    return;
  }

  auto& tab = m_tabs[index];
  const QString trimmed = filePath.trimmed();

  if (!trimmed.isEmpty() && tab.thumbnailPath != trimmed) {
    removeFileIfSafe(tab.thumbnailPath);
    tab.thumbnailPath = trimmed;
  } else if (trimmed.isEmpty() && !tab.thumbnailPath.isEmpty()) {
    removeFileIfSafe(tab.thumbnailPath);
    tab.thumbnailPath.clear();
  }

  tab.thumbnailVersion++;
  tab.thumbnailLastUsedMs = QDateTime::currentMSecsSinceEpoch();

  enforceThumbnailLimit(tabId);
  emit dataChanged(this->index(index), this->index(index), {ThumbnailUrlRole});
}

void TabModel::markThumbnailUsedById(int tabId)
{
  if (tabId <= 0) {
    return;
  }

  const int index = indexOfTabId(tabId);
  if (index < 0 || index >= m_tabs.size()) {
    return;
  }

  auto& tab = m_tabs[index];
  if (tab.thumbnailPath.isEmpty()) {
    return;
  }

  tab.thumbnailLastUsedMs = QDateTime::currentMSecsSinceEpoch();
  enforceThumbnailLimit(tabId);
}

bool TabModel::isLoadingAt(int index) const
{
  if (index < 0 || index >= m_tabs.size()) {
    return false;
  }
  return m_tabs[index].isLoading;
}

void TabModel::setLoadingAt(int index, bool loading)
{
  if (index < 0 || index >= m_tabs.size()) {
    return;
  }

  auto& tab = m_tabs[index];
  if (tab.isLoading == loading) {
    return;
  }

  tab.isLoading = loading;
  emit dataChanged(this->index(index), this->index(index), {IsLoadingRole});
}

bool TabModel::isAudioPlayingAt(int index) const
{
  if (index < 0 || index >= m_tabs.size()) {
    return false;
  }
  return m_tabs[index].isAudioPlaying;
}

void TabModel::setAudioPlayingAt(int index, bool playing)
{
  if (index < 0 || index >= m_tabs.size()) {
    return;
  }

  auto& tab = m_tabs[index];
  if (tab.isAudioPlaying == playing) {
    return;
  }

  tab.isAudioPlaying = playing;
  emit dataChanged(this->index(index), this->index(index), {IsAudioPlayingRole});
}

bool TabModel::isMutedAt(int index) const
{
  if (index < 0 || index >= m_tabs.size()) {
    return false;
  }
  return m_tabs[index].isMuted;
}

void TabModel::setMutedAt(int index, bool muted)
{
  if (index < 0 || index >= m_tabs.size()) {
    return;
  }

  auto& tab = m_tabs[index];
  if (tab.isMuted == muted) {
    return;
  }

  tab.isMuted = muted;
  emit dataChanged(this->index(index), this->index(index), {IsMutedRole});
}

qint64 TabModel::lastActivatedMsAt(int index) const
{
  if (index < 0 || index >= m_tabs.size()) {
    return 0;
  }
  return m_tabs[index].lastActivatedMs;
}

void TabModel::setUrlAt(int index, const QUrl& url)
{
  if (index < 0 || index >= m_tabs.size()) {
    return;
  }

  auto& tab = m_tabs[index];
  if (tab.url == url) {
    return;
  }

  tab.url = url;
  emit dataChanged(this->index(index), this->index(index), {UrlRole});
}

void TabModel::setInitialUrlAt(int index, const QUrl& url)
{
  if (index < 0 || index >= m_tabs.size()) {
    return;
  }

  auto& tab = m_tabs[index];
  if (tab.initialUrl == url) {
    return;
  }

  tab.initialUrl = url;
}

void TabModel::setTitleAt(int index, const QString& title)
{
  if (index < 0 || index >= m_tabs.size()) {
    return;
  }

  auto& tab = m_tabs[index];
  const QString trimmed = title.trimmed();
  const QString nextTitle = trimmed.isEmpty() ? QStringLiteral("New Tab") : trimmed;

  if (tab.pageTitle == nextTitle) {
    return;
  }

  tab.pageTitle = nextTitle;
  emit dataChanged(this->index(index), this->index(index), {TitleRole});
}

void TabModel::setCustomTitleAt(int index, const QString& title)
{
  if (index < 0 || index >= m_tabs.size()) {
    return;
  }

  auto& tab = m_tabs[index];
  const QString trimmed = title.trimmed();
  const QString nextTitle = trimmed.isEmpty() ? QString() : trimmed;

  if (tab.customTitle == nextTitle) {
    return;
  }

  tab.customTitle = nextTitle;
  emit dataChanged(this->index(index), this->index(index), {TitleRole, CustomTitleRole});
}

bool TabModel::canRestoreLastClosedTab() const
{
  return !m_closedTabs.isEmpty();
}

int TabModel::restoreLastClosedTab()
{
  if (m_closedTabs.isEmpty()) {
    return -1;
  }

  const TabEntry entry = m_closedTabs.takeLast();
  const int idx = addTabWithId(0, entry.url, entry.pageTitle, true);
  setInitialUrlAt(idx, entry.initialUrl.isValid() ? entry.initialUrl : entry.url);
  setCustomTitleAt(idx, entry.customTitle);
  setEssentialAt(idx, entry.essential);
  setGroupIdAt(idx, entry.groupId);
  setFaviconUrlAt(idx, entry.faviconUrl);
  setLoadingAt(idx, false);
  setAudioPlayingAt(idx, false);
  setMutedAt(idx, false);
  return idx;
}

void TabModel::removeTabInternal(int index, bool recordClosed)
{
  if (index < 0 || index >= m_tabs.size()) {
    return;
  }

  removeFileIfSafe(m_tabs[index].thumbnailPath);

  const int removedTabId = m_tabs[index].id;
  const bool selectionWasChanged = m_selectedTabIds.contains(removedTabId);
  m_selectedTabIds.remove(removedTabId);

  if (recordClosed) {
    if (m_closedTabs.size() >= 20) {
      m_closedTabs.removeFirst();
    }
    m_closedTabs.push_back(m_tabs[index]);
  }

  beginRemoveRows(QModelIndex(), index, index);
  m_tabs.removeAt(index);
  endRemoveRows();

  if (selectionWasChanged) {
    emit selectionChanged();
  }

  updateActiveIndexAfterClose(index);
}

void TabModel::updateActiveIndexAfterClose(int closedIndex)
{
  if (m_tabs.isEmpty()) {
    setActiveIndex(-1);
    return;
  }

  if (m_activeIndex == closedIndex) {
    setActiveIndex(qMin(closedIndex, m_tabs.size() - 1));
    return;
  }

  if (closedIndex < m_activeIndex) {
    setActiveIndex(m_activeIndex - 1);
  }
}

void TabModel::enforceThumbnailLimit(int keepTabId)
{
  QVector<int> withThumb;
  withThumb.reserve(m_tabs.size());

  for (int i = 0; i < m_tabs.size(); ++i) {
    if (!m_tabs[i].thumbnailPath.isEmpty()) {
      withThumb.push_back(i);
    }
  }

  if (withThumb.size() <= kMaxThumbnails) {
    return;
  }

  std::sort(withThumb.begin(), withThumb.end(), [this, keepTabId](int a, int b) {
    const auto& ta = m_tabs[a];
    const auto& tb = m_tabs[b];

    const bool aKeep = keepTabId > 0 && ta.id == keepTabId;
    const bool bKeep = keepTabId > 0 && tb.id == keepTabId;
    if (aKeep != bKeep) {
      return !aKeep;
    }

    if (ta.thumbnailLastUsedMs != tb.thumbnailLastUsedMs) {
      return ta.thumbnailLastUsedMs < tb.thumbnailLastUsedMs;
    }
    return ta.id < tb.id;
  });

  const int toEvict = withThumb.size() - kMaxThumbnails;
  for (int i = 0; i < toEvict; ++i) {
    const int index = withThumb[i];
    auto& tab = m_tabs[index];
    if (keepTabId > 0 && tab.id == keepTabId) {
      continue;
    }

    removeFileIfSafe(tab.thumbnailPath);
    tab.thumbnailPath.clear();
    tab.thumbnailVersion++;
    tab.thumbnailLastUsedMs = 0;
    emit dataChanged(this->index(index), this->index(index), {ThumbnailUrlRole});
  }
}
