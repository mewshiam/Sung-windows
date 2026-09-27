#pragma once
#include <QObject>
#include <QVariantMap>
#include "systemtray.h"

// The transient now-playing hint. On Windows it rides the shared tray icon's
// balloon, which Windows 10 and 11 render as a regular toast; the icon itself
// is the Backend's SystemTray, which also answers the tray behaviours.
class PlaybackNotifier : public QObject {
  Q_OBJECT
public:
  explicit PlaybackNotifier(QObject *parent = nullptr);
  void setTray(SystemTray *tray) { m_tray = tray; }
  void show(const QString &title, const QString &artist);
  void clear();
private:
  void flush();
  QVariantMap m_queued;
  SystemTray *m_tray = nullptr;
};
