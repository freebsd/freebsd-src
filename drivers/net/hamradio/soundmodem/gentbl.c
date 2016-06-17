/*****************************************************************************/

/*
 *      gentbl.c  -- soundcard radio modem driver table generator.
 *
 *      Copyright (C) 1996  Thomas Sailer (sailer@ife.ee.ethz.ch)
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Please note that the GPL allows you to use the driver, NOT the radio.
 *  In order to use the radio, you need a license from the communications
 *  authority of your country.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

/* -------------------------------------------------------------------- */

static void gentbl_offscostab(FILE *f, unsigned int nbits)
{
	int i;

	fprintf(f, "\n/*\n * small cosine table in U8 format\n */\n"
		"#define OFFSCOSTABBITS %u\n"
		"#define OFFSCOSTABSIZE (1<<OFFSCOSTABBITS)\n\n", 
		nbits);
	fprintf(f, "static unsigned char offscostab[OFFSCOSTABSIZE] = {\n\t");
	for (i = 0; i < (1<<nbits); i++) {
		fprintf(f, "%4u", (int)
			(128+127.0*cos(i*2.0*M_PI/(1<<nbits))));
		if (i < (1<<nbits)-1) 
			fprintf(f, "%s", (i & 7) == 7 ? ",\n\t" : ",");
	}
	fprintf(f, "\n};\n\n"
		"#define OFFSCOS(x) offscostab[((x)>>%d)&0x%x]\n\n",
		16-nbits, (1<<nbits)-1);
}

/* -------------------------------------------------------------------- */

static void gentbl_costab(FILE *f, unsigned int nbits)
{
        int i;

	fprintf(f, "\n/*\n * more accurate cosine table\n */\n\n"
		"static const short costab[%d] = {", (1<<nbits));
        for (i = 0; i < (1<<nbits); i++) {
                if (!(i & 7))
                        fprintf(f, "\n\t");
                fprintf(f, "%6d", (int)(32767.0*cos(i*2.0*M_PI/(1<<nbits))));
                if (i != ((1<<nbits)-1))
                        fprintf(f, ", ");
        }
        fprintf(f, "\n};\n\n#define COS(x) costab[((x)>>%d)&0x%x]\n"
		"#define SIN(x) COS((x)+0xc000)\n\n", 16-nbits,
		(1<<nbits)-1);
}

/* -------------------------------------------------------------------- */

#define AFSK12_SAMPLE_RATE 9600
#define AFSK12_TX_FREQ_LO 1200
#define AFSK12_TX_FREQ_HI 2200
#define AFSK12_CORRLEN 8

static void gentbl_afsk1200(FILE *f)
{
        int i, v, sum;

#define ARGLO(x) 2.0*M_PI*(double)x*(double)AFSK12_TX_FREQ_LO/(double)AFSK12_SAMPLE_RATE
#define ARGHI(x) 2.0*M_PI*(double)x*(double)AFSK12_TX_FREQ_HI/(double)AFSK12_SAMPLE_RATE

	fprintf(f, "\n/*\n * afsk1200 specific tables\n */\n"
		"#define AFSK12_SAMPLE_RATE %u\n"
		"#define AFSK12_TX_FREQ_LO %u\n"
		"#define AFSK12_TX_FREQ_HI %u\n"
		"#define AFSK12_CORRLEN %u\n\n",
		AFSK12_SAMPLE_RATE, AFSK12_TX_FREQ_LO, 
		AFSK12_TX_FREQ_HI, AFSK12_CORRLEN);
	fprintf(f, "static const int afsk12_tx_lo_i[] = {\n\t");
        for(sum = i = 0; i < AFSK12_CORRLEN; i++) {
		sum += (v = 127.0*cos(ARGLO(i)));
                fprintf(f, " %4i%c", v, (i < AFSK12_CORRLEN-1) ? ',' : ' ');
	}
        fprintf(f, "\n};\n#define SUM_AFSK12_TX_LO_I %d\n\n"
		"static const int afsk12_tx_lo_q[] = {\n\t", sum);
        for(sum = i = 0; i < AFSK12_CORRLEN; i++) {
		sum += (v = 127.0*sin(ARGLO(i))); 
                fprintf(f, " %4i%c", v, (i < AFSK12_CORRLEN-1) ? ',' : ' ');
	}
        fprintf(f, "\n};\n#define SUM_AFSK12_TX_LO_Q %d\n\n"
		"static const int afsk12_tx_hi_i[] = {\n\t", sum);
        for(sum = i = 0; i < AFSK12_CORRLEN; i++) {
		sum += (v = 127.0*cos(ARGHI(i))); 
                fprintf(f, " %4i%c", v, (i < AFSK12_CORRLEN-1) ? ',' : ' ');
	}
        fprintf(f, "\n};\n#define SUM_AFSK12_TX_HI_I %d\n\n"
		"static const int afsk12_tx_hi_q[] = {\n\t", sum);
        for(sum = i = 0; i < AFSK12_CORRLEN; i++)  {
		sum += (v = 127.0*sin(ARGHI(i)));
                fprintf(f, " %4i%c", v, (i < AFSK12_CORRLEN-1) ? ',' : ' ');
	}
	fprintf(f, "\n};\n#define SUM_AFSK12_TX_HI_Q %d\n\n", sum);
#undef ARGLO
#undef ARGHI
}

/* -------------------------------------------------------------------- */

static const float fsk96_tx_coeff_4[32] = {
              -0.001152,        0.000554,        0.002698,        0.002753,
              -0.002033,       -0.008861,       -0.008002,        0.006607,
               0.023727,        0.018905,       -0.018056,       -0.057957,
              -0.044368,        0.055683,        0.207667,        0.322048,
               0.322048,        0.207667,        0.055683,       -0.044368,
              -0.057957,       -0.018056,        0.018905,        0.023727,
               0.006607,       -0.008002,       -0.008861,       -0.002033,
               0.002753,        0.002698,        0.000554,       -0.001152
};

static const float fsk96_tx_coeff_5[40] = {
              -0.001009,       -0.000048,        0.001376,        0.002547,
               0.002061,       -0.001103,       -0.005795,       -0.008170,
              -0.004017,        0.006924,        0.018225,        0.019238,
               0.002925,       -0.025777,       -0.048064,       -0.039683,
               0.013760,        0.104144,        0.200355,        0.262346,
               0.262346,        0.200355,        0.104144,        0.013760,
              -0.039683,       -0.048064,       -0.025777,        0.002925,
               0.019238,        0.018225,        0.006924,       -0.004017,
              -0.008170,       -0.005795,       -0.001103,        0.002061,
               0.002547,        0.001376,       -0.000048,       -0.001009
};

#define HAMMING(x) (0.54-0.46*cos(2*M_PI*(x)));

static inline float hamming(float x)
{
	return 0.54-0.46*cos(2*M_PI*x);
}

static inline float sinc(float x)
{
	if (x == 0)
		return 1;
	x *= M_PI;
	return sin(x)/x;
}

static void gentbl_fsk9600(FILE *f)
{
        int i, j, k, l, m;
	float s;
	float c[44];
	float min, max;

	fprintf(f, "\n/*\n * fsk9600 specific tables\n */\n");
	min = max = 0;
	memset(c, 0, sizeof(c));
#if 0
	memcpy(c+2, fsk96_tx_coeff_4, sizeof(fsk96_tx_coeff_4));
#else
	for (i = 0; i < 29; i++)
		c[3+i] = sinc(1.2*((i-14.0)/4.0))*hamming(i/28.0)/3.5;
#endif
	fprintf(f, "static unsigned char fsk96_txfilt_4[] = {\n\t");
	for (i = 0; i < 4; i++) {
		for (j = 0; j < 256; j++) {
			for (k = 1, s = 0, l = i; k < 256; k <<= 1) {
				if (j & k) {
					for (m = 0; m < 4; m++, l++)
						s += c[l];
				} else {
					for (m = 0; m < 4; m++, l++)
						s -= c[l];
				}
			}
			s *= 0.75;
			if (s > max)
				max = s;
			if (s < min)
				min = s;
			fprintf(f, "%4d", (int)(128+127*s));
			if (i < 3 || j < 255)
				fprintf(f, ",%s", (j & 7) == 7 
					? "\n\t" : "");
		}
	}
#ifdef VERBOSE
	fprintf(stderr, "fsk9600: txfilt4: min = %f; max = %f\n", min, max);
#endif
	fprintf(f, "\n};\n\n");
	min = max = 0;
	memset(c, 0, sizeof(c));
#if 0
	memcpy(c+2, fsk96_tx_coeff_5, sizeof(fsk96_tx_coeff_5));
#else
	for (i = 0; i < 36; i++)
		c[4+i] = sinc(1.2*((i-17.5)/5.0))*hamming(i/35.0)/4.5;
#endif
	fprintf(f, "static unsigned char fsk96_txfilt_5[] = {\n\t");
	for (i = 0; i < 5; i++) {
		for (j = 0; j < 256; j++) {
			for (k = 1, s = 0, l = i; k < 256; k <<= 1) {
				if (j & k) {
					for (m = 0; m < 5; m++, l++)
						s += c[l];
				} else {
					for (m = 0; m < 5; m++, l++)
						s -= c[l];
				}
			}
			s *= 0.75;
			if (s > max)
				max = s;
			if (s < min)
				min = s;
			fprintf(f, "%4d", (int)(128+127*s));
			if (i < 4 || j < 255)
				fprintf(f, ",%s", (j & 7) == 7 
					? "\n\t" : "");
		}
	}
#ifdef VERBOSE
	fprintf(stderr, "fsk9600: txfilt5: min = %f; max = %f\n", min, max);
#endif
	fprintf(f, "\n};\n\n");
}
	
/* -------------------------------------------------------------------- */

#define AFSK26_SAMPLERATE  16000

#define AFSK26_NUMCAR      2
#define AFSK26_FIRSTCAR    2000
#define AFSK26_MSK_LEN     6
#define AFSK26_RXOVER      2

#define AFSK26_DEMCORRLEN (2*AFSK26_MSK_LEN)

#define AFSK26_WINDOW(x) ((1-cos(2.0*M_PI*(x)))/2.0)

#define AFSK26_AMPL(x) (((x)?1.0:0.7))

#undef AFSK26_AMPL
#define AFSK26_AMPL(x) 1

static void gentbl_afsk2666(FILE *f)
{
        int i, j, k, l, o, v, sumi, sumq;
        float window[AFSK26_DEMCORRLEN*AFSK26_RXOVER];
	int cfreq[AFSK26_NUMCAR];

	fprintf(f, "\n/*\n * afsk2666 specific tables\n */\n"
		"#define AFSK26_DEMCORRLEN %d\n"
		"#define AFSK26_SAMPLERATE %d\n\n", AFSK26_DEMCORRLEN,
		AFSK26_SAMPLERATE);
	fprintf(f, "static const unsigned int afsk26_carfreq[%d] = { ",
		AFSK26_NUMCAR);
	for (i = 0; i < AFSK26_NUMCAR; i++) {
		cfreq[i] = 0x10000*AFSK26_FIRSTCAR/AFSK26_SAMPLERATE+
			0x10000*i/AFSK26_MSK_LEN/2;
		fprintf(f, "0x%x", cfreq[i]);
		if (i < AFSK26_NUMCAR-1)
			fprintf(f, ", ");
	}
	fprintf(f, " };\n\n");
        for (i = 0; i < AFSK26_DEMCORRLEN*AFSK26_RXOVER; i++)
                window[i] = AFSK26_WINDOW(((float)i)/(AFSK26_DEMCORRLEN*
						      AFSK26_RXOVER)) * 127.0;
        fprintf(f, "\nstatic const struct {\n\t"
               "int i[%d];\n\tint q[%d];\n} afsk26_dem_tables[%d][%d] = {\n", 
               AFSK26_DEMCORRLEN, AFSK26_DEMCORRLEN, AFSK26_RXOVER, AFSK26_NUMCAR);
        for (o = AFSK26_RXOVER-1; o >= 0; o--) {
                fprintf(f, "\t{\n");
                for (i = 0; i < AFSK26_NUMCAR; i++) {
                        j = cfreq[i];
                        fprintf(f, "\t\t{{ ");
                        for (l = AFSK26_DEMCORRLEN-1, k = (j * o)/AFSK26_RXOVER, sumi = 0; l >= 0; 
                             l--, k = (k+j)&0xffffu) {
				sumi += (v = AFSK26_AMPL(i)*window[l*AFSK26_RXOVER+o]*
					 cos(M_PI*k/32768.0));
                                fprintf(f, "%6d%s", v, l ? ", " : " }, { ");
			}
                        for (l = AFSK26_DEMCORRLEN-1, k = (j * o)/AFSK26_RXOVER, sumq = 0; l >= 0; 
                             l--, k = (k+j)&0xffffu) {
				sumq += (v = AFSK26_AMPL(i)*window[l*AFSK26_RXOVER+o]*
					 sin(M_PI*k/32768.0));
                                fprintf(f, "%6d%s", v, l ? ", " : " }}");
			}
                        if (i < 1)
                                fprintf(f, ",");
                        fprintf(f, "\n#define AFSK26_DEM_SUM_I_%d_%d %d\n"
				"#define AFSK26_DEM_SUM_Q_%d_%d %d\n", 
				AFSK26_RXOVER-1-o, i, sumi, AFSK26_RXOVER-1-o, i, sumq);
                }
                fprintf(f, "\t}%s\n", o ? "," : "");
        }
        fprintf(f, "};\n\n");
}

/* -------------------------------------------------------------------- */

#define ATAN_TABLEN 1024

static void gentbl_atantab(FILE *f)
{
        int i;
        short x;

	fprintf(f, "\n/*\n"
		" * arctan table (indexed by i/q; should really be indexed by i/(i+q)\n"
		" */\n""#define ATAN_TABLEN %d\n\n"
		"static const unsigned short atan_tab[ATAN_TABLEN+2] = {",
               ATAN_TABLEN);
        for (i = 0; i <= ATAN_TABLEN; i++) {
                if (!(i & 7))
                        fprintf(f, "\n\t");
                x = atan(i / (float)ATAN_TABLEN) / M_PI * 0x8000;
                fprintf(f, "%6d, ", x);
        }
        fprintf(f, "%6d\n};\n\n", x);
	
}

/* -------------------------------------------------------------------- */

#define PSK48_TXF_OVERSAMPLING 5
#define PSK48_TXF_NUMSAMPLES 16
#define PSK48_RXF_LEN     64

static const float psk48_tx_coeff[80] = {
              -0.000379,       -0.000640,       -0.000000,        0.000772,
               0.000543,       -0.000629,       -0.001187,       -0.000000,
               0.001634,        0.001183,       -0.001382,       -0.002603,
              -0.000000,        0.003481,        0.002472,       -0.002828,
              -0.005215,       -0.000000,        0.006705,        0.004678,
              -0.005269,       -0.009584,       -0.000000,        0.012065,
               0.008360,       -0.009375,       -0.017028,       -0.000000,
               0.021603,        0.015123,       -0.017229,       -0.032012,
              -0.000000,        0.043774,        0.032544,       -0.040365,
              -0.084963,       -0.000000,        0.201161,        0.374060,
               0.374060,        0.201161,       -0.000000,       -0.084963,
              -0.040365,        0.032544,        0.043774,       -0.000000,
              -0.032012,       -0.017229,        0.015123,        0.021603,
              -0.000000,       -0.017028,       -0.009375,        0.008360,
               0.012065,       -0.000000,       -0.009584,       -0.005269,
               0.004678,        0.006705,       -0.000000,       -0.005215,
              -0.002828,        0.002472,        0.003481,       -0.000000,
              -0.002603,       -0.001382,        0.001183,        0.001634,
              -0.000000,       -0.001187,       -0.000629,        0.000543,
               0.000772,       -0.000000,       -0.000640,       -0.000379
};

static const float psk48_rx_coeff[PSK48_RXF_LEN] = {
              -0.000219,        0.000360,        0.000873,        0.001080,
               0.000747,       -0.000192,       -0.001466,       -0.002436,
              -0.002328,       -0.000699,        0.002101,        0.004809,
               0.005696,        0.003492,       -0.001633,       -0.007660,
              -0.011316,       -0.009627,       -0.001780,        0.009712,
               0.019426,        0.021199,        0.011342,       -0.008583,
              -0.030955,       -0.044093,       -0.036634,       -0.002651,
               0.054742,        0.123101,        0.184198,        0.220219,
               0.220219,        0.184198,        0.123101,        0.054742,
              -0.002651,       -0.036634,       -0.044093,       -0.030955,
              -0.008583,        0.011342,        0.021199,        0.019426,
               0.009712,       -0.001780,       -0.009627,       -0.011316,
              -0.007660,       -0.001633,        0.003492,        0.005696,
               0.004809,        0.002101,       -0.000699,       -0.002328,
              -0.002436,       -0.001466,       -0.000192,        0.000747,
               0.001080,        0.000873,        0.000360,       -0.000219
};

static void gentbl_psk4800(FILE *f)
{
        int i, j, k;
        short x;

	fprintf(f, "\n/*\n * psk4800 specific tables\n */\n"
		"#define PSK48_TXF_OVERSAMPLING %d\n"
		"#define PSK48_TXF_NUMSAMPLES %d\n\n"
		"#define PSK48_SAMPLERATE  8000\n"
		"#define PSK48_CAR_FREQ    2000\n"
		"#define PSK48_PSK_LEN     5\n"
		"#define PSK48_RXF_LEN     %u\n"
		"#define PSK48_PHASEINC    (0x10000*PSK48_CAR_FREQ/PSK48_SAMPLERATE)\n"
		"#define PSK48_SPHASEINC   (0x10000/(2*PSK48_PSK_LEN))\n\n"
		"static const short psk48_tx_table[PSK48_TXF_OVERSAMPLING*"
		"PSK48_TXF_NUMSAMPLES*8*2] = {", 
		PSK48_TXF_OVERSAMPLING, PSK48_TXF_NUMSAMPLES, PSK48_RXF_LEN);
        for (i = 0; i < PSK48_TXF_OVERSAMPLING; i++) {
                for (j = 0; j < PSK48_TXF_NUMSAMPLES; j++) {
                        fprintf(f, "\n\t");
                        for (k = 0; k < 8; k++) {
                                x = 32767.0 * cos(k*M_PI/4.0) * 
                                        psk48_tx_coeff[j * PSK48_TXF_OVERSAMPLING + i];
                                fprintf(f, "%6d, ", x);
                        }
                        fprintf(f, "\n\t");
                        for (k = 0; k < 8; k++) {
                                x = 32767.0 * sin(k*M_PI/4.0) * 
                                        psk48_tx_coeff[j * PSK48_TXF_OVERSAMPLING + i];
                                fprintf(f, "%6d", x);
                                if (k != 7 || j != PSK48_TXF_NUMSAMPLES-1 || 
                                    i != PSK48_TXF_OVERSAMPLING-1)
                                        fprintf(f, ", ");
                        }
                }
        }
        fprintf(f, "\n};\n\n");

	fprintf(f, "static const short psk48_rx_coeff[PSK48_RXF_LEN] = {\n\t");
	for (i = 0; i < PSK48_RXF_LEN; i++) {
		fprintf(f, "%6d", (int)(psk48_rx_coeff[i]*32767.0));
		if (i < PSK48_RXF_LEN-1)
			fprintf(f, ",%s", (i & 7) == 7 ? "\n\t" : "");
	}
	fprintf(f, "\n};\n\n");
}

/* -------------------------------------------------------------------- */

static void gentbl_hapn4800(FILE *f)
{
        int i, j, k, l;
	float s;
	float c[44];
	float min, max;

	fprintf(f, "\n/*\n * hapn4800 specific tables\n */\n\n");
	/*
	 * firstly generate tables for the FM transmitter modulator
	 */
	min = max = 0;
	memset(c, 0, sizeof(c));
	for (i = 0; i < 24; i++)
		c[8+i] = sinc(1.5*((i-11.5)/8.0))*hamming(i/23.0)/2.4;
	for (i = 0; i < 24; i++)
		c[i] -= c[i+8];
	fprintf(f, "static unsigned char hapn48_txfilt_8[] = {\n\t");
	for (i = 0; i < 8; i++) {
		for (j = 0; j < 16; j++) {
			for (k = 1, s = 0, l = i; k < 16; k <<= 1, l += 8) {
				if (j & k)
					s += c[l];
				else 
					s -= c[l];
			}
			if (s > max)
				max = s;
			if (s < min)
				min = s;
			fprintf(f, "%4d", (int)(128+127*s));
			if (i < 7 || j < 15)
				fprintf(f, ",%s", (j & 7) == 7 
					? "\n\t" : "");
		}
	}
#ifdef VERBOSE
	fprintf(stderr, "hapn4800: txfilt8: min = %f; max = %f\n", min, max);
#endif
	fprintf(f, "\n};\n\n");
	min = max = 0;
	memset(c, 0, sizeof(c));
	for (i = 0; i < 30; i++)
		c[10+i] = sinc(1.5*((i-14.5)/10.0))*hamming(i/29.0)/2.4;
	for (i = 0; i < 30; i++)
		c[i] -= c[i+10];
	fprintf(f, "static unsigned char hapn48_txfilt_10[] = {\n\t");
	for (i = 0; i < 10; i++) {
		for (j = 0; j < 16; j++) {
			for (k = 1, s = 0, l = i; k < 16; k <<= 1, l += 10) {
				if (j & k) 
					s += c[l];
				else 
					s -= c[l];
			}
			if (s > max)
				max = s;
			if (s < min)
				min = s;
			fprintf(f, "%4d", (int)(128+127*s));
			if (i < 9 || j < 15)
				fprintf(f, ",%s", (j & 7) == 7 
					? "\n\t" : "");
		}
	}
#ifdef VERBOSE
	fprintf(stderr, "hapn4800: txfilt10: min = %f; max = %f\n", min, max);
#endif
	fprintf(f, "\n};\n\n");
	/*
	 * secondly generate tables for the PM transmitter modulator
	 */
	min = max = 0;
	memset(c, 0, sizeof(c));
	for (i = 0; i < 25; i++)
		c[i] = sinc(1.4*((i-12.0)/8.0))*hamming(i/24.0)/6.3;
	for (i = 0; i < 25; i++)
		for (j = 1; j < 8; j++)
			c[i] += c[i+j];
	fprintf(f, "static unsigned char hapn48_txfilt_pm8[] = {\n\t");
	for (i = 0; i < 8; i++) {
		for (j = 0; j < 16; j++) {
			for (k = 1, s = 0, l = i; k < 16; k <<= 1, l += 8) {
				if (j & k)
					s += c[l];
				else 
					s -= c[l];
			}
			if (s > max)
				max = s;
			if (s < min)
				min = s;
			fprintf(f, "%4d", (int)(128+127*s));
			if (i < 7 || j < 15)
				fprintf(f, ",%s", (j & 7) == 7 
					? "\n\t" : "");
		}
	}
#ifdef VERBOSE
	fprintf(stderr, "hapn4800: txfiltpm8: min = %f; max = %f\n", min, max);
#endif
	fprintf(f, "\n};\n\n");
	min = max = 0;
	memset(c, 0, sizeof(c));
	for (i = 0; i < 31; i++)
		c[10+i] = sinc(1.4*((i-15.0)/10.0))*hamming(i/30.0)/7.9;
	for (i = 0; i < 31; i++)
		for (j = 1; j < 10; j++)
			c[i] += c[i+j];
	fprintf(f, "static unsigned char hapn48_txfilt_pm10[] = {\n\t");
	for (i = 0; i < 10; i++) {
		for (j = 0; j < 16; j++) {
			for (k = 1, s = 0, l = i; k < 16; k <<= 1, l += 10) {
				if (j & k) 
					s += c[l];
				else 
					s -= c[l];
			}
			if (s > max)
				max = s;
			if (s < min)
				min = s;
			fprintf(f, "%4d", (int)(128+127*s));
			if (i < 9 || j < 15)
				fprintf(f, ",%s", (j & 7) == 7 
					? "\n\t" : "");
		}
	}
#ifdef VERBOSE
	fprintf(stderr, "hapn4800: txfiltpm10: min = %f; max = %f\n", min, max);
#endif
	fprintf(f, "\n};\n\n");
	
}

/* -------------------------------------------------------------------- */

#define AFSK24_SAMPLERATE  16000
#define AFSK24_CORRLEN     14

static void gentbl_afsk2400(FILE *f, float tcm3105clk)
{
	int i, sum, v;

	fprintf(f, "\n/*\n * afsk2400 specific tables (tcm3105 clk %7fHz)\n */\n"
		"#define AFSK24_TX_FREQ_LO %d\n"
		"#define AFSK24_TX_FREQ_HI %d\n"
		"#define AFSK24_BITPLL_INC %d\n"
		"#define AFSK24_SAMPLERATE %d\n\n", tcm3105clk, 
		(int)(tcm3105clk/3694.0), (int)(tcm3105clk/2015.0), 
		0x10000*2400/AFSK24_SAMPLERATE, AFSK24_SAMPLERATE);

#define ARGLO(x) 2.0*M_PI*(double)x*(tcm3105clk/3694.0)/(double)AFSK24_SAMPLERATE
#define ARGHI(x) 2.0*M_PI*(double)x*(tcm3105clk/2015.0)/(double)AFSK24_SAMPLERATE
#define WINDOW(x) hamming((float)(x)/(AFSK24_CORRLEN-1.0))

	fprintf(f, "static const int afsk24_tx_lo_i[] = {\n\t");
        for(sum = i = 0; i < AFSK24_CORRLEN; i++) {
		sum += (v = 127.0*cos(ARGLO(i))*WINDOW(i));
                fprintf(f, " %4i%c", v, (i < AFSK24_CORRLEN-1) ? ',' : ' ');
	}
        fprintf(f, "\n};\n#define SUM_AFSK24_TX_LO_I %d\n\n"
		"static const int afsk24_tx_lo_q[] = {\n\t", sum);
        for(sum = i = 0; i < AFSK24_CORRLEN; i++) {
		sum += (v = 127.0*sin(ARGLO(i))*WINDOW(i)); 
                fprintf(f, " %4i%c", v, (i < AFSK24_CORRLEN-1) ? ',' : ' ');
	}
        fprintf(f, "\n};\n#define SUM_AFSK24_TX_LO_Q %d\n\n"
		"static const int afsk24_tx_hi_i[] = {\n\t", sum);
        for(sum = i = 0; i < AFSK24_CORRLEN; i++) {
		sum += (v = 127.0*cos(ARGHI(i))*WINDOW(i)); 
                fprintf(f, " %4i%c", v, (i < AFSK24_CORRLEN-1) ? ',' : ' ');
	}
        fprintf(f, "\n};\n#define SUM_AFSK24_TX_HI_I %d\n\n"
		"static const int afsk24_tx_hi_q[] = {\n\t", sum);
        for(sum = i = 0; i < AFSK24_CORRLEN; i++)  {
		sum += (v = 127.0*sin(ARGHI(i))*WINDOW(i));
                fprintf(f, " %4i%c", v, (i < AFSK24_CORRLEN-1) ? ',' : ' ');
	}
	fprintf(f, "\n};\n#define SUM_AFSK24_TX_HI_Q %d\n\n", sum);
#undef ARGLO
#undef ARGHI
#undef WINDOW
}

/* -------------------------------------------------------------------- */

static char *progname;

static void gentbl_banner(FILE *f)
{
	fprintf(f, "/*\n * THIS FILE IS GENERATED AUTOMATICALLY BY %s, "
		"DO NOT EDIT!\n */\n\n", progname);
}

/* -------------------------------------------------------------------- */

int main(int argc, char *argv[])
{
	FILE *f;
	
	progname = argv[0];
	if (!(f = fopen("sm_tbl_afsk1200.h", "w")))
		exit(1);
	gentbl_banner(f);
	gentbl_offscostab(f, 6);
	gentbl_costab(f, 6);
	gentbl_afsk1200(f);
	fclose(f);
	if (!(f = fopen("sm_tbl_afsk2666.h", "w")))
		exit(1);
	gentbl_banner(f);
	gentbl_offscostab(f, 6);
	gentbl_costab(f, 6);
	gentbl_afsk2666(f);
	fclose(f);
	if (!(f = fopen("sm_tbl_psk4800.h", "w")))
		exit(1);
	gentbl_banner(f);
	gentbl_psk4800(f);
	gentbl_costab(f, 8);
	gentbl_atantab(f);
	fclose(f);
	if (!(f = fopen("sm_tbl_hapn4800.h", "w")))
		exit(1);
	gentbl_banner(f);
	gentbl_hapn4800(f);
	fclose(f);
	if (!(f = fopen("sm_tbl_fsk9600.h", "w")))
		exit(1);
	gentbl_banner(f);
	gentbl_fsk9600(f);
	fclose(f);
	if (!(f = fopen("sm_tbl_afsk2400_8.h", "w")))
		exit(1);
	gentbl_banner(f);
	gentbl_offscostab(f, 6);
	gentbl_costab(f, 6);
	gentbl_afsk2400(f, 8000000);
	fclose(f);
	if (!(f = fopen("sm_tbl_afsk2400_7.h", "w")))
		exit(1);
	gentbl_banner(f);
	gentbl_offscostab(f, 6);
	gentbl_costab(f, 6);
	gentbl_afsk2400(f, 7372800);
	fclose(f);
	exit(0);
}


/* -------------------------------------------------------------------- */
