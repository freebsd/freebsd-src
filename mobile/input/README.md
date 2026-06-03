# Mobile Input Subsystem

This is a Linux-compatible input subsystem for a BSD-based mobile operating system.
It reads from Linux evdev (/dev/input/event*) and provides multi-touch, gestures,
keyboard, and mouse support.

## Components

- **evdev.h + evdev.c**: Low-level evdev interface with auto-discovery.
- **input_backend.h + input_backend.c**: Unified input backend that manages multiple devices.
- **multitouch.h + multitouch.c**: Multi-touch touchpoint tracking.
- **gestures2.h + gestures2.c**: Advanced gesture recognition (tap, double-tap, long-press, swipe, pinch, rotate, edge swipe, pull-down).
- **keyboard.h + keyboard.c**: Keyboard handling with modifier and key repeat support.
- **pointing.h + pointing.c**: Mouse/trackpad pointer handling with acceleration.
- **input_hal.h + input_hal.c**: Hardware abstraction layer for device enumeration and hotplug.
- **Makefile**: Builds a static library.
- **README.md**: This file.

## Usage

See the header files for function documentation.

## Building

    make

## License

BSD 2-Clause License