#pragma once
#ifdef __GLIBC__
#include <malloc.h>
#endif
#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

// glibc keeps what a program frees on its own free lists and gives memory back
// to the system only from the top of the heap, or when a whole mmapped block is
// released. Short-lived work that allocates a lot in small pieces, such as
// reading or writing a large library, leaves its freed pages between allocations
// that live on, so the process stays at its peak long after the work is done:
// four saves of a 10,000-song library left it 114 MiB larger, all of it memory
// already freed. malloc_trim(0) returns every whole free page wherever it sits,
// in every arena, and brought it back to where it started.
// https://man7.org/linux/man-pages/man3/malloc_trim.3.html
// It walks the free lists, so it belongs after such work rather than on a
// timer; on that heap it took 4.7 ms.
//
// Windows has no malloc_trim: its allocator returns large blocks as they are
// freed, but the pages stay charged to the process's working set until the
// system wants them, so Task Manager keeps showing a peak the process no
// longer holds. Two calls put those pages back on the system's free list.
// SetProcessWorkingSetSize(-1, -1) asks for the smallest working set there is
// and EmptyWorkingSet takes everything that is not mapped out already; both
// leave committed memory alone, so the pages simply fault back in on the next
// touch - a soft fault, microseconds, not a reload from disk. The cost only
// makes sense when the window is out of sight or the heavy work has just
// finished, which is where the callers below (and the interface, on state
// changes) call it.
inline void returnFreedMemory() {
#ifdef __GLIBC__
  malloc_trim(0);
#endif
#ifdef Q_OS_WIN
  HANDLE process = GetCurrentProcess();
  SetProcessWorkingSetSize(process, SIZE_T(-1), SIZE_T(-1));
  EmptyWorkingSet(process);
#endif
}
