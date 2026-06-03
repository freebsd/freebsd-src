# Audio Subsystem for BSD-based Mobile OS

This directory contains the audio subsystem for the mobile OS, including:

- Audio pipeline management
- Mixer with per-channel volume control
- Digital signal processing (EQ, noise suppression, etc.)
- ALSA compatibility layer for audio hardware
- Audio routing (speaker, headphone, Bluetooth, etc.)
- Central audio daemon (audiod) for system-wide audio management

## Components

- `pipeline.h/c` - Audio processing pipeline
- `mixer.h/c` - Audio mixer with multiple channels
- `dsp.h/c` - Digital signal processing functions
- `alsa_compat.h/c` - ALSA-compatible interface to audio hardware
- `route.h/c` - Audio routing management
- `audio_server.h/c` - Central audio daemon with UNIX socket IPC

## Building

Run `make` to build the static library `libaudio.a` and the audio server daemon `audiod`.

Run `make clean` to remove generated files.

## Usage

The audio server daemon listens on UNIX socket `/var/run/audio.sock` and provides an ASRPC interface for controlling audio settings.

See individual header files for API documentation.