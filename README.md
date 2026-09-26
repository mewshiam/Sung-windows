<div align="center">

<img src="assets/readme-now-playing.png" alt="Sung playing a song, the cover in a flower shape ringed by the visualizer" width="100%">


# Sung for Windows

**YouTube Music, your music files, and your music server. Native, portable, no browser.**

[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
![Windows](https://img.shields.io/badge/platform-Windows-0078D6.svg)
![Linux](https://img.shields.io/badge/platform-Linux-blue.svg)
![Qt 6](https://img.shields.io/badge/built_with-Qt_6-41CD52.svg)

[Download](#install-on-windows) · [What this build adds](#what-this-build-adds) · [Features](#features) · [Build from source](#build-from-source) · [Troubleshooting](#troubleshooting)

</div>

> **About this repository.** [Sung](https://github.com/yappologistic/Sung) is a beautiful, minimal Material 3 player written in C++ and Qt Quick by [**@yappologistic**](https://github.com/yappologistic). All credit for the player belongs to them — go there for the original Linux project, and consider [buying them a coffee](https://buymeacoffee.com/e_gurl). This repository packages that player for Windows as a portable zip and adds three small community features on top. Everything else here is Sung, unchanged.

## What this build adds

- **SponsorBlock** — sponsor reads, self-promotion, long intros and other non-music segments are skipped automatically, using community timings from [sponsor.ajay.app](https://sponsor.ajay.app). Off by default; turn it on under **Settings → Playback → Skip non-music segments**. While the switch is off, nothing is sent anywhere.
- **Discord Rich Presence** — the playing track appears on your Discord profile with its cover, elapsed time and a button back to the song, through Discord's own local pipe connection. No token, no extra account, no third-party service. Off by default; enable under **Settings → Connections → Discord**.
- **Proxy support** — every outbound request (YouTube Music, yt-dlp, lyrics, artwork, SponsorBlock) can go through an HTTP or SOCKS5 proxy under **Settings → Connections → Network**, for example `http://127.0.0.1:8080` or `socks5://host:1080`.

## Install on Windows

1. Download `Sung-windows-x64.zip` from the [**Releases**](https://github.com/mewshiam/Sung-windows/releases) page.
2. Unzip it anywhere — `C:\Tools\Sung`, a user folder or a USB drive all work. No installer, no admin rights.
3. Run `Sung\bin\sung.exe`.

The zip is self-contained: Qt, an embedded Python runtime for the YouTube helper, and ffmpeg all ship inside it.

```
Sung\
├── bin\        sung.exe, Qt DLLs and plugins, MSVC runtime, ffmpeg/ffprobe, qt.conf
├── helper\     the Python sources (also embedded in the exe as resources)
├── runtime\    embedded Python with ytmusicapi + yt-dlp preinstalled
├── licenses\   third-party license texts
├── LICENSE · NOTICE · README.md
```

- **SmartScreen** may warn on first launch ("Windows protected your PC"). The exe is unsigned; choose *More info → Run anyway*.
- Settings and library data live under `%APPDATA%\Sung`, like any well-behaved Windows program.
- Keep the zip's folder structure: `runtime\` must stay next to `bin\`, or the YouTube helper cannot start.

### Notes and limitations on Windows

- **Media keys and the system media flyout** are not wired up yet — Windows has no D-Bus for MPRIS, so all playback control lives in the app itself (the mini player included). WinRT's `SystemMediaTransportControls` is the natural next step.
- **Now-playing notifications** appear as tray balloon toasts; the tray icon exists only while a toast can show and hides again when playback pauses.
- **"Remember me" for server logins** degrades to a per-session sign-in with a clear message from the app (Linux stores these in the system keyring via `secret-tool`, which has no Windows CLI equivalent).
- **Pause on headphone disconnect**: the PulseAudio fine-tuning is skipped; a vanished audio device still pauses playback through Qt's own device notifications.
- The **single-instance guard** uses a named pipe derived from your profile path, so two Windows users each get their own instance, and double-clicking the exe again raises the existing window.
- Song titles and searches in **any language** work: the helper speaks UTF-8 end to end.

## Features

- **YouTube Music**: search songs, albums, artists and playlists; play audio without an embedded browser or ad interface, at standard quality or a data saver setting.
- **Navidrome / Subsonic**: browse and search your server, play original or transcoded audio, edit server playlists, rate songs, sync favorites and listening history, and display server lyrics.
- **Jellyfin**: browse music libraries, albums, artists and genres; search, stream original or transcoded audio, manage permitted server playlists, sync favorites and display synchronized lyrics.
- **Your music**: import FLAC, MP3 and other supported audio files or folders; browse albums and artists, search paths and group songs by folder. Mix local and YouTube songs in the same playlists.
- **Animated artwork**: local animated covers and automatic online covers for matching YouTube songs, shared across the player, immersive view and mini player; lists use still covers.
- **Appearance**: light and dark themes, a pickable Material accent color, artwork-derived color, an ambient cover backdrop, density and per-view layouts.
- **Lyrics**: synchronized lyrics, an immersive view, optional poster-style lines, timing adjustments, LRC import, seek previews and search with jump-to-line playback.
- **Offline**: songs you have played are kept on disk under a limit you set, so a replay starts at once and needs no network.
- **Library tools**: likes, listening history, smart mixes, custom smart playlists, M3U playlist import and export, custom playlist covers, playlist cleanup, multi-selection, drag reordering and Undo.
- **Playback controls**: mini player, queue editing with source headings, an immersive up-next carousel, volume normalization, shuffle, repeat, sleep timer, playback speed and audio-device selection.
- **Keyboard and assistive use**: every control takes focus and shows it, sections are marked as headings, and colors are solved to keep 4.5:1 contrast in both themes and at either contrast setting.
- **Desktop integration**: media keys through MPRIS on Linux, optional notifications, light/dark themes and Noctalia palette support.
- **SponsorBlock**, **Discord Rich Presence** and **Proxy** as described above — each one switched off until you turn it on in Settings.

Native rendering and bounded artwork caches keep Sung lightweight. Animations can be disabled in Settings.

## Using Sung

On first run Sung offers a three-step setup: theme and accent color, a music folder, and the page to open on. Every step can be skipped, and each control also lives in Settings.

**YouTube.** Search from the capsule at the top of the window, or paste a song, album or playlist link. Playback controls, the queue (**Ctrl+L**) and the immersive player (**F11**) are one click away. Streaming quality and offline keeping are under **Settings → Privacy & data**.

**Local files.** Use **Library → Local files → +** to import files, or **Folders → Add folder…** for a whole music folder (subfolders are scanned recursively and updates are watched while Sung runs). Local and YouTube songs mix freely in the same playlists.

**Music servers.** Open **Settings → Connections → Music server**, choose **Subsonic** (including Navidrome) or **Jellyfin**, and enter the server root address, username and password. One server account can be connected at a time; a "remember me" checkbox is offered where the platform can store credentials. Local playlists can mix YouTube, local and server songs; server playlists accept songs from that server only.

**Playlists and tools.** Create smart playlists from artist, title, year, length, source and liked-status rules; import and export M3U; drag to reorder; use **Clean up** to review duplicates and missing files. Up to 20 listening sessions save your queue and playback settings for later.

**Lyrics.** Synchronized lyrics load for YouTube and server songs; import your own `.lrc`, adjust timing, or open the immersive view with poster-style lines.

**Privacy.** YouTube browsing is anonymous; likes, playlists and history stay on your machine and never sync with a Google account. There is no analytics or telemetry. Optional lyric lookups send only the song's title, artist and duration and can be disabled. Library data can be exported and re-imported as JSON. On Windows everything lives under `%APPDATA%\Sung`; on Linux under the usual XDG directories.

### Keyboard shortcuts

| Shortcut | Action |
| --- | --- |
| Space | Play / pause |
| Ctrl+F | Focus search |
| Ctrl+Shift+P | Quick actions |
| Ctrl+J | Show the playing song in the queue |
| ? / F1 | Keyboard shortcut reference (outside text fields) |
| Ctrl+M | Toggle mini player |
| F11 | Toggle immersive player |
| 0 – 9 | Jump to that tenth of the track |
| Ctrl+A | Select songs in the focused list |
| Escape | Close the current view or clear selection |

The full, day-to-day manual — artwork pipelines, volume normalization details, server integration fine print and more — lives in the [upstream README](https://github.com/yappologistic/Sung#readme).

## Build from source

### Windows

Requirements: **Qt 6.10 or newer** (MSVC 2022 64-bit kit with Core, Concurrent, Gui, Quick, Qml, QuickControls2, Multimedia, Network and Svg — the online installer's default "Qt 6.10 for desktop development" covers all of them; the QML bytecode step uses `DISCARD_QML_CONTENTS`, which older Qt releases lack), **Visual Studio 2022** with the C++ toolset (or just the Build Tools), and **CMake 3.24+** with **Ninja**. Python is not needed to build; pip/PyPI access is needed once to prepare the bundled helper runtime.

From an *x64 Native Tools Command Prompt for VS 2022*:

```bat
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build

mkdir Sung\bin
copy build\sung.exe Sung\bin\
C:\Qt\6.10.3\msvc2022_64\bin\windeployqt.exe --release --no-translations ^
    --no-system-d3d-compiler --no-opengl-sw Sung\bin\sung.exe
copy helper\catalog.py helper\online_artwork.py helper\requirements.txt Sung\helper\
```

`build\sung.exe` embeds the QML, icons and the helper's Python scripts as Qt resources; it needs Qt's DLLs beside it, which `windeployqt` provides. To reproduce the fully self-contained zip that CI produces (embedded Python with `ytmusicapi` and `yt-dlp` preinstalled, ffmpeg beside the exe), read [`.github/workflows/windows.yml`](.github/workflows/windows.yml) — it is the authoritative recipe, step by step.

### Linux

Sung is a Linux player first. On CachyOS/Arch:

```bash
sudo pacman -S --needed git base-devel cmake ninja python nodejs ffmpeg qt6-base qt6-declarative qt6-multimedia qt6-svg qt6-wayland qt6-imageformats
git clone https://github.com/yappologistic/Sung.git && cd Sung
./scripts/install.sh
```

Installation is per-user in `~/.local`; do not run the install script with `sudo`. Other distributions need the equivalent Qt 6.8+ packages (Core, Gui, Quick, Qml, QuickControls2, Multimedia, Network, DBus, Svg, Wayland and the image formats), a C++20 compiler, CMake 3.24+, Ninja, Python 3, Node.js 20+ and FFmpeg.

For development on either platform: `./scripts/setup.sh`, `./scripts/build.sh`, `./scripts/run.sh`; tests via `./scripts/test.sh`.

## Troubleshooting

- **"YouTube helper could not start"** — the `runtime\` folder must sit next to `helper\` and `bin\`. If you moved files, keep the zip's structure.
- **Playback problems** — playback depends on YouTube availability, region and network conditions. Sung buffers a whole song before playing it, so starting a track can take a moment.
- **YouTube playback breaks after an upstream change** — update the resolvers inside the package with the bundled interpreter:

  ```bat
  Sung\runtime\python.exe -m pip install --upgrade "yt-dlp[default]" ytmusicapi
  ```

  …or simply grab the newest release, which ships current versions.
- **SmartScreen warning** — the portable exe is unsigned; *More info → Run anyway* is expected until a signing certificate is set up.
- **Blank or generic file icon** — Windows caches file icons per path. After replacing the exe, run `ie4uinit.exe -show` from the Run dialog (Win+R), or simply move or rename the `Sung` folder; the icon then appears. The icon is embedded in the exe in the classic multi-size bitmap format, so no special viewer is needed.
- **No sound / codec issues** — Qt Multimedia uses Windows Media Foundation with its own ffmpeg runtime; copying the package folder wholesale avoids missing-DLL problems.

## Credits and license

- **[Sung](https://github.com/yappologistic/Sung)** and everything that makes it good — [**@yappologistic**](https://github.com/yappologistic). Support the project on [Buy Me a Coffee](https://buymeacoffee.com/e_gurl).
- Windows packaging, SponsorBlock integration, Discord Rich Presence and the proxy option — the community build in this repository.
- [SponsorBlock](https://sponsor.ajay.app) community segment data; Discord via its local IPC; lyrics from [LRCLIB](https://lrclib.net); artwork matching via Apple Music's public pages and [MusicBrainz](https://musicbrainz.org).

[MIT](LICENSE). Material Symbols are licensed under Apache-2.0; see [NOTICE](NOTICE) for third-party acknowledgments. Sung is an independent project and is not affiliated with Google, YouTube or Discord.
