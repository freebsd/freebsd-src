/*
 * Copyright (c) 1998-2000 Sendmail, Inc. and its suppliers.
 *	All rights reserved.
 * Copyright (c) 1983, 1995-1997 Eric P. Allman.  All rights reserved.
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 */

#ifndef lint
static char id[] = "@(#)$Id: conf.c,v 8.646.2.2.2.32 2000/09/23 00:31:33 ca Exp $";
#endif /* ! lint */

#include <sendmail.h>
#include <sendmail/pathnames.h>

# include <sys/ioctl.h>
# include <sys/param.h>

#include <limits.h>
#if NETINET || NETINET6
# include <arpa/inet.h>
#endif /* NETINET || NETINET6 */
#if HASULIMIT && defined(HPUX11)
# include <ulimit.h>
#endif /* HASULIMIT && defined(HPUX11) */


static void	setupmaps __P((void));
static void	setupmailers __P((void));
static int	get_num_procs_online __P((void));


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
		/* originator fields, most to least significant */
	{ "resent-sender",		H_FROM|H_RESENT,	NULL	},
	{ "resent-from",		H_FROM|H_RESENT,	NULL	},
	{ "resent-reply-to",		H_FROM|H_RESENT,	NULL	},
	{ "sender",			H_FROM,			NULL	},
	{ "from",			H_FROM,			NULL	},
	{ "reply-to",			H_FROM,			NULL	},
	{ "errors-to",			H_FROM|H_ERRORSTO,	NULL	},
	{ "full-name",			H_ACHECK,		NULL	},
	{ "return-receipt-to",		H_RECEIPTTO,		NULL	},

		/* destination fields */
	{ "to",				H_RCPT,			NULL	},
	{ "resent-to",			H_RCPT|H_RESENT,	NULL	},
	{ "cc",				H_RCPT,			NULL	},
	{ "resent-cc",			H_RCPT|H_RESENT,	NULL	},
	{ "bcc",			H_RCPT|H_BCC,		NULL	},
	{ "resent-bcc",			H_RCPT|H_BCC|H_RESENT,	NULL	},
	{ "apparently-to",		H_RCPT,			NULL	},

		/* message identification and control */
	{ "message-id",			0,			NULL	},
	{ "resent-message-id",		H_RESENT,		NULL	},
	{ "message",			H_EOH,			NULL	},
	{ "text",			H_EOH,			NULL	},

		/* date fields */
	{ "date",			0,			NULL	},
	{ "resent-date",		H_RESENT,		NULL	},

		/* trace fields */
	{ "received",			H_TRACE|H_FORCE,	NULL	},
	{ "x400-received",		H_TRACE|H_FORCE,	NULL	},
	{ "via",			H_TRACE|H_FORCE,	NULL	},
	{ "mail-from",			H_TRACE|H_FORCE,	NULL	},

		/* miscellaneous fields */
	{ "comments",			H_FORCE|H_ENCODABLE,	NULL	},
	{ "return-path",		H_FORCE|H_ACHECK|H_BINDLATE,	NULL	},
	{ "content-transfer-encoding",	H_CTE,			NULL	},
	{ "content-type",		H_CTYPE,		NULL	},
	{ "content-length",		H_ACHECK,		NULL	},
	{ "subject",			H_ENCODABLE,		NULL	},
	{ "x-authentication-warning",	H_FORCE,		NULL	},

	{ NULL,				0,			NULL	}
};



/*
**  Privacy values
*/

struct prival PrivacyValues[] =
{
	{ "public",		PRIV_PUBLIC		},
	{ "needmailhelo",	PRIV_NEEDMAILHELO	},
	{ "needexpnhelo",	PRIV_NEEDEXPNHELO	},
	{ "needvrfyhelo",	PRIV_NEEDVRFYHELO	},
	{ "noexpn",		PRIV_NOEXPN		},
	{ "novrfy",		PRIV_NOVRFY		},
	{ "restrictmailq",	PRIV_RESTRICTMAILQ	},
	{ "restrictqrun",	PRIV_RESTRICTQRUN	},
	{ "noetrn",		PRIV_NOETRN		},
	{ "noverb",		PRIV_NOVERB		},
	{ "authwarnings",	PRIV_AUTHWARNINGS	},
	{ "noreceipts",		PRIV_NORECEIPTS		},
	{ "nobodyreturn",	PRIV_NOBODYRETN		},
	{ "goaway",		PRIV_GOAWAY		},
	{ NULL,			0			}
};

/*
**  DontBlameSendmail values
*/
struct dbsval DontBlameSendmailValues[] =
{
	{ "safe",			DBS_SAFE			},
	{ "assumesafechown",		DBS_ASSUMESAFECHOWN		},
	{ "groupwritabledirpathsafe",	DBS_GROUPWRITABLEDIRPATHSAFE	},
	{ "groupwritableforwardfilesafe",
					DBS_GROUPWRITABLEFORWARDFILESAFE },
	{ "groupwritableincludefilesafe",
					DBS_GROUPWRITABLEINCLUDEFILESAFE },
	{ "groupwritablealiasfile",	DBS_GROUPWRITABLEALIASFILE	},
	{ "worldwritablealiasfile",	DBS_WORLDWRITABLEALIASFILE	},
	{ "forwardfileinunsafedirpath",	DBS_FORWARDFILEINUNSAFEDIRPATH	},
	{ "includefileinunsafedirpath",	DBS_INCLUDEFILEINUNSAFEDIRPATH	},
	{ "mapinunsafedirpath",		DBS_MAPINUNSAFEDIRPATH	},
	{ "linkedaliasfileinwritabledir",
					DBS_LINKEDALIASFILEINWRITABLEDIR },
	{ "linkedclassfileinwritabledir",
					DBS_LINKEDCLASSFILEINWRITABLEDIR },
	{ "linkedforwardfileinwritabledir",
					DBS_LINKEDFORWARDFILEINWRITABLEDIR },
	{ "linkedincludefileinwritabledir",
					DBS_LINKEDINCLUDEFILEINWRITABLEDIR },
	{ "linkedmapinwritabledir",	DBS_LINKEDMAPINWRITABLEDIR	},
	{ "linkedserviceswitchfileinwritabledir",
					DBS_LINKEDSERVICESWITCHFILEINWRITABLEDIR },
	{ "filedeliverytohardlink",	DBS_FILEDELIVERYTOHARDLINK	},
	{ "filedeliverytosymlink",	DBS_FILEDELIVERYTOSYMLINK	},
	{ "writemaptohardlink",		DBS_WRITEMAPTOHARDLINK		},
	{ "writemaptosymlink",		DBS_WRITEMAPTOSYMLINK		},
	{ "writestatstohardlink",	DBS_WRITESTATSTOHARDLINK	},
	{ "writestatstosymlink",	DBS_WRITESTATSTOSYMLINK		},
	{ "forwardfileingroupwritabledirpath",
					DBS_FORWARDFILEINGROUPWRITABLEDIRPATH },
	{ "includefileingroupwritabledirpath",
					DBS_INCLUDEFILEINGROUPWRITABLEDIRPATH },
	{ "classfileinunsafedirpath",	DBS_CLASSFILEINUNSAFEDIRPATH	},
	{ "errorheaderinunsafedirpath",	DBS_ERRORHEADERINUNSAFEDIRPATH	},
	{ "helpfileinunsafedirpath",	DBS_HELPFILEINUNSAFEDIRPATH	},
	{ "forwardfileinunsafedirpathsafe",
					DBS_FORWARDFILEINUNSAFEDIRPATHSAFE },
	{ "includefileinunsafedirpathsafe",
					DBS_INCLUDEFILEINUNSAFEDIRPATHSAFE },
	{ "runprograminunsafedirpath",	DBS_RUNPROGRAMINUNSAFEDIRPATH	},
	{ "runwritableprogram",		DBS_RUNWRITABLEPROGRAM		},
	{ "nonrootsafeaddr",		DBS_NONROOTSAFEADDR		},
	{ "truststickybit",		DBS_TRUSTSTICKYBIT		},
	{ "dontwarnforwardfileinunsafedirpath",
					DBS_DONTWARNFORWARDFILEINUNSAFEDIRPATH },
	{ "insufficiententropy",	DBS_INSUFFICIENTENTROPY },
#if _FFR_UNSAFE_SASL
	{ "groupreadablesaslfile",	DBS_GROUPREADABLESASLFILE	},
#endif /* _FFR_UNSAFE_SASL */
#if _FFR_UNSAFE_WRITABLE_INCLUDE
	{ "groupwritableforwardfile",	DBS_GROUPWRITABLEFORWARDFILE	},
	{ "groupwritableincludefile",	DBS_GROUPWRITABLEINCLUDEFILE	},
	{ "worldwritableforwardfile",	DBS_WORLDWRITABLEFORWARDFILE	},
	{ "worldwritableincludefile",	DBS_WORLDWRITABLEINCLUDEFILE	},
#endif /* _FFR_UNSAFE_WRITABLE_INCLUDE */
	{ NULL,				0				}
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

#define MINUTES		* 60
#define HOURS		* 60 MINUTES
#define DAYS		* 24 HOURS

#ifndef MAXRULERECURSION
# define MAXRULERECURSION	50	/* max ruleset recursion depth */
#endif /* ! MAXRULERECURSION */

void
setdefaults(e)
	register ENVELOPE *e;
{
	int i;
	int numprocs;
	struct passwd *pw;

	numprocs = get_num_procs_online();
	SpaceSub = ' ';				/* option B */
	QueueLA = 8 * numprocs;			/* option x */
	RefuseLA = 12 * numprocs;		/* option X */
	WkRecipFact = 30000L;			/* option y */
	WkClassFact = 1800L;			/* option z */
	WkTimeFact = 90000L;			/* option Z */
	QueueFactor = WkRecipFact * 20;		/* option q */
	FileMode = (RealUid != geteuid()) ? 0644 : 0600;
						/* option F */
#if _FFR_QUEUE_FILE_MODE
	QueueFileMode = (RealUid != geteuid()) ? 0644 : 0600;
						/* option QueueFileMode */
#endif /* _FFR_QUEUE_FILE_MODE */

	if (((pw = sm_getpwnam("mailnull")) != NULL && pw->pw_uid != 0) ||
	    ((pw = sm_getpwnam("sendmail")) != NULL && pw->pw_uid != 0) ||
	    ((pw = sm_getpwnam("daemon")) != NULL && pw->pw_uid != 0))
	{
		DefUid = pw->pw_uid;		/* option u */
		DefGid = pw->pw_gid;		/* option g */
		DefUser = newstr(pw->pw_name);
	}
	else
	{
		DefUid = 1;			/* option u */
		DefGid = 1;			/* option g */
		setdefuser();
	}
	TrustedUid = 0;
	if (tTd(37, 4))
		dprintf("setdefaults: DefUser=%s, DefUid=%d, DefGid=%d\n",
			DefUser != NULL ? DefUser : "<1:1>",
			(int) DefUid, (int) DefGid);
	CheckpointInterval = 10;		/* option C */
	MaxHopCount = 25;			/* option h */
	set_delivery_mode(SM_FORK, e);		/* option d */
	e->e_errormode = EM_PRINT;		/* option e */
	e->e_queuedir = NOQDIR;
	e->e_ctime = curtime();
	SevenBitInput = FALSE;			/* option 7 */
	MaxMciCache = 1;			/* option k */
	MciCacheTimeout = 5 MINUTES;		/* option K */
	LogLevel = 9;				/* option L */
	inittimeouts(NULL, FALSE);		/* option r */
	PrivacyFlags = PRIV_PUBLIC;		/* option p */
	MeToo = TRUE;				/* option m */
	SendMIMEErrors = TRUE;			/* option f */
	SuperSafe = TRUE;			/* option s */
	clrbitmap(DontBlameSendmail);		/* DontBlameSendmail option */
#if MIME8TO7
	MimeMode = MM_CVTMIME|MM_PASS8BIT;	/* option 8 */
#else /* MIME8TO7 */
	MimeMode = MM_PASS8BIT;
#endif /* MIME8TO7 */
	for (i = 0; i < MAXTOCLASS; i++)
	{
		TimeOuts.to_q_return[i] = 5 DAYS;	/* option T */
		TimeOuts.to_q_warning[i] = 0;		/* option T */
	}
	ServiceSwitchFile = "/etc/mail/service.switch";
	ServiceCacheMaxAge = (time_t) 10;
	HostsFile = _PATH_HOSTS;
	PidFile = newstr(_PATH_SENDMAILPID);
	MustQuoteChars = "@,;:\\()[].'";
	MciInfoTimeout = 30 MINUTES;
	MaxRuleRecursion = MAXRULERECURSION;
	MaxAliasRecursion = 10;
	MaxMacroRecursion = 10;
	ColonOkInAddr = TRUE;
	DontLockReadFiles = TRUE;
	DoubleBounceAddr = "postmaster";
	MaxHeadersLength = MAXHDRSLEN;
	MaxForwardEntries = 0;
#if SASL
	AuthMechanisms = newstr(AUTH_MECHANISMS);
#endif /* SASL */
#ifdef HESIOD_INIT
	HesiodContext = NULL;
#endif /* HESIOD_INIT */
#if NETINET6
	/* Detect if IPv6 is available at run time */
	i = socket(AF_INET6, SOCK_STREAM, 0);
	if (i >= 0)
	{
		InetMode = AF_INET6;
		(void) close(i);
	}
	else
		InetMode = AF_INET;
#else /* NETINET6 */
	InetMode = AF_INET;
#endif /* NETINET6 */
	ControlSocketName = NULL;
	memset(&ConnectOnlyTo, '\0', sizeof ConnectOnlyTo);
	DataFileBufferSize = 4096;
	XscriptFileBufferSize = 4096;
	for (i = 0; i < MAXRWSETS; i++)
		RuleSetNames[i] = NULL;
#if _FFR_MILTER
	InputFilters[0] = NULL;
#endif /* _FFR_MILTER */
	setupmaps();
	setupmailers();
	setupheaders();
}


/*
**  SETDEFUSER -- set/reset DefUser using DefUid (for initgroups())
*/

void
setdefuser()
{
	struct passwd *defpwent;
	static char defuserbuf[40];

	DefUser = defuserbuf;
	defpwent = sm_getpwuid(DefUid);
	snprintf(defuserbuf, sizeof defuserbuf, "%s",
		defpwent == NULL ? "nobody" : defpwent->pw_name);
	if (tTd(37, 4))
		dprintf("setdefuser: DefUid=%d, DefUser=%s\n",
		       (int) DefUid, DefUser);
}
/*
**  SETUPMAILERS -- initialize default mailers
*/

static void
setupmailers()
{
	char buf[100];

	(void) strlcpy(buf, "prog, P=/bin/sh, F=lsoDq9, T=X-Unix/X-Unix/X-Unix, A=sh -c \201u",
		sizeof buf);
	makemailer(buf);

	(void) strlcpy(buf, "*file*, P=[FILE], F=lsDFMPEouq9, T=X-Unix/X-Unix/X-Unix, A=FILE \201u",
		sizeof buf);
	makemailer(buf);

	(void) strlcpy(buf, "*include*, P=/dev/null, F=su, A=INCLUDE \201u",
		sizeof buf);
	makemailer(buf);
	initerrmailers();
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

static void
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
#endif /* NEWDB */

#ifdef NDBM
	MAPDEF("dbm", ".dir", MCF_ALIASOK|MCF_REBUILDABLE,
		map_parseargs, ndbm_map_open, ndbm_map_close,
		ndbm_map_lookup, ndbm_map_store);
#endif /* NDBM */

#ifdef NIS
	MAPDEF("nis", NULL, MCF_ALIASOK,
		map_parseargs, nis_map_open, null_map_close,
		nis_map_lookup, null_map_store);
#endif /* NIS */

#ifdef NISPLUS
	MAPDEF("nisplus", NULL, MCF_ALIASOK,
		map_parseargs, nisplus_map_open, null_map_close,
		nisplus_map_lookup, null_map_store);
#endif /* NISPLUS */

#ifdef LDAPMAP
	MAPDEF("ldap", NULL, MCF_ALIASOK,
		ldapmap_parseargs, ldapmap_open, ldapmap_close,
		ldapmap_lookup, null_map_store);

	/* Deprecated */
	MAPDEF("ldapx", NULL, MCF_ALIASOK,
		ldapx_map_parseargs, ldapmap_open, ldapmap_close,
		ldapmap_lookup, null_map_store);
#endif /* LDAPMAP */

#ifdef PH_MAP
	MAPDEF("ph", NULL, 0,
		ph_map_parseargs, ph_map_open, ph_map_close,
		ph_map_lookup, null_map_store);
#endif /* PH_MAP */

#if MAP_NSD
	/* IRIX 6.5 nsd support */
	MAPDEF("nsd", NULL, MCF_ALIASOK,
	       map_parseargs, null_map_open, null_map_close,
	       nsd_map_lookup, null_map_store);
#endif /* MAP_NSD */

#ifdef HESIOD
	MAPDEF("hesiod", NULL, MCF_ALIASOK|MCF_ALIASONLY,
		map_parseargs, hes_map_open, null_map_close,
		hes_map_lookup, null_map_store);
#endif /* HESIOD */

#if NETINFO
	MAPDEF("netinfo", NULL, MCF_ALIASOK,
		map_parseargs, ni_map_open, null_map_close,
		ni_map_lookup, null_map_store);
#endif /* NETINFO */

#if 0
	MAPDEF("dns", NULL, 0,
		dns_map_init, null_map_open, null_map_close,
		dns_map_lookup, null_map_store);
#endif /* 0 */

#if NAMED_BIND
	/* best MX DNS lookup */
	MAPDEF("bestmx", NULL, MCF_OPTFILE,
		map_parseargs, null_map_open, null_map_close,
		bestmx_map_lookup, null_map_store);
#endif /* NAMED_BIND */

	MAPDEF("host", NULL, 0,
		host_map_init, null_map_open, null_map_close,
		host_map_lookup, null_map_store);

	MAPDEF("text", NULL, MCF_ALIASOK,
		map_parseargs, text_map_open, null_map_close,
		text_map_lookup, null_map_store);

	MAPDEF("stab", NULL, MCF_ALIASOK|MCF_ALIASONLY,
		map_parseargs, stab_map_open, null_map_close,
		stab_map_lookup, stab_map_store);

	MAPDEF("implicit", NULL, MCF_ALIASOK|MCF_ALIASONLY|MCF_REBUILDABLE,
		map_parseargs, impl_map_open, impl_map_close,
		impl_map_lookup, impl_map_store);

	/* access to system passwd file */
	MAPDEF("user", NULL, MCF_OPTFILE,
		map_parseargs, user_map_open, null_map_close,
		user_map_lookup, null_map_store);

	/* dequote map */
	MAPDEF("dequote", NULL, 0,
		dequote_init, null_map_open, null_map_close,
		dequote_map, null_map_store);

#ifdef MAP_REGEX
	MAPDEF("regex", NULL, 0,
		regex_map_init, null_map_open, null_map_close,
		regex_map_lookup, null_map_store);
#endif /* MAP_REGEX */

#if USERDB
	/* user database */
	MAPDEF("userdb", ".db", 0,
		map_parseargs, null_map_open, null_map_close,
		udb_map_lookup, null_map_store);
#endif /* USERDB */

	/* arbitrary programs */
	MAPDEF("program", NULL, MCF_ALIASOK,
		map_parseargs, null_map_open, null_map_close,
		prog_map_lookup, null_map_store);

	/* sequenced maps */
	MAPDEF("sequence", NULL, MCF_ALIASOK,
		seq_map_parse, null_map_open, null_map_close,
		seq_map_lookup, seq_map_store);

	/* switched interface to sequenced maps */
	MAPDEF("switch", NULL, MCF_ALIASOK,
		map_parseargs, switch_map_open, null_map_close,
		seq_map_lookup, seq_map_store);

	/* null map lookup -- really for internal use only */
	MAPDEF("null", NULL, MCF_ALIASOK|MCF_OPTFILE,
		map_parseargs, null_map_open, null_map_close,
		null_map_lookup, null_map_store);

	/* syslog map -- logs information to syslog */
	MAPDEF("syslog", NULL, 0,
		syslog_map_parseargs, null_map_open, null_map_close,
		syslog_map_lookup, null_map_store);

	/* macro storage map -- rulesets can set macros */
	MAPDEF("macro", NULL, 0,
		dequote_init, null_map_open, null_map_close,
		macro_map_lookup, null_map_store);

	/* arithmetic map -- add/subtract/compare */
	MAPDEF("arith", NULL, 0,
		dequote_init, null_map_open, null_map_close,
		arith_map_lookup, null_map_store);

	if (tTd(38, 2))
	{
		/* bogus map -- always return tempfail */
		MAPDEF("bogus",	NULL, MCF_ALIASOK|MCF_OPTFILE,
		       map_parseargs, null_map_open, null_map_close,
		       bogus_map_lookup, null_map_store);
	}
}

#undef MAPDEF
/*
**  INITHOSTMAPS -- initial host-dependent maps
**
**	This should act as an interface to any local service switch
**	provided by the host operating system.
**
**	Parameters:
**		none
**
**	Returns:
**		none
**
**	Side Effects:
**		Should define maps "host" and "users" as necessary
**		for this OS.  If they are not defined, they will get
**		a default value later.  It should check to make sure
**		they are not defined first, since it's possible that
**		the config file has provided an override.
*/

void
inithostmaps()
{
	register int i;
	int nmaps;
	char *maptype[MAXMAPSTACK];
	short mapreturn[MAXMAPACTIONS];
	char buf[MAXLINE];

	/*
	**  Set up default hosts maps.
	*/

#if 0
	nmaps = switch_map_find("hosts", maptype, mapreturn);
	for (i = 0; i < nmaps; i++)
	{
		if (strcmp(maptype[i], "files") == 0 &&
		    stab("hosts.files", ST_MAP, ST_FIND) == NULL)
		{
			(void) strlcpy(buf, "hosts.files text -k 0 -v 1 /etc/hosts",
				sizeof buf);
			(void) makemapentry(buf);
		}
# if NAMED_BIND
		else if (strcmp(maptype[i], "dns") == 0 &&
		    stab("hosts.dns", ST_MAP, ST_FIND) == NULL)
		{
			(void) strlcpy(buf, "hosts.dns dns A", sizeof buf);
			(void) makemapentry(buf);
		}
# endif /* NAMED_BIND */
# ifdef NISPLUS
		else if (strcmp(maptype[i], "nisplus") == 0 &&
		    stab("hosts.nisplus", ST_MAP, ST_FIND) == NULL)
		{
			(void) strlcpy(buf, "hosts.nisplus nisplus -k name -v address hosts.org_dir",
				sizeof buf);
			(void) makemapentry(buf);
		}
# endif /* NISPLUS */
# ifdef NIS
		else if (strcmp(maptype[i], "nis") == 0 &&
		    stab("hosts.nis", ST_MAP, ST_FIND) == NULL)
		{
			(void) strlcpy(buf, "hosts.nis nis -k 0 -v 1 hosts.byname",
				sizeof buf);
			(void) makemapentry(buf);
		}
# endif /* NIS */
# if NETINFO
		else if (strcmp(maptype[i], "netinfo") == 0) &&
		    stab("hosts.netinfo", ST_MAP, ST_FIND) == NULL)
		{
			(void) strlcpy(buf, "hosts.netinfo netinfo -v name /machines",
				sizeof buf);
			(void) makemapentry(buf);
		}
# endif /* NETINFO */
	}
#endif /* 0 */

	/*
	**  Make sure we have a host map.
	*/

	if (stab("host", ST_MAP, ST_FIND) == NULL)
	{
		/* user didn't initialize: set up host map */
		(void) strlcpy(buf, "host host", sizeof buf);
#if NAMED_BIND
		if (ConfigLevel >= 2)
			(void) strlcat(buf, " -a. -D", sizeof buf);
#endif /* NAMED_BIND */
		(void) makemapentry(buf);
	}

	/*
	**  Set up default aliases maps
	*/

	nmaps = switch_map_find("aliases", maptype, mapreturn);
	for (i = 0; i < nmaps; i++)
	{
		if (strcmp(maptype[i], "files") == 0 &&
		    stab("aliases.files", ST_MAP, ST_FIND) == NULL)
		{
			(void) strlcpy(buf, "aliases.files null", sizeof buf);
			(void) makemapentry(buf);
		}
#ifdef NISPLUS
		else if (strcmp(maptype[i], "nisplus") == 0 &&
		    stab("aliases.nisplus", ST_MAP, ST_FIND) == NULL)
		{
			(void) strlcpy(buf, "aliases.nisplus nisplus -kalias -vexpansion mail_aliases.org_dir",
				sizeof buf);
			(void) makemapentry(buf);
		}
#endif /* NISPLUS */
#ifdef NIS
		else if (strcmp(maptype[i], "nis") == 0 &&
		    stab("aliases.nis", ST_MAP, ST_FIND) == NULL)
		{
			(void) strlcpy(buf, "aliases.nis nis mail.aliases",
				sizeof buf);
			(void) makemapentry(buf);
		}
#endif /* NIS */
#if NETINFO
		else if (strcmp(maptype[i], "netinfo") == 0 &&
		    stab("aliases.netinfo", ST_MAP, ST_FIND) == NULL)
		{
			(void) strlcpy(buf, "aliases.netinfo netinfo -z, /aliases",
				sizeof buf);
			(void) makemapentry(buf);
		}
#endif /* NETINFO */
#ifdef HESIOD
		else if (strcmp(maptype[i], "hesiod") == 0 &&
		    stab("aliases.hesiod", ST_MAP, ST_FIND) == NULL)
		{
			(void) strlcpy(buf, "aliases.hesiod hesiod aliases",
				sizeof buf);
			(void) makemapentry(buf);
		}
#endif /* HESIOD */
	}
	if (stab("aliases", ST_MAP, ST_FIND) == NULL)
	{
		(void) strlcpy(buf, "aliases switch aliases", sizeof buf);
		(void) makemapentry(buf);
	}

#if 0		/* "user" map class is a better choice */
	/*
	**  Set up default users maps.
	*/

	nmaps = switch_map_find("passwd", maptype, mapreturn);
	for (i = 0; i < nmaps; i++)
	{
		if (strcmp(maptype[i], "files") == 0 &&
		    stab("users.files", ST_MAP, ST_FIND) == NULL)
		{
			(void) strlcpy(buf, "users.files text -m -z: -k0 -v6 /etc/passwd",
				sizeof buf);
			(void) makemapentry(buf);
		}
# ifdef NISPLUS
		else if (strcmp(maptype[i], "nisplus") == 0 &&
		    stab("users.nisplus", ST_MAP, ST_FIND) == NULL)
		{
			(void) strlcpy(buf, "users.nisplus nisplus -m -kname -vhome passwd.org_dir",
				sizeof buf);
			(void) makemapentry(buf);
		}
# endif /* NISPLUS */
# ifdef NIS
		else if (strcmp(maptype[i], "nis") == 0 &&
		    stab("users.nis", ST_MAP, ST_FIND) == NULL)
		{
			(void) strlcpy(buf, "users.nis nis -m passwd.byname",
				sizeof buf);
			(void) makemapentry(buf);
		}
# endif /* NIS */
# ifdef HESIOD
		else if (strcmp(maptype[i], "hesiod") == 0) &&
		    stab("users.hesiod", ST_MAP, ST_FIND) == NULL)
		{
			(void) strlcpy(buf, "users.hesiod hesiod", sizeof buf);
			(void) makemapentry(buf);
		}
# endif /* HESIOD */
	}
	if (stab("users", ST_MAP, ST_FIND) == NULL)
	{
		(void) strlcpy(buf, "users switch -m passwd", sizeof buf);
		(void) makemapentry(buf);
	}
#endif /* 0 */
}
/*
**  SWITCH_MAP_FIND -- find the list of types associated with a map
**
**	This is the system-dependent interface to the service switch.
**
**	Parameters:
**		service -- the name of the service of interest.
**		maptype -- an out-array of strings containing the types
**			of access to use for this service.  There can
**			be at most MAXMAPSTACK types for a single service.
**		mapreturn -- an out-array of return information bitmaps
**			for the map.
**
**	Returns:
**		The number of map types filled in, or -1 for failure.
**
**	Side effects:
**		Preserves errno so nothing in the routine clobbers it.
*/

#if defined(SOLARIS) || (defined(sony_news) && defined(__svr4))
# define _USE_SUN_NSSWITCH_
#endif /* defined(SOLARIS) || (defined(sony_news) && defined(__svr4)) */

#ifdef _USE_SUN_NSSWITCH_
# include <nsswitch.h>
#endif /* _USE_SUN_NSSWITCH_ */

#if defined(ultrix) || (defined(__osf__) && defined(__alpha))
# define _USE_DEC_SVC_CONF_
#endif /* defined(ultrix) || (defined(__osf__) && defined(__alpha)) */

#ifdef _USE_DEC_SVC_CONF_
# include <sys/svcinfo.h>
#endif /* _USE_DEC_SVC_CONF_ */

int
switch_map_find(service, maptype, mapreturn)
	char *service;
	char *maptype[MAXMAPSTACK];
	short mapreturn[MAXMAPACTIONS];
{
	int svcno;
	int save_errno = errno;

#ifdef _USE_SUN_NSSWITCH_
	struct __nsw_switchconfig *nsw_conf;
	enum __nsw_parse_err pserr;
	struct __nsw_lookup *lk;
	static struct __nsw_lookup lkp0 =
		{ "files", {1, 0, 0, 0}, NULL, NULL };
	static struct __nsw_switchconfig lkp_default =
		{ 0, "sendmail", 3, &lkp0 };

	for (svcno = 0; svcno < MAXMAPACTIONS; svcno++)
		mapreturn[svcno] = 0;

	if ((nsw_conf = __nsw_getconfig(service, &pserr)) == NULL)
		lk = lkp_default.lookups;
	else
		lk = nsw_conf->lookups;
	svcno = 0;
	while (lk != NULL)
	{
		maptype[svcno] = lk->service_name;
		if (lk->actions[__NSW_NOTFOUND] == __NSW_RETURN)
			mapreturn[MA_NOTFOUND] |= 1 << svcno;
		if (lk->actions[__NSW_TRYAGAIN] == __NSW_RETURN)
			mapreturn[MA_TRYAGAIN] |= 1 << svcno;
		if (lk->actions[__NSW_UNAVAIL] == __NSW_RETURN)
			mapreturn[MA_TRYAGAIN] |= 1 << svcno;
		svcno++;
		lk = lk->next;
	}
	errno = save_errno;
	return svcno;
#endif /* _USE_SUN_NSSWITCH_ */

#ifdef _USE_DEC_SVC_CONF_
	struct svcinfo *svcinfo;
	int svc;

	for (svcno = 0; svcno < MAXMAPACTIONS; svcno++)
		mapreturn[svcno] = 0;

	svcinfo = getsvc();
	if (svcinfo == NULL)
		goto punt;
	if (strcmp(service, "hosts") == 0)
		svc = SVC_HOSTS;
	else if (strcmp(service, "aliases") == 0)
		svc = SVC_ALIASES;
	else if (strcmp(service, "passwd") == 0)
		svc = SVC_PASSWD;
	else
	{
		errno = save_errno;
		return -1;
	}
	for (svcno = 0; svcno < SVC_PATHSIZE; svcno++)
	{
		switch (svcinfo->svcpath[svc][svcno])
		{
		  case SVC_LOCAL:
			maptype[svcno] = "files";
			break;

		  case SVC_YP:
			maptype[svcno] = "nis";
			break;

		  case SVC_BIND:
			maptype[svcno] = "dns";
			break;

# ifdef SVC_HESIOD
		  case SVC_HESIOD:
			maptype[svcno] = "hesiod";
			break;
# endif /* SVC_HESIOD */

		  case SVC_LAST:
			errno = save_errno;
			return svcno;
		}
	}
	errno = save_errno;
	return svcno;
#endif /* _USE_DEC_SVC_CONF_ */

#if !defined(_USE_SUN_NSSWITCH_) && !defined(_USE_DEC_SVC_CONF_)
	/*
	**  Fall-back mechanism.
	*/

	STAB *st;
	time_t now = curtime();

	for (svcno = 0; svcno < MAXMAPACTIONS; svcno++)
		mapreturn[svcno] = 0;

	if ((now - ServiceCacheTime) > (time_t) ServiceCacheMaxAge)
	{
		/* (re)read service switch */
		register FILE *fp;
		long sff = SFF_REGONLY|SFF_OPENASROOT|SFF_NOLOCK;

		if (!bitnset(DBS_LINKEDSERVICESWITCHFILEINWRITABLEDIR,
			    DontBlameSendmail))
			sff |= SFF_NOWLINK;

		if (ConfigFileRead)
			ServiceCacheTime = now;
		fp = safefopen(ServiceSwitchFile, O_RDONLY, 0, sff);
		if (fp != NULL)
		{
			char buf[MAXLINE];

			while (fgets(buf, sizeof buf, fp) != NULL)
			{
				register char *p;

				p = strpbrk(buf, "#\n");
				if (p != NULL)
					*p = '\0';
				p = strpbrk(buf, " \t");
				if (p != NULL)
					*p++ = '\0';
				if (buf[0] == '\0')
					continue;
				if (p == NULL)
				{
					sm_syslog(LOG_ERR, NOQID,
						  "Bad line on %.100s: %.100s",
						  ServiceSwitchFile,
						  buf);
					continue;
				}
				while (isspace(*p))
					p++;
				if (*p == '\0')
					continue;

				/*
				**  Find/allocate space for this service entry.
				**	Space for all of the service strings
				**	are allocated at once.  This means
				**	that we only have to free the first
				**	one to free all of them.
				*/

				st = stab(buf, ST_SERVICE, ST_ENTER);
				if (st->s_service[0] != NULL)
					free((void *) st->s_service[0]);
				p = newstr(p);
				for (svcno = 0; svcno < MAXMAPSTACK; )
				{
					if (*p == '\0')
						break;
					st->s_service[svcno++] = p;
					p = strpbrk(p, " \t");
					if (p == NULL)
						break;
					*p++ = '\0';
					while (isspace(*p))
						p++;
				}
				if (svcno < MAXMAPSTACK)
					st->s_service[svcno] = NULL;
			}
			(void) fclose(fp);
		}
	}

	/* look up entry in cache */
	st = stab(service, ST_SERVICE, ST_FIND);
	if (st != NULL && st->s_service[0] != NULL)
	{
		/* extract data */
		svcno = 0;
		while (svcno < MAXMAPSTACK)
		{
			maptype[svcno] = st->s_service[svcno];
			if (maptype[svcno++] == NULL)
				break;
		}
		errno = save_errno;
		return --svcno;
	}
#endif /* !defined(_USE_SUN_NSSWITCH_) && !defined(_USE_DEC_SVC_CONF_) */

#if !defined(_USE_SUN_NSSWITCH_)
	/* if the service file doesn't work, use an absolute fallback */
# ifdef _USE_DEC_SVC_CONF_
  punt:
# endif /* _USE_DEC_SVC_CONF_ */
	for (svcno = 0; svcno < MAXMAPACTIONS; svcno++)
		mapreturn[svcno] = 0;
	svcno = 0;
	if (strcmp(service, "aliases") == 0)
	{
		maptype[svcno++] = "files";
# if defined(AUTO_NETINFO_ALIASES) && defined (NETINFO)
		maptype[svcno++] = "netinfo";
# endif /* defined(AUTO_NETINFO_ALIASES) && defined (NETINFO) */
# ifdef AUTO_NIS_ALIASES
#  ifdef NISPLUS
		maptype[svcno++] = "nisplus";
#  endif /* NISPLUS */
#  ifdef NIS
		maptype[svcno++] = "nis";
#  endif /* NIS */
# endif /* AUTO_NIS_ALIASES */
		errno = save_errno;
		return svcno;
	}
	if (strcmp(service, "hosts") == 0)
	{
# if NAMED_BIND
		maptype[svcno++] = "dns";
# else /* NAMED_BIND */
#  if defined(sun) && !defined(BSD)
		/* SunOS */
		maptype[svcno++] = "nis";
#  endif /* defined(sun) && !defined(BSD) */
# endif /* NAMED_BIND */
# if defined(AUTO_NETINFO_HOSTS) && defined (NETINFO)
		maptype[svcno++] = "netinfo";
# endif /* defined(AUTO_NETINFO_HOSTS) && defined (NETINFO) */
		maptype[svcno++] = "files";
		errno = save_errno;
		return svcno;
	}
	errno = save_errno;
	return -1;
#endif /* !defined(_USE_SUN_NSSWITCH_) */
}
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
			pw = sm_getpwuid(RealUid);
			if (pw != NULL)
				myname = newstr(pw->pw_name);
		}
		else
		{
			uid_t uid = RealUid;

			myname = newstr(myname);
			if ((pw = sm_getpwnam(myname)) == NULL ||
			      (uid != 0 && uid != pw->pw_uid))
			{
				pw = sm_getpwuid(uid);
				if (pw != NULL)
					myname = newstr(pw->pw_name);
			}
		}
		if (myname == NULL || myname[0] == '\0')
		{
			syserr("554 5.3.0 Who are you?");
			myname = "postmaster";
		}
	}

	return myname;
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
		return NULL;
	}

	/* see if we have write permission */
	if (stat(pathn, &stbuf) < 0 || !bitset(S_IWOTH, stbuf.st_mode))
	{
		errno = 0;
		return NULL;
	}

	/* see if the user is logged in */
	if (getlogin() == NULL)
		return NULL;

	/* looks good */
	return pathn;
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
**	message should be given using "usrerr" and an EX_ code
**	should be returned.  You can also set to->q_status to
**	a DSN-style status code.
**
**	EF_NO_BODY_RETN can be set in e->e_flags to suppress the
**	body during the return-to-sender function; this should be done
**	on huge messages.  This bit may already be set by the ESMTP
**	protocol.
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

int
checkcompat(to, e)
	register ADDRESS *to;
	register ENVELOPE *e;
{
	if (tTd(49, 1))
		dprintf("checkcompat(to=%s, from=%s)\n",
			to->q_paddr, e->e_from.q_paddr);

#ifdef EXAMPLE_CODE
	/* this code is intended as an example only */
	register STAB *s;

	s = stab("arpa", ST_MAILER, ST_FIND);
	if (s != NULL && strcmp(e->e_from.q_mailer->m_name, "local") != 0 &&
	    to->q_mailer == s->s_mailer)
	{
		usrerr("553 No ARPA mail through this machine: see your system administration");
		/* e->e_flags |= EF_NO_BODY_RETN; to suppress body on return */
		to->q_status = "5.7.1";
		return EX_UNAVAILABLE;
	}
#endif /* EXAMPLE_CODE */
	return EX_OK;
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
	/*
	**  First, try for modern signal calls
	**  and restartable syscalls
	*/

# ifdef SA_RESTART
	struct sigaction n, o;

	memset(&n, '\0', sizeof n);
#  if USE_SA_SIGACTION
	n.sa_sigaction = (void(*)(int, siginfo_t *, void *)) handler;
	n.sa_flags = SA_RESTART|SA_SIGINFO;
#  else /* USE_SA_SIGACTION */
	n.sa_handler = handler;
	n.sa_flags = SA_RESTART;
#  endif /* USE_SA_SIGACTION */
	if (sigaction(sig, &n, &o) < 0)
		return SIG_ERR;
	return o.sa_handler;
# else /* SA_RESTART */

	/*
	**  Else check for SYS5SIGNALS or
	**  BSD4_3 signals
	*/

#  if defined(SYS5SIGNALS) || defined(BSD4_3)
#   ifdef BSD4_3
	return signal(sig, handler);
#   else /* BSD4_3 */
	return sigset(sig, handler);
#   endif /* BSD4_3 */
#  else /* defined(SYS5SIGNALS) || defined(BSD4_3) */

	/*
	**  Finally, if nothing else is available,
	**  go for a default
	*/

	struct sigaction n, o;

	memset(&n, '\0', sizeof n);
	n.sa_handler = handler;
	if (sigaction(sig, &n, &o) < 0)
		return SIG_ERR;
	return o.sa_handler;
#  endif /* defined(SYS5SIGNALS) || defined(BSD4_3) */
# endif /* SA_RESTART */
}
/*
**  BLOCKSIGNAL -- hold a signal to prevent delivery
**
**	Parameters:
**		sig -- the signal to block.
**
**	Returns:
**		1 signal was previously blocked
**		0 signal was not previously blocked
**		-1 on failure.
*/

int
blocksignal(sig)
	int sig;
{
# ifdef BSD4_3
#  ifndef sigmask
#   define sigmask(s)	(1 << ((s) - 1))
#  endif /* ! sigmask */
	return (sigblock(sigmask(sig)) & sigmask(sig)) != 0;
# else /* BSD4_3 */
#  ifdef ALTOS_SYSTEM_V
	sigfunc_t handler;

	handler = sigset(sig, SIG_HOLD);
	if (handler == SIG_ERR)
		return -1;
	else
		return handler == SIG_HOLD;
#  else /* ALTOS_SYSTEM_V */
	sigset_t sset, oset;

	(void) sigemptyset(&sset);
	(void) sigaddset(&sset, sig);
	if (sigprocmask(SIG_BLOCK, &sset, &oset) < 0)
		return -1;
	else
		return sigismember(&oset, sig);
#  endif /* ALTOS_SYSTEM_V */
# endif /* BSD4_3 */
}
/*
**  RELEASESIGNAL -- release a held signal
**
**	Parameters:
**		sig -- the signal to release.
**
**	Returns:
**		1 signal was previously blocked
**		0 signal was not previously blocked
**		-1 on failure.
*/

int
releasesignal(sig)
	int sig;
{
# ifdef BSD4_3
	return (sigsetmask(sigblock(0) & ~sigmask(sig)) & sigmask(sig)) != 0;
# else /* BSD4_3 */
#  ifdef ALTOS_SYSTEM_V
	sigfunc_t handler;

	handler = sigset(sig, SIG_HOLD);
	if (sigrelse(sig) < 0)
		return -1;
	else
		return handler == SIG_HOLD;
#  else /* ALTOS_SYSTEM_V */
	sigset_t sset, oset;

	(void) sigemptyset(&sset);
	(void) sigaddset(&sset, sig);
	if (sigprocmask(SIG_UNBLOCK, &sset, &oset) < 0)
		return -1;
	else
		return sigismember(&oset, sig);
#  endif /* ALTOS_SYSTEM_V */
# endif /* BSD4_3 */
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

void
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

void
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
# include <compat.h>
#endif /* _AUX_SOURCE */

#if SHARE_V1
# include <shares.h>
#endif /* SHARE_V1 */

void
init_md(argc, argv)
	int argc;
	char **argv;
{
#ifdef _AUX_SOURCE
	setcompat(getcompat() | COMPAT_BSDPROT);
#endif /* _AUX_SOURCE */

#ifdef SUN_EXTENSIONS
	init_md_sun();
#endif /* SUN_EXTENSIONS */

#if _CONVEX_SOURCE
	/* keep gethostby*() from stripping the local domain name */
	set_domain_trim_off();
#endif /* _CONVEX_SOURCE */
#ifdef __QNX__
	/*
	**  Due to QNX's network distributed nature, you can target a tcpip
	**  stack on a different node in the qnx network; this patch lets
	**  this feature work.  The __sock_locate() must be done before the
	**  environment is clear.
	*/
	__sock_locate();
#endif /* __QNX__ */
#if SECUREWARE || defined(_SCO_unix_)
	set_auth_parameters(argc, argv);

# ifdef _SCO_unix_
	/*
	**  This is required for highest security levels (the kernel
	**  won't let it call set*uid() or run setuid binaries without
	**  it).  It may be necessary on other SECUREWARE systems.
	*/

	if (getluid() == -1)
		setluid(0);
# endif /* _SCO_unix_ */
#endif /* SECUREWARE || defined(_SCO_unix_) */


#ifdef VENDOR_DEFAULT
	VendorCode = VENDOR_DEFAULT;
#else /* VENDOR_DEFAULT */
	VendorCode = VENDOR_BERKELEY;
#endif /* VENDOR_DEFAULT */
}
/*
**  INIT_VENDOR_MACROS -- vendor-dependent macro initializations
**
**	Called once, on startup.
**
**	Parameters:
**		e -- the global envelope.
**
**	Returns:
**		none.
**
**	Side Effects:
**		vendor-dependent.
*/

void
init_vendor_macros(e)
	register ENVELOPE *e;
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
#define LA_INT		2	/* read kmem for avenrun; interpret as long */
#define LA_FLOAT	3	/* read kmem for avenrun; interpret as float */
#define LA_SUBR		4	/* call getloadavg */
#define LA_MACH		5	/* MACH load averages (as on NeXT boxes) */
#define LA_SHORT	6	/* read kmem for avenrun; interpret as short */
#define LA_PROCSTR	7	/* read string ("1.17") from /proc/loadavg */
#define LA_READKSYM	8	/* SVR4: use MIOC_READKSYM ioctl call */
#define LA_DGUX		9	/* special DGUX implementation */
#define LA_HPUX		10	/* special HPUX implementation */
#define LA_IRIX6	11	/* special IRIX 6.2 implementation */
#define LA_KSTAT	12	/* special Solaris kstat(3k) implementation */
#define LA_DEVSHORT	13	/* read short from a device */
#define LA_ALPHAOSF	14	/* Digital UNIX (OSF/1 on Alpha) table() call */

/* do guesses based on general OS type */
#ifndef LA_TYPE
# define LA_TYPE	LA_ZERO
#endif /* ! LA_TYPE */

#ifndef FSHIFT
# if defined(unixpc)
#  define FSHIFT	5
# endif /* defined(unixpc) */

# if defined(__alpha) || defined(IRIX)
#  define FSHIFT	10
# endif /* defined(__alpha) || defined(IRIX) */

#endif /* ! FSHIFT */

#ifndef FSHIFT
# define FSHIFT		8
#endif /* ! FSHIFT */

#ifndef FSCALE
# define FSCALE		(1 << FSHIFT)
#endif /* ! FSCALE */

#ifndef LA_AVENRUN
# ifdef SYSTEM5
#  define LA_AVENRUN	"avenrun"
# else /* SYSTEM5 */
#  define LA_AVENRUN	"_avenrun"
# endif /* SYSTEM5 */
#endif /* ! LA_AVENRUN */

/* _PATH_KMEM should be defined in <paths.h> */
#ifndef _PATH_KMEM
# define _PATH_KMEM	"/dev/kmem"
#endif /* ! _PATH_KMEM */

#if (LA_TYPE == LA_INT) || (LA_TYPE == LA_FLOAT) || (LA_TYPE == LA_SHORT)

# include <nlist.h>

/* _PATH_UNIX should be defined in <paths.h> */
# ifndef _PATH_UNIX
#  if defined(SYSTEM5)
#   define _PATH_UNIX	"/unix"
#  else /* defined(SYSTEM5) */
#   define _PATH_UNIX	"/vmunix"
#  endif /* defined(SYSTEM5) */
# endif /* ! _PATH_UNIX */

# ifdef _AUX_SOURCE
struct nlist	Nl[2];
# else /* _AUX_SOURCE */
struct nlist	Nl[] =
{
	{ LA_AVENRUN },
	{ 0 },
};
# endif /* _AUX_SOURCE */
# define X_AVENRUN	0

static int
getla()
{
	static int kmem = -1;
# if LA_TYPE == LA_INT
	long avenrun[3];
# else /* LA_TYPE == LA_INT */
#  if LA_TYPE == LA_SHORT
	short avenrun[3];
#  else /* LA_TYPE == LA_SHORT */
	double avenrun[3];
#  endif /* LA_TYPE == LA_SHORT */
# endif /* LA_TYPE == LA_INT */
	extern int errno;
	extern off_t lseek();

	if (kmem < 0)
	{
# ifdef _AUX_SOURCE
		(void) strlcpy(Nl[X_AVENRUN].n_name, LA_AVENRUN,
			       sizeof Nl[X_AVENRUN].n_name);
		Nl[1].n_name[0] = '\0';
# endif /* _AUX_SOURCE */

# if defined(_AIX3) || defined(_AIX4)
		if (knlist(Nl, 1, sizeof Nl[0]) < 0)
# else /* defined(_AIX3) || defined(_AIX4) */
		if (nlist(_PATH_UNIX, Nl) < 0)
# endif /* defined(_AIX3) || defined(_AIX4) */
		{
			if (tTd(3, 1))
				dprintf("getla: nlist(%s): %s\n", _PATH_UNIX,
					errstring(errno));
			return -1;
		}
		if (Nl[X_AVENRUN].n_value == 0)
		{
			if (tTd(3, 1))
				dprintf("getla: nlist(%s, %s) ==> 0\n",
					_PATH_UNIX, LA_AVENRUN);
			return -1;
		}
# ifdef NAMELISTMASK
		Nl[X_AVENRUN].n_value &= NAMELISTMASK;
# endif /* NAMELISTMASK */

		kmem = open(_PATH_KMEM, 0, 0);
		if (kmem < 0)
		{
			if (tTd(3, 1))
				dprintf("getla: open(/dev/kmem): %s\n",
					errstring(errno));
			return -1;
		}
		(void) fcntl(kmem, F_SETFD, FD_CLOEXEC);
	}
	if (tTd(3, 20))
		dprintf("getla: symbol address = %#lx\n",
			(u_long) Nl[X_AVENRUN].n_value);
	if (lseek(kmem, (off_t) Nl[X_AVENRUN].n_value, SEEK_SET) == -1 ||
	    read(kmem, (char *) avenrun, sizeof(avenrun)) < sizeof(avenrun))
	{
		/* thank you Ian */
		if (tTd(3, 1))
			dprintf("getla: lseek or read: %s\n",
				errstring(errno));
		return -1;
	}
# if (LA_TYPE == LA_INT) || (LA_TYPE == LA_SHORT)
	if (tTd(3, 5))
	{
#  if LA_TYPE == LA_SHORT
		dprintf("getla: avenrun = %d", avenrun[0]);
		if (tTd(3, 15))
			dprintf(", %d, %d", avenrun[1], avenrun[2]);
#  else /* LA_TYPE == LA_SHORT */
		dprintf("getla: avenrun = %ld", avenrun[0]);
		if (tTd(3, 15))
			dprintf(", %ld, %ld", avenrun[1], avenrun[2]);
#  endif /* LA_TYPE == LA_SHORT */
		dprintf("\n");
	}
	if (tTd(3, 1))
		dprintf("getla: %d\n",
			(int) (avenrun[0] + FSCALE/2) >> FSHIFT);
	return ((int) (avenrun[0] + FSCALE/2) >> FSHIFT);
# else /* (LA_TYPE == LA_INT) || (LA_TYPE == LA_SHORT) */
	if (tTd(3, 5))
	{
		dprintf("getla: avenrun = %g", avenrun[0]);
		if (tTd(3, 15))
			dprintf(", %g, %g", avenrun[1], avenrun[2]);
		dprintf("\n");
	}
	if (tTd(3, 1))
		dprintf("getla: %d\n", (int) (avenrun[0] +0.5));
	return ((int) (avenrun[0] + 0.5));
# endif /* (LA_TYPE == LA_INT) || (LA_TYPE == LA_SHORT) */
}

#endif /* (LA_TYPE == LA_INT) || (LA_TYPE == LA_FLOAT) || (LA_TYPE == LA_SHORT) */

#if LA_TYPE == LA_READKSYM

# include <sys/ksym.h>

static int
getla()
{
	static int kmem = -1;
	long avenrun[3];
	extern int errno;
	struct mioc_rksym mirk;

	if (kmem < 0)
	{
		kmem = open("/dev/kmem", 0, 0);
		if (kmem < 0)
		{
			if (tTd(3, 1))
				dprintf("getla: open(/dev/kmem): %s\n",
					errstring(errno));
			return -1;
		}
		(void) fcntl(kmem, F_SETFD, FD_CLOEXEC);
	}
	mirk.mirk_symname = LA_AVENRUN;
	mirk.mirk_buf = avenrun;
	mirk.mirk_buflen = sizeof(avenrun);
	if (ioctl(kmem, MIOC_READKSYM, &mirk) < 0)
	{
		if (tTd(3, 1))
			dprintf("getla: ioctl(MIOC_READKSYM) failed: %s\n",
				errstring(errno));
		return -1;
	}
	if (tTd(3, 5))
	{
		dprintf("getla: avenrun = %d", avenrun[0]);
		if (tTd(3, 15))
			dprintf(", %d, %d", avenrun[1], avenrun[2]);
		dprintf("\n");
	}
	if (tTd(3, 1))
		dprintf("getla: %d\n",
			(int) (avenrun[0] + FSCALE/2) >> FSHIFT);
	return ((int) (avenrun[0] + FSCALE/2) >> FSHIFT);
}

#endif /* LA_TYPE == LA_READKSYM */

#if LA_TYPE == LA_DGUX

# include <sys/dg_sys_info.h>

static int
getla()
{
	struct dg_sys_info_load_info load_info;

	dg_sys_info((long *)&load_info,
		DG_SYS_INFO_LOAD_INFO_TYPE, DG_SYS_INFO_LOAD_VERSION_0);

	if (tTd(3, 1))
		dprintf("getla: %d\n", (int) (load_info.one_minute + 0.5));

	return ((int) (load_info.one_minute + 0.5));
}

#endif /* LA_TYPE == LA_DGUX */

#if LA_TYPE == LA_HPUX

/* forward declarations to keep gcc from complaining */
struct pst_dynamic;
struct pst_status;
struct pst_static;
struct pst_vminfo;
struct pst_diskinfo;
struct pst_processor;
struct pst_lv;
struct pst_swapinfo;

# include <sys/param.h>
# include <sys/pstat.h>

static int
getla()
{
	struct pst_dynamic pstd;

	if (pstat_getdynamic(&pstd, sizeof(struct pst_dynamic),
			     (size_t) 1, 0) == -1)
		return 0;

	if (tTd(3, 1))
		dprintf("getla: %d\n", (int) (pstd.psd_avg_1_min + 0.5));

	return (int) (pstd.psd_avg_1_min + 0.5);
}

#endif /* LA_TYPE == LA_HPUX */

#if LA_TYPE == LA_SUBR

static int
getla()
{
	double avenrun[3];

	if (getloadavg(avenrun, sizeof(avenrun) / sizeof(avenrun[0])) < 0)
	{
		if (tTd(3, 1))
			dprintf("getla: getloadavg failed: %s",
				errstring(errno));
		return -1;
	}
	if (tTd(3, 1))
		dprintf("getla: %d\n", (int) (avenrun[0] +0.5));
	return ((int) (avenrun[0] + 0.5));
}

#endif /* LA_TYPE == LA_SUBR */

#if LA_TYPE == LA_MACH

/*
**  This has been tested on NEXTSTEP release 2.1/3.X.
*/

# if defined(NX_CURRENT_COMPILER_RELEASE) && NX_CURRENT_COMPILER_RELEASE > NX_COMPILER_RELEASE_3_0
#  include <mach/mach.h>
# else /* defined(NX_CURRENT_COMPILER_RELEASE) && NX_CURRENT_COMPILER_RELEASE > NX_COMPILER_RELEASE_3_0 */
#  include <mach.h>
# endif /* defined(NX_CURRENT_COMPILER_RELEASE) && NX_CURRENT_COMPILER_RELEASE > NX_COMPILER_RELEASE_3_0 */

static int
getla()
{
	processor_set_t default_set;
	kern_return_t error;
	unsigned int info_count;
	struct processor_set_basic_info info;
	host_t host;

	error = processor_set_default(host_self(), &default_set);
	if (error != KERN_SUCCESS)
	{
		if (tTd(3, 1))
			dprintf("getla: processor_set_default failed: %s",
				errstring(errno));
		return -1;
	}
	info_count = PROCESSOR_SET_BASIC_INFO_COUNT;
	if (processor_set_info(default_set, PROCESSOR_SET_BASIC_INFO,
			       &host, (processor_set_info_t)&info,
			       &info_count) != KERN_SUCCESS)
	{
		if (tTd(3, 1))
			dprintf("getla: processor_set_info failed: %s",
				errstring(errno));
		return -1;
	}
	if (tTd(3, 1))
		dprintf("getla: %d\n",
			(int) ((info.load_average + (LOAD_SCALE / 2)) /
			       LOAD_SCALE));
	return (int) (info.load_average + (LOAD_SCALE / 2)) / LOAD_SCALE;
}

#endif /* LA_TYPE == LA_MACH */

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
# endif /* ! _PATH_LOADAVG */

static int
getla()
{
	double avenrun;
	register int result;
	FILE *fp;

	fp = fopen(_PATH_LOADAVG, "r");
	if (fp == NULL)
	{
		if (tTd(3, 1))
			dprintf("getla: fopen(%s): %s\n",
				_PATH_LOADAVG, errstring(errno));
		return -1;
	}
	result = fscanf(fp, "%lf", &avenrun);
	(void) fclose(fp);
	if (result != 1)
	{
		if (tTd(3, 1))
			dprintf("getla: fscanf() = %d: %s\n",
				result, errstring(errno));
		return -1;
	}

	if (tTd(3, 1))
		dprintf("getla(): %.2f\n", avenrun);

	return ((int) (avenrun + 0.5));
}

#endif /* LA_TYPE == LA_PROCSTR */

#if LA_TYPE == LA_IRIX6

# include <sys/sysmp.h>

int getla(void)
{
	static int kmem = -1;
	int avenrun[3];

	if (kmem < 0)
	{
		kmem = open(_PATH_KMEM, 0, 0);
		if (kmem < 0)
		{
			if (tTd(3, 1))
				dprintf("getla: open(%s): %s\n", _PATH_KMEM,
					errstring(errno));
			return -1;
		}
		(void) fcntl(kmem, F_SETFD, FD_CLOEXEC);
	}

	if (lseek(kmem, (sysmp(MP_KERNADDR, MPKA_AVENRUN) & 0x7fffffff), SEEK_SET) == -1 ||
	    read(kmem, (char *)avenrun, sizeof(avenrun)) < sizeof(avenrun))
	{
		if (tTd(3, 1))
			dprintf("getla: lseek or read: %s\n",
				errstring(errno));
		return -1;
	}
	if (tTd(3, 5))
	{
		dprintf("getla: avenrun = %ld", (long int) avenrun[0]);
		if (tTd(3, 15))
			dprintf(", %ld, %ld",
				(long int) avenrun[1], (long int) avenrun[2]);
		dprintf("\n");
	}

	if (tTd(3, 1))
		dprintf("getla: %d\n",
			(int) (avenrun[0] + FSCALE/2) >> FSHIFT);
	return ((int) (avenrun[0] + FSCALE/2) >> FSHIFT);

}
#endif /* LA_TYPE == LA_IRIX6 */

#if LA_TYPE == LA_KSTAT

# include <kstat.h>

static int
getla()
{
	static kstat_ctl_t *kc = NULL;
	static kstat_t *ksp = NULL;
	kstat_named_t *ksn;
	int la;

	if (kc == NULL)		/* if not initialized before */
		kc = kstat_open();
	if (kc == NULL)
	{
		if (tTd(3, 1))
			dprintf("getla: kstat_open(): %s\n",
				errstring(errno));
		return -1;
	}
	if (ksp == NULL)
		ksp = kstat_lookup(kc, "unix", 0, "system_misc");
	if (ksp == NULL)
	{
		if (tTd(3, 1))
			dprintf("getla: kstat_lookup(): %s\n",
				errstring(errno));
		return -1;
	}
	if (kstat_read(kc, ksp, NULL) < 0)
	{
		if (tTd(3, 1))
			dprintf("getla: kstat_read(): %s\n",
				errstring(errno));
		return -1;
	}
	ksn = (kstat_named_t *) kstat_data_lookup(ksp, "avenrun_1min");
	la = ((double)ksn->value.ul + FSCALE/2) / FSCALE;
	/* kstat_close(kc); /o do not close for fast access */
	return la;
}

#endif /* LA_TYPE == LA_KSTAT */

#if LA_TYPE == LA_DEVSHORT

/*
**  Read /dev/table/avenrun for the load average.  This should contain
**  three shorts for the 1, 5, and 15 minute loads.  We only read the
**  first, since that's all we care about.
**
**	Intended for SCO OpenServer 5.
*/

# ifndef _PATH_AVENRUN
#  define _PATH_AVENRUN	"/dev/table/avenrun"
# endif /* ! _PATH_AVENRUN */

static int
getla()
{
	static int afd = -1;
	short avenrun;
	int loadav;
	int r;

	errno = EBADF;

	if (afd == -1 || lseek(afd, 0L, SEEK_SET) == -1)
	{
		if (errno != EBADF)
			return -1;
		afd = open(_PATH_AVENRUN, O_RDONLY|O_SYNC);
		if (afd < 0)
		{
			sm_syslog(LOG_ERR, NOQID,
				"can't open %s: %m",
				_PATH_AVENRUN);
			return -1;
		}
	}

	r = read(afd, &avenrun, sizeof avenrun);

	if (tTd(3, 5))
		dprintf("getla: avenrun = %d\n", avenrun);
	loadav = (int) (avenrun + FSCALE/2) >> FSHIFT;
	if (tTd(3, 1))
		dprintf("getla: %d\n", loadav);
	return loadav;
}

#endif /* LA_TYPE == LA_DEVSHORT */

#if LA_TYPE == LA_ALPHAOSF
struct rtentry;
struct mbuf;
# include <sys/table.h>

int getla()
{
	int ave = 0;
	struct tbl_loadavg tab;

	if (table(TBL_LOADAVG, 0, &tab, 1, sizeof(tab)) == -1)
	{
		if (tTd(3, 1))
			dprintf("getla: table %s\n", errstring(errno));
		return -1;
	}

	if (tTd(3, 1))
		dprintf("getla: scale = %d\n", tab.tl_lscale);

	if (tab.tl_lscale)
		ave = ((tab.tl_avenrun.l[2] + (tab.tl_lscale/2)) /
		       tab.tl_lscale);
	else
		ave = (int) (tab.tl_avenrun.d[2] + 0.5);

	if (tTd(3, 1))
		dprintf("getla: %d\n", ave);

	return ave;
}

#endif /* LA_TYPE == LA_ALPHAOSF */

#if LA_TYPE == LA_ZERO

static int
getla()
{
	if (tTd(3, 1))
		dprintf("getla: ZERO\n");
	return 0;
}

#endif /* LA_TYPE == LA_ZERO */

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
static char  rcsid[] = "@(#)$OrigId: getloadavg.c,v 1.16 1991/06/21 12:51:15 paul Exp $";
#endif /* ! lint */

#ifdef apollo
# undef volatile
# include <apollo/base.h>

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
     return 0;
}
#endif /* apollo */
/*
**  SM_GETLA -- get the current load average and set macro
**
**	Parameters:
**		e -- the envelope for the load average macro.
**
**	Returns:
**		The current load average as an integer.
**
**	Side Effects:
**		Sets the load average macro ({load_avg}) if
**		envelope e is not NULL.
*/

int
sm_getla(e)
	ENVELOPE *e;
{
	register int la;

	la = getla();
	if (e != NULL)
	{
		char labuf[8];

		snprintf(labuf, sizeof labuf, "%d", la);
		define(macid("{load_avg}", NULL), newstr(labuf), e);
	}
	return la;
}

/*
**  SHOULDQUEUE -- should this message be queued or sent?
**
**	Compares the message cost to the load average to decide.
**
**	Parameters:
**		pri -- the priority of the message in question.
**		ct -- the message creation time.
**
**	Returns:
**		TRUE -- if this message should be queued up for the
**			time being.
**		FALSE -- if the load is low enough to send this message.
**
**	Side Effects:
**		none.
*/

/* ARGSUSED1 */
bool
shouldqueue(pri, ct)
	long pri;
	time_t ct;
{
	bool rval;

	if (tTd(3, 30))
		dprintf("shouldqueue: CurrentLA=%d, pri=%ld: ",
			CurrentLA, pri);
	if (CurrentLA < QueueLA)
	{
		if (tTd(3, 30))
			dprintf("FALSE (CurrentLA < QueueLA)\n");
		return FALSE;
	}
#if 0	/* this code is reported to cause oscillation around RefuseLA */
	if (CurrentLA >= RefuseLA && QueueLA < RefuseLA)
	{
		if (tTd(3, 30))
			dprintf("TRUE (CurrentLA >= RefuseLA)\n");
		return TRUE;
	}
#endif /* 0 */
	rval = pri > (QueueFactor / (CurrentLA - QueueLA + 1));
	if (tTd(3, 30))
		dprintf("%s (by calculation)\n", rval ? "TRUE" : "FALSE");
	return rval;
}
/*
**  REFUSECONNECTIONS -- decide if connections should be refused
**
**	Parameters:
**		name -- daemon name (for error messages only)
**		e -- the current envelope.
**		d -- number of daemon
**
**	Returns:
**		TRUE if incoming SMTP connections should be refused
**			(for now).
**		FALSE if we should accept new work.
**
**	Side Effects:
**		Sets process title when it is rejecting connections.
*/

bool
refuseconnections(name, e, d)
	char *name;
	ENVELOPE *e;
	int d;
{
	time_t now;
	static time_t lastconn[MAXDAEMONS];
	static int conncnt[MAXDAEMONS];


#ifdef XLA
	if (!xla_smtp_ok())
		return TRUE;
#endif /* XLA */

	now = curtime();
	if (now != lastconn[d])
	{
		lastconn[d] = now;
		conncnt[d] = 0;
	}
	else if (conncnt[d]++ > ConnRateThrottle && ConnRateThrottle > 0)
	{
		/* sleep to flatten out connection load */
		sm_setproctitle(TRUE, e, "deferring connections on daemon %s: %d per second",
				name, ConnRateThrottle);
		if (LogLevel >= 9)
			sm_syslog(LOG_INFO, NOQID,
				"deferring connections on daemon %s: %d per second",
				name, ConnRateThrottle);
		(void) sleep(1);
	}

	CurrentLA = getla();
	if (RefuseLA > 0 && CurrentLA >= RefuseLA)
	{
		sm_setproctitle(TRUE, e, "rejecting connections on daemon %s: load average: %d",
				name, CurrentLA);
		if (LogLevel >= 9)
			sm_syslog(LOG_INFO, NOQID,
				"rejecting connections on daemon %s: load average: %d",
				name, CurrentLA);
		return TRUE;
	}

	if (MaxChildren > 0 && CurChildren >= MaxChildren)
	{
		proc_list_probe();
		if (CurChildren >= MaxChildren)
		{
			sm_setproctitle(TRUE, e, "rejecting connections on daemon %s: %d children, max %d",
					name, CurChildren, MaxChildren);
			if (LogLevel >= 9)
				sm_syslog(LOG_INFO, NOQID,
					"rejecting connections on daemon %s: %d children, max %d",
					name, CurChildren, MaxChildren);
			return TRUE;
		}
	}

	return FALSE;
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

#define SPT_NONE	0	/* don't use it at all */
#define SPT_REUSEARGV	1	/* cover argv with title information */
#define SPT_BUILTIN	2	/* use libc builtin */
#define SPT_PSTAT	3	/* use pstat(PSTAT_SETCMD, ...) */
#define SPT_PSSTRINGS	4	/* use PS_STRINGS->... */
#define SPT_SYSMIPS	5	/* use sysmips() supported by NEWS-OS 6 */
#define SPT_SCO		6	/* write kernel u. area */
#define SPT_CHANGEARGV	7	/* write our own strings into argv[] */

#ifndef SPT_TYPE
# define SPT_TYPE	SPT_REUSEARGV
#endif /* ! SPT_TYPE */


#if SPT_TYPE != SPT_NONE && SPT_TYPE != SPT_BUILTIN

# if SPT_TYPE == SPT_PSTAT
#  include <sys/pstat.h>
# endif /* SPT_TYPE == SPT_PSTAT */
# if SPT_TYPE == SPT_PSSTRINGS
#  include <machine/vmparam.h>
#  include <sys/exec.h>
#  ifndef PS_STRINGS	/* hmmmm....  apparently not available after all */
#   undef SPT_TYPE
#   define SPT_TYPE	SPT_REUSEARGV
#  else /* ! PS_STRINGS */
#   ifndef NKPDE			/* FreeBSD 2.0 */
#    define NKPDE 63
typedef unsigned int	*pt_entry_t;
#   endif /* ! NKPDE */
#  endif /* ! PS_STRINGS */
# endif /* SPT_TYPE == SPT_PSSTRINGS */

# if SPT_TYPE == SPT_PSSTRINGS || SPT_TYPE == SPT_CHANGEARGV
#  define SETPROC_STATIC	static
# else /* SPT_TYPE == SPT_PSSTRINGS || SPT_TYPE == SPT_CHANGEARGV */
#  define SETPROC_STATIC
# endif /* SPT_TYPE == SPT_PSSTRINGS || SPT_TYPE == SPT_CHANGEARGV */

# if SPT_TYPE == SPT_SYSMIPS
#  include <sys/sysmips.h>
#  include <sys/sysnews.h>
# endif /* SPT_TYPE == SPT_SYSMIPS */

# if SPT_TYPE == SPT_SCO
#  include <sys/immu.h>
#  include <sys/dir.h>
#  include <sys/user.h>
#  include <sys/fs/s5param.h>
#  if PSARGSZ > MAXLINE
#   define SPT_BUFSIZE	PSARGSZ
#  endif /* PSARGSZ > MAXLINE */
# endif /* SPT_TYPE == SPT_SCO */

# ifndef SPT_PADCHAR
#  define SPT_PADCHAR	' '
# endif /* ! SPT_PADCHAR */

#endif /* SPT_TYPE != SPT_NONE && SPT_TYPE != SPT_BUILTIN */

#ifndef SPT_BUFSIZE
# define SPT_BUFSIZE	MAXLINE
#endif /* ! SPT_BUFSIZE */

/*
**  Pointers for setproctitle.
**	This allows "ps" listings to give more useful information.
*/

static char	**Argv = NULL;		/* pointer to argument vector */
static char	*LastArgv = NULL;	/* end of argv */
#if SPT_TYPE != SPT_BUILTIN
static void	setproctitle __P((const char *, ...));
#endif /* SPT_TYPE != SPT_BUILTIN */

void
initsetproctitle(argc, argv, envp)
	int argc;
	char **argv;
	char **envp;
{
	register int i, envpsize = 0;
	extern char **environ;

	/*
	**  Move the environment so setproctitle can use the space at
	**  the top of memory.
	*/

	for (i = 0; envp[i] != NULL; i++)
		envpsize += strlen(envp[i]) + 1;
	environ = (char **) xalloc(sizeof (char *) * (i + 1));
	for (i = 0; envp[i] != NULL; i++)
		environ[i] = newstr(envp[i]);
	environ[i] = NULL;

	/*
	**  Save start and extent of argv for setproctitle.
	*/

	Argv = argv;

	/*
	**  Determine how much space we can use for setproctitle.
	**  Use all contiguous argv and envp pointers starting at argv[0]
	*/
	for (i = 0; i < argc; i++)
	{
		if (i == 0 || LastArgv + 1 == argv[i])
			LastArgv = argv[i] + strlen(argv[i]);
	}
	for (i = 0; LastArgv != NULL && envp[i] != NULL; i++)
	{
		if (LastArgv + 1 == envp[i])
			LastArgv = envp[i] + strlen(envp[i]);
	}
}

#if SPT_TYPE != SPT_BUILTIN

/*VARARGS1*/
static void
# ifdef __STDC__
setproctitle(const char *fmt, ...)
# else /* __STDC__ */
setproctitle(fmt, va_alist)
	const char *fmt;
	va_dcl
# endif /* __STDC__ */
{
# if SPT_TYPE != SPT_NONE
	register int i;
	register char *p;
	SETPROC_STATIC char buf[SPT_BUFSIZE];
	VA_LOCAL_DECL
#  if SPT_TYPE == SPT_PSTAT
	union pstun pst;
#  endif /* SPT_TYPE == SPT_PSTAT */
#  if SPT_TYPE == SPT_SCO
	off_t seek_off;
	static int kmem = -1;
	static int kmempid = -1;
	struct user u;
#  endif /* SPT_TYPE == SPT_SCO */

	p = buf;

	/* print sendmail: heading for grep */
	(void) strlcpy(p, "sendmail: ", SPACELEFT(buf, p));
	p += strlen(p);

	/* print the argument string */
	VA_START(fmt);
	(void) vsnprintf(p, SPACELEFT(buf, p), fmt, ap);
	VA_END;

	i = strlen(buf);

#  if SPT_TYPE == SPT_PSTAT
	pst.pst_command = buf;
	pstat(PSTAT_SETCMD, pst, i, 0, 0);
#  endif /* SPT_TYPE == SPT_PSTAT */
#  if SPT_TYPE == SPT_PSSTRINGS
	PS_STRINGS->ps_nargvstr = 1;
	PS_STRINGS->ps_argvstr = buf;
#  endif /* SPT_TYPE == SPT_PSSTRINGS */
#  if SPT_TYPE == SPT_SYSMIPS
	sysmips(SONY_SYSNEWS, NEWS_SETPSARGS, buf);
#  endif /* SPT_TYPE == SPT_SYSMIPS */
#  if SPT_TYPE == SPT_SCO
	if (kmem < 0 || kmempid != getpid())
	{
		if (kmem >= 0)
			close(kmem);
		kmem = open(_PATH_KMEM, O_RDWR, 0);
		if (kmem < 0)
			return;
		(void) fcntl(kmem, F_SETFD, FD_CLOEXEC);
		kmempid = getpid();
	}
	buf[PSARGSZ - 1] = '\0';
	seek_off = UVUBLK + (off_t) u.u_psargs - (off_t) &u;
	if (lseek(kmem, (off_t) seek_off, SEEK_SET) == seek_off)
		(void) write(kmem, buf, PSARGSZ);
#  endif /* SPT_TYPE == SPT_SCO */
#  if SPT_TYPE == SPT_REUSEARGV
	if (LastArgv == NULL)
		return;

	if (i > LastArgv - Argv[0] - 2)
	{
		i = LastArgv - Argv[0] - 2;
		buf[i] = '\0';
	}
	(void) strlcpy(Argv[0], buf, i + 1);
	p = &Argv[0][i];
	while (p < LastArgv)
		*p++ = SPT_PADCHAR;
	Argv[1] = NULL;
#  endif /* SPT_TYPE == SPT_REUSEARGV */
#  if SPT_TYPE == SPT_CHANGEARGV
	Argv[0] = buf;
	Argv[1] = 0;
#  endif /* SPT_TYPE == SPT_CHANGEARGV */
# endif /* SPT_TYPE != SPT_NONE */
}

#endif /* SPT_TYPE != SPT_BUILTIN */
/*
**  SM_SETPROCTITLE -- set process task and set process title for ps
**
**	Possibly set process status and call setproctitle() to
**	change the ps display.
**
**	Parameters:
**		status -- whether or not to store as process status
**		e -- the current envelope.
**		fmt -- a printf style format string.
**		a, b, c -- possible parameters to fmt.
**
**	Returns:
**		none.
*/

/*VARARGS2*/
void
#ifdef __STDC__
sm_setproctitle(bool status, ENVELOPE *e, const char *fmt, ...)
#else /* __STDC__ */
sm_setproctitle(status, e, fmt, va_alist)
	bool status;
	ENVELOPE *e;
	const char *fmt;
	va_dcl
#endif /* __STDC__ */
{
	char buf[SPT_BUFSIZE];
	VA_LOCAL_DECL

	/* print the argument string */
	VA_START(fmt);
	(void) vsnprintf(buf, sizeof buf, fmt, ap);
	VA_END;

	if (status)
		proc_list_set(getpid(), buf);

	if (ProcTitlePrefix != NULL)
	{
		char prefix[SPT_BUFSIZE];

		expand(ProcTitlePrefix, prefix, sizeof prefix, e);
		setproctitle("%s: %s", prefix, buf);
	}
	else
		setproctitle("%s", buf);
}
/*
**  WAITFOR -- wait for a particular process id.
**
**	Parameters:
**		pid -- process id to wait for.
**
**	Returns:
**		status of pid.
**		-1 if pid never shows up.
**
**	Side Effects:
**		none.
*/

int
waitfor(pid)
	pid_t pid;
{
# ifdef WAITUNION
	union wait st;
# else /* WAITUNION */
	auto int st;
# endif /* WAITUNION */
	pid_t i;
# if defined(ISC_UNIX) || defined(_SCO_unix_)
	int savesig;
# endif /* defined(ISC_UNIX) || defined(_SCO_unix_) */

	do
	{
		errno = 0;
# if defined(ISC_UNIX) || defined(_SCO_unix_)
		savesig = releasesignal(SIGCHLD);
# endif /* defined(ISC_UNIX) || defined(_SCO_unix_) */
		i = wait(&st);
# if defined(ISC_UNIX) || defined(_SCO_unix_)
		if (savesig > 0)
			blocksignal(SIGCHLD);
# endif /* defined(ISC_UNIX) || defined(_SCO_unix_) */
		if (i > 0)
			(void) proc_list_drop(i);
	} while ((i >= 0 || errno == EINTR) && i != pid);
	if (i < 0)
		return -1;
# ifdef WAITUNION
	return st.w_status;
# else /* WAITUNION */
	return st;
# endif /* WAITUNION */
}
/*
**  REAPCHILD -- pick up the body of my child, lest it become a zombie
**
**	Parameters:
**		sig -- the signal that got us here (unused).
**
**	Returns:
**		none.
**
**	Side Effects:
**		Picks up extant zombies.
**		Control socket exits may restart/shutdown daemon.
*/

/* ARGSUSED0 */
SIGFUNC_DECL
reapchild(sig)
	int sig;
{
	int save_errno = errno;
	int st;
	pid_t pid;
#if HASWAITPID
	auto int status;
	int count;

	count = 0;
	while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
	{
		st = status;
		if (count++ > 1000)
		{
			if (LogLevel > 0)
				sm_syslog(LOG_ALERT, NOQID,
					"reapchild: waitpid loop: pid=%d, status=%x",
					pid, status);
			break;
		}
#else /* HASWAITPID */
# ifdef WNOHANG
	union wait status;

	while ((pid = wait3(&status, WNOHANG, (struct rusage *) NULL)) > 0)
	{
		st = status.w_status;
# else /* WNOHANG */
	auto int status;

	/*
	**  Catch one zombie -- we will be re-invoked (we hope) if there
	**  are more.  Unreliable signals probably break this, but this
	**  is the "old system" situation -- waitpid or wait3 are to be
	**  strongly preferred.
	*/

	if ((pid = wait(&status)) > 0)
	{
		st = status;
# endif /* WNOHANG */
#endif /* HASWAITPID */
		/* Drop PID and check if it was a control socket child */
		if (proc_list_drop(pid) == PROC_CONTROL &&
		    WIFEXITED(st))
		{
			/* if so, see if we need to restart or shutdown */
			if (WEXITSTATUS(st) == EX_RESTART)
			{
				/* emulate a SIGHUP restart */
				sighup(0);
				/* NOTREACHED */
			}
			else if (WEXITSTATUS(st) == EX_SHUTDOWN)
			{
				/* emulate a SIGTERM shutdown */
				intsig(0);
				/* NOTREACHED */
			}
		}
	}
#ifdef SYS5SIGNALS
	(void) setsignal(SIGCHLD, reapchild);
#endif /* SYS5SIGNALS */
	errno = save_errno;
	return SIGFUNC_RETURN;
}
/*
**  PUTENV -- emulation of putenv() in terms of setenv()
**
**	Not needed on Posix-compliant systems.
**	This doesn't have full Posix semantics, but it's good enough
**		for sendmail.
**
**	Parameter:
**		env -- the environment to put.
**
**	Returns:
**		none.
*/

#if NEEDPUTENV

# if NEEDPUTENV == 2		/* no setenv(3) call available */

int
putenv(str)
	char *str;
{
	char **current;
	int matchlen, envlen = 0;
	char *tmp;
	char **newenv;
	static bool first = TRUE;
	extern char **environ;

	/*
	 * find out how much of str to match when searching
	 * for a string to replace.
	 */
	if ((tmp = strchr(str, '=')) == NULL || tmp == str)
		matchlen = strlen(str);
	else
		matchlen = (int) (tmp - str);
	++matchlen;

	/*
	 * Search for an existing string in the environment and find the
	 * length of environ.  If found, replace and exit.
	 */
	for (current = environ; *current; current++)
	{
		++envlen;

		if (strncmp(str, *current, matchlen) == 0)
		{
			/* found it, now insert the new version */
			*current = (char *)str;
			return 0;
		}
	}

	/*
	 * There wasn't already a slot so add space for a new slot.
	 * If this is our first time through, use malloc(), else realloc().
	 */
	if (first)
	{
		newenv = (char **) malloc(sizeof(char *) * (envlen + 2));
		if (newenv == NULL)
			return -1;

		first = FALSE;
		(void) memcpy(newenv, environ, sizeof(char *) * envlen);
	}
	else
	{
		newenv = (char **) realloc((char *)environ, sizeof(char *) * (envlen + 2));
		if (newenv == NULL)
			return -1;
	}

	/* actually add in the new entry */
	environ = newenv;
	environ[envlen] = (char *)str;
	environ[envlen + 1] = NULL;

	return 0;
}

# else /* NEEDPUTENV == 2 */

int
putenv(env)
	char *env;
{
	char *p;
	int l;
	char nbuf[100];

	p = strchr(env, '=');
	if (p == NULL)
		return 0;
	l = p - env;
	if (l > sizeof nbuf - 1)
		l = sizeof nbuf - 1;
	memmove(nbuf, env, l);
	nbuf[l] = '\0';
	return setenv(nbuf, ++p, 1);
}

# endif /* NEEDPUTENV == 2 */
#endif /* NEEDPUTENV */
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

#if !HASUNSETENV

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

#endif /* !HASUNSETENV */
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
#endif /* SOLARIS */

int
getdtsize()
{
# ifdef RLIMIT_NOFILE
	struct rlimit rl;

	if (getrlimit(RLIMIT_NOFILE, &rl) >= 0)
		return rl.rlim_cur;
# endif /* RLIMIT_NOFILE */

# if HASGETDTABLESIZE
	return getdtablesize();
# else /* HASGETDTABLESIZE */
#  ifdef _SC_OPEN_MAX
	return sysconf(_SC_OPEN_MAX);
#  else /* _SC_OPEN_MAX */
	return NOFILE;
#  endif /* _SC_OPEN_MAX */
# endif /* HASGETDTABLESIZE */
}
/*
**  UNAME -- get the UUCP name of this system.
*/

#if !HASUNAME

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
			return 0;
	}

	/* try /usr/include/whoami.h -- has a #define somewhere */
	if ((file = fopen("/usr/include/whoami.h", "r")) != NULL)
	{
		char buf[MAXLINE];

		while (fgets(buf, MAXLINE, file) != NULL)
		{
			if (sscanf(buf, "#define sysname \"%*[^\"]\"",
					NODE_LENGTH, name->nodename) > 0)
				break;
		}
		(void) fclose(file);
		if (name->nodename[0] != '\0')
			return 0;
	}

#  if 0
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
			return 0;
	}
#  endif /* 0 */

	return -1;
}
#endif /* !HASUNAME */
/*
**  INITGROUPS -- initialize groups
**
**	Stub implementation for System V style systems
*/

#if !HASINITGROUPS

initgroups(name, basegid)
	char *name;
	int basegid;
{
	return 0;
}

#endif /* !HASINITGROUPS */
/*
**  SETGROUPS -- set group list
**
**	Stub implementation for systems that don't have group lists
*/

#ifndef NGROUPS_MAX

int
setgroups(ngroups, grouplist)
	int ngroups;
	GIDSET_T grouplist[];
{
	return 0;
}

#endif /* ! NGROUPS_MAX */
/*
**  SETSID -- set session id (for non-POSIX systems)
*/

#if !HASSETSID

pid_t
setsid __P ((void))
{
#  ifdef TIOCNOTTY
	int fd;

	fd = open("/dev/tty", O_RDWR, 0);
	if (fd >= 0)
	{
		(void) ioctl(fd, TIOCNOTTY, (char *) 0);
		(void) close(fd);
	}
#  endif /* TIOCNOTTY */
#  ifdef SYS5SETPGRP
	return setpgrp();
#  else /* SYS5SETPGRP */
	return setpgid(0, getpid());
#  endif /* SYS5SETPGRP */
}

#endif /* !HASSETSID */
/*
**  FSYNC -- dummy fsync
*/

#if NEEDFSYNC

fsync(fd)
	int fd;
{
# ifdef O_SYNC
	return fcntl(fd, F_SETFL, O_SYNC);
# else /* O_SYNC */
	/* nothing we can do */
	return 0;
# endif /* O_SYNC */
}

#endif /* NEEDFSYNC */
/*
**  DGUX_INET_ADDR -- inet_addr for DG/UX
**
**	Data General DG/UX version of inet_addr returns a struct in_addr
**	instead of a long.  This patches things.  Only needed on versions
**	prior to 5.4.3.
*/

#ifdef DGUX_5_4_2

# undef inet_addr

long
dgux_inet_addr(host)
	char *host;
{
	struct in_addr haddr;

	haddr = inet_addr(host);
	return haddr.s_addr;
}

#endif /* DGUX_5_4_2 */
/*
**  GETOPT -- for old systems or systems with bogus implementations
*/

#if NEEDGETOPT

/*
 * Copyright (c) 1985 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */


/*
**  this version hacked to add `atend' flag to allow state machine
**  to reset if invoked by the program to scan args for a 2nd time
*/

# if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)getopt.c	4.3 (Berkeley) 3/9/86";
# endif /* defined(LIBC_SCCS) && !defined(lint) */

/*
 * get option letter from argument vector
 */
# ifdef _CONVEX_SOURCE
extern int	optind, opterr, optopt;
extern char	*optarg;
# else /* _CONVEX_SOURCE */
int	opterr = 1;		/* if error message should be printed */
int	optind = 1;		/* index into parent argv vector */
int	optopt = 0;		/* character checked for validity */
char	*optarg = NULL;		/* argument associated with option */
# endif /* _CONVEX_SOURCE */

# define BADCH	(int)'?'
# define EMSG	""
# define tell(s)	if (opterr) {fputs(*nargv,stderr);fputs(s,stderr); \
			fputc(optopt,stderr);fputc('\n',stderr);return(BADCH);}

int
getopt(nargc,nargv,ostr)
	int		nargc;
	char *const	*nargv;
	const char	*ostr;
{
	static char	*place = EMSG;	/* option letter processing */
	static char	atend = 0;
	register char	*oli = NULL;	/* option letter list index */

	if (atend) {
		atend = 0;
		place = EMSG;
	}
	if(!*place) {			/* update scanning pointer */
		if (optind >= nargc || *(place = nargv[optind]) != '-' || !*++place) {
			atend++;
			return -1;
		}
		if (*place == '-') {	/* found "--" */
			++optind;
			atend++;
			return -1;
		}
	}				/* option letter okay? */
	if ((optopt = (int)*place++) == (int)':' || !(oli = strchr(ostr,optopt))) {
		if (!*place) ++optind;
		tell(": illegal option -- ");
	}
	if (oli && *++oli != ':') {		/* don't need argument */
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

#endif /* NEEDGETOPT */
/*
**  VFPRINTF, VSPRINTF -- for old 4.3 BSD systems missing a real version
*/

#if NEEDVPRINTF

# define MAXARG	16

vfprintf(fp, fmt, ap)
	FILE *fp;
	char *fmt;
	char **ap;
{
	char *bp[MAXARG];
	int i = 0;

	while (*ap && i < MAXARG)
		bp[i++] = *ap++;
	fprintf(fp, fmt, bp[0], bp[1], bp[2], bp[3],
			 bp[4], bp[5], bp[6], bp[7],
			 bp[8], bp[9], bp[10], bp[11],
			 bp[12], bp[13], bp[14], bp[15]);
}

vsprintf(s, fmt, ap)
	char *s;
	char *fmt;
	char **ap;
{
	char *bp[MAXARG];
	int i = 0;

	while (*ap && i < MAXARG)
		bp[i++] = *ap++;
	sprintf(s, fmt, bp[0], bp[1], bp[2], bp[3],
			bp[4], bp[5], bp[6], bp[7],
			bp[8], bp[9], bp[10], bp[11],
			bp[12], bp[13], bp[14], bp[15]);
}

#endif /* NEEDVPRINTF */
/*
**  USERSHELLOK -- tell if a user's shell is ok for unrestricted use
**
**	Parameters:
**		user -- the name of the user we are checking.
**		shell -- the user's shell from /etc/passwd
**
**	Returns:
**		TRUE -- if it is ok to use this for unrestricted access.
**		FALSE -- if the shell is restricted.
*/

#if !HASGETUSERSHELL

# ifndef _PATH_SHELLS
#  define _PATH_SHELLS	"/etc/shells"
# endif /* ! _PATH_SHELLS */

# if defined(_AIX3) || defined(_AIX4)
#  include <userconf.h>
#  if _AIX4 >= 40200
#   include <userpw.h>
#  endif /* _AIX4 >= 40200 */
#  include <usersec.h>
# endif /* defined(_AIX3) || defined(_AIX4) */

static char	*DefaultUserShells[] =
{
	"/bin/sh",		/* standard shell */
	"/usr/bin/sh",
	"/bin/csh",		/* C shell */
	"/usr/bin/csh",
# ifdef __hpux
#  ifdef V4FS
	"/usr/bin/rsh",		/* restricted Bourne shell */
	"/usr/bin/ksh",		/* Korn shell */
	"/usr/bin/rksh",	/* restricted Korn shell */
	"/usr/bin/pam",
	"/usr/bin/keysh",	/* key shell (extended Korn shell) */
	"/usr/bin/posix/sh",
#  else /* V4FS */
	"/bin/rsh",		/* restricted Bourne shell */
	"/bin/ksh",		/* Korn shell */
	"/bin/rksh",		/* restricted Korn shell */
	"/bin/pam",
	"/usr/bin/keysh",	/* key shell (extended Korn shell) */
	"/bin/posix/sh",
#  endif /* V4FS */
# endif /* __hpux */
# if defined(_AIX3) || defined(_AIX4)
	"/bin/ksh",		/* Korn shell */
	"/usr/bin/ksh",
	"/bin/tsh",		/* trusted shell */
	"/usr/bin/tsh",
	"/bin/bsh",		/* Bourne shell */
	"/usr/bin/bsh",
# endif /* defined(_AIX3) || defined(_AIX4) */
# if defined(__svr4__) || defined(__svr5__)
	"/bin/ksh",		/* Korn shell */
	"/usr/bin/ksh",
# endif /* defined(__svr4__) || defined(__svr5__) */
# ifdef sgi
	"/sbin/sh",		/* SGI's shells really live in /sbin */
	"/sbin/csh",
	"/bin/ksh",		/* Korn shell */
	"/sbin/ksh",
	"/usr/bin/ksh",
	"/bin/tcsh",		/* Extended csh */
	"/usr/bin/tcsh",
# endif /* sgi */
	NULL
};

#endif /* !HASGETUSERSHELL */

#define WILDCARD_SHELL	"/SENDMAIL/ANY/SHELL/"

bool
usershellok(user, shell)
	char *user;
	char *shell;
{
# if HASGETUSERSHELL
	register char *p;
	extern char *getusershell();

	if (shell == NULL || shell[0] == '\0' || wordinclass(user, 't') ||
	    ConfigLevel <= 1)
		return TRUE;

	setusershell();
	while ((p = getusershell()) != NULL)
		if (strcmp(p, shell) == 0 || strcmp(p, WILDCARD_SHELL) == 0)
			break;
	endusershell();
	return p != NULL;
# else /* HASGETUSERSHELL */
#  if USEGETCONFATTR
	auto char *v;
#  endif /* USEGETCONFATTR */
	register FILE *shellf;
	char buf[MAXLINE];

	if (shell == NULL || shell[0] == '\0' || wordinclass(user, 't') ||
	    ConfigLevel <= 1)
		return TRUE;

#  if USEGETCONFATTR
	/*
	**  Naturally IBM has a "better" idea.....
	**
	**	What a crock.  This interface isn't documented, it is
	**	considered part of the security library (-ls), and it
	**	only works if you are running as root (since the list
	**	of valid shells is obviously a source of great concern).
	**	I recommend that you do NOT define USEGETCONFATTR,
	**	especially since you are going to have to set up an
	**	/etc/shells anyhow to handle the cases where getconfattr
	**	fails.
	*/

	if (getconfattr(SC_SYS_LOGIN, SC_SHELLS, &v, SEC_LIST) == 0 && v != NULL)
	{
		while (*v != '\0')
		{
			if (strcmp(v, shell) == 0 || strcmp(v, WILDCARD_SHELL) == 0)
				return TRUE;
			v += strlen(v) + 1;
		}
		return FALSE;
	}
#  endif /* USEGETCONFATTR */

	shellf = fopen(_PATH_SHELLS, "r");
	if (shellf == NULL)
	{
		/* no /etc/shells; see if it is one of the std shells */
		char **d;

		if (errno != ENOENT && LogLevel > 3)
			sm_syslog(LOG_ERR, NOQID,
				  "usershellok: cannot open %s: %s",
				  _PATH_SHELLS, errstring(errno));

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
		while (*p != '\0' && *p != '#' && !(isascii(*p) && isspace(*p)))
			p++;
		*p = '\0';
		if (strcmp(shell, q) == 0 || strcmp(WILDCARD_SHELL, q) == 0)
		{
			(void) fclose(shellf);
			return TRUE;
		}
	}
	(void) fclose(shellf);
	return FALSE;
# endif /* HASGETUSERSHELL */
}
/*
**  FREEDISKSPACE -- see how much free space is on the queue filesystem
**
**	Only implemented if you have statfs.
**
**	Parameters:
**		dir -- the directory in question.
**		bsize -- a variable into which the filesystem
**			block size is stored.
**
**	Returns:
**		The number of blocks free on the queue filesystem.
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
#define SFS_STATVFS	6	/* use <sys/statvfs.h> implementation */

#ifndef SFS_TYPE
# define SFS_TYPE	SFS_NONE
#endif /* ! SFS_TYPE */

#if SFS_TYPE == SFS_USTAT
# include <ustat.h>
#endif /* SFS_TYPE == SFS_USTAT */
#if SFS_TYPE == SFS_4ARGS || SFS_TYPE == SFS_STATFS
# include <sys/statfs.h>
#endif /* SFS_TYPE == SFS_4ARGS || SFS_TYPE == SFS_STATFS */
#if SFS_TYPE == SFS_VFS
# include <sys/vfs.h>
#endif /* SFS_TYPE == SFS_VFS */
#if SFS_TYPE == SFS_MOUNT
# include <sys/mount.h>
#endif /* SFS_TYPE == SFS_MOUNT */
#if SFS_TYPE == SFS_STATVFS
# include <sys/statvfs.h>
#endif /* SFS_TYPE == SFS_STATVFS */

long
freediskspace(dir, bsize)
	char *dir;
	long *bsize;
{
# if SFS_TYPE != SFS_NONE
#  if SFS_TYPE == SFS_USTAT
	struct ustat fs;
	struct stat statbuf;
#   define FSBLOCKSIZE	DEV_BSIZE
#   define SFS_BAVAIL	f_tfree
#  else /* SFS_TYPE == SFS_USTAT */
#   if defined(ultrix)
	struct fs_data fs;
#    define SFS_BAVAIL	fd_bfreen
#    define FSBLOCKSIZE	1024L
#   else /* defined(ultrix) */
#    if SFS_TYPE == SFS_STATVFS
	struct statvfs fs;
#     define FSBLOCKSIZE	fs.f_frsize
#    else /* SFS_TYPE == SFS_STATVFS */
	struct statfs fs;
#     define FSBLOCKSIZE	fs.f_bsize
#    endif /* SFS_TYPE == SFS_STATVFS */
#   endif /* defined(ultrix) */
#  endif /* SFS_TYPE == SFS_USTAT */
#  ifndef SFS_BAVAIL
#   define SFS_BAVAIL f_bavail
#  endif /* ! SFS_BAVAIL */

#  if SFS_TYPE == SFS_USTAT
	if (stat(dir, &statbuf) == 0 && ustat(statbuf.st_dev, &fs) == 0)
#  else /* SFS_TYPE == SFS_USTAT */
#   if SFS_TYPE == SFS_4ARGS
	if (statfs(dir, &fs, sizeof fs, 0) == 0)
#   else /* SFS_TYPE == SFS_4ARGS */
#    if SFS_TYPE == SFS_STATVFS
	if (statvfs(dir, &fs) == 0)
#    else /* SFS_TYPE == SFS_STATVFS */
#     if defined(ultrix)
	if (statfs(dir, &fs) > 0)
#     else /* defined(ultrix) */
	if (statfs(dir, &fs) == 0)
#     endif /* defined(ultrix) */
#    endif /* SFS_TYPE == SFS_STATVFS */
#   endif /* SFS_TYPE == SFS_4ARGS */
#  endif /* SFS_TYPE == SFS_USTAT */
	{
		if (bsize != NULL)
			*bsize = FSBLOCKSIZE;
		if (fs.SFS_BAVAIL <= 0)
			return 0;
		else if (fs.SFS_BAVAIL > LONG_MAX)
			return (long) LONG_MAX;
		else
			return (long) fs.SFS_BAVAIL;
	}
# endif /* SFS_TYPE != SFS_NONE */
	return -1;
}
/*
**  ENOUGHDISKSPACE -- is there enough free space on the queue fs?
**
**	Only implemented if you have statfs.
**
**	Parameters:
**		msize -- the size to check against.  If zero, we don't yet
**		know how big the message will be, so just check for
**		a "reasonable" amount.
**		log -- log message?
**
**	Returns:
**		TRUE if there is enough space.
**		FALSE otherwise.
*/

bool
enoughdiskspace(msize, log)
	long msize;
	bool log;
{
	long bfree;
	long bsize;

	if (MinBlocksFree <= 0 && msize <= 0)
	{
		if (tTd(4, 80))
			dprintf("enoughdiskspace: no threshold\n");
		return TRUE;
	}

	bfree = freediskspace(QueueDir, &bsize);
	if (bfree >= 0)
	{
		if (tTd(4, 80))
			dprintf("enoughdiskspace: bavail=%ld, need=%ld\n",
				bfree, msize);

		/* convert msize to block count */
		msize = msize / bsize + 1;
		if (MinBlocksFree >= 0)
			msize += MinBlocksFree;

		if (bfree < msize)
		{
			if (log && LogLevel > 0)
				sm_syslog(LOG_ALERT, CurEnv->e_id,
					"low on space (have %ld, %s needs %ld in %s)",
					bfree,
					CurHostName == NULL ? "SMTP-DAEMON" : CurHostName,
					msize, QueueDir);
			return FALSE;
		}
	}
	else if (tTd(4, 80))
		dprintf("enoughdiskspace failure: min=%ld, need=%ld: %s\n",
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
#endif /* ETIMEDOUT */
#ifdef ESTALE
	  case ESTALE:			/* Stale NFS file handle */
#endif /* ESTALE */
#ifdef ENETDOWN
	  case ENETDOWN:		/* Network is down */
#endif /* ENETDOWN */
#ifdef ENETUNREACH
	  case ENETUNREACH:		/* Network is unreachable */
#endif /* ENETUNREACH */
#ifdef ENETRESET
	  case ENETRESET:		/* Network dropped connection on reset */
#endif /* ENETRESET */
#ifdef ECONNABORTED
	  case ECONNABORTED:		/* Software caused connection abort */
#endif /* ECONNABORTED */
#ifdef ECONNRESET
	  case ECONNRESET:		/* Connection reset by peer */
#endif /* ECONNRESET */
#ifdef ENOBUFS
	  case ENOBUFS:			/* No buffer space available */
#endif /* ENOBUFS */
#ifdef ESHUTDOWN
	  case ESHUTDOWN:		/* Can't send after socket shutdown */
#endif /* ESHUTDOWN */
#ifdef ECONNREFUSED
	  case ECONNREFUSED:		/* Connection refused */
#endif /* ECONNREFUSED */
#ifdef EHOSTDOWN
	  case EHOSTDOWN:		/* Host is down */
#endif /* EHOSTDOWN */
#ifdef EHOSTUNREACH
	  case EHOSTUNREACH:		/* No route to host */
#endif /* EHOSTUNREACH */
#ifdef EDQUOT
	  case EDQUOT:			/* Disc quota exceeded */
#endif /* EDQUOT */
#ifdef EPROCLIM
	  case EPROCLIM:		/* Too many processes */
#endif /* EPROCLIM */
#ifdef EUSERS
	  case EUSERS:			/* Too many users */
#endif /* EUSERS */
#ifdef EDEADLK
	  case EDEADLK:			/* Resource deadlock avoided */
#endif /* EDEADLK */
#ifdef EISCONN
	  case EISCONN:			/* Socket already connected */
#endif /* EISCONN */
#ifdef EINPROGRESS
	  case EINPROGRESS:		/* Operation now in progress */
#endif /* EINPROGRESS */
#ifdef EALREADY
	  case EALREADY:		/* Operation already in progress */
#endif /* EALREADY */
#ifdef EADDRINUSE
	  case EADDRINUSE:		/* Address already in use */
#endif /* EADDRINUSE */
#ifdef EADDRNOTAVAIL
	  case EADDRNOTAVAIL:		/* Can't assign requested address */
#endif /* EADDRNOTAVAIL */
#ifdef ETXTBSY
	  case ETXTBSY:			/* (Apollo) file locked */
#endif /* ETXTBSY */
#if defined(ENOSR) && (!defined(ENOBUFS) || (ENOBUFS != ENOSR))
	  case ENOSR:			/* Out of streams resources */
#endif /* defined(ENOSR) && (!defined(ENOBUFS) || (ENOBUFS != ENOSR)) */
#ifdef ENOLCK
	  case ENOLCK:			/* No locks available */
#endif /* ENOLCK */
	  case E_SM_OPENTIMEOUT:	/* PSEUDO: open timed out */
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
	int i;
	int save_errno;
# if !HASFLOCK
	int action;
	struct flock lfd;

	if (ext == NULL)
		ext = "";

	memset(&lfd, '\0', sizeof lfd);
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
		dprintf("lockfile(%s%s, action=%d, type=%d): ",
			filename, ext, action, lfd.l_type);

	while ((i = fcntl(fd, action, &lfd)) < 0 && errno == EINTR)
		continue;
	if (i >= 0)
	{
		if (tTd(55, 60))
			dprintf("SUCCESS\n");
		return TRUE;
	}
	save_errno = errno;

	if (tTd(55, 60))
		dprintf("(%s) ", errstring(save_errno));

	/*
	**  On SunOS, if you are testing using -oQ/tmp/mqueue or
	**  -oA/tmp/aliases or anything like that, and /tmp is mounted
	**  as type "tmp" (that is, served from swap space), the
	**  previous fcntl will fail with "Invalid argument" errors.
	**  Since this is fairly common during testing, we will assume
	**  that this indicates that the lock is successfully grabbed.
	*/

	if (save_errno == EINVAL)
	{
		if (tTd(55, 60))
			dprintf("SUCCESS\n");
		return TRUE;
	}

	if (!bitset(LOCK_NB, type) ||
	    (save_errno != EACCES && save_errno != EAGAIN))
	{
		int omode = -1;
#  ifdef F_GETFL
		(void) fcntl(fd, F_GETFL, &omode);
		errno = save_errno;
#  endif /* F_GETFL */
		syserr("cannot lockf(%s%s, fd=%d, type=%o, omode=%o, euid=%d)",
			filename, ext, fd, type, omode, geteuid());
		dumpfd(fd, TRUE, TRUE);
	}
# else /* !HASFLOCK */
	if (ext == NULL)
		ext = "";

	if (tTd(55, 60))
		dprintf("lockfile(%s%s, type=%o): ", filename, ext, type);

	while ((i = flock(fd, type)) < 0 && errno == EINTR)
		continue;
	if (i >= 0)
	{
		if (tTd(55, 60))
			dprintf("SUCCESS\n");
		return TRUE;
	}
	save_errno = errno;

	if (tTd(55, 60))
		dprintf("(%s) ", errstring(save_errno));

	if (!bitset(LOCK_NB, type) || save_errno != EWOULDBLOCK)
	{
		int omode = -1;
#  ifdef F_GETFL
		(void) fcntl(fd, F_GETFL, &omode);
		errno = save_errno;
#  endif /* F_GETFL */
		syserr("cannot flock(%s%s, fd=%d, type=%o, omode=%o, euid=%d)",
			filename, ext, fd, type, omode, geteuid());
		dumpfd(fd, TRUE, TRUE);
	}
# endif /* !HASFLOCK */
	if (tTd(55, 60))
		dprintf("FAIL\n");
	errno = save_errno;
	return FALSE;
}
/*
**  CHOWNSAFE -- tell if chown is "safe" (executable only by root)
**
**	Unfortunately, given that we can't predict other systems on which
**	a remote mounted (NFS) filesystem will be mounted, the answer is
**	almost always that this is unsafe.
**
**	Note also that many operating systems have non-compliant
**	implementations of the _POSIX_CHOWN_RESTRICTED variable and the
**	fpathconf() routine.  According to IEEE 1003.1-1990, if
**	_POSIX_CHOWN_RESTRICTED is defined and not equal to -1, then
**	no non-root process can give away the file.  However, vendors
**	don't take NFS into account, so a comfortable value of
**	_POSIX_CHOWN_RESTRICTED tells us nothing.
**
**	Also, some systems (e.g., IRIX 6.2) return 1 from fpathconf()
**	even on files where chown is not restricted.  Many systems get
**	this wrong on NFS-based filesystems (that is, they say that chown
**	is restricted [safe] on NFS filesystems where it may not be, since
**	other systems can access the same filesystem and do file giveaway;
**	only the NFS server knows for sure!)  Hence, it is important to
**	get the value of SAFENFSPATHCONF correct -- it should be defined
**	_only_ after testing (see test/t_pathconf.c) a system on an unsafe
**	NFS-based filesystem to ensure that you can get meaningful results.
**	If in doubt, assume unsafe!
**
**	You may also need to tweak IS_SAFE_CHOWN -- it should be a
**	condition indicating whether the return from pathconf indicates
**	that chown is safe (typically either > 0 or >= 0 -- there isn't
**	even any agreement about whether a zero return means that a file
**	is or is not safe).  It defaults to "> 0".
**
**	If the parent directory is safe (writable only by owner back
**	to the root) then we can relax slightly and trust fpathconf
**	in more circumstances.  This is really a crock -- if this is an
**	NFS mounted filesystem then we really know nothing about the
**	underlying implementation.  However, most systems pessimize and
**	return an error (EINVAL or EOPNOTSUPP) on NFS filesystems, which
**	we interpret as unsafe, as we should.  Thus, this heuristic gets
**	us into a possible problem only on systems that have a broken
**	pathconf implementation and which are also poorly configured
**	(have :include: files in group- or world-writable directories).
**
**	Parameters:
**		fd -- the file descriptor to check.
**		safedir -- set if the parent directory is safe.
**
**	Returns:
**		TRUE -- if the chown(2) operation is "safe" -- that is,
**			only root can chown the file to an arbitrary user.
**		FALSE -- if an arbitrary user can give away a file.
*/

#ifndef IS_SAFE_CHOWN
# define IS_SAFE_CHOWN	> 0
#endif /* ! IS_SAFE_CHOWN */

bool
chownsafe(fd, safedir)
	int fd;
	bool safedir;
{
# if (!defined(_POSIX_CHOWN_RESTRICTED) || _POSIX_CHOWN_RESTRICTED != -1) && \
    (defined(_PC_CHOWN_RESTRICTED) || defined(_GNU_TYPES_H))
	int rval;

	/* give the system administrator a chance to override */
	if (bitnset(DBS_ASSUMESAFECHOWN, DontBlameSendmail))
		return TRUE;

	/*
	**  Some systems (e.g., SunOS) seem to have the call and the
	**  #define _PC_CHOWN_RESTRICTED, but don't actually implement
	**  the call.  This heuristic checks for that.
	*/

	errno = 0;
	rval = fpathconf(fd, _PC_CHOWN_RESTRICTED);
#  if SAFENFSPATHCONF
	return errno == 0 && rval IS_SAFE_CHOWN;
#  else /* SAFENFSPATHCONF */
	return safedir && errno == 0 && rval IS_SAFE_CHOWN;
#  endif /* SAFENFSPATHCONF */
# else /* (!defined(_POSIX_CHOWN_RESTRICTED) || _POSIX_CHOWN_RESTRICTED != -1) && \ */
	return bitnset(DBS_ASSUMESAFECHOWN, DontBlameSendmail);
# endif /* (!defined(_POSIX_CHOWN_RESTRICTED) || _POSIX_CHOWN_RESTRICTED != -1) && \ */
}
/*
**  RESETLIMITS -- reset system controlled resource limits
**
**	This is to avoid denial-of-service attacks
**
**	Parameters:
**		none
**
**	Returns:
**		none
*/

#if HASSETRLIMIT
# ifdef RLIMIT_NEEDS_SYS_TIME_H
#  include <sys/time.h>
# endif /* RLIMIT_NEEDS_SYS_TIME_H */
# include <sys/resource.h>
#endif /* HASSETRLIMIT */
#ifndef FD_SETSIZE
# define FD_SETSIZE	256
#endif /* ! FD_SETSIZE */

void
resetlimits()
{
#if HASSETRLIMIT
	struct rlimit lim;

	lim.rlim_cur = lim.rlim_max = RLIM_INFINITY;
	(void) setrlimit(RLIMIT_CPU, &lim);
	(void) setrlimit(RLIMIT_FSIZE, &lim);
# ifdef RLIMIT_NOFILE
	lim.rlim_cur = lim.rlim_max = FD_SETSIZE;
	(void) setrlimit(RLIMIT_NOFILE, &lim);
# endif /* RLIMIT_NOFILE */
#else /* HASSETRLIMIT */
# if HASULIMIT
	(void) ulimit(2, 0x3fffff);
	(void) ulimit(4, FD_SETSIZE);
# endif /* HASULIMIT */
#endif /* HASSETRLIMIT */
	errno = 0;
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
#if NETINFO
	{
		char *cflocation;

		cflocation = ni_propval("/locations", NULL, "sendmail",
					"sendmail.cf", '\0');
		if (cflocation != NULL)
			return cflocation;
	}
#endif /* NETINFO */

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
	{
		VendorCode = VENDOR_BERKELEY;
		return TRUE;
	}

	/* add vendor extensions here */

#ifdef SUN_EXTENSIONS
	if (strcasecmp(vendor, "Sun") == 0)
	{
		VendorCode = VENDOR_SUN;
		return TRUE;
	}
#endif /* SUN_EXTENSIONS */

#if defined(VENDOR_NAME) && defined(VENDOR_CODE)
	if (strcasecmp(vendor, VENDOR_NAME) == 0)
	{
		VendorCode = VENDOR_CODE;
		return TRUE;
	}
#endif /* defined(VENDOR_NAME) && defined(VENDOR_CODE) */

	return FALSE;
}
/*
**  GETVENDOR -- return vendor name based on vendor code
**
**	Parameters:
**		vendorcode -- numeric representation of vendor.
**
**	Returns:
**		string containing vendor name.
*/

char *
getvendor(vendorcode)
	int vendorcode;
{
#if defined(VENDOR_NAME) && defined(VENDOR_CODE)
	/*
	**  Can't have the same switch case twice so need to
	**  handle VENDOR_CODE outside of switch.  It might
	**  match one of the existing VENDOR_* codes.
	*/

	if (vendorcode == VENDOR_CODE)
		return VENDOR_NAME;
#endif /* defined(VENDOR_NAME) && defined(VENDOR_CODE) */

	switch (vendorcode)
	{
		case VENDOR_BERKELEY:
			return "Berkeley";

		case VENDOR_SUN:
			return "Sun";

		case VENDOR_HP:
			return "HP";

		case VENDOR_IBM:
			return "IBM";

		case VENDOR_SENDMAIL:
			return "Sendmail";

		default:
			return "Unknown";
	}
}
/*
**  VENDOR_PRE_DEFAULTS, VENDOR_POST_DEFAULTS -- set vendor-specific defaults
**
**	Vendor_pre_defaults is called before reading the configuration
**	file; vendor_post_defaults is called immediately after.
**
**	Parameters:
**		e -- the global environment to initialize.
**
**	Returns:
**		none.
*/

#if SHARE_V1
int	DefShareUid;	/* default share uid to run as -- unused??? */
#endif /* SHARE_V1 */

void
vendor_pre_defaults(e)
	ENVELOPE *e;
{
#if SHARE_V1
	/* OTHERUID is defined in shares.h, do not be alarmed */
	DefShareUid = OTHERUID;
#endif /* SHARE_V1 */
#if defined(SUN_EXTENSIONS) && defined(SUN_DEFAULT_VALUES)
	sun_pre_defaults(e);
#endif /* defined(SUN_EXTENSIONS) && defined(SUN_DEFAULT_VALUES) */
#ifdef apollo
	/*
	**  stupid domain/os can't even open
	**  /etc/mail/sendmail.cf without this
	*/

	setuserenv("ISP", NULL);
	setuserenv("SYSTYPE", NULL);
#endif /* apollo */
}


void
vendor_post_defaults(e)
	ENVELOPE *e;
{
#ifdef __QNX__
	char *p;

	/* Makes sure the SOCK environment variable remains */
	if (p = getextenv("SOCK"))
		setuserenv("SOCK", p);
#endif /* __QNX__ */
#if defined(SUN_EXTENSIONS) && defined(SUN_DEFAULT_VALUES)
	sun_post_defaults(e);
#endif /* defined(SUN_EXTENSIONS) && defined(SUN_DEFAULT_VALUES) */
}
/*
**  VENDOR_DAEMON_SETUP -- special vendor setup needed for daemon mode
*/

void
vendor_daemon_setup(e)
	ENVELOPE *e;
{
#if HASSETLOGIN
	(void) setlogin(RunAsUserName);
#endif /* HASSETLOGIN */
#if SECUREWARE
	if (getluid() != -1)
	{
		usrerr("Daemon cannot have LUID");
		finis(FALSE, EX_USAGE);
	}
#endif /* SECUREWARE */
}
/*
**  VENDOR_SET_UID -- do setup for setting a user id
**
**	This is called when we are still root.
**
**	Parameters:
**		uid -- the uid we are about to become.
**
**	Returns:
**		none.
*/

void
vendor_set_uid(uid)
	UID_T uid;
{
	/*
	**  We need to setup the share groups (lnodes)
	**  and add auditing information (luid's)
	**  before we loose our ``root''ness.
	*/
#if SHARE_V1
	if (setupshares(uid, syserr) != 0)
		syserr("Unable to set up shares");
#endif /* SHARE_V1 */
#if SECUREWARE
	(void) setup_secure(uid);
#endif /* SECUREWARE */
}
/*
**  VALIDATE_CONNECTION -- check connection for rationality
**
**	If the connection is rejected, this routine should log an
**	appropriate message -- but should never issue any SMTP protocol.
**
**	Parameters:
**		sap -- a pointer to a SOCKADDR naming the peer.
**		hostname -- the name corresponding to sap.
**		e -- the current envelope.
**
**	Returns:
**		error message from rejection.
**		NULL if not rejected.
*/

#if TCPWRAPPERS
# include <tcpd.h>

/* tcpwrappers does no logging, but you still have to declare these -- ugh */
int	allow_severity	= LOG_INFO;
int	deny_severity	= LOG_NOTICE;
#endif /* TCPWRAPPERS */

#if DAEMON
char *
validate_connection(sap, hostname, e)
	SOCKADDR *sap;
	char *hostname;
	ENVELOPE *e;
{
# if TCPWRAPPERS
	char *host;
# endif /* TCPWRAPPERS */

	if (tTd(48, 3))
		dprintf("validate_connection(%s, %s)\n",
			hostname, anynet_ntoa(sap));

	if (rscheck("check_relay", hostname, anynet_ntoa(sap),
		    e, TRUE, TRUE, 4) != EX_OK)
	{
		static char reject[BUFSIZ*2];
		extern char MsgBuf[];

		if (tTd(48, 4))
			dprintf("  ... validate_connection: BAD (rscheck)\n");

		if (strlen(MsgBuf) >= 3)
			(void) strlcpy(reject, MsgBuf, sizeof reject);
		else
			(void) strlcpy(reject, "Access denied", sizeof reject);

		return reject;
	}

# if TCPWRAPPERS
	if (hostname[0] == '[' && hostname[strlen(hostname) - 1] == ']')
		host = "unknown";
	else
		host = hostname;
	if (!hosts_ctl("sendmail", host, anynet_ntoa(sap), STRING_UNKNOWN))
	{
		if (tTd(48, 4))
			dprintf("  ... validate_connection: BAD (tcpwrappers)\n");
		if (LogLevel >= 4)
			sm_syslog(LOG_NOTICE, e->e_id,
				"tcpwrappers (%s, %s) rejection",
				host, anynet_ntoa(sap));
		return "Access denied";
	}
# endif /* TCPWRAPPERS */
	if (tTd(48, 4))
		dprintf("  ... validate_connection: OK\n");
	return NULL;
}

#endif /* DAEMON */
/*
**  STRTOL -- convert string to long integer
**
**	For systems that don't have it in the C library.
**
**	This is taken verbatim from the 4.4-Lite C library.
*/

#if NEEDSTRTOL

# if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)strtol.c	8.1 (Berkeley) 6/4/93";
# endif /* defined(LIBC_SCCS) && !defined(lint) */

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
	return acc;
}

#endif /* NEEDSTRTOL */
/*
**  STRSTR -- find first substring in string
**
**	Parameters:
**		big -- the big (full) string.
**		little -- the little (sub) string.
**
**	Returns:
**		A pointer to the first instance of little in big.
**		big if little is the null string.
**		NULL if little is not contained in big.
*/

#if NEEDSTRSTR

char *
strstr(big, little)
	char *big;
	char *little;
{
	register char *p = big;
	int l;

	if (*little == '\0')
		return big;
	l = strlen(little);

	while ((p = strchr(p, *little)) != NULL)
	{
		if (strncmp(p, little, l) == 0)
			return p;
		p++;
	}
	return NULL;
}

#endif /* NEEDSTRSTR */
/*
**  SM_GETHOSTBY{NAME,ADDR} -- compatibility routines for gethostbyXXX
**
**	Some operating systems have wierd problems with the gethostbyXXX
**	routines.  For example, Solaris versions at least through 2.3
**	don't properly deliver a canonical h_name field.  This tries to
**	work around these problems.
**
**	Support IPv6 as well as IPv4.
*/

#if NETINET6 && NEEDSGETIPNODE && __RES < 19990909

# ifndef AI_DEFAULT
#  define AI_DEFAULT	0	/* dummy */
# endif /* ! AI_DEFAULT */
# ifndef AI_ADDRCONFIG
#  define AI_ADDRCONFIG	0	/* dummy */
# endif /* ! AI_ADDRCONFIG */
# ifndef AI_V4MAPPED
#  define AI_V4MAPPED	0	/* dummy */
# endif /* ! AI_V4MAPPED */
# ifndef AI_ALL
#  define AI_ALL	0	/* dummy */
# endif /* ! AI_ALL */

static struct hostent *
getipnodebyname(name, family, flags, err)
	char *name;
	int family;
	int flags;
	int *err;
{
	bool resv6 = TRUE;
	struct hostent *h;

	if (family == AF_INET6)
	{
		/* From RFC2133, section 6.1 */
		resv6 = bitset(RES_USE_INET6, _res.options);
		_res.options |= RES_USE_INET6;
	}
	h_errno = 0;
	h = gethostbyname(name);
	*err = h_errno;
	if (family == AF_INET6 && !resv6)
		_res.options &= ~RES_USE_INET6;
	return h;
}

static struct hostent *
getipnodebyaddr(addr, len, family, err)
	char *addr;
	int len;
	int family;
	int *err;
{
	struct hostent *h;

	h_errno = 0;
	h = gethostbyaddr(addr, len, family);
	*err = h_errno;
	return h;
}
#endif /* NEEDSGETIPNODE && NETINET6 && __RES < 19990909 */

struct hostent *
sm_gethostbyname(name, family)
	char *name;
	int family;
{
	struct hostent *h = NULL;
#if (SOLARIS > 10000 && SOLARIS < 20400) || (defined(SOLARIS) && SOLARIS < 204) || (defined(sony_news) && defined(__svr4))
# if SOLARIS == 20300 || SOLARIS == 203
	static struct hostent hp;
	static char buf[1000];
	extern struct hostent *_switch_gethostbyname_r();

	if (tTd(61, 10))
		dprintf("_switch_gethostbyname_r(%s)... ", name);
	h = _switch_gethostbyname_r(name, &hp, buf, sizeof(buf), &h_errno);
# else /* SOLARIS == 20300 || SOLARIS == 203 */
	extern struct hostent *__switch_gethostbyname();

	if (tTd(61, 10))
		dprintf("__switch_gethostbyname(%s)... ", name);
	h = __switch_gethostbyname(name);
# endif /* SOLARIS == 20300 || SOLARIS == 203 */
#else /* (SOLARIS > 10000 && SOLARIS < 20400) || (defined(SOLARIS) && SOLARIS < 204) || (defined(sony_news) && defined(__svr4)) */
	int nmaps;
# if NETINET6
	int flags = AI_DEFAULT|AI_ALL;
	int err;
# endif /* NETINET6 */
	int save_errno;
	char *maptype[MAXMAPSTACK];
	short mapreturn[MAXMAPACTIONS];
	char hbuf[MAXNAME];

	if (tTd(61, 10))
		dprintf("sm_gethostbyname(%s, %d)... ", name, family);

# if NETINET6
#  if ADDRCONFIG_IS_BROKEN
	flags &= ~AI_ADDRCONFIG;
#  endif /* ADDRCONFIG_IS_BROKEN */
	h = getipnodebyname(name, family, flags, &err);
	h_errno = err;
# else /* NETINET6 */
	h = gethostbyname(name);
# endif /* NETINET6 */

	save_errno = errno;
	if (h == NULL)
	{
		if (tTd(61, 10))
			dprintf("failure\n");

		nmaps = switch_map_find("hosts", maptype, mapreturn);
		while (--nmaps >= 0)
			if (strcmp(maptype[nmaps], "nis") == 0 ||
			    strcmp(maptype[nmaps], "files") == 0)
				break;
		if (nmaps >= 0)
		{
			/* try short name */
			if (strlen(name) > (SIZE_T) sizeof hbuf - 1)
			{
				errno = save_errno;
				return NULL;
			}
			(void) strlcpy(hbuf, name, sizeof hbuf);
			shorten_hostname(hbuf);

			/* if it hasn't been shortened, there's no point */
			if (strcmp(hbuf, name) != 0)
			{
				if (tTd(61, 10))
					dprintf("sm_gethostbyname(%s, %d)... ",
					       hbuf, family);

# if NETINET6
				h = getipnodebyname(hbuf, family,
						    AI_V4MAPPED|AI_ALL,
						    &err);
				h_errno = err;
				save_errno = errno;
# else /* NETINET6 */
				h = gethostbyname(hbuf);
				save_errno = errno;
# endif /* NETINET6 */
			}
		}
	}
#endif /* (SOLARIS > 10000 && SOLARIS < 20400) || (defined(SOLARIS) && SOLARIS < 204) || (defined(sony_news) && defined(__svr4)) */
	if (tTd(61, 10))
	{
		if (h == NULL)
			dprintf("failure\n");
		else
		{
			dprintf("%s\n", h->h_name);
			if (tTd(61, 11))
			{
#if NETINET6
				struct in6_addr ia6;
				char buf6[INET6_ADDRSTRLEN];
#else /* NETINET6 */
				struct in_addr ia;
#endif /* NETINET6 */
				int i;

				if (h->h_aliases != NULL)
					for (i = 0; h->h_aliases[i] != NULL;
					     i++)
						dprintf("\talias: %s\n",
							h->h_aliases[i]);
				for (i = 0; h->h_addr_list[i] != NULL; i++)
				{
					char *addr;

#if NETINET6
					memmove(&ia6, h->h_addr_list[i],
						IN6ADDRSZ);
					addr = anynet_ntop(&ia6,
							   buf6, sizeof buf6);
#else /* NETINET6 */
					memmove(&ia, h->h_addr_list[i],
						INADDRSZ);
					addr = (char *) inet_ntoa(ia);
#endif /* NETINET6 */
					if (addr != NULL)
						dprintf("\taddr: %s\n", addr);
				}
			}
		}
	}
	errno = save_errno;
	return h;
}

struct hostent *
sm_gethostbyaddr(addr, len, type)
	char *addr;
	int len;
	int type;
{
	struct hostent *hp;
#if (SOLARIS > 10000 && SOLARIS < 20400) || (defined(SOLARIS) && SOLARIS < 204)
# if SOLARIS == 20300 || SOLARIS == 203
	static struct hostent he;
	static char buf[1000];
	extern struct hostent *_switch_gethostbyaddr_r();

	hp = _switch_gethostbyaddr_r(addr, len, type, &he, buf, sizeof(buf), &h_errno);
# else /* SOLARIS == 20300 || SOLARIS == 203 */
	extern struct hostent *__switch_gethostbyaddr();

	hp = __switch_gethostbyaddr(addr, len, type);
# endif /* SOLARIS == 20300 || SOLARIS == 203 */
#else /* (SOLARIS > 10000 && SOLARIS < 20400) || (defined(SOLARIS) && SOLARIS < 204) */
# if NETINET6
	int err;
# endif /* NETINET6 */

# if NETINET6
	hp = getipnodebyaddr(addr, len, type, &err);
	h_errno = err;
# else /* NETINET6 */
	hp = gethostbyaddr(addr, len, type);
# endif /* NETINET6 */
	return hp;
#endif /* (SOLARIS > 10000 && SOLARIS < 20400) || (defined(SOLARIS) && SOLARIS < 204) */
}
/*
**  SM_GETPW{NAM,UID} -- wrapper for getpwnam and getpwuid
*/


struct passwd *
sm_getpwnam(user)
	char *user;
{
# ifdef _AIX4
	extern struct passwd *_getpwnam_shadow(const char *, const int);

	return _getpwnam_shadow(user, 0);
# else /* _AIX4 */
	return getpwnam(user);
# endif /* _AIX4 */
}

struct passwd *
sm_getpwuid(uid)
	UID_T uid;
{
# if defined(_AIX4) && 0
	extern struct passwd *_getpwuid_shadow(const int, const int);

	return _getpwuid_shadow(uid,0);
# else /* defined(_AIX4) && 0 */
	return getpwuid(uid);
# endif /* defined(_AIX4) && 0 */
}
/*
**  SECUREWARE_SETUP_SECURE -- Convex SecureWare setup
**
**	Set up the trusted computing environment for C2 level security
**	under SecureWare.
**
**	Parameters:
**		uid -- uid of the user to initialize in the TCB
**
**	Returns:
**		none
**
**	Side Effects:
**		Initialized the user in the trusted computing base
*/

#if SECUREWARE

# include <sys/security.h>
# include <prot.h>

void
secureware_setup_secure(uid)
	UID_T uid;
{
	int rc;

	if (getluid() != -1)
		return;

	if ((rc = set_secure_info(uid)) != SSI_GOOD_RETURN)
	{
		switch (rc)
		{
		  case SSI_NO_PRPW_ENTRY:
			syserr("No protected passwd entry, uid = %d", uid);
			break;

		  case SSI_LOCKED:
			syserr("Account has been disabled, uid = %d", uid);
			break;

		  case SSI_RETIRED:
			syserr("Account has been retired, uid = %d", uid);
			break;

		  case SSI_BAD_SET_LUID:
			syserr("Could not set LUID, uid = %d", uid);
			break;

		  case SSI_BAD_SET_PRIVS:
			syserr("Could not set kernel privs, uid = %d", uid);

		  default:
			syserr("Unknown return code (%d) from set_secure_info(%d)",
				rc, uid);
			break;
		}
		finis(FALSE, EX_NOPERM);
	}
}
#endif /* SECUREWARE */
/*
**  ADD_HOSTNAMES -- Add a hostname to class 'w' based on IP address
**
**	Add hostnames to class 'w' based on the IP address read from
**	the network interface.
**
**	Parameters:
**		sa -- a pointer to a SOCKADDR containing the address
**
**	Returns:
**		0 if successful, -1 if host lookup fails.
*/

static int
add_hostnames(sa)
	SOCKADDR *sa;
{
	struct hostent *hp;
	char **ha;
	char hnb[MAXHOSTNAMELEN];

	/* lookup name with IP address */
	switch (sa->sa.sa_family)
	{
#if NETINET
		case AF_INET:
			hp = sm_gethostbyaddr((char *) &sa->sin.sin_addr,
				sizeof(sa->sin.sin_addr), sa->sa.sa_family);
			break;
#endif /* NETINET */

#if NETINET6
		case AF_INET6:
			hp = sm_gethostbyaddr((char *) &sa->sin6.sin6_addr,
				sizeof(sa->sin6.sin6_addr), sa->sa.sa_family);
			break;
#endif /* NETINET6 */

		default:
			/* Give warning about unsupported family */
			if (LogLevel > 3)
				sm_syslog(LOG_WARNING, NOQID,
					  "Unsupported address family %d: %.100s",
					  sa->sa.sa_family, anynet_ntoa(sa));
			return -1;
	}

	if (hp == NULL)
	{
		int save_errno = errno;

		if (LogLevel > 3 &&
#if NETINET6
		    !(sa->sa.sa_family == AF_INET6 &&
		      IN6_IS_ADDR_LINKLOCAL(&sa->sin6.sin6_addr)) &&
#endif /* NETINET6 */
		    TRUE)
			sm_syslog(LOG_WARNING, NOQID,
				"gethostbyaddr(%.100s) failed: %d\n",
				anynet_ntoa(sa),
#if NAMED_BIND
				h_errno
#else /* NAMED_BIND */
				-1
#endif /* NAMED_BIND */
				);
		errno = save_errno;
		return -1;
	}

	/* save its cname */
	if (!wordinclass((char *) hp->h_name, 'w'))
	{
		setclass('w', (char *) hp->h_name);
		if (tTd(0, 4))
			dprintf("\ta.k.a.: %s\n", hp->h_name);

		if (snprintf(hnb, sizeof hnb, "[%s]", hp->h_name) < sizeof hnb
		    && !wordinclass((char *) hnb, 'w'))
			setclass('w', hnb);
	}
	else
	{
		if (tTd(0, 43))
			dprintf("\ta.k.a.: %s (already in $=w)\n", hp->h_name);
	}

	/* save all it aliases name */
	for (ha = hp->h_aliases; ha != NULL && *ha != NULL; ha++)
	{
		if (!wordinclass(*ha, 'w'))
		{
			setclass('w', *ha);
			if (tTd(0, 4))
				dprintf("\ta.k.a.: %s\n", *ha);
			if (snprintf(hnb, sizeof hnb,
				     "[%s]", *ha) < sizeof hnb &&
			    !wordinclass((char *) hnb, 'w'))
				setclass('w', hnb);
		}
		else
		{
			if (tTd(0, 43))
				dprintf("\ta.k.a.: %s (already in $=w)\n",
					*ha);
		}
	}
	return 0;
}
/*
**  LOAD_IF_NAMES -- load interface-specific names into $=w
**
**	Parameters:
**		none.
**
**	Returns:
**		none.
**
**	Side Effects:
**		Loads $=w with the names of all the interfaces.
*/

#if !NETINET
# define SIOCGIFCONF_IS_BROKEN	1 /* XXX */
#endif /* !NETINET */

#if defined(SIOCGIFCONF) && !SIOCGIFCONF_IS_BROKEN
struct rtentry;
struct mbuf;
# ifndef SUNOS403
#  include <sys/time.h>
# endif /* ! SUNOS403 */
# if (_AIX4 >= 40300) && !defined(_NET_IF_H)
#  undef __P
# endif /* (_AIX4 >= 40300) && !defined(_NET_IF_H) */
# include <net/if.h>
#endif /* defined(SIOCGIFCONF) && !SIOCGIFCONF_IS_BROKEN */

void
load_if_names()
{
#if NETINET6 && defined(SIOCGLIFCONF)
	int s;
	int i;
	struct lifconf lifc;
	struct lifnum lifn;
	int numifs;

	s = socket(InetMode, SOCK_DGRAM, 0);
	if (s == -1)
		return;

	/* get the list of known IP address from the kernel */
#   ifdef SIOCGLIFNUM
	lifn.lifn_family = AF_UNSPEC;
	lifn.lifn_flags = 0;
	if (ioctl(s, SIOCGLIFNUM, (char *)&lifn) < 0)
	{
		/* can't get number of interfaces -- fall back */
		if (tTd(0, 4))
			dprintf("SIOCGLIFNUM failed: %s\n", errstring(errno));
		numifs = -1;
	}
	else
	{
		numifs = lifn.lifn_count;
		if (tTd(0, 42))
			dprintf("system has %d interfaces\n", numifs);
	}
	if (numifs < 0)
#   endif /* SIOCGLIFNUM */
		numifs = MAXINTERFACES;

	if (numifs <= 0)
	{
		close(s);
		return;
	}
	lifc.lifc_len = numifs * sizeof (struct lifreq);
	lifc.lifc_buf = xalloc(lifc.lifc_len);
	lifc.lifc_family = AF_UNSPEC;
	lifc.lifc_flags = 0;
	if (ioctl(s, SIOCGLIFCONF, (char *)&lifc) < 0)
	{
		if (tTd(0, 4))
			dprintf("SIOCGLIFCONF failed: %s\n", errstring(errno));
		close(s);
		return;
	}

	/* scan the list of IP address */
	if (tTd(0, 40))
		dprintf("scanning for interface specific names, lifc_len=%d\n",
			lifc.lifc_len);

	for (i = 0; i < lifc.lifc_len; )
	{
		struct lifreq *ifr = (struct lifreq *)&lifc.lifc_buf[i];
		SOCKADDR *sa = (SOCKADDR *) &ifr->lifr_addr;
		char *addr;
		struct in6_addr ia6;
		struct in_addr ia;
#   ifdef SIOCGLIFFLAGS
		struct lifreq ifrf;
#   endif /* SIOCGLIFFLAGS */
		char ip_addr[256];
		char buf6[INET6_ADDRSTRLEN];
		int af = ifr->lifr_addr.ss_family;

		/*
		**  We must close and recreate the socket each time
		**  since we don't know what type of socket it is now
		**  (each status function may change it).
		*/

		(void) close(s);

		s = socket(af, SOCK_DGRAM, 0);
		if (s == -1)
			return;

		/*
		**  If we don't have a complete ifr structure,
		**  don't try to use it.
		*/

		if ((lifc.lifc_len - i) < sizeof *ifr)
			break;

#   ifdef BSD4_4_SOCKADDR
		if (sa->sa.sa_len > sizeof ifr->lifr_addr)
			i += sizeof ifr->lifr_name + sa->sa.sa_len;
		else
#   endif /* BSD4_4_SOCKADDR */
			i += sizeof *ifr;

		if (tTd(0, 20))
			dprintf("%s\n", anynet_ntoa(sa));

		if (af != AF_INET && af != AF_INET6)
			continue;

#   ifdef SIOCGLIFFLAGS
		memset(&ifrf, '\0', sizeof(struct lifreq));
		(void) strlcpy(ifrf.lifr_name, ifr->lifr_name,
			       sizeof(ifrf.lifr_name));
		if (ioctl(s, SIOCGLIFFLAGS, (char *) &ifrf) < 0)
		{
			if (tTd(0, 4))
				dprintf("SIOCGLIFFLAGS failed: %s\n",
					errstring(errno));
			continue;
		}
		else if (tTd(0, 41))
			dprintf("\tflags: %lx\n",
				(unsigned long)ifrf.lifr_flags);

		if (!bitset(IFF_UP, ifrf.lifr_flags))
			continue;
#   endif /* SIOCGLIFFLAGS */

		ip_addr[0] = '\0';

		/* extract IP address from the list*/
		switch (af)
		{
		  case AF_INET6:
			ia6 = sa->sin6.sin6_addr;
			if (ia6.s6_addr == in6addr_any.s6_addr)
			{
				addr = anynet_ntop(&ia6, buf6, sizeof buf6);
				message("WARNING: interface %s is UP with %s address",
					ifr->lifr_name,
					addr == NULL ? "(NULL)" : addr);
				continue;
			}

			/* save IP address in text from */
			addr = anynet_ntop(&ia6, buf6, sizeof buf6);
			if (addr != NULL)
				(void) snprintf(ip_addr, sizeof ip_addr,
						"[%.*s]",
						(int) sizeof ip_addr - 3, addr);
			break;

		  case AF_INET:
			ia = sa->sin.sin_addr;
			if (ia.s_addr == INADDR_ANY ||
			    ia.s_addr == INADDR_NONE)
			{
				message("WARNING: interface %s is UP with %s address",
					ifr->lifr_name, inet_ntoa(ia));
				continue;
			}

			/* save IP address in text from */
			(void) snprintf(ip_addr, sizeof ip_addr, "[%.*s]",
					(int) sizeof ip_addr - 3, inet_ntoa(ia));
			break;
		}

		if (*ip_addr == '\0')
			continue;

		if (!wordinclass(ip_addr, 'w'))
		{
			setclass('w', ip_addr);
			if (tTd(0, 4))
				dprintf("\ta.k.a.: %s\n", ip_addr);
		}

#   ifdef SIOCGLIFFLAGS
		/* skip "loopback" interface "lo" */
		if (bitset(IFF_LOOPBACK, ifrf.lifr_flags))
			continue;
#   endif /* SIOCGLIFFLAGS */
		(void) add_hostnames(sa);
	}
	free(lifc.lifc_buf);
	close(s);
#else /* NETINET6 && defined(SIOCGLIFCONF) */
# if defined(SIOCGIFCONF) && !SIOCGIFCONF_IS_BROKEN
	int s;
	int i;
	struct ifconf ifc;
	int numifs;

	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s == -1)
		return;

	/* get the list of known IP address from the kernel */
#  if defined(SIOCGIFNUM) && !SIOCGIFNUM_IS_BROKEN
	if (ioctl(s, SIOCGIFNUM, (char *) &numifs) < 0)
	{
		/* can't get number of interfaces -- fall back */
		if (tTd(0, 4))
			dprintf("SIOCGIFNUM failed: %s\n", errstring(errno));
		numifs = -1;
	}
	else if (tTd(0, 42))
		dprintf("system has %d interfaces\n", numifs);
	if (numifs < 0)
#  endif /* defined(SIOCGIFNUM) && !SIOCGIFNUM_IS_BROKEN */
		numifs = MAXINTERFACES;

	if (numifs <= 0)
	{
		(void) close(s);
		return;
	}
	ifc.ifc_len = numifs * sizeof (struct ifreq);
	ifc.ifc_buf = xalloc(ifc.ifc_len);
	if (ioctl(s, SIOCGIFCONF, (char *)&ifc) < 0)
	{
		if (tTd(0, 4))
			dprintf("SIOCGIFCONF failed: %s\n", errstring(errno));
		(void) close(s);
		free(ifc.ifc_buf);
		return;
	}

	/* scan the list of IP address */
	if (tTd(0, 40))
		dprintf("scanning for interface specific names, ifc_len=%d\n",
			ifc.ifc_len);

	for (i = 0; i < ifc.ifc_len; )
	{
		int af;
		struct ifreq *ifr = (struct ifreq *) &ifc.ifc_buf[i];
		SOCKADDR *sa = (SOCKADDR *) &ifr->ifr_addr;
#   if NETINET6
		char *addr;
		struct in6_addr ia6;
#   endif /* NETINET6 */
		struct in_addr ia;
#   ifdef SIOCGIFFLAGS
		struct ifreq ifrf;
#   endif /* SIOCGIFFLAGS */
		char ip_addr[256];
#   if NETINET6
		char buf6[INET6_ADDRSTRLEN];
#   endif /* NETINET6 */

		/*
		**  If we don't have a complete ifr structure,
		**  don't try to use it.
		*/

		if ((ifc.ifc_len - i) < sizeof *ifr)
			break;

#   ifdef BSD4_4_SOCKADDR
		if (sa->sa.sa_len > sizeof ifr->ifr_addr)
			i += sizeof ifr->ifr_name + sa->sa.sa_len;
		else
#   endif /* BSD4_4_SOCKADDR */
			i += sizeof *ifr;

		if (tTd(0, 20))
			dprintf("%s\n", anynet_ntoa(sa));

		af = ifr->ifr_addr.sa_family;
		if (af != AF_INET
#   if NETINET6
		    && af != AF_INET6
#   endif /* NETINET6 */
		    )
			continue;

#   ifdef SIOCGIFFLAGS
		memset(&ifrf, '\0', sizeof(struct ifreq));
		(void) strlcpy(ifrf.ifr_name, ifr->ifr_name,
			       sizeof(ifrf.ifr_name));
		(void) ioctl(s, SIOCGIFFLAGS, (char *) &ifrf);
		if (tTd(0, 41))
			dprintf("\tflags: %lx\n",
				(unsigned long) ifrf.ifr_flags);
#    define IFRFREF ifrf
#   else /* SIOCGIFFLAGS */
#    define IFRFREF (*ifr)
#   endif /* SIOCGIFFLAGS */

		if (!bitset(IFF_UP, IFRFREF.ifr_flags))
			continue;

		ip_addr[0] = '\0';

		/* extract IP address from the list*/
		switch (af)
		{
		  case AF_INET:
			ia = sa->sin.sin_addr;
			if (ia.s_addr == INADDR_ANY ||
			    ia.s_addr == INADDR_NONE)
			{
				message("WARNING: interface %s is UP with %s address",
					ifr->ifr_name, inet_ntoa(ia));
				continue;
			}

			/* save IP address in text from */
			(void) snprintf(ip_addr, sizeof ip_addr, "[%.*s]",
					(int) sizeof ip_addr - 3,
					inet_ntoa(ia));
			break;

#   if NETINET6
		  case AF_INET6:
			ia6 = sa->sin6.sin6_addr;
			if (ia6.s6_addr == in6addr_any.s6_addr)
			{
				addr = anynet_ntop(&ia6, buf6, sizeof buf6);
				message("WARNING: interface %s is UP with %s address",
					ifr->ifr_name,
					addr == NULL ? "(NULL)" : addr);
				continue;
			}

			/* save IP address in text from */
			addr = anynet_ntop(&ia6, buf6, sizeof buf6);
			if (addr != NULL)
				(void) snprintf(ip_addr, sizeof ip_addr,
						"[%.*s]",
						(int) sizeof ip_addr - 3, addr);
			break;

#   endif /* NETINET6 */
		}

		if (ip_addr[0] == '\0')
			continue;

		if (!wordinclass(ip_addr, 'w'))
		{
			setclass('w', ip_addr);
			if (tTd(0, 4))
				dprintf("\ta.k.a.: %s\n", ip_addr);
		}

		/* skip "loopback" interface "lo" */
		if (bitset(IFF_LOOPBACK, IFRFREF.ifr_flags))
			continue;

		(void) add_hostnames(sa);
	}
	free(ifc.ifc_buf);
	(void) close(s);
#  undef IFRFREF
# endif /* defined(SIOCGIFCONF) && !SIOCGIFCONF_IS_BROKEN */
#endif /* NETINET6 && defined(SIOCGLIFCONF) */
}
/*
**  ISLOOPBACK -- is socket address in the loopback net?
**
**	Parameters:
**		sa -- socket address.
**
**	Returns:
**		TRUE -- is socket address in the loopback net?
**		FALSE -- otherwise
**
*/

bool
isloopback(sa)
	SOCKADDR sa;
{
#if NETINET6
	if (IN6_IS_ADDR_LOOPBACK(&sa.sin6.sin6_addr))
		return TRUE;
#else /* NETINET6 */
	/* XXX how to correctly extract IN_LOOPBACKNET part? */
	if (((ntohl(sa.sin.sin_addr.s_addr) & IN_CLASSA_NET)
	     >> IN_CLASSA_NSHIFT) == IN_LOOPBACKNET)
		return TRUE;
#endif /* NETINET6 */
	return FALSE;
}
/*
**  GET_NUM_PROCS_ONLINE -- return the number of processors currently online
**
**	Parameters:
**		none.
**
**	Returns:
**		The number of processors online.
*/

static int
get_num_procs_online()
{
	int nproc = 0;

#ifdef USESYSCTL
# if defined(CTL_HW) && defined(HW_NCPU)
	size_t sz;
	int mib[2];

	mib[0] = CTL_HW;
	mib[1] = HW_NCPU;
	sz = (size_t) sizeof nproc;
	(void) sysctl(mib, 2, &nproc, &sz, NULL, 0);
# endif /* defined(CTL_HW) && defined(HW_NCPUS) */
#else /* USESYSCTL */
# ifdef _SC_NPROCESSORS_ONLN
	nproc = (int) sysconf(_SC_NPROCESSORS_ONLN);
# else /* _SC_NPROCESSORS_ONLN */
#  ifdef __hpux
#   include <sys/pstat.h>
	struct pst_dynamic psd;

	if (pstat_getdynamic(&psd, sizeof(psd), (size_t)1, 0) != -1)
		nproc = psd.psd_proc_cnt;
#  endif /* __hpux */
# endif /* _SC_NPROCESSORS_ONLN */
#endif /* USESYSCTL */

	if (nproc <= 0)
		nproc = 1;
	return nproc;
}
/*
**  SEED_RANDOM -- seed the random number generator
**
**	Parameters:
**		none
**
**	Returns:
**		none
*/

void
seed_random()
{
#if HASSRANDOMDEV
	srandomdev();
#else /* HASSRANDOMDEV */
	long seed;
	struct timeval t;

	seed = (long) getpid();
	if (gettimeofday(&t, NULL) >= 0)
		seed += t.tv_sec + t.tv_usec;

# if HASRANDOM
	(void) srandom(seed);
# else /* HASRANDOM */
	(void) srand((unsigned int) seed);
# endif /* HASRANDOM */
#endif /* HASSRANDOMDEV */
}
/*
**  SM_SYSLOG -- syslog wrapper to keep messages under SYSLOG_BUFSIZE
**
**	Parameters:
**		level -- syslog level
**		id -- envelope ID or NULL (NOQUEUE)
**		fmt -- format string
**		arg... -- arguments as implied by fmt.
**
**	Returns:
**		none
*/

/* VARARGS3 */
void
#ifdef __STDC__
sm_syslog(int level, const char *id, const char *fmt, ...)
#else /* __STDC__ */
sm_syslog(level, id, fmt, va_alist)
	int level;
	const char *id;
	const char *fmt;
	va_dcl
#endif /* __STDC__ */
{
	static char *buf = NULL;
	static size_t bufsize;
	char *begin, *end;
	int save_errno;
	int seq = 1;
	int idlen;
	char buf0[MAXLINE];
	extern int SnprfOverflow;
	extern int SyslogErrno;
	extern char *DoprEnd;
	VA_LOCAL_DECL

	save_errno = SyslogErrno = errno;
	if (id == NULL)
		id = "NOQUEUE";
	else if (strcmp(id, NOQID) == 0)
		id = "";
	idlen = strlen(id);

	if (buf == NULL)
	{
		buf = buf0;
		bufsize = sizeof buf0;
	}

	for (;;)
	{
		/* do a virtual vsnprintf into buf */
		VA_START(fmt);
		buf[0] = 0;
		DoprEnd = buf + bufsize - 1;
		SnprfOverflow = 0;
		sm_dopr(buf, fmt, ap);
		*DoprEnd = '\0';
		VA_END;
		/* end of virtual vsnprintf */

		if (SnprfOverflow == 0)
			break;

		/* String too small, redo with correct size */
		bufsize += SnprfOverflow + 1;
		if (buf != buf0)
			free(buf);
		buf = xalloc(bufsize * sizeof (char));
	}
	if ((strlen(buf) + idlen + 1) < SYSLOG_BUFSIZE)
	{
#if LOG
		if (*id == '\0')
			syslog(level, "%s", buf);
		else
			syslog(level, "%s: %s", id, buf);
#else /* LOG */
		/*XXX should do something more sensible */
		if (*id == '\0')
			fprintf(stderr, "%s\n", buf);
		else
			fprintf(stderr, "%s: %s\n", id, buf);
#endif /* LOG */
		if (buf == buf0)
			buf = NULL;
		errno = save_errno;
		return;
	}

	begin = buf;
	while (*begin != '\0' &&
	       (strlen(begin) + idlen + 5) > SYSLOG_BUFSIZE)
	{
		char save;

		if (seq == 999)
		{
			/* Too many messages */
			break;
		}
		end = begin + SYSLOG_BUFSIZE - idlen - 12;
		while (end > begin)
		{
			/* Break on comma or space */
			if (*end == ',' || *end == ' ')
			{
				end++;	  /* Include separator */
				break;
			}
			end--;
		}
		/* No separator, break midstring... */
		if (end == begin)
			end = begin + SYSLOG_BUFSIZE - idlen - 12;
		save = *end;
		*end = 0;
#if LOG
		syslog(level, "%s[%d]: %s ...", id, seq++, begin);
#else /* LOG */
		fprintf(stderr, "%s[%d]: %s ...\n", id, seq++, begin);
#endif /* LOG */
		*end = save;
		begin = end;
	}
	if (seq == 999)
#if LOG
		syslog(level, "%s[%d]: log terminated, too many parts",
			id, seq);
#else /* LOG */
		fprintf(stderr, "%s[%d]: log terminated, too many parts\n",
			id, seq);
#endif /* LOG */
	else if (*begin != '\0')
#if LOG
		syslog(level, "%s[%d]: %s", id, seq, begin);
#else /* LOG */
		fprintf(stderr, "%s[%d]: %s\n", id, seq, begin);
#endif /* LOG */
	if (buf == buf0)
		buf = NULL;
	errno = save_errno;
}
/*
**  HARD_SYSLOG -- call syslog repeatedly until it works
**
**	Needed on HP-UX, which apparently doesn't guarantee that
**	syslog succeeds during interrupt handlers.
*/

#if defined(__hpux) && !defined(HPUX11)

# define MAXSYSLOGTRIES	100
# undef syslog
# ifdef V4FS
#  define XCNST	const
#  define CAST	(const char *)
# else /* V4FS */
#  define XCNST
#  define CAST
# endif /* V4FS */

void
# ifdef __STDC__
hard_syslog(int pri, XCNST char *msg, ...)
# else /* __STDC__ */
hard_syslog(pri, msg, va_alist)
	int pri;
	XCNST char *msg;
	va_dcl
# endif /* __STDC__ */
{
	int i;
	char buf[SYSLOG_BUFSIZE];
	VA_LOCAL_DECL;

	VA_START(msg);
	vsnprintf(buf, sizeof buf, msg, ap);
	VA_END;

	for (i = MAXSYSLOGTRIES; --i >= 0 && syslog(pri, CAST "%s", buf) < 0; )
		continue;
}

# undef CAST
#endif /* defined(__hpux) && !defined(HPUX11) */
#if NEEDLOCAL_HOSTNAME_LENGTH
/*
**  LOCAL_HOSTNAME_LENGTH
**
**	This is required to get sendmail to compile against BIND 4.9.x
**	on Ultrix.
**
**	Unfortunately, a Compaq Y2K patch kit provides it without
**	bumping __RES in /usr/include/resolv.h so we can't automatically
**	figure out whether it is needed.
*/

int
local_hostname_length(hostname)
	char *hostname;
{
	int len_host, len_domain;

	if (!*_res.defdname)
		res_init();
	len_host = strlen(hostname);
	len_domain = strlen(_res.defdname);
	if (len_host > len_domain &&
	    (strcasecmp(hostname + len_host - len_domain,
			_res.defdname) == 0) &&
	    hostname[len_host - len_domain - 1] == '.')
		return len_host - len_domain - 1;
	else
		return 0;
}
#endif /* NEEDLOCAL_HOSTNAME_LENGTH */

/*
**  Compile-Time options
*/

char	*CompileOptions[] =
{
#ifdef HESIOD
	"HESIOD",
#endif /* HESIOD */
#if HES_GETMAILHOST
	"HES_GETMAILHOST",
#endif /* HES_GETMAILHOST */
#ifdef LDAPMAP
	"LDAPMAP",
#endif /* LDAPMAP */
#ifdef MAP_NSD
	"MAP_NSD",
#endif /* MAP_NSD */
#ifdef MAP_REGEX
	"MAP_REGEX",
#endif /* MAP_REGEX */
#if LOG
	"LOG",
#endif /* LOG */
#if MATCHGECOS
	"MATCHGECOS",
#endif /* MATCHGECOS */
#if MIME7TO8
	"MIME7TO8",
#endif /* MIME7TO8 */
#if MIME8TO7
	"MIME8TO7",
#endif /* MIME8TO7 */
#if NAMED_BIND
	"NAMED_BIND",
#endif /* NAMED_BIND */
#ifdef NDBM
	"NDBM",
#endif /* NDBM */
#if NETINET
	"NETINET",
#endif /* NETINET */
#if NETINET6
	"NETINET6",
#endif /* NETINET6 */
#if NETINFO
	"NETINFO",
#endif /* NETINFO */
#if NETISO
	"NETISO",
#endif /* NETISO */
#if NETNS
	"NETNS",
#endif /* NETNS */
#if NETUNIX
	"NETUNIX",
#endif /* NETUNIX */
#if NETX25
	"NETX25",
#endif /* NETX25 */
#ifdef NEWDB
	"NEWDB",
#endif /* NEWDB */
#ifdef NIS
	"NIS",
#endif /* NIS */
#ifdef NISPLUS
	"NISPLUS",
#endif /* NISPLUS */
#ifdef PH_MAP
	"PH_MAP",
#endif /* PH_MAP */
#if QUEUE
	"QUEUE",
#endif /* QUEUE */
#if SASL
	"SASL",
#endif /* SASL */
#if SCANF
	"SCANF",
#endif /* SCANF */
#if SFIO
	"SFIO",
#endif /* SFIO */
#if SMTP
	"SMTP",
#endif /* SMTP */
#if SMTPDEBUG
	"SMTPDEBUG",
#endif /* SMTPDEBUG */
#if STARTTLS
	"STARTTLS",
#endif /* STARTTLS */
#ifdef SUID_ROOT_FILES_OK
	"SUID_ROOT_FILES_OK",
#endif /* SUID_ROOT_FILES_OK */
#if TCPWRAPPERS
	"TCPWRAPPERS",
#endif /* TCPWRAPPERS */
#if USERDB
	"USERDB",
#endif /* USERDB */
#if XDEBUG
	"XDEBUG",
#endif /* XDEBUG */
#ifdef XLA
	"XLA",
#endif /* XLA */
	NULL
};


/*
**  OS compile options.
*/

char	*OsCompileOptions[] =
{
#if BOGUS_O_EXCL
	"BOGUS_O_EXCL",
#endif /* BOGUS_O_EXCL */
#if FAST_PID_RECYCLE
	"FAST_PID_RECYCLE",
#endif /* FAST_PID_RECYCLE */
#if HASFCHOWN
	"HASFCHOWN",
#endif /* HASFCHOWN */
#if HASFCHMOD
	"HASFCHMOD",
#endif /* HASFCHMOD */
#if HASFLOCK
	"HASFLOCK",
#endif /* HASFLOCK */
#if HASGETDTABLESIZE
	"HASGETDTABLESIZE",
#endif /* HASGETDTABLESIZE */
#if HASGETUSERSHELL
	"HASGETUSERSHELL",
#endif /* HASGETUSERSHELL */
#if HASINITGROUPS
	"HASINITGROUPS",
#endif /* HASINITGROUPS */
#if HASLSTAT
	"HASLSTAT",
#endif /* HASLSTAT */
#if HASRANDOM
	"HASRANDOM",
#endif /* HASRANDOM */
#if HASSETLOGIN
	"HASSETLOGIN",
#endif /* HASSETLOGIN */
#if HASSETREUID
	"HASSETREUID",
#endif /* HASSETREUID */
#if HASSETRLIMIT
	"HASSETRLIMIT",
#endif /* HASSETRLIMIT */
#if HASSETSID
	"HASSETSID",
#endif /* HASSETSID */
#if HASSETUSERCONTEXT
	"HASSETUSERCONTEXT",
#endif /* HASSETUSERCONTEXT */
#if HASSETVBUF
	"HASSETVBUF",
#endif /* HASSETVBUF */
#if HASSNPRINTF
	"HASSNPRINTF",
#endif /* HASSNPRINTF */
#if HAS_ST_GEN
	"HAS_ST_GEN",
#endif /* HAS_ST_GEN */
#if HASSRANDOMDEV
	"HASSRANDOMDEV",
#endif /* HASSRANDOMDEV */
#if HASURANDOMDEV
	"HASURANDOMDEV",
#endif /* HASURANDOMDEV */
#if HASSTRERROR
	"HASSTRERROR",
#endif /* HASSTRERROR */
#if HASULIMIT
	"HASULIMIT",
#endif /* HASULIMIT */
#if HASUNAME
	"HASUNAME",
#endif /* HASUNAME */
#if HASUNSETENV
	"HASUNSETENV",
#endif /* HASUNSETENV */
#if HASWAITPID
	"HASWAITPID",
#endif /* HASWAITPID */
#if IDENTPROTO
	"IDENTPROTO",
#endif /* IDENTPROTO */
#if IP_SRCROUTE
	"IP_SRCROUTE",
#endif /* IP_SRCROUTE */
#if O_EXLOCK && HASFLOCK && !BOGUS_O_EXCL
	"LOCK_ON_OPEN",
#endif /* O_EXLOCK && HASFLOCK && !BOGUS_O_EXCL */
#if NEEDFSYNC
	"NEEDFSYNC",
#endif /* NEEDFSYNC */
#if NOFTRUNCATE
	"NOFTRUNCATE",
#endif /* NOFTRUNCATE */
#if RLIMIT_NEEDS_SYS_TIME_H
	"RLIMIT_NEEDS_SYS_TIME_H",
#endif /* RLIMIT_NEEDS_SYS_TIME_H */
#if SAFENFSPATHCONF
	"SAFENFSPATHCONF",
#endif /* SAFENFSPATHCONF */
#if SECUREWARE
	"SECUREWARE",
#endif /* SECUREWARE */
#if SHARE_V1
	"SHARE_V1",
#endif /* SHARE_V1 */
#if SIOCGIFCONF_IS_BROKEN
	"SIOCGIFCONF_IS_BROKEN",
#endif /* SIOCGIFCONF_IS_BROKEN */
#if SIOCGIFNUM_IS_BROKEN
	"SIOCGIFNUM_IS_BROKEN",
#endif /* SIOCGIFNUM_IS_BROKEN */
#if SNPRINTF_IS_BROKEN
	"SNPRINTF_IS_BROKEN",
#endif /* SNPRINTF_IS_BROKEN */
#if SO_REUSEADDR_IS_BROKEN
	"SO_REUSEADDR_IS_BROKEN",
#endif /* SO_REUSEADDR_IS_BROKEN */
#if SYS5SETPGRP
	"SYS5SETPGRP",
#endif /* SYS5SETPGRP */
#if SYSTEM5
	"SYSTEM5",
#endif /* SYSTEM5 */
#if USE_SA_SIGACTION
	"USE_SA_SIGACTION",
#endif /* USE_SA_SIGACTION */
#if USE_SIGLONGJMP
	"USE_SIGLONGJMP",
#endif /* USE_SIGLONGJMP */
#if USESETEUID
	"USESETEUID",
#endif /* USESETEUID */
	NULL
};

