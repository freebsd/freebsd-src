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
#define SIZE 1000
#endif
#ifndef TIMES
#define TIMES OPS/SIZE
#else
#undef OPS
#define OPS (SIZE*TIMES)
#endif

main ()
{
  mp_limb_t nptr[SIZE];
  mp_limb_t qptr[SIZE];
  mp_limb_t pptr[SIZE];
  mp_limb_t dlimb, rlimb, plimb;
  mp_size_t nsize, qsize, psize;
  int test;

  for (test = 0; ; test++)
    {
#ifdef RANDOM
      nsize = random () % SIZE + 1;
#else
      nsize = SIZE;
#endif

      mpn_random2 (nptr, nsize);

      mpn_random2 (&dlimb, 1);
      if (dlimb == 0)
	abort ();

      rlimb = mpn_divmod_1 (qptr, nptr, nsize, dlimb);
      qsize = nsize - (qptr[nsize - 1] == 0);
      if (qsize == 0)
	{
	  plimb = rlimb;
	  psize = qsize;
	}
      else
	{
	  plimb = mpn_mul_1 (pptr, qptr, qsize, dlimb);
	  psize = qsize;
	  plimb += mpn_add_1 (pptr, pptr, psize, rlimb);
	}
      if (plimb != 0)
	pptr[psize++] = plimb;


      if (nsize != psize || mpn_cmp (nptr, pptr, nsize) != 0)
	abort ();
    }
}
