# UOS Wayland Compositor

A production-grade Wayland compositor implementation for the BSD-based Mobile OS.

## Overview

This compositor provides a complete Wayland display server with:

- **DRM/KMS Backend**: Direct kernel mode setting for display management
- **GBM Buffer Allocation**: GPU buffer management for client surfaces
- **OpenGL ES 2.0 Renderer**: Hardware-accelerated compositing
- **Input Handling**: Multi-touch, pointer, and keyboard support via evdev
- **xdg-shell Support**: Window management protocol for desktop applications

## Architecture

```
+------------------+
|   wl_display     |
+--------+---------+
         |
+--------v---------+     +------------------+
|   compositor.c   |---->|    backend.c     |
+--------+---------+     |  (DRM/GBM)       |
         |               +------------------+
+--------v---------+     +------------------+
|   renderer.c     |---->|  EGL/OpenGL ES   |
+--------+---------+     +------------------+
         |
+--------v---------+     +------------------+
|     seat.c       |---->|    evdev         |
+--------+---------+     +------------------+
         |
+--------v---------+
|   xdg-shell.c    |
+--------+---------+
         |
+--------v---------+
|    window.c      |
+------------------+
```

## Files

| File | Description |
|------|-------------|
| `compositor.h/c` | Core compositor: Wayland display, event loop, surface management |
| `backend.h/c` | DRM/KMS backend with GBM buffer allocation |
| `renderer.h/c` | OpenGL ES renderer for compositing |
| `seat.h/c` | Input seat: pointer, keyboard, touch |
| `protocols.h` | Wayland protocol stubs and definitions |
| `xdg-shell.h/c` | xdg-shell protocol implementation |
| `window.h/c` | Window management: state, z-order, visibility |
| `shell.h/c` | Shell integration: lock screen, overlays, popups |
| `output.h/c` | Display output: mode setting, scaling, hotplug |

## Usage

```c
#include <uos/compositor.h>

int main(void) {
    uos_backend_t backend;

    if (comp_init(&backend) != 0) {
        fprintf(stderr, "Failed to initialize compositor\n");
        return 1;
    }

    comp_run();  /* Blocks in event loop */

    comp_shutdown();
    return 0;
}
```

## Building

```sh
make
sudo make install
```

## Dependencies

- libdrm - Direct Rendering Manager
- libgbm - Generic Buffer Manager
- libinput - Input device handling
- libEGL - Khronos EGL
- libGLESv2 - OpenGL ES 2.0

## License

BSD-style license. See individual files for copyright notices.