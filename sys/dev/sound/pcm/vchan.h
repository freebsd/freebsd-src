/*-
 * Copyright (c) 2001 Cameron Grant <cg@freebsd.org>
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
 *
 * $FreeBSD: src/sys/dev/sound/pcm/vchan.h,v 1.5 2007/05/31 18:43:32 ariff Exp $
 */

int vchan_create(struct pcm_channel *parent, int num);
int vchan_destroy(struct pcm_channel *c);
int vchan_initsys(device_t dev);

/*
 * Default speed / format
 */
#define VCHAN_DEFAULT_SPEED	48000
#define VCHAN_DEFAULT_AFMT	(AFMT_S16_LE | AFMT_STEREO)
#define VCHAN_DEFAULT_STRFMT	"s16le"

#define VCHAN_PLAY		0
#define VCHAN_REC		1

/*
 * Offset by +/- 1 so we can distinguish bogus pointer.
 */
#define VCHAN_SYSCTL_DATA(x, y)						\
		((void *)((intptr_t)(((((x) + 1) & 0xfff) << 2) |	\
		(((VCHAN_##y) + 1) & 0x3))))

#define VCHAN_SYSCTL_DATA_SIZE	sizeof(void *)
#define VCHAN_SYSCTL_UNIT(x)	((int)(((intptr_t)(x) >> 2) & 0xfff) - 1)
#define VCHAN_SYSCTL_DIR(x)	((int)((intptr_t)(x) & 0x3) - 1)
