/* env.c: Replacement environment handling functions.

%%% portions-copyright-cmetz
Portions of this software are Copyright 1996 by Craig Metz, All Rights
Reserved. The Inner Net License Version 2 applies to these portions of
the software.
You should have received a copy of the license with this software. If
you didn't get a copy, you may request one from <license@inner.net>.

Portions of this software are Copyright 1995 by Randall Atkinson and Dan
McDonald, All Rights Reserved. All Rights under this copyright are assigned
to the U.S. Naval Research Laboratory (NRL). The NRL Copyright Notice and
License Agreement applies to this software.

	History:

        Modified by cmetz for OPIE 2.2. Changed ifdefs for libmissing.
	     Combined all env functions and made _findenv static.
             Including headers is a good idea, though. Add more headers.
	Modified at NRL for OPIE 2.0.
	Originally from BSD.
*/
/*
 * Copyright (c) 1987 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include "opie_cfg.h"
#include <stdio.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */
#include "opie.h"

static char *_findenv FUNCTION((name, offset), register char *name AND int *offset)
{
  extern char **environ;
  register int len;
  register char **P, *C;

  for (C = name, len = 0; *C && *C != '='; ++C, ++len);
  for (P = environ; *P; ++P)
    if (!strncmp(*P, name, len))
      if (*(C = *P + len) == '=') {
	*offset = P - environ;
	return (++C);
      }
  return (NULL);
}

#if !HAVE_GETENV
char *getenv FUNCTION((name), char *name)
{
  int offset;
  char *_findenv();

  return (_findenv(name, &offset));
}
#endif /* !HAVE_GETENV */

#if !HAVE_SETENV
int setenv FUNCTION((name, value, rewrite), char *name AND char *value AND int rewrite)
{
  extern char **environ;
  static int alloced;	/* if allocated space before */
  register char *C;
  int l_value, offset;

  if (*value == '=')	/* no `=' in value */
    ++value;
  l_value = strlen(value);
  if ((C = _findenv(name, &offset))) {	/* find if already exists */
    if (!rewrite)
      return (0);
    if (strlen(C) >= l_value) {	/* old larger; copy over */
      while (*C++ = *value++);
      return (0);
    }
  } else {	/* create new slot */
    register int cnt;
    register char **P;

    for (P = environ, cnt = 0; *P; ++P, ++cnt);
    if (alloced) {	/* just increase size */
      environ = (char **) realloc((char *) environ,
				  (u_int) (sizeof(char *) * (cnt + 2)));

      if (!environ)
	return (-1);
    } else {	/* get new space */
      alloced = 1;	/* copy old entries into it */
      P = (char **) malloc((u_int) (sizeof(char *) *
				    (cnt + 2)));

      if (!P)
	return (-1);
      strncpy(P, environ, cnt * sizeof(char *));

      environ = P;
    }
    environ[cnt + 1] = NULL;
    offset = cnt;
  }
  for (C = name; *C && *C != '='; ++C);	/* no `=' in name */
  if (!(environ[offset] =	/* name + `=' + value */
	malloc((u_int) ((int) (C - name) + l_value + 2))))
    return (-1);
  for (C = environ[offset]; (*C = *name++) && *C != '='; ++C);
  for (*C++ = '='; *C++ = *value++;);
  return (0);
}
#endif /* !HAVE_SETENV */

#if !HAVE_UNSETENV
VOIDRET unsetenv FUNCTION((name), char *name)
{
  extern char **environ;
  register char **P;
  int offset;

  while (_findenv(name, &offset))	/* if set multiple times */
    for (P = &environ[offset];; ++P)
      if (!(*P = *(P + 1)))
	break;
}
#endif /* !HAVE_UNSETENV */
