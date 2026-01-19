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

QColor lerpColor(const QColor& a, const QColor& b, qreal t)
{
  const qreal f = qBound<qreal>(0.0, t, 1.0);
  return QColor::fromRgbF(
    a.redF() + (b.redF() - a.redF()) * f,
    a.greenF() + (b.greenF() - a.greenF()) * f,
    a.blueF() + (b.blueF() - a.blueF()) * f,
    1.0);
}

QColor applyStrength(const QColor& c, int strength)
{
  const int s = qBound(0, strength, 100);
  const qreal normalized = 1.0 - (static_cast<qreal>(s) / 100.0);
  return mixWithWhite(c, normalized * 0.92);
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

QColor ThemeController::backgroundMid() const
{
  return m_backgroundMid;
}

QColor ThemeController::backgroundTo() const
{
  return m_backgroundTo;
}

int ThemeController::backgroundAngle() const
{
  return m_backgroundAngle;
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

  const bool themeUsesWorkspaceAccent = !hasTokens || tokens.useWorkspaceAccent || !tokens.accentColor.isValid();

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
  QColor nextMid;
  QColor nextTo;
  int nextAngle = 0;
  if (!themeUsesWorkspaceAccent && hasTokens && tokens.backgroundFrom.isValid() && tokens.backgroundTo.isValid()) {
    nextFrom = tokens.backgroundFrom;
    nextTo = tokens.backgroundTo;
  } else if (themeUsesWorkspaceAccent && m_workspaces && m_workspaces->hasCustomBackgroundAt(m_workspaces->activeIndex())) {
    const int idx = m_workspaces->activeIndex();
    const QColor wsFrom = m_workspaces->backgroundFromAt(idx);
    const QColor wsTo = m_workspaces->backgroundToAt(idx);
    const int strength = m_workspaces->backgroundStrengthAt(idx);
    const bool hasMid = m_workspaces->hasCustomBackgroundMidAt(idx);

    nextFrom = applyStrength(wsFrom, strength);
    nextTo = applyStrength(wsTo, strength);
    nextMid = hasMid ? applyStrength(m_workspaces->backgroundMidAt(idx), strength) : QColor();
    nextAngle = m_workspaces->backgroundAngleAt(idx);
  } else {
    nextFrom = mixWithWhite(nextAccent, 0.88);
    nextTo = mixWithWhite(nextAccent.darker(130), 0.92);
  }

  if (!nextMid.isValid()) {
    nextMid = lerpColor(nextFrom, nextTo, 0.5);
  }

  const int nextRadius = qBound(0, hasTokens ? tokens.cornerRadius : 10, 24);
  const int nextSpacing = qBound(0, hasTokens ? tokens.spacing : 8, 32);

  if (m_accentColor == nextAccent && m_backgroundFrom == nextFrom && m_backgroundMid == nextMid && m_backgroundTo == nextTo
      && m_backgroundAngle == nextAngle && m_cornerRadius == nextRadius && m_spacing == nextSpacing) {
    return;
  }

  m_accentColor = nextAccent;
  m_backgroundFrom = nextFrom;
  m_backgroundMid = nextMid;
  m_backgroundTo = nextTo;
  m_backgroundAngle = nextAngle;
  m_cornerRadius = nextRadius;
  m_spacing = nextSpacing;
  emit themeChanged();
}
