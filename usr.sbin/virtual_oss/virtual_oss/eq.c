/*-
 * Copyright (c) 2021 Hans Petter Selasky
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

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "int.h"

void
vclient_tx_equalizer(struct virtual_client *pvc,
    int64_t *src, size_t total)
{
	double *f_data;
	size_t channels;
	size_t f_size;
	size_t x;

	f_size = pvc->profile->tx_filter_size;
	if (f_size == 0 || total == 0)
		return;

	channels = pvc->channels;
	total /= channels;

	while (1) {
		size_t delta;
		size_t offset;
		size_t y;

		offset = pvc->tx_filter_offset;
		delta = f_size - offset;

		if (delta > total)
			delta = total;

		for (x = 0; x != channels; x++) {
			f_data = pvc->profile->tx_filter_data[x];
			if (f_data == NULL)
				continue;

			for (y = 0; y != delta; y++) {
				pvc->tx_filter_in[x][y + offset] = src[x + y * channels];
				src[x + y * channels] = pvc->tx_filter_out[x][y + offset];
			}
		}

		pvc->tx_filter_offset += delta;
		total -= delta;
		src += delta * channels;

		/* check if there is enough data for a new transform */
		if (pvc->tx_filter_offset == f_size) {
			for (x = 0; x != channels; x++) {
				f_data = pvc->profile->tx_filter_data[x];
				if (f_data == NULL)
					continue;

				/* shift down output */
				for (y = 0; y != f_size; y++) {
					pvc->tx_filter_out[x][y] = pvc->tx_filter_out[x][y + f_size];
					pvc->tx_filter_out[x][y + f_size] = 0;
				}
				/* perform transform */
				voss_x3_multiply_double(pvc->tx_filter_in[x],
				    f_data, pvc->tx_filter_out[x], f_size);
			}
			pvc->tx_filter_offset = 0;
		}
		if (total == 0)
			break;
	}
}

void
vclient_rx_equalizer(struct virtual_client *pvc,
    int64_t *src, size_t total)
{
	double *f_data;
	size_t channels;
	size_t f_size;
	size_t x;

	f_size = pvc->profile->rx_filter_size;

	if (f_size == 0 || total == 0)
		return;

	channels = pvc->channels;
	total /= channels;

	while (1) {
		size_t delta;
		size_t offset;
		size_t y;

		offset = pvc->rx_filter_offset;
		delta = f_size - offset;

		if (delta > total)
			delta = total;

		for (x = 0; x != channels; x++) {
			f_data = pvc->profile->rx_filter_data[x];
			if (f_data == NULL)
				continue;

			for (y = 0; y != delta; y++) {
				pvc->rx_filter_in[x][y + offset] = src[x + y * channels];
				src[x + y * channels] = pvc->rx_filter_out[x][y + offset];
			}
		}

		pvc->rx_filter_offset += delta;
		total -= delta;
		src += delta * channels;

		/* check if there is enough data for a new transform */
		if (pvc->rx_filter_offset == f_size) {
			for (x = 0; x != channels; x++) {
				f_data = pvc->profile->rx_filter_data[x];
				if (f_data == NULL)
					continue;

				/* shift output down */
				for (y = 0; y != f_size; y++) {
					pvc->rx_filter_out[x][y] = pvc->rx_filter_out[x][y + f_size];
					pvc->rx_filter_out[x][y + f_size] = 0;
				}
				/* perform transform */
				voss_x3_multiply_double(pvc->rx_filter_in[x],
				    f_data, pvc->rx_filter_out[x], f_size);
			}
			pvc->rx_filter_offset = 0;
		}
		if (total == 0)
			break;
	}
}

int
vclient_eq_alloc(struct virtual_client *pvc)
{
	uint8_t x;

	pvc->tx_filter_offset = 0;
	pvc->rx_filter_offset = 0;

	for (x = 0; x != pvc->channels; x++) {
		uint32_t size;

		size = pvc->profile->tx_filter_size;
		if (size != 0) {
			pvc->tx_filter_in[x] =
			    malloc(sizeof(pvc->tx_filter_in[x][0]) * size);
			pvc->tx_filter_out[x] =
			    calloc(2 * size, sizeof(pvc->tx_filter_out[x][0]));
			if (pvc->tx_filter_in[x] == NULL ||
			    pvc->tx_filter_out[x] == NULL)
				goto error;
		}
		size = pvc->profile->rx_filter_size;
		if (size != 0) {
			pvc->rx_filter_in[x] =
			    malloc(sizeof(pvc->rx_filter_in[x][0]) * size);
			pvc->rx_filter_out[x] =
			    calloc(2 * size, sizeof(pvc->rx_filter_out[x][0]));
			if (pvc->rx_filter_in[x] == NULL ||
			    pvc->rx_filter_out[x] == NULL)
				goto error;
		}
	}
	return (0);

error:
	vclient_eq_free(pvc);
	return (ENOMEM);
}

void
vclient_eq_free(struct virtual_client *pvc)
{
	uint8_t x;

	pvc->tx_filter_offset = 0;
	pvc->rx_filter_offset = 0;

	for (x = 0; x != VMAX_CHAN; x++) {
		free(pvc->tx_filter_in[x]);
		pvc->tx_filter_in[x] = NULL;

		free(pvc->rx_filter_in[x]);
		pvc->rx_filter_in[x] = NULL;

		free(pvc->tx_filter_out[x]);
		pvc->tx_filter_out[x] = NULL;

		free(pvc->rx_filter_out[x]);
		pvc->rx_filter_out[x] = NULL;
	}
}
