#pragma once
// Discord Rich Presence over Discord's own local IPC socket.
//
// A running Discord listens on a per-user pipe (Windows) or Unix socket that
// any local process may speak to: an eight-byte header, then JSON. The first
// frame names a registered application, after which the connection carries
// one command that matters here, SET_ACTIVITY, which is what paints the
// "listening to" line on the user's profile.
//
// The application identifier below is the one the Pear Desktop project
// registered for their players, so the profile entry names a real
// application rather than an unnamed one. Nothing is sent anywhere except
// to the Discord process on the same machine; what Discord does with it
// afterwards is Discord's own business.
//
// Updates go out when the song changes and when playback starts, pauses or
// seeks. Elapsed time comes from timestamps rather than a stream of
// progress frames, so nothing keeps flowing while the song plays. A pause
// longer than ten minutes clears the profile, the way Pear Desktop's own
// plugin does.
#include <QLocalSocket>
#include <QTimer>
#include <QElapsedTimer>
#include <QDateTime>
#include <QDataStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCoreApplication>
#include <QVariantMap>
#include <algorithm>

#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

// Registered by the Pear Desktop team for their desktop players.
static constexpr const char *discordClientId = "1177081335727267940";

class DiscordPresence : public QObject {
public:
  explicit DiscordPresence(QObject *parent = nullptr) : QObject(parent) {
    m_retry.setSingleShot(true);
    m_retry.setInterval(15000);
    connect(&m_retry, &QTimer::timeout, this, [this] { if (m_enabled) open(); });
    connect(&m_socket, &QLocalSocket::connected, this, [this] {
      m_buffer.clear();
      // The pipe only accepts commands once the handshake has been answered.
      writeFrame(0, QJsonObject{{"v", 1}, {"client_id", QString::fromLatin1(discordClientId)}});
    });
    connect(&m_socket, &QLocalSocket::readyRead, this, &DiscordPresence::readReplies);
    connect(&m_socket, &QLocalSocket::errorOccurred, this, [this] {
      m_live = false;
      if (!m_enabled) return;
      // Walk to the next candidate endpoint; start over from the front when
      // every one of them refused, and stop hammering until the timer fires.
      ++m_endpoint;
      if (m_endpoint >= 10) { m_endpoint = 0; m_retry.start(); return; }
      if (m_socket.state() == QLocalSocket::UnconnectedState) open();
    });
    connect(&m_socket, &QLocalSocket::disconnected, this, [this] {
      m_live = false;
      if (m_enabled) m_retry.start();
    });
    m_clearAfterPause.setSingleShot(true);
    m_clearAfterPause.setInterval(10 * 60 * 1000);
    connect(&m_clearAfterPause, &QTimer::timeout, this, [this] { sendActivity(QJsonObject()); });
  }

  bool connected() const { return m_live; }

  void setEnabled(bool on) {
    if (m_enabled == on) return;
    m_enabled = on;
    if (on) { update(m_track, m_playing, m_position, m_duration); return; }
    m_retry.stop();
    m_clearAfterPause.stop();
    if (m_socket.state() == QLocalSocket::ConnectedState) sendActivity(QJsonObject());
    m_socket.abort();
  }

  void update(const QVariantMap &track, bool playing, qint64 positionMs, qint64 durationMs) {
    m_track = track; m_playing = playing; m_position = positionMs; m_duration = durationMs;
    if (!m_enabled) return;
    m_clearAfterPause.stop();
    const QJsonObject activity = activityFor(track, playing, positionMs, durationMs);
    if (activity.isEmpty()) return;
    if (m_socket.state() != QLocalSocket::ConnectedState) { open(); return; }
    sendActivity(activity);
    // A long pause means nobody is listening; stop wearing the song.
    if (!playing) m_clearAfterPause.start();
  }

  void clear() {
    m_track.clear();
    m_clearAfterPause.stop();
    if (!m_enabled) return;
    if (m_socket.state() == QLocalSocket::ConnectedState) sendActivity(QJsonObject());
  }

private:
  QJsonObject activityFor(const QVariantMap &track, bool playing, qint64 positionMs, qint64 durationMs) const {
    const QString title = track.value("title").toString();
    if (title.isEmpty()) return {};
    QJsonObject activity;
    activity["details"] = title.left(128);
    const QString artist = track.value("artist").toString();
    if (!artist.isEmpty()) activity["state"] = artist.left(128);
    // Details, not the application name, are what the status line shows.
    activity["status_display_type"] = 2;
    if (playing && durationMs > 0) {
      const qint64 start = QDateTime::currentSecsSinceEpoch() - positionMs / 1000;
      activity["timestamps"] = QJsonObject{
          {"start", double(start)},
          {"end", double(start + durationMs / 1000)}};
    }
    const QString album = track.value("album").toString();
    const QString art = track.value("art").toString();
    if (art.startsWith("http")) {
      QJsonObject assets;
      assets["large_image"] = art;
      if (!album.isEmpty()) assets["large_text"] = album.left(128);
      activity["assets"] = assets;
    }
    const QString video = track.value("videoId").toString();
    if (!video.isEmpty()) {
      QJsonObject button;
      button["label"] = "Play on YouTube Music";
      button["url"] = "https://music.youtube.com/watch?v=" + video;
      activity["buttons"] = QJsonArray{button};
    }
    return activity;
  }

  void open() {
    if (m_socket.state() != QLocalSocket::UnconnectedState) return;
    // Ten numbered endpoints per platform, matching how Discord itself
    // picks the first free one when several copies run.
    QString runtime = qEnvironmentVariable("XDG_RUNTIME_DIR");
#ifdef Q_OS_UNIX
    if (runtime.isEmpty()) runtime = "/run/user/" + QString::number(::getuid());
#endif
    QStringList endpoints;
#ifdef Q_OS_WIN
    for (int i = 0; i < 10; ++i)
      endpoints << QStringLiteral(R"(\\.\pipe\discord-ipc-)") + QString::number(i);
#else
    for (int i = 0; i < 10; ++i) {
      if (runtime.isEmpty()) break;
      endpoints << runtime + "/discord-ipc-" + QString::number(i)
                << runtime + "/snap.discord/discord-ipc-" + QString::number(i)
                << runtime + "/app/com.discordapp.Discord/discord-ipc-" + QString::number(i);
    }
#endif
    if (m_endpoint >= endpoints.size()) m_endpoint = 0;
    if (m_endpoint >= endpoints.size()) { m_retry.start(); return; }
    m_socket.connectToServer(endpoints[m_endpoint]);
  }

  void sendActivity(const QJsonObject &activity) {
    if (m_socket.state() != QLocalSocket::ConnectedState) return;
    QJsonObject args;
    args["pid"] = qint64(QCoreApplication::applicationPid());
    // An absent activity is what clears the profile entry.
    args["activity"] = activity.isEmpty() ? QJsonValue() : activity;
    writeFrame(1, QJsonObject{{"cmd", "SET_ACTIVITY"}, {"nonce", QString::number(++m_nonce)}, {"args", args}});
  }

  void writeFrame(qint32 op, const QJsonObject &payload) {
    const QByteArray json = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    QByteArray header;
    QDataStream stream(&header, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream << op << qint32(json.size());
    m_socket.write(header + json);
    m_socket.flush();
  }

  void readReplies() {
    m_buffer.append(m_socket.readAll());
    while (m_buffer.size() >= 8) {
      qint32 op = 0, length = 0;
      QDataStream stream(m_buffer);
      stream.setByteOrder(QDataStream::LittleEndian);
      stream >> op >> length;
      if (length < 0 || length > 1 << 20) { m_socket.abort(); return; }
      if (m_buffer.size() < 8 + length) return;
      const QJsonObject payload = QJsonDocument::fromJson(m_buffer.mid(8, length)).object();
      m_buffer.remove(0, 8 + length);
      // The keepalive ping must be answered or Discord drops the pipe.
      if (op == 3) { writeFrame(4, payload); continue; }
      if (op == 1 && payload.value("evt").toString() == "READY") {
        m_live = true;
        if (!m_track.isEmpty()) update(m_track, m_playing, m_position, m_duration);
      }
    }
  }

  QLocalSocket m_socket;
  QTimer m_retry, m_clearAfterPause;
  QByteArray m_buffer;
  QVariantMap m_track;
  bool m_enabled = false, m_playing = false, m_live = false;
  qint64 m_position = 0, m_duration = 0;
  quint32 m_nonce = 0;
  int m_endpoint = 0;
};
