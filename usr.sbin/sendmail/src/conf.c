/*
 * Copyright (c) 1983 Eric P. Allman
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static char sccsid[] = "@(#)conf.c	8.82 (Berkeley) 3/6/94";
#endif /* not lint */

# include "sendmail.h"
# include "pathnames.h"
# include <sys/ioctl.h>
# include <sys/param.h>
# include <netdb.h>
# include <pwd.h>

/*
**  CONF.C -- Sendmail Configuration Tables.
**
**	Defines the configuration of this installation.
**
**	Configuration Variables:
**		HdrInfo -- a table describing well-known header fields.
**			Each entry has the field name and some flags,
**			which are described in sendmail.h.
**
**	Notes:
**		I have tried to put almost all the reasonable
**		configuration information into the configuration
**		file read at runtime.  My intent is that anything
**		here is a function of the version of UNIX you
**		are running, or is really static -- for example
**		the headers are a superset of widely used
**		protocols.  If you find yourself playing with
**		this file too much, you may be making a mistake!
*/




/*
**  Header info table
**	Final (null) entry contains the flags used for any other field.
**
**	Not all of these are actually handled specially by sendmail
**	at this time.  They are included as placeholders, to let
**	you know that "someday" I intend to have sendmail do
**	something with them.
*/

struct hdrinfo	HdrInfo[] =
{
		/* originator fields, most to least significant  */
	"resent-sender",	H_FROM|H_RESENT,
	"resent-from",		H_FROM|H_RESENT,
	"resent-reply-to",	H_FROM|H_RESENT,
	"sender",		H_FROM,
	"from",			H_FROM,
	"reply-to",		H_FROM,
	"full-name",		H_ACHECK,
	"return-receipt-to",	H_FROM|H_RECEIPTTO,
	"errors-to",		H_FROM|H_ERRORSTO,

		/* destination fields */
	"to",			H_RCPT,
	"resent-to",		H_RCPT|H_RESENT,
	"cc",			H_RCPT,
	"resent-cc",		H_RCPT|H_RESENT,
	"bcc",			H_RCPT|H_ACHECK,
	"resent-bcc",		H_RCPT|H_ACHECK|H_RESENT,
	"apparently-to",	H_RCPT,

		/* message identification and control */
	"message-id",		0,
	"resent-message-id",	H_RESENT,
	"message",		H_EOH,
	"text",			H_EOH,

		/* date fields */
	"date",			0,
	"resent-date",		H_RESENT,

		/* trace fields */
	"received",		H_TRACE|H_FORCE,
	"x400-received",	H_TRACE|H_FORCE,
	"via",			H_TRACE|H_FORCE,
	"mail-from",		H_TRACE|H_FORCE,

		/* miscellaneous fields */
	"comments",		H_FORCE,
	"return-path",		H_FORCE|H_ACHECK,

	NULL,			0,
};



/*
**  Location of system files/databases/etc.
*/

char	*PidFile =	_PATH_SENDMAILPID;	/* stores daemon proc id */



/*
**  Privacy values
*/

struct prival PrivacyValues[] =
{
	"public",		PRIV_PUBLIC,
	"needmailhelo",		PRIV_NEEDMAILHELO,
	"needexpnhelo",		PRIV_NEEDEXPNHELO,
	"needvrfyhelo",		PRIV_NEEDVRFYHELO,
	"noexpn",		PRIV_NOEXPN,
	"novrfy",		PRIV_NOVRFY,
	"restrictmailq",	PRIV_RESTRICTMAILQ,
	"restrictqrun",		PRIV_RESTRICTQRUN,
	"authwarnings",		PRIV_AUTHWARNINGS,
	"goaway",		PRIV_GOAWAY,
	NULL,			0,
};



/*
**  Miscellaneous stuff.
*/

int	DtableSize =	50;		/* max open files; reset in 4.2bsd */


/*
**  Following should be config parameters (and probably will be in
**  future releases).  In the meantime, setting these is considered
**  unsupported, and is intentionally undocumented.
*/

#ifdef BROKENSMTPPEERS
bool	BrokenSmtpPeers = TRUE;		/* set if you have broken SMTP peers */
#else
bool	BrokenSmtpPeers = FALSE;	/* set if you have broken SMTP peers */
#endif
#ifdef NOLOOPBACKCHECK
bool	CheckLoopBack = FALSE;		/* set to check HELO loopback */
#else
bool	CheckLoopBack = TRUE;		/* set to check HELO loopback */
#endif

/*
**  SETDEFAULTS -- set default values
**
**	Because of the way freezing is done, these must be initialized
**	using direct code.
**
**	Parameters:
**		e -- the default envelope.
**
**	Returns:
**		none.
**
**	Side Effects:
**		Initializes a bunch of global variables to their
**		default values.
*/

#define DAYS		* 24 * 60 * 60

setdefaults(e)
	register ENVELOPE *e;
{
	SpaceSub = ' ';				/* option B */
	QueueLA = 8;				/* option x */
	RefuseLA = 12;				/* option X */
	WkRecipFact = 30000L;			/* option y */
	WkClassFact = 1800L;			/* option z */
	WkTimeFact = 90000L;			/* option Z */
	QueueFactor = WkRecipFact * 20;		/* option q */
	FileMode = (RealUid != geteuid()) ? 0644 : 0600;
						/* option F */
	DefUid = 1;				/* option u */
	DefGid = 1;				/* option g */
	CheckpointInterval = 10;		/* option C */
	MaxHopCount = 25;			/* option h */
	e->e_sendmode = SM_FORK;		/* option d */
	e->e_errormode = EM_PRINT;		/* option e */
	SevenBit = FALSE;			/* option 7 */
	MaxMciCache = 1;			/* option k */
	MciCacheTimeout = 300;			/* option K */
	LogLevel = 9;				/* option L */
	settimeouts(NULL);			/* option r */
	TimeOuts.to_q_return = 5 DAYS;		/* option T */
	TimeOuts.to_q_warning = 0;		/* option T */
	PrivacyFlags = 0;			/* option p */
	setdefuser();
	setupmaps();
	setupmailers();
}


/*
**  SETDEFUSER -- set/reset DefUser using DefUid (for initgroups())
*/

setdefuser()
{
	struct passwd *defpwent;
	static char defuserbuf[40];

	DefUser = defuserbuf;
	if ((defpwent = getpwuid(DefUid)) != NULL)
		strcpy(defuserbuf, defpwent->pw_name);
	else
		strcpy(defuserbuf, "nobody");
}
/*
**  HOST_MAP_INIT -- initialize host class structures
*/

bool
host_map_init(map, args)
	MAP *map;
	char *args;
{
	register char *p = args;

	for (;;)
	{
		while (isascii(*p) && isspace(*p))
			p++;
		if (*p != '-')
			break;
		switch (*++p)
		{
		  case 'a':
			map->map_app = ++p;
			break;
		}
		while (*p != '\0' && !(isascii(*p) && isspace(*p)))
			p++;
		if (*p != '\0')
			*p++ = '\0';
	}
	if (map->map_app != NULL)
		map->map_app = newstr(map->map_app);
	return TRUE;
}
/*
**  SETUPMAILERS -- initialize default mailers
*/

setupmailers()
{
	char buf[100];

	strcpy(buf, "prog, P=/bin/sh, F=lsD, A=sh -c $u");
	makemailer(buf);

	strcpy(buf, "*file*, P=/dev/null, F=lsDFMPEu, A=FILE");
	makemailer(buf);

	strcpy(buf, "*include*, P=/dev/null, F=su, A=INCLUDE");
	makemailer(buf);
}
/*
**  SETUPMAPS -- set up map classes
*/

#define MAPDEF(name, ext, flags, parse, open, close, lookup, store) \
	{ \
		extern bool parse __P((MAP *, char *)); \
		extern bool open __P((MAP *, int)); \
		extern void close __P((MAP *)); \
		extern char *lookup __P((MAP *, char *, char **, int *)); \
		extern void store __P((MAP *, char *, char *)); \
		s = stab(name, ST_MAPCLASS, ST_ENTER); \
		s->s_mapclass.map_cname = name; \
		s->s_mapclass.map_ext = ext; \
		s->s_mapclass.map_cflags = flags; \
		s->s_mapclass.map_parse = parse; \
		s->s_mapclass.map_open = open; \
		s->s_mapclass.map_close = close; \
		s->s_mapclass.map_lookup = lookup; \
		s->s_mapclass.map_store = store; \
	}

setupmaps()
{
	register STAB *s;

#ifdef NEWDB
	MAPDEF("hash", ".db", MCF_ALIASOK|MCF_REBUILDABLE,
		map_parseargs, hash_map_open, db_map_close,
		db_map_lookup, db_map_store);
	MAPDEF("btree", ".db", MCF_ALIASOK|MCF_REBUILDABLE,
		map_parseargs, bt_map_open, db_map_close,
		db_map_lookup, db_map_store);
#endif

#ifdef NDBM
	MAPDEF("dbm", ".dir", MCF_ALIASOK|MCF_REBUILDABLE,
		map_parseargs, ndbm_map_open, ndbm_map_close,
		ndbm_map_lookup, ndbm_map_store);
#endif

#ifdef NIS
	MAPDEF("nis", NULL, MCF_ALIASOK,
		map_parseargs, nis_map_open, nis_map_close,
		nis_map_lookup, nis_map_store);
#endif

	MAPDEF("stab", NULL, MCF_ALIASOK|MCF_ALIASONLY,
		map_parseargs, stab_map_open, stab_map_close,
		stab_map_lookup, stab_map_store);

	MAPDEF("implicit", NULL, MCF_ALIASOK|MCF_ALIASONLY|MCF_REBUILDABLE,
		map_parseargs, impl_map_open, impl_map_close,
		impl_map_lookup, impl_map_store);

	/* host DNS lookup */
	MAPDEF("host", NULL, 0,
		host_map_init, null_map_open, null_map_close,
		host_map_lookup, null_map_store);

	/* dequote map */
	MAPDEF("dequote", NULL, 0,
		dequote_init, null_map_open, null_map_close,
		dequote_map, null_map_store);

#if 0
# ifdef USERDB
	/* user database */
	MAPDEF("udb", ".db", 0,
		udb_map_parse, null_map_open, null_map_close,
		udb_map_lookup, null_map_store);
# endif
#endif
}

#undef MAPDEF
/*
**  USERNAME -- return the user id of the logged in user.
**
**	Parameters:
**		none.
**
**	Returns:
**		The login name of the logged in user.
**
**	Side Effects:
**		none.
**
**	Notes:
**		The return value is statically allocated.
*/

char *
username()
{
	static char *myname = NULL;
	extern char *getlogin();
	register struct passwd *pw;

	/* cache the result */
	if (myname == NULL)
	{
		myname = getlogin();
		if (myname == NULL || myname[0] == '\0')
		{
			pw = getpwuid(RealUid);
			if (pw != NULL)
				myname = newstr(pw->pw_name);
		}
		else
		{
			uid_t uid = RealUid;

			myname = newstr(myname);
			if ((pw = getpwnam(myname)) == NULL ||
			      (uid != 0 && uid != pw->pw_uid))
			{
				pw = getpwuid(uid);
				if (pw != NULL)
					myname = newstr(pw->pw_name);
			}
		}
		if (myname == NULL || myname[0] == '\0')
		{
			syserr("554 Who are you?");
			myname = "postmaster";
		}
	}

	return (myname);
}
/*
**  TTYPATH -- Get the path of the user's tty
**
**	Returns the pathname of the user's tty.  Returns NULL if
**	the user is not logged in or if s/he has write permission
**	denied.
**
**	Parameters:
**		none
**
**	Returns:
**		pathname of the user's tty.
**		NULL if not logged in or write permission denied.
**
**	Side Effects:
**		none.
**
**	WARNING:
**		Return value is in a local buffer.
**
**	Called By:
**		savemail
*/

char *
ttypath()
{
	struct stat stbuf;
	register char *pathn;
	extern char *ttyname();
	extern char *getlogin();

	/* compute the pathname of the controlling tty */
	if ((pathn = ttyname(2)) == NULL && (pathn = ttyname(1)) == NULL &&
	    (pathn = ttyname(0)) == NULL)
	{
		errno = 0;
		return (NULL);
	}

	/* see if we have write permission */
	if (stat(pathn, &stbuf) < 0 || !bitset(02, stbuf.st_mode))
	{
		errno = 0;
		return (NULL);
	}

	/* see if the user is logged in */
	if (getlogin() == NULL)
		return (NULL);

	/* looks good */
	return (pathn);
}
/*
**  CHECKCOMPAT -- check for From and To person compatible.
**
**	This routine can be supplied on a per-installation basis
**	to determine whether a person is allowed to send a message.
**	This allows restriction of certain types of internet
**	forwarding or registration of users.
**
**	If the hosts are found to be incompatible, an error
**	message should be given using "usrerr" and 0 should
**	be returned.
**
**	'NoReturn' can be set to suppress the return-to-sender
**	function; this should be done on huge messages.
**
**	Parameters:
**		to -- the person being sent to.
**
**	Returns:
**		an exit status
**
**	Side Effects:
**		none (unless you include the usrerr stuff)
*/

checkcompat(to, e)
	register ADDRESS *to;
	register ENVELOPE *e;
{
# ifdef lint
	if (to == NULL)
		to++;
# endif /* lint */

	if (tTd(49, 1))
		printf("checkcompat(to=%s, from=%s)\n",
			to->q_paddr, e->e_from.q_paddr);

# ifdef EXAMPLE_CODE
	/* this code is intended as an example only */
	register STAB *s;

	s = stab("arpa", ST_MAILER, ST_FIND);
	if (s != NULL && e->e_from.q_mailer != LocalMailer &&
	    to->q_mailer == s->s_mailer)
	{
		usrerr("553 No ARPA mail through this machine: see your system administration");
		/* NoReturn = TRUE; to supress return copy */
		return (EX_UNAVAILABLE);
	}
# endif /* EXAMPLE_CODE */
	return (EX_OK);
}
/*
**  SETSIGNAL -- set a signal handler
**
**	This is essentially old BSD "signal(3)".
*/

sigfunc_t
setsignal(sig, handler)
	int sig;
	sigfunc_t handler;
{
#if defined(SYS5SIGNALS) || defined(BSD4_3) || defined(_AUX_SOURCE)
	return signal(sig, handler);
#else
	struct sigaction n, o;

	bzero(&n, sizeof n);
	n.sa_handler = handler;
	if (sigaction(sig, &n, &o) < 0)
		return SIG_ERR;
	return o.sa_handler;
#endif
}
/*
**  HOLDSIGS -- arrange to hold all signals
**
**	Parameters:
**		none.
**
**	Returns:
**		none.
**
**	Side Effects:
**		Arranges that signals are held.
*/

holdsigs()
{
}
/*
**  RLSESIGS -- arrange to release all signals
**
**	This undoes the effect of holdsigs.
**
**	Parameters:
**		none.
**
**	Returns:
**		none.
**
**	Side Effects:
**		Arranges that signals are released.
*/

rlsesigs()
{
}
/*
**  INIT_MD -- do machine dependent initializations
**
**	Systems that have global modes that should be set should do
**	them here rather than in main.
*/

#ifdef _AUX_SOURCE
# include	<compat.h>
#endif

init_md(argc, argv)
	int argc;
	char **argv;
{
#ifdef _AUX_SOURCE
	setcompat(getcompat() | COMPAT_BSDPROT);
#endif
}
/*
**  GETLA -- get the current load average
**
**	This code stolen from la.c.
**
**	Parameters:
**		none.
**
**	Returns:
**		The current load average as an integer.
**
**	Side Effects:
**		none.
*/

/* try to guess what style of load average we have */
#define LA_ZERO		1	/* always return load average as zero */
#define LA_INT		2	/* read kmem for avenrun; interpret as long */
#define LA_FLOAT	3	/* read kmem for avenrun; interpret as float */
#define LA_SUBR		4	/* call getloadavg */
#define LA_MACH		5	/* MACH load averages (as on NeXT boxes) */
#define LA_SHORT	6	/* read kmem for avenrun; interpret as short */
#define LA_PROCSTR	7	/* read string ("1.17") from /proc/loadavg */

/* do guesses based on general OS type */
#ifndef LA_TYPE
# define LA_TYPE	LA_ZERO
#endif

#if (LA_TYPE == LA_INT) || (LA_TYPE == LA_FLOAT) || (LA_TYPE == LA_SHORT)

#include <nlist.h>

#ifndef LA_AVENRUN
# ifdef SYSTEM5
#  define LA_AVENRUN	"avenrun"
# else
#  define LA_AVENRUN	"_avenrun"
# endif
#endif

/* _PATH_UNIX should be defined in <paths.h> */
#ifndef _PATH_UNIX
# if defined(SYSTEM5)
#  define _PATH_UNIX	"/unix"
# else
#  define _PATH_UNIX	"/vmunix"
# endif
#endif

struct	nlist Nl[] =
{
	{ LA_AVENRUN },
#define	X_AVENRUN	0
	{ 0 },
};

#ifndef FSHIFT
# if defined(unixpc)
#  define FSHIFT	5
# endif

# if defined(__alpha)
#  define FSHIFT	10
# endif

# if (LA_TYPE == LA_INT) || (LA_TYPE == LA_SHORT)
#  define FSHIFT	8
# endif
#endif

#if ((LA_TYPE == LA_INT) || (LA_TYPE == LA_SHORT)) && !defined(FSCALE)
#  define FSCALE	(1 << FSHIFT)
#endif

getla()
{
	static int kmem = -1;
#if LA_TYPE == LA_INT
	long avenrun[3];
#else
# if LA_TYPE == LA_SHORT
	short avenrun[3];
# else
	double avenrun[3];
# endif
#endif
	extern off_t lseek();
	extern int errno;

	if (kmem < 0)
	{
		kmem = open("/dev/kmem", 0, 0);
		if (kmem < 0)
		{
			if (tTd(3, 1))
				printf("getla: open(/dev/kmem): %s\n",
					errstring(errno));
			return (-1);
		}
		(void) fcntl(kmem, F_SETFD, 1);
		if (nlist(_PATH_UNIX, Nl) < 0)
		{
			if (tTd(3, 1))
				printf("getla: nlist(%s): %s\n", _PATH_UNIX,
					errstring(errno));
			return (-1);
		}
		if (Nl[X_AVENRUN].n_value == 0)
		{
			if (tTd(3, 1))
				printf("getla: nlist(%s, %s) ==> 0\n",
					_PATH_UNIX, LA_AVENRUN);
			return (-1);
		}
	}
	if (tTd(3, 20))
		printf("getla: symbol address = %#x\n", Nl[X_AVENRUN].n_value);
	if (lseek(kmem, (off_t) Nl[X_AVENRUN].n_value, 0) == -1 ||
	    read(kmem, (char *) avenrun, sizeof(avenrun)) < sizeof(avenrun))
	{
		/* thank you Ian */
		if (tTd(3, 1))
			printf("getla: lseek or read: %s\n", errstring(errno));
		return (-1);
	}
#if (LA_TYPE == LA_INT) || (LA_TYPE == LA_SHORT)
	if (tTd(3, 5))
	{
		printf("getla: avenrun = %d", avenrun[0]);
		if (tTd(3, 15))
			printf(", %d, %d", avenrun[1], avenrun[2]);
		printf("\n");
	}
	if (tTd(3, 1))
		printf("getla: %d\n", (int) (avenrun[0] + FSCALE/2) >> FSHIFT);
	return ((int) (avenrun[0] + FSCALE/2) >> FSHIFT);
#else
	if (tTd(3, 5))
	{
		printf("getla: avenrun = %g", avenrun[0]);
		if (tTd(3, 15))
			printf(", %g, %g", avenrun[1], avenrun[2]);
		printf("\n");
	}
	if (tTd(3, 1))
		printf("getla: %d\n", (int) (avenrun[0] +0.5));
	return ((int) (avenrun[0] + 0.5));
#endif
}

#else
#if LA_TYPE == LA_SUBR

#ifdef DGUX

#include <sys/dg_sys_info.h>

int getla()
{
	struct dg_sys_info_load_info load_info;

	dg_sys_info((long *)&load_info,
		DG_SYS_INFO_LOAD_INFO_TYPE, DG_SYS_INFO_LOAD_VERSION_0);

	return((int) (load_info.one_minute + 0.5));
}

#else

getla()
{
	double avenrun[3];

	if (getloadavg(avenrun, sizeof(avenrun) / sizeof(avenrun[0])) < 0)
	{
		if (tTd(3, 1))
			perror("getla: getloadavg failed:");
		return (-1);
	}
	if (tTd(3, 1))
		printf("getla: %d\n", (int) (avenrun[0] +0.5));
	return ((int) (avenrun[0] + 0.5));
}

#endif /* DGUX */
#else
#if LA_TYPE == LA_MACH

/*
**  This has been tested on NEXTSTEP release 2.1/3.X.
*/

#if defined(NX_CURRENT_COMPILER_RELEASE) && NX_CURRENT_COMPILER_RELEASE > NX_COMPILER_RELEASE_3_0
# include <mach/mach.h>
#else
# include <mach.h>
#endif

getla()
{
	processor_set_t default_set;
	kern_return_t error;
	unsigned int info_count;
	struct processor_set_basic_info info;
	host_t host;

	error = processor_set_default(host_self(), &default_set);
	if (error != KERN_SUCCESS)
		return -1;
	info_count = PROCESSOR_SET_BASIC_INFO_COUNT;
	if (processor_set_info(default_set, PROCESSOR_SET_BASIC_INFO,
			       &host, (processor_set_info_t)&info,
			       &info_count) != KERN_SUCCESS)
	{
		return -1;
	}
	return (int) (info.load_average + (LOAD_SCALE / 2)) / LOAD_SCALE;
}


#else
#if LA_TYPE == LA_PROCSTR

/*
**  Read /proc/loadavg for the load average.  This is assumed to be
**  in a format like "0.15 0.12 0.06".
**
**	Initially intended for Linux.  This has been in the kernel
**	since at least 0.99.15.
*/

# ifndef _PATH_LOADAVG
#  define _PATH_LOADAVG	"/proc/loadavg"
# endif

int
getla()
{
	double avenrun;
	register int result;
	FILE *fp;

	fp = fopen(_PATH_LOADAVG, "r");
	if (fp == NULL) 
	{
		if (tTd(3, 1))
			printf("getla: fopen(%s): %s\n",
				_PATH_LOADAVG, errstring(errno));
		return -1;
	}
	result = fscanf(fp, "%lf", &avenrun);
	fclose(fp);
	if (result != 1)
	{
		if (tTd(3, 1))
			printf("getla: fscanf() = %d: %s\n",
				result, errstring(errno));
		return -1;
	}

	if (tTd(3, 1))
		printf("getla(): %.2f\n", avenrun);

	return ((int) (avenrun + 0.5));
}

#else

getla()
{
	if (tTd(3, 1))
		printf("getla: ZERO\n");
	return (0);
}

#endif
#endif
#endif
#endif


/*
 * Copyright 1989 Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of M.I.T. not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  M.I.T. makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * M.I.T. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL M.I.T.
 * BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Authors:  Many and varied...
 */

/* Non Apollo stuff removed by Don Lewis 11/15/93 */
#ifndef lint
static char  rcsid[] = "@(#)$Id: conf.c,v 1.6 1994/03/19 07:36:47 alm Exp $";
#endif /* !lint */

#ifdef apollo
# undef volatile
#    include <apollo/base.h>

/* ARGSUSED */
int getloadavg( call_data )
     caddr_t	call_data;	/* pointer to (double) return value */
{
     double *avenrun = (double *) call_data;
     int i;
     status_$t      st;
     long loadav[3];
     proc1_$get_loadav(loadav, &st);
     *avenrun = loadav[0] / (double) (1 << 16);
     return(0);
}
#   endif /* apollo */
/*
**  SHOULDQUEUE -- should this message be queued or sent?
**
**	Compares the message cost to the load average to decide.
**
**	Parameters:
**		pri -- the priority of the message in question.
**		ctime -- the message creation time.
**
**	Returns:
**		TRUE -- if this message should be queued up for the
**			time being.
**		FALSE -- if the load is low enough to send this message.
**
**	Side Effects:
**		none.
*/

bool
shouldqueue(pri, ctime)
	long pri;
	time_t ctime;
{
	if (CurrentLA < QueueLA)
		return (FALSE);
	if (CurrentLA >= RefuseLA)
		return (TRUE);
	return (pri > (QueueFactor / (CurrentLA - QueueLA + 1)));
}
/*
**  REFUSECONNECTIONS -- decide if connections should be refused
**
**	Parameters:
**		none.
**
**	Returns:
**		TRUE if incoming SMTP connections should be refused
**			(for now).
**		FALSE if we should accept new work.
**
**	Side Effects:
**		none.
*/

bool
refuseconnections()
{
#ifdef XLA
	if (!xla_smtp_ok())
		return TRUE;
#endif

	/* this is probably too simplistic */
	return (CurrentLA >= RefuseLA);
}
/*
**  SETPROCTITLE -- set process title for ps
**
**	Parameters:
**		fmt -- a printf style format string.
**		a, b, c -- possible parameters to fmt.
**
**	Returns:
**		none.
**
**	Side Effects:
**		Clobbers argv of our main procedure so ps(1) will
**		display the title.
*/

#ifdef SETPROCTITLE
# ifdef HASSETPROCTITLE
   *** ERROR ***  Cannot have both SETPROCTITLE and HASSETPROCTITLE defined
# endif
# ifdef __hpux
#  include <sys/pstat.h>
# endif
# ifdef BSD4_4
#  include <machine/vmparam.h>
#  include <sys/exec.h>
#  ifdef __bsdi__
#   undef PS_STRINGS	/* BSDI 1.0 doesn't do PS_STRINGS as we expect */
#   define PROCTITLEPAD	'\0'
#  endif
#  ifdef PS_STRINGS
#   define SETPROC_STATIC static
#  endif
# endif
# ifndef SETPROC_STATIC
#  define SETPROC_STATIC
# endif
#endif

#ifndef PROCTITLEPAD
# define PROCTITLEPAD	' '
#endif

#ifndef HASSETPROCTITLE

/*VARARGS1*/
#ifdef __STDC__
setproctitle(char *fmt, ...)
#else
setproctitle(fmt, va_alist)
	char *fmt;
	va_dcl
#endif
{
# ifdef SETPROCTITLE
	register char *p;
	register int i;
	SETPROC_STATIC char buf[MAXLINE];
	VA_LOCAL_DECL
#  ifdef __hpux
	union pstun pst;
#  endif
	extern char **Argv;
	extern char *LastArgv;

	p = buf;

	/* print sendmail: heading for grep */
	(void) strcpy(p, "sendmail: ");
	p += strlen(p);

	/* print the argument string */
	VA_START(fmt);
	(void) vsprintf(p, fmt, ap);
	VA_END;

	i = strlen(buf);

#  ifdef __hpux
	pst.pst_command = buf;
	pstat(PSTAT_SETCMD, pst, i, 0, 0);
#  else
#   ifdef PS_STRINGS
	PS_STRINGS->ps_nargvstr = 1;
	PS_STRINGS->ps_argvstr = buf;
#   else
	if (i > LastArgv - Argv[0] - 2)
	{
		i = LastArgv - Argv[0] - 2;
		buf[i] = '\0';
	}
	(void) strcpy(Argv[0], buf);
	p = &Argv[0][i];
	while (p < LastArgv)
		*p++ = PROCTITLEPAD;
#   endif
#  endif
# endif /* SETPROCTITLE */
}

#endif
/*
**  REAPCHILD -- pick up the body of my child, lest it become a zombie
**
**	Parameters:
**		none.
**
**	Returns:
**		none.
**
**	Side Effects:
**		Picks up extant zombies.
*/

void
reapchild()
{
	int olderrno = errno;
# ifdef HASWAITPID
	auto int status;
	int count;
	int pid;

	count = 0;
	while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
	{
		if (count++ > 1000)
		{
			syslog(LOG_ALERT, "reapchild: waitpid loop: pid=%d, status=%x",
				pid, status);
			break;
		}
	}
# else
# ifdef WNOHANG
	union wait status;

	while (wait3(&status, WNOHANG, (struct rusage *) NULL) > 0)
		continue;
# else /* WNOHANG */
	auto int status;

	while (wait(&status) > 0)
		continue;
# endif /* WNOHANG */
# endif
# ifdef SYS5SIGNALS
	(void) setsignal(SIGCHLD, reapchild);
# endif
	errno = olderrno;
}
/*
**  UNSETENV -- remove a variable from the environment
**
**	Not needed on newer systems.
**
**	Parameters:
**		name -- the string name of the environment variable to be
**			deleted from the current environment.
**
**	Returns:
**		none.
**
**	Globals:
**		environ -- a pointer to the current environment.
**
**	Side Effects:
**		Modifies environ.
*/

#ifndef HASUNSETENV

void
unsetenv(name)
	char *name;
{
	extern char **environ;
	register char **pp;
	int len = strlen(name);

	for (pp = environ; *pp != NULL; pp++)
	{
		if (strncmp(name, *pp, len) == 0 &&
		    ((*pp)[len] == '=' || (*pp)[len] == '\0'))
			break;
	}

	for (; *pp != NULL; pp++)
		*pp = pp[1];
}

#endif
/*
**  GETDTABLESIZE -- return number of file descriptors
**
**	Only on non-BSD systems
**
**	Parameters:
**		none
**
**	Returns:
**		size of file descriptor table
**
**	Side Effects:
**		none
*/

#ifdef SOLARIS
# include <sys/resource.h>
#endif

int
getdtsize()
{
#ifdef RLIMIT_NOFILE
	struct rlimit rl;

	if (getrlimit(RLIMIT_NOFILE, &rl) >= 0)
		return rl.rlim_cur;
#endif

# ifdef HASGETDTABLESIZE
	return getdtablesize();
# else
#  ifdef _SC_OPEN_MAX
	return sysconf(_SC_OPEN_MAX);
#  else
	return NOFILE;
#  endif
# endif
}
/*
**  UNAME -- get the UUCP name of this system.
*/

#ifndef HASUNAME

int
uname(name)
	struct utsname *name;
{
	FILE *file;
	char *n;

	name->nodename[0] = '\0';

	/* try /etc/whoami -- one line with the node name */
	if ((file = fopen("/etc/whoami", "r")) != NULL)
	{
		(void) fgets(name->nodename, NODE_LENGTH + 1, file);
		(void) fclose(file);
		n = strchr(name->nodename, '\n');
		if (n != NULL)
			*n = '\0';
		if (name->nodename[0] != '\0')
			return (0);
	}

	/* try /usr/include/whoami.h -- has a #define somewhere */
	if ((file = fopen("/usr/include/whoami.h", "r")) != NULL)
	{
		char buf[MAXLINE];

		while (fgets(buf, MAXLINE, file) != NULL)
			if (sscanf(buf, "#define sysname \"%*[^\"]\"",
					NODE_LENGTH, name->nodename) > 0)
				break;
		(void) fclose(file);
		if (name->nodename[0] != '\0')
			return (0);
	}

#ifdef TRUST_POPEN
	/*
	**  Popen is known to have security holes.
	*/

	/* try uuname -l to return local name */
	if ((file = popen("uuname -l", "r")) != NULL)
	{
		(void) fgets(name, NODE_LENGTH + 1, file);
		(void) pclose(file);
		n = strchr(name, '\n');
		if (n != NULL)
			*n = '\0';
		if (name->nodename[0] != '\0')
			return (0);
	}
#endif
	
	return (-1);
}
#endif /* HASUNAME */
/*
**  INITGROUPS -- initialize groups
**
**	Stub implementation for System V style systems
*/

#ifndef HASINITGROUPS

initgroups(name, basegid)
	char *name;
	int basegid;
{
	return 0;
}

#endif
/*
**  SETSID -- set session id (for non-POSIX systems)
*/

#ifndef HASSETSID

pid_t
setsid __P ((void))
{
#ifdef TIOCNOTTY
	int fd;

	fd = open("/dev/tty", 2);
	if (fd >= 0)
	{
		(void) ioctl(fd, (int) TIOCNOTTY, (char *) 0);
		(void) close(fd);
	}
#endif /* TIOCNOTTY */
# ifdef SYS5SETPGRP
	return setpgrp();
# else
	return setpgid(0, getpid());
# endif
}

#endif
/*
**  DGUX_INET_ADDR -- inet_addr for DG/UX
**
**	Data General DG/UX version of inet_addr returns a struct in_addr
**	instead of a long.  This patches things.
*/

#ifdef DGUX

#undef inet_addr

long
dgux_inet_addr(host)
	char *host;
{
	struct in_addr haddr;

	haddr = inet_addr(host);
	return haddr.s_addr;
}

#endif
/*
**  GETOPT -- for old systems or systems with bogus implementations
*/

#ifdef NEEDGETOPT

/*
 * Copyright (c) 1985 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */


/*
** this version hacked to add `atend' flag to allow state machine
** to reset if invoked by the program to scan args for a 2nd time
*/

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)getopt.c	4.3 (Berkeley) 3/9/86";
#endif /* LIBC_SCCS and not lint */

#include <stdio.h>

/*
 * get option letter from argument vector
 */
#ifdef _CONVEX_SOURCE
extern int	optind, opterr;
#else
int	opterr = 1;		/* if error message should be printed */
int	optind = 1;		/* index into parent argv vector */
#endif
int	optopt;			/* character checked for validity */
char	*optarg;		/* argument associated with option */

#define BADCH	(int)'?'
#define EMSG	""
#define tell(s)	if (opterr) {fputs(*nargv,stderr);fputs(s,stderr); \
		fputc(optopt,stderr);fputc('\n',stderr);return(BADCH);}

getopt(nargc,nargv,ostr)
	int		nargc;
	char *const	*nargv;
	const char	*ostr;
{
	static char	*place = EMSG;	/* option letter processing */
	static char	atend = 0;
	register char	*oli;		/* option letter list index */

	if (atend) {
		atend = 0;
		place = EMSG;
	}
	if(!*place) {			/* update scanning pointer */
		if (optind >= nargc || *(place = nargv[optind]) != '-' || !*++place) {
			atend++;
			return(EOF);
		}
		if (*place == '-') {	/* found "--" */
			++optind;
			atend++;
			return(EOF);
		}
	}				/* option letter okay? */
	if ((optopt = (int)*place++) == (int)':' || !(oli = strchr(ostr,optopt))) {
		if (!*place) ++optind;
		tell(": illegal option -- ");
	}
	if (*++oli != ':') {		/* don't need argument */
		optarg = NULL;
		if (!*place) ++optind;
	}
	else {				/* need an argument */
		if (*place) optarg = place;	/* no white space */
		else if (nargc <= ++optind) {	/* no arg */
			place = EMSG;
			tell(": option requires an argument -- ");
		}
	 	else optarg = nargv[optind];	/* white space */
		place = EMSG;
		++optind;
	}
	return(optopt);			/* dump back option letter */
}

#endif
/*
**  VFPRINTF, VSPRINTF -- for old 4.3 BSD systems missing a real version
*/

#ifdef NEEDVPRINTF

#define MAXARG	16

vfprintf(fp, fmt, ap)
	FILE *	fp;
	char *	fmt;
	char **	ap;
{
	char *	bp[MAXARG];
	int	i = 0;

	while (*ap && i < MAXARG)
		bp[i++] = *ap++;
	fprintf(fp, fmt, bp[0], bp[1], bp[2], bp[3],
			 bp[4], bp[5], bp[6], bp[7],
			 bp[8], bp[9], bp[10], bp[11],
			 bp[12], bp[13], bp[14], bp[15]);
}

vsprintf(s, fmt, ap)
	char *	s;
	char *	fmt;
	char **	ap;
{
	char *	bp[MAXARG];
	int	i = 0;

	while (*ap && i < MAXARG)
		bp[i++] = *ap++;
	sprintf(s, fmt, bp[0], bp[1], bp[2], bp[3],
			bp[4], bp[5], bp[6], bp[7],
			bp[8], bp[9], bp[10], bp[11],
			bp[12], bp[13], bp[14], bp[15]);
}

#endif
/*
**  USERSHELLOK -- tell if a user's shell is ok for unrestricted use
**
**	Parameters:
**		shell -- the user's shell from /etc/passwd
**
**	Returns:
**		TRUE -- if it is ok to use this for unrestricted access.
**		FALSE -- if the shell is restricted.
*/

#if !HASGETUSERSHELL

# ifndef _PATH_SHELLS
#  define _PATH_SHELLS	"/etc/shells"
# endif

char	*DefaultUserShells[] =
{
	"/bin/sh",
	"/usr/bin/sh",
	"/bin/csh",
	"/usr/bin/csh",
#ifdef __hpux
	"/bin/rsh",
	"/bin/ksh",
	"/bin/rksh",
	"/bin/pam",
	"/usr/bin/keysh",
	"/bin/posix/sh",
#endif
	NULL
};

#endif

#define WILDCARD_SHELL	"/SENDMAIL/ANY/SHELL/"

bool
usershellok(shell)
	char *shell;
{
#if HASGETUSERSHELL
	register char *p;
	extern char *getusershell();

	setusershell();
	while ((p = getusershell()) != NULL)
		if (strcmp(p, shell) == 0 || strcmp(p, WILDCARD_SHELL) == 0)
			break;
	endusershell();
	return p != NULL;
#else
	register FILE *shellf;
	char buf[MAXLINE];

	shellf = fopen(_PATH_SHELLS, "r");
	if (shellf == NULL)
	{
		/* no /etc/shells; see if it is one of the std shells */
		char **d;

		for (d = DefaultUserShells; *d != NULL; d++)
		{
			if (strcmp(shell, *d) == 0)
				return TRUE;
		}
		return FALSE;
	}

	while (fgets(buf, sizeof buf, shellf) != NULL)
	{
		register char *p, *q;

		p = buf;
		while (*p != '\0' && *p != '#' && *p != '/')
			p++;
		if (*p == '#' || *p == '\0')
			continue;
		q = p;
		while (*p != '\0' && *p != '#' && !isspace(*p))
			p++;
		*p = '\0';
		if (strcmp(shell, q) == 0 || strcmp(WILDCARD_SHELL, q) == 0)
		{
			fclose(shellf);
			return TRUE;
		}
	}
	fclose(shellf);
	return FALSE;
#endif
}
/*
**  FREESPACE -- see how much free space is on the queue filesystem
**
**	Only implemented if you have statfs.
**
**	Parameters:
**		dir -- the directory in question.
**		bsize -- a variable into which the filesystem
**			block size is stored.
**
**	Returns:
**		The number of bytes free on the queue filesystem.
**		-1 if the statfs call fails.
**
**	Side effects:
**		Puts the filesystem block size into bsize.
*/

/* statfs types */
#define SFS_NONE	0	/* no statfs implementation */
#define SFS_USTAT	1	/* use ustat */
#define SFS_4ARGS	2	/* use four-argument statfs call */
#define SFS_VFS		3	/* use <sys/vfs.h> implementation */
#define SFS_MOUNT	4	/* use <sys/mount.h> implementation */
#define SFS_STATFS	5	/* use <sys/statfs.h> implementation */

#ifndef SFS_TYPE
# define SFS_TYPE	SFS_NONE
#endif

#if SFS_TYPE == SFS_USTAT
# include <ustat.h>
#endif
#if SFS_TYPE == SFS_4ARGS || SFS_TYPE == SFS_STATFS
# include <sys/statfs.h>
#endif
#if SFS_TYPE == SFS_VFS
# include <sys/vfs.h>
#endif
#if SFS_TYPE == SFS_MOUNT
# include <sys/mount.h>
#endif

long
freespace(dir, bsize)
	char *dir;
	long *bsize;
{
#if SFS_TYPE != SFS_NONE
# if SFS_TYPE == SFS_USTAT
	struct ustat fs;
	struct stat statbuf;
#  define FSBLOCKSIZE	DEV_BSIZE
#  define f_bavail	f_tfree
# else
#  if defined(ultrix)
	struct fs_data fs;
#   define f_bavail	fd_bfreen
#   define FSBLOCKSIZE	fs.fd_bsize
#  else
	struct statfs fs;
#   define FSBLOCKSIZE	fs.f_bsize
#   if defined(_SCO_unix_) || defined(IRIX) || defined(apollo)
#    define f_bavail f_bfree
#   endif
#  endif
# endif
	extern int errno;

# if SFS_TYPE == SFS_USTAT
	if (stat(dir, &statbuf) == 0 && ustat(statbuf.st_dev, &fs) == 0)
# else
#  if SFS_TYPE == SFS_4ARGS
	if (statfs(dir, &fs, sizeof fs, 0) == 0)
#  else
#   if defined(ultrix)
	if (statfs(dir, &fs) > 0)
#   else
	if (statfs(dir, &fs) == 0)
#   endif
#  endif
# endif
	{
		if (bsize != NULL)
			*bsize = FSBLOCKSIZE;
		return (fs.f_bavail);
	}
#endif
	return (-1);
}
/*
**  ENOUGHSPACE -- check to see if there is enough free space on the queue fs
**
**	Only implemented if you have statfs.
**
**	Parameters:
**		msize -- the size to check against.  If zero, we don't yet
**		know how big the message will be, so just check for
**		a "reasonable" amount.
**
**	Returns:
**		TRUE if there is enough space.
**		FALSE otherwise.
*/

bool
enoughspace(msize)
	long msize;
{
	long bfree, bsize;

	if (MinBlocksFree <= 0 && msize <= 0)
	{
		if (tTd(4, 80))
			printf("enoughspace: no threshold\n");
		return TRUE;
	}

	if ((bfree = freespace(QueueDir, &bsize)) >= 0)
	{
		if (tTd(4, 80))
			printf("enoughspace: bavail=%ld, need=%ld\n",
				bfree, msize);

		/* convert msize to block count */
		msize = msize / bsize + 1;
		if (MinBlocksFree >= 0)
			msize += MinBlocksFree;

		if (bfree < msize)
		{
#ifdef LOG
			if (LogLevel > 0)
				syslog(LOG_ALERT,
					"%s: low on space (have %ld, %s needs %ld in %s)",
					CurEnv->e_id, bfree,
					CurHostName, msize, QueueDir);
#endif
			return FALSE;
		}
	}
	else if (tTd(4, 80))
		printf("enoughspace failure: min=%ld, need=%ld: %s\n",
			MinBlocksFree, msize, errstring(errno));
	return TRUE;
}
/*
**  TRANSIENTERROR -- tell if an error code indicates a transient failure
**
**	This looks at an errno value and tells if this is likely to
**	go away if retried later.
**
**	Parameters:
**		err -- the errno code to classify.
**
**	Returns:
**		TRUE if this is probably transient.
**		FALSE otherwise.
*/

bool
transienterror(err)
	int err;
{
	switch (err)
	{
	  case EIO:			/* I/O error */
	  case ENXIO:			/* Device not configured */
	  case EAGAIN:			/* Resource temporarily unavailable */
	  case ENOMEM:			/* Cannot allocate memory */
	  case ENODEV:			/* Operation not supported by device */
	  case ENFILE:			/* Too many open files in system */
	  case EMFILE:			/* Too many open files */
	  case ENOSPC:			/* No space left on device */
#ifdef ETIMEDOUT
	  case ETIMEDOUT:		/* Connection timed out */
#endif
#ifdef ESTALE
	  case ESTALE:			/* Stale NFS file handle */
#endif
#ifdef ENETDOWN
	  case ENETDOWN:		/* Network is down */
#endif
#ifdef ENETUNREACH
	  case ENETUNREACH:		/* Network is unreachable */
#endif
#ifdef ENETRESET
	  case ENETRESET:		/* Network dropped connection on reset */
#endif
#ifdef ECONNABORTED
	  case ECONNABORTED:		/* Software caused connection abort */
#endif
#ifdef ECONNRESET
	  case ECONNRESET:		/* Connection reset by peer */
#endif
#ifdef ENOBUFS
	  case ENOBUFS:			/* No buffer space available */
#endif
#ifdef ESHUTDOWN
	  case ESHUTDOWN:		/* Can't send after socket shutdown */
#endif
#ifdef ECONNREFUSED
	  case ECONNREFUSED:		/* Connection refused */
#endif
#ifdef EHOSTDOWN
	  case EHOSTDOWN:		/* Host is down */
#endif
#ifdef EHOSTUNREACH
	  case EHOSTUNREACH:		/* No route to host */
#endif
#ifdef EDQUOT
	  case EDQUOT:			/* Disc quota exceeded */
#endif
#ifdef EPROCLIM
	  case EPROCLIM:		/* Too many processes */
#endif
#ifdef EUSERS
	  case EUSERS:			/* Too many users */
#endif
#ifdef EDEADLK
	  case EDEADLK:			/* Resource deadlock avoided */
#endif
#ifdef EISCONN
	  case EISCONN:			/* Socket already connected */
#endif
#ifdef EINPROGRESS
	  case EINPROGRESS:		/* Operation now in progress */
#endif
#ifdef EALREADY
	  case EALREADY:		/* Operation already in progress */
#endif
#ifdef EADDRINUSE
	  case EADDRINUSE:		/* Address already in use */
#endif
#ifdef EADDRNOTAVAIL
	  case EADDRNOTAVAIL:		/* Can't assign requested address */
#endif
#ifdef ETXTBSY
	  case ETXTBSY:			/* (Apollo) file locked */
#endif
#if defined(ENOSR) && (!defined(ENOBUFS) || (ENOBUFS != ENOSR))
	  case ENOSR:			/* Out of streams resources */
#endif
		return TRUE;
	}

	/* nope, must be permanent */
	return FALSE;
}
/*
**  LOCKFILE -- lock a file using flock or (shudder) fcntl locking
**
**	Parameters:
**		fd -- the file descriptor of the file.
**		filename -- the file name (for error messages).
**		ext -- the filename extension.
**		type -- type of the lock.  Bits can be:
**			LOCK_EX -- exclusive lock.
**			LOCK_NB -- non-blocking.
**
**	Returns:
**		TRUE if the lock was acquired.
**		FALSE otherwise.
*/

bool
lockfile(fd, filename, ext, type)
	int fd;
	char *filename;
	char *ext;
	int type;
{
# if !HASFLOCK
	int action;
	struct flock lfd;

	if (ext == NULL)
		ext = "";
		
	bzero(&lfd, sizeof lfd);
	if (bitset(LOCK_UN, type))
		lfd.l_type = F_UNLCK;
	else if (bitset(LOCK_EX, type))
		lfd.l_type = F_WRLCK;
	else
		lfd.l_type = F_RDLCK;

	if (bitset(LOCK_NB, type))
		action = F_SETLK;
	else
		action = F_SETLKW;

	if (tTd(55, 60))
		printf("lockfile(%s%s, action=%d, type=%d): ",
			filename, ext, action, lfd.l_type);

	if (fcntl(fd, action, &lfd) >= 0)
	{
		if (tTd(55, 60))
			printf("SUCCESS\n");
		return TRUE;
	}

	if (tTd(55, 60))
		printf("(%s) ", errstring(errno));

	/*
	**  On SunOS, if you are testing using -oQ/tmp/mqueue or
	**  -oA/tmp/aliases or anything like that, and /tmp is mounted
	**  as type "tmp" (that is, served from swap space), the
	**  previous fcntl will fail with "Invalid argument" errors.
	**  Since this is fairly common during testing, we will assume
	**  that this indicates that the lock is successfully grabbed.
	*/

	if (errno == EINVAL)
	{
		if (tTd(55, 60))
			printf("SUCCESS\n");
		return TRUE;
	}

	if (!bitset(LOCK_NB, type) || (errno != EACCES && errno != EAGAIN))
	{
		int omode = -1;
#  ifdef F_GETFL
		int oerrno = errno;

		(void) fcntl(fd, F_GETFL, &omode);
		errno = oerrno;
#  endif
		syserr("cannot lockf(%s%s, fd=%d, type=%o, omode=%o, euid=%d)",
			filename, ext, fd, type, omode, geteuid());
	}
# else
	if (ext == NULL)
		ext = "";

	if (tTd(55, 60))
		printf("lockfile(%s%s, type=%o): ", filename, ext, type);

	if (flock(fd, type) >= 0)
	{
		if (tTd(55, 60))
			printf("SUCCESS\n");
		return TRUE;
	}

	if (tTd(55, 60))
		printf("(%s) ", errstring(errno));

	if (!bitset(LOCK_NB, type) || errno != EWOULDBLOCK)
	{
		int omode = -1;
#  ifdef F_GETFL
		int oerrno = errno;

		(void) fcntl(fd, F_GETFL, &omode);
		errno = oerrno;
#  endif
		syserr("cannot flock(%s%s, fd=%d, type=%o, omode=%o, euid=%d)",
			filename, ext, fd, type, omode, geteuid());
	}
# endif
	if (tTd(55, 60))
		printf("FAIL\n");
	return FALSE;
}
/*
**  CHOWNSAFE -- tell if chown is "safe" (executable only by root)
**
**	Parameters:
**		fd -- the file descriptor to check.
**
**	Returns:
**		TRUE -- if only root can chown the file to an arbitrary
**			user.
**		FALSE -- if an arbitrary user can give away a file.
*/

#if defined(__FreeBSD__) && defined(_POSIX_CHOWN_RESTRICTED)
#	undef _POSIX_CHOWN_RESTRICTED
#	define _POSIX_CHOWN_RESTRICTED 1
#endif

bool
chownsafe(fd)
	int fd;
{
#ifdef __hpux
	char *s;
	int tfd;
	uid_t o_uid, o_euid;
	gid_t o_gid, o_egid;
	bool rval;
	struct stat stbuf;

	o_uid = getuid();
	o_euid = geteuid();
	o_gid = getgid();
	o_egid = getegid();
	fstat(fd, &stbuf);
	setresuid(stbuf.st_uid, stbuf.st_uid, -1);
	setresgid(stbuf.st_gid, stbuf.st_gid, -1);
	s = tmpnam(NULL);
	tfd = open(s, O_RDONLY|O_CREAT, 0600);
	rval = fchown(tfd, DefUid, DefGid) != 0;
	close(tfd);
	unlink(s);
	setreuid(o_uid, o_euid);
	setresgid(o_gid, o_egid, -1);
	return rval;
#else
# ifdef _POSIX_CHOWN_RESTRICTED
#  if _POSIX_CHOWN_RESTRICTED == -1
	return FALSE;
#  else
	return TRUE;
#  endif
# else
#  ifdef _PC_CHOWN_RESTRICTED
	return fpathconf(fd, _PC_CHOWN_RESTRICTED) > 0;
#  else
#   ifdef BSD
	return TRUE;
#   else
	return FALSE;
#   endif
#  endif
# endif
#endif
}
/*
**  GETCFNAME -- return the name of the .cf file.
**
**	Some systems (e.g., NeXT) determine this dynamically.
*/

char *
getcfname()
{
	if (ConfFile != NULL)
		return ConfFile;
#ifdef NETINFO
	{
		extern char *ni_propval();
		char *cflocation;

		cflocation = ni_propval("/locations/sendmail", "sendmail.cf");
		if (cflocation != NULL)
			return cflocation;
	}
#endif
	return _PATH_SENDMAILCF;
}
/*
**  SETVENDOR -- process vendor code from V configuration line
**
**	Parameters:
**		vendor -- string representation of vendor.
**
**	Returns:
**		TRUE -- if ok.
**		FALSE -- if vendor code could not be processed.
**
**	Side Effects:
**		It is reasonable to set mode flags here to tweak
**		processing in other parts of the code if necessary.
**		For example, if you are a vendor that uses $%y to
**		indicate YP lookups, you could enable that here.
*/

bool
setvendor(vendor)
	char *vendor;
{
	if (strcasecmp(vendor, "Berkeley") == 0)
		return TRUE;

	/* add vendor extensions here */

	return FALSE;
}
/*
**  STRTOL -- convert string to long integer
**
**	For systems that don't have it in the C library.
**
**	This is taken verbatim from the 4.4-Lite C library.
*/

#ifdef NEEDSTRTOL

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)strtol.c	8.1 (Berkeley) 6/4/93";
#endif /* LIBC_SCCS and not lint */

#include <limits.h>

/*
 * Convert a string to a long integer.
 *
 * Ignores `locale' stuff.  Assumes that the upper and lower case
 * alphabets and digits are each contiguous.
 */

long
strtol(nptr, endptr, base)
	const char *nptr;
	char **endptr;
	register int base;
{
	register const char *s = nptr;
	register unsigned long acc;
	register int c;
	register unsigned long cutoff;
	register int neg = 0, any, cutlim;

	/*
	 * Skip white space and pick up leading +/- sign if any.
	 * If base is 0, allow 0x for hex and 0 for octal, else
	 * assume decimal; if base is already 16, allow 0x.
	 */
	do {
		c = *s++;
	} while (isspace(c));
	if (c == '-') {
		neg = 1;
		c = *s++;
	} else if (c == '+')
		c = *s++;
	if ((base == 0 || base == 16) &&
	    c == '0' && (*s == 'x' || *s == 'X')) {
		c = s[1];
		s += 2;
		base = 16;
	}
	if (base == 0)
		base = c == '0' ? 8 : 10;

	/*
	 * Compute the cutoff value between legal numbers and illegal
	 * numbers.  That is the largest legal value, divided by the
	 * base.  An input number that is greater than this value, if
	 * followed by a legal input character, is too big.  One that
	 * is equal to this value may be valid or not; the limit
	 * between valid and invalid numbers is then based on the last
	 * digit.  For instance, if the range for longs is
	 * [-2147483648..2147483647] and the input base is 10,
	 * cutoff will be set to 214748364 and cutlim to either
	 * 7 (neg==0) or 8 (neg==1), meaning that if we have accumulated
	 * a value > 214748364, or equal but the next digit is > 7 (or 8),
	 * the number is too big, and we will return a range error.
	 *
	 * Set any if any `digits' consumed; make it negative to indicate
	 * overflow.
	 */
	cutoff = neg ? -(unsigned long)LONG_MIN : LONG_MAX;
	cutlim = cutoff % (unsigned long)base;
	cutoff /= (unsigned long)base;
	for (acc = 0, any = 0;; c = *s++) {
		if (isdigit(c))
			c -= '0';
		else if (isalpha(c))
			c -= isupper(c) ? 'A' - 10 : 'a' - 10;
		else
			break;
		if (c >= base)
			break;
		if (any < 0 || acc > cutoff || acc == cutoff && c > cutlim)
			any = -1;
		else {
			any = 1;
			acc *= base;
			acc += c;
		}
	}
	if (any < 0) {
		acc = neg ? LONG_MIN : LONG_MAX;
		errno = ERANGE;
	} else if (neg)
		acc = -acc;
	if (endptr != 0)
		*endptr = (char *)(any ? s - 1 : nptr);
	return (acc);
}

#endif
/*
**  SOLARIS_GETHOSTBY{NAME,ADDR} -- compatibility routines for gethostbyXXX
**
**	Solaris versions prior through 2.3 don't properly deliver a
**	canonical h_name field.  This tries to work around it.
*/

#ifdef SOLARIS

struct hostent *
solaris_gethostbyname(name)
	const char *name;
{
# ifdef SOLARIS_2_3
	static struct hostent hp;
	static char buf[1000];
	extern struct hostent *_switch_gethostbyname_r();

	return _switch_gethostbyname_r(name, &hp, buf, sizeof(buf), &h_errno);
# else
	extern struct hostent *__switch_gethostbyname();

	return __switch_gethostbyname(name);
# endif
}

struct hostent *
solaris_gethostbyaddr(addr, len, type)
	const char *addr;
	int len;
	int type;
{
# ifdef SOLARIS_2_3
	static struct hostent hp;
	static char buf[1000];
	extern struct hostent *_switch_gethostbyaddr_r();

	return _switch_gethostbyaddr_r(addr, len, type, &hp, buf, sizeof(buf), &h_errno);
# else
	extern struct hostent *__switch_gethostbyaddr();

	return __switch_gethostbyaddr(addr, len, type);
# endif
}

#endif
/*
**  NI_PROPVAL -- netinfo property value lookup routine
**
**	Parameters:
**		directory -- the Netinfo directory name.
**		propname -- the Netinfo property name.
**
**	Returns:
**		NULL -- if:
**			1. the directory is not found
**			2. the property name is not found
**			3. the property contains multiple values
**			4. some error occured
**		else -- the location of the config file.
**
**	Notes:
**      	Caller should free the return value of ni_proval
*/

#ifdef NETINFO

# include <netinfo/ni.h>

# define LOCAL_NETINFO_DOMAIN    "."
# define PARENT_NETINFO_DOMAIN   ".."
# define MAX_NI_LEVELS           256

char *
ni_propval(directory, propname)
	char *directory;
	char *propname;
{
	char *propval = NULL;
	int i;
	void *ni = NULL;
	void *lastni = NULL;
	ni_status nis;
	ni_id nid;
	ni_namelist ninl;

	/*
	**  If the passed directory and property name are found
	**  in one of netinfo domains we need to search (starting
	**  from the local domain moving all the way back to the
	**  root domain) set propval to the property's value
	**  and return it.
	*/

	for (i = 0; i < MAX_NI_LEVELS; ++i)
	{
		if (i == 0)
		{
			nis = ni_open(NULL, LOCAL_NETINFO_DOMAIN, &ni);
		}
		else
		{
			if (lastni != NULL)
				ni_free(lastni);
			lastni = ni;
			nis = ni_open(lastni, PARENT_NETINFO_DOMAIN, &ni);
		}

		/*
		**  Don't bother if we didn't get a handle on a
		**  proper domain.  This is not necessarily an error.
		**  We would get a positive ni_status if, for instance
		**  we never found the directory or property and tried
		**  to open the parent of the root domain!
		*/

		if (nis != 0)
			break;

		/*
		**  Find the path to the server information.
		*/

		if (ni_pathsearch(ni, &nid, directory) != 0)
			continue;

		/*
		**  Find "host" information.
		*/

		if (ni_lookupprop(ni, &nid, propname, &ninl) != 0)
			continue;

		/*
		**  If there's only one name in
		**  the list, assume we've got
		**  what we want.
		*/

		if (ninl.ni_namelist_len == 1)
		{
			propval = ni_name_dup(ninl.ni_namelist_val[0]);
			break;
		}
	}

	/*
	**  Clean up.
	*/

	if (ni != NULL)
		ni_free(ni);
	if (lastni != NULL && ni != lastni)
		ni_free(lastni);

	return propval;
}

#endif /* NETINFO */
/*
**  HARD_SYSLOG -- call syslog repeatedly until it works
**
**	Needed on HP-UX, which apparently doesn't guarantee that
**	syslog succeeds during interrupt handlers.
*/

#ifdef __hpux

# define MAXSYSLOGTRIES	100
# undef syslog

# ifdef __STDC__
hard_syslog(int pri, char *msg, ...)
# else
hard_syslog(pri, msg, va_alist)
	int pri;
	char *msg;
	va_dcl
# endif
{
	int i;
	char buf[SYSLOG_BUFSIZE * 2];
	VA_LOCAL_DECL;

	VA_START(msg);
	vsprintf(buf, msg, ap);
	VA_END;

	for (i = MAXSYSLOGTRIES; --i >= 0 && syslog(pri, "%s", buf) < 0; )
		continue;
}

#endif
