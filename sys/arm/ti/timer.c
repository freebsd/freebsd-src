/*-
 * Copyright (c) 2011
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
 * 3. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BEN GRAY ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BEN GRAY BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/time.h>
#include <sys/timeet.h>
#include <sys/bus.h>

void
cpu_initclocks(void)
{
}

/**
 *	DELAY - Delay for at least N microseconds.
 *	@n: number of microseconds to delay by
 *
 *	This function is called all over the kernel and is suppose to provide a
 *	consistent delay.  It is a busy loop and blocks polling a timer when called.
 *
 *	RETURNS:
 *	nothing
 */
void
DELAY(int n)
{
#if defined(CPU_CLOCKSPEED)
	int32_t counts_per_usec;
	int32_t counts;
	uint32_t first, last;
#endif
	
	if (n <= 0)
		return;
	
	/* Check the timers are setup, if not just use a for loop for the meantime */
	if (1) {

		/* If the CPU clock speed is defined we use that via the 'cycle count'
		 * register, this should give us a pretty accurate delay value.  If not
		 * defined we use a basic for loop with a very simply calculation.
		 */
#if defined(CPU_CLOCKSPEED)
		counts_per_usec = (CPU_CLOCKSPEED / 1000000);
		counts = counts_per_usec * 1000;
		
		__asm __volatile("mrc p15, 0, %0, c9, c13, 0" : "=r" (first));
		while (counts > 0) {
			__asm __volatile("mrc p15, 0, %0, c9, c13, 0" : "=r" (last));
			counts -= (int32_t)(last - first);
			first = last;
		}
#else
		uint32_t val;
		for (; n > 0; n--)
			for (val = 200; val > 0; val--)
				cpufunc_nullop(); /* 
						   * Prevent gcc from
						   * optimizing out the
						   * loop
						   */
						     
#endif		
		return;
	}
}
