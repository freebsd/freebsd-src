# uOS(m) Desktop Shell

Full desktop environment for the BSD-based Mobile OS. Provides the visual shell: wallpapers, icons, dock, panel, notifications, lock screen, animations, and workspace management.

## Components

| Module | Description |
|--------|-------------|
| `wallpaper` | Wallpaper engine - static, gradient, solid, animated |
| `icon_theme` | Icon system with LRU cache and fallback generation |
| `dock` | Application dock at bottom/left/right |
| `panel` | Top system panel with applets |
| `notifications` | Notification system with popups and history |
| `lockscreen` | Lock screen with clock and shortcuts |
| `overview` | App switcher / overview (grid layout) |
| `workspace` | Virtual desktop manager |
| `animations` | Spring physics and easing animations |
| `search` | Global search/run with inline calculator |
| `app_launcher` | .desktop file parser and launcher |
| `task_switcher` | Alt+Tab task switcher |
| `theme` | Theme engine with CSS-like stylesheets |
| `panel_applets` | Clock, battery, network, sound, tray, power |

## Building

```bash
cd E:\freebsd-src\mobile
cd desktop
make
```

This builds all sub-modules in the desktop shell.

## Architecture

Each module is a self-contained library with:
- Header file (`.h`) defining public API
- Implementation (`.c`) with internal state
- Private `Makefile` per module

The desktop shell runs as a privileged client of the Wayland compositor, using the DRM/GBM/GL stack from `mobile/gfx` and rendering via `compositor_core`.

## Display Support

- HiDPI (2x) displays supported
- Per-monitor wallpaper
- Adaptive layout for portrait/landscape

## License

BSD 3-Clause
