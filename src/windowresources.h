#pragma once
#include <QObject>
#include <QQuickWindow>
#include <QTimer>
#include "freedmemory.h"
#include "roundedart.h"

// Keep render resources warm for quick toggles. After a window has been hidden
// for 30 seconds, let Qt release its recreatable scene and graphics resources.
// QML objects, playback, navigation and control state remain alive throughout.
class WindowResources : public QObject {
  Q_OBJECT
public:
  using QObject::QObject;
  Q_INVOKABLE void manage(QQuickWindow *window) {
    if (!window || window->findChild<QTimer *>("renderResourceIdleTimer", Qt::FindDirectChildrenOnly))
      return;
    auto timer = new QTimer(window);
    timer->setObjectName("renderResourceIdleTimer");
    timer->setSingleShot(true);
    timer->setInterval(30000);
    connect(timer, &QTimer::timeout, window, [window] {
      if (window->isVisible() && window->visibility() != QWindow::Minimized)
        return;
      // Half a minute unseen is long enough to stop pretending the decoded
      // covers and the trimmed working set cost nothing. The in-memory decodes
      // go first - every surface re-decodes from the disk cache in the time it
      // takes to bring the window back - and the working set follows, so what
      // the release frees is charged off the process as well.
      RoundedArt::trimMemory();
      window->setPersistentSceneGraph(false);
      window->setPersistentGraphics(false);
      window->releaseResources();
      returnFreedMemory();
    });
    const auto visibilityChanged = [window, timer] {
      if (!window->isVisible() || window->visibility() == QWindow::Minimized) {
        timer->start();
      } else {
        timer->stop();
        window->setPersistentGraphics(true);
        window->setPersistentSceneGraph(true);
      }
    };
    connect(window, &QWindow::visibilityChanged, window, visibilityChanged);
    visibilityChanged();
  }
};
