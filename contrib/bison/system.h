#ifndef BISON_SYSTEM_H
#define BISON_SYSTEM_H

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef MSDOS
#include <io.h>
#endif

#ifdef _MSC_VER
#include <stdlib.h>
#include <process.h>
#define getpid _getpid
#endif

#if defined(HAVE_STDLIB_H) || defined(MSDOS)
#include <stdlib.h>
#endif

#if defined(HAVE_UNISTD_H)
#include <unistd.h>
#endif

#if (defined(VMS) || defined(MSDOS)) && !defined(HAVE_STRING_H)
#define HAVE_STRING_H 1
#endif

#if defined(STDC_HEADERS) || defined(HAVE_STRING_H)
#include <string.h>
/* An ANSI string.h and pre-ANSI memory.h might conflict.  */
#if !defined(STDC_HEADERS) && defined(HAVE_MEMORY_H)
#include <memory.h>
#endif /* not STDC_HEADERS and HAVE_MEMORY_H */
#ifndef bcopy
#define bcopy(src, dst, num) memcpy((dst), (src), (num))
#endif
#else /* not STDC_HEADERS and not HAVE_STRING_H */
#include <strings.h>
/* memory.h and strings.h conflict on some systems.  */
#endif /* not STDC_HEADERS and not HAVE_STRING_H */

#if defined(STDC_HEADERS) || defined(HAVE_CTYPE_H)
#include <ctype.h>
#endif

#ifdef HAVE_LOCALE_H
# include <locale.h>
#endif
#ifndef HAVE_SETLOCALE
# define setlocale(Category, Locale)
#endif

#ifdef ENABLE_NLS
# include <libintl.h>
# define _(Text) gettext (Text)
#else
# undef bindtextdomain
# define bindtextdomain(Domain, Directory)
# undef textdomain
# define textdomain(Domain)
# define _(Text) Text
#endif
#define N_(Text) Text

#ifndef LOCALEDIR
#define LOCALEDIR "/usr/local/share/locale"
#endif

#endif  /* BISON_SYSTEM_H */
