/* error.h -- declaration for error-reporting function
   Copyright (C) 1995 Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.  */

#ifndef ERROR_H
#define ERROR_H

/* Add prototype support.  Normally this is done in cvs.h, but that
   doesn't get included from lib/savecwd.c.  */
#ifndef PROTO
#if defined (USE_PROTOTYPES) ? USE_PROTOTYPES : defined (__STDC__)
#define PROTO(ARGS) ARGS
#else
#define PROTO(ARGS) ()
#endif
#endif

#ifndef __attribute__
/* This feature is available in gcc versions 2.5 and later.  */
# if __GNUC__ < 2 || (__GNUC__ == 2 && __GNUC_MINOR__ < 5) || __STRICT_ANSI__
#  define __attribute__(Spec) /* empty */
# endif
/* The __-protected variants of `format' and `printf' attributes
   are accepted by gcc versions 2.6.4 (effectively 2.7) and later.  */
# if __GNUC__ < 2 || (__GNUC__ == 2 && __GNUC_MINOR__ < 7)
#  define __format__ format
#  define __printf__ printf
# endif
#endif

#ifdef __STDC__
void error (int, int, const char *, ...) \
  __attribute__ ((__format__ (__printf__, 3, 4)));
#else
void error ();
#endif

/* Exit due to an error.  Similar to error (1, 0, "message"), but call
   it in the case where the message has already been printed.  */
extern void error_exit PROTO ((void));

/* If non-zero, error will use the CVS protocol to report error
   messages.  This will only be set in the CVS server parent process;
   most other code is run via do_cvs_command, which forks off a child
   process and packages up its stderr in the protocol.  */
extern int error_use_protocol;

#endif /* ERROR_H */
