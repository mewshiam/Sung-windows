#pragma once
#include <QObject>
#include <QVariantMap>
#ifdef Q_OS_WIN
class QSystemTrayIcon;
#endif

class PlaybackNotifier : public QObject {
  Q_OBJECT
public:
  explicit PlaybackNotifier(QObject *parent = nullptr);
  void show(const QString &title, const QString &artist);
  void clear();
private slots:
  void notificationClosed(uint id, uint reason);
private:
  void flush();
  void close(uint id);
  QVariantMap m_queued;
  uint m_id = 0;
  quint64 m_generation = 0;
  bool m_pending = false;
#ifdef Q_OS_WIN
  // The toast host on Windows; the Unix side speaks D-Bus instead.
  QSystemTrayIcon *m_tray = nullptr;
#endif
};
