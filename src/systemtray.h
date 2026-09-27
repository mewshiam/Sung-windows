#pragma once
#include <QObject>
#include <QString>

// The notification-area icon. It exists while something needs it there - a
// now-playing toast, or one of the tray behaviours the user switched on -
// and answers clicks with the signals below. The Windows implementation
// speaks Shell_NotifyIcon directly, because Qt's own wrapper lives in
// Qt Widgets and this Qt Quick application does not carry them.
class SystemTray : public QObject {
  Q_OBJECT
public:
  explicit SystemTray(QObject *parent = nullptr);
  ~SystemTray() override;
  // Whether the icon stays for the user's sake (minimize or close to the
  // tray), as opposed to only while a toast is showing.
  void setKept(bool kept);
  // What the menu's second entry says, Play or Pause, this moment.
  void setPlaying(bool playing);
  // The line a hover over the icon shows.
  void setTip(const QString &tip);
  void toast(const QString &title, const QString &text);
  void hideToast();
signals:
  void activateRequested();
  void playPauseRequested();
  void previousRequested();
  void nextRequested();
  void quitRequested();
private:
  bool wanted() const { return m_kept || m_toast; }
  void update();
  bool m_kept = false;
  bool m_toast = false;
  bool m_playing = false;
#ifdef Q_OS_WIN
  struct Icon;
  Icon *m_icon = nullptr;
#endif
};
