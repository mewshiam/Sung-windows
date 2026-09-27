#include "systemtray.h"
#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>

// The icon's host: a never-shown window that receives the tray's callback
// messages, owns the context menu, and re-claims its corner after Explorer
// restarts. The same arrangement carries the balloon toasts, which Windows
// 10 and 11 render as regular notifications.
struct SystemTray::Icon {
  SystemTray *q = nullptr;
  HWND window = nullptr;
  bool added = false;
  bool playing = false;
  QString tip = QStringLiteral("Sung");
  UINT taskbarCreated = 0;

  ~Icon() {
    remove();
    if (window) DestroyWindow(window);
  }

  static Icon *from(HWND hwnd) {
    return reinterpret_cast<Icon *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  }

  static LRESULT CALLBACK proc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp) {
    if (message == WM_NCCREATE) {
      SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                        reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW *>(lp)->lpCreateParams));
    }
    if (Icon *icon = from(hwnd))
      return icon->receive(hwnd, message, wp, lp);
    return DefWindowProcW(hwnd, message, wp, lp);
  }

  bool ready() {
    if (window) return true;
    WNDCLASSW wc = {};
    wc.lpfnWndProc = &Icon::proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"SungTrayHost";
    if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
      return false;
    window = CreateWindowW(wc.lpszClassName, L"Sung", 0, 0, 0, 0, 0,
                           nullptr, nullptr, wc.hInstance, this);
    if (!window) return false;
    taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
    return true;
  }

  NOTIFYICONDATAW base() const {
    NOTIFYICONDATAW data = {};
    data.cbSize = sizeof(data);
    data.hWnd = window;
    data.uID = 1;
    return data;
  }

  bool add() {
    if (!ready()) return false;
    NOTIFYICONDATAW data = base();
    data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    data.uCallbackMessage = WM_APP + 1;
    // The executable's own icon sits at resource 1; drawn at the tray's own
    // size, with the generic application icon as the fallback.
    data.hIcon = static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1),
                                               IMAGE_ICON, GetSystemMetrics(SM_CXSMICON),
                                               GetSystemMetrics(SM_CYSMICON), LR_DEFAULTSIZE));
    if (!data.hIcon) data.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    tip.left(127).toWCharArray(data.szTip);
    data.szTip[qMin(127, tip.length())] = L'\0';
    added = Shell_NotifyIconW(NIM_ADD, &data) != FALSE;
    return added;
  }

  void remove() {
    if (!added) return;
    NOTIFYICONDATAW data = base();
    Shell_NotifyIconW(NIM_DELETE, &data);
    added = false;
  }

  void setTipText(const QString &text) {
    tip = text;
    if (!added) return;
    NOTIFYICONDATAW data = base();
    data.uFlags = NIF_TIP | NIF_SHOWTIP;
    tip.left(127).toWCharArray(data.szTip);
    data.szTip[qMin(127, tip.length())] = L'\0';
    Shell_NotifyIconW(NIM_MODIFY, &data);
  }

  void toast(const QString &title, const QString &text) {
    if (!add()) return;
    NOTIFYICONDATAW data = base();
    data.uFlags = NIF_INFO;
    // A quiet-time-respecting, icon-less toast: the text is the message.
    data.dwInfoFlags = NIIF_NONE | NIIF_RESPECT_QUIET_TIME;
    // Both balloon texts are fixed-size buffers; copy inside their bounds
    // and terminate them by hand.
    title.left(63).toWCharArray(data.szInfoTitle);
    text.left(255).toWCharArray(data.szInfo);
    data.szInfoTitle[qMin(63, title.length())] = L'\0';
    data.szInfo[qMin(255, text.length())] = L'\0';
    Shell_NotifyIconW(NIM_MODIFY, &data);
  }

  void hideToast() {
    if (!added) return;
    NOTIFYICONDATAW data = base();
    data.uFlags = NIF_INFO;
    data.dwInfoFlags = NIIF_NONE;
    Shell_NotifyIconW(NIM_MODIFY, &data);
  }

  void menu() {
    HMENU menu = CreatePopupMenu();
    if (!menu) return;
    AppendMenuW(menu, MF_STRING, 1, L"Open Sung");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, 2, playing ? L"Pause" : L"Play");
    AppendMenuW(menu, MF_STRING, 3, L"Previous");
    AppendMenuW(menu, MF_STRING, 4, L"Next");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, 5, L"Quit Sung");
    POINT point;
    GetCursorPos(&point);
    // The standard ritual for a tray menu: the window takes the foreground
    // so the menu dismisses on a click elsewhere, and WM_NULL clears the
    // focus remnant TrackPopupMenu leaves behind.
    SetForegroundWindow(window);
    const int chosen = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON,
                                      point.x, point.y, 0, window, nullptr);
    PostMessageW(window, WM_NULL, 0, 0);
    DestroyMenu(menu);
    switch (chosen) {
    case 1: emit q->activateRequested(); break;
    case 2: emit q->playPauseRequested(); break;
    case 3: emit q->previousRequested(); break;
    case 4: emit q->nextRequested(); break;
    case 5: emit q->quitRequested(); break;
    default: break;
    }
  }

  LRESULT receive(HWND hwnd, UINT message, WPARAM wp, LPARAM lp) {
    if (taskbarCreated && message == taskbarCreated) {
      // Explorer restarted and took the corner with it; claim the spot again.
      remove();
      add();
      return 0;
    }
    if (message == WM_APP + 1) {
      switch (static_cast<UINT>(lp)) {
      case WM_LBUTTONUP:
        emit q->activateRequested();
        break;
      case WM_RBUTTONUP:
      case WM_CONTEXTMENU:
        menu();
        break;
      default: break;
      }
      return 0;
    }
    return DefWindowProcW(hwnd, message, wp, lp);
  }
};

SystemTray::SystemTray(QObject *parent) : QObject(parent) {}
SystemTray::~SystemTray() { delete m_icon; }

void SystemTray::setKept(bool kept) {
  if (m_kept == kept) return;
  m_kept = kept;
  update();
}

void SystemTray::setPlaying(bool playing) {
  m_playing = playing;
  if (m_icon) m_icon->playing = playing;
}

void SystemTray::setTip(const QString &tip) {
  if (m_icon) m_icon->setTipText(tip.isEmpty() ? QStringLiteral("Sung") : tip);
}

void SystemTray::toast(const QString &title, const QString &text) {
  m_toast = true;
  update();
  if (m_icon) m_icon->toast(title, text);
}

void SystemTray::hideToast() {
  m_toast = false;
  if (m_icon) m_icon->hideToast();
  update();
}

// The icon joins the tray when something wants it there and leaves when
// nothing does, so a user who never asked for the tray sees no tray.
void SystemTray::update() {
  if (wanted()) {
    if (!m_icon) m_icon = new Icon{this};
    if (!m_icon->added) m_icon->add();
  } else if (m_icon) {
    m_icon->remove();
  }
}

#else

SystemTray::SystemTray(QObject *parent) : QObject(parent) {}
SystemTray::~SystemTray() = default;
void SystemTray::setKept(bool) {}
void SystemTray::setPlaying(bool) {}
void SystemTray::setTip(const QString &) {}
void SystemTray::toast(const QString &, const QString &) {}
void SystemTray::hideToast() {}
void SystemTray::update() {}

#endif
