/* strcas.c
   Compare two strings case insensitively.  */

#include "uucp.h"
#include <ctype.h>

int
strcasecmp (z1, z2)
     const char *z1;
     const char *z2;
{
  char b1, b2;

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
    }
  if (*z2 == '\0')
    return 0;
  else
    return -1;
}
