/*-
 * Copyright (c) 2020 Hans Petter Selasky
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

#include "int.h"
#include "virtual_oss.h"

struct virtual_compressor voss_output_compressor_param = {
	.knee = 85,
	.attack = 3,
	.decay = 20,
};
double voss_output_compressor_gain[VMAX_CHAN];

void
voss_compressor(int64_t *buffer, double *p_ch_gain,
    const struct virtual_compressor *p_param, const unsigned samples,
    const unsigned maxchan, const int64_t fmt_max)
{
	int64_t knee_amp;
	int64_t sample;
	unsigned ch;
	unsigned i;
	double amp;

	/* check if compressor is enabled */
	if (p_param->enabled != 1)
		return;

	knee_amp = (fmt_max * p_param->knee) / VIRTUAL_OSS_KNEE_MAX;

	for (ch = i = 0; i != samples; i++) {
		sample = buffer[i];
		if (sample < 0)
			sample = -sample;

		amp = p_ch_gain[ch];
		if (sample > knee_amp) {
			const double gain = (double)knee_amp / (double)sample;
			if (gain < amp)
				amp += (gain - amp) / (1LL << p_param->attack);
		}
		buffer[i] *= amp;
		amp += (1.0 - amp) / (1LL << p_param->decay);
		p_ch_gain[ch] = amp;

		if (++ch == maxchan)
			ch = 0;
	}
}
