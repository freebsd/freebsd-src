#include <stdio.h>
#include <stdlib.h>
#include "gmp.h"
#include "gmp-impl.h"

#define ADD 1
#define SUB 2

#ifndef METHOD
#define METHOD ADD
#endif

#if METHOD == ADD
#define REFCALL refmpn_add_n
#define TESTCALL mpn_add_n
#endif

#if METHOD == SUB
#define REFCALL refmpn_sub_n
#define TESTCALL mpn_sub_n
#endif

mp_limb_t refmpn_add_n ();
mp_limb_t refmpn_sub_n ();

#define SIZE 100

main (argc, argv)
     int argc;
     char **argv;
{
  mp_size_t alloc_size, max_size, size, i, cumul_size;
  mp_ptr s1, s2, dx, dy;
  int s1_align, s2_align, d_align;
  long pass, n_passes;
  mp_limb_t cx, cy;

  max_size = SIZE;
  n_passes = 1000000;

  argc--; argv++;
  if (argc)
    {
      max_size = atol (*argv);
      argc--; argv++;
    }

  alloc_size = max_size + 32;
  s1 = malloc (alloc_size * BYTES_PER_MP_LIMB);
  s2 = malloc (alloc_size * BYTES_PER_MP_LIMB);
  dx = malloc (alloc_size * BYTES_PER_MP_LIMB);
  dy = malloc (alloc_size * BYTES_PER_MP_LIMB);

  cumul_size = 0;
  for (pass = 0; pass < n_passes; pass++)
    {
      cumul_size += size;
      if (cumul_size >= 1000000)
	{
	  cumul_size -= 1000000;
	  printf ("%d ", pass); fflush (stdout);
	}
      s1_align = random () % 32;
      s2_align = random () % 32;
      d_align = random () % 32;

      size = random () % max_size + 1;

      mpn_random2 (s1 + s1_align, size);
      mpn_random2 (s2 + s2_align, size);

      for (i = 0; i < alloc_size; i++)
	dx[i] = dy[i] = i + 0x9876500;

      cx = TESTCALL (dx + d_align, s1 + s1_align, s2 + s2_align, size);
      cy = REFCALL (dy + d_align, s1 + s1_align, s2 + s2_align, size);

      if (cx != cy || mpn_cmp (dx, dy, alloc_size) != 0)
	abort ();
    }

  printf ("%d passes OK\n", n_passes);
  exit (0);
}

mp_limb_t
#if __STDC__
refmpn_add_n (mp_ptr res_ptr,
	      mp_srcptr s1_ptr, mp_srcptr s2_ptr, mp_size_t size)
#else
refmpn_add_n (res_ptr, s1_ptr, s2_ptr, size)
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
      y += cy;			/* add previous carry to one addend */
      cy = (y < cy);		/* get out carry from that addition */
      y = x + y;		/* add other addend */
      cy = (y < x) + cy;	/* get out carry from that add, combine */
      res_ptr[j] = y;
    }
  while (++j != 0);

  return cy;
}

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
