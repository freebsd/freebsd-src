/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.org> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id: dtmfdecode.c,v 1.6 1999/12/13 21:25:24 hm Exp $
 *
 * $FreeBSD$
 *
 * Extract DTMF signalling from ISDN4BSD A-law coded audio data
 *
 * A-Law to linear conversion from the sox package.
 *
 */

#include <stdio.h>
#include <math.h>

/* Integer math scaling factor */
#define FSC	(1<<12)

/* Alaw parameters */
#define SIGN_BIT        (0x80)          /* Sign bit for a A-law byte. */
#define QUANT_MASK      (0xf)           /* Quantization field mask. */
#define SEG_SHIFT       (4)             /* Left shift for segment number. */
#define SEG_MASK        (0x70)          /* Segment field mask. */

static int
alaw2linear(a_val)
        unsigned char   a_val;
{
        int             t;
        int             seg;

        a_val ^= 0x55;

        t = (a_val & QUANT_MASK) << 4;
        seg = ((unsigned)a_val & SEG_MASK) >> SEG_SHIFT;
        switch (seg) {
        case 0:
                t += 8;
                break;
        case 1:
                t += 0x108;
                break;
        default:
                t += 0x108;
                t <<= seg - 1;
        }
        return ((a_val & SIGN_BIT) ? t : -t);
}

#ifdef USE_COS
/* The frequencies we're trying to detect */
static int dtmf[8] = {697, 770, 852, 941, 1209, 1336, 1477, 1633};
#else
/* precalculated: p1[kk] = (-cos(2 * 3.141592 * dtmf[kk] / 8000.0) * FSC) */
static int p1[8] = {-3497, -3369, -3212, -3027, -2384, -2040, -1635, -1164};
#endif

/* This is the Q of the filter (pole radius) */
#define POLRAD .99

#define P2 ((int)(POLRAD*POLRAD*FSC))

int
main(int argc, char **argv)
{
	int i, kk, t, nn, s, so, ia;
	int x, c, d, f, h[8], k[8], n, y[8];
#ifdef USE_COS
	int p1[8];
#endif	
	int alaw[256];
	char key[256];

	for (kk = 0; kk < 8; kk++) {
		y[kk] = h[kk] = k[kk] = 0;
#ifdef USE_COS
		p1[kk] = (-cos(2 * 3.141592 * dtmf[kk] / 8000.0) * FSC);
#endif		
	}

	for (i = 0; i < 256; i++) {
		key[i] = '?';
		alaw[i] = alaw2linear(i) / (32768/FSC);
	}

	/* We encode the tones in 8 bits, translate those to symbol */
	key[0x00] = '\0';

	key[0x11] = '1'; key[0x12] = '4'; key[0x14] = '7'; key[0x18] = '*';
	key[0x21] = '2'; key[0x22] = '5'; key[0x24] = '8'; key[0x28] = '0';
	key[0x41] = '3'; key[0x42] = '6'; key[0x44] = '9'; key[0x48] = '#';
	key[0x81] = 'A'; key[0x82] = 'B'; key[0x84] = 'C'; key[0x88] = 'D';

	nn = 0;
	ia = 0;
	so = 0;
	t = 0;
	while ((i = getchar()) != EOF)
	{
		t++;

		/* Convert to our format */
		x = alaw[i];

		/* Input amplitude */
		if (x > 0)
			ia += (x - ia) / 128;
		else
			ia += (-x - ia) / 128;

		/* For each tone */
		s = 0;
		for(kk = 0; kk < 8; kk++) {

			/* Turn the crank */
			c = (P2 * (x - k[kk])) / FSC;
			d = x + c;
			f = (p1[kk] * (d - h[kk])) / FSC;
			n = x - k[kk] - c;
			k[kk] = h[kk] + f;
			h[kk] = f + d;

			/* Detect and Average */
			if (n > 0)
				y[kk] += (n - y[kk]) / 64;
			else
				y[kk] += (-n - y[kk]) / 64;

			/* Threshold */
			if (y[kk] > FSC/10 && y[kk] > ia)
				s |= 1 << kk;
		}

		/* Hysteresis and noise supressor */
		if (s != so) {
/* printf("x %d %x -> %x\n",t,so, s); */
			nn = 0;
			so = s;
		} else if (nn++ == 520 && key[s]) {
			putchar(key[s]);
/* printf(" %d %x\n",t,s); */
		}
	}
	putchar('\n');
	return (0);
}
