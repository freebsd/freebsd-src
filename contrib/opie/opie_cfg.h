/* opie_cfg.h: Various configuration-type pieces of information for OPIE.

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

	Modified by cmetz for OPIE 2.3. Splatted with opie_auto.h.
	        Obseleted many symbols. Changed OPIE_PASS_{MIN,MAX} to
		OPIE_SECRET_{MIN,MAX}. Fixed SHADOW+UTMP definitions.
		Removed a lot of symbols.
        Modified by cmetz for OPIE 2.2. Got rid of ANSIPROTO and ARGS.
                Got rid of TRUE and FALSE definitions. Moved UINT4 to
                opie.h and removed UINT2.
	Modified at NRL for OPIE 2.1. Fixed sigprocmask declaration.
		Gutted for autoconf. Split up for autoconf.
	Written at NRL for OPIE 2.0.

	History of opie_auto.h:

	Modified by cmetz for OPIE 2.22. Support the Solaris TTYPROMPT drain
		bamage on all systems -- it doesn't hurt others, and it's
		not something Autoconf can check for yet.
        Modified by cmetz for OPIE 2.2. Don't replace sigprocmask by ifdef.
                Added configure check for LS_COMMAND. Added setreuid/setgid
                band-aids.
        Modified at NRL for OPIE 2.2. Require /etc/shadow for Linux to use
                shadow passwords.
        Modified at NRL for OPIE 2.11. Removed version defines.
	Modified at NRL for OPIE 2.1. Fixed sigprocmask declaration.
		Gutted for autoconf. Split up for autoconf.
	Written at NRL for OPIE 2.0.
*/

#define VERSION "2.3"
#define DATE    "Sunday, September 22, 1996"

#ifndef unix
#define unix 1
#endif /* unix */

#include "config.h"
#include "options.h"

/* System characteristics */

#if HAVE_GETUTXLINE && HAVE_UTMPX_H
#define DOUTMPX 1
#else /* HAVE_GETUTXLINE && HAVE_UTMPX_H */
#define DOUTMPX 0
#endif /* HAVE_GETUTXLINE && HAVE_UTMPX_H */

/* Adapted from the Autoconf hypertext info pages */
#if HAVE_DIRENT_H
#include <dirent.h>
#else /* HAVE_DIRENT_H */
#define dirent direct
#if HAVE_SYS_NDIR_H
#include <sys/ndir.h>
#endif /* HAVE_SYS_NDIR_H */
#if HAVE_SYS_DIR_H
#include <sys/dir.h>
#endif /* HAVE_SYS_DIR_H */
#if HAVE_NDIR_H
#include <ndir.h>
#endif /* HAVE_NDIR_H */
#endif /* HAVE_DIRENT_H */

#ifndef MAIL_DIR
#ifdef PATH_MAIL
#define MAIL_DIR PATH_MAIL
#else /* PATH_MAIL */
#ifdef _PATH_MAIL
#define MAIL_DIR _PATH_MAIL
#else /* _PATH_MAIL */
#ifdef _PATH_MAILDIR
#define MAIL_DIR _PATH_MAILDIR
#else /* _PATH_MAILDIR */
#define MAIL_DIR "/usr/spool/mail"
#endif /* _PATH_MAILDIR */
#endif /* _PATH_MAIL */
#endif /* PATH_MAIL */
#endif /* MAIL_DIR */

#if HAVE_SHADOW_H && HAVE_GETSPENT && HAVE_ENDSPENT
#if defined(linux) && !HAVE_ETC_SHADOW 
#define HAVE_SHADOW 0
#else /* defined(linux) && !HAVE_ETC_SHADOW */
#define HAVE_SHADOW 1
#endif /* defined(linux) && !HAVE_ETC_SHADOW */
#endif /* HAVE_SHADOW_H && HAVE_GETSPENT && HAVE_ENDSPENT */

#if !HAVE_SETEUID && HAVE_SETREUID
#define seteuid(x) setreuid(-1, x)
#endif /* !HAVE_SETEUID && HAVE_SETREUID */

#if !HAVE_SETEGID && HAVE_SETREGID
#define setegid(x) setregid(-1, x)
#endif /* !HAVE_SETEGID && HAVE_SETREGID */

/* If the user didn't specify, default to MD5 */
#ifndef MDX
#define MDX 5
#endif	/* MDX */

#ifndef _PATH_BSHELL
#define _PATH_BSHELL    "/bin/sh"
#endif

#ifndef _PATH_DEVNULL
#define _PATH_DEVNULL	  "/dev/null"
#endif

#ifndef _PATH_FTPUSERS
#define	_PATH_FTPUSERS	"/etc/ftpusers"
#endif

#ifndef TTYGRPNAME
#define TTYGRPNAME	"tty"	/* name of group to own ttys */
#endif

#ifndef NO_LOGINS_FILE
#define NO_LOGINS_FILE	"/etc/nologin"
#endif

#ifndef QUIET_LOGIN_FILE
#define QUIET_LOGIN_FILE  ".hushlogin"
#endif

#ifndef OPIE_ALWAYS_FILE
#define OPIE_ALWAYS_FILE ".opiealways"
#endif

#ifndef OPIE_LOCK_PREFIX
#define OPIE_LOCK_PREFIX "/tmp/opie-lock."
#endif

#ifndef OPIE_LOCK_TIMEOUT
#define OPIE_LOCK_TIMEOUT (30*60)
#endif

#ifndef MOTD_FILE
#define MOTD_FILE         "/etc/motd"
#endif

#ifndef NBBY
#define NBBY 8	/* Reasonable for modern systems */
#endif	/* NBBY */

#ifndef LOGIN_PATH
#define LOGIN_PATH "/usr/ucb:/bin:/usr/bin"
#endif	/* LOGIN_PATH */

#ifndef POINTER
#define POINTER unsigned char *
#endif /* POINTER */

#define _OPIE 1
