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
#define SIZE 100
#endif
#ifndef TIMES
#define TIMES OPS/SIZE
#else
#undef OPS
#define OPS (SIZE*TIMES)
#endif

main ()
{
  mp_limb_t nptr[2 * SIZE];
  mp_limb_t dptr[SIZE];
  mp_limb_t qptr[2 * SIZE];
  mp_limb_t pptr[2 * SIZE];
  mp_limb_t rptr[2 * SIZE];
  mp_size_t nsize, dsize, qsize, rsize, psize;
  int test;
  mp_limb_t qlimb;

  for (test = 0; ; test++)
    {
#ifdef RANDOM
      nsize = random () % (2 * SIZE) + 1;
      dsize = random () % nsize + 1;
#else
      nsize = 2 * SIZE;
      dsize = SIZE;
#endif

      mpn_random2 (nptr, nsize);
      mpn_random2 (dptr, dsize);
      dptr[dsize - 1] |= (mp_limb_t) 1 << (BITS_PER_MP_LIMB - 1);

      MPN_COPY (rptr, nptr, nsize);
      qlimb = mpn_divrem (qptr, (mp_size_t) 0, rptr, nsize, dptr, dsize);
      rsize = dsize;
      qsize = nsize - dsize;
      qptr[qsize] = qlimb;
      qsize += qlimb;
      if (qsize == 0 || qsize > 2 * SIZE)
	{
	  continue;		/* bogus */
	}
      else
	{
	  mp_limb_t cy;
	  if (qsize > dsize)
	    mpn_mul (pptr, qptr, qsize, dptr, dsize);
	  else
	    mpn_mul (pptr, dptr, dsize, qptr, qsize);
	  psize = qsize + dsize;
	  psize -= pptr[psize - 1] == 0;
	  cy = mpn_add (pptr, pptr, psize, rptr, rsize);
	  pptr[psize] = cy;
	  psize += cy;
	}

      if (nsize != psize || mpn_cmp (nptr, pptr, nsize) != 0)
	abort ();
    }
}
