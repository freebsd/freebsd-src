/* $FreeBSD$ */

/*-
 * Copyright (c) 2000-2002 Hiroyuki Aizu <aizu@navi.org>
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

#define NO_RECORDING /* XXX: some routines missing from uaudio.c */

/* Defined in uaudio.c, used in uaudio_pcm,c */

void	uaudio_chan_set_param_pcm_dma_buff(device_t dev, u_char *start,
		u_char *end, struct pcm_channel *pc);
int	uaudio_trigger_output(device_t dev);
int	uaudio_halt_out_dma(device_t dev);
#ifndef NO_RECORDING
int	uaudio_trigger_input(device_t dev);
int	uaudio_halt_in_dma(device_t dev);
#endif
void	uaudio_chan_set_param(device_t, u_char *, u_char *);
void	uaudio_chan_set_param_blocksize(device_t dev, u_int32_t blocksize);
void	uaudio_chan_set_param_speed(device_t dev, u_int32_t speed);
void	uaudio_chan_set_param_format(device_t dev, u_int32_t format);
int	uaudio_chan_getptr(device_t dev);
void	uaudio_mixer_set(device_t dev, unsigned type, unsigned left,
		unsigned right);
u_int32_t uaudio_query_mix_info(device_t dev);
void	uaudio_query_formats(device_t dev, u_int32_t *pfmt, u_int32_t *rfmt);

