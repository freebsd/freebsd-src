/* readrec.c: The __opiereadrec() library function.

%%% copyright-cmetz
This software is Copyright 1996 by Craig Metz, All Rights Reserved.
The Inner Net License Version 2 applies to this software.
You should have received a copy of the license with this software. If
you didn't get a copy, you may request one from <license@inner.net>.

	History:

	Created by cmetz for OPIE 2.3.
*/
#include "opie_cfg.h"

#include <stdio.h>
#include <sys/types.h>
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

  if (!(opie->opie_n = atoi(c)))
    return -1;

  if (!(c2 = strchr(opie->opie_seed = c2, ' ')))
    return -1;

  *(c2++) = 0;

  while(*c2 == ' ') c2++;

  if (!(c2 = strchr(opie->opie_val = c2, ' ')))
    return -1;

  *(c2++) = 0;

  return 0;
}

static int parseextrec FUNCTION((opie), struct opie *opie)
{
  char *c;

  if (!(c = strchr(opie->opie_extbuf, ' ')))
    return -1;

  *(c++) = 0;
  while(*c == ' ') c++;

  if (!(c = strchr(opie->opie_reinitkey = c, ' ')))
    return -1;

  *(c++) = 0;

  return 0;
}

int __opiereadrec FUNCTION((opie), struct opie *opie)
{
  FILE *f = NULL, *f2 = NULL;
  int rval = -1;

  if (!(f = __opieopen(STD_KEY_FILE, 0, 0644))) {
#if DEBUG
    syslog(LOG_DEBUG, "__opiereadrec: __opieopen(STD_KEY_FILE..) failed!");
#endif /* DEBUG */
    goto ret;
  }

  if (!(f2 = __opieopen(EXT_KEY_FILE, 0, 0600))) {
#if DEBUG
    syslog(LOG_DEBUG, "__opiereadrec: __opieopen(EXT_KEY_FILE..) failed!");
#endif /* DEBUG */
  }

  {
  int i;

  if ((i = open(STD_KEY_FILE, O_RDWR)) < 0) {
    opie->opie_flags &= ~__OPIE_FLAGS_RW;
#if DEBUG
    syslog(LOG_DEBUG, "__opiereadrec: open(STD_KEY_FILE, O_RDWR) failed: %s", strerror(errno));
#endif /* DEBUG */
  } else {
    close(i);
    if ((i = open(EXT_KEY_FILE, O_RDWR)) < 0) {
      opie->opie_flags &= ~__OPIE_FLAGS_RW;
#if DEBUG
      syslog(LOG_DEBUG, "__opiereadrec: open(STD_KEY_FILE, O_RDWR) failed: %s", strerror(errno));
#endif /* DEBUG */
    } else {
      close(i);
      opie->opie_flags |= __OPIE_FLAGS_RW;
    }
  }
  }

  if (opie->opie_buf[0]) {
    if (fseek(f, opie->opie_recstart, SEEK_SET))
      goto ret;

    if (fgets(opie->opie_buf, sizeof(opie->opie_buf), f))
      goto ret;

    if (parserec(opie))
      goto ret;

    if (opie->opie_extbuf[0]) {
      if (!f2) {
#if DEBUG
	syslog(LOG_DEBUG, "__opiereadrec: can't read ext file, but could before?");
#endif /* DEBUG */
	goto ret;
      }

      if (fseek(f2, opie->opie_extrecstart, SEEK_SET))
	goto ret;
      
      if (fgets(opie->opie_extbuf, sizeof(opie->opie_extbuf), f2))
	goto ret;
	
      if (parseextrec(opie))
	goto ret;
    }

    rval = 0;
    goto ret;
  }

  if (!opie->opie_principal)
    return -1;
    
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
	goto ret;
    } while (strcmp(principal, opie->opie_principal));

    if (!f2) {
      opie->opie_extbuf[0] = rval = 0;
      goto ret;
    }

    do {
      if ((opie->opie_extrecstart = ftell(f2)) < 0)
	goto ret;
      
      if (!fgets(opie->opie_extbuf, sizeof(opie->opie_extbuf), f2)) {
	if (feof(f2)) {
	  opie->opie_reinitkey = NULL;
	  rval = 0;
	} else
	  rval = 1;
	goto ret;
      }
      
      if (parseextrec(opie))
	goto ret;
    } while (strcmp(principal, opie->opie_extbuf));

    rval = 0;
  }

ret:
  if (f)
    fclose(f);
  if (f2)
    fclose(f2);
  return rval;
}

