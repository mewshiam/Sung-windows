#include "motionartwork.h"
#include <QCoreApplication>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QImageReader>
#include <QMediaPlayer>
#include <QMovie>
#include <QThreadPool>
#include <QVideoFrame>
#include <QVideoSink>
#include <QTransform>
#include <QtConcurrentRun>

// An animation larger than the cap is decoded down to it; a smaller one is
// left at its own size for the surface to scale, not enlarged up front.
static QSize bounded(const QSize &native,int cap) {
  return native.width()>cap || native.height()>cap ? native.scaled(cap,cap,Qt::KeepAspectRatio) : native;
}
// Converting a video frame - QVideoFrame::toImage, the rotation and the
// downscale of a 2732px tier - is tens of milliseconds of CPU work. On the
// GUI thread it was a dropped frame each time; the work belongs beside the
// cover decodes, off the thread that draws. One thread, in order: frames
// come from one player, and a second would let two conversions race.
static QThreadPool *convertPool() {
  static QThreadPool *pool = nullptr;
  if (!pool) {
    pool = new QThreadPool(QCoreApplication::instance());
    pool->setMaxThreadCount(1);
  }
  return pool;
}
MotionArtwork::MotionArtwork(QObject *parent):QObject(parent) {}
MotionArtwork::~MotionArtwork() { clear(); }
void MotionArtwork::clear() {
  m_movie.reset();m_player.reset();m_sink.reset();m_frame={};m_movieFailed=false;
}
void MotionArtwork::publish(QImage frame) {
  if(frame.isNull())return;
  if(frame.width()>m_maximumSize || frame.height()>m_maximumSize)frame=frame.scaled(m_maximumSize,m_maximumSize,Qt::KeepAspectRatio,Qt::SmoothTransformation);
  m_frame=std::move(frame);emit frameChanged();
}
void MotionArtwork::publishVideo(const QVideoFrame &frame) {
  if(!m_running || !frame.isValid())return;
  // Back-pressure: if the last conversion is still running, this frame is
  // dropped. A 30fps cover converting slower than that simply animates at
  // the rate the machine manages; nothing queues, nothing waits, and the
  // frame that lands is never older than the newest one offered.
  bool expected=false;
  if(!m_converting.compare_exchange_strong(expected,true))return;
  const int maximumSize=m_maximumSize;
  auto *watcher=new QFutureWatcher<QImage>(this);
  connect(watcher,&QFutureWatcherBase::finished,this,[this,watcher]{
    const QImage image=watcher->result();
    watcher->deleteLater();
    m_converting=false;
    publish(image);
  });
  watcher->setFuture(QtConcurrent::run(convertPool(),[frame,maximumSize]()mutable{
    QImage image=frame.toImage();
    if(image.isNull())return QImage();
    if(frame.rotation()!=QtVideo::Rotation::None)image=image.transformed(QTransform().rotate(int(frame.rotation())));
    if(frame.mirrored())image=image.transformed(QTransform().scale(-1,1));
    if(image.width()>maximumSize || image.height()>maximumSize)
      image=image.scaled(maximumSize,maximumSize,Qt::KeepAspectRatio,Qt::SmoothTransformation);
    return image;
  }));
}
void MotionArtwork::setSource(const QUrl &source) {
  if(m_source==source)return;
  clear();m_source=source;emit sourceChanged();emit frameChanged();
  const QFileInfo file(source.toLocalFile());
  if(!source.isLocalFile() || !file.isFile() || file.size()>128*1024*1024)return;
  const auto suffix=file.suffix().toLower();
  if(suffix=="gif" || suffix=="webp") {
    QImageReader reader(file.absoluteFilePath());const auto size=reader.size();
    if(!size.isValid() || size.width()>4096 || size.height()>4096 || !reader.supportsAnimation())return;
    m_movie=std::make_unique<QMovie>(file.absoluteFilePath());
    m_movie->setCacheMode(QMovie::CacheNone);
    m_movie->setScaledSize(bounded(size,m_maximumSize));
    connect(m_movie.get(),&QMovie::frameChanged,m_movie.get(),[this]{publish(m_movie->currentImage());});
    // QMovie emits finished() in the call that detects an unreadable frame.
    // Queue the next pass so start() cannot recurse through that signal, and
    // leave a failed decoder stopped even when finished() follows error().
    connect(m_movie.get(),&QMovie::finished,m_movie.get(),[this]{
      if(m_running && !m_movieFailed)m_movie->start();
    },Qt::QueuedConnection);
    connect(m_movie.get(),&QMovie::error,m_movie.get(),[this]{
      m_movieFailed=true;
      m_movie->stop();
      m_frame={};emit frameChanged();
    });
    if(m_running)m_movie->start();
  } else if(suffix=="mp4" || suffix=="webm") {
    m_sink=std::make_unique<QVideoSink>();m_player=std::make_unique<QMediaPlayer>();
    // No QAudioOutput: artwork never adds an audio stream to music playback.
    m_player->setVideoSink(m_sink.get());m_player->setLoops(QMediaPlayer::Infinite);
    connect(m_player.get(),&QMediaPlayer::tracksChanged,m_player.get(),[this]{m_player->setActiveAudioTrack(-1);});
    connect(m_sink.get(),&QVideoSink::videoFrameChanged,m_sink.get(),[this](const QVideoFrame &frame){publishVideo(frame);});
    connect(m_player.get(),&QMediaPlayer::errorOccurred,m_player.get(),[this]{m_frame={};emit frameChanged();});
    QUrl local=QUrl::fromLocalFile(file.absoluteFilePath());m_player->setSource(local);
    if(m_running)m_player->play();
  }
}
void MotionArtwork::setRunning(bool running) {
  if(m_running==running)return;
  m_running=running;emit runningChanged();
  if(m_movie){
    if(running && !m_movieFailed && m_movie->state()==QMovie::NotRunning)m_movie->start();
    else if(m_movie->state()!=QMovie::NotRunning)m_movie->setPaused(!running);
  }
  if(m_player){if(running)m_player->play();else m_player->pause();}
}
void MotionArtwork::setMaximumSize(int size) {
  size=qBound(64,size,4096);
  if(m_maximumSize==size)return;
  m_maximumSize=size;emit maximumSizeChanged();
  if(m_movie){
    QImageReader reader(m_movie->fileName());const auto native=reader.size();
    if(native.isValid())m_movie->setScaledSize(bounded(native,size));
  }
}
