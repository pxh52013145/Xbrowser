#pragma once

#include <QObject>

#include <QColor>
#include <QPointer>

class AppSettings;
class ThemePackModel;
class WorkspaceModel;

class ThemeController : public QObject
{
  Q_OBJECT

  Q_PROPERTY(QColor accentColor READ accentColor NOTIFY themeChanged)
  Q_PROPERTY(QColor backgroundFrom READ backgroundFrom NOTIFY themeChanged)
  Q_PROPERTY(QColor backgroundTo READ backgroundTo NOTIFY themeChanged)
  Q_PROPERTY(int cornerRadius READ cornerRadius NOTIFY themeChanged)
  Q_PROPERTY(int spacing READ spacing NOTIFY themeChanged)
  Q_PROPERTY(int motionFastMs READ motionFastMs CONSTANT)
  Q_PROPERTY(int motionNormalMs READ motionNormalMs CONSTANT)
  Q_PROPERTY(int motionSlowMs READ motionSlowMs CONSTANT)

public:
  explicit ThemeController(QObject* parent = nullptr);

  void setWorkspaces(WorkspaceModel* workspaces);
  void setSettings(AppSettings* settings);
  void setThemePacks(ThemePackModel* packs);

  QColor accentColor() const;
  QColor backgroundFrom() const;
  QColor backgroundTo() const;
  int cornerRadius() const;
  int spacing() const;
  int motionFastMs() const;
  int motionNormalMs() const;
  int motionSlowMs() const;

signals:
  void themeChanged();

private:
  void refresh();

  QPointer<WorkspaceModel> m_workspaces;
  QPointer<AppSettings> m_settings;
  QPointer<ThemePackModel> m_packs;
  QColor m_accentColor;
  QColor m_backgroundFrom;
  QColor m_backgroundTo;
  int m_cornerRadius = 10;
  int m_spacing = 8;
};
