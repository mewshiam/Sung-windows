#include "playbacknotifier.h"
#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")

// The freedesktop notification bus does not exist on Windows, so the same
// transient now-playing hint goes through a tray icon's balloon message,
// which Windows 10 and 11 render as a regular toast. The tray icon comes
// from Shell_NotifyIcon directly: Qt's own wrapper lives in Qt Widgets,
// and this Qt Quick application does not carry them. The icon uses the
// executable's own, and stays alive for the notifier's lifetime because
// Windows drops balloons from icons that vanish.
struct PlaybackNotifier::TrayBalloon {
  HWND window = nullptr;
  bool added = false;
  ~TrayBalloon() { remove(); if (window) DestroyWindow(window); }
  bool ready() {
    if (window) return true;
    WNDCLASSW wc = {};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"SungNotificationHost";
    if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
      return false;
    // A message-only window: the tray needs a handle for its callbacks,
    // not a visible surface.
    window = CreateWindowW(wc.lpszClassName, L"Sung", 0, 0, 0, 0, 0,
                           HWND_MESSAGE, nullptr, wc.hInstance, nullptr);
    return window != nullptr;
  }
  bool present() {
    if (!ready()) return false;
    if (added) return true;
    NOTIFYICONDATAW data = {};
    data.cbSize = sizeof(data);
    data.hWnd = window;
    data.uID = 1;
    data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    data.uCallbackMessage = WM_APP + 1;
    // The executable's own icon sits at resource 1; the generic application
    // icon is the fallback if it ever fails to load.
    data.hIcon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1));
    if (!data.hIcon) data.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    lstrcpyW(data.szTip, L"Sung");
    added = Shell_NotifyIconW(NIM_ADD, &data) != FALSE;
    return added;
  }
  void show(const QString &title, const QString &text) {
    if (!present()) return;
    NOTIFYICONDATAW data = {};
    data.cbSize = sizeof(data);
    data.hWnd = window;
    data.uID = 1;
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
  void remove() {
    if (!added) return;
    NOTIFYICONDATAW data = {};
    data.cbSize = sizeof(data);
    data.hWnd = window;
    data.uID = 1;
    Shell_NotifyIconW(NIM_DELETE, &data);
    added = false;
  }
};

PlaybackNotifier::PlaybackNotifier(QObject *parent) : QObject(parent) {}
void PlaybackNotifier::show(const QString &title, const QString &artist) {
  if (title.isEmpty())
    return;
  m_queued = {{"title", title.left(200)}, {"artist", artist.left(200)}};
  flush();
}
void PlaybackNotifier::clear() {
  m_queued.clear();
  // Windows removes a balloon when its tray icon goes away, which is the
  // dismissal the Unix bus gets from CloseNotification.
  if (m_tray)
    m_tray->remove();
}
void PlaybackNotifier::flush() {
  if (m_queued.isEmpty())
    return;
  if (!m_tray)
    m_tray = new TrayBalloon();
  const auto track = m_queued;
  m_queued.clear();
  const auto artist = track.value("artist").toString();
  const auto text =
      artist.isEmpty() ? track.value("title").toString()
                       : artist + " — " + track.value("title").toString();
  m_tray->show("Sung", text);
}
void PlaybackNotifier::close(uint) {}
void PlaybackNotifier::notificationClosed(uint, uint) {}
#else
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>

static QDBusMessage message(const QString &method) {
  return QDBusMessage::createMethodCall("org.freedesktop.Notifications", "/org/freedesktop/Notifications", "org.freedesktop.Notifications", method);
}
PlaybackNotifier::PlaybackNotifier(QObject *parent) : QObject(parent) {
  auto bus=QDBusConnection::sessionBus();
  bus.connect("org.freedesktop.Notifications","/org/freedesktop/Notifications","org.freedesktop.Notifications","NotificationClosed",this,SLOT(notificationClosed(uint,uint)));
  auto watcher=new QDBusServiceWatcher("org.freedesktop.Notifications",bus,QDBusServiceWatcher::WatchForOwnerChange,this);
  connect(watcher,&QDBusServiceWatcher::serviceOwnerChanged,this,[this](const QString &,const QString &oldOwner,const QString &){m_id=0;if(!oldOwner.isEmpty())++m_generation;});
}
void PlaybackNotifier::show(const QString &title,const QString &artist) {
  if(title.isEmpty())return;
  m_queued={{"title",title.left(200)},{"artist",artist.left(200)}};
  flush();
}
void PlaybackNotifier::close(uint id) {
  if(!id)return;
  auto request=message("CloseNotification");request<<id;
  QDBusConnection::sessionBus().asyncCall(request,2000);
}
void PlaybackNotifier::clear() {
  m_queued.clear();++m_generation;close(m_id);m_id=0;
}
void PlaybackNotifier::notificationClosed(uint id,uint) {if(id==m_id)m_id=0;}
void PlaybackNotifier::flush() {
  if(m_pending||m_queued.isEmpty())return;
  const auto track=m_queued;m_queued.clear();m_pending=true;
  const auto generation=m_generation;
  auto request=message("Notify");
  const QVariantMap hints{{"desktop-entry","sung"},{"category","music"},{"transient",true},{"suppress-sound",true},{"urgency",QVariant::fromValue(uchar(0))}};
  request<<QString("Sung")<<m_id<<QString("sung")<<track.value("title").toString()<<track.value("artist").toString().toHtmlEscaped()<<QStringList{}<<hints<<5000;
  auto pending=new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(request,2000),this);
  connect(pending,&QDBusPendingCallWatcher::finished,this,[this,generation](QDBusPendingCallWatcher *call){
    QDBusPendingReply<uint> result=*call;
    if(!result.isError()){if(generation==m_generation)m_id=result.value();else close(result.value());}
    m_pending=false;call->deleteLater();flush();
  });
}
#endif
