/*-
 * Copyright (c) 2012-2022 Hans Petter Selasky
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
#include <sys/types.h>
#include <sys/filio.h>
#include <sys/rtprio.h>
#include <sys/nv.h>
#include <sys/sndstat.h>
#include <sys/soundcard.h>

#include <dlfcn.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <err.h>
#include <sysexits.h>
#include <signal.h>
#include <fcntl.h>
#include <paths.h>

#include <cuse.h>
#include <pthread.h>

#include "backend.h"
#include "int.h"
#include "utils.h"
#include "virtual_oss.h"

pthread_mutex_t atomic_mtx;
pthread_cond_t atomic_cv;

static void
atomic_init(void)
{
	if (pthread_mutex_init(&atomic_mtx, NULL) != 0)
		err(1, "pthread_mutex_init");
	if (pthread_cond_init(&atomic_cv, NULL) != 0)
		err(1, "pthread_cond_init");
}

uint32_t
vclient_sample_bytes(vclient_t *pvc)
{
	uint32_t fmt = pvc->format;

	if (fmt & AFMT_16BIT)
		return (2);
	else if (fmt & AFMT_24BIT)
		return (3);
	else if (fmt & AFMT_32BIT)
		return (4);
	else if (fmt & AFMT_8BIT)
		return (1);
	else
		return (0);
	/* TODO AFMT_BPS */
}

static uint32_t
vclient_output_delay(vclient_t *pvc)
{
	uint64_t size;
	uint64_t mod;

	if (pvc->tx_busy == 0)
		vclient_import_write_locked(pvc);

	mod = pvc->channels * vclient_sample_bytes(pvc);

	size = vring_total_read_len(&pvc->tx_ring[0]);
	size = (size / 8) * vclient_sample_bytes(pvc);

	size = (size * (uint64_t)pvc->sample_rate) /
	    (uint64_t)voss_dsp_sample_rate;
	size += vring_total_read_len(&pvc->tx_ring[1]);
	size -= size % mod;

	return (size);
}

static uint32_t
vclient_input_delay(vclient_t *pvc)
{
	if (pvc->rx_busy == 0)
		vclient_export_read_locked(pvc);
	return (vring_total_read_len(&pvc->rx_ring[1]));
}

uint32_t
vclient_bufsize_scaled(vclient_t *pvc)
{
	uint32_t samples_scaled = ((uint64_t)voss_dsp_samples *
	    (uint64_t)pvc->sample_rate) / (uint64_t)voss_dsp_sample_rate;
	if (samples_scaled == 0)
		samples_scaled = 1;
	return (pvc->channels * samples_scaled * vclient_sample_bytes(pvc));
}

static uint64_t
vclient_bufsize_consumed(vclient_t *pvc, uint64_t ts)
{
	int64_t delta;
	int64_t samples_scaled;
	int64_t retval;

	delta = virtual_oss_timestamp() - ts;
	if (delta < 0)
		delta = 0;
	samples_scaled = (delta * (uint64_t)pvc->sample_rate) / 1000000000ULL;
	if (samples_scaled < 0)
		samples_scaled = 0;
	retval = pvc->channels * samples_scaled * vclient_sample_bytes(pvc);
	if (retval < 0)
		retval = 0;
	return (retval);
}

/*
 * VLC and some other audio player use this value for jitter
 * computations and expect it to be very accurate. VirtualOSS is block
 * based and does not have sample accuracy. Use the system clock to
 * update this value as we go along instead:
 */
static uint32_t
vclient_output_delay_adjusted(vclient_t *pvc)
{
	int64_t retval = vclient_output_delay(pvc) -
	    vclient_bufsize_consumed(pvc, pvc->tx_timestamp);
	if (retval < 0)
		retval = 0;
	return (retval);
}

vmonitor_t *
vmonitor_alloc(int *pid, vmonitor_head_t *phead)
{
	int id = 0;
	vmonitor_t *pvm;

	TAILQ_FOREACH(pvm, phead, entry)
	    id++;

	if (id >= 64) {
		*pid = 0;
		return (NULL);
	}
	pvm = malloc(sizeof(*pvm));
	if (pvm == NULL) {
		*pid = 0;
		return (NULL);
	}
	memset(pvm, 0, sizeof(*pvm));

	pvm->mute = 1;

	TAILQ_INSERT_TAIL(phead, pvm, entry);

	*pid = id;
	return (pvm);
}

int64_t
vclient_noise(uint32_t *pnoise, int64_t volume, int8_t shift)
{
	const uint32_t prime = 0xFFFF1DU;
	int64_t temp;

	/* compute next noise sample */
	temp = *pnoise;
	if (temp & 1)
		temp += prime;
	temp /= 2;
	*pnoise = temp;

	/* unsigned to signed conversion */
	temp ^= 0x800000ULL;
	if (temp & 0x800000U)
		temp |= -0x800000ULL;

	/* properly amplify */
	temp *= volume;

	/* bias shift */
	shift -= 23 + VVOLUME_UNIT_SHIFT;

	/* range check and shift noise */
	if (__predict_false(shift < -63 || shift > 63))
		temp = 0;
	else if (shift < 0)
		temp >>= -shift;
	else
		temp <<= shift;

	return (temp);
}

static void
vresample_free(vresample_t *pvr)
{
	if (pvr->state != NULL)
		src_delete(pvr->state);
	free(pvr->data_in);
	free(pvr->data_out);
	memset(pvr, 0, sizeof(*pvr));
}

static int
vresample_setup(vclient_t *pvc, vresample_t *pvr, int samples)
{
	int code = 0;

	if (pvr->state != NULL)
		return (0);
	pvr->state = src_new(voss_libsamplerate_quality, pvc->channels, &code);
	if (pvr->state == NULL)
		goto error;
	pvr->data_in = malloc(sizeof(float) * samples);
	if (pvr->data_in == NULL)
		goto error;
	pvr->data_out = malloc(sizeof(float) * samples);
	if (pvr->data_out == NULL)
		goto error;
	pvr->data.data_in = pvr->data_in;
	pvr->data.data_out = pvr->data_out;
	return (0);
error:
	vresample_free(pvr);
	return (CUSE_ERR_NO_MEMORY);
}

void
vclient_free(vclient_t *pvc)
{
	vresample_free(&pvc->rx_resample);
	vresample_free(&pvc->tx_resample);

	/* free equalizer */
	vclient_eq_free(pvc);

	/* free ring buffers */
	vring_free(&pvc->rx_ring[0]);
	vring_free(&pvc->rx_ring[1]);
	vring_free(&pvc->tx_ring[0]);
	vring_free(&pvc->tx_ring[1]);

	free(pvc);
}

vclient_t *
vclient_alloc(void)
{
	vclient_t *pvc;

	pvc = malloc(sizeof(*pvc));
	if (pvc == NULL)
		return (NULL);

	memset(pvc, 0, sizeof(*pvc));

	pvc->rx_noise_rem = 1;
	pvc->tx_noise_rem = 1;
	pvc->rx_volume = 1 << VVOLUME_UNIT_SHIFT;
	pvc->tx_volume = 1 << VVOLUME_UNIT_SHIFT;

	return (pvc);
}

int
vclient_get_default_fmt(vprofile_t *pvp, int type)
{
	int retval;

	if (type == VTYPE_WAV_HDR) {
		switch (pvp->bits) {
		case 16:
			retval = AFMT_S16_LE;
			break;
		case 24:
			retval = AFMT_S24_LE;
			break;
		case 32:
			retval = AFMT_S32_LE;
			break;
		default:
			retval = AFMT_S8;
			break;
		}
	} else {
		switch (pvp->bits) {
		case 16:
			retval = AFMT_S16_NE;
			break;
		case 24:
			retval = AFMT_S24_NE;
			break;
		case 32:
			retval = AFMT_S32_NE;
			break;
		default:
			retval = AFMT_S8;
			break;
		}
	}
	return (retval);
}

int
vclient_setup_buffers(vclient_t *pvc, int size, int frags,
    int channels, int format, int sample_rate)
{
	size_t bufsize_internal;
	size_t bufsize_min;
	size_t mod_internal;
	size_t mod;
	uint64_t ts;
	int bufsize;

	/* check we are not busy */
	if (pvc->rx_busy || pvc->tx_busy)
		return (CUSE_ERR_BUSY);

	/* free equalizer */
	vclient_eq_free(pvc);

	/* free existing ring buffers */
	vring_free(&pvc->rx_ring[0]);
	vring_free(&pvc->rx_ring[1]);
	vring_free(&pvc->tx_ring[0]);
	vring_free(&pvc->tx_ring[1]);

	/* reset resampler */
	vresample_free(&pvc->rx_resample);
	vresample_free(&pvc->tx_resample);

	if (sample_rate > 0)
		pvc->sample_rate = sample_rate;
	if (format != 0)
		pvc->format = format;
	if (channels > 0)
		pvc->channels = channels;

	mod = pvc->channels * vclient_sample_bytes(pvc);
	mod_internal = pvc->channels * 8;

	if (size > 0) {
		size += mod - 1;
		size -= size % mod;

		pvc->buffer_size = size;
		pvc->buffer_size_set = 1;
	} else if (pvc->buffer_size_set == 0)
		pvc->buffer_size = vclient_bufsize_scaled(pvc);

	pvc->low_water = pvc->buffer_size;

	if (frags > 0) {
		pvc->buffer_frags = frags;
		pvc->buffer_frags_set = 1;
	} else if (pvc->buffer_frags_set == 0)
		pvc->buffer_frags = 2;

	/* sanity checks */
	if (frags < 0 || size < 0)
		return (CUSE_ERR_INVALID);
	if (pvc->format == 0)
		return (CUSE_ERR_INVALID);
	if (pvc->buffer_frags <= 0 || pvc->buffer_frags >= 1024)
		return (CUSE_ERR_INVALID);
	if (pvc->buffer_size <= 0 || pvc->buffer_size >= (1024 * 1024))
		return (CUSE_ERR_INVALID);
	if ((pvc->buffer_size * pvc->buffer_frags) >= (128 * 1024 * 1024))
		return (CUSE_ERR_INVALID);
	if (pvc->channels <= 0 || pvc->channels > pvc->profile->channels)
		return (CUSE_ERR_INVALID);

	/* get buffer sizes */
	bufsize = pvc->buffer_frags * pvc->buffer_size;
	bufsize_internal = ((uint64_t)bufsize * (uint64_t)voss_dsp_sample_rate * 8ULL) /
	  ((uint64_t)pvc->sample_rate * (uint64_t)vclient_sample_bytes(pvc));

	bufsize_min = voss_dsp_samples * pvc->channels * 8;

	/* check for too small buffer size */
	if (bufsize_internal < bufsize_min)
		return (CUSE_ERR_INVALID);

	/* allow for jitter */
	bufsize_internal *= 2ULL;

	/* align buffer size */
	bufsize_internal += (mod_internal - 1);
	bufsize_internal -= (bufsize_internal % mod_internal);

	/* allocate new buffers */
	if (vring_alloc(&pvc->rx_ring[0], bufsize_internal))
		goto err_0;
	if (vring_alloc(&pvc->rx_ring[1], bufsize))
		goto err_1;
	if (vring_alloc(&pvc->tx_ring[0], bufsize_internal))
		goto err_2;
	if (vring_alloc(&pvc->tx_ring[1], bufsize))
		goto err_3;
	if (vclient_eq_alloc(pvc))
		goto err_4;

	ts = virtual_oss_timestamp();

	pvc->rx_samples = 0;
	pvc->tx_samples = 0;
	pvc->tx_timestamp = ts;
	pvc->rx_timestamp = ts;

	return (0);

err_4:
	vring_free(&pvc->tx_ring[1]);
err_3:
	vring_free(&pvc->tx_ring[0]);
err_2:
	vring_free(&pvc->rx_ring[1]);
err_1:
	vring_free(&pvc->rx_ring[0]);
err_0:
	return (CUSE_ERR_NO_MEMORY);
}

static int
vclient_open_sub(struct cuse_dev *pdev, int fflags __unused, int type)
{
	vclient_t *pvc;
	vprofile_t *pvp;
	int error;

	pvp = cuse_dev_get_priv0(pdev);

	pvc = vclient_alloc();
	if (pvc == NULL)
		return (CUSE_ERR_NO_MEMORY);

	pvc->profile = pvp;

	/* setup buffers */
	error = vclient_setup_buffers(pvc, 0, 0, pvp->channels,
	    vclient_get_default_fmt(pvp, type), voss_dsp_sample_rate);
	if (error != 0) {
		vclient_free(pvc);
		return (error);
	}

	pvc->type = type;

	cuse_dev_set_per_file_handle(pdev, pvc);

	atomic_lock();
	/* only allow one synchronization source at a time */
	if (pvc->profile->synchronized) {
		if (voss_has_synchronization != 0)
			error = CUSE_ERR_BUSY;
		else
			voss_has_synchronization++;
	}
	if (error == 0)
		TAILQ_INSERT_TAIL(&pvc->profile->head, pvc, entry);
	atomic_unlock();

	return (error);
}

static int
vclient_open_wav(struct cuse_dev *pdev, int fflags)
{
	return (vclient_open_sub(pdev, fflags, VTYPE_WAV_HDR));
}

static int
vclient_open_oss(struct cuse_dev *pdev, int fflags)
{
	return (vclient_open_sub(pdev, fflags, VTYPE_OSS_DAT));
}

static int
vclient_close(struct cuse_dev *pdev, int fflags __unused)
{
	vclient_t *pvc;

	pvc = cuse_dev_get_per_file_handle(pdev);
	if (pvc == NULL)
		return (CUSE_ERR_INVALID);

	atomic_lock();
	if (pvc->profile->synchronized) {
		voss_has_synchronization--;

		/* wait for virtual_oss_process(), if any */
		while (pvc->sync_busy) {
			pvc->sync_wakeup = 1;
			atomic_wakeup();
			atomic_wait();
		}
	}
	TAILQ_REMOVE(&pvc->profile->head, pvc, entry);
	atomic_unlock();

	vclient_free(pvc);

	return (0);
}

static int
vclient_read_silence_locked(vclient_t *pvc)
{
	size_t size;
	int delta_in;

	delta_in = pvc->profile->rec_delay - pvc->rec_delay;
	if (delta_in < 1)
		return (0);

	size = delta_in * pvc->channels * 8;
	size = vring_write_zero(&pvc->rx_ring[0], size);
	pvc->rec_delay += size / (pvc->channels * 8);

	delta_in = pvc->profile->rec_delay - pvc->rec_delay;
	if (delta_in < 1)
		return (0);

	return (1);
}

static int
vclient_generate_wav_header_locked(vclient_t *pvc)
{
	uint8_t *ptr;
	size_t mod;
	size_t len;

	vring_get_write(&pvc->rx_ring[1], &ptr, &len);

	mod = pvc->channels * vclient_sample_bytes(pvc);

	if (mod == 0 || len < (44 + mod - 1))
		return (CUSE_ERR_INVALID);

	/* align to next sample */
	len = 44 + mod - 1;
	len -= len % mod;

	/* pre-advance write pointer */
	vring_inc_write(&pvc->rx_ring[1], len);

	/* clear block */
	memset(ptr, 0, len);

	/* fill out data header */
	ptr[len - 8] = 'd';
	ptr[len - 7] = 'a';
	ptr[len - 6] = 't';
	ptr[len - 5] = 'a';

	/* magic for unspecified length */
	ptr[len - 4] = 0x00;
	ptr[len - 3] = 0xF0;
	ptr[len - 2] = 0xFF;
	ptr[len - 1] = 0x7F;

	/* fill out header */
	*ptr++ = 'R';
	*ptr++ = 'I';
	*ptr++ = 'F';
	*ptr++ = 'F';

	/* total chunk size - unknown */

	*ptr++ = 0;
	*ptr++ = 0;
	*ptr++ = 0;
	*ptr++ = 0;

	*ptr++ = 'W';
	*ptr++ = 'A';
	*ptr++ = 'V';
	*ptr++ = 'E';
	*ptr++ = 'f';
	*ptr++ = 'm';
	*ptr++ = 't';
	*ptr++ = ' ';

	/* make sure header fits in PCM block */
	len -= 28;

	*ptr++ = len;
	*ptr++ = len >> 8;
	*ptr++ = len >> 16;
	*ptr++ = len >> 24;

	/* audioformat = PCM */

	*ptr++ = 0x01;
	*ptr++ = 0x00;

	/* number of channels */

	len = pvc->channels;

	*ptr++ = len;
	*ptr++ = len >> 8;

	/* sample rate */

	len = pvc->sample_rate;

	*ptr++ = len;
	*ptr++ = len >> 8;
	*ptr++ = len >> 16;
	*ptr++ = len >> 24;

	/* byte rate */

	len = pvc->sample_rate * pvc->channels * vclient_sample_bytes(pvc);

	*ptr++ = len;
	*ptr++ = len >> 8;
	*ptr++ = len >> 16;
	*ptr++ = len >> 24;

	/* block align */

	len = pvc->channels * vclient_sample_bytes(pvc);

	*ptr++ = len;
	*ptr++ = len >> 8;

	/* bits per sample */

	len = vclient_sample_bytes(pvc) * 8;

	*ptr++ = len;
	*ptr++ = len >> 8;

	return (0);
}

int
vclient_export_read_locked(vclient_t *pvc) __requires_exclusive(atomic_mtx)
{
	enum { MAX_FRAME = 1024 };
	size_t dst_mod;
	size_t src_mod;
	int error;

	if (pvc->type == VTYPE_WAV_HDR) {
		error = vclient_generate_wav_header_locked(pvc);
		if (error != 0)
			return (error);
		/* only write header once */
		pvc->type = VTYPE_WAV_DAT;
	}
	error = vclient_read_silence_locked(pvc);
	if (error != 0)
		return (0);

	dst_mod = pvc->channels * vclient_sample_bytes(pvc);
	src_mod = pvc->channels * 8;

	if (pvc->sample_rate == (int)voss_dsp_sample_rate) {
		while (1) {
			uint8_t *src_ptr;
			size_t src_len;
			uint8_t *dst_ptr;
			size_t dst_len;

			vring_get_read(&pvc->rx_ring[0], &src_ptr, &src_len);
			vring_get_write(&pvc->rx_ring[1], &dst_ptr, &dst_len);

			src_len /= src_mod;
			dst_len /= dst_mod;

			/* compare number of samples */
			if (dst_len > src_len)
				dst_len = src_len;
			else
				src_len = dst_len;

			if (dst_len == 0)
				break;

			src_len *= src_mod;
			dst_len *= dst_mod;

			format_export(pvc->format,
			    (const int64_t *)(uintptr_t)src_ptr,
			    dst_ptr, dst_len);

			vring_inc_read(&pvc->rx_ring[0], src_len);
			vring_inc_write(&pvc->rx_ring[1], dst_len);
		}
	} else {
		vresample_t *pvr = &pvc->rx_resample;

		if (vresample_setup(pvc, pvr, MAX_FRAME * pvc->channels) != 0)
			return (CUSE_ERR_NO_MEMORY);

		while (1) {
			uint8_t *src_ptr;
			size_t src_len;
			uint8_t *dst_ptr;
			size_t dst_len;
			int64_t temp[MAX_FRAME * pvc->channels];
			size_t samples;
			size_t y;

			vring_get_read(&pvc->rx_ring[0], &src_ptr, &src_len);
			vring_get_write(&pvc->rx_ring[1], &dst_ptr, &dst_len);

			src_len /= src_mod;
			dst_len /= dst_mod;

			/* compare number of samples */
			if (dst_len > src_len)
				dst_len = src_len;
			else
				src_len = dst_len;

			if (dst_len > MAX_FRAME)
				dst_len = src_len = MAX_FRAME;

			if (dst_len == 0)
				break;

			src_len *= src_mod;
			dst_len *= dst_mod;

			for (y = 0; y != src_len; y += 8) {
				pvr->data_in[y / 8] =
				    *(int64_t *)(uintptr_t)(src_ptr + y);
			}

			/* setup parameters for transform */
			pvr->data.input_frames = src_len / src_mod;
			pvr->data.output_frames = dst_len / dst_mod;
			pvr->data.src_ratio = (float)pvc->sample_rate / (float)voss_dsp_sample_rate;

			pvc->rx_busy = 1;
			atomic_unlock();
			error = src_process(pvr->state, &pvr->data);
			atomic_lock();
			pvc->rx_busy = 0;

			if (error != 0)
				break;

			src_len = pvr->data.input_frames_used * src_mod;
			dst_len = pvr->data.output_frames_gen * dst_mod;

			samples = pvr->data.output_frames_gen * pvc->channels;

			for (y = 0; y != samples; y++)
				temp[y] = pvr->data_out[y];

			format_export(pvc->format, temp, dst_ptr, dst_len);

			vring_inc_read(&pvc->rx_ring[0], src_len);
			vring_inc_write(&pvc->rx_ring[1], dst_len);

			/* check if no data was moved */
			if (src_len == 0 && dst_len == 0)
				break;
		}
	}
	if (pvc->sync_busy)
		atomic_wakeup();
	return (0);
}

static int
vclient_read(struct cuse_dev *pdev, int fflags,
    void *peer_ptr, int len)
{
	vclient_t *pvc;

	int error;
	int retval;

	pvc = cuse_dev_get_per_file_handle(pdev);
	if (pvc == NULL)
		return (CUSE_ERR_INVALID);

	atomic_lock();

	if (pvc->rx_busy) {
		atomic_unlock();
		return (CUSE_ERR_BUSY);
	}
	pvc->rx_enabled = 1;

	retval = 0;

	while (len > 0) {
		uint8_t *buf_ptr;
		size_t buf_len;

		error = vclient_export_read_locked(pvc);
		if (error != 0) {
			retval = error;
			break;
		}

		vring_get_read(&pvc->rx_ring[1], &buf_ptr, &buf_len);

		if (buf_len == 0) {
			/* out of data */
			if (fflags & CUSE_FFLAG_NONBLOCK) {
				if (retval == 0)
					retval = CUSE_ERR_WOULDBLOCK;
				break;
			}
			pvc->rx_busy = 1;
			atomic_wait();
			pvc->rx_busy = 0;
			if (cuse_got_peer_signal() == 0) {
				if (retval == 0)
					retval = CUSE_ERR_SIGNAL;
				break;
			}
			continue;
		}
		if ((int)buf_len > len)
			buf_len = len;

		pvc->rx_busy = 1;
		atomic_unlock();
		error = cuse_copy_out(buf_ptr, peer_ptr, buf_len);
		atomic_lock();
		pvc->rx_busy = 0;

		if (error != 0) {
			retval = error;
			break;
		}
		peer_ptr = ((uint8_t *)peer_ptr) + buf_len;
		retval += buf_len;
		len -= buf_len;

		vring_inc_read(&pvc->rx_ring[1], buf_len);
	}
	atomic_unlock();

	return (retval);
}

void
vclient_import_write_locked(vclient_t *pvc) __requires_exclusive(atomic_mtx)
{
	enum { MAX_FRAME = 1024 };
	size_t dst_mod;
	size_t src_mod;

	dst_mod = pvc->channels * 8;
	src_mod = pvc->channels * vclient_sample_bytes(pvc);

	if (pvc->sample_rate == (int)voss_dsp_sample_rate) {
		while (1) {
			uint8_t *src_ptr;
			size_t src_len;
			uint8_t *dst_ptr;
			size_t dst_len;

			vring_get_read(&pvc->tx_ring[1], &src_ptr, &src_len);
			vring_get_write(&pvc->tx_ring[0], &dst_ptr, &dst_len);

			src_len /= src_mod;
			dst_len /= dst_mod;

			/* compare number of samples */
			if (dst_len > src_len)
				dst_len = src_len;
			else
				src_len = dst_len;

			if (dst_len == 0)
				break;

			src_len *= src_mod;
			dst_len *= dst_mod;

			format_import(pvc->format, src_ptr, src_len,
			    (int64_t *)(uintptr_t)dst_ptr);

			vring_inc_read(&pvc->tx_ring[1], src_len);
			vring_inc_write(&pvc->tx_ring[0], dst_len);
		}
	} else {
		vresample_t *pvr = &pvc->tx_resample;

		if (vresample_setup(pvc, pvr, MAX_FRAME * pvc->channels) != 0)
			return;

		while (1) {
			uint8_t *src_ptr;
			size_t src_len;
			uint8_t *dst_ptr;
			size_t dst_len;
			int64_t temp[MAX_FRAME * pvc->channels];
			size_t samples;
			size_t y;
			int error;

			vring_get_read(&pvc->tx_ring[1], &src_ptr, &src_len);
			vring_get_write(&pvc->tx_ring[0], &dst_ptr, &dst_len);

			src_len /= src_mod;
			dst_len /= dst_mod;

			/* compare number of samples */
			if (dst_len > src_len)
				dst_len = src_len;
			else
				src_len = dst_len;

			if (dst_len > MAX_FRAME)
				dst_len = src_len = MAX_FRAME;

			if (dst_len == 0)
				break;

			src_len *= src_mod;
			dst_len *= dst_mod;

			format_import(pvc->format, src_ptr, src_len, temp);

			src_len /= vclient_sample_bytes(pvc);

			for (y = 0; y != src_len; y++)
				pvr->data_in[y] = temp[y];

			src_len *= vclient_sample_bytes(pvc);

			/* setup parameters for transform */
			pvr->data.input_frames = src_len / src_mod;
			pvr->data.output_frames = dst_len / dst_mod;
			pvr->data.src_ratio = (float)voss_dsp_sample_rate / (float)pvc->sample_rate;

			pvc->tx_busy = 1;
			atomic_unlock();
			error = src_process(pvr->state, &pvr->data);
			atomic_lock();
			pvc->tx_busy = 0;

			if (error != 0)
				break;

			src_len = pvr->data.input_frames_used * src_mod;
			dst_len = pvr->data.output_frames_gen * dst_mod;

			samples = pvr->data.output_frames_gen * pvc->channels;

			for (y = 0; y != samples; y++) {
				((int64_t *)(uintptr_t)dst_ptr)[y] =
				    pvr->data_out[y];
			}

			vring_inc_read(&pvc->tx_ring[1], src_len);
			vring_inc_write(&pvc->tx_ring[0], dst_len);

			/* check if no data was moved */
			if (src_len == 0 && dst_len == 0)
				break;
		}
	}
	if (pvc->sync_busy)
		atomic_wakeup();
}

static int
vclient_write_oss(struct cuse_dev *pdev, int fflags,
    const void *peer_ptr, int len)
{
	vclient_t *pvc;

	int error;
	int retval;

	pvc = cuse_dev_get_per_file_handle(pdev);
	if (pvc == NULL)
		return (CUSE_ERR_INVALID);

	retval = 0;

	atomic_lock();

	if (pvc->tx_busy) {
		atomic_unlock();
		return (CUSE_ERR_BUSY);
	}
	pvc->tx_enabled = 1;

	while (1) {
		uint8_t *buf_ptr;
		size_t buf_len;

		vclient_import_write_locked(pvc);

		if (len < 1)
			break;

		vring_get_write(&pvc->tx_ring[1], &buf_ptr, &buf_len);

		if (buf_len == 0) {
			/* out of data */
			if (fflags & CUSE_FFLAG_NONBLOCK) {
				if (retval == 0)
					retval = CUSE_ERR_WOULDBLOCK;
				break;
			}
			pvc->tx_busy = 1;
			atomic_wait();
			pvc->tx_busy = 0;
			if (cuse_got_peer_signal() == 0) {
				if (retval == 0)
					retval = CUSE_ERR_SIGNAL;
				break;
			}
			continue;
		}
		if ((int)buf_len > len)
			buf_len = len;

		pvc->tx_busy = 1;
		atomic_unlock();
		error = cuse_copy_in(peer_ptr, buf_ptr, buf_len);
		atomic_lock();
		pvc->tx_busy = 0;

		if (error != 0) {
			retval = error;
			break;
		}
		peer_ptr = ((const uint8_t *)peer_ptr) + buf_len;
		retval += buf_len;
		len -= buf_len;

		vring_inc_write(&pvc->tx_ring[1], buf_len);
	}
	atomic_unlock();

	return (retval);
}

static int
vclient_write_wav(struct cuse_dev *pdev __unused, int fflags __unused,
    const void *peer_ptr __unused, int len __unused)
{
	return (CUSE_ERR_INVALID);
}

static int
vclient_set_channels(vclient_t *pvc, int channels)
{
	if (pvc->channels == channels)
		return (0);
	return (vclient_setup_buffers(pvc, 0, 0, channels, 0, 0));
}

/* greatest common divisor, Euclid equation */
static uint64_t
vclient_gcd_64(uint64_t a, uint64_t b)
{
	uint64_t an;
	uint64_t bn;

	while (b != 0) {
		an = b;
		bn = a % b;
		a = an;
		b = bn;
	}
	return (a);
}

static uint64_t
vclient_scale(uint64_t value, uint64_t mul, uint64_t div)
{
	uint64_t gcd = vclient_gcd_64(mul, div);

	mul /= gcd;
	div /= gcd;

	return ((value * mul) / div);
}

static int
vclient_ioctl_oss(struct cuse_dev *pdev, int fflags __unused,
    unsigned long cmd, void *peer_data)
{
	union {
		int	val;
		unsigned long long lval;
		oss_sysinfo sysinfo;
		oss_card_info card_info;
		oss_audioinfo audioinfo;
		audio_buf_info buf_info;
		oss_count_t oss_count;
		count_info oss_count_info;
		audio_errinfo errinfo;
		oss_label_t label;
		oss_longname_t longname;
	}     data;

	vclient_t *pvc;

	uint64_t bytes;

	int len;
	int error;
	int temp;

	pvc = cuse_dev_get_per_file_handle(pdev);
	if (pvc == NULL)
		return (CUSE_ERR_INVALID);

	len = IOCPARM_LEN(cmd);

	if (len < 0 || len > (int)sizeof(data))
		return (CUSE_ERR_INVALID);

	if (cmd & IOC_IN) {
		error = cuse_copy_in(peer_data, &data, len);
		if (error)
			return (error);
	} else {
		error = 0;
	}

	atomic_lock();

	switch (cmd) {
	case OSS_GETVERSION:
		data.val = SOUND_VERSION;
		break;
	case SNDCTL_SYSINFO:
		memset(&data.sysinfo, 0, sizeof(data.sysinfo));
		strcpy(data.sysinfo.product, "VOSS");
		strcpy(data.sysinfo.version, "1.0");
		data.sysinfo.versionnum = SOUND_VERSION;
		data.sysinfo.numaudios = 1;
		data.sysinfo.numcards = 1;
		data.sysinfo.numaudioengines = 1;
		strcpy(data.sysinfo.license, "BSD");
		memset(data.sysinfo.filler, -1, sizeof(data.sysinfo.filler));
		break;
	case SNDCTL_CARDINFO:
		memset(&data.card_info, 0, sizeof(data.card_info));
		strlcpy(data.card_info.shortname, pvc->profile->oss_name,
		    sizeof(data.card_info.shortname));
		break;
	case SNDCTL_AUDIOINFO:
	case SNDCTL_AUDIOINFO_EX:
	case SNDCTL_ENGINEINFO:
		memset(&data.audioinfo, 0, sizeof(data.audioinfo));
		strlcpy(data.audioinfo.name, pvc->profile->oss_name,
		    sizeof(data.audioinfo.name));
		snprintf(data.audioinfo.devnode, sizeof(data.audioinfo.devnode),
		    "/dev/%s", pvc->profile->oss_name);
		data.audioinfo.caps = DSP_CAP_INPUT | DSP_CAP_OUTPUT;
		data.audioinfo.iformats = VSUPPORTED_AFMT;
		data.audioinfo.oformats = VSUPPORTED_AFMT;
		data.audioinfo.enabled = 1;
		data.audioinfo.min_rate = (int)8000;
		data.audioinfo.max_rate = (int)voss_dsp_sample_rate;
		data.audioinfo.max_channels = pvc->profile->channels;
		/* range check */
		if (voss_libsamplerate_enable == 0 ||
		    data.audioinfo.min_rate > data.audioinfo.max_rate)
			data.audioinfo.min_rate = data.audioinfo.max_rate;
		data.audioinfo.nrates = 1;
		data.audioinfo.rates[0] = (int)voss_dsp_sample_rate;
		if (voss_libsamplerate_enable != 0 &&
		    96000 != voss_dsp_sample_rate)
			data.audioinfo.rates[data.audioinfo.nrates++] = 96000;
		if (voss_libsamplerate_enable != 0 &&
		    48000 != voss_dsp_sample_rate)
			data.audioinfo.rates[data.audioinfo.nrates++] = 48000;
		if (voss_libsamplerate_enable != 0 &&
		    44100 != voss_dsp_sample_rate)
			data.audioinfo.rates[data.audioinfo.nrates++] = 44100;
		if (voss_libsamplerate_enable != 0 &&
		    24000 != voss_dsp_sample_rate)
			data.audioinfo.rates[data.audioinfo.nrates++] = 24000;
		if (voss_libsamplerate_enable != 0 &&
		    16000 != voss_dsp_sample_rate)
			data.audioinfo.rates[data.audioinfo.nrates++] = 16000;
		if (voss_libsamplerate_enable != 0 &&
		    8000 != voss_dsp_sample_rate)
			data.audioinfo.rates[data.audioinfo.nrates++] = 8000;
		data.audioinfo.latency = -1;
		break;
	case FIONREAD:
		data.val = vclient_input_delay(pvc);
		break;
	case FIONWRITE:
		data.val = vring_total_read_len(&pvc->tx_ring[1]);
		break;
	case FIOASYNC:
	case SNDCTL_DSP_NONBLOCK:
	case FIONBIO:
		break;
	case SNDCTL_DSP_SETBLKSIZE:
	case _IOWR('P', 4, int):
		error = vclient_setup_buffers(pvc, data.val, 0, 0, 0, 0);
		/* FALLTHROUGH */
	case SNDCTL_DSP_GETBLKSIZE:
		data.val = pvc->buffer_size;
		break;
	case SNDCTL_DSP_SETFRAGMENT:
		if ((data.val & 0xFFFF) < 4) {
			/* need at least 16 bytes of buffer */
			data.val &= ~0xFFFF;
			data.val |= 4;
		} else if ((data.val & 0xFFFF) > 24) {
			/* no more than 16MBytes of buffer */
			data.val &= ~0xFFFF;
			data.val |= 24;
		}
		error = vclient_setup_buffers(pvc,
		    (1 << (data.val & 0xFFFF)), (data.val >> 16), 0, 0, 0);
		if (error) {
			/* fallback to defaults */
			pvc->buffer_size_set = 0;
			pvc->buffer_frags_set = 0;
			error = vclient_setup_buffers(pvc, 0, 0, 0, 0, 0);
			if (error)
				break;
			/* figure out log2() of actual buffer size */
			for (data.val = 0;
			     data.val < 24 && (1U << data.val) < pvc->buffer_size;
			     data.val++)
				;
			/* or in the actual number of fragments */
			data.val |= (pvc->buffer_frags << 16);
		}
		break;
	case SNDCTL_DSP_RESET:
		error = vclient_setup_buffers(pvc, 0, 0, 0, 0, 0);
		break;
	case SNDCTL_DSP_SYNC:
		break;
	case SNDCTL_DSP_SPEED:
		if (data.val >= 8000 && data.val <= 96000 &&
		    voss_libsamplerate_enable != 0) {
			error = vclient_setup_buffers(pvc, 0, 0, 0, 0, data.val);
		}
		/* return current speed */
		data.val = (int)pvc->sample_rate;
		break;
	case SOUND_PCM_READ_RATE:
		data.val = (int)pvc->sample_rate;
		break;
	case SNDCTL_DSP_STEREO:
		if (data.val != 0) {
			error = vclient_set_channels(pvc, 2);
		} else {
			error = vclient_set_channels(pvc, 1);
		}
		data.val = (pvc->channels == 2);
		break;
	case SOUND_PCM_WRITE_CHANNELS:
		if (data.val < 0) {
			data.val = 0;
			error = CUSE_ERR_INVALID;
			break;
		}
		if (data.val == 0) {
			data.val = pvc->channels;
		} else {
			error = vclient_set_channels(pvc, data.val);
		}
		break;
	case SOUND_PCM_READ_CHANNELS:
		data.val = pvc->channels;
		break;
	case AIOGFMT:
	case SNDCTL_DSP_GETFMTS:
		data.val = VSUPPORTED_AFMT | AFMT_FULLDUPLEX |
		    (pvc->profile->channels > 1 ? AFMT_STEREO : 0);
		break;
	case AIOSFMT:
	case SNDCTL_DSP_SETFMT:
		if (data.val != AFMT_QUERY) {
			temp = data.val & VSUPPORTED_AFMT;
			if (temp == 0 || (temp & (temp - 1)) != 0) {
				error = CUSE_ERR_INVALID;
			} else {
				error = vclient_setup_buffers(pvc, 0, 0, 0, temp, 0);
			}
		} else {
			data.val = pvc->format;
		}
		break;
	case SNDCTL_DSP_GETISPACE:
		memset(&data.buf_info, 0, sizeof(data.buf_info));
		data.buf_info.fragsize = pvc->buffer_size;
		data.buf_info.fragstotal = pvc->buffer_frags;
		bytes = (pvc->buffer_size * pvc->buffer_frags);
		temp = vclient_input_delay(pvc);
		if (temp < 0 || (uint64_t)temp > bytes)
			temp = bytes;
		data.buf_info.fragments = temp / pvc->buffer_size;
		data.buf_info.bytes = temp;
		break;
	case SNDCTL_DSP_GETOSPACE:
		memset(&data.buf_info, 0, sizeof(data.buf_info));
		data.buf_info.fragsize = pvc->buffer_size;
		data.buf_info.fragstotal = pvc->buffer_frags;
		bytes = (pvc->buffer_size * pvc->buffer_frags);
		temp = vclient_output_delay(pvc);
		if (temp < 0 || (uint64_t)temp >= bytes) {
			/* buffer is full */
			data.buf_info.fragments = 0;
			data.buf_info.bytes = 0;
		} else {
			/* buffer is not full */
			bytes -= temp;
			data.buf_info.fragments = bytes / pvc->buffer_size;
			data.buf_info.bytes = bytes;
		}
		break;
	case SNDCTL_DSP_GETCAPS:
		data.val = PCM_CAP_REALTIME | PCM_CAP_DUPLEX |
		    PCM_CAP_INPUT | PCM_CAP_OUTPUT | PCM_CAP_TRIGGER |
		    PCM_CAP_VIRTUAL;
		break;
	case SOUND_PCM_READ_BITS:
		data.val = vclient_sample_bytes(pvc) * 8;
		break;
	case SNDCTL_DSP_SETTRIGGER:
		if (data.val & PCM_ENABLE_INPUT) {
			pvc->rx_enabled = 1;
		} else {
			pvc->rx_enabled = 0;
			vring_reset(&pvc->rx_ring[1]);
		}

		if (data.val & PCM_ENABLE_OUTPUT) {
			pvc->tx_enabled = 1;
		} else {
			pvc->tx_enabled = 0;
			vring_reset(&pvc->tx_ring[1]);
		}
		break;
	case SNDCTL_DSP_GETTRIGGER:
		data.val = 0;
		if (pvc->rx_enabled)
			data.val |= PCM_ENABLE_INPUT;
		if (pvc->tx_enabled)
			data.val |= PCM_ENABLE_OUTPUT;
		break;
	case SNDCTL_DSP_GETODELAY:
		data.val = vclient_output_delay_adjusted(pvc);
		break;
	case SNDCTL_DSP_POST:
		break;
	case SNDCTL_DSP_SETDUPLEX:
		break;
	case SNDCTL_DSP_GETRECVOL:
		temp = (pvc->rx_volume * 100) >> VVOLUME_UNIT_SHIFT;
		data.val = (temp & 0x00FF) |
		    ((temp << 8) & 0xFF00);
		break;
	case SNDCTL_DSP_SETRECVOL:
		pvc->rx_volume = ((data.val & 0xFF) << VVOLUME_UNIT_SHIFT) / 100;
		break;
	case SNDCTL_DSP_GETPLAYVOL:
		temp = (pvc->tx_volume * 100) >> VVOLUME_UNIT_SHIFT;
		data.val = (temp & 0x00FF) |
		    ((temp << 8) & 0xFF00);
		break;
	case SNDCTL_DSP_SETPLAYVOL:
		pvc->tx_volume = ((data.val & 0xFF) << VVOLUME_UNIT_SHIFT) / 100;
		break;
	case SNDCTL_DSP_CURRENT_IPTR:
		memset(&data.oss_count, 0, sizeof(data.oss_count));
		/* compute input samples per channel */
		data.oss_count.samples =
		    vclient_scale(pvc->rx_samples, pvc->sample_rate, voss_dsp_sample_rate);
		data.oss_count.samples /= pvc->channels;
		data.oss_count.fifo_samples =
		    vclient_input_delay(pvc) / (pvc->channels * vclient_sample_bytes(pvc));
		break;
	case SNDCTL_DSP_CURRENT_OPTR:
		memset(&data.oss_count, 0, sizeof(data.oss_count));
		/* compute output samples per channel */
		data.oss_count.samples =
		    vclient_scale(pvc->tx_samples, pvc->sample_rate, voss_dsp_sample_rate);
		data.oss_count.samples /= pvc->channels;
		data.oss_count.fifo_samples =
		    vclient_output_delay(pvc) / (pvc->channels * vclient_sample_bytes(pvc));
		break;
	case SNDCTL_DSP_GETIPTR:
		memset(&data.oss_count_info, 0, sizeof(data.oss_count_info));
		/* compute input bytes */
		bytes =
		    vclient_scale(pvc->rx_samples, pvc->sample_rate, voss_dsp_sample_rate) *
		    vclient_sample_bytes(pvc);
		data.oss_count_info.bytes = bytes;
		data.oss_count_info.blocks = bytes / pvc->buffer_size;
		data.oss_count_info.ptr = bytes % (pvc->buffer_size * pvc->buffer_frags);
		break;
	case SNDCTL_DSP_GETOPTR:
		memset(&data.oss_count_info, 0, sizeof(data.oss_count_info));
		/* compute output bytes */
		bytes =
		    vclient_scale(pvc->tx_samples, pvc->sample_rate, voss_dsp_sample_rate) *
		    vclient_sample_bytes(pvc);
		data.oss_count_info.bytes = bytes;
		data.oss_count_info.blocks = bytes / pvc->buffer_size;
		data.oss_count_info.ptr = bytes % (pvc->buffer_size * pvc->buffer_frags);
		break;
	case SNDCTL_DSP_HALT_OUTPUT:
		pvc->tx_enabled = 0;
		break;
	case SNDCTL_DSP_HALT_INPUT:
		pvc->rx_enabled = 0;
		break;
	case SNDCTL_DSP_LOW_WATER:
		if (data.val > 0 && data.val <
		    (int)(pvc->buffer_frags * pvc->buffer_size)) {
			pvc->low_water = data.val;
		} else {
			error = CUSE_ERR_INVALID;
		}
		break;
	case SNDCTL_DSP_GETERROR:
		memset(&data.errinfo, 0, sizeof(data.errinfo));
		break;
	case SNDCTL_DSP_SYNCGROUP:
	case SNDCTL_DSP_SYNCSTART:
		break;
	case SNDCTL_DSP_POLICY:
		break;
	case SNDCTL_DSP_COOKEDMODE:
		break;
	case SNDCTL_DSP_GET_CHNORDER:
		data.lval = CHNORDER_NORMAL;
		break;
	case SNDCTL_DSP_GETCHANNELMASK:
		data.val = DSP_BIND_FRONT;
		break;
	case SNDCTL_DSP_BIND_CHANNEL:
		break;
	case SNDCTL_GETLABEL:
		memset(&data.label, 0, sizeof(data.label));
		break;
	case SNDCTL_SETLABEL:
		break;
	case SNDCTL_GETSONG:
		memset(&data.longname, 0, sizeof(data.longname));
		break;
	case SNDCTL_SETSONG:
		break;
	case SNDCTL_SETNAME:
		break;
	default:
		error = CUSE_ERR_INVALID;
		break;
	}
	atomic_unlock();

	if (error == 0) {
		if (cmd & IOC_OUT)
			error = cuse_copy_out(&data, peer_data, len);
	}
	return (error);
}

static int
vclient_ioctl_wav(struct cuse_dev *pdev, int fflags __unused,
    unsigned long cmd, void *peer_data)
{
	union {
		int	val;
	}     data;

	vclient_t *pvc;
	int len;
	int error;

	pvc = cuse_dev_get_per_file_handle(pdev);
	if (pvc == NULL)
		return (CUSE_ERR_INVALID);

	len = IOCPARM_LEN(cmd);

	if (len < 0 || len > (int)sizeof(data))
		return (CUSE_ERR_INVALID);

	if (cmd & IOC_IN) {
		error = cuse_copy_in(peer_data, &data, len);
		if (error)
			return (error);
	} else {
		error = 0;
	}

	atomic_lock();
	switch (cmd) {
	case FIONREAD:
		data.val = vclient_input_delay(pvc);
		break;
	case FIOASYNC:
	case SNDCTL_DSP_NONBLOCK:
	case FIONBIO:
		break;
	default:
		error = CUSE_ERR_INVALID;
		break;
	}
	atomic_unlock();

	if (error == 0) {
		if (cmd & IOC_OUT)
			error = cuse_copy_out(&data, peer_data, len);
	}
	return (error);
}

static int
vclient_poll(struct cuse_dev *pdev, int fflags, int events)
{
	vclient_t *pvc;

	int retval = CUSE_POLL_NONE;

	pvc = cuse_dev_get_per_file_handle(pdev);
	if (pvc == NULL)
		return (retval);

	atomic_lock();
	if ((events & CUSE_POLL_READ) && (fflags & CUSE_FFLAG_READ)) {
		pvc->rx_enabled = 1;
		if (vclient_input_delay(pvc) >= pvc->low_water)
			retval |= CUSE_POLL_READ;
	}
	if ((events & CUSE_POLL_WRITE) && (fflags & CUSE_FFLAG_WRITE)) {
		const uint32_t out_dly = vclient_output_delay(pvc);
		const uint32_t out_buf = (pvc->buffer_frags * pvc->buffer_size);

		if (out_dly < out_buf && (out_buf - out_dly) >= pvc->low_water)
			retval |= CUSE_POLL_WRITE;
	}
	atomic_unlock();

	return (retval);
}

static const struct cuse_methods vclient_oss_methods = {
	.cm_open = vclient_open_oss,
	.cm_close = vclient_close,
	.cm_read = vclient_read,
	.cm_write = vclient_write_oss,
	.cm_ioctl = vclient_ioctl_oss,
	.cm_poll = vclient_poll,
};

static const struct cuse_methods vclient_wav_methods = {
	.cm_open = vclient_open_wav,
	.cm_close = vclient_close,
	.cm_read = vclient_read,
	.cm_write = vclient_write_wav,
	.cm_ioctl = vclient_ioctl_wav,
	.cm_poll = vclient_poll,
};

vprofile_head_t virtual_profile_client_head;
vprofile_head_t virtual_profile_loopback_head;

vmonitor_head_t virtual_monitor_input;
vmonitor_head_t virtual_monitor_output;
vmonitor_head_t virtual_monitor_local;

uint32_t voss_max_channels;
uint32_t voss_mix_channels;
uint32_t voss_dsp_samples;
uint32_t voss_dsp_max_channels;
uint32_t voss_dsp_sample_rate;
uint32_t voss_dsp_bits;
uint8_t	voss_libsamplerate_enable;
uint8_t	voss_libsamplerate_quality = SRC_SINC_FASTEST;
int	voss_is_recording = 1;
int	voss_has_synchronization;
volatile sig_atomic_t voss_exit = 0;

static int voss_dsp_perm = 0666;
static int voss_do_background;
static const char *voss_pid_path;

uint32_t voss_dsp_rx_refresh;
uint32_t voss_dsp_tx_refresh;
char voss_dsp_rx_device[VMAX_STRING];
char voss_dsp_tx_device[VMAX_STRING];
char voss_ctl_device[VMAX_STRING];

uint32_t voss_jitter_up;
uint32_t voss_jitter_down;

struct voss_backend *voss_rx_backend;
struct voss_backend *voss_tx_backend;

static int voss_dups;
static int voss_ntds;
static pthread_t *voss_tds;

/* XXX I do not like the prefix argument... */
static struct voss_backend *
voss_load_backend(const char *prefix, const char *name, const char *dir)
{
	struct voss_backend *backend;
	void *hdl;
	char lpath[64], bsym[64];

	snprintf(lpath, sizeof(lpath), "%s/lib/virtual_oss/voss_%s.so",
	    prefix, name);
	snprintf(bsym, sizeof(bsym), "voss_backend_%s_%s", name, dir);

	if ((hdl = dlopen(lpath, RTLD_NOW)) == NULL)
		errx(1, "%s", dlerror());
	if ((backend = dlsym(hdl, bsym)) == NULL) {
		warnx("%s", dlerror());
		dlclose(hdl);
		exit(EXIT_FAILURE);
	}

	return (backend);
}

static void
voss_rx_backend_refresh(void)
{
	/* setup RX backend */
	if (strcmp(voss_dsp_rx_device, "/dev/null") == 0) {
		voss_rx_backend = voss_load_backend("/usr", "null", "rec");
	} else if (strstr(voss_dsp_rx_device, "/dev/bluetooth/") == voss_dsp_rx_device) {
		voss_rx_backend = voss_load_backend("/usr/local", "bt", "rec");
	} else if (strstr(voss_dsp_rx_device, "/dev/sndio/") == voss_dsp_rx_device) {
		voss_rx_backend = voss_load_backend("/usr/local", "sndio", "rec");
	} else {
		voss_rx_backend = voss_load_backend("/usr", "oss", "rec");
	}
}

static void
voss_tx_backend_refresh(void)
{
	/* setup TX backend */
	if (strcmp(voss_dsp_tx_device, "/dev/null") == 0) {
		voss_tx_backend = voss_load_backend("/usr", "null", "play");
	} else if (strstr(voss_dsp_tx_device, "/dev/bluetooth/") == voss_dsp_tx_device) {
		voss_tx_backend = voss_load_backend("/usr/local", "bt", "play");
	} else if (strstr(voss_dsp_tx_device, "/dev/sndio/") == voss_dsp_tx_device) {
		voss_tx_backend = voss_load_backend("/usr/local", "sndio", "play");
	} else {
		voss_tx_backend = voss_load_backend("/usr", "oss", "play");
	}
}

static void
usage(void)
{
	fprintf(stderr, "Usage: virtual_oss [options...] [device] \\\n"
	    "\t" "-C 2 -c 2 -r 48000 -b 16 -s 100.0ms -f /dev/dsp3 \\\n"
	    "\t" "-P /dev/dsp3 -R /dev/dsp1 \\\n"
	    "\t" "-O /dev/dsp3 -R /dev/null \\\n"
	    "\t" "-c 1 -m 0,0 [-w wav.0] -d dsp100.0 \\\n"
	    "\t" "-c 1 -m 0,0 [-w wav.0] -d vdsp.0 \\\n"
	    "\t" "-c 2 -m 0,0,1,1 [-w wav.1] -d vdsp.1 \\\n"
	    "\t" "-c 2 -m 0,0,1,1 [-w wav.loopback] -l vdsp.loopback \\\n"
	    "\t" "-c 2 -m 0,0,1,1 [-w wav.loopback] -L vdsp.loopback \\\n"
	    "\t" "-B # run in background \\\n"
	    "\t" "-s <samples> or <milliseconds>ms \\\n"
	    "\t" "-S # enable automatic resampling using libsamplerate \\\n"
	    "\t" "-Q <0,1,2> # quality of resampling 0=best,1=medium,2=fastest (default) \\\n"
	    "\t" "-b <bits> \\\n"
	    "\t" "-r <rate> \\\n"
	    "\t" "-i <rtprio> \\\n"
	    "\t" "-a <amp -63..63> \\\n"
	    "\t" "-a i,<rx_amp -63..63> \\\n"
	    "\t" "-a o,<tx_amp -63..63> \\\n"
	    "\t" "-g <knee,attack,decay> # enable device RX compressor\\\n"
	    "\t" "-x <knee,attack,decay> # enable output compressor\\\n"
	    "\t" "-p <pol 0..1> \\\n"
	    "\t" "-e <rxtx_mute 0..1> \\\n"
	    "\t" "-e <rx_mute 0..1>,<tx_mute 0..1> \\\n"
	    "\t" "-m <mapping> \\\n"
	    "\t" "-m <rx0,tx0,rx1,tx1...rxN,txN> \\\n"
	    "\t" "-C <mixchans>\\\n"
	    "\t" "-c <dspchans> \\\n"
	    "\t" "-M <monitorfilter> \\\n"
	    "\t" "-M i,<src>,<dst>,<pol>,<mute>,<amp> \\\n"
	    "\t" "-M o,<src>,<dst>,<pol>,<mute>,<amp> \\\n"
	    "\t" "-M x,<src>,<dst>,<pol>,<mute>,<amp> \\\n"
	    "\t" "-F <rx_filter_samples> or <milliseconds>ms \\\n"
	    "\t" "-G <tx_filter_samples> or <milliseconds>ms \\\n"
	    "\t" "-E <enable_recording, 0 or 1> \\\n"
	    "\t" "-N <max HTTP connections, default is 1> \\\n"
	    "\t" "-H <bind HTTP server to this host> \\\n"
	    "\t" "-o <bind HTTP server to this port, default is 80> \\\n"
	    "\t" "-J <bind RTP server to this network interface> \\\n"
	    "\t" "-k <bind RTP server to this port, default is 8080> \\\n"
	    "\t" "-t vdsp.ctl \n"
	    "\t" "Left channel = 0\n"
	    "\t" "Right channel = 1\n"
	    "\t" "Max channels = %d\n", VMAX_CHAN);

	exit(EX_USAGE);
}

static void
init_compressor(struct virtual_profile *pvp)
{
	int x;

	memset(&pvp->rx_compressor_param, 0, sizeof(pvp->rx_compressor_param));

	pvp->rx_compressor_param.knee = 85;
	pvp->rx_compressor_param.attack = 3;
	pvp->rx_compressor_param.decay = 20;

	for (x = 0; x != VMAX_CHAN; x++)
		pvp->rx_compressor_gain[x] = 1.0;
}

static void
init_mapping(struct virtual_profile *pvp)
{
	int x;

	for (x = 0; x != VMAX_CHAN; x++) {
		pvp->rx_src[x] = x;
		pvp->tx_dst[x] = x;
	}
}

static void
init_sndstat(vprofile_t *ptr)
{
	int err;
	nvlist_t *nvl;
	nvlist_t *di = NULL, *dichild = NULL;
	struct sndstioc_nv_arg arg;
	unsigned int min_rate, max_rate;

	nvl = nvlist_create(0);
	if (nvl == NULL) {
		warn("Failed to create nvlist");
		goto done;
	}

	di = nvlist_create(0);
	if (di == NULL) {
		warn("Failed to create nvlist");
		goto done;
	}

	dichild = nvlist_create(0);
	if (dichild == NULL) {
		warn("Failed to create nvlist");
		goto done;
	}

	nvlist_add_string(di, SNDST_DSPS_PROVIDER, "virtual_oss");
	nvlist_add_string(di, SNDST_DSPS_DESC, "virtual_oss device");
	nvlist_add_number(di, SNDST_DSPS_PCHAN, 1);
	nvlist_add_number(di, SNDST_DSPS_RCHAN, 1);
	min_rate = 8000;
	max_rate = voss_dsp_sample_rate;
	if (voss_libsamplerate_enable == 0 ||
	    min_rate > max_rate)
		min_rate = max_rate;
	if (voss_libsamplerate_enable != 0 && max_rate < 96000)
		max_rate = 96000;
	nvlist_add_number(dichild, SNDST_DSPS_INFO_MIN_RATE, min_rate);
	nvlist_add_number(dichild, SNDST_DSPS_INFO_MAX_RATE, max_rate);
	nvlist_add_number(dichild, SNDST_DSPS_INFO_FORMATS, VSUPPORTED_AFMT);
	nvlist_add_number(dichild, SNDST_DSPS_INFO_MIN_CHN, ptr->channels);
	nvlist_add_number(dichild, SNDST_DSPS_INFO_MAX_CHN, ptr->channels);
	nvlist_add_nvlist(di, SNDST_DSPS_INFO_PLAY, dichild);
	nvlist_add_nvlist(di, SNDST_DSPS_INFO_REC, dichild);

	nvlist_add_string(di, SNDST_DSPS_DEVNODE,
	    ptr->oss_name);
	nvlist_append_nvlist_array(nvl, SNDST_DSPS, di);

	if (nvlist_error(nvl)) {
		warn("Failed building nvlist");
		goto done;
	}

	arg.buf = nvlist_pack(nvl, &arg.nbytes);
	if (arg.buf == NULL) {
		warn("Failed to pack nvlist");
		goto done;
	}
	err = ioctl(ptr->fd_sta, SNDSTIOC_ADD_USER_DEVS, &arg);
	free(arg.buf);
	if (err != 0) {
		warn("Failed to issue ioctl(SNDSTIOC_ADD_USER_DEVS)");
		goto done;
	}

done:
	nvlist_destroy(di);
	nvlist_destroy(dichild);
	nvlist_destroy(nvl);
}

static const char *
dup_profile(vprofile_t *pvp, int *pamp, int pol, int rx_mute,
    int tx_mute, int synchronized, int is_client)
{
	vprofile_t *ptr;
	struct cuse_dev *pdev;
	int x;

	rx_mute = rx_mute ? 1 : 0;
	tx_mute = tx_mute ? 1 : 0;
	pol = pol ? 1 : 0;

	/* Range check amplitude argument. */
	for (x = 0; x != 2; x++) {
		if (pamp[x] < -63)
			pamp[x] = -63;
		else if (pamp[x] > 63)
			pamp[x] = 63;
	}

	ptr = malloc(sizeof(*ptr));
	if (ptr == NULL)
		return ("Out of memory");

	memcpy(ptr, pvp, sizeof(*ptr));

	ptr->synchronized = synchronized;
	ptr->fd_sta = -1;
	TAILQ_INIT(&ptr->head);

	for (x = 0; x != ptr->channels; x++) {
		ptr->tx_mute[x] = tx_mute;
		ptr->rx_mute[x] = rx_mute;
		ptr->tx_shift[x] = pamp[1];
		ptr->rx_shift[x] = pamp[0];
		ptr->tx_pol[x] = pol;
		ptr->rx_pol[x] = pol;
	}

	/* create DSP device */
	if (ptr->oss_name[0] != 0) {
		/*
		 * Detect /dev/dsp creation and try to disable system
		 * basename cloning automatically:
		 */
		if (strcmp(ptr->oss_name, "dsp") == 0)
			system("sysctl hw.snd.basename_clone=0");

		/* create DSP character device */
		pdev = cuse_dev_create(&vclient_oss_methods, ptr, NULL,
		    0, 0, voss_dsp_perm, ptr->oss_name);
		if (pdev == NULL) {
			free(ptr);
			return ("Could not create CUSE DSP device");
		}

		/* register to sndstat */
		ptr->fd_sta = open("/dev/sndstat", O_WRONLY);
		if (ptr->fd_sta < 0) {
			warn("Could not open /dev/sndstat");
		} else {
			init_sndstat(ptr);
		}
	}
	/* create WAV device */
	if (ptr->wav_name[0] != 0) {
		pdev = cuse_dev_create(&vclient_wav_methods, ptr, NULL,
		    0, 0, voss_dsp_perm, ptr->wav_name);
		if (pdev == NULL) {
			free(ptr);
			return ("Could not create CUSE WAV device");
		}
	}

	atomic_lock();
	if (is_client)
		TAILQ_INSERT_TAIL(&virtual_profile_client_head, ptr, entry);
	else
		TAILQ_INSERT_TAIL(&virtual_profile_loopback_head, ptr, entry);
	atomic_unlock();

	voss_dups++;

	/* need new names next time */
	memset(pvp->oss_name, 0, sizeof(pvp->oss_name));
	memset(pvp->wav_name, 0, sizeof(pvp->wav_name));

	/* need to set new filter sizes */
	pvp->rx_filter_size = 0;
	pvp->tx_filter_size = 0;

	/* need to specify new HTTP parameters next time */
	pvp->http.host = NULL;
	pvp->http.port = NULL;
	pvp->http.nstate = 0;
	pvp->http.rtp_ifname = NULL;
	pvp->http.rtp_port = NULL;

	/* need to specify new amplification next time */
	pamp[0] = 0;
	pamp[1] = 0;

	/* need to set new compressor parameters next time */
	init_compressor(pvp);

	return (voss_httpd_start(ptr));
}

static void
virtual_pipe(int sig __unused)
{
	voss_dsp_tx_refresh = 1;
	voss_dsp_rx_refresh = 1;
}

static void
virtual_cuse_hup(int sig __unused)
{
	atomic_wakeup();
}

static void *
virtual_cuse_process(void *arg __unused)
{
	signal(SIGHUP, &virtual_cuse_hup);

	while (1) {
		if (cuse_wait_and_process() != 0)
			break;
	}
	return (NULL);
}

static void
virtual_cuse_init_profile(struct virtual_profile *pvp)
{
	memset(pvp, 0, sizeof(*pvp));

	init_compressor(pvp);
	init_mapping(pvp);
}

static void
virtual_sig_exit(int sig __unused)
{
	voss_exit = 1;
}

static const char *
parse_options(int narg, char **pparg, int is_main)
{
	const char *ptr;
	int a, b, c;
	int val;
	int idx;
	int type;
	int opt_mute[2] = {0, 0};
	int opt_amp[2] = {0, 0};
	int opt_pol = 0;
	const char *optstr;
	struct virtual_profile profile;
	struct rtprio rtp;
	float samples_ms;

	if (is_main)
		optstr = "N:J:k:H:o:F:G:w:e:p:a:C:c:r:b:f:g:x:i:m:M:d:l:L:s:t:h?O:P:Q:R:SBD:E:";
	else
		optstr = "F:G:w:e:p:a:c:b:f:m:M:d:l:L:s:O:P:R:E:";

	virtual_cuse_init_profile(&profile);

	/* reset getopt parsing */
	optreset = 1;
	optind = 1;

	while ((c = getopt(narg, pparg, optstr)) != -1) {
		switch (c) {
		case 'B':
			voss_do_background = 1;
			break;
		case 'D':
			voss_pid_path = optarg;
			break;
		case 'C':
			if (voss_mix_channels != 0) {
				return ("The -C argument may only be used once");
			}
			voss_mix_channels = atoi(optarg);
			if (voss_mix_channels >= VMAX_CHAN) {
				return ("Number of mixing channels is too high");
			}
			break;
		case 'a':
			switch (optarg[0]) {
			case '-':
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				opt_amp[0] = -(opt_amp[1] = atoi(optarg));
				break;
			case 'i':
				if (optarg[1] != ',')
					return ("Expected comma after 'i'");
				opt_amp[0] = atoi(optarg + 2);
				break;
			case 'o':
				if (optarg[1] != ',')
					return ("Expected comma after 'o'");
				opt_amp[1] = atoi(optarg + 2);
				break;
			default:
				return ("Invalid syntax for amplitude argument");
			}
			break;
		case 'E':
			voss_is_recording = (atoi(optarg) != 0);
			break;
		case 'e':
			idx = 0;
			ptr = optarg;
			memset(opt_mute, 0, sizeof(opt_mute));
			while (1) {
				c = *ptr++;
				if (c == ',' || c == 0) {
					idx++;
					if (c == 0)
						break;
					continue;
				}
				if (idx < 2 && c >= '0' && c <= '1') {
					opt_mute[idx] = c - '0';
				} else {
					return ("Invalid -e parameter");
				}
			}
			switch (idx) {
			case 1:
				opt_mute[1] = opt_mute[0];
				break;
			case 2:
				break;
			default:
				return ("Invalid -e parameter");
			}
			break;
		case 'p':
			opt_pol = atoi(optarg);
			break;
		case 'c':
			profile.channels = atoi(optarg);
			if (profile.channels == 0)
				return ("Number of channels is zero");
			if (profile.channels > VMAX_CHAN)
				return ("Number of channels is too high");
			break;
		case 'r':
			voss_dsp_sample_rate = atoi(optarg);
			if (voss_dsp_sample_rate < 8000)
				return ("Sample rate is too low, 8000 Hz");
			if (voss_dsp_sample_rate > 0xFFFFFF)
				return ("Sample rate is too high");
			break;
		case 'i':
			memset(&rtp, 0, sizeof(rtp));
			rtp.type = RTP_PRIO_REALTIME;
			rtp.prio = atoi(optarg);
			if (rtprio(RTP_SET, getpid(), &rtp) != 0)
				printf("Cannot set realtime priority\n");
			break;
		case 'b':
			profile.bits = atoi(optarg);
			switch (profile.bits) {
			case 8:
			case 16:
			case 24:
			case 32:
				break;
			default:
				return ("Invalid number of sample bits");
			}
			break;
		case 'g':
			if (profile.rx_compressor_param.enabled)
				return ("Compressor already enabled for this device");
			if (sscanf(optarg, "%d,%d,%d", &a, &b, &c) != 3 ||
			    a < VIRTUAL_OSS_KNEE_MIN ||
			    a > VIRTUAL_OSS_KNEE_MAX ||
			    b < VIRTUAL_OSS_ATTACK_MIN ||
			    b > VIRTUAL_OSS_ATTACK_MAX ||
			    c < VIRTUAL_OSS_DECAY_MIN ||
			    c > VIRTUAL_OSS_DECAY_MAX)
				return ("Invalid device compressor argument(s)");
			profile.rx_compressor_param.enabled = 1;
			profile.rx_compressor_param.knee = a;
			profile.rx_compressor_param.attack = b;
			profile.rx_compressor_param.decay = c;
			break;
		case 'x':
			if (voss_output_compressor_param.enabled)
				return ("Compressor already enabled for output");
			if (sscanf(optarg, "%d,%d,%d", &a, &b, &c) != 3 ||
			    a < VIRTUAL_OSS_KNEE_MIN ||
			    a > VIRTUAL_OSS_KNEE_MAX ||
			    b < VIRTUAL_OSS_ATTACK_MIN ||
			    b > VIRTUAL_OSS_ATTACK_MAX ||
			    c < VIRTUAL_OSS_DECAY_MIN ||
			    c > VIRTUAL_OSS_DECAY_MAX)
				return ("Invalid output compressor argument(s)");
			voss_output_compressor_param.enabled = 1;
			voss_output_compressor_param.knee = a;
			voss_output_compressor_param.attack = b;
			voss_output_compressor_param.decay = c;
			break;
		case 'f':
		case 'O':
		case 'P':
		case 'R':
			if (voss_dsp_sample_rate == 0 || voss_dsp_samples == 0)
				return ("Missing -r or -s parameters");
			if (voss_dsp_bits == 0) {
				if (profile.bits == 0)
					return ("Missing -b parameter");
				voss_dsp_bits = profile.bits;
			}
			if (voss_dsp_max_channels == 0) {
				if (profile.channels == 0)
					return ("Missing -c parameter");
				voss_dsp_max_channels = profile.channels;
			}
			if (c == 'f' || c == 'R') {
				if (strlen(optarg) > VMAX_STRING - 1)
					return ("Device name too long");
				strncpy(voss_dsp_rx_device, optarg, sizeof(voss_dsp_rx_device));
				voss_rx_backend_refresh();
				voss_dsp_rx_refresh = 1;
			}
			if (c == 'f' || c == 'P' || c == 'O') {
				if (strlen(optarg) > VMAX_STRING - 1)
					return ("Device name too long");
				strncpy(voss_dsp_tx_device, optarg, sizeof(voss_dsp_tx_device));
				voss_tx_backend_refresh();
				voss_dsp_tx_refresh = 1;

				if (c == 'O' && voss_has_synchronization == 0)
					voss_has_synchronization++;
			}
			break;
		case 'w':
			if (strlen(optarg) > VMAX_STRING - 1)
				return ("Device name too long");
			strncpy(profile.wav_name, optarg, sizeof(profile.wav_name));
			break;
		case 'd':
			if (strlen(optarg) > VMAX_STRING - 1)
				return ("Device name too long");
			strncpy(profile.oss_name, optarg, sizeof(profile.oss_name));

			if (profile.bits == 0 || voss_dsp_sample_rate == 0 ||
			    profile.channels == 0 || voss_dsp_samples == 0)
				return ("Missing -b, -r, -c or -s parameters");

			val = (voss_dsp_samples *
			    profile.bits * profile.channels) / 8;
			if (val <= 0 || val >= (1024 * 1024))
				return ("-s option value is too big");

			ptr = dup_profile(&profile, opt_amp, opt_pol,
			    opt_mute[0], opt_mute[1], 0, 1);
			if (ptr != NULL)
				return (ptr);
			break;
		case 'L':
		case 'l':
			if (strlen(optarg) > VMAX_STRING - 1)
				return ("Device name too long");
			strncpy(profile.oss_name, optarg, sizeof(profile.oss_name));

			if (profile.bits == 0 || voss_dsp_sample_rate == 0 ||
			    profile.channels == 0 || voss_dsp_samples == 0)
				return ("Missing -b, -r, -r or -s parameters");

			val = (voss_dsp_samples *
			    profile.bits * profile.channels) / 8;
			if (val <= 0 || val >= (1024 * 1024))
				return ("-s option value is too big");

			ptr = dup_profile(&profile, opt_amp, opt_pol,
			    opt_mute[0], opt_mute[1], c == 'L', 0);
			if (ptr != NULL)
				return (ptr);
			break;
		case 'S':
			voss_libsamplerate_enable = 1;
			break;
		case 'Q':
			c = atoi(optarg);
			switch (c) {
			case 0:
				voss_libsamplerate_quality = SRC_SINC_BEST_QUALITY;
				break;
			case 1:
				voss_libsamplerate_quality = SRC_SINC_MEDIUM_QUALITY;
				break;
			default:
				voss_libsamplerate_quality = SRC_SINC_FASTEST;
				break;
			}
			break;
		case 's':
			if (voss_dsp_samples != 0)
				return ("-s option may only be used once");
			if (profile.bits == 0 || profile.channels == 0)
				return ("-s option requires -b and -c options");
			if (strlen(optarg) > 2 &&
			    sscanf(optarg, "%f", &samples_ms) == 1 &&
			    strcmp(optarg + strlen(optarg) - 2, "ms") == 0) {
				if (voss_dsp_sample_rate == 0)
					return ("-s <X>ms option requires -r option");
				if (samples_ms < 0.125 || samples_ms >= 1000.0)
					return ("-s <X>ms option has invalid value");
				voss_dsp_samples = voss_dsp_sample_rate * samples_ms / 1000.0;
			} else {
				voss_dsp_samples = atoi(optarg);
			}
			if (voss_dsp_samples >= (1U << 24))
				return ("-s option requires a non-zero positive value");
			break;
		case 't':
			if (voss_ctl_device[0])
				return ("-t parameter may only be used once");

			strlcpy(voss_ctl_device, optarg, sizeof(voss_ctl_device));
			break;
		case 'm':
			ptr = optarg;
			val = 0;
			idx = 0;
			init_mapping(&profile);
			while (1) {
				c = *ptr++;
				if (c == ',' || c == 0) {
					if (idx >= (2 * VMAX_CHAN))
						return ("Too many channels in mask");
					if (idx & 1)
						profile.tx_dst[idx / 2] = val;
					else
						profile.rx_src[idx / 2] = val;
					if (c == 0)
						break;
					val = 0;
					idx++;
					continue;
				}
				if (c >= '0' && c <= '9') {
					val *= 10;
					val += c - '0';
				}
			}
			break;
		case 'M':
			ptr = optarg;
			type = *ptr;
			if (type == 'i' || type == 'o' || type == 'x') {
				vmonitor_t *pvm;

				int src = 0;
				int dst = 0;
				int pol = 0;
				int mute = 0;
				int amp = 0;
				int neg;

				ptr++;
				if (*ptr == ',')
					ptr++;
				else if (type == 'i')
					return ("Expected comma after 'i'");
				else if (type == 'o')
					return ("Expected comma after 'o'");
				else
					return ("Expected comma after 'x'");

				val = 0;
				neg = 0;
				idx = 0;
				while (1) {
					c = *ptr++;
					if (c == '-') {
						neg = 1;
						continue;
					}
					if (c == ',' || c == 0) {
						switch (idx) {
						case 0:
							src = val;
							break;
						case 1:
							dst = val;
							break;
						case 2:
							pol = val ? 1 : 0;
							break;
						case 3:
							mute = val ? 1 : 0;
							break;
						case 4:
							if (val > 31) {
								return ("Absolute amplitude "
								    "for -M parameter "
								    "cannot exceed 31");
							}
							amp = neg ? -val : val;
							break;
						default:
							break;
						}
						if (c == 0)
							break;
						val = 0;
						neg = 0;
						idx++;
						continue;
					}
					if (c >= '0' && c <= '9') {
						val *= 10;
						val += c - '0';
					}
				}
				if (idx < 4)
					return ("Too few parameters for -M");

				pvm = vmonitor_alloc(&idx,
				    (type == 'i') ? &virtual_monitor_input :
				    (type == 'x') ? &virtual_monitor_local :
				    &virtual_monitor_output);

				if (pvm == NULL)
					return ("Out of memory");

				pvm->src_chan = src;
				pvm->dst_chan = dst;
				pvm->pol = pol;
				pvm->mute = mute;
				pvm->shift = amp;
			} else {
				return ("Invalid -M parameter");
			}
			break;
		case 'F':
			if (strlen(optarg) > 2 &&
			    sscanf(optarg, "%f", &samples_ms) == 1 &&
			    strcmp(optarg + strlen(optarg) - 2, "ms") == 0) {
				if (voss_dsp_sample_rate == 0)
					return ("-F <X>ms option requires -r option");
				if (samples_ms < 0.125 || samples_ms >= 1000.0)
					return ("-F <X>ms option has invalid value");
				profile.rx_filter_size = voss_dsp_sample_rate * samples_ms / 1000.0;
			} else {
				profile.rx_filter_size = atoi(optarg);
			}
			/* make value power of two */
			while ((profile.rx_filter_size - 1) & profile.rx_filter_size)
				profile.rx_filter_size += ~(profile.rx_filter_size - 1) & profile.rx_filter_size;
			/* range check */
			if (profile.rx_filter_size > VIRTUAL_OSS_FILTER_MAX)
				return ("Invalid -F parameter is out of range");
			break;
		case 'G':
			if (strlen(optarg) > 2 &&
			    sscanf(optarg, "%f", &samples_ms) == 1 &&
			    strcmp(optarg + strlen(optarg) - 2, "ms") == 0) {
				if (voss_dsp_sample_rate == 0)
					return ("-G <X>ms option requires -r option");
				if (samples_ms < 0.125 || samples_ms >= 1000.0)
					return ("-G <X>ms option has invalid value");
				profile.tx_filter_size = voss_dsp_sample_rate * samples_ms / 1000.0;
			} else {
				profile.tx_filter_size = atoi(optarg);
			}
			/* make value power of two */
			while ((profile.tx_filter_size - 1) & profile.tx_filter_size)
				profile.tx_filter_size += ~(profile.tx_filter_size - 1) & profile.tx_filter_size;
			/* range check */
			if (profile.tx_filter_size > VIRTUAL_OSS_FILTER_MAX)
				return ("Invalid -F parameter is out of range");
			break;
		case 'N':
			profile.http.nstate = atoi(optarg);
			break;
		case 'H':
			profile.http.host = optarg;
			if (profile.http.port == NULL)
				profile.http.port = "80";
			if (profile.http.nstate == 0)
				profile.http.nstate = 1;
			break;
		case 'o':
			profile.http.port = optarg;
			break;
		case 'J':
			profile.http.rtp_ifname = optarg;
			if (profile.http.rtp_port == NULL)
				profile.http.rtp_port = "8080";
			break;
		case 'k':
			profile.http.rtp_port = optarg;
			break;
		default:
			if (is_main)
				usage();
			else
				return ("Invalid option detected");
			break;
		}
	}
	return (NULL);
}

static void
create_threads(void)
{
	int idx;

	/* Give each DSP device 4 threads */
	voss_ntds = voss_dups * 4;
	voss_tds = malloc(voss_ntds * sizeof(pthread_t));
	if (voss_tds == NULL)
		err(1, "malloc");

	for (idx = 0; idx < voss_ntds; idx++) {
		if (pthread_create(&voss_tds[idx], NULL, &virtual_cuse_process,
		    NULL) != 0)
			err(1, "pthread_create");
	}

	/* Reset until next time called */
	voss_dups = 0;
}

static void
destroy_threads(void)
{
	int idx;

	for (idx = 0; idx < voss_ntds; idx++)
		pthread_cancel(voss_tds[idx]);
	free(voss_tds);
}

void
voss_add_options(char *str)
{
	static char name[] = { "virtual_oss" };
	const char sep[] = "\t ";
	const char *ptrerr;
	char *parg[64];
	char *word;
	char *brkt;
	int narg = 0;

	parg[narg++] = name;

	for (word = strtok_r(str, sep, &brkt); word != NULL;
	     word = strtok_r(NULL, sep, &brkt)) {
		if (narg >= 64) {
			ptrerr = "Too many arguments";
			goto done;
		}
		parg[narg++] = word;
	}
	ptrerr = parse_options(narg, parg, 0);
done:
	if (ptrerr != NULL) {
		strlcpy(str, ptrerr, VIRTUAL_OSS_OPTIONS_MAX);
	} else {
		str[0] = 0;
		create_threads();
	}
}

int
main(int argc, char **argv)
{
	const char *ptrerr;
	struct sigaction sa;
	struct cuse_dev *pdev = NULL;

	TAILQ_INIT(&virtual_profile_client_head);
	TAILQ_INIT(&virtual_profile_loopback_head);

	TAILQ_INIT(&virtual_monitor_input);
	TAILQ_INIT(&virtual_monitor_output);
	TAILQ_INIT(&virtual_monitor_local);

	atomic_init();

	/* automagically load the cuse.ko module, if any */
	if (feature_present("cuse") == 0) {
		if (system("kldload cuse") == -1)
			warn("Failed to kldload cuse");
	}

	if (cuse_init() != 0)
		errx(EX_USAGE, "Could not connect to cuse module");

	signal(SIGPIPE, &virtual_pipe);

	memset(&sa, 0, sizeof(sa));
	sigfillset(&sa.sa_mask);
	sa.sa_handler = virtual_sig_exit;
	if (sigaction(SIGINT, &sa, NULL) < 0)
		err(1, "sigaction(SIGINT)");
	if (sigaction(SIGTERM, &sa, NULL) < 0)
		err(1, "sigaction(SIGTERM)");

	ptrerr = parse_options(argc, argv, 1);
	if (ptrerr != NULL)
		errx(EX_USAGE, "%s", ptrerr);

	if (voss_dsp_rx_device[0] == 0 || voss_dsp_tx_device[0] == 0)
		errx(EX_USAGE, "Missing -f argument");

	/* use DSP channels as default */
	if (voss_mix_channels == 0)
		voss_mix_channels = voss_dsp_max_channels;

	if (voss_mix_channels > voss_dsp_max_channels)
		voss_max_channels = voss_mix_channels;
	else
		voss_max_channels = voss_dsp_max_channels;

	if (voss_dsp_samples > (voss_dsp_sample_rate / 4))
		errx(EX_USAGE, "Too many buffer samples given by -s argument");

	/* check if daemon mode is requested */
	if (voss_do_background != 0 && daemon(0, 0) != 0)
		errx(EX_SOFTWARE, "Cannot become daemon");

	if (voss_pid_path != NULL) {
		int pidfile = open(voss_pid_path, O_RDWR | O_CREAT | O_TRUNC, 0600);
		pid_t mypid = getpid();
		char mypidstr[8];
		snprintf(mypidstr, sizeof(mypidstr), "%d\n", mypid);
		if (pidfile < 0)
			errx(EX_SOFTWARE, "Cannot create PID file '%s'", voss_pid_path);
		if (write(pidfile, mypidstr, strlen(mypidstr)) !=
		    (ssize_t)strlen(mypidstr))
			errx(EX_SOFTWARE, "Cannot write PID file");
		close(pidfile);
	}

	/* setup audio delay unit */
	voss_ad_init(voss_dsp_sample_rate);

	/* Create CTL device */

	if (voss_ctl_device[0] != 0) {
		pdev = cuse_dev_create(&vctl_methods, NULL, NULL,
		    0, 0, voss_dsp_perm, voss_ctl_device);
		if (pdev == NULL)
			errx(EX_USAGE, "Could not create '/dev/%s'", voss_ctl_device);

		voss_dups++;
	}

	/* Create worker threads */
	create_threads();

	/* Run DSP threads */

	virtual_oss_process(NULL);

	destroy_threads();

	if (voss_ctl_device[0] != 0)
		cuse_dev_destroy(pdev);

	return (0);
}
