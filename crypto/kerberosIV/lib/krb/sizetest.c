#include "krb_locl.h"

RCSID("$Id: sizetest.c,v 1.5 1996/11/15 18:39:19 bg Exp $");

static
void
err(const char *msg)
{
  fputs(msg, stderr);
  exit(1);
}

int
main()
{
  if (sizeof(u_int8_t) < 1)
    err("sizeof(u_int8_t) is smaller than 1 byte\n");
  if (sizeof(u_int16_t) < 2)
    err("sizeof(u_int16_t) is smaller than 2 bytes\n");
  if (sizeof(u_int32_t) < 4)
    err("sizeof(u_int32_t) is smaller than 4 bytes\n");

  if (sizeof(u_int8_t) > 1)
    fputs("warning: sizeof(u_int8_t) is larger than 1 byte, "
	  "some stuff may not work properly!\n", stderr);

  {
    u_int8_t u = 1;
    int i;
    for (i = 0; u != 0 && i < 100; i++)
      u <<= 1;

    if (i < 8)
      err("u_int8_t is smaller than 8 bits\n");
    else if (i > 8)
      fputs("warning: u_int8_t is larger than 8 bits, "
	    "some stuff may not work properly!\n", stderr);
  }

  exit(0);
}
