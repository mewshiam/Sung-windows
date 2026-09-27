#include "playbacknotifier.h"

PlaybackNotifier::PlaybackNotifier(QObject *parent) : QObject(parent) {}

void PlaybackNotifier::show(const QString &title, const QString &artist) {
  if (title.isEmpty())
    return;
  m_queued = {{"title", title.left(200)}, {"artist", artist.left(200)}};
  flush();
}

void PlaybackNotifier::clear() {
  m_queued.clear();
  // The toast is the now-playing hint, and withdrawing it is what pausing
  // gets; the icon itself only leaves when nothing else wants it there.
  if (m_tray)
    m_tray->hideToast();
}

void PlaybackNotifier::flush() {
  if (m_queued.isEmpty())
    return;
  if (!m_tray)
    return;
  const auto track = m_queued;
  m_queued.clear();
  const auto artist = track.value("artist").toString();
  const auto text =
      artist.isEmpty() ? track.value("title").toString()
                       : artist + " — " + track.value("title").toString();
  m_tray->toast("Sung", text);
}
