/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.org> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id: dtmfdecode.c,v 1.1 1999/02/15 19:13:47 hm Exp $
 *
 * Extract DTMF signalling from a .g711a file from ISDN4BSD
 *
 * A-Law to linear conversion from the sox package.
 *
 */

#include <stdio.h>
#include <math.h>

/*
 * g711.c
 *
 * u-law, A-law and linear PCM conversions.
 */
#define SIGN_BIT        (0x80)          /* Sign bit for a A-law byte. */
#define QUANT_MASK      (0xf)           /* Quantization field mask. */
#define NSEGS           (8)             /* Number of A-law segments. */
#define SEG_SHIFT       (4)             /* Left shift for segment number. */
#define SEG_MASK        (0x70)          /* Segment field mask. */

/*
 * alaw2linear() - Convert an A-law value to 16-bit linear PCM
 *
 */
int
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

int flip[256];

double dtmf[8] = {697, 770, 852, 941, 1209, 1336, 1477, 1633};
double p1[8];

/* This is the Q of the filter (pole radius).  must be less than 1.0 */
#define POLRAD .99

#define P2 (POLRAD*POLRAD)

#define NNN 100

char key [256];

int
main(int argc, char **argv)
{
	int i, j, kk, nn, s, so = 0;
	double x, a[8], b[8], c[8], d[8], e[8], f[8], g[8], h[8], k[8], l[8], m[8], n[8], y[8];


	for (kk = 0; kk < 8; kk++) {
		y[kk] = g[kk] = k[kk] = 0.0;
		p1[kk] = (-cos(2 * 3.141592 * dtmf[kk] / 8000.0));
	}

	for (i=0;i<256;i++) {
		key[i] = '\0';
		flip[i]  = (i &   1) << 7;
		flip[i] |= (i &   2) << 5;
		flip[i] |= (i &   4) << 3;
		flip[i] |= (i &   8) << 1;
		flip[i] |= (i &  16) >> 1;
		flip[i] |= (i &  32) << 3;
		flip[i] |= (i &  64) << 5;
		flip[i] |= (i & 128) << 7;
	}

	key[0x00] = '\0';
	key[0x11] = '1';
	key[0x12] = '4';
	key[0x14] = '7';
	key[0x18] = '*';

	key[0x21] = '2';
	key[0x22] = '5';
	key[0x24] = '8';
	key[0x28] = '0';

	key[0x41] = '3';
	key[0x42] = '6';
	key[0x44] = '9';
	key[0x48] = '#';

	key[0x81] = 'A';
	key[0x82] = 'B';
	key[0x84] = 'C';
	key[0x88] = 'D';

	x = 0.0;
	nn = 0;
	while ((i = getchar()) != EOF) {
		i = flip[i];
		j = alaw2linear(i);

		x = j / 32768.0;
		s = 0;
		for(kk = 0; kk < 8; kk++) {
			a[kk] = x;
			h[kk] = g[kk];
			l[kk] = k[kk];

			b[kk] = a[kk] - l[kk];
			c[kk] = P2 * b[kk];
			d[kk] = a[kk] + c[kk];
			e[kk] = d[kk] - h[kk];
			f[kk] = p1[kk] * e[kk];
			g[kk] = f[kk] + d[kk];
			k[kk] = h[kk] + f[kk];
			m[kk] = l[kk] + c[kk];
			n[kk] = a[kk] - m[kk];
		
			y[kk] += (fabs(n[kk]) - y[kk]) / 20.0;
			if (y[kk] > .1)
				s |= 1 << kk;
		}
		if (s != so)
			nn = 0;
		else
			nn++;
		if (nn == NNN) {
			if (key[s])
				putchar(key[s]);
		}
		so = s;
	}
	printf("\n");
	return (0);
}
