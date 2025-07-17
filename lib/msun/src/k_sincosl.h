/*-
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 * Copyright (c) 2008 Steven G. Kargl, David Schultz, Bruce D. Evans.
 *
 * Developed at SunSoft, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice 
 * is preserved.
 * ====================================================
 *
 * k_sinl.c and k_cosl.c merged by Steven G. Kargl
 */

#if LDBL_MANT_DIG == 64		/* ld80 version of k_sincosl.c. */

#if defined(__amd64__) || defined(__i386__)
/* Long double constants are slow on these arches, and broken on i386. */
static const volatile double
C1hi = 0.041666666666666664,		/*  0x15555555555555.0p-57 */
C1lo = 2.2598839032744733e-18,		/*  0x14d80000000000.0p-111 */
S1hi = -0.16666666666666666,		/* -0x15555555555555.0p-55 */
S1lo = -9.2563760475949941e-18;		/* -0x15580000000000.0p-109 */
#define	S1	((long double)S1hi + S1lo)
#define	C1	((long double)C1hi + C1lo)
#else
static const long double
C1 =  0.0416666666666666666136L,	/*  0xaaaaaaaaaaaaaa9b.0p-68 */
S1 = -0.166666666666666666671L;		/* -0xaaaaaaaaaaaaaaab.0p-66 */
#endif

static const double
C2 = -0.0013888888888888874,		/* -0x16c16c16c16c10.0p-62 */
C3 =  0.000024801587301571716,		/*  0x1a01a01a018e22.0p-68 */
C4 = -0.00000027557319215507120,	/* -0x127e4fb7602f22.0p-74 */
C5 =  0.0000000020876754400407278,	/*  0x11eed8caaeccf1.0p-81 */
C6 = -1.1470297442401303e-11,		/* -0x19393412bd1529.0p-89 */
C7 =  4.7383039476436467e-14,		/*  0x1aac9d9af5c43e.0p-97 */
S2 =  0.0083333333333333332,		/*  0x11111111111111.0p-59 */
S3 = -0.00019841269841269427,		/* -0x1a01a01a019f81.0p-65 */
S4 =  0.0000027557319223597490,		/*  0x171de3a55560f7.0p-71 */
S5 = -0.000000025052108218074604,	/* -0x1ae64564f16cad.0p-78 */
S6 =  1.6059006598854211e-10,		/*  0x161242b90243b5.0p-85 */
S7 = -7.6429779983024564e-13,		/* -0x1ae42ebd1b2e00.0p-93 */
S8 =  2.6174587166648325e-15;		/*  0x179372ea0b3f64.0p-101 */

static inline void
__kernel_sincosl(long double x, long double y, int iy, long double *sn,
    long double *cs)
{
	long double hz, r, v, w, z;

	z = x * x;
	v = z * x;
	/*
	 * XXX Replace Horner scheme with an algorithm suitable for CPUs
	 * with more complex pipelines.
	 */
	r = S2 + z * (S3 + z * (S4 + z * (S5 + z * (S6 + z * (S7 + z * S8)))));

	if (iy == 0)
		*sn = x + v * (S1 + z * r);
	else
		*sn = x - ((z * (y / 2 - v * r) - y) - v * S1);

	hz = z / 2;
	w = 1 - hz;
	r = z * (C1 + z * (C2 + z * (C3 + z * (C4 + z * (C5 + z * (C6 +
	    z * C7))))));
	*cs = w + (((1 - w) - hz) + (z * r - x * y));
}

#elif LDBL_MANT_DIG == 113	/* ld128 version of k_sincosl.c. */

static const long double
S1 = -0.16666666666666666666666666666666666606732416116558L,
S2 =  0.0083333333333333333333333333333331135404851288270047L,
S3 = -0.00019841269841269841269841269839935785325638310428717L,
S4 =  0.27557319223985890652557316053039946268333231205686e-5L,
S5 = -0.25052108385441718775048214826384312253862930064745e-7L,
S6 =  0.16059043836821614596571832194524392581082444805729e-9L,
S7 = -0.76471637318198151807063387954939213287488216303768e-12L,
S8 =  0.28114572543451292625024967174638477283187397621303e-14L;

static const double
S9  = -0.82206352458348947812512122163446202498005154296863e-17,
S10 =  0.19572940011906109418080609928334380560135358385256e-19,
S11 = -0.38680813379701966970673724299207480965452616911420e-22,
S12 =  0.64038150078671872796678569586315881020659912139412e-25;

static const long double
C1 =  4.16666666666666666666666666666666667e-02L,
C2 = -1.38888888888888888888888888888888834e-03L,
C3 =  2.48015873015873015873015873015446795e-05L,
C4 = -2.75573192239858906525573190949988493e-07L,
C5 =  2.08767569878680989792098886701451072e-09L,
C6 = -1.14707455977297247136657111139971865e-11L,
C7 =  4.77947733238738518870113294139830239e-14L,
C8 = -1.56192069685858079920640872925306403e-16L,
C9 =  4.11031762320473354032038893429515732e-19L,
C10= -8.89679121027589608738005163931958096e-22L,
C11=  1.61171797801314301767074036661901531e-24L,
C12= -2.46748624357670948912574279501044295e-27L;

static inline void
__kernel_sincosl(long double x, long double y, int iy, long double *sn, 
    long double *cs)
{
	long double hz, r, v, w, z;

	z = x * x;
	v = z * x;
	/*
	 * XXX Replace Horner scheme with an algorithm suitable for CPUs
	 * with more complex pipelines.
	 */
	r = S2 + z * (S3 + z * (S4 + z * (S5 + z * (S6 + z * (S7 + z * (S8 +
	    z * (S9 + z * (S10 + z * (S11 + z * S12)))))))));

	if (iy == 0)
		*sn = x + v * (S1 + z * r);
	else
		*sn = x - ((z * (y / 2 - v * r) - y) - v * S1);

	hz = z / 2;
	w = 1 - hz;
	r = z * (C1 + z * (C2 + z * (C3 + z * (C4 + z * (C5 + z * (C6 + 
	    z * (C7 + z * (C8 + z * (C9 + z * (C10 + z * (C11+z*C12)))))))))));

	*cs =  w + (((1 - w) - hz) + (z * r - x * y));
}
#else
#error "Unsupported long double format"
#endif
