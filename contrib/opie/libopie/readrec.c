/* readrec.c: The __opiereadrec() library function.

%%% copyright-cmetz-96
This software is Copyright 1996-1998 by Craig Metz, All Rights Reserved.
The Inner Net License Version 2 applies to this software.
You should have received a copy of the license with this software. If
you didn't get a copy, you may request one from <license@inner.net>.

	History:

	Modified by cmetz for OPIE 2.31. Removed active attack protection
		support. Fixed a debug message typo. Keep going after bogus
                records. Set read flag.
	Created by cmetz for OPIE 2.3.
*/
#include "opie_cfg.h"

#include <stdio.h>
#include <sys/types.h>
#include <errno.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */
#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif /* HAVE_FCNTL_H */
#include <ctype.h>
#include <errno.h>
#if DEBUG
#include <syslog.h>
#endif /* DEBUG */
#include "opie.h"

static int parserec FUNCTION((opie), struct opie *opie)
{
  char *c, *c2;

  if (!(c2 = strchr(opie->opie_principal = opie->opie_buf, ' ')))
    return -1;

  while(*c2 == ' ') c2++;
  *(c2 - 1) = 0;

  if (!(c2 = strchr(c = c2, ' ')))
    return -1;

  *(c2++) = 0;

  {
  char *c3;

  opie->opie_n = strtoul(c, &c3, 10);

  if (*c3)
    return -1;
  };

  if (!(c2 = strchr(opie->opie_seed = c2, ' ')))
    return -1;

  *(c2++) = 0;

  while(*c2 == ' ') c2++;

  if (!(c2 = strchr(opie->opie_val = c2, ' ')))
    return -1;

  *(c2++) = 0;

  return 0;
}

int __opiereadrec FUNCTION((opie), struct opie *opie)
{
  FILE *f = NULL;
  int rval = -1;

  if (!(f = __opieopen(KEY_FILE, 0, 0644))) {
#if DEBUG
    syslog(LOG_DEBUG, "__opiereadrec: __opieopen(KEY_FILE..) failed!");
#endif /* DEBUG */
    goto ret;
  }

  {
  int i;

  if ((i = open(KEY_FILE, O_RDWR)) < 0) {
    opie->opie_flags &= ~__OPIE_FLAGS_RW;
#if DEBUG
    syslog(LOG_DEBUG, "__opiereadrec: open(KEY_FILE, O_RDWR) failed: %s", strerror(errno));
#endif /* DEBUG */
  } else {
    close(i);
    opie->opie_flags |= __OPIE_FLAGS_RW;
  }
  }

  if (opie->opie_buf[0]) {
    if (fseek(f, opie->opie_recstart, SEEK_SET))
      goto ret;

    if (fgets(opie->opie_buf, sizeof(opie->opie_buf), f))
      goto ret;

    if (parserec(opie))
      goto ret;

    opie->opie_flags |= __OPIE_FLAGS_READ;
    rval = 0;
    goto ret;
  }

  if (!opie->opie_principal)
    goto ret;

  {
    char *c, principal[OPIE_PRINCIPAL_MAX];
    int i;
    
    if (c = strchr(opie->opie_principal, ':'))
      *c = 0;
    if (strlen(opie->opie_principal) > OPIE_PRINCIPAL_MAX)
      (opie->opie_principal)[OPIE_PRINCIPAL_MAX] = 0;
    
    strcpy(principal, opie->opie_principal);
    
    do {
      if ((opie->opie_recstart = ftell(f)) < 0)
	goto ret;

      if (!fgets(opie->opie_buf, sizeof(opie->opie_buf), f)) {
	rval = 1;
	goto ret;
      }

      if (parserec(opie))
	continue;
    } while (strcmp(principal, opie->opie_principal));

    rval = 0;
  }

ret:
  if (f)
    fclose(f);
  return rval;
}
