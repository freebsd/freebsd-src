/* strrch.c
   Look for the last occurrence of a character in a string.  This is
   supposed to work for a null byte, although we never actually call
   it with one.  */

#include "uucp.h"

char *
strrchr (z, b)
     const char *z;
     int b;
{
  char *zret;

  b = (char) b;
  zret = NULL;
  do
    {
      if (*z == b)
	zret = (char *) z;
    }
  while (*z++ != '\0');
  return zret;
}
