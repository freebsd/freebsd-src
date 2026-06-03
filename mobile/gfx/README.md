# FreeBSD Mobile Graphics Stack

This directory contains a low-level graphics stack for a BSD-based mobile OS, providing hardware-accelerated rendering via DRM/KMS, GBM, EGL, and OpenGL ES. It includes fallback software rendering for systems without hardware acceleration.

## Components

### DRM (`drm.h` + `drm.c`)
Direct Rendering Manager client for kernel graphics hardware interaction.
- Opens DRM device (`/dev/dri/card0`)
- Enumerates resources (connectors, CRTCs, encoders, framebuffers)
- Finds connectors by type (HDMI, DP, eDP, LVDS, DSI)
- Sets display modes
- Handles page flips and vertical blank synchronization
- Creates framebuffers via GBM dumb buffers
- Detects GPU vendors (Intel, AMD, ARM, Qualcomm)

### GBM (`gbm.h` + `gbm.c`)
Generic Buffer Management for buffer object creation and surface handling.
- Creates GBM device from DRM file descriptor
- Creates buffer objects (BOs) for rendering and scanout
- Creates GBM surfaces for double/triple buffering
- Maps BOs for CPU access
- Exchanges buffers with EGL surfaces
- Supports format negotiation and modifier handling

### EGL (`egl.h` + `egl.c`)
EGL wrapper for display, context, and surface management.
- Wraps native display (DRM FD) as EGLDisplay
- Initializes EGL and selects configurations
- Creates EGL contexts and surfaces (window, pbuffer)
- Manages rendering context (makeCurrent, swapBuffers)
- Supports EGL image creation for zero-copy dmabuf import
- Provides EGL_PLATFORM_DEVICE_EXT for headless rendering

### OpenGL ES (`gl.h` + `gl.c`)
OpenGL ES 2.0/3.0 function pointer wrapper and utility functions.
- Loads GL function pointers via eglGetProcAddress
- Shader compilation and linking utilities
- Texture creation and upload
- Textured quad rendering for UI elements
- Includes built-in shader sources:
  * Passthrough vertex shader (position + texcoord)
  * Solid color fragment shader
  * Texture fragment shader
  * Gradient fragment shader (for wallpapers/panels)

### Hardware Cursor (`cursor.h` + `cursor.c`)
Hardware-accelerated cursor support with software fallback.
- Initializes 64x64 ARGB cursor buffer object
- Moves, shows, hides cursor via DRM IOCTLs
- Software fallback renders cursor into compositor buffer

### GPU Detection (`gpu_detect.h` + `gpu_detect.c`)
Automatic GPU detection and capability reporting.
- Scans `/sys/class/drm` and `/sys/bus/pci/devices`
- Identifies GPU vendor and device IDs
- Determines 3D acceleration and OpenGL support
- Reports maximum supported resolution

### Software Renderer (`renderer.h` + `renderer.c`)
2D software rendering fallback for systems without GL.
- Pixel buffer management
- Rectangle filling and outlining
- Blitting with scaling
- Circle and line drawing
- Gradient fills
- Clipping support

## Building

```bash
make
```

Produces `libgfx.a` containing all graphics functionality.

## Dependencies

- libdrm (Direct Rendering Manager)
- libgbm (Generic Buffer Manager)
- libEGL (EGL library)
- libGLESv2 (OpenGL ES 2.0)
- Standard C library

## Usage

Initialize the graphics stack in this order:

1. Detect GPU: `gpu_probe(&gpu_info)`
2. Open DRM device: `drm_open("/dev/dri/card0", &drm_dev)`
3. Create GBM device: `gbm_dev = gbm_device_create(drm_dev->fd)`
4. Create EGL display: `egl_disp = egl_get_display(drm_dev->fd)`
5. Initialize EGL: `egl_initialize(egl_disp, &major, &minor)`
6. Select EGL config and create context/surface
7. Initialize GL: `gl_init(&gl, egl_disp)`
8. Create framebuffers via DRM/GBM for rendering
9. Render using GL or software renderer
10. Swap buffers via EGL or DRM page flip
11. Manage hardware cursor via cursor API

See individual header files for detailed API documentation.

## GPU Support

Tested with:
- Intel HD Graphics (i915 driver)
- AMD Radeon (amdgpu driver)
- ARM Mali (lima/panfrost drivers)
- Qualcomm Adreno (freedreno driver)

## License

BSD 2-Clause License. See individual source files for details.