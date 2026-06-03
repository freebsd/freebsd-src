/*
 * Copyright (c) 2026 The FreeBSD Mobile Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _MOBILE_GFX_CURSOR_H_
#define _MOBILE_GFX_CURSOR_H_

#include <stdint.h>
#include <stdbool.h>
#include "drm.h"

/* Cursor size */
#define CURSOR_WIDTH  64
#define CURSOR_HEIGHT 64

/* Public API */
int cursor_init(int drm_fd);
void cursor_fini(void);
int cursor_set(uint32_t crtc_id, int x, int y, uint32_t bo_handle);
int cursor_move(uint32_t crtc_id, int x, int y);
int cursor_hide(uint32_t crtc_id);
int cursor_show(uint32_t crtc_id);

/* Software fallback */
void cursor_render_software(uint8_t *buffer, int width, int height, int pitch,
                            int x, int y, uint32_t fg, uint32_t bg);

#endif /* _MOBILE_GFX_CURSOR_H_ */