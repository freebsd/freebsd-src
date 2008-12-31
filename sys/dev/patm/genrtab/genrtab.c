/*-
 * Copyright (c) 2003
 *	Fraunhofer Institute for Open Communication Systems (FhG Fokus).
 * 	All rights reserved.
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
 *
 * Author: Hartmut Brandt <harti@freebsd.org>
 *
 * This program is used to generate the different rate tables for the IDT77252
 * driver. The generated tables are slightly different from those in the
 * IDT manual.
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/dev/patm/genrtab/genrtab.c,v 1.2.18.1 2008/11/25 02:59:29 kensmith Exp $");

#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <ieeefp.h>

/* verbosity flag */
static int verbose;

/* number of table entries */
static const u_int tsize = 256;

/* number of rate difference tables to create */
static const u_int ndtables = 16;

/* cell rate offset for log 0 */
static const double offset = 10.0;

/*
 * Make an internal form of the interval and be sure to round down.
 */
static u_int
d2interval(double d)
{
	fp_rnd_t r;
	u_int s, id;

	r = fpsetround(FP_RZ);
	id = (u_int)rint(32 * d);
	fpsetround(r);

	s = 0;
	while (id >= 32 * 32) {
		s++;
		id >>= 1;
	}
	return ((s << 10) | (id));
}

/*
 * Convert an internal interval back to a real one.
 */
static double
interval2d(u_int id)
{
	return ((1 << ((id >> 10) & 0xf)) * ((id & 0x3ff) / 32.0));
}

/*
 * Convert double to ATM-Forum format. Make sure to round up.
 */
static u_int
cps2atmf(double cps)
{
	fp_rnd_t r;
	u_int s, id;

	if (cps < 1.0)
		return (0);

	s = 0;
	while (cps >= 2.0) {
		s++;
		cps /= 2;
	}
	r = fpsetround(FP_RP);
	id = (u_int)rint(512 * cps);
	fpsetround(r);

	return ((1 << 14) | (s << 9) | (id & 0x1ff));
}

/*
 * Convert ATM forum format to double
 */
static double
atmf2cps(u_int atmf)
{
	return (((atmf >> 14) & 1) * (1 << ((atmf >> 9) & 0x1f)) *
	    ((512 + (atmf & 0x1ff)) / 512.0));
}

/*
 * A cell rate to the logarithmic one
 */
static double
cps2log(u_int alink, double lg)
{
	if (lg <= offset)
		return (0);
	if (lg >= alink)
		return (tsize - 1);

	return ((tsize - 1) * (1 - log(alink / lg) / log(alink / offset)));
}

/*
 * Convert log to cell rate
 */
static double
log2cps(u_int alink, u_int lg)
{
	return (alink / pow(alink / offset,
	    (double)(tsize - lg - 1) / (tsize - 1)));
}

/*
 * Convert a double to an internal scaled double
 */
static u_int
d2ifp(double fp)
{
	fp_rnd_t r;
	u_int s, ifp;

	fp *= (1 << 16);

	r = fpsetround(FP_RN);
	ifp = (u_int)rint(fp);
	fpsetround(r);

	s = 0;
	while (ifp >= 1024) {
		s++;
		ifp >>= 1;
	}
	return ((s << 10) | (ifp));
}

/*
 * Convert internal scaled float to double
 */
static double
ifp2d(u_int p)
{
	return ((p & 0x3ff) * (1 << ((p >> 10) & 0xf)) / 65536.0);
}

/*
 * Generate log to rate conversion table
 */
static void
gen_log2rate(u_int alink)
{
	u_int i, iinterval, atmf, n, nrm;
	double rate, interval, xinterval, cps, xcps;

	for (i = 0; i < 256; i++) {
		/* get the desired rate */
		rate = alink / pow(alink / offset,
		    (double)(tsize - i - 1) / (tsize - 1));

		/* convert this to an interval */
		interval = alink / rate;

		/* make the internal form of this interval, be sure to
		 * round down */
		iinterval = d2interval(interval);

		/* now convert back */
		xinterval = interval2d(iinterval);

		/* make a cps from this interval */
		cps = alink / xinterval;

		/* convert this to its ATM forum format */
		atmf = cps2atmf(cps);

		/* and back */
		xcps = atmf2cps(atmf);

		/* decide on NRM */
		if (xcps < 40.0) {
			nrm = 0;
			n = 3;
		} else if (xcps < 80.0) {
			nrm = 1;
			n = 4;
		} else if (xcps < 160.0) {
			nrm = 2;
			n = 8;
		} else if (xcps < 320.0) {
			nrm = 3;
			n = 16;
		} else {
			nrm = 4;
			n = 32;
		}

		/* print */
		if (verbose)
			printf(" 0x%08x, /* %03u: cps=%f nrm=%u int=%f */\n",
			    (atmf << 17) | (nrm << 14) | iinterval, i,
			    xcps, n, xinterval);
		else
			printf("0x%08x,\n", (atmf << 17) | (nrm << 14) |
			    iinterval);
	}
}

/*
 * Generate rate to log conversion table
 */
static void
gen_rate2log(u_int alink)
{
	u_int i, atmf, val, ilcr;
	double cps, lcr;
	fp_rnd_t r;

	val = 0;
	for (i = 0; i < 512; i++) {
		/* make ATM Forum CPS from index */
		atmf = (((i & 0x1f0) >> 4) << 9) |
		    ((i & 0xf) << 5) | (1 << 14);

		/* make cps */
		cps = atmf2cps(atmf);

		/* convert to log */
		lcr = cps2log(alink, cps);

		r = fpsetround(FP_RN);
		ilcr = (u_int)rint(lcr);
		fpsetround(r);

		/* put together */
		val |= ilcr << (8 * (i % 4));

		/* print */
		if (i % 4 == 3) {
			if (verbose)
				printf(" 0x%08x,\t", val);
			else
				printf("0x%08x,\n", val);
			val = 0;
		} else if (verbose)
			printf("\t\t");
		if (verbose)
			printf("/* %03u: %f -> %f */\n", i,
			    cps, log2cps(alink, ilcr));
	}
}

/*
 * Generate one entry into the global table
 */
static void
gen_glob_entry(u_int alink, u_int fill, u_int ci, u_int ni)
{
	if (verbose)
		printf("  0x%08x,	/* %2u/32 %8.6f, %6u, ci=%u, ni=%u */\n",
		    cps2atmf(alink * fill / 32.0) | (ci << 17) | (ni << 16),
		    fill, fill / 32.0, alink * fill / 32, ci, ni);
	else
		printf("0x%08x,\n",
		    cps2atmf(alink * fill / 32.0) | (ci << 17) | (ni << 16));
}

/*
 * Generate global parameter table
 */
static void
gen_glob(u_int alink)
{
	u_int i;

	gen_glob_entry(alink, 32, 0, 0);
	gen_glob_entry(alink, 16, 0, 0);
	gen_glob_entry(alink,  8, 0, 1);
	gen_glob_entry(alink,  4, 0, 1);
	gen_glob_entry(alink,  2, 1, 1);
	gen_glob_entry(alink,  1, 1, 1);
	gen_glob_entry(alink,  0, 1, 1);
	gen_glob_entry(alink,  0, 1, 1);

	for (i = 0; i < tsize/2 - 8; i++) {
		if (i % 16 == 0)
			printf(" ");
		printf(" 0,");
		if (i % 16 == 15)
			printf("\n");
	}
	printf("\n");
}

/*
 * Generate additive rate increase tables
 */
static void
gen_air(u_int alink)
{
	u_int t, i;
	double diff;	/* cell rate to increase by */
	double cps;
	double add;
	u_int val, a;

	for (t = 0; t < ndtables; t++) {
		diff = (double)alink / (1 << t);
		printf("/* AIR %u: diff=%f */\n", t, diff);
		val = 0;
		for (i = 0; i < tsize; i++) {
			cps = log2cps(alink, i);
			cps += diff;
			if (cps > alink)
				cps = alink;

			add = cps2log(alink, cps) - i;

			a = d2ifp(add);

			if (i % 2) {
				val |= a << 16;
				if (verbose)
					printf("  0x%08x,\t", val);
				else
					printf("0x%08x,\n", val);
			} else {
				val = a;
				if (verbose)
					printf("\t\t");
			}
			if (verbose)
				printf("/* %3u: %f */\n", i, ifp2d(add));
		}
	}
}

/*
 * Generate rate decrease table
 */
static void
gen_rdf(u_int alink)
{
	double d;
	u_int t, i, f, val, diff;

	for (t = 0; t < ndtables; t++) {
		/* compute the log index difference */
		if (t == 0) {
			d = tsize - 1;
		} else {
			f = 1 << t;
			d = (tsize - 1) / log(alink / offset);
			d *= log((double)f / (f - 1));
		}
		printf(" /* RDF %u: 1/%u: %f */\n", t, 1 << t, d);
		val = 0;
		for (i = 0; i < tsize; i++) {
			if (i < d)
				diff = d2ifp(i);
			else
				diff = d2ifp(d);
			if (i % 2) {
				val |= diff << 16;
				if (verbose)
					printf("  0x%08x,\t", val);
				else
					printf("0x%08x,\n", val);
			} else {
				val = diff;
				if (verbose)
					printf("\t\t");
			}
			if (verbose)
				printf("/* %3u: %f */\n", i, ifp2d(diff));
		}
	}
}

/*
 * Create all the tables for a given link cell rate and link bit rate.
 * The link bit rate is only used to name the table.
 */
static void
gen_tables(u_int alink, u_int mbps)
{
	printf("\n");
	printf("/*\n");
	printf(" * Tables for %ucps and %uMbps\n", alink, mbps);
	printf(" */\n");
	printf("const uint32_t patm_rtables%u[128 * (4 + 2 * %u)] = {\n",
	    mbps, ndtables);

	gen_log2rate(alink);
	gen_rate2log(alink);
	gen_glob(alink);
	gen_air(alink);
	gen_rdf(alink);

	printf("};\n");
}

int
main(int argc, char *argv[])
{
	int opt;

	while ((opt = getopt(argc, argv, "v")) != -1)
		switch (opt) {

		  case 'v':
			verbose = 1;
			break;
		}

	printf("/*\n");
	printf(" * This file was generated by `%s'\n", argv[0]);
	printf(" */\n");
	printf("\n");
	printf("#include <sys/cdefs.h>\n");
	printf("__FBSDID(\"$FreeBSD: src/sys/dev/patm/genrtab/genrtab.c,v 1.2.18.1 2008/11/25 02:59:29 kensmith Exp $\");\n");
	printf("\n");
	printf("#include <sys/types.h>\n");
	printf("\n");
	printf("const u_int patm_rtables_size = 128 * (4 + 2 * %u);\n",
	    ndtables);
	printf("const u_int patm_rtables_ntab = %u;\n", ndtables);
	gen_tables(352768, 155);
	gen_tables( 59259,  25);
	return (0);
}
