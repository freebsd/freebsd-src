/*-
 * Copyright (c) 2017 Hans Petter Selasky
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

#ifndef VOSS_X3_LOG2_COMBA
#define	VOSS_X3_LOG2_COMBA 5
#endif

#if (VOSS_X3_LOG2_COMBA < 2)
#error "VOSS_X3_LOG2_COMBA must be greater than 1"
#endif

struct voss_x3_input_double {
	double	a;
	double	b;
} __aligned(16);

/*
 * <input size> = "stride"
 * <output size> = 2 * "stride"
 */
static void
voss_x3_multiply_sub_double(struct voss_x3_input_double *input, double *ptr_low, double *ptr_high,
    const size_t stride, const uint8_t toggle)
{
	size_t x;
	size_t y;

	if (stride >= (1UL << VOSS_X3_LOG2_COMBA)) {
		const size_t strideh = stride >> 1;

		if (toggle) {

			/* inverse step */
			for (x = 0; x != strideh; x++) {
				double a, b, c, d;

				a = ptr_low[x];
				b = ptr_low[x + strideh];
				c = ptr_high[x];
				d = ptr_high[x + strideh];

				ptr_low[x + strideh] = a + b;
				ptr_high[x] = a + b + c + d;
			}

			voss_x3_multiply_sub_double(input, ptr_low, ptr_low + strideh, strideh, 1);

			for (x = 0; x != strideh; x++)
				ptr_low[x + strideh] = -ptr_low[x + strideh];

			voss_x3_multiply_sub_double(input + strideh, ptr_low + strideh, ptr_high + strideh, strideh, 1);

			/* forward step */
			for (x = 0; x != strideh; x++) {
				double a, b, c, d;

				a = ptr_low[x];
				b = ptr_low[x + strideh];
				c = ptr_high[x];
				d = ptr_high[x + strideh];

				ptr_low[x + strideh] = -a - b;
				ptr_high[x] = c + b - d;

				input[x + strideh].a += input[x].a;
				input[x + strideh].b += input[x].b;
			}

			voss_x3_multiply_sub_double(input + strideh, ptr_low + strideh, ptr_high, strideh, 0);
		} else {
			voss_x3_multiply_sub_double(input + strideh, ptr_low + strideh, ptr_high, strideh, 1);

			/* inverse step */
			for (x = 0; x != strideh; x++) {
				double a, b, c, d;

				a = ptr_low[x];
				b = ptr_low[x + strideh];
				c = ptr_high[x];
				d = ptr_high[x + strideh];

				ptr_low[x + strideh] = -a - b;
				ptr_high[x] = a + b + c + d;

				input[x + strideh].a -= input[x].a;
				input[x + strideh].b -= input[x].b;
			}

			voss_x3_multiply_sub_double(input + strideh, ptr_low + strideh, ptr_high + strideh, strideh, 0);

			for (x = 0; x != strideh; x++)
				ptr_low[x + strideh] = -ptr_low[x + strideh];

			voss_x3_multiply_sub_double(input, ptr_low, ptr_low + strideh, strideh, 0);

			/* forward step */
			for (x = 0; x != strideh; x++) {
				double a, b, c, d;

				a = ptr_low[x];
				b = ptr_low[x + strideh];
				c = ptr_high[x];
				d = ptr_high[x + strideh];

				ptr_low[x + strideh] = b - a;
				ptr_high[x] = c - b - d;
			}
		}
	} else {
		for (x = 0; x != stride; x++) {
			double value = input[x].a;

			for (y = 0; y != (stride - x); y++) {
				ptr_low[x + y] += input[y].b * value;
			}

			for (; y != stride; y++) {
				ptr_high[x + y - stride] += input[y].b * value;
			}
		}
	}
}

/*
 * <input size> = "max"
 * <output size> = 2 * "max"
 */
void
voss_x3_multiply_double(const int64_t *va, const double *vb, double *pc, const size_t max)
{
	struct voss_x3_input_double input[max];
	size_t x;

	/* check for non-power of two */
	if (max & (max - 1))
		return;

	/* setup input vector */
	for (x = 0; x != max; x++) {
		input[x].a = va[x];
		input[x].b = vb[x];
	}

	/* do multiplication */
	voss_x3_multiply_sub_double(input, pc, pc + max, max, 1);
}
