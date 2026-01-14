#include "ThemeController.h"

#include "AppSettings.h"
#include "ThemePackModel.h"
#include "WorkspaceModel.h"

namespace
{
QColor mixWithWhite(const QColor& c, qreal factor)
{
  const qreal f = qBound<qreal>(0.0, factor, 1.0);
  return QColor::fromRgbF(
    c.redF() * (1.0 - f) + 1.0 * f,
    c.greenF() * (1.0 - f) + 1.0 * f,
    c.blueF() * (1.0 - f) + 1.0 * f,
    1.0);
}
}

ThemeController::ThemeController(QObject* parent)
  : QObject(parent)
{
}

void ThemeController::setWorkspaces(WorkspaceModel* workspaces)
{
  if (m_workspaces == workspaces) {
    return;
  }

  if (m_workspaces) {
    disconnect(m_workspaces, nullptr, this, nullptr);
  }

  m_workspaces = workspaces;

  if (m_workspaces) {
    connect(m_workspaces, &WorkspaceModel::activeIndexChanged, this, &ThemeController::refresh);
    connect(m_workspaces, &QAbstractItemModel::dataChanged, this, [this] {
      refresh();
    });
    connect(m_workspaces, &QAbstractItemModel::rowsInserted, this, &ThemeController::refresh);
    connect(m_workspaces, &QAbstractItemModel::rowsRemoved, this, &ThemeController::refresh);
  }

  refresh();
}

void ThemeController::setSettings(AppSettings* settings)
{
  if (m_settings == settings) {
    return;
  }

  if (m_settings) {
    disconnect(m_settings, nullptr, this, nullptr);
  }

  m_settings = settings;
  if (m_settings) {
    connect(m_settings, &AppSettings::themeIdChanged, this, &ThemeController::refresh);
  }

  refresh();
}

void ThemeController::setThemePacks(ThemePackModel* packs)
{
  if (m_packs == packs) {
    return;
  }

  if (m_packs) {
    disconnect(m_packs, nullptr, this, nullptr);
  }

  m_packs = packs;
  if (m_packs) {
    connect(m_packs, &QAbstractItemModel::modelReset, this, &ThemeController::refresh);
    connect(m_packs, &QAbstractItemModel::dataChanged, this, &ThemeController::refresh);
    connect(m_packs, &QAbstractItemModel::rowsInserted, this, &ThemeController::refresh);
    connect(m_packs, &QAbstractItemModel::rowsRemoved, this, &ThemeController::refresh);
  }

  refresh();
}

QColor ThemeController::accentColor() const
{
  return m_accentColor;
}

QColor ThemeController::backgroundFrom() const
{
  return m_backgroundFrom;
}

QColor ThemeController::backgroundTo() const
{
  return m_backgroundTo;
}

int ThemeController::cornerRadius() const
{
  return m_cornerRadius;
}

int ThemeController::spacing() const
{
  return m_spacing;
}

int ThemeController::motionFastMs() const
{
  return 120;
}

int ThemeController::motionNormalMs() const
{
  return 180;
}

int ThemeController::motionSlowMs() const
{
  return 240;
}

void ThemeController::refresh()
{
  const QString themeId = m_settings ? m_settings->themeId() : QStringLiteral("workspace");

  xbrowser::ThemeTokens tokens;
  const bool hasTokens = m_packs && m_packs->tokensForThemeId(themeId, &tokens);

  QColor nextAccent("#6d9eeb");
  if (hasTokens && tokens.accentColor.isValid()) {
    nextAccent = tokens.accentColor;
  } else if (m_workspaces) {
    const QColor ws = m_workspaces->activeAccentColor();
    if (ws.isValid()) {
      nextAccent = ws;
    }
  }

  QColor nextFrom;
  QColor nextTo;
  if (hasTokens && tokens.backgroundFrom.isValid() && tokens.backgroundTo.isValid() && !tokens.useWorkspaceAccent) {
    nextFrom = tokens.backgroundFrom;
    nextTo = tokens.backgroundTo;
  } else {
    nextFrom = mixWithWhite(nextAccent, 0.88);
    nextTo = mixWithWhite(nextAccent.darker(130), 0.92);
  }

  const int nextRadius = qBound(0, hasTokens ? tokens.cornerRadius : 10, 24);
  const int nextSpacing = qBound(0, hasTokens ? tokens.spacing : 8, 32);

  if (m_accentColor == nextAccent && m_backgroundFrom == nextFrom && m_backgroundTo == nextTo
      && m_cornerRadius == nextRadius && m_spacing == nextSpacing) {
    return;
  }

  m_accentColor = nextAccent;
  m_backgroundFrom = nextFrom;
  m_backgroundTo = nextTo;
  m_cornerRadius = nextRadius;
  m_spacing = nextSpacing;
  emit themeChanged();
}
