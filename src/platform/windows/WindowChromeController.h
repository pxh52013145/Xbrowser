#pragma once

#include <QAbstractNativeEventFilter>
#include <QObject>
#include <QPointer>
#include <QVariantList>

class QQuickItem;
class QQuickWindow;

class WindowChromeController : public QObject, public QAbstractNativeEventFilter
{
  Q_OBJECT
  Q_PROPERTY(Button hoveredButton READ hoveredButton NOTIFY hoveredButtonChanged)
  Q_PROPERTY(Button pressedButton READ pressedButton NOTIFY pressedButtonChanged)

public:
  enum Button
  {
    None = 0,
    Minimize,
    Maximize,
    Close
  };
  Q_ENUM(Button)

  explicit WindowChromeController(QObject* parent = nullptr);
  ~WindowChromeController() override;

  Button hoveredButton() const;
  Button pressedButton() const;

  Q_INVOKABLE void attach(QObject* windowObject);

  Q_INVOKABLE void setCaptionItem(QObject* itemObject);
  Q_INVOKABLE void setCaptionExcludeItems(const QVariantList& items);

  Q_INVOKABLE void setMinimizeButtonItem(QObject* itemObject);
  Q_INVOKABLE void setMaximizeButtonItem(QObject* itemObject);
  Q_INVOKABLE void setCloseButtonItem(QObject* itemObject);

  bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;

signals:
  void hoveredButtonChanged();
  void pressedButtonChanged();

private:
  void applyWindowStyles();
  void updateFrameOnResize();
  void showSystemMenu(const QPoint& screenPos);

  QQuickWindow* window() const;

  QRectF itemRectInWindowDips(const QQuickItem* item) const;
  bool pointInItem(const QQuickItem* item, const QPointF& dipPos) const;
  bool pointInAnyExcludeItem(const QPointF& dipPos) const;

  bool handleNcCalcSize(qintptr wParam, qintptr lParam, qintptr* result);
  bool handleGetMinMaxInfo(void* lParam, qintptr* result) const;
  bool handleNcHitTest(qintptr lParam, qintptr* result) const;
  bool handleNcRightButtonUp(qintptr lParam, qintptr wParam, qintptr* result);

  void updateHoveredButtonFromHitTest(qintptr hitTest);
  void clearNonClientTracking();
  void ensureNonClientLeaveTracking();

  void setHoveredButton(Button button);
  void setPressedButton(Button button);
  static Button buttonFromHitTest(qintptr hitTest);

  QPointer<QQuickWindow> m_window;
  bool m_installed = false;

  QPointer<QQuickItem> m_captionItem;
  QList<QPointer<QQuickItem>> m_captionExcludeItems;

  QPointer<QQuickItem> m_minimizeButtonItem;
  QPointer<QQuickItem> m_maximizeButtonItem;
  QPointer<QQuickItem> m_closeButtonItem;

  Button m_hoveredButton = None;
  Button m_pressedButton = None;
  bool m_trackingNonClientLeave = false;
};
