/*
 * Copyright (c) 1999 Cameron Grant <gandalf@vilnya.demon.co.uk>
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
 * $FreeBSD: src/sys/dev/sound/pcm/mixer.h,v 1.2.2.2 2000/07/19 21:18:46 cg Exp $
 */

extern int mixer_init(snddev_info *d, snd_mixer *m, void *devinfo);
extern int mixer_reinit(snddev_info *d);
extern int mixer_set(snddev_info *d, unsigned dev, unsigned lev);
extern int mixer_get(snddev_info *d, int dev);
extern int mixer_setrecsrc(snddev_info *d, u_int32_t src);
extern int mixer_getrecsrc(snddev_info *d);
extern int mixer_ioctl(snddev_info *d, u_long cmd, caddr_t arg);

extern void change_bits(mixer_tab *t, u_char *regval, int dev, int chn, int newval);

void mix_setdevs(snd_mixer *m, u_int32_t v);
void mix_setrecdevs(snd_mixer *m, u_int32_t v);
u_int32_t mix_getdevs(snd_mixer *m);
u_int32_t mix_getrecdevs(snd_mixer *m);
void *mix_getdevinfo(snd_mixer *m);
