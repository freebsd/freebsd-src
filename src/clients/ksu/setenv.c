/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
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

/* based on @(#)setenv.c        5.2 (Berkeley) 6/27/88 */

#include "autoconf.h"
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *_findenv(char *, int *);

#ifndef HAVE_SETENV
extern int setenv(char *, char *, int);
#endif
#ifndef HAVE_UNSETENV
extern void unsetenv(char *);
#endif

/*
 * setenv --
 *      Set the value of the environmental variable "name" to be
 *      "value".  If rewrite is set, replace any current value.
 */
#ifndef HAVE_SETENV
int
setenv(name, value, rewrite)
    register char *name, *value;
    int rewrite;
{
    extern char **environ;
    static int alloced;                     /* if allocated space before */
    register char *C;
    int l_value, offset;

    if (*value == '=')                      /* no `=' in value */
        ++value;
    l_value = strlen(value);
    if ((C = _findenv(name, &offset))) {    /* find if already exists */
        if (!rewrite)
            return(0);
        if (strlen(C) >= l_value) {     /* old larger; copy over */
            while ((*C++ = *value++));
            return(0);
        }
    }
    else {                                  /* create new slot */
        register int    cnt;
        register char   **P;

        for (P = environ, cnt = 0; *P; ++P, ++cnt);
        if (alloced) {                  /* just increase size */
            environ = (char **)realloc((char *)environ,
                                       (u_int)(sizeof(char *) * (cnt + 2)));
            if (!environ)
                return(-1);
        }
        else {                          /* get new space */
            alloced = 1;            /* copy old entries into it */
            P = (char **)malloc((u_int)(sizeof(char *) *
                                        (cnt + 2)));
            if (!P)
                return(-1);
            memcpy(P, environ, cnt * sizeof(char *));
            environ = P;
        }
        environ[cnt + 1] = NULL;
        offset = cnt;
    }
    for (C = name; *C && *C != '='; ++C);   /* no `=' in name */
    if (!(environ[offset] =                 /* name + `=' + value */
          malloc((u_int)((int)(C - name) + l_value + 2))))
        return(-1);
    for (C = environ[offset]; (*C = *name++) &&( *C != '='); ++C);
    for (*C++ = '='; (*C++ = *value++) != NULL;);
    return(0);
}
#endif

/*
 * unsetenv(name) --
 *      Delete environmental variable "name".
 */
#ifndef HAVE_UNSETENV
void
unsetenv(name)
    char    *name;
{
    extern  char    **environ;
    register char   **P;
    int     offset;

    while (_findenv(name, &offset))         /* if set multiple times */
        for (P = &environ[offset];; ++P)
            if (!(*P = *(P + 1)))
                break;
}
#endif

/* based on @(#)getenv.c        5.5 (Berkeley) 6/27/88 */

/*
 * getenv --
 *      Returns ptr to value associated with name, if any, else NULL.
 */
#ifndef HAVE_GETENV
char *
getenv(name)
    char *name;
{
    int offset;

    return(_findenv(name, &offset));
}
#endif

/*
 * _findenv --
 *      Returns pointer to value associated with name, if any, else NULL.
 *      Sets offset to be the offset of the name/value combination in the
 *      environmental array, for use by setenv(3) and unsetenv(3).
 *      Explicitly removes '=' in argument name.
 *
 */
static char *
_findenv(name, offset)
    register char *name;
    int *offset;
{
    extern char **environ;
    register int len;
    register char **P, *C;

    for (C = name, len = 0; *C && *C != '='; ++C, ++len);
    for (P = environ; *P; ++P)
        if (!strncmp(*P, name, len))
            if (*(C = *P + len) == '=') {
                *offset = P - environ;
                return(++C);
            }
    return(NULL);
}
