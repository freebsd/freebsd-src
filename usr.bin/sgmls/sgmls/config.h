/* unix.cfg: Configuration file for sgmls on Unix. */

/* A list of filename templates to use for searching for external entities.
The filenames are separated by the character specified in PATH_FILE_SEP.
See sgmls.man for details. */
#define DEFAULT_PATH "/usr/share/sgml/%O/%C/%T:%N.%X:%N.%D"
/* The character that separates the filenames templates. */
#define PATH_FILE_SEP ':'
/* The character that separates filenames in a system identifier.
Usually the same as PATH_FILE_SEP. */
#define SYSID_FILE_SEP ':'
/* The environment variable that contains the list of filename templates. */
#define PATH_ENV_VAR "SGML_PATH"
/* A macro that returns non-zero if the filename is relative to the
   current directory. */
#define FILE_IS_RELATIVE(p) ((p)[0] != '/')
/* A string containing the characters that can separate the directory
   part of a filename from the basename. */
#define DIR_BASE_SEP "/"
/* The environment variable that contains the list of catalog entry files.
   Filenames are separated by PATH_FILE_SEP. */
#define CATALOG_FILES_ENV_VAR "SGML_CATALOG_FILES"
/* Default list of catalog entry files. */
#define DEFAULT_CATALOG_FILES "CATALOG:/usr/share/sgml/CATALOG"

/* MIN_DAT_SUBS_FROM and MIN_DATS_SUBS_TO tell sgmls how to transform a name
or system identifier into a legal filename.  A character in
MIN_DAT_SUBS_FROM will be transformed into the character in the
corresponding position in MIN_DAT_SUBS_TO.  If there is no such
position, then the character is removed. */
/* This says that spaces should be transformed to underscores, and
slashes to percents. */
#define MIN_DAT_SUBS_FROM " /"
#define MIN_DAT_SUBS_TO   "_%"

/* Define this to allow tracing. */
/* #define TRACE 1 */

/* Define this you want support for subdocuments.  This is implemented
using features that are not part of Standard C, so you might not want
to define it if you are porting to a new system.  Otherwise I suggest
you leave it defined. */
#define SUPPORT_SUBDOC 1

/* Define HAVE_EXTENDED_PRINTF if your *printf functions supports
X/Open extensions; if they do, then, for example,

  printf("%2$s%1$s", "bar", "foo")

should print `foobar'.  */

/* #define HAVE_EXTENDED_PRINTF 1 */

/* Define HAVE_CAT if your system provides the X/Open message
catalogue functions catopen() and catgets(), and you want to use them.
An implementations of these functions is included and will be used if
you don't define this.  On SunOS 4.1.1, if you do define this you
should set CC=/usr/xpg2bin/cc in the makefile. */

#define HAVE_CAT 1

#ifdef __STDC__
/* Define this if your compiler supports prototypes. */
#define USE_PROTOTYPES 1
#endif

/* Can't use <stdarg.h> without prototypes. */
#ifndef USE_PROTOTYPES
#define VARARGS 1
#endif

/* If your compiler defines __STDC__ but doesn't provide <stdarg.h>,
you must define VARARGS yourself here. */
/* #define VARARGS 1 */

/* Define this if you do not have strerror(). */
/* #define STRERROR_MISSING 1 */

/* Define this unless the character testing functions in ctype.h
are defined for all values representable as an unsigned char.  You do
not need to define this if your system is ANSI C conformant.  You
should define for old Unix systems. */
/* #define USE_ISASCII 1 */

/* Define this if your system provides the BSD style string operations
rather than ANSI C ones (eg bcopy() rather than memcpy(), and index()
rather than strchr()). */
/* #define BSD_STRINGS 1 */

/* Define this if you have getopt(). */
#define HAVE_GETOPT 1

/* Define this if you have access(). */
#define HAVE_ACCESS 1

/* Define this if you have <unistd.h>. */
#define HAVE_UNISTD_H 1

/* Define this if you have <sys/stat.h>. */
#define HAVE_SYS_STAT_H 1

/* Define this if you have waitpid(). */
#define HAVE_WAITPID 1

/* Define this if your system is POSIX.1 (ISO 9945-1:1990) compliant. */
#define POSIX 1

/* Define this if you have the vfork() system call. */
#define HAVE_VFORK 1

/* Define this if you have <vfork.h>. */
/* #define HAVE_VFORK_H 1 */

/* Define this if you don't have <stdlib.h> */
/* #define STDLIB_H_MISSING 1 */

/* Define this if you don't have <stddef.h> */
/* #define STDDEF_H_MISSING 1 */

/* Define this if you don't have <limits.h> */
/* #define LIMITS_H_MISSING 1 */

/* Define this if you don't have remove(); unlink() will be used instead. */
/* #define REMOVE_MISSING 1 */

/* Define this if you don't have raise(); kill() will be used instead. */
/* #define RAISE_MISSING 1 */

/* Define this if you don't have fsetpos() and fgetpos(). */
/* #define FPOS_MISSING 1 */

/* Universal pointer type. */
/* If your compiler doesn't fully support void *, change `void' to `char'. */
typedef void *UNIV;

/* If your compiler doesn't support void as a function return type,
change `void' to `int'. */
typedef void VOID;

/* If you don't have an ANSI C conformant <limits.h>, define
CHAR_SIGNED as 1 or 0 according to whether the `char' type is signed.
The <limits.h> on some versions of System Release V 3.2 is not ANSI C
conformant: the value of CHAR_MIN is 0 even though the `char' type is
signed. */

/* #define CHAR_SIGNED 1 */
/* #define CHAR_SIGNED 0 */
#ifndef CHAR_SIGNED
#include <limits.h>
#if CHAR_MIN < 0
#define CHAR_SIGNED 1
#else
#define CHAR_SIGNED 0
#endif
#endif /* not CHAR_SIGNED */

/* Assume the system character set is ISO Latin-1. */
#include "latin1.h"
