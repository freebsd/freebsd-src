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
 * $FreeBSD$
 */

int mixer_init(device_t dev, kobj_class_t cls, void *devinfo);
int mixer_uninit(device_t dev);
int mixer_reinit(device_t dev);
int mixer_ioctl(struct snddev_info *d, u_long cmd, caddr_t arg);
int mixer_busy(struct snd_mixer *m, int busy);
int mixer_isbusy(struct snd_mixer *m);

int mixer_hwvol_init(device_t dev);
void mixer_hwvol_mute(device_t dev);
void mixer_hwvol_step(device_t dev, int left_step, int right_step);

void mix_setdevs(struct snd_mixer *m, u_int32_t v);
void mix_setrecdevs(struct snd_mixer *m, u_int32_t v);
u_int32_t mix_getdevs(struct snd_mixer *m);
u_int32_t mix_getrecdevs(struct snd_mixer *m);
void *mix_getdevinfo(struct snd_mixer *m);

/*
 * this is a kludge to allow hiding of the struct snd_mixer definition
 * 512 should be enough for all architectures
 */
#define	MIXER_SIZE	(512 + sizeof(struct kobj))

#define MIXER_DECLARE(name) DEFINE_CLASS(name, name ## _methods, MIXER_SIZE)
