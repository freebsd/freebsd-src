/* Test mpz_inp_binary and mpz_out_binary.

   We write and read back some test strings, and both compare
   the numerical result, and make sure the pattern on file is
   what we expect.  The latter is important for compatibility
   between machines with different word sizes.  */

#include <stdio.h>
#include "gmp.h"

FILE *file;

test (str, binary_len, binary_str)
     char *str;
     int binary_len;
     char *binary_str;
{
  mpz_t x, y;
  int n_written;
  char buf[100];

  mpz_init_set_str (x, str, 0);
  mpz_init (y);

  fseek (file, 0, SEEK_SET);
  mpz_out_binary (file, x);
  n_written = ftell (file);
  if (n_written != binary_len)
    abort ();

  fseek (file, 0, SEEK_SET);
  mpz_inp_binary (y, file);
  if (n_written != ftell (file))
    abort ();
  if (mpz_cmp (x, y) != 0)
    abort ();

  fseek (file, 0, SEEK_SET);
  fread (buf, n_written, 1, file);
  if (memcmp (buf, binary_str, binary_len) != 0)
    abort ();

  mpz_clear (x);
}

main ()
{
  file = fopen ("xtmpfile", "w+");

  test ("0", 4,
	"\000\000\000\000");

  test ("1", 5,
	"\000\000\000\001\001");
  test ("0x123", 6,
	"\000\000\000\002\001\043");
  test ("0xdeadbeef", 8,
	"\000\000\000\004\336\255\276\357");
  test ("0xbabefaced", 9,
	"\000\000\000\005\013\253\357\254\355");
  test ("0x123456789facade0", 12,
	"\000\000\000\010\022\064\126\170\237\254\255\340");

  test ("-1", 5,
	"\377\377\377\377\001");
  test ("-0x123", 6,
	"\377\377\377\376\001\043");
  test ("-0xdeadbeef", 8,
	"\377\377\377\374\336\255\276\357");
  test ("-0xbabefaced", 9,
	"\377\377\377\373\013\253\357\254\355");
  test ("-0x123456789facade0", 12,
	"\377\377\377\370\022\064\126\170\237\254\255\340");

  exit (0);
}
