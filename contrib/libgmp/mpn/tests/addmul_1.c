#include <stdio.h>
#include "gmp.h"
#include "gmp-impl.h"
#include "longlong.h"

#ifndef USG
#include <sys/time.h>
#include <sys/resource.h>

unsigned long
cputime ()
{
    struct rusage rus;

    getrusage (0, &rus);
    return rus.ru_utime.tv_sec * 1000 + rus.ru_utime.tv_usec / 1000;
}
#else
#include <time.h>

#ifndef CLOCKS_PER_SEC
#define CLOCKS_PER_SEC 1000000
#endif

#if CLOCKS_PER_SEC >= 10000
#define CLOCK_TO_MILLISEC(cl) ((cl) / (CLOCKS_PER_SEC / 1000))
#else
#define CLOCK_TO_MILLISEC(cl) ((cl) * 1000 / CLOCKS_PER_SEC)
#endif

unsigned long
cputime ()
{
  return CLOCK_TO_MILLISEC (clock ());
}
#endif

#define M * 1000000

#ifndef CLOCK
#if defined (__m88k__)
#define CLOCK 20 M
#elif defined (__i386__)
#define CLOCK (16.666667 M)
#elif defined (__m68k__)
#define CLOCK (20 M)
#elif defined (_IBMR2)
#define CLOCK (25 M)
#elif defined (__sparc__)
#define CLOCK (20 M)
#elif defined (__sun__)
#define CLOCK (20 M)
#elif defined (__mips)
#define CLOCK (40 M)
#elif defined (__hppa__)
#define CLOCK (50 M)
#elif defined (__alpha)
#define CLOCK (133 M)
#else
#error "Don't know CLOCK of your machine"
#endif
#endif

#ifndef OPS
#define OPS 20000000
#endif
#ifndef SIZE
#define SIZE 496
#endif
#ifndef TIMES
#define TIMES OPS/SIZE
#else
#undef OPS
#define OPS (SIZE*TIMES)
#endif

mp_limb_t
refmpn_addmul_1 (res_ptr, s1_ptr, s1_size, s2_limb)
     register mp_ptr res_ptr;
     register mp_srcptr s1_ptr;
     mp_size_t s1_size;
     register mp_limb_t s2_limb;
{
  register mp_limb_t cy_limb;
  register mp_size_t j;
  register mp_limb_t prod_high, prod_low;
  register mp_limb_t x;

  /* The loop counter and index J goes from -SIZE to -1.  This way
     the loop becomes faster.  */
  j = -s1_size;

  /* Offset the base pointers to compensate for the negative indices.  */
  res_ptr -= j;
  s1_ptr -= j;

  cy_limb = 0;
  do
    {
      umul_ppmm (prod_high, prod_low, s1_ptr[j], s2_limb);

      prod_low += cy_limb;
      cy_limb = (prod_low < cy_limb) + prod_high;

      x = res_ptr[j];
      prod_low = x + prod_low;
      cy_limb += (prod_low < x);
      res_ptr[j] = prod_low;
    }
  while (++j != 0);

  return cy_limb;
}

main (argc, argv)
     int argc;
     char **argv;
{
  mp_limb_t s1[SIZE];
  mp_limb_t dx[SIZE+2];
  mp_limb_t dy[SIZE+2];
  mp_limb_t cyx, cyy;
  int i;
  long t0, t;
  int test;
  mp_limb_t xlimb;
  mp_size_t size;
  double cyc;

  for (test = 0; ; test++)
    {
#ifdef RANDOM
      size = (random () % SIZE + 1);
#else
      size = SIZE;
#endif

      mpn_random2 (s1, size);
      mpn_random2 (dy+1, size);

      if (random () % 0x100 == 0)
	xlimb = 0;
      else
	mpn_random2 (&xlimb, 1);

      dy[size+1] = 0x12345678;
      dy[0] = 0x87654321;

#if defined (PRINT) || defined (XPRINT)
      printf ("xlimb=%*lX\n", (int) (2 * sizeof(mp_limb_t)), xlimb);
#endif
#ifdef PRINT
      mpn_print (dy+1, size);
      mpn_print (s1, size);
#endif

      MPN_COPY (dx, dy, size+2);
      t0 = cputime();
      for (i = 0; i < TIMES; i++)
	cyx = refmpn_addmul_1 (dx+1, s1, size, xlimb);
      t = cputime() - t0;
#if TIMES != 1
      cyc = ((double) t * CLOCK) / (OPS * 1000.0);
      printf ("refmpn_addmul_1: %5ldms (%.2f cycles/limb) [%.2f Gb/s]\n",
	      t,
	      cyc,
	      CLOCK/cyc*BITS_PER_MP_LIMB*BITS_PER_MP_LIMB);
#endif

      MPN_COPY (dx, dy, size+2);
      t0 = cputime();
      for (i = 0; i < TIMES; i++)
	cyy = mpn_addmul_1 (dx+1, s1, size, xlimb);
      t = cputime() - t0;
#if TIMES != 1
      cyc = ((double) t * CLOCK) / (OPS * 1000.0);
      printf ("mpn_addmul_1:    %5ldms (%.2f cycles/limb) [%.2f Gb/s]\n",
	      t,
	      cyc,
	      CLOCK/cyc*BITS_PER_MP_LIMB*BITS_PER_MP_LIMB);
#endif

      MPN_COPY (dx, dy, size+2);
      cyx = refmpn_addmul_1 (dx+1, s1, size, xlimb);
      cyy = mpn_addmul_1 (dy+1, s1, size, xlimb);

#ifdef PRINT
      printf ("%*lX ", (int) (2 * sizeof(mp_limb_t)), cyx);
      mpn_print (dx+1, size);
      printf ("%*lX ", (int) (2 * sizeof(mp_limb_t)), cyy);
      mpn_print (dy+1, size);
#endif

#ifndef NOCHECK
      if (cyx != cyy || mpn_cmp (dx, dy, size+2) != 0
	  || dx[size+1] != 0x12345678 || dx[0] != 0x87654321)
	{
#ifndef PRINT
	  printf ("%*lX ", (int) (2 * sizeof(mp_limb_t)), cyx);
	  mpn_print (dx+1, size);
	  printf ("%*lX ", (int) (2 * sizeof(mp_limb_t)), cyy);
	  mpn_print (dy+1, size);
#endif
	  abort();
	}
#endif
    }
}

mpn_print (mp_ptr p, mp_size_t size)
{
  mp_size_t i;

  for (i = size - 1; i >= 0; i--)
    {
      printf ("%0*lX", (int) (2 * sizeof(mp_limb_t)), p[i]);
#ifdef SPACE
      if (i != 0)
	printf (" ");
#endif
    }
  puts ("");
}
