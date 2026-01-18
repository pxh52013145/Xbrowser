#include "WindowsShareController.h"

#include <Windows.h>

#include <roapi.h>
#include <shobjidl_core.h>
#include <windows.applicationmodel.datatransfer.h>
#include <windows.foundation.h>
#include <wrl.h>
#include <wrl/wrappers/corewrappers.h>

#include <QGuiApplication>
#include <QWindow>

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Wrappers::HStringReference;

namespace
{
HRESULT ensureWinRt()
{
  const HRESULT hr = RoInitialize(RO_INIT_SINGLETHREADED);
  if (hr == RPC_E_CHANGED_MODE) {
    return S_OK;
  }
  return hr;
}

HWND resolveShareWindow()
{
  QWindow* window = QGuiApplication::focusWindow();
  if (!window) {
    const auto windows = QGuiApplication::topLevelWindows();
    for (QWindow* candidate : windows) {
      if (candidate && candidate->isVisible()) {
        window = candidate;
        break;
      }
    }
  }

  if (!window) {
    return nullptr;
  }

  const WId wid = window->winId();
  if (!wid) {
    return nullptr;
  }

  return reinterpret_cast<HWND>(wid);
}

QString normalizeTitle(const QString& rawTitle, const QUrl& url)
{
  const QString title = rawTitle.trimmed();
  if (!title.isEmpty()) {
    return title;
  }
  if (url.isValid()) {
    const QString text = url.toString(QUrl::FullyEncoded).trimmed();
    if (!text.isEmpty()) {
      return text;
    }
  }
  return QStringLiteral("Link");
}

QString normalizeUrlText(const QUrl& url)
{
  if (!url.isValid()) {
    return {};
  }

  const QString text = url.toString(QUrl::FullyEncoded).trimmed();
  if (text.isEmpty()) {
    return {};
  }
  return text;
}

bool tryShowShareUi(HWND hwnd, const QString& title, const QString& urlText)
{
  if (!hwnd) {
    return false;
  }

  if (title.trimmed().isEmpty() || urlText.trimmed().isEmpty()) {
    return false;
  }

  if (FAILED(ensureWinRt())) {
    return false;
  }

  ComPtr<IDataTransferManagerInterop> interop;
  HRESULT hr = RoGetActivationFactory(
    HStringReference(RuntimeClass_Windows_ApplicationModel_DataTransfer_DataTransferManager).Get(),
    IID_PPV_ARGS(&interop));
  if (FAILED(hr) || !interop) {
    return false;
  }

  ComPtr<ABI::Windows::ApplicationModel::DataTransfer::IDataTransferManager> manager;
  hr = interop->GetForWindow(hwnd, IID_PPV_ARGS(&manager));
  if (FAILED(hr) || !manager) {
    return false;
  }

  const std::wstring titleWide = title.toStdWString();
  const std::wstring urlWide = urlText.toStdWString();

  EventRegistrationToken token{};
  hr = manager->add_DataRequested(
    Callback<__FITypedEventHandler_2_Windows__CApplicationModel__CDataTransfer__CDataTransferManager_Windows__CApplicationModel__CDataTransfer__CDataRequestedEventArgs>(
      [titleWide, urlWide](
        ABI::Windows::ApplicationModel::DataTransfer::IDataTransferManager* /*sender*/,
        ABI::Windows::ApplicationModel::DataTransfer::IDataRequestedEventArgs* args) -> HRESULT {
        if (!args) {
          return E_INVALIDARG;
        }

        ComPtr<ABI::Windows::ApplicationModel::DataTransfer::IDataRequest> request;
        HRESULT hr = args->get_Request(&request);
        if (FAILED(hr) || !request) {
          return hr;
        }

        ComPtr<ABI::Windows::ApplicationModel::DataTransfer::IDataPackage> package;
        hr = request->get_Data(&package);
        if (FAILED(hr) || !package) {
          return hr;
        }

        ComPtr<ABI::Windows::ApplicationModel::DataTransfer::IDataPackagePropertySet> props;
        hr = package->get_Properties(&props);
        if (SUCCEEDED(hr) && props) {
          props->put_Title(HStringReference(titleWide.c_str()).Get());
          props->put_Description(HStringReference(urlWide.c_str()).Get());
        }

        package->SetText(HStringReference(urlWide.c_str()).Get());

        ComPtr<ABI::Windows::Foundation::IUriRuntimeClassFactory> uriFactory;
        hr = RoGetActivationFactory(
          HStringReference(RuntimeClass_Windows_Foundation_Uri).Get(),
          IID_PPV_ARGS(&uriFactory));
        if (SUCCEEDED(hr) && uriFactory) {
          ComPtr<ABI::Windows::Foundation::IUriRuntimeClass> uri;
          hr = uriFactory->CreateUri(HStringReference(urlWide.c_str()).Get(), &uri);
          if (SUCCEEDED(hr) && uri) {
            ComPtr<ABI::Windows::ApplicationModel::DataTransfer::IDataPackage2> package2;
            if (SUCCEEDED(package.As(&package2)) && package2) {
              package2->SetWebLink(uri.Get());
            }
          }
        }

        return S_OK;
      })
      .Get(),
    &token);
  if (FAILED(hr)) {
    return false;
  }

  hr = interop->ShowShareUIForWindow(hwnd);
  manager->remove_DataRequested(token);

  return SUCCEEDED(hr);
}
}

WindowsShareController::WindowsShareController(QObject* parent)
  : QObject(parent)
  , m_canShare(probeCanShare())
{
}

bool WindowsShareController::canShare() const
{
  return m_canShare;
}

bool WindowsShareController::shareUrl(const QString& title, const QUrl& url)
{
  const QString urlText = normalizeUrlText(url);
  if (urlText.isEmpty()) {
    return false;
  }

  const QString normalizedTitle = normalizeTitle(title, url);
  const HWND hwnd = resolveShareWindow();
  return tryShowShareUi(hwnd, normalizedTitle, urlText);
}

bool WindowsShareController::probeCanShare() const
{
  if (FAILED(ensureWinRt())) {
    return false;
  }

  ComPtr<IDataTransferManagerInterop> interop;
  const HRESULT hr = RoGetActivationFactory(
    HStringReference(RuntimeClass_Windows_ApplicationModel_DataTransfer_DataTransferManager).Get(),
    IID_PPV_ARGS(&interop));
  return SUCCEEDED(hr) && interop;
}

