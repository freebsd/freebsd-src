/* strncasecmp.c: A replacement for the strncasecmp function

%%% copyright-cmetz
This software is Copyright 1996 by Craig Metz, All Rights Reserved.
The Inner Net License Version 2 applies to this software.
You should have received a copy of the license with this software. If
you didn't get a copy, you may request one from <license@inner.net>.

        History:

	Created by cmetz for OPIE 2.2.
*/
#include "opie_cfg.h"
#include "opie.h"

int strncasecmp FUNCTION((s1, s2, n), unsigned char *s1 AND unsigned char *s2 AND int n)
{
	unsigned char c1, c2;
	while(*s1 && *s2 && n--) {
		c1 = ((*s1 >= 'A') && (*s1 <= 'Z')) ? (*s1++) + ('a' - 'A') : (*s1++);
		c2 = ((*s2 >= 'A') && (*s2 <= 'Z')) ? (*s2++) + ('a' - 'A') : (*s2++);
		if (c1 != c2)
			return (c1 > c2) ? 1 : -1;
	}
	if (*s1 && !*s2)
		return 1;
	if (!*s1 && *s2)
		return -1;
	return 0;
}
