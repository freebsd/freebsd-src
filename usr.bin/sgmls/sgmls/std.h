/* std.h -
   Include standard header files.
*/

#ifndef STD_H
#define STD_H 1

#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>
#ifdef SUPPORT_SUBDOC
#include <signal.h>
#endif /* SUPPORT_SUBDOC */

#ifndef STDDEF_H_MISSING
#include <stddef.h>
#endif /* not STDDEF_H_MISSING */

#ifndef LIMITS_H_MISSING
#include <limits.h>
#endif /* not LIMITS_H_MISSING */

#ifndef UINT_MAX
#define UINT_MAX (sizeof(unsigned int) == 2 ? 0x7fff : \
  (sizeof(unsigned int) == 4 ? 0x7fffffff : cant_guess_UINT_MAX))
#endif

#ifdef VARARGS
#include <varargs.h>
#else
#include <stdarg.h>
#endif

#ifdef BSD_STRINGS
#include <strings.h>
#define memcpy(to, from, n) bcopy(from, to, n)
#define memcmp(p, q, n) bcmp(p, q, n)
#define strchr(s, c) index(s, c)
#define strrchr(s, c) rindex(s, c)
#else /* not BSD_STRINGS */
#include <string.h>
#endif /* not BSD_STRINGS */

extern char *strerror();

#ifdef STDLIB_H_MISSING
UNIV malloc();
UNIV calloc();
UNIV realloc();
char *getenv();
long atol();
#else /* not STDLIB_H_MISSING */
#include <stdlib.h>
#endif /* not STDLIB_H_MISSING */

#ifdef REMOVE_MISSING
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */
#define remove unlink
#endif /* REMOVE_MISSING */

#ifdef RAISE_MISSING
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */
#define raise(sig) kill(getpid(), sig)
#endif /* RAISE_MISSING */

#ifndef offsetof
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif

#ifndef EXIT_FAILURE
#define EXIT_FAILURE 1
#endif
#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS 0
#endif

#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif

#ifdef FPOS_MISSING
typedef long fpos_t;
#define fsetpos(stream, pos) fseek(stream, *(pos), SEEK_SET)
#define fgetpos(stream, pos) ((*(pos) = ftell(stream)) == -1L)
#endif /* FPOS_MISSING */

/* Old BSD systems lack L_tmpnam and tmpnam().  This is a partial
emulation using mktemp().  It requires that the argument to tmpnam()
be non-NULL. */

#ifndef L_tmpnam
#define tmpnam_template "/tmp/sgmlsXXXXXX"
#define L_tmpnam (sizeof(tmpnam_template))
#undef tmpnam
#define tmpnam(buf) \
  (mktemp(strcpy(buf, tmpnam_template)) == 0 || (buf)[0] == '\0' ? 0 : (buf))
#endif /* not L_tmpnam */

#ifndef errno
extern int errno;
#endif

#endif /* not STD_H */
