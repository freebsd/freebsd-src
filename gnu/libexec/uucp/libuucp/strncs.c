/* strncs.c
   Compare two strings case insensitively up to a point.  */

#include "uucp.h"
#include <ctype.h>

int
strncasecmp (z1, z2, c)
     const char *z1;
     const char *z2;
     size_t c;
{
  char b1, b2;

  if (c == 0)
    return 0;
  while ((b1 = *z1++) != '\0')
    {
      b2 = *z2++;
      if (b2 == '\0')
	return 1;
      if (b1 != b2)
	{
	  if (isupper (BUCHAR (b1)))
	    b1 = tolower (BUCHAR (b1));
	  if (isupper (BUCHAR (b2)))
	    b2 = tolower (BUCHAR (b2));
	  if (b1 != b2)
	    return b1 - b2;
	}
      --c;
      if (c == 0)
	return 0;
    }
  if (*z2 == '\0')
    return 0;
  else
    return -1;
}
