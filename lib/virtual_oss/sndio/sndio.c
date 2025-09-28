/*-
 * Copyright (c) 2021 Tim Creech <tcreech@tcreech.com>
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

#include <sys/queue.h>
#include <sys/soundcard.h>

#include <err.h>
#include <stdlib.h>
#include <string.h>

#include <sndio.h>

#include "backend.h"
#include "int.h"

static struct sio_hdl *
get_sio_hdl(struct voss_backend *pbe)
{
	if (pbe)
		return (pbe->arg);

	return (NULL);
}

static void
sndio_close(struct voss_backend *pbe)
{
	if (!pbe)
		return;

	if (get_sio_hdl(pbe))
		sio_close(get_sio_hdl(pbe));
}

static int
sndio_get_signedness(int *fmt)
{
	int s_fmt = *fmt & (VPREFERRED_SLE_AFMT | VPREFERRED_SBE_AFMT);

	if (s_fmt) {
		*fmt = s_fmt;
		return (1);
	}
	*fmt = *fmt & (VPREFERRED_ULE_AFMT | VPREFERRED_UBE_AFMT);
	return (0);
}

static int
sndio_get_endianness_is_le(int *fmt)
{
	int le_fmt = *fmt & (VPREFERRED_SLE_AFMT | VPREFERRED_ULE_AFMT);

	if (le_fmt) {
		*fmt = le_fmt;
		return (1);
	}
	*fmt = *fmt & (VPREFERRED_SBE_AFMT | VPREFERRED_UBE_AFMT);
	return (0);
}

static int
sndio_get_bits(int *fmt)
{
	if (*fmt & AFMT_16BIT)
		return (16);
	if (*fmt & AFMT_24BIT)
		return (24);
	if (*fmt & AFMT_32BIT)
		return (32);
	if (*fmt & AFMT_8BIT)
		return (8);
	return (-1);
	/* TODO AFMT_BIT */
}

static int
sndio_open(struct voss_backend *pbe, const char *devname,
    int samplerate, int bufsize, int *pchannels, int *pformat, int direction)
{
	const char *sndio_name = devname + strlen("/dev/sndio/");

	int sig = sndio_get_signedness(pformat);
	int le = sndio_get_endianness_is_le(pformat);
	int bits = sndio_get_bits(pformat);

	if (bits == -1) {
		warn("unsupported format precision");
		return (-1);
	}

	struct sio_hdl *hdl = sio_open(sndio_name, direction, 0);

	if (hdl == 0) {
		warn("sndio: failed to open device");
		return (-1);
	}

	struct sio_par par;

	sio_initpar(&par);
	par.pchan = *pchannels;
	par.sig = sig;
	par.bits = bits;
	par.bps = SIO_BPS(bits);
	par.le = le;
	par.rate = samplerate;
	par.appbufsz = bufsize;
	par.xrun = SIO_SYNC;
	if (!sio_setpar(hdl, &par))
		errx(1, "internal error, sio_setpar() failed");
	if (!sio_getpar(hdl, &par))
		errx(1, "internal error, sio_getpar() failed");
	if ((int)par.pchan != *pchannels)
		errx(1, "couldn't set number of channels");
	if ((int)par.sig != sig || (int)par.bits != bits || (int)par.le != le)
		errx(1, "couldn't set format");
	if ((int)par.bits != bits)
		errx(1, "couldn't set precision");
	if ((int)par.rate < samplerate * 995 / 1000 ||
	    (int)par.rate > samplerate * 1005 / 1000)
		errx(1, "couldn't set rate");
	if (par.xrun != SIO_SYNC)
		errx(1, "couldn't set xun policy");

	/* Save the device handle with the backend */
	pbe->arg = hdl;

	/* Start the device. */
	if (!sio_start(hdl))
		errx(1, "couldn't start device");

	return (0);
}

static int
sndio_open_play(struct voss_backend *pbe, const char *devname,
    int samplerate, int bufsize, int *pchannels, int *pformat)
{
	return (sndio_open(pbe, devname, samplerate, bufsize, pchannels, pformat, SIO_PLAY));
}

static int
sndio_open_rec(struct voss_backend *pbe, const char *devname,
    int samplerate, int bufsize, int *pchannels, int *pformat)
{
	return (sndio_open(pbe, devname, samplerate, bufsize, pchannels, pformat, SIO_REC));
}

static int
sndio_play_transfer(struct voss_backend *pbe, void *ptr, int len)
{
	return (sio_write(get_sio_hdl(pbe), ptr, len));
}

static int
sndio_rec_transfer(struct voss_backend *pbe, void *ptr, int len)
{
	return (sio_read(get_sio_hdl(pbe), ptr, len));
}

static void
sndio_delay(struct voss_backend *pbe __unused, int *pdelay)
{
	*pdelay = -1;
}

struct voss_backend voss_backend_sndio_rec = {
	.open = sndio_open_rec,
	.close = sndio_close,
	.transfer = sndio_rec_transfer,
	.delay = sndio_delay,
	.fd = -1,
};

struct voss_backend voss_backend_sndio_play = {
	.open = sndio_open_play,
	.close = sndio_close,
	.transfer = sndio_play_transfer,
	.delay = sndio_delay,
	.fd = -1,
};
