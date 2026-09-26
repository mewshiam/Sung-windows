# Sung for Windows

Sung started as a Linux player, and this document describes how it builds and
ships on Windows. The result of the Windows build is a **portable zip**: unpack
it anywhere, run `Sung\bin\sung.exe`, no installer and no system Qt or Python
required.

## Getting a build

### The easy way: GitHub Actions

The repository ships `.github/workflows/windows.yml`. Every push, pull
request, or manual dispatch builds `sung.exe` on a Windows runner with Qt
6.8 and MSVC, packages it with `windeployqt`, and uploads a
**Sung-windows-x64** artifact. Download it from the *Actions* tab of the
repository (run page → *Artifacts* section). Pushing a tag like `v1.2.0`
also attaches the zip to a GitHub release.

### Building locally on Windows

Requirements:

1. **Qt 6.10 or newer** with the MSVC 2022 64-bit kit, plus the Qt modules Core,
   Concurrent, Gui, Quick, Qml, QuickControls2, Multimedia, Network and Svg
   (the online installer's default "Qt 6.10 for desktop development" covers
   all of them). The QML bytecode compilation uses `DISCARD_QML_CONTENTS`,
   which older Qt releases do not provide.
2. **Visual Studio 2022** with the C++ toolset (or just the Build Tools).
3. **CMake 3.24+** and **Ninja**.
4. Python 3 is *not* required to build, but `pip`/PyPI access is needed once
   if you want the bundled helper runtime (see below).

From an *x64 Native Tools Command Prompt for VS 2022*:

```bat
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build
```

`build\sung.exe` is the application. It already embeds the QML, icons and the
helper's Python scripts as Qt resources; it still needs Qt's DLLs beside it
at runtime, which the next step provides:

```bat
mkdir Sung\bin
copy build\sung.exe Sung\bin\
C:\Qt\6.10.3\msvc2022_64\bin\windeployqt.exe --release --no-translations ^
    --no-system-d3d-compiler --no-opengl-sw Sung\bin\sung.exe
copy helper\catalog.py helper\online_artwork.py helper\requirements.txt Sung\helper\
```

To reproduce the fully self-contained zip that CI produces (embedded Python
with `ytmusicapi` and `yt-dlp` preinstalled, ffmpeg beside the exe), read
`.github/workflows/windows.yml` — it is the authoritative recipe, step by
step.

## What the portable zip contains

```
Sung\
├── bin\          sung.exe, Qt DLLs and plugins, MSVC runtime,
│                 ffmpeg/ffprobe, qt.conf
├── helper\       the Python sources (also embedded in the exe as resources)
├── runtime\      embedded Python 3.12 with ytmusicapi + yt-dlp installed
├── licenses\     third-party license texts
├── LICENSE
├── NOTICE
└── README-WINDOWS.md   (this file)
```

- **YouTube Music** works out of the box — the embedded interpreter and the
  `ytmusicapi`/`yt-dlp` pair ship inside `runtime\`.
- **Animated artwork** works out of the box — `ffmpeg.exe`/`ffprobe.exe` sit
  next to the executable, and the application puts its own folder at the
  front of the PATH it hands to children.
- The app keeps its data under `%APPDATA%\Sung`, like any well-behaved
  Windows program.
- Local files, Navidrome/Subsonic, Jellyfin, lyrics, listening statistics and
  the smart mixes behave as on Linux.

## Feature notes and current limitations on Windows

- **System media controls**: Linux exposes MPRIS over D-Bus. Windows has no
  D-Bus, so for now the media keys and the system media flyout are not
  wired up; all playback control lives in the app itself (including the
  mini player). Wiring WinRT's `SystemMediaTransportControls` is the
  natural next step for `src/main.cpp`/`src/backend.h`.
- **Notifications**: now-playing toasts appear through the tray
  notification host; the tray icon exists only while a toast can show and
  hides again when playback pauses.
- **Credential storage**: server passwords and the Last.fm token are stored
  in the system keyring on Linux (`secret-tool`). Windows has no equivalent
  CLI, so "remember me" degrades to per-session sign-in with a clear
  message from the app.
- **Pause on headphone disconnect**: the PulseAudio fine-tuning
  (`pactl`) is skipped on Windows; a vanished audio device still pauses
  playback through Qt's device notifications.
- The single-instance guard uses a named pipe derived from your profile
  path, so two Windows users each get their own instance, and double-clicking
  the exe again raises the existing window.

## Troubleshooting

- **"YouTube helper could not start"** — the `runtime\` folder must sit next
  to `helper\` and `bin\`. If you moved files, keep the zip's structure.
- **No sound / codec issues** — Qt Multimedia on Windows uses the Windows
  Media Foundation with its own ffmpeg runtime; copying the package folder
  wholesale avoids missing-DLL problems.
- **SmartScreen warning** — the portable exe is unsigned; "More info → Run
  anyway" is expected until a signing certificate is set up.
