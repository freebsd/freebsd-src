/*
 * Copyright (c) 2000 Andre Lucas.  All rights reserved.
 * Portions copyright (c) 1998 Todd C. Miller
 * Portions copyright (c) 1996 Jason Downs
 * Portions copyright (c) 1996 Theo de Raadt
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Markus Friedl.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 ** loginrec.c:  platform-independent login recording and lastlog retrieval
 **/

/*
  The new login code explained
  ============================

  This code attempts to provide a common interface to login recording
  (utmp and friends) and last login time retrieval.

  Its primary means of achieving this is to use 'struct logininfo', a
  union of all the useful fields in the various different types of
  system login record structures one finds on UNIX variants.

  We depend on autoconf to define which recording methods are to be
  used, and which fields are contained in the relevant data structures
  on the local system. Many C preprocessor symbols affect which code
  gets compiled here.

  The code is designed to make it easy to modify a particular
  recording method, without affecting other methods nor requiring so
  many nested conditional compilation blocks as were commonplace in
  the old code.

  For login recording, we try to use the local system's libraries as
  these are clearly most likely to work correctly. For utmp systems
  this usually means login() and logout() or setutent() etc., probably
  in libutil, along with logwtmp() etc. On these systems, we fall back
  to writing the files directly if we have to, though this method
  requires very thorough testing so we do not corrupt local auditing
  information. These files and their access methods are very system
  specific indeed.

  For utmpx systems, the corresponding library functions are
  setutxent() etc. To the author's knowledge, all utmpx systems have
  these library functions and so no direct write is attempted. If such
  a system exists and needs support, direct analogues of the [uw]tmp
  code should suffice.

  Retrieving the time of last login ('lastlog') is in some ways even
  more problemmatic than login recording. Some systems provide a
  simple table of all users which we seek based on uid and retrieve a
  relatively standard structure. Others record the same information in
  a directory with a separate file, and others don't record the
  information separately at all. For systems in the latter category,
  we look backwards in the wtmp or wtmpx file for the last login entry
  for our user. Naturally this is slower and on busy systems could
  incur a significant performance penalty.

  Calling the new code
  --------------------

  In OpenSSH all login recording and retrieval is performed in
  login.c. Here you'll find working examples. Also, in the logintest.c
  program there are more examples.

  Internal handler calling method
  -------------------------------

  When a call is made to login_login() or login_logout(), both
  routines set a struct logininfo flag defining which action (log in,
  or log out) is to be taken. They both then call login_write(), which
  calls whichever of the many structure-specific handlers autoconf
  selects for the local system.

  The handlers themselves handle system data structure specifics. Both
  struct utmp and struct utmpx have utility functions (see
  construct_utmp*()) to try to make it simpler to add extra systems
  that introduce new features to either structure.

  While it may seem terribly wasteful to replicate so much similar
  code for each method, experience has shown that maintaining code to
  write both struct utmp and utmpx in one function, whilst maintaining
  support for all systems whether they have library support or not, is
  a difficult and time-consuming task.

  Lastlog support proceeds similarly. Functions login_get_lastlog()
  (and its OpenSSH-tuned friend login_get_lastlog_time()) call
  getlast_entry(), which tries one of three methods to find the last
  login time. It uses local system lastlog support if it can,
  otherwise it tries wtmp or wtmpx before giving up and returning 0,
  meaning "tilt".

  Maintenance
  -----------

  In many cases it's possible to tweak autoconf to select the correct
  methods for a particular platform, either by improving the detection
  code (best), or by presetting DISABLE_<method> or CONF_<method>_FILE
  symbols for the platform.

  Use logintest to check which symbols are defined before modifying
  configure.ac and loginrec.c. (You have to build logintest yourself
  with 'make logintest' as it's not built by default.)

  Otherwise, patches to the specific method(s) are very helpful!

*/

/**
 ** TODO:
 **   homegrown ttyslot()
 **   test, test, test
 **
 ** Platform status:
 ** ----------------
 **
 ** Known good:
 **   Linux (Redhat 6.2, Debian)
 **   Solaris
 **   HP-UX 10.20 (gcc only)
 **   IRIX
 **   NeXT - M68k/HPPA/Sparc (4.2/3.3)
 **
 ** Testing required: Please send reports!
 **   NetBSD
 **   HP-UX 11
 **   AIX
 **
 ** Platforms with known problems:
 **   Some variants of Slackware Linux
 **
 **/

#include "includes.h"

#include "ssh.h"
#include "xmalloc.h"
#include "loginrec.h"
#include "log.h"
#include "atomicio.h"

RCSID("$Id: loginrec.c,v 1.40 2002/04/23 13:09:19 djm Exp $");
RCSID("$FreeBSD$");

#ifdef HAVE_UTIL_H
#  include <util.h>
#endif

#ifdef HAVE_LIBUTIL_H
#   include <libutil.h>
#endif

/**
 ** prototypes for helper functions in this file
 **/

#if HAVE_UTMP_H
void set_utmp_time(struct logininfo *li, struct utmp *ut);
void construct_utmp(struct logininfo *li, struct utmp *ut);
#endif

#ifdef HAVE_UTMPX_H
void set_utmpx_time(struct logininfo *li, struct utmpx *ut);
void construct_utmpx(struct logininfo *li, struct utmpx *ut);
#endif

int utmp_write_entry(struct logininfo *li);
int utmpx_write_entry(struct logininfo *li);
int wtmp_write_entry(struct logininfo *li);
int wtmpx_write_entry(struct logininfo *li);
int lastlog_write_entry(struct logininfo *li);
int syslogin_write_entry(struct logininfo *li);

int getlast_entry(struct logininfo *li);
int lastlog_get_entry(struct logininfo *li);
int wtmp_get_entry(struct logininfo *li);
int wtmpx_get_entry(struct logininfo *li);

/* pick the shortest string */
#define MIN_SIZEOF(s1,s2) ( sizeof(s1) < sizeof(s2) ? sizeof(s1) : sizeof(s2) )

/**
 ** platform-independent login functions
 **/

/* login_login(struct logininfo *)     -Record a login
 *
 * Call with a pointer to a struct logininfo initialised with
 * login_init_entry() or login_alloc_entry()
 *
 * Returns:
 *  >0 if successful
 *  0  on failure (will use OpenSSH's logging facilities for diagnostics)
 */
int
login_login (struct logininfo *li)
{
	li->type = LTYPE_LOGIN;
	return login_write(li);
}


/* login_logout(struct logininfo *)     - Record a logout
 *
 * Call as with login_login()
 *
 * Returns:
 *  >0 if successful
 *  0  on failure (will use OpenSSH's logging facilities for diagnostics)
 */
int
login_logout(struct logininfo *li)
{
	li->type = LTYPE_LOGOUT;
	return login_write(li);
}

/* login_get_lastlog_time(int)           - Retrieve the last login time
 *
 * Retrieve the last login time for the given uid. Will try to use the
 * system lastlog facilities if they are available, but will fall back
 * to looking in wtmp/wtmpx if necessary
 *
 * Returns:
 *   0 on failure, or if user has never logged in
 *   Time in seconds from the epoch if successful
 *
 * Useful preprocessor symbols:
 *   DISABLE_LASTLOG: If set, *never* even try to retrieve lastlog
 *                    info
 *   USE_LASTLOG: If set, indicates the presence of system lastlog
 *                facilities. If this and DISABLE_LASTLOG are not set,
 *                try to retrieve lastlog information from wtmp/wtmpx.
 */
unsigned int
login_get_lastlog_time(const int uid)
{
	struct logininfo li;

	if (login_get_lastlog(&li, uid))
		return li.tv_sec;
	else
		return 0;
}

/* login_get_lastlog(struct logininfo *, int)   - Retrieve a lastlog entry
 *
 * Retrieve a logininfo structure populated (only partially) with
 * information from the system lastlog data, or from wtmp/wtmpx if no
 * system lastlog information exists.
 *
 * Note this routine must be given a pre-allocated logininfo.
 *
 * Returns:
 *  >0: A pointer to your struct logininfo if successful
 *  0  on failure (will use OpenSSH's logging facilities for diagnostics)
 *
 */
struct logininfo *
login_get_lastlog(struct logininfo *li, const int uid)
{
	struct passwd *pw;

	memset(li, '\0', sizeof(*li));
	li->uid = uid;

	/*
	 * If we don't have a 'real' lastlog, we need the username to
	 * reliably search wtmp(x) for the last login (see
	 * wtmp_get_entry().)
	 */
	pw = getpwuid(uid);
	if (pw == NULL)
		fatal("login_get_lastlog: Cannot find account for uid %i", uid);

	/* No MIN_SIZEOF here - we absolutely *must not* truncate the
	 * username */
	strlcpy(li->username, pw->pw_name, sizeof(li->username));

	if (getlast_entry(li))
		return li;
	else
		return NULL;
}


/* login_alloc_entry(int, char*, char*, char*)    - Allocate and initialise
 *                                                  a logininfo structure
 *
 * This function creates a new struct logininfo, a data structure
 * meant to carry the information required to portably record login info.
 *
 * Returns a pointer to a newly created struct logininfo. If memory
 * allocation fails, the program halts.
 */
struct
logininfo *login_alloc_entry(int pid, const char *username,
			     const char *hostname, const char *line)
{
	struct logininfo *newli;

	newli = (struct logininfo *) xmalloc (sizeof(*newli));
	(void)login_init_entry(newli, pid, username, hostname, line);
	return newli;
}


/* login_free_entry(struct logininfo *)    - free struct memory */
void
login_free_entry(struct logininfo *li)
{
	xfree(li);
}


/* login_init_entry(struct logininfo *, int, char*, char*, char*)
 *                                        - initialise a struct logininfo
 *
 * Populates a new struct logininfo, a data structure meant to carry
 * the information required to portably record login info.
 *
 * Returns: 1
 */
int
login_init_entry(struct logininfo *li, int pid, const char *username,
		 const char *hostname, const char *line)
{
	struct passwd *pw;

	memset(li, 0, sizeof(*li));

	li->pid = pid;

	/* set the line information */
	if (line)
		line_fullname(li->line, line, sizeof(li->line));

	if (username) {
		strlcpy(li->username, username, sizeof(li->username));
		pw = getpwnam(li->username);
		if (pw == NULL)
			fatal("login_init_entry: Cannot find user \"%s\"", li->username);
		li->uid = pw->pw_uid;
	}

	if (hostname)
		strlcpy(li->hostname, hostname, sizeof(li->hostname));

	return 1;
}

/* login_set_current_time(struct logininfo *)    - set the current time
 *
 * Set the current time in a logininfo structure. This function is
 * meant to eliminate the need to deal with system dependencies for
 * time handling.
 */
void
login_set_current_time(struct logininfo *li)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);

	li->tv_sec = tv.tv_sec;
	li->tv_usec = tv.tv_usec;
}

/* copy a sockaddr_* into our logininfo */
void
login_set_addr(struct logininfo *li, const struct sockaddr *sa,
	       const unsigned int sa_size)
{
	unsigned int bufsize = sa_size;

	/* make sure we don't overrun our union */
	if (sizeof(li->hostaddr) < sa_size)
		bufsize = sizeof(li->hostaddr);

	memcpy((void *)&(li->hostaddr.sa), (const void *)sa, bufsize);
}


/**
 ** login_write: Call low-level recording functions based on autoconf
 ** results
 **/
int
login_write (struct logininfo *li)
{
#ifndef HAVE_CYGWIN
	if ((int)geteuid() != 0) {
	  log("Attempt to write login records by non-root user (aborting)");
	  return 1;
	}
#endif

	/* set the timestamp */
	login_set_current_time(li);
#ifdef USE_LOGIN
	syslogin_write_entry(li);
#endif
#ifdef USE_LASTLOG
	if (li->type == LTYPE_LOGIN) {
		lastlog_write_entry(li);
	}
#endif
#ifdef USE_UTMP
	utmp_write_entry(li);
#endif
#ifdef USE_WTMP
	wtmp_write_entry(li);
#endif
#ifdef USE_UTMPX
	utmpx_write_entry(li);
#endif
#ifdef USE_WTMPX
	wtmpx_write_entry(li);
#endif
	return 0;
}

#ifdef LOGIN_NEEDS_UTMPX
int
login_utmp_only(struct logininfo *li)
{
	li->type = LTYPE_LOGIN; 
	login_set_current_time(li);
# ifdef USE_UTMP
	utmp_write_entry(li);
# endif
# ifdef USE_WTMP
	wtmp_write_entry(li);
# endif
# ifdef USE_UTMPX
	utmpx_write_entry(li);
# endif
# ifdef USE_WTMPX
	wtmpx_write_entry(li);
# endif
	return 0;
}
#endif

/**
 ** getlast_entry: Call low-level functions to retrieve the last login
 **                time.
 **/

/* take the uid in li and return the last login time */
int
getlast_entry(struct logininfo *li)
{
#ifdef USE_LASTLOG
	return(lastlog_get_entry(li));
#else /* !USE_LASTLOG */

#ifdef DISABLE_LASTLOG
	/* On some systems we shouldn't even try to obtain last login
	 * time, e.g. AIX */
	return 0;
# else /* DISABLE_LASTLOG */
	/* Try to retrieve the last login time from wtmp */
#  if defined(USE_WTMP) && (defined(HAVE_TIME_IN_UTMP) || defined(HAVE_TV_IN_UTMP))
	/* retrieve last login time from utmp */
	return (wtmp_get_entry(li));
#  else /* defined(USE_WTMP) && (defined(HAVE_TIME_IN_UTMP) || defined(HAVE_TV_IN_UTMP)) */
	/* If wtmp isn't available, try wtmpx */
#   if defined(USE_WTMPX) && (defined(HAVE_TIME_IN_UTMPX) || defined(HAVE_TV_IN_UTMPX))
	/* retrieve last login time from utmpx */
	return (wtmpx_get_entry(li));
#   else
	/* Give up: No means of retrieving last login time */
	return 0;
#   endif /* USE_WTMPX && (HAVE_TIME_IN_UTMPX || HAVE_TV_IN_UTMPX) */
#  endif /* USE_WTMP && (HAVE_TIME_IN_UTMP || HAVE_TV_IN_UTMP) */
# endif /* DISABLE_LASTLOG */
#endif /* USE_LASTLOG */
}



/*
 * 'line' string utility functions
 *
 * These functions process the 'line' string into one of three forms:
 *
 * 1. The full filename (including '/dev')
 * 2. The stripped name (excluding '/dev')
 * 3. The abbreviated name (e.g. /dev/ttyp00 -> yp00
 *                               /dev/pts/1  -> ts/1 )
 *
 * Form 3 is used on some systems to identify a .tmp.? entry when
 * attempting to remove it. Typically both addition and removal is
 * performed by one application - say, sshd - so as long as the choice
 * uniquely identifies a terminal it's ok.
 */


/* line_fullname(): add the leading '/dev/' if it doesn't exist make
 * sure dst has enough space, if not just copy src (ugh) */
char *
line_fullname(char *dst, const char *src, int dstsize)
{
	memset(dst, '\0', dstsize);
	if ((strncmp(src, "/dev/", 5) == 0) || (dstsize < (strlen(src) + 5))) {
		strlcpy(dst, src, dstsize);
	} else {
		strlcpy(dst, "/dev/", dstsize);
		strlcat(dst, src, dstsize);
	}
	return dst;
}

/* line_stripname(): strip the leading '/dev' if it exists, return dst */
char *
line_stripname(char *dst, const char *src, int dstsize)
{
	memset(dst, '\0', dstsize);
	if (strncmp(src, "/dev/", 5) == 0)
		strlcpy(dst, src + 5, dstsize);
	else
		strlcpy(dst, src, dstsize);
	return dst;
}

/* line_abbrevname(): Return the abbreviated (usually four-character)
 * form of the line (Just use the last <dstsize> characters of the
 * full name.)
 *
 * NOTE: use strncpy because we do NOT necessarily want zero
 * termination */
char *
line_abbrevname(char *dst, const char *src, int dstsize)
{
	size_t len;

	memset(dst, '\0', dstsize);

	/* Always skip prefix if present */
	if (strncmp(src, "/dev/", 5) == 0)
		src += 5;

#ifdef WITH_ABBREV_NO_TTY
	if (strncmp(src, "tty", 3) == 0)
		src += 3;
#endif

	len = strlen(src);

	if (len > 0) {
		if (((int)len - dstsize) > 0)
			src +=  ((int)len - dstsize);

		/* note: _don't_ change this to strlcpy */
		strncpy(dst, src, (size_t)dstsize);
	}

	return dst;
}

/**
 ** utmp utility functions
 **
 ** These functions manipulate struct utmp, taking system differences
 ** into account.
 **/

#if defined(USE_UTMP) || defined (USE_WTMP) || defined (USE_LOGIN)

/* build the utmp structure */
void
set_utmp_time(struct logininfo *li, struct utmp *ut)
{
# ifdef HAVE_TV_IN_UTMP
	ut->ut_tv.tv_sec = li->tv_sec;
	ut->ut_tv.tv_usec = li->tv_usec;
# else
#  ifdef HAVE_TIME_IN_UTMP
	ut->ut_time = li->tv_sec;
#  endif
# endif
}

void
construct_utmp(struct logininfo *li,
		    struct utmp *ut)
{
	memset(ut, '\0', sizeof(*ut));

	/* First fill out fields used for both logins and logouts */

# ifdef HAVE_ID_IN_UTMP
	line_abbrevname(ut->ut_id, li->line, sizeof(ut->ut_id));
# endif

# ifdef HAVE_TYPE_IN_UTMP
	/* This is done here to keep utmp constants out of struct logininfo */
	switch (li->type) {
	case LTYPE_LOGIN:
		ut->ut_type = USER_PROCESS;
#ifdef _CRAY
		cray_set_tmpdir(ut);
#endif
		break;
	case LTYPE_LOGOUT:
		ut->ut_type = DEAD_PROCESS;
#ifdef _CRAY
		cray_retain_utmp(ut, li->pid);
#endif
		break;
	}
# endif
	set_utmp_time(li, ut);

	line_stripname(ut->ut_line, li->line, sizeof(ut->ut_line));

# ifdef HAVE_PID_IN_UTMP
	ut->ut_pid = li->pid;
# endif

	/* If we're logging out, leave all other fields blank */
	if (li->type == LTYPE_LOGOUT)
	  return;

	/*
	 * These fields are only used when logging in, and are blank
	 * for logouts.
	 */

	/* Use strncpy because we don't necessarily want null termination */
	strncpy(ut->ut_name, li->username, MIN_SIZEOF(ut->ut_name, li->username));
# ifdef HAVE_HOST_IN_UTMP
	realhostname_sa(ut->ut_host, sizeof ut->ut_host,
	    &li->hostaddr.sa, li->hostaddr.sa.sa_len);
# endif
# ifdef HAVE_ADDR_IN_UTMP
	/* this is just a 32-bit IP address */
	if (li->hostaddr.sa.sa_family == AF_INET)
		ut->ut_addr = li->hostaddr.sa_in.sin_addr.s_addr;
# endif
}
#endif /* USE_UTMP || USE_WTMP || USE_LOGIN */

/**
 ** utmpx utility functions
 **
 ** These functions manipulate struct utmpx, accounting for system
 ** variations.
 **/

#if defined(USE_UTMPX) || defined (USE_WTMPX)
/* build the utmpx structure */
void
set_utmpx_time(struct logininfo *li, struct utmpx *utx)
{
# ifdef HAVE_TV_IN_UTMPX
	utx->ut_tv.tv_sec = li->tv_sec;
	utx->ut_tv.tv_usec = li->tv_usec;
# else /* HAVE_TV_IN_UTMPX */
#  ifdef HAVE_TIME_IN_UTMPX
	utx->ut_time = li->tv_sec;
#  endif /* HAVE_TIME_IN_UTMPX */
# endif /* HAVE_TV_IN_UTMPX */
}

void
construct_utmpx(struct logininfo *li, struct utmpx *utx)
{
	memset(utx, '\0', sizeof(*utx));
# ifdef HAVE_ID_IN_UTMPX
	line_abbrevname(utx->ut_id, li->line, sizeof(utx->ut_id));
# endif

	/* this is done here to keep utmp constants out of loginrec.h */
	switch (li->type) {
	case LTYPE_LOGIN:
		utx->ut_type = USER_PROCESS;
		break;
	case LTYPE_LOGOUT:
		utx->ut_type = DEAD_PROCESS;
		break;
	}
	line_stripname(utx->ut_line, li->line, sizeof(utx->ut_line));
	set_utmpx_time(li, utx);
	utx->ut_pid = li->pid;
	/* strncpy(): Don't necessarily want null termination */
	strncpy(utx->ut_name, li->username, MIN_SIZEOF(utx->ut_name, li->username));

	if (li->type == LTYPE_LOGOUT)
		return;

	/*
	 * These fields are only used when logging in, and are blank
	 * for logouts.
	 */

# ifdef HAVE_HOST_IN_UTMPX
	strncpy(utx->ut_host, li->hostname, MIN_SIZEOF(utx->ut_host, li->hostname));
# endif
# ifdef HAVE_ADDR_IN_UTMPX
	/* this is just a 32-bit IP address */
	if (li->hostaddr.sa.sa_family == AF_INET)
		utx->ut_addr = li->hostaddr.sa_in.sin_addr.s_addr;
# endif
# ifdef HAVE_SYSLEN_IN_UTMPX
	/* ut_syslen is the length of the utx_host string */
	utx->ut_syslen = MIN(strlen(li->hostname), sizeof(utx->ut_host));
# endif
}
#endif /* USE_UTMPX || USE_WTMPX */

/**
 ** Low-level utmp functions
 **/

/* FIXME: (ATL) utmp_write_direct needs testing */
#ifdef USE_UTMP

/* if we can, use pututline() etc. */
# if !defined(DISABLE_PUTUTLINE) && defined(HAVE_SETUTENT) && \
	defined(HAVE_PUTUTLINE)
#  define UTMP_USE_LIBRARY
# endif


/* write a utmp entry with the system's help (pututline() and pals) */
# ifdef UTMP_USE_LIBRARY
static int
utmp_write_library(struct logininfo *li, struct utmp *ut)
{
	setutent();
	pututline(ut);

#  ifdef HAVE_ENDUTENT
	endutent();
#  endif
	return 1;
}
# else /* UTMP_USE_LIBRARY */

/* write a utmp entry direct to the file */
/* This is a slightly modification of code in OpenBSD's login.c */
static int
utmp_write_direct(struct logininfo *li, struct utmp *ut)
{
	struct utmp old_ut;
	register int fd;
	int tty;

	/* FIXME: (ATL) ttyslot() needs local implementation */

#if defined(HAVE_GETTTYENT)
	register struct ttyent *ty;

	tty=0;

	setttyent();
	while ((struct ttyent *)0 != (ty = getttyent())) {
		tty++;
		if (!strncmp(ty->ty_name, ut->ut_line, sizeof(ut->ut_line)))
			break;
	}
	endttyent();

	if((struct ttyent *)0 == ty) {
		log("utmp_write_entry: tty not found");
		return(1);
	}
#else /* FIXME */

	tty = ttyslot(); /* seems only to work for /dev/ttyp? style names */

#endif /* HAVE_GETTTYENT */

	if (tty > 0 && (fd = open(UTMP_FILE, O_RDWR|O_CREAT, 0644)) >= 0) {
		(void)lseek(fd, (off_t)(tty * sizeof(struct utmp)), SEEK_SET);
		/*
		 * Prevent luser from zero'ing out ut_host.
		 * If the new ut_line is empty but the old one is not
		 * and ut_line and ut_name match, preserve the old ut_line.
		 */
		if (atomicio(read, fd, &old_ut, sizeof(old_ut)) == sizeof(old_ut) &&
			(ut->ut_host[0] == '\0') && (old_ut.ut_host[0] != '\0') &&
			(strncmp(old_ut.ut_line, ut->ut_line, sizeof(ut->ut_line)) == 0) &&
			(strncmp(old_ut.ut_name, ut->ut_name, sizeof(ut->ut_name)) == 0)) {
			(void)memcpy(ut->ut_host, old_ut.ut_host, sizeof(ut->ut_host));
		}

		(void)lseek(fd, (off_t)(tty * sizeof(struct utmp)), SEEK_SET);
		if (atomicio(write, fd, ut, sizeof(*ut)) != sizeof(*ut))
			log("utmp_write_direct: error writing %s: %s",
			    UTMP_FILE, strerror(errno));

		(void)close(fd);
		return 1;
	} else {
		return 0;
	}
}
# endif /* UTMP_USE_LIBRARY */

static int
utmp_perform_login(struct logininfo *li)
{
	struct utmp ut;

	construct_utmp(li, &ut);
# ifdef UTMP_USE_LIBRARY
	if (!utmp_write_library(li, &ut)) {
		log("utmp_perform_login: utmp_write_library() failed");
		return 0;
	}
# else
	if (!utmp_write_direct(li, &ut)) {
		log("utmp_perform_login: utmp_write_direct() failed");
		return 0;
	}
# endif
	return 1;
}


static int
utmp_perform_logout(struct logininfo *li)
{
	struct utmp ut;

	construct_utmp(li, &ut);
# ifdef UTMP_USE_LIBRARY
	if (!utmp_write_library(li, &ut)) {
		log("utmp_perform_logout: utmp_write_library() failed");
		return 0;
	}
# else
	if (!utmp_write_direct(li, &ut)) {
		log("utmp_perform_logout: utmp_write_direct() failed");
		return 0;
	}
# endif
	return 1;
}


int
utmp_write_entry(struct logininfo *li)
{
	switch(li->type) {
	case LTYPE_LOGIN:
		return utmp_perform_login(li);

	case LTYPE_LOGOUT:
		return utmp_perform_logout(li);

	default:
		log("utmp_write_entry: invalid type field");
		return 0;
	}
}
#endif /* USE_UTMP */


/**
 ** Low-level utmpx functions
 **/

/* not much point if we don't want utmpx entries */
#ifdef USE_UTMPX

/* if we have the wherewithall, use pututxline etc. */
# if !defined(DISABLE_PUTUTXLINE) && defined(HAVE_SETUTXENT) && \
	defined(HAVE_PUTUTXLINE)
#  define UTMPX_USE_LIBRARY
# endif


/* write a utmpx entry with the system's help (pututxline() and pals) */
# ifdef UTMPX_USE_LIBRARY
static int
utmpx_write_library(struct logininfo *li, struct utmpx *utx)
{
	setutxent();
	pututxline(utx);

#  ifdef HAVE_ENDUTXENT
	endutxent();
#  endif
	return 1;
}

# else /* UTMPX_USE_LIBRARY */

/* write a utmp entry direct to the file */
static int
utmpx_write_direct(struct logininfo *li, struct utmpx *utx)
{
	log("utmpx_write_direct: not implemented!");
	return 0;
}
# endif /* UTMPX_USE_LIBRARY */

static int
utmpx_perform_login(struct logininfo *li)
{
	struct utmpx utx;

	construct_utmpx(li, &utx);
# ifdef UTMPX_USE_LIBRARY
	if (!utmpx_write_library(li, &utx)) {
		log("utmpx_perform_login: utmp_write_library() failed");
		return 0;
	}
# else
	if (!utmpx_write_direct(li, &ut)) {
		log("utmpx_perform_login: utmp_write_direct() failed");
		return 0;
	}
# endif
	return 1;
}


static int
utmpx_perform_logout(struct logininfo *li)
{
	struct utmpx utx;

	construct_utmpx(li, &utx);
# ifdef HAVE_ID_IN_UTMPX
	line_abbrevname(utx.ut_id, li->line, sizeof(utx.ut_id));
# endif
# ifdef HAVE_TYPE_IN_UTMPX
	utx.ut_type = DEAD_PROCESS;
# endif

# ifdef UTMPX_USE_LIBRARY
	utmpx_write_library(li, &utx);
# else
	utmpx_write_direct(li, &utx);
# endif
	return 1;
}

int
utmpx_write_entry(struct logininfo *li)
{
	switch(li->type) {
	case LTYPE_LOGIN:
		return utmpx_perform_login(li);
	case LTYPE_LOGOUT:
		return utmpx_perform_logout(li);
	default:
		log("utmpx_write_entry: invalid type field");
		return 0;
	}
}
#endif /* USE_UTMPX */


/**
 ** Low-level wtmp functions
 **/

#ifdef USE_WTMP

/* write a wtmp entry direct to the end of the file */
/* This is a slight modification of code in OpenBSD's logwtmp.c */
static int
wtmp_write(struct logininfo *li, struct utmp *ut)
{
	struct stat buf;
	int fd, ret = 1;

	if ((fd = open(WTMP_FILE, O_WRONLY|O_APPEND, 0)) < 0) {
		log("wtmp_write: problem writing %s: %s",
		    WTMP_FILE, strerror(errno));
		return 0;
	}
	if (fstat(fd, &buf) == 0)
		if (atomicio(write, fd, ut, sizeof(*ut)) != sizeof(*ut)) {
			ftruncate(fd, buf.st_size);
			log("wtmp_write: problem writing %s: %s",
			    WTMP_FILE, strerror(errno));
			ret = 0;
		}
	(void)close(fd);
	return ret;
}

static int
wtmp_perform_login(struct logininfo *li)
{
	struct utmp ut;

	construct_utmp(li, &ut);
	return wtmp_write(li, &ut);
}


static int
wtmp_perform_logout(struct logininfo *li)
{
	struct utmp ut;

	construct_utmp(li, &ut);
	return wtmp_write(li, &ut);
}


int
wtmp_write_entry(struct logininfo *li)
{
	switch(li->type) {
	case LTYPE_LOGIN:
		return wtmp_perform_login(li);
	case LTYPE_LOGOUT:
		return wtmp_perform_logout(li);
	default:
		log("wtmp_write_entry: invalid type field");
		return 0;
	}
}


/* Notes on fetching login data from wtmp/wtmpx
 *
 * Logouts are usually recorded with (amongst other things) a blank
 * username on a given tty line.  However, some systems (HP-UX is one)
 * leave all fields set, but change the ut_type field to DEAD_PROCESS.
 *
 * Since we're only looking for logins here, we know that the username
 * must be set correctly. On systems that leave it in, we check for
 * ut_type==USER_PROCESS (indicating a login.)
 *
 * Portability: Some systems may set something other than USER_PROCESS
 * to indicate a login process. I don't know of any as I write. Also,
 * it's possible that some systems may both leave the username in
 * place and not have ut_type.
 */

/* return true if this wtmp entry indicates a login */
static int
wtmp_islogin(struct logininfo *li, struct utmp *ut)
{
	if (strncmp(li->username, ut->ut_name,
		MIN_SIZEOF(li->username, ut->ut_name)) == 0) {
# ifdef HAVE_TYPE_IN_UTMP
		if (ut->ut_type & USER_PROCESS)
			return 1;
# else
		return 1;
# endif
	}
	return 0;
}

int
wtmp_get_entry(struct logininfo *li)
{
	struct stat st;
	struct utmp ut;
	int fd, found=0;

	/* Clear the time entries in our logininfo */
	li->tv_sec = li->tv_usec = 0;

	if ((fd = open(WTMP_FILE, O_RDONLY)) < 0) {
		log("wtmp_get_entry: problem opening %s: %s",
		    WTMP_FILE, strerror(errno));
		return 0;
	}
	if (fstat(fd, &st) != 0) {
		log("wtmp_get_entry: couldn't stat %s: %s",
		    WTMP_FILE, strerror(errno));
		close(fd);
		return 0;
	}

	/* Seek to the start of the last struct utmp */
	if (lseek(fd, -(off_t)sizeof(struct utmp), SEEK_END) == -1) {
		/* Looks like we've got a fresh wtmp file */
		close(fd);
		return 0;
	}

	while (!found) {
		if (atomicio(read, fd, &ut, sizeof(ut)) != sizeof(ut)) {
			log("wtmp_get_entry: read of %s failed: %s",
			    WTMP_FILE, strerror(errno));
			close (fd);
			return 0;
		}
		if ( wtmp_islogin(li, &ut) ) {
			found = 1;
			/* We've already checked for a time in struct
			 * utmp, in login_getlast(). */
# ifdef HAVE_TIME_IN_UTMP
			li->tv_sec = ut.ut_time;
# else
#  if HAVE_TV_IN_UTMP
			li->tv_sec = ut.ut_tv.tv_sec;
#  endif
# endif
			line_fullname(li->line, ut.ut_line,
				      MIN_SIZEOF(li->line, ut.ut_line));
# ifdef HAVE_HOST_IN_UTMP
			strlcpy(li->hostname, ut.ut_host,
				MIN_SIZEOF(li->hostname, ut.ut_host));
# endif
			continue;
		}
		/* Seek back 2 x struct utmp */
		if (lseek(fd, -(off_t)(2 * sizeof(struct utmp)), SEEK_CUR) == -1) {
			/* We've found the start of the file, so quit */
			close (fd);
			return 0;
		}
	}

	/* We found an entry. Tidy up and return */
	close(fd);
	return 1;
}
# endif /* USE_WTMP */


/**
 ** Low-level wtmpx functions
 **/

#ifdef USE_WTMPX
/* write a wtmpx entry direct to the end of the file */
/* This is a slight modification of code in OpenBSD's logwtmp.c */
static int
wtmpx_write(struct logininfo *li, struct utmpx *utx)
{
	struct stat buf;
	int fd, ret = 1;

	if ((fd = open(WTMPX_FILE, O_WRONLY|O_APPEND, 0)) < 0) {
		log("wtmpx_write: problem opening %s: %s",
		    WTMPX_FILE, strerror(errno));
		return 0;
	}

	if (fstat(fd, &buf) == 0)
		if (atomicio(write, fd, utx, sizeof(*utx)) != sizeof(*utx)) {
			ftruncate(fd, buf.st_size);
			log("wtmpx_write: problem writing %s: %s",
			    WTMPX_FILE, strerror(errno));
			ret = 0;
		}
	(void)close(fd);

	return ret;
}


static int
wtmpx_perform_login(struct logininfo *li)
{
	struct utmpx utx;

	construct_utmpx(li, &utx);
	return wtmpx_write(li, &utx);
}


static int
wtmpx_perform_logout(struct logininfo *li)
{
	struct utmpx utx;

	construct_utmpx(li, &utx);
	return wtmpx_write(li, &utx);
}


int
wtmpx_write_entry(struct logininfo *li)
{
	switch(li->type) {
	case LTYPE_LOGIN:
		return wtmpx_perform_login(li);
	case LTYPE_LOGOUT:
		return wtmpx_perform_logout(li);
	default:
		log("wtmpx_write_entry: invalid type field");
		return 0;
	}
}

/* Please see the notes above wtmp_islogin() for information about the
   next two functions */

/* Return true if this wtmpx entry indicates a login */
static int
wtmpx_islogin(struct logininfo *li, struct utmpx *utx)
{
	if ( strncmp(li->username, utx->ut_name,
		MIN_SIZEOF(li->username, utx->ut_name)) == 0 ) {
# ifdef HAVE_TYPE_IN_UTMPX
		if (utx->ut_type == USER_PROCESS)
			return 1;
# else
		return 1;
# endif
	}
	return 0;
}


int
wtmpx_get_entry(struct logininfo *li)
{
	struct stat st;
	struct utmpx utx;
	int fd, found=0;

	/* Clear the time entries */
	li->tv_sec = li->tv_usec = 0;

	if ((fd = open(WTMPX_FILE, O_RDONLY)) < 0) {
		log("wtmpx_get_entry: problem opening %s: %s",
		    WTMPX_FILE, strerror(errno));
		return 0;
	}
	if (fstat(fd, &st) != 0) {
		log("wtmpx_get_entry: couldn't stat %s: %s",
		    WTMP_FILE, strerror(errno));
		close(fd);
		return 0;
	}

	/* Seek to the start of the last struct utmpx */
	if (lseek(fd, -(off_t)sizeof(struct utmpx), SEEK_END) == -1 ) {
		/* probably a newly rotated wtmpx file */
		close(fd);
		return 0;
	}

	while (!found) {
		if (atomicio(read, fd, &utx, sizeof(utx)) != sizeof(utx)) {
			log("wtmpx_get_entry: read of %s failed: %s",
			    WTMPX_FILE, strerror(errno));
			close (fd);
			return 0;
		}
		/* Logouts are recorded as a blank username on a particular line.
		 * So, we just need to find the username in struct utmpx */
		if ( wtmpx_islogin(li, &utx) ) {
# ifdef HAVE_TV_IN_UTMPX
			li->tv_sec = utx.ut_tv.tv_sec;
# else
#  ifdef HAVE_TIME_IN_UTMPX
			li->tv_sec = utx.ut_time;
#  endif
# endif
			line_fullname(li->line, utx.ut_line, sizeof(li->line));
# ifdef HAVE_HOST_IN_UTMPX
			strlcpy(li->hostname, utx.ut_host,
				MIN_SIZEOF(li->hostname, utx.ut_host));
# endif
			continue;
		}
		if (lseek(fd, -(off_t)(2 * sizeof(struct utmpx)), SEEK_CUR) == -1) {
			close (fd);
			return 0;
		}
	}

	close(fd);
	return 1;
}
#endif /* USE_WTMPX */

/**
 ** Low-level libutil login() functions
 **/

#ifdef USE_LOGIN
static int
syslogin_perform_login(struct logininfo *li)
{
	struct utmp *ut;

	if (! (ut = (struct utmp *)malloc(sizeof(*ut)))) {
		log("syslogin_perform_login: couldn't malloc()");
		return 0;
	}
	construct_utmp(li, ut);
	login(ut);

	return 1;
}

static int
syslogin_perform_logout(struct logininfo *li)
{
# ifdef HAVE_LOGOUT
	char line[8];

	(void)line_stripname(line, li->line, sizeof(line));

	if (!logout(line)) {
		log("syslogin_perform_logout: logout() returned an error");
#  ifdef HAVE_LOGWTMP
	} else {
		logwtmp(line, "", "");
#  endif
	}
	/* FIXME: (ATL - if the need arises) What to do if we have
	 * login, but no logout?  what if logout but no logwtmp? All
	 * routines are in libutil so they should all be there,
	 * but... */
# endif
	return 1;
}

int
syslogin_write_entry(struct logininfo *li)
{
	switch (li->type) {
	case LTYPE_LOGIN:
		return syslogin_perform_login(li);
	case LTYPE_LOGOUT:
		return syslogin_perform_logout(li);
	default:
		log("syslogin_write_entry: Invalid type field");
		return 0;
	}
}
#endif /* USE_LOGIN */

/* end of file log-syslogin.c */

/**
 ** Low-level lastlog functions
 **/

#ifdef USE_LASTLOG
#define LL_FILE 1
#define LL_DIR 2
#define LL_OTHER 3

static void
lastlog_construct(struct logininfo *li, struct lastlog *last)
{
	/* clear the structure */
	memset(last, '\0', sizeof(*last));

	(void)line_stripname(last->ll_line, li->line, sizeof(last->ll_line));
	strlcpy(last->ll_host, li->hostname,
		MIN_SIZEOF(last->ll_host, li->hostname));
	last->ll_time = li->tv_sec;
}

static int
lastlog_filetype(char *filename)
{
	struct stat st;

	if (stat(LASTLOG_FILE, &st) != 0) {
		log("lastlog_perform_login: Couldn't stat %s: %s", LASTLOG_FILE,
			strerror(errno));
		return 0;
	}
	if (S_ISDIR(st.st_mode))
		return LL_DIR;
	else if (S_ISREG(st.st_mode))
		return LL_FILE;
	else
		return LL_OTHER;
}


/* open the file (using filemode) and seek to the login entry */
static int
lastlog_openseek(struct logininfo *li, int *fd, int filemode)
{
	off_t offset;
	int type;
	char lastlog_file[1024];

	type = lastlog_filetype(LASTLOG_FILE);
	switch (type) {
		case LL_FILE:
			strlcpy(lastlog_file, LASTLOG_FILE, sizeof(lastlog_file));
			break;
		case LL_DIR:
			snprintf(lastlog_file, sizeof(lastlog_file), "%s/%s",
				 LASTLOG_FILE, li->username);
			break;
		default:
			log("lastlog_openseek: %.100s is not a file or directory!",
			    LASTLOG_FILE);
			return 0;
	}

	*fd = open(lastlog_file, filemode);
	if ( *fd < 0) {
		debug("lastlog_openseek: Couldn't open %s: %s",
		    lastlog_file, strerror(errno));
		return 0;
	}

	if (type == LL_FILE) {
		/* find this uid's offset in the lastlog file */
		offset = (off_t) ((long)li->uid * sizeof(struct lastlog));

		if ( lseek(*fd, offset, SEEK_SET) != offset ) {
			log("lastlog_openseek: %s->lseek(): %s",
			 lastlog_file, strerror(errno));
			return 0;
		}
	}

	return 1;
}

static int
lastlog_perform_login(struct logininfo *li)
{
	struct lastlog last;
	int fd;

	/* create our struct lastlog */
	lastlog_construct(li, &last);

	if (!lastlog_openseek(li, &fd, O_RDWR|O_CREAT))
		return(0);

	/* write the entry */
	if (atomicio(write, fd, &last, sizeof(last)) != sizeof(last)) {
		close(fd);
		log("lastlog_write_filemode: Error writing to %s: %s",
		    LASTLOG_FILE, strerror(errno));
		return 0;
	}

	close(fd);
	return 1;
}

int
lastlog_write_entry(struct logininfo *li)
{
	switch(li->type) {
	case LTYPE_LOGIN:
		return lastlog_perform_login(li);
	default:
		log("lastlog_write_entry: Invalid type field");
		return 0;
	}
}

static void
lastlog_populate_entry(struct logininfo *li, struct lastlog *last)
{
	line_fullname(li->line, last->ll_line, sizeof(li->line));
	strlcpy(li->hostname, last->ll_host,
		MIN_SIZEOF(li->hostname, last->ll_host));
	li->tv_sec = last->ll_time;
}

int
lastlog_get_entry(struct logininfo *li)
{
	struct lastlog last;
	int fd;

	if (!lastlog_openseek(li, &fd, O_RDONLY))
		return 0;

	if (atomicio(read, fd, &last, sizeof(last)) != sizeof(last)) {
		close(fd);
		log("lastlog_get_entry: Error reading from %s: %s",
		    LASTLOG_FILE, strerror(errno));
		return 0;
	}

	close(fd);

	lastlog_populate_entry(li, &last);

	return 1;
}
#endif /* USE_LASTLOG */
