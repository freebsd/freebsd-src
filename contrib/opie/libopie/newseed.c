/* newseed.c: The opienewseed() library function.

%%% copyright-cmetz-96
This software is Copyright 1996-1997 by Craig Metz, All Rights Reserved.
The Inner Net License Version 2 applies to this software.
You should have received a copy of the license with this software. If
you didn't get a copy, you may request one from <license@inner.net>.

	History:

	Modified by cmetz for OPIE 2.31. Added time.h.
	Created by cmetz for OPIE 2.22.
*/

#include "opie_cfg.h"
#if HAVE_TIME_H
#include <time.h>
#endif /* HAVE_TIME_H */
#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */
#include <ctype.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */
#if HAVE_SYS_UTSNAME_H
#include <sys/utsname.h>
#endif /* HAVE_SYS_UTSNAME_H */
#include <errno.h>
#include "opie.h"

int opienewseed FUNCTION((seed), char *seed)
{
  if (!seed)
    return -1;

  if (seed[0]) {
    int i;
    
    if ((i = strlen(seed)) >= OPIE_SEED_MIN) {
      long j;
      char *c;
      
      if (i > OPIE_SEED_MAX)
	i = OPIE_SEED_MAX;

      c = seed + i - 1;

      while(c != seed) {
	if (!isdigit(*c))
	  break;
	c--;
      }

      c++;

      if (j = strtol(c, (char **)0, 10)) {
	char buf[OPIE_SEED_MAX];

	*c = 0;
	strcpy(buf, seed);

	if (errno == ERANGE) {
	  j = 1;
	} else {
	  int k = 1, l = OPIE_SEED_MAX - strlen(buf);
	  while(l--) k *= 10;

	  if (++j >= k)
	    j = 1;
	}

	sprintf(seed, "%s%04ld", buf, j);
	return 0;
      }
    }
  }

  {
    {
    time_t now;
    time(&now);
    srand(now);
    }

    {
    struct utsname utsname;

    if (uname(&utsname) < 0) {
#if 0
      perror("uname");
#endif /* 0 */
      utsname.nodename[0] = 'k';
      utsname.nodename[1] = 'e';
    }
    utsname.nodename[2] = 0;

    sprintf(seed, "%s%04d", utsname.nodename, (rand() % 9999) + 1);
    return 0;
    }
  }
}

