/* generator.c: The opiegenerator() library function.

%%% portions-copyright-cmetz-96
Portions of this software are Copyright 1996-1997 by Craig Metz, All Rights
Reserved. The Inner Net License Version 2 applies to these portions of
the software.
You should have received a copy of the license with this software. If
you didn't get a copy, you may request one from <license@inner.net>.

        History:

	Modified by cmetz for OPIE 2.31. Renamed "init" to "init-hex".
	      Removed active attack protection support. Fixed fairly
              bug in how init response was computed (i.e., dead wrong).
	Modified by cmetz for OPIE 2.3. Use _opieparsechallenge(). ifdef
	      around string.h. Output hex responses by default, output
	      OTP re-init extended responses (same secret) if sequence
	      number falls below 10.
	Modified by cmetz for OPIE 2.2. Use FUNCTION declaration et al.
              Bug fixes.
	Created at NRL for OPIE 2.2.
*/

#include "opie_cfg.h"
#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */
#include "opie.h"

static char *algids[] = { NULL, NULL, NULL, "sha1", "md4", "md5" };

int opiegenerator FUNCTION((buffer, secret, response), char *buffer AND char *secret AND char *response)
{
  int algorithm;
  int sequence;
  char *seed;
  char key[8];
  int i;

  if (!(buffer = strstr(buffer, "otp-")))
    return 1;

  buffer += 4;

  if (_opieparsechallenge(buffer, &algorithm, &sequence, &seed))
    return 1;

  if ((sequence < 2) || (sequence > 9999))
    return 1;

  if (opiepasscheck(secret))
    return -2;

  if (i = opiekeycrunch(algorithm, key, seed, secret))
    return i;


  if (sequence < 10) {
    char newseed[OPIE_SEED_MAX + 1];
    char newkey[8];
    char *c;
    char buf[OPIE_SEED_MAX + 48 + 1];

    while (sequence-- != 0)
      opiehash(key, algorithm);

    if (opienewseed(strcpy(newseed, seed)) < 0)
      return -1;

    if (opiekeycrunch(algorithm, newkey, newseed, secret))
      return -1;

    for (i = 0; i < 499; i++)
      opiehash(newkey, algorithm);

    strcpy(response, "init-hex:");
    strcat(response, opiebtoh(buf, key));
    sprintf(buf, ":%s 499 %s:", algids[algorithm], newseed);
    strcat(response, buf);
    strcat(response, opiebtoh(buf, newkey));
  } else {
    while (sequence-- != 0)
      opiehash(key, algorithm);

    opiebtoh(response, key);
  }

  return 0;
}
