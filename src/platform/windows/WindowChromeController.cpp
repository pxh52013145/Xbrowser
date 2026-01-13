#include "WindowChromeController.h"

#include <Windows.h>
#include <windowsx.h>

#include <QGuiApplication>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScreen>

namespace
{
int resizeBorderPx()
{
  const int frameX = GetSystemMetrics(SM_CXSIZEFRAME);
  const int frameY = GetSystemMetrics(SM_CYSIZEFRAME);
  const int padded = GetSystemMetrics(SM_CXPADDEDBORDER);
  return qMax(4, qMax(frameX, frameY) + padded);
}

HWND hwndForWindow(const QQuickWindow* window)
{
  return window ? reinterpret_cast<HWND>(window->winId()) : nullptr;
}

QPoint toPoint(LPARAM lParam)
{
  return QPoint(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
}
}

WindowChromeController::WindowChromeController(QObject* parent)
  : QObject(parent)
{
}

WindowChromeController::~WindowChromeController()
{
  if (m_installed) {
    QGuiApplication::instance()->removeNativeEventFilter(this);
  }
}

WindowChromeController::Button WindowChromeController::hoveredButton() const
{
  return m_hoveredButton;
}

WindowChromeController::Button WindowChromeController::pressedButton() const
{
  return m_pressedButton;
}

void WindowChromeController::attach(QObject* windowObject)
{
  QQuickWindow* nextWindow = qobject_cast<QQuickWindow*>(windowObject);
  if (!nextWindow) {
    return;
  }

  if (m_window == nextWindow) {
    applyWindowStyles();
    return;
  }

  m_window = nextWindow;

  if (!m_installed) {
    QGuiApplication::instance()->installNativeEventFilter(this);
    m_installed = true;
  }

  connect(m_window, &QQuickWindow::visibleChanged, this, &WindowChromeController::applyWindowStyles);
  connect(m_window, &QQuickWindow::windowStateChanged, this, &WindowChromeController::updateFrameOnResize);
  connect(m_window, &QQuickWindow::widthChanged, this, &WindowChromeController::updateFrameOnResize);
  connect(m_window, &QQuickWindow::heightChanged, this, &WindowChromeController::updateFrameOnResize);

  applyWindowStyles();
}

void WindowChromeController::setCaptionItem(QObject* itemObject)
{
  m_captionItem = qobject_cast<QQuickItem*>(itemObject);
}

void WindowChromeController::setCaptionExcludeItems(const QVariantList& items)
{
  m_captionExcludeItems.clear();
  m_captionExcludeItems.reserve(items.size());

  for (const QVariant& v : items) {
    QObject* obj = v.value<QObject*>();
    QQuickItem* item = qobject_cast<QQuickItem*>(obj);
    if (item) {
      m_captionExcludeItems.push_back(item);
    }
  }
}

void WindowChromeController::setMinimizeButtonItem(QObject* itemObject)
{
  m_minimizeButtonItem = qobject_cast<QQuickItem*>(itemObject);
}

void WindowChromeController::setMaximizeButtonItem(QObject* itemObject)
{
  m_maximizeButtonItem = qobject_cast<QQuickItem*>(itemObject);
}

void WindowChromeController::setCloseButtonItem(QObject* itemObject)
{
  m_closeButtonItem = qobject_cast<QQuickItem*>(itemObject);
}

bool WindowChromeController::nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result)
{
  if (eventType != "windows_generic_MSG" && eventType != "windows_dispatcher_MSG") {
    return false;
  }

  const QQuickWindow* w = window();
  const HWND hwnd = hwndForWindow(w);
  if (!hwnd) {
    return false;
  }

  MSG* msg = static_cast<MSG*>(message);
  if (!msg || msg->hwnd != hwnd) {
    return false;
  }

  switch (msg->message) {
    case WM_NCCALCSIZE:
      return handleNcCalcSize(static_cast<qintptr>(msg->wParam), static_cast<qintptr>(msg->lParam), result);
    case WM_GETMINMAXINFO:
      return handleGetMinMaxInfo(reinterpret_cast<void*>(msg->lParam), result);
    case WM_NCHITTEST:
      return handleNcHitTest(static_cast<qintptr>(msg->lParam), result);
    case WM_NCRBUTTONUP:
      return handleNcRightButtonUp(static_cast<qintptr>(msg->lParam), static_cast<qintptr>(msg->wParam), result);
    case WM_NCMOUSEMOVE:
      updateHoveredButtonFromHitTest(static_cast<qintptr>(msg->wParam));
      return false;
    case WM_NCMOUSELEAVE:
      clearNonClientTracking();
      return false;
    case WM_NCLBUTTONDOWN: {
      const Button b = buttonFromHitTest(static_cast<qintptr>(msg->wParam));
      if (b == None) {
        return false;
      }

      setPressedButton(b);
      if (result) {
        *result = 0;
      }
      return true;
    }
    case WM_NCLBUTTONUP: {
      const Button b = buttonFromHitTest(static_cast<qintptr>(msg->wParam));
      if (b == None) {
        setPressedButton(None);
        return false;
      }

      setPressedButton(None);

      const WPARAM command = [hwnd, b]() -> WPARAM {
        switch (b) {
          case Minimize:
            return SC_MINIMIZE;
          case Maximize:
            return IsZoomed(hwnd) ? SC_RESTORE : SC_MAXIMIZE;
          case Close:
            return SC_CLOSE;
          default:
            return 0;
        }
      }();

      if (command != 0) {
        PostMessageW(hwnd, WM_SYSCOMMAND, command, 0);
      }

      if (result) {
        *result = 0;
      }
      return true;
    }
    case WM_CAPTURECHANGED:
      setPressedButton(None);
      return false;
    case WM_MOUSEMOVE:
      if (m_hoveredButton != None) {
        setHoveredButton(None);
      }
      return false;
    default:
      return false;
  }
}

void WindowChromeController::applyWindowStyles()
{
  const QQuickWindow* w = window();
  const HWND hwnd = hwndForWindow(w);
  if (!hwnd) {
    return;
  }

  LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
  const LONG_PTR desired =
    style | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_CAPTION;

  if (style != desired) {
    style = desired;
    SetWindowLongPtr(hwnd, GWL_STYLE, style);
    SetWindowPos(
      hwnd,
      nullptr,
      0,
      0,
      0,
      0,
      SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
  }

  updateFrameOnResize();
}

void WindowChromeController::updateFrameOnResize()
{
  const QQuickWindow* w = window();
  const HWND hwnd = hwndForWindow(w);
  if (!hwnd) {
    return;
  }

  SetWindowPos(
    hwnd,
    nullptr,
    0,
    0,
    0,
    0,
    SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
}

void WindowChromeController::showSystemMenu(const QPoint& screenPos)
{
  const QQuickWindow* w = window();
  const HWND hwnd = hwndForWindow(w);
  if (!hwnd) {
    return;
  }

  HMENU menu = GetSystemMenu(hwnd, FALSE);
  if (!menu) {
    return;
  }

  const UINT cmd = TrackPopupMenu(
    menu,
    TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY,
    screenPos.x(),
    screenPos.y(),
    0,
    hwnd,
    nullptr);

  if (cmd != 0) {
    PostMessageW(hwnd, WM_SYSCOMMAND, cmd, 0);
  }
}

QQuickWindow* WindowChromeController::window() const
{
  return m_window.data();
}

QRectF WindowChromeController::itemRectInWindowDips(const QQuickItem* item) const
{
  if (!item) {
    return {};
  }
  const QPointF topLeft = item->mapToScene(QPointF(0, 0));
  return QRectF(topLeft, QSizeF(item->width(), item->height()));
}

bool WindowChromeController::pointInItem(const QQuickItem* item, const QPointF& dipPos) const
{
  if (!item || !item->isVisible() || item->width() <= 0.0 || item->height() <= 0.0) {
    return false;
  }
  return itemRectInWindowDips(item).contains(dipPos);
}

bool WindowChromeController::pointInAnyExcludeItem(const QPointF& dipPos) const
{
  for (const auto& item : m_captionExcludeItems) {
    if (pointInItem(item, dipPos)) {
      return true;
    }
  }
  return false;
}

bool WindowChromeController::handleNcCalcSize(qintptr wParam, qintptr lParam, qintptr* result)
{
  if (!result) {
    return false;
  }

  RECT* rectPtr = nullptr;
  NCCALCSIZE_PARAMS* params = nullptr;
  if (wParam) {
    params = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);
    if (!params) {
      return false;
    }
    rectPtr = &params->rgrc[0];
  } else {
    rectPtr = reinterpret_cast<RECT*>(lParam);
    if (!rectPtr) {
      return false;
    }
  }

  RECT& r = *rectPtr;

  const QQuickWindow* w = window();
  const HWND hwnd = hwndForWindow(w);
  if (hwnd && IsZoomed(hwnd)) {
    const int border = resizeBorderPx();
    r.left += border;
    r.top += border;
    r.right -= border;
    r.bottom -= border;
  }

  *result = 0;
  return true;
}

bool WindowChromeController::handleGetMinMaxInfo(void* lParam, qintptr* result) const
{
  if (!result) {
    return false;
  }

  auto* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
  if (!mmi) {
    return false;
  }

  const QQuickWindow* w = window();
  const HWND hwnd = hwndForWindow(w);
  if (!hwnd) {
    return false;
  }

  HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
  if (!monitor) {
    return false;
  }

  MONITORINFO mi{};
  mi.cbSize = sizeof(mi);
  if (!GetMonitorInfoW(monitor, &mi)) {
    return false;
  }

  const RECT work = mi.rcWork;
  const RECT monitorRect = mi.rcMonitor;

  mmi->ptMaxPosition.x = work.left - monitorRect.left;
  mmi->ptMaxPosition.y = work.top - monitorRect.top;
  mmi->ptMaxSize.x = work.right - work.left;
  mmi->ptMaxSize.y = work.bottom - work.top;

  *result = 0;
  return true;
}

bool WindowChromeController::handleNcHitTest(qintptr lParam, qintptr* result) const
{
  if (!result) {
    return false;
  }

  const QQuickWindow* w = window();
  const HWND hwnd = hwndForWindow(w);
  if (!w || !hwnd) {
    return false;
  }

  const QPoint screen = toPoint(static_cast<LPARAM>(lParam));
  POINT pt{ screen.x(), screen.y() };
  if (!ScreenToClient(hwnd, &pt)) {
    return false;
  }

  const qreal dpr = w->devicePixelRatio();
  const QPointF dipPos(pt.x / dpr, pt.y / dpr);

  if (pointInItem(m_closeButtonItem, dipPos)) {
    *result = HTCLOSE;
    return true;
  }
  if (pointInItem(m_maximizeButtonItem, dipPos)) {
    *result = HTMAXBUTTON;
    return true;
  }
  if (pointInItem(m_minimizeButtonItem, dipPos)) {
    *result = HTMINBUTTON;
    return true;
  }

  const RECT client = [] (HWND hwnd) {
    RECT r{};
    GetClientRect(hwnd, &r);
    return r;
  }(hwnd);

  const int border = resizeBorderPx();
  const bool canResize = !IsZoomed(hwnd);

  if (canResize) {
    const bool left = pt.x >= client.left && pt.x < client.left + border;
    const bool right = pt.x <= client.right && pt.x > client.right - border;
    const bool top = pt.y >= client.top && pt.y < client.top + border;
    const bool bottom = pt.y <= client.bottom && pt.y > client.bottom - border;

    if (top && left) {
      *result = HTTOPLEFT;
      return true;
    }
    if (top && right) {
      *result = HTTOPRIGHT;
      return true;
    }
    if (bottom && left) {
      *result = HTBOTTOMLEFT;
      return true;
    }
    if (bottom && right) {
      *result = HTBOTTOMRIGHT;
      return true;
    }
    if (left) {
      *result = HTLEFT;
      return true;
    }
    if (right) {
      *result = HTRIGHT;
      return true;
    }
    if (top) {
      *result = HTTOP;
      return true;
    }
    if (bottom) {
      *result = HTBOTTOM;
      return true;
    }
  }

  if (pointInItem(m_captionItem, dipPos) && !pointInAnyExcludeItem(dipPos)) {
    *result = HTCAPTION;
    return true;
  }

  return false;
}

bool WindowChromeController::handleNcRightButtonUp(qintptr lParam, qintptr wParam, qintptr* result)
{
  if (!result) {
    return false;
  }

  if (wParam != HTCAPTION) {
    return false;
  }

  const QPoint screen = toPoint(static_cast<LPARAM>(lParam));
  showSystemMenu(screen);

  *result = 0;
  return true;
}

void WindowChromeController::updateHoveredButtonFromHitTest(qintptr hitTest)
{
  const Button b = buttonFromHitTest(hitTest);
  setHoveredButton(b);
  if (b != None) {
    ensureNonClientLeaveTracking();
  }
}

void WindowChromeController::clearNonClientTracking()
{
  m_trackingNonClientLeave = false;
  setHoveredButton(None);
  setPressedButton(None);
}

void WindowChromeController::ensureNonClientLeaveTracking()
{
  if (m_trackingNonClientLeave) {
    return;
  }

  const QQuickWindow* w = window();
  const HWND hwnd = hwndForWindow(w);
  if (!hwnd) {
    return;
  }

  TRACKMOUSEEVENT tme{};
  tme.cbSize = sizeof(tme);
  tme.dwFlags = TME_LEAVE | TME_NONCLIENT;
  tme.hwndTrack = hwnd;

  if (TrackMouseEvent(&tme)) {
    m_trackingNonClientLeave = true;
  }
}

void WindowChromeController::setHoveredButton(Button button)
{
  if (m_hoveredButton == button) {
    return;
  }
  m_hoveredButton = button;
  emit hoveredButtonChanged();
}

void WindowChromeController::setPressedButton(Button button)
{
  if (m_pressedButton == button) {
    return;
  }
  m_pressedButton = button;
  emit pressedButtonChanged();
}

WindowChromeController::Button WindowChromeController::buttonFromHitTest(qintptr hitTest)
{
  switch (hitTest) {
    case HTMINBUTTON:
      return Minimize;
    case HTMAXBUTTON:
      return Maximize;
    case HTCLOSE:
      return Close;
    default:
      return None;
  }
}
