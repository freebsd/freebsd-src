#include <stdio.h>
#include "gmp.h"
#include "gmp-impl.h"

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
#define OPS 10000000
#endif
#ifndef SIZE
#define SIZE 328
#endif
#ifndef TIMES
#define TIMES OPS/SIZE
#else
#undef OPS
#define OPS (SIZE*TIMES)
#endif


mp_limb_t
#if __STDC__
refmpn_sub_n (mp_ptr res_ptr,
	       mp_srcptr s1_ptr, mp_srcptr s2_ptr, mp_size_t size)
#else
refmpn_sub_n (res_ptr, s1_ptr, s2_ptr, size)
     register mp_ptr res_ptr;
     register mp_srcptr s1_ptr;
     register mp_srcptr s2_ptr;
     mp_size_t size;
#endif
{
  register mp_limb_t x, y, cy;
  register mp_size_t j;

  /* The loop counter and index J goes from -SIZE to -1.  This way
     the loop becomes faster.  */
  j = -size;

  /* Offset the base pointers to compensate for the negative indices.  */
  s1_ptr -= j;
  s2_ptr -= j;
  res_ptr -= j;

  cy = 0;
  do
    {
      y = s2_ptr[j];
      x = s1_ptr[j];
      y += cy;			/* add previous carry to subtrahend */
      cy = (y < cy);		/* get out carry from that addition */
      y = x - y;		/* main subtract */
      cy = (y > x) + cy;	/* get out carry from the subtract, combine */
      res_ptr[j] = y;
    }
  while (++j != 0);

  return cy;
}

main (argc, argv)
     int argc;
     char **argv;
{
  mp_limb_t s1[SIZE];
  mp_limb_t s2[SIZE];
  mp_limb_t dx[SIZE+1];
  mp_limb_t dy[SIZE+1];
  int cyx, cyy;
  int i;
  long t0, t;
  int test;
  mp_size_t size;

  for (test = 0; ; test++)
    {
#ifdef RANDOM
      size = (random () % SIZE + 1);
#else
      size = SIZE;
#endif

      mpn_random2 (s1, size);
      mpn_random2 (s2, size);

      dx[size] = 0x12345678;
      dy[size] = 0x12345678;

#ifdef PRINT
      mpn_print (s1, size);
      mpn_print (s2, size);
#endif
      t0 = cputime();
      for (i = 0; i < TIMES; i++)
	cyx = refmpn_sub_n (dx, s1, s2, size);
      t = cputime() - t0;
#if TIMES != 1
      printf ("refmpn_sub_n:   %ldms (%.2f cycles/limb)\n",
	      t,
	      ((double) t * CLOCK) / (OPS * 1000.0));
#endif
#ifdef PRINT
      printf ("%d ", cyx); mpn_print (dx, size);
#endif

      t0 = cputime();
      for (i = 0; i < TIMES; i++)
	cyx = mpn_sub_n (dx, s1, s2, size);
      t = cputime() - t0;
#if TIMES != 1
      printf ("mpn_sub_n:   %ldms (%.2f cycles/limb)\n",
	      t,
	      ((double) t * CLOCK) / (OPS * 1000.0));
#endif
#ifdef PRINT
      printf ("%d ", cyx); mpn_print (dx, size);
#endif

#ifndef NOCHECK
      /* Put garbage in the destination.  */
      for (i = 0; i < size; i++)
	{
	  dx[i] = 0x7654321;
	  dy[i] = 0x1234567;
	}

      cyx = refmpn_sub_n (dx, s1, s2, size);
      cyy = mpn_sub_n (dy, s1, s2, size);
      if (cyx != cyy || mpn_cmp (dx, dy, size) != 0
	  || dx[size] != 0x12345678 || dy[size] != 0x12345678)
	{
#ifndef PRINT
	  printf ("%d ", cyx); mpn_print (dx, size);
	  printf ("%d ", cyy); mpn_print (dy, size);
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
