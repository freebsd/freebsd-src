#include "gmp.h"

main ()
{
printf ("/* gmp-mparam.h -- Compiler/machine parameter header file.\n\n");
printf ("    *** CREATED BY A PROGRAM -- DO NOT EDIT ***\n\n");
printf ("Copyright (C) 1996 Free Software Foundation, Inc.  */\n\n");

printf ("#define BITS_PER_MP_LIMB %d\n", 8 * sizeof (mp_limb_t));
printf ("#define BYTES_PER_MP_LIMB %d\n", sizeof (mp_limb_t));
printf ("#define BITS_PER_LONGINT %d\n", 8 * sizeof (long));
printf ("#define BITS_PER_INT %d\n", 8 * sizeof (int));
printf ("#define BITS_PER_SHORTINT %d\n", 8 * sizeof (short));
printf ("#define BITS_PER_CHAR 8\n");
exit (0);
}
