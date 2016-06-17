/*
 * arch/sh64/kernel/led.c
 *
 * Copyright (C) 2002 Stuart Menefy <stuart.menefy@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Flash the LEDs
 */
#include <linux/config.h>
#include <linux/stddef.h>
#include <linux/sched.h>

void mach_alphanum(int pos, unsigned char val);
void mach_led(int pos, int val);

void print_seg(char *file, int line)
{
	int i;
	unsigned int nibble;

	for (i = 0; i < 5; i++) {
		mach_alphanum(i, file[i]);
	}

	for (i = 0; i < 3; i++) {
		nibble = ((line >> (i * 4)) & 0xf);
		mach_alphanum(7 - i, nibble + ((nibble > 9) ? 55 : 48));
	}
}

void print_seg_num(unsigned num)
{
	int i;
	unsigned int nibble;

	for (i = 0; i < 8; i++) {
		nibble = ((num >> (i * 4)) & 0xf);

		mach_alphanum(7 - i, nibble + ((nibble > 9) ? 55 : 48));
	}
}

/* acts like an actual heart beat -- ie thump-thump-pause... */
void heartbeat(void)
{
	static unsigned int cnt = 0, period = 0, dist = 0;

	if (cnt == 0 || cnt == dist) {
		mach_led(-1, 1);
	} else if (cnt == 7 || cnt == dist + 7) {
		mach_led(-1, 0);
	}

	if (++cnt > period) {
		cnt = 0;

		/* 
		 * The hyperbolic function below modifies the heartbeat period
		 * length in dependency of the current (5min) load. It goes
		 * through the points f(0)=126, f(1)=86, f(5)=51, f(inf)->30.
		 */
		period = ((672 << FSHIFT) / (5 * avenrun[0] +
					    (7 << FSHIFT))) + 30;
		dist = period / 4;
	}
}

