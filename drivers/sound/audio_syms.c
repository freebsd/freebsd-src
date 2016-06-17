/*
 * Exported symbols for audio driver.
 * __NO_VERSION__ because this is still part of sound.o.
 */

#define __NO_VERSION__
#include <linux/module.h>

char audio_syms_symbol;

#include "sound_config.h"
#include "sound_calls.h"

EXPORT_SYMBOL(DMAbuf_start_dma);
EXPORT_SYMBOL(DMAbuf_open_dma);
EXPORT_SYMBOL(DMAbuf_close_dma);
EXPORT_SYMBOL(DMAbuf_inputintr);
EXPORT_SYMBOL(DMAbuf_outputintr);
EXPORT_SYMBOL(dma_ioctl);
EXPORT_SYMBOL(audio_open);
EXPORT_SYMBOL(audio_release);
