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
static char sccsid[] = "@(#)conf.c	8.3 (Berkeley) 7/13/93";
#endif /* not lint */

# include <sys/ioctl.h>
# include <sys/param.h>
# include <signal.h>
# include <pwd.h>
# include "sendmail.h"
# include "pathnames.h"

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
	"return-receipt-to",	H_FROM /* |H_RECEIPTTO */,
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

char	*ConfFile =	_PATH_SENDMAILCF;	/* runtime configuration */
char	*FreezeFile =	_PATH_SENDMAILFC;	/* frozen version of above */
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
	"restrictmailq",	PRIV_RESTRMAILQ,
	"authwarnings",		PRIV_AUTHWARNINGS,
	"goaway",		PRIV_GOAWAY,
	NULL,			0,
};



/*
**  Miscellaneous stuff.
*/

int	DtableSize =	50;		/* max open files; reset in 4.2bsd */
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
# endif lint
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
#define LA_INT		2	/* read kmem for avenrun; interpret as int */
#define LA_FLOAT	3	/* read kmem for avenrun; interpret as float */
#define LA_SUBR		4	/* call getloadavg */

#ifndef LA_TYPE
#  if defined(sun) && !defined(BSD)
#    define LA_TYPE		LA_INT
#  endif
#  if defined(mips) || defined(__alpha)
     /* Ultrix or OSF/1 or RISC/os */
#    define LA_TYPE		LA_INT
#    define LA_AVENRUN		"avenrun"
#  endif
#  if defined(__hpux)
#    define LA_TYPE		LA_FLOAT
#    define LA_AVENRUN		"avenrun"
#  endif
#  if defined(__NeXT__)
#    define LA_TYPE		LA_ZERO
#  endif

/* now do the guesses based on general OS type */
#  ifndef LA_TYPE
#   if defined(SYSTEM5)
#    define LA_TYPE		LA_INT
#    define LA_AVENRUN		"avenrun"
#   else
#    if defined(BSD)
#     define LA_TYPE		LA_SUBR
#    else
#     define LA_TYPE		LA_ZERO
#    endif
#   endif
#  endif
#endif

#if (LA_TYPE == LA_INT) || (LA_TYPE == LA_FLOAT)

#include <nlist.h>

#ifndef LA_AVENRUN
#define LA_AVENRUN	"_avenrun"
#endif

/* _PATH_UNIX should be defined in <paths.h> */
#ifndef _PATH_UNIX
#  if defined(__hpux)
#    define _PATH_UNIX		"/hp-ux"
#  endif
#  if defined(mips) && !defined(ultrix)
     /* powerful RISC/os */
#    define _PATH_UNIX		"/unix"
#  endif
#  if defined(Solaris2)
     /* Solaris 2 */
#    define _PATH_UNIX		"/kernel/unix"
#  endif
#  if defined(SYSTEM5)
#    ifndef _PATH_UNIX
#      define _PATH_UNIX	"/unix"
#    endif
#  endif
#  ifndef _PATH_UNIX
#    define _PATH_UNIX		"/vmunix"
#  endif
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

# if (LA_TYPE == LA_INT)
#  define FSHIFT	8
# endif
#endif

#if (LA_TYPE == LA_INT) && !defined(FSCALE)
#  define FSCALE	(1 << FSHIFT)
#endif

getla()
{
	static int kmem = -1;
#if LA_TYPE == LA_INT
	long avenrun[3];
#else
	double avenrun[3];
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
#if LA_TYPE == LA_INT
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

#else

getla()
{
	if (tTd(3, 1))
		printf("getla: ZERO\n");
	return (0);
}

#endif
#endif
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
# ifdef __hpux
#  include <sys/pstat.h>
# endif
# ifdef BSD4_4
#  include <machine/vmparam.h>
#  include <sys/exec.h>
#  ifdef PS_STRINGS
#   define SETPROC_STATIC static
#  endif
# endif
# ifndef SETPROC_STATIC
#  define SETPROC_STATIC
# endif
#endif

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
		*p++ = ' ';
#   endif
#  endif
# endif /* SETPROCTITLE */
}
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

# include <sys/wait.h>

void
reapchild()
{
# if defined(WIFEXITED) && !defined(__NeXT__)
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
# ifdef SYSTEM5
	(void) signal(SIGCHLD, reapchild);
# endif
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

#ifdef UNSETENV

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

#endif /* UNSETENV */
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

# if defined(_SC_OPEN_MAX) && !defined(NO_SYSCONF)
	return sysconf(_SC_OPEN_MAX);
# else
#  ifdef HASGETDTABLESIZE
	return getdtablesize();
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
# if !defined(SYSTEM5) || defined(__hpux)
#  define HASINITGROUPS
# endif
#endif

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
# ifdef SYSTEM5
	return setpgrp();
# else
	return 0;
# endif
}

#endif
/*
**  ENOUGHSPACE -- check to see if there is enough free space on the queue fs
**
**	Only implemented if you have statfs.
**
**	Parameters:
**		msize -- the size to check against.  If zero, we don't yet
**			know how big the message will be, so just check for
**			a "reasonable" amount.
**
**	Returns:
**		TRUE if there is enough space.
**		FALSE otherwise.
*/

#ifndef HASSTATFS
# if defined(BSD4_4) || defined(__osf__)
#  define HASSTATFS
# endif
#endif

#ifdef HASSTATFS
# undef HASUSTAT
#endif

#if defined(HASUSTAT)
# include <ustat.h>
#endif

#ifdef HASSTATFS
# if defined(sgi) || defined(apollo)
#  include <sys/statfs.h>
# else
#  if (defined(sun) && !defined(BSD)) || defined(__hpux)
#   include <sys/vfs.h>
#  else
#   include <sys/mount.h>
#  endif
# endif
#endif

bool
enoughspace(msize)
	long msize;
{
#if defined(HASSTATFS) || defined(HASUSTAT)
# if defined(HASUSTAT)
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
#  endif
# endif
	extern int errno;

	if (MinBlocksFree <= 0 && msize <= 0)
	{
		if (tTd(4, 80))
			printf("enoughspace: no threshold\n");
		return TRUE;
	}

# if defined(HASUSTAT)
	if (stat(QueueDir, &statbuf) == 0 && ustat(statbuf.st_dev, &fs) == 0)
# else
#  if defined(sgi) || defined(apollo)
	if (statfs(QueueDir, &fs, sizeof fs, 0) == 0)
#  else
#   if defined(ultrix)
	if (statfs(QueueDir, &fs) > 0)
#   else
	if (statfs(QueueDir, &fs) == 0)
#   endif
#  endif
# endif
	{
		if (tTd(4, 80))
			printf("enoughspace: bavail=%ld, need=%ld\n",
				fs.f_bavail, msize);

		/* convert msize to block count */
		msize = msize / FSBLOCKSIZE + 1;
		if (MinBlocksFree >= 0)
			msize += MinBlocksFree;

		if (fs.f_bavail < msize)
		{
#ifdef LOG
			if (LogLevel > 0)
				syslog(LOG_ALERT, "%s: low on space (have %ld, need %ld)",
					QueueDir, fs.f_bavail, msize);
#endif
			return FALSE;
		}
	}
	else if (tTd(4, 80))
		printf("enoughspace failure: min=%ld, need=%ld: %s\n",
			MinBlocksFree, msize, errstring(errno));
#endif
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
#ifdef ENOSR
	  case ENOSR:			/* Out of streams resources */
#endif
		return TRUE;
	}

	/* nope, must be permanent */
	return FALSE;
}
/*
**  LOCKFILE -- lock a file using flock or (shudder) lockf
**
**	Parameters:
**		fd -- the file descriptor of the file.
**		filename -- the file name (for error messages).
**		type -- type of the lock.  Bits can be:
**			LOCK_EX -- exclusive lock.
**			LOCK_NB -- non-blocking.
**
**	Returns:
**		TRUE if the lock was acquired.
**		FALSE otherwise.
*/

bool
lockfile(fd, filename, type)
	int fd;
	char *filename;
	int type;
{
# ifdef LOCKF
	int action;
	struct flock lfd;

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

	lfd.l_whence = lfd.l_start = lfd.l_len = 0;

	if (fcntl(fd, action, &lfd) >= 0)
		return TRUE;

	if (!bitset(LOCK_NB, type) || (errno != EACCES && errno != EAGAIN))
		syserr("cannot lockf(%s, %o)", filename, type);
# else
	if (flock(fd, type) >= 0)
		return TRUE;

	if (!bitset(LOCK_NB, type) || errno != EWOULDBLOCK)
		syserr("cannot flock(%s, %o)", filename, type);
# endif
	return FALSE;
}
