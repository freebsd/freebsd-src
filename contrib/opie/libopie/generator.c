/* generator.c: The opiegenerator() library function.

%%% portions-copyright-cmetz-96
Portions of this software are Copyright 1996-1998 by Craig Metz, All Rights
Reserved. The Inner Net License Version 2 applies to these portions of
the software.
You should have received a copy of the license with this software. If
you didn't get a copy, you may request one from <license@inner.net>.

        History:

	Modified by cmetz for OPIE 2.32. If secret=NULL, always return
		as if opieauto returned "get the secret". Renamed
		_opieparsechallenge() to __opieparsechallenge(). Check
		challenge for extended response support and don't send
		an init-hex response if extended response support isn't
		indicated in the challenge.
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

$FreeBSD: src/contrib/opie/libopie/generator.c,v 1.3.6.1 2000/06/09 07:15:01 kris Exp $
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
  int exts;

  if (!(buffer = strstr(buffer, "otp-")))
    return 1;

  buffer += 4;

  if (__opieparsechallenge(buffer, &algorithm, &sequence, &seed, &exts))
    return 1;

  if ((sequence < 2) || (sequence > 9999))
    return 1;

  if (!secret[0])
   return 2;

  if (opiepasscheck(secret))
    return -2;

  if (i = opiekeycrunch(algorithm, key, seed, secret))
    return i;

  if (sequence < 10) {
    if (!(exts & 1))
      return 1;

    {
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
    };
  } else {
    while (sequence-- != 0)
      opiehash(key, algorithm);

    opiebtoh(response, key);
  }

  return 0;
}
