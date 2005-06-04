/*-
 * Copyright (c) 2005 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <machine/prom.h>
#include <machine/rpb.h>

static unsigned long lastpcc;
static int tnsec;

/*
 * Count the number of elapsed seconds since this function was called first.
 * The algorithm uses the processor's cycle counter, which means that it'd
 * better be called frequently (on a 433Mhz machine this means at least once
 * every 9 seconds or so).
 */
int
getsecs()
{
	struct rpb *hwrpb = (struct rpb *)HWRPB_ADDR;
	unsigned long curpcc;
	int delta;

	if (tnsec == 0) {
		tnsec = 1;
		lastpcc = alpha_rpcc() & 0xfffffffful;
		return (tnsec);
	}

	curpcc = alpha_rpcc() & 0xfffffffful;
	if (curpcc < lastpcc)
		curpcc += 0x100000000ul;

	delta = (curpcc - lastpcc) / hwrpb->rpb_cc_freq;
	if (delta) {
		tnsec += delta;
		lastpcc = curpcc & 0xfffffffful;
	}
	return (tnsec);
}
