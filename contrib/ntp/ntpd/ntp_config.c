/*
 * ntp_config.c - read and apply configuration information
 */
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef HAVE_NETINFO
# include <netinfo/ni.h>
#endif

#include "ntpd.h"
#include "ntp_io.h"
#include "ntp_unixtime.h"
#include "ntp_refclock.h"
#include "ntp_filegen.h"
#include "ntp_stdlib.h"
#include "ntp_config.h"
#include "ntp_cmdargs.h"

#ifdef PUBKEY
# include "ntp_crypto.h"
#endif /* PUBKEY */

#include <stdio.h>
#include <ctype.h>
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#include <signal.h>
#ifndef SIGCHLD
# define SIGCHLD SIGCLD
#endif
#if !defined(VMS)
# ifdef HAVE_SYS_WAIT_H
#  include <sys/wait.h>
# endif
#endif /* VMS */

#ifdef SYS_WINNT
# include <io.h>
extern HANDLE ResolverThreadHandle;
#endif /* SYS_WINNT */

extern int priority_done;

/*
 * These routines are used to read the configuration file at
 * startup time.  An entry in the file must fit on a single line.
 * Entries are processed as multiple tokens separated by white space
 * Lines are considered terminated when a '#' is encountered.  Blank
 * lines are ignored.
 */

/*
 * We understand the following configuration entries and defaults.
 *
 * peer [ addr ] [ version 3 ] [ key 0 ] [ minpoll 6 ] [ maxpoll 10 ]
 * server [ addr ] [ version 3 ] [ key 0 ] [ minpoll 6 ] [ maxpoll 10 ]
 * broadcast [ addr ] [ version 3 ] [ key 0 ] [ ttl 1 ]
 * broadcastclient
 * multicastclient [ 224.0.1.1 ]
 * manycastclient [ addr ] [ version 3 ] [ key 0 ] [ minpoll 6 ] [ maxpoll 10 ]
 * manycastserver [ 224.0.1.1 ]
 * broadcastdelay 0.0102
 * restrict [ addr ] [ mask 255.255.255.0 ] ignore|noserve|notrust|noquery
 * driftfile file_name
 * keys file_name
 * publickey file_name
 * privatekey file_name
 * statsdir /var/NTP/
 * filegen peerstats [ file peerstats ] [ type day ] [ link ]
 * clientlimit [ n ]
 * clientperiod [ 3600 ]
 * trustedkey [ key ]
 * requestkey [ key]
 * controlkey [ key ]
 * trap [ addr ]
 * fudge [ addr ] [ stratum ] [ refid ] ...
 * pidfile [ ]
 * setvar [ ]
 * logfile logfile
 * logconfig [+|-|=][{sync|sys|peer|clock}{{,all}{info|statistics|events|status}}]...
 * enable auth|bclient|pll|kernel|monitor|stats|calibrate
 * disable auth|bclient|pll|kernel|monitor|stats|calibrate
 * phone ...
 * pps device [assert|clear] [hardpps]
 * priority high|normal
 */

/*
 * Translation table - keywords to function index
 */
struct keyword {
	const char *text;
	int keytype;
};

/*
 * Command keywords
 */
static	struct keyword keywords[] = {
	{ "authenticate",	CONFIG_AUTHENTICATE },
	{ "automax",		CONFIG_AUTOMAX },
	{ "broadcast",		CONFIG_BROADCAST },
	{ "broadcastclient",	CONFIG_BROADCASTCLIENT },
	{ "broadcastdelay",	CONFIG_BDELAY },
	{ "clientlimit",	CONFIG_CLIENTLIMIT },
	{ "clientperiod",	CONFIG_CLIENTPERIOD },
#ifdef PUBKEY
	{ "crypto",		CONFIG_CRYPTO },
#endif /* PUBKEY */
	{ "controlkey",		CONFIG_CONTROLKEY },
	{ "disable",		CONFIG_DISABLE },
	{ "driftfile",		CONFIG_DRIFTFILE },
	{ "enable",		CONFIG_ENABLE },
	{ "filegen",		CONFIG_FILEGEN },
	{ "fudge",		CONFIG_FUDGE },
	{ "includefile",	CONFIG_INCLUDEFILE },
	{ "keys",		CONFIG_KEYS },
#ifdef PUBKEY
	{ "keysdir",		CONFIG_KEYSDIR },
#endif /* PUBKEY */
	{ "logconfig",		CONFIG_LOGCONFIG },
	{ "logfile",		CONFIG_LOGFILE },
	{ "manycastclient",	CONFIG_MANYCASTCLIENT },
	{ "manycastserver",	CONFIG_MANYCASTSERVER },
	{ "multicastclient",	CONFIG_MULTICASTCLIENT },
	{ "peer",		CONFIG_PEER },
	{ "phone",		CONFIG_PHONE },
	{ "pidfile",		CONFIG_PIDFILE },
	{ "pps",		CONFIG_PPS },
	{ "requestkey",		CONFIG_REQUESTKEY },
	{ "restrict",		CONFIG_RESTRICT },
	{ "revoke",		CONFIG_REVOKE },
	{ "server",		CONFIG_SERVER },
	{ "setvar",		CONFIG_SETVAR },
	{ "statistics",		CONFIG_STATISTICS },
	{ "statsdir",		CONFIG_STATSDIR },
	{ "tinker",		CONFIG_TINKER },
	{ "trap",		CONFIG_TRAP },
	{ "trustedkey",		CONFIG_TRUSTEDKEY },
	{ "",			CONFIG_UNKNOWN }
};

/*
 * "peer", "server", "broadcast" modifier keywords
 */
static	struct keyword mod_keywords[] = {
	{ "autokey",		CONF_MOD_SKEY },
	{ "burst",		CONF_MOD_BURST },
	{ "iburst",		CONF_MOD_IBURST },
	{ "key",		CONF_MOD_KEY },
	{ "maxpoll",		CONF_MOD_MAXPOLL },
	{ "minpoll",		CONF_MOD_MINPOLL },
	{ "mode",		CONF_MOD_MODE },    /* refclocks */
	{ "noselect",		CONF_MOD_NOSELECT },
	{ "prefer",		CONF_MOD_PREFER },
#ifdef PUBKEY
	{ "publickey",		CONF_MOD_PUBLICKEY },
#endif /* PUBKEY */
	{ "ttl",		CONF_MOD_TTL },     /* NTP peers */
	{ "version",		CONF_MOD_VERSION },
	{ "",			CONFIG_UNKNOWN }
};

/*
 * "restrict" modifier keywords
 */
static	struct keyword res_keywords[] = {
	{ "ignore",		CONF_RES_IGNORE },
	{ "limited",		CONF_RES_LIMITED },
	{ "kod",		CONF_RES_DEMOBILIZE },
	{ "lowpriotrap",	CONF_RES_LPTRAP },
	{ "mask",		CONF_RES_MASK },
	{ "nomodify",		CONF_RES_NOMODIFY },
	{ "nopeer",		CONF_RES_NOPEER },
	{ "noquery",		CONF_RES_NOQUERY },
	{ "noserve",		CONF_RES_NOSERVE },
	{ "notrap",		CONF_RES_NOTRAP },
	{ "notrust",		CONF_RES_NOTRUST },
	{ "ntpport",		CONF_RES_NTPPORT },
	{ "version",		CONF_RES_VERSION },
	{ "",			CONFIG_UNKNOWN }
};

/*
 * "trap" modifier keywords
 */
static	struct keyword trap_keywords[] = {
	{ "port",		CONF_TRAP_PORT },
	{ "interface",		CONF_TRAP_INTERFACE },
	{ "",			CONFIG_UNKNOWN }
};

/*
 * "fudge" modifier keywords
 */
static	struct keyword fudge_keywords[] = {
	{ "flag1",		CONF_FDG_FLAG1 },
	{ "flag2",		CONF_FDG_FLAG2 },
	{ "flag3",		CONF_FDG_FLAG3 },
	{ "flag4",		CONF_FDG_FLAG4 },
	{ "refid",		CONF_FDG_REFID },
	{ "stratum",		CONF_FDG_STRATUM },
	{ "time1",		CONF_FDG_TIME1 },
	{ "time2",		CONF_FDG_TIME2 },
	{ "",			CONFIG_UNKNOWN }
};


/*
 * "filegen" modifier keywords
 */
static	struct keyword filegen_keywords[] = {
	{ "disable",		CONF_FGEN_FLAG_DISABLE },
	{ "enable",		CONF_FGEN_FLAG_ENABLE },
	{ "file",		CONF_FGEN_FILE },
	{ "link",		CONF_FGEN_FLAG_LINK },
	{ "nolink",		CONF_FGEN_FLAG_NOLINK },
	{ "type",		CONF_FGEN_TYPE },
	{ "",			CONFIG_UNKNOWN }
};

/*
 * "type" modifier keywords
 */
static	struct keyword fgen_types[] = {
	{ "age",		FILEGEN_AGE   },
	{ "day",		FILEGEN_DAY   },
	{ "month",		FILEGEN_MONTH },
	{ "none",		FILEGEN_NONE  },
	{ "pid",		FILEGEN_PID   },
	{ "week",		FILEGEN_WEEK  },
	{ "year",		FILEGEN_YEAR  },
	{ "",			CONFIG_UNKNOWN}
};

/*
 * "enable", "disable" modifier keywords
 */
static struct keyword flags_keywords[] = {
	{ "auth",		PROTO_AUTHENTICATE },
	{ "bclient",		PROTO_BROADCLIENT },
	{ "calibrate",		PROTO_CAL },
	{ "kernel",		PROTO_KERNEL },
	{ "monitor",		PROTO_MONITOR },
	{ "ntp",		PROTO_NTP },
	{ "pps",		PROTO_PPS },
	{ "stats",		PROTO_FILEGEN },
	{ "",			CONFIG_UNKNOWN }
};

/*
 * "pps" modifier keywords
 */
static struct keyword pps_keywords[] = {
	{ "assert",		CONF_PPS_ASSERT },
	{ "clear",		CONF_PPS_CLEAR },
	{ "hardpps",		CONF_PPS_HARDPPS },
	{ "",			CONFIG_UNKNOWN }
};

/*
 * "tinker" modifier keywords
 */
static struct keyword tinker_keywords[] = {
	{ "step",		CONF_CLOCK_MAX },
	{ "panic",		CONF_CLOCK_PANIC },
	{ "dispersion",		CONF_CLOCK_PHI },
	{ "stepout",		CONF_CLOCK_MINSTEP },
	{ "minpoll",		CONF_CLOCK_MINPOLL },
	{ "allan",		CONF_CLOCK_ALLAN },
	{ "huffpuff",		CONF_CLOCK_HUFFPUFF },
	{ "",			CONFIG_UNKNOWN }
};

#ifdef PUBKEY
/*
 * "crypto" modifier keywords
 */
static struct keyword crypto_keywords[] = {
	{ "dh",			CONF_CRYPTO_DH },
	{ "flags",		CONF_CRYPTO_FLAGS },
	{ "leap",		CONF_CRYPTO_LEAP },
	{ "privatekey",		CONF_CRYPTO_PRIVATEKEY },
	{ "publickey",		CONF_CRYPTO_PUBLICKEY },
	{ "",			CONFIG_UNKNOWN }
};
#endif /* PUBKEY */

/*
 * "logconfig" building blocks
 */
struct masks {
	const char	  *name;
	unsigned long mask;
};

static struct masks logcfg_class[] = {
	{ "clock",		NLOG_OCLOCK },
	{ "peer",		NLOG_OPEER },
	{ "sync",		NLOG_OSYNC },
	{ "sys",		NLOG_OSYS },
	{ (char *)0,	0 }
};

static struct masks logcfg_item[] = {
	{ "info",		NLOG_INFO },
	{ "allinfo",		NLOG_SYSINFO|NLOG_PEERINFO|NLOG_CLOCKINFO|NLOG_SYNCINFO },
	{ "events",		NLOG_EVENT },
	{ "allevents",		NLOG_SYSEVENT|NLOG_PEEREVENT|NLOG_CLOCKEVENT|NLOG_SYNCEVENT },
	{ "status",		NLOG_STATUS },
	{ "allstatus",		NLOG_SYSSTATUS|NLOG_PEERSTATUS|NLOG_CLOCKSTATUS|NLOG_SYNCSTATUS },
	{ "statistics",		NLOG_STATIST },
	{ "allstatistics",	NLOG_SYSSTATIST|NLOG_PEERSTATIST|NLOG_CLOCKSTATIST|NLOG_SYNCSTATIST },
	{ "allclock",		(NLOG_INFO|NLOG_STATIST|NLOG_EVENT|NLOG_STATUS)<<NLOG_OCLOCK },
	{ "allpeer",		(NLOG_INFO|NLOG_STATIST|NLOG_EVENT|NLOG_STATUS)<<NLOG_OPEER },
	{ "allsys",		(NLOG_INFO|NLOG_STATIST|NLOG_EVENT|NLOG_STATUS)<<NLOG_OSYS },
	{ "allsync",		(NLOG_INFO|NLOG_STATIST|NLOG_EVENT|NLOG_STATUS)<<NLOG_OSYNC },
	{ "all",		NLOG_SYSMASK|NLOG_PEERMASK|NLOG_CLOCKMASK|NLOG_SYNCMASK },
	{ (char *)0,	0 }
};

/*
 * Limits on things
 */
#define MAXTOKENS	20	/* 20 tokens on line */
#define MAXLINE		1024	/* maximum length of line */
#define MAXPHONE	5	/* maximum number of phone strings */
#define MAXPPS		20	/* maximum length of PPS device string */
#define MAXINCLUDELEVEL	5	/* maximum include file levels */

/*
 * Miscellaneous macros
 */
#define STRSAME(s1, s2)	(*(s1) == *(s2) && strcmp((s1), (s2)) == 0)
#define ISEOL(c)	((c) == '#' || (c) == '\n' || (c) == '\0')
#define ISSPACE(c)	((c) == ' ' || (c) == '\t')
#define STREQ(a, b)	(*(a) == *(b) && strcmp((a), (b)) == 0)

#define KEY_TYPE_MD5	4

/*
 * File descriptor used by the resolver save routines, and temporary file
 * name.
 */
static FILE *res_fp;
#ifndef SYS_WINNT
static char res_file[20];	/* enough for /tmp/ntpXXXXXX\0 */
#define RES_TEMPFILE	"/tmp/ntpXXXXXX"
#else
static char res_file[MAX_PATH];
#endif /* SYS_WINNT */

/*
 * Definitions of things either imported from or exported to outside
 */
char const *progname;
char	sys_phone[MAXPHONE][MAXDIAL]; /* ACTS phone numbers */
char	pps_device[MAXPPS + 1]; /* PPS device name */
int	pps_assert;
int	pps_hardpps;
#if defined(HAVE_SCHED_SETSCHEDULER)
int	config_priority_override = 0;
int	config_priority;
#endif

const char *config_file;
#ifdef HAVE_NETINFO
 struct netinfo_config_state *config_netinfo = NULL;
 int check_netinfo = 1;
#endif /* HAVE_NETINFO */
#ifdef SYS_WINNT
 char *alt_config_file;
 LPTSTR temp;
 char config_file_storage[MAX_PATH];
 char alt_config_file_storage[MAX_PATH];
#endif /* SYS_WINNT */

#ifdef HAVE_NETINFO
/*
 * NetInfo configuration state
 */
struct netinfo_config_state {
	void *domain;		/* domain with config */
	ni_id config_dir;	/* ID config dir      */
	int prop_index;		/* current property   */
	int val_index;		/* current value      */
	char **val_list;       	/* value list         */
};
#endif

/*
 * Function prototypes
 */
static	unsigned long get_pfxmatch P((char **, struct masks *));
static	unsigned long get_match P((char *, struct masks *));
static	unsigned long get_logmask P((char *));
#ifdef HAVE_NETINFO
static	struct netinfo_config_state *get_netinfo_config P((void));
static	void free_netinfo_config P((struct netinfo_config_state *));
static	int gettokens_netinfo P((struct netinfo_config_state *, char **, int *));
#endif
static	int gettokens P((FILE *, char *, char **, int *));
static	int matchkey P((char *, struct keyword *));
static	int getnetnum P((const char *, struct sockaddr_in *, int));
static	void save_resolve P((char *, int, int, int, int, u_int, int,
    keyid_t, u_char *));
static	void do_resolve_internal P((void));
static	void abort_resolve P((void));
#if !defined(VMS)
static	RETSIGTYPE catchchild P((int));
#endif /* VMS */

/*
 * get_pfxmatch - find value for prefixmatch
 * and update char * accordingly
 */
static unsigned long
get_pfxmatch(
	char ** s,
	struct masks *m
	)
{
	while (m->name) {
		if (strncmp(*s, m->name, strlen(m->name)) == 0) {
			*s += strlen(m->name);
			return m->mask;
		} else {
			m++;
		}
	}
	return 0;
}

/*
 * get_match - find logmask value
 */
static unsigned long
get_match(
	char *s,
	struct masks *m
	)
{
	while (m->name) {
		if (strcmp(s, m->name) == 0) {
			return m->mask;
		} else {
			m++;
		}
	}
	return 0;
}

/*
 * get_logmask - build bitmask for ntp_syslogmask
 */
static unsigned long
get_logmask(
	char *s
	)
{
	char *t;
	unsigned long offset;
	unsigned long mask;

	t = s;
	offset = get_pfxmatch(&t, logcfg_class);
	mask   = get_match(t, logcfg_item);

	if (mask)
		return mask << offset;
	else
		msyslog(LOG_ERR, "logconfig: illegal argument %s - ignored", s);

	return 0;
}


/*
 * getconfig - get command line options and read the configuration file
 */
void
getconfig(
	int argc,
	char *argv[]
	)
{
	register int i;
	int c;
	int errflg;
	int peerversion;
	int minpoll;
	int maxpoll;
	int ttl;
	long stratum;
	unsigned long ul;
	keyid_t peerkey;
	u_char *peerkeystr;
	u_long fudgeflag;
	u_int peerflags;
	int hmode;
	struct sockaddr_in peeraddr;
	struct sockaddr_in maskaddr;
	FILE *fp[MAXINCLUDELEVEL+1];
	FILE *includefile;
	int includelevel = 0;
	char line[MAXLINE];
	char *(tokens[MAXTOKENS]);
	int ntokens;
	int tok = CONFIG_UNKNOWN;
	struct interface *localaddr;
	struct refclockstat clock_stat;
	FILEGEN *filegen;

	/*
	 * Initialize, initialize
	 */
	errflg = 0;
	/* HMS: don't initialize debug to 0 here! */
#ifndef SYS_WINNT
	config_file = CONFIG_FILE;
#else
	temp = CONFIG_FILE;
	if (!ExpandEnvironmentStrings((LPCTSTR)temp, (LPTSTR)config_file_storage, (DWORD)sizeof(config_file_storage))) {
		msyslog(LOG_ERR, "ExpandEnvironmentStrings CONFIG_FILE failed: %m\n");
		exit(1);
	}
	config_file = config_file_storage;

	temp = ALT_CONFIG_FILE;
	if (!ExpandEnvironmentStrings((LPCTSTR)temp, (LPTSTR)alt_config_file_storage, (DWORD)sizeof(alt_config_file_storage))) {
		msyslog(LOG_ERR, "ExpandEnvironmentStrings ALT_CONFIG_FILE failed: %m\n");
		exit(1);
	}
	alt_config_file = alt_config_file_storage;

#endif /* SYS_WINNT */
	progname = argv[0];
	res_fp = NULL;
	memset((char *)sys_phone, 0, sizeof(sys_phone));
	ntp_syslogmask = NLOG_SYNCMASK; /* set more via logconfig */

	/*
	 * install a non default variable with this daemon version
	 */
	(void) sprintf(line, "daemon_version=\"%s\"", Version);
	set_sys_var(line, strlen(line)+1, RO);

	/*
	 * Say how we're setting the time of day
	 */
	(void) sprintf(line, "settimeofday=\"%s\"", set_tod_using);
	set_sys_var(line, strlen(line)+1, RO);

	/*
	 * Initialize the loop.
	 */
	loop_config(LOOP_DRIFTINIT, 0.);

	getCmdOpts(argc, argv);

	if (
	    (fp[0] = fopen(FindConfig(config_file), "r")) == NULL
#ifdef HAVE_NETINFO
	    /* If there is no config_file, try NetInfo. */
	    && check_netinfo && !(config_netinfo = get_netinfo_config())
#endif /* HAVE_NETINFO */
	    ) {
		fprintf(stderr, "getconfig: Couldn't open <%s>\n", FindConfig(config_file));
		msyslog(LOG_INFO, "getconfig: Couldn't open <%s>", FindConfig(config_file));
#ifdef SYS_WINNT
		/* Under WinNT try alternate_config_file name, first NTP.CONF, then NTP.INI */

		if ((fp[0] = fopen(FindConfig(alt_config_file), "r")) == NULL) {

			/*
			 * Broadcast clients can sometimes run without
			 * a configuration file.
			 */

			fprintf(stderr, "getconfig: Couldn't open <%s>\n", FindConfig(alt_config_file));
			msyslog(LOG_INFO, "getconfig: Couldn't open <%s>", FindConfig(alt_config_file));
			return;
		}
#else  /* not SYS_WINNT */
		return;
#endif /* not SYS_WINNT */
	}

	for (;;) {
		if (fp[includelevel])
			tok = gettokens(fp[includelevel], line, tokens, &ntokens);
#ifdef HAVE_NETINFO
		else
			tok = gettokens_netinfo(config_netinfo, tokens, &ntokens);
#endif /* HAVE_NETINFO */

		if (tok == CONFIG_UNKNOWN) {
		    if (includelevel > 0) {
			fclose(fp[includelevel--]);
			continue;
		    } else {
			break;
		    }
		}

		switch(tok) {
		    case CONFIG_PEER:
		    case CONFIG_SERVER:
		    case CONFIG_MANYCASTCLIENT:
		    case CONFIG_BROADCAST:
			if (tok == CONFIG_PEER)
			    hmode = MODE_ACTIVE;
			else if (tok == CONFIG_SERVER)
			    hmode = MODE_CLIENT;
			else if (tok == CONFIG_MANYCASTCLIENT)
			    hmode = MODE_CLIENT;
			else
			    hmode = MODE_BROADCAST;

			if (ntokens < 2) {
				msyslog(LOG_ERR,
					"No address for %s, line ignored",
					tokens[0]);
				break;
			}

			if (!getnetnum(tokens[1], &peeraddr, 0)) {
				errflg = -1;
			} else {
				errflg = 0;

				if (
#ifdef REFCLOCK
					!ISREFCLOCKADR(&peeraddr) &&
#endif
					ISBADADR(&peeraddr)) {
					msyslog(LOG_ERR,
						"attempt to configure invalid address %s",
						ntoa(&peeraddr));
					break;
				}
				/*
				 * Shouldn't be able to specify multicast
				 * address for server/peer!
				 * and unicast address for manycastclient!
				 */
				if (((tok == CONFIG_SERVER) ||
				     (tok == CONFIG_PEER)) &&
#ifdef REFCLOCK
				    !ISREFCLOCKADR(&peeraddr) &&
#endif
				    IN_CLASSD(ntohl(peeraddr.sin_addr.s_addr))) {
					msyslog(LOG_ERR,
						"attempt to configure invalid address %s",
						ntoa(&peeraddr));
					break;
				}
				if ((tok == CONFIG_MANYCASTCLIENT) &&
				    !IN_CLASSD(ntohl(peeraddr.sin_addr.s_addr))) {
					msyslog(LOG_ERR,
						"attempt to configure invalid address %s",
						ntoa(&peeraddr));
					break;
				}
			}
			
			peerversion = NTP_VERSION;
			minpoll = NTP_MINDPOLL;
			maxpoll = NTP_MAXDPOLL;
			peerkey = 0;
			peerkeystr = "*";
			peerflags = 0;
			ttl = 0;
			for (i = 2; i < ntokens; i++)
			    switch (matchkey(tokens[i], mod_keywords)) {
				case CONF_MOD_VERSION:
				    if (i >= ntokens-1) {
					    msyslog(LOG_ERR,
						    "peer/server version requires an argument");
					    errflg = 1;
					    break;
				    }
				    peerversion = atoi(tokens[++i]);
				    if ((u_char)peerversion > NTP_VERSION
					|| (u_char)peerversion < NTP_OLDVERSION) {
					    msyslog(LOG_ERR,
						    "inappropriate version number %s, line ignored",
						    tokens[i]);
					    errflg = 1;
				    }
				    break;
					
				case CONF_MOD_KEY:
				    if (i >= ntokens-1) {
					    msyslog(LOG_ERR,
						    "key: argument required");
					    errflg = 1;
					    break;
				    }
				    peerkey = (int)atol(tokens[++i]);
				    peerflags |= FLAG_AUTHENABLE;
				    break;

				case CONF_MOD_MINPOLL:
				    if (i >= ntokens-1) {
					    msyslog(LOG_ERR,
						    "minpoll: argument required");
					    errflg = 1;
					    break;
				    }
				    minpoll = atoi(tokens[++i]);
				    if (minpoll < NTP_MINPOLL) {
					    msyslog(LOG_INFO,
						    "minpoll: provided value (%d) is below minimum (%d)",
						    minpoll, NTP_MINPOLL);
					minpoll = NTP_MINPOLL;
				    }
				    break;

				case CONF_MOD_MAXPOLL:
				    if (i >= ntokens-1) {
					    msyslog(LOG_ERR,
						    "maxpoll: argument required"
						    );
					    errflg = 1;
					    break;
				    }
				    maxpoll = atoi(tokens[++i]);
				    if (maxpoll > NTP_MAXPOLL) {
					    msyslog(LOG_INFO,
						    "maxpoll: provided value (%d) is above maximum (%d)",
						    maxpoll, NTP_MAXPOLL);
					maxpoll = NTP_MAXPOLL;
				    }
				    break;

				case CONF_MOD_PREFER:
				    peerflags |= FLAG_PREFER;
				    break;

				case CONF_MOD_NOSELECT:
				    peerflags |= FLAG_NOSELECT;
				    break;

				case CONF_MOD_BURST:
				    peerflags |= FLAG_BURST;
				    break;

				case CONF_MOD_IBURST:
				    peerflags |= FLAG_IBURST;
				    break;
#ifdef AUTOKEY
				case CONF_MOD_SKEY:
				    peerflags |= FLAG_SKEY |
					FLAG_AUTHENABLE;
				    break;

#ifdef PUBKEY
				case CONF_MOD_PUBLICKEY:
				    if (i >= ntokens - 1) {
					msyslog(LOG_ERR,
					    "Public key file name required");
					errflg = 1;
					break;
				    }
				    peerflags |= FLAG_SKEY |
					FLAG_AUTHENABLE;
 				    peerkeystr = tokens[++i];
				    break;
#endif /* PUBKEY */
#endif /* AUTOKEY */

				case CONF_MOD_TTL:
				    if (i >= ntokens-1) {
					    msyslog(LOG_ERR,
						    "ttl: argument required");
					    errflg = 1;
					    break;
				    }
				    ttl = atoi(tokens[++i]);
				    break;

				case CONF_MOD_MODE:
				    if (i >= ntokens-1) {
					    msyslog(LOG_ERR,
						    "mode: argument required");
					    errflg = 1;
					    break;
				    }
				    ttl = atoi(tokens[++i]);
				    break;

				case CONFIG_UNKNOWN:
				    errflg = 1;
				    break;
			    }
			if (minpoll > maxpoll) {
				msyslog(LOG_ERR, "config error: minpoll > maxpoll");
				errflg = 1;
			}
			if (errflg == 0) {
			    if (peer_config(&peeraddr, any_interface, hmode,
				    peerversion, minpoll, maxpoll, peerflags,
				    ttl, peerkey, peerkeystr) == 0) {
					msyslog(LOG_ERR,
						"configuration of %s failed",
						ntoa(&peeraddr));
			    }
	
			} else if (errflg == -1) {
				save_resolve(tokens[1], hmode, peerversion,
				    minpoll, maxpoll, peerflags, ttl,
				    peerkey, peerkeystr);
			}
			break;

		    case CONFIG_DRIFTFILE:
			if (ntokens >= 2)
			    stats_config(STATS_FREQ_FILE, tokens[1]);
			else
			    stats_config(STATS_FREQ_FILE, (char *)0);
			break;
	
		    case CONFIG_PIDFILE:
			if (ntokens >= 2)
			    stats_config(STATS_PID_FILE, tokens[1]);
			else
			    stats_config(STATS_PID_FILE, (char *)0);
			break;

		    case CONFIG_INCLUDEFILE:
			if (ntokens < 2) {
			    msyslog(LOG_ERR, "includefile needs one argument");
			    break;
			}
			if (includelevel >= MAXINCLUDELEVEL) {
			    fprintf(stderr, "getconfig: Maximum include file level exceeded.\n");
			    msyslog(LOG_INFO, "getconfig: Maximum include file level exceeded.");
			    break;
			}
			includefile = fopen(FindConfig(tokens[1]), "r");
			if (includefile == NULL) {
			    fprintf(stderr, "getconfig: Couldn't open <%s>\n", FindConfig(tokens[1]));
			    msyslog(LOG_INFO, "getconfig: Couldn't open <%s>", FindConfig(tokens[1]));
			    break;
			}
			fp[++includelevel] = includefile;
			break;

		    case CONFIG_LOGFILE:
			if (ntokens >= 2) {
				FILE *new_file;

				new_file = fopen(tokens[1], "a");
				if (new_file != NULL) {
					NLOG(NLOG_SYSINFO) /* conditional if clause for conditional syslog */
					    msyslog(LOG_NOTICE, "logging to file %s", tokens[1]);
					if (syslog_file != NULL &&
					    fileno(syslog_file) != fileno(new_file))
					    (void)fclose(syslog_file);

					syslog_file = new_file;
					syslogit = 0;
				}
				else
				    msyslog(LOG_ERR,
					    "Cannot open log file %s",
					    tokens[1]);
			}
			else
			    msyslog(LOG_ERR, "logfile needs one argument");
			break;

		    case CONFIG_LOGCONFIG:
			for (i = 1; i < ntokens; i++)
			{
				int add = 1;
				int equals = 0;
				char * s = &tokens[i][0];

				switch (*s) {
				    case '+':
				    case '-':
				    case '=':
					add = *s == '+';
					equals = *s == '=';
					s++;
					break;

				    default:
					break;
				}
				if (equals) {
					ntp_syslogmask = get_logmask(s);
				} else {				
					if (add) {
						ntp_syslogmask |= get_logmask(s);
					} else {
						ntp_syslogmask &= ~get_logmask(s);
					}
				}
#ifdef DEBUG
				if (debug)
				    printf("ntp_syslogmask = 0x%08lx (%s)\n", ntp_syslogmask, tokens[i]);
#endif
			}
			break;

		    case CONFIG_BROADCASTCLIENT:
			proto_config(PROTO_BROADCLIENT, 1, 0.);
			break;
			
		    case CONFIG_MULTICASTCLIENT:
		    case CONFIG_MANYCASTSERVER:
			if (ntokens > 1) {
				for (i = 1; i < ntokens; i++) {
					if (getnetnum(tokens[i], &peeraddr, 1))
					    proto_config(PROTO_MULTICAST_ADD,
							 peeraddr.sin_addr.s_addr, 0.);
				}
			} else
			    proto_config(PROTO_MULTICAST_ADD,
					 htonl(INADDR_NTP), 0.);
			if (tok == CONFIG_MULTICASTCLIENT)
				sys_bclient = 1;
			else if (tok == CONFIG_MANYCASTSERVER)
				sys_manycastserver = 1;
			break;

		    case CONFIG_AUTHENTICATE:
			errflg = 0;
			if (ntokens >= 2) {
				if (STREQ(tokens[1], "yes"))
				    proto_config(PROTO_AUTHENTICATE, 1, 0.);
				else if (STREQ(tokens[1], "no"))
				    proto_config(PROTO_AUTHENTICATE, 0, 0.);
				else
				    errflg++;
			} else {
				errflg++;
			}

			if (errflg)
			    msyslog(LOG_ERR,
				    "should be `authenticate yes|no'");
			break;

		    case CONFIG_KEYS:
			if (ntokens >= 2) {
				getauthkeys(tokens[1]);
			}
			break;

		    case CONFIG_TINKER:
			for (i = 1; i < ntokens; i++) {
			    int temp;
			    double ftemp;

			    temp = matchkey(tokens[i++],
				 tinker_keywords);
			    if (i > ntokens - 1) {
				msyslog(LOG_ERR,
				    "tinker: missing argument");
				errflg++;
				break;
			    }
			    sscanf(tokens[i], "%lf", &ftemp);
			    switch(temp) {
			    case CONF_CLOCK_MAX:
                                loop_config(LOOP_MAX, ftemp);
				break;

			    case CONF_CLOCK_PANIC:
				loop_config(LOOP_PANIC, ftemp);
				break;

			    case CONF_CLOCK_PHI:
				loop_config(LOOP_PHI, ftemp);
				break;

			    case CONF_CLOCK_MINSTEP:
				loop_config(LOOP_MINSTEP, ftemp);
				break;

			    case CONF_CLOCK_MINPOLL:
				loop_config(LOOP_MINPOLL, ftemp);
				break;

			    case CONF_CLOCK_ALLAN:
				loop_config(LOOP_ALLAN, ftemp);
				break;

			    case CONF_CLOCK_HUFFPUFF:
				loop_config(LOOP_HUFFPUFF, ftemp);
				break;
			    }
			}
			break;

#ifdef AUTOKEY
		    case CONFIG_REVOKE:
			if (ntokens >= 2)
			    sys_revoke = 1 << max(atoi(tokens[1]), 10);
			break;

		    case CONFIG_AUTOMAX:
			if (ntokens >= 2)
			    sys_automax = 1 << max(atoi(tokens[1]), 10);
			break;

#ifdef PUBKEY
		    case CONFIG_KEYSDIR:
			if (ntokens < 2) {
			    msyslog(LOG_ERR,
				"Keys directory name required");
			    break;
			}
			crypto_config(CRYPTO_CONF_KEYS, tokens[1]);
			break;
	
		    case CONFIG_CRYPTO:
			if (ntokens == 1) {
				crypto_config(CRYPTO_CONF_FLAGS	, "0");
				break;
			}
			for (i = 1; i < ntokens; i++) {
			    int temp;

			    temp = matchkey(tokens[i++], crypto_keywords);
			    if (i > ntokens - 1) {
				msyslog(LOG_ERR,
				    "crypto: missing argument");
				errflg++;
				break;
			    }
			    switch(temp) {
			    case CONF_CRYPTO_FLAGS:
				crypto_config(CRYPTO_CONF_FLAGS, tokens[i]);
				break;

			    case CONF_CRYPTO_LEAP:
				crypto_config(CRYPTO_CONF_LEAP, tokens[i]);
				break;

			    case CONF_CRYPTO_DH:
				crypto_config(CRYPTO_CONF_DH, tokens[i]);
				break;

			    case CONF_CRYPTO_PRIVATEKEY:
				crypto_config(CRYPTO_CONF_PRIV, tokens[i]);
				break;

			    case CONF_CRYPTO_PUBLICKEY:
				crypto_config(CRYPTO_CONF_PUBL, tokens[i]);
				break;

			    case CONF_CRYPTO_CERT:
				crypto_config(CRYPTO_CONF_CERT, tokens[i]);
				break;

			    default:
				msyslog(LOG_ERR, "crypto: unknown keyword");
				break;
			    }
			}
			break;
#endif /* PUBKEY */
#endif /* AUTOKEY */

		    case CONFIG_RESTRICT:
			if (ntokens < 2) {
				msyslog(LOG_ERR, "restrict requires an address");
				break;
			}
			if (STREQ(tokens[1], "default"))
			    peeraddr.sin_addr.s_addr = htonl(INADDR_ANY);
			else if (!getnetnum(tokens[1], &peeraddr, 1))
			    break;

			/*
			 * Use peerversion as flags, peerkey as mflags.  Ick.
			 */
			peerversion = 0;
			peerkey = 0;
			errflg = 0;
			maskaddr.sin_addr.s_addr = ~(u_int32)0;
			for (i = 2; i < ntokens; i++) {
				switch (matchkey(tokens[i], res_keywords)) {
				    case CONF_RES_MASK:
					if (i >= ntokens-1) {
						msyslog(LOG_ERR,
							"mask keyword needs argument");
						errflg++;
						break;
					}
					i++;
					if (!getnetnum(tokens[i], &maskaddr, 1))
					    errflg++;
					break;

				    case CONF_RES_IGNORE:
					peerversion |= RES_IGNORE;
					break;

				    case CONF_RES_NOSERVE:
					peerversion |= RES_DONTSERVE;
					break;

				    case CONF_RES_NOTRUST:
					peerversion |= RES_DONTTRUST;
					break;

				    case CONF_RES_NOQUERY:
					peerversion |= RES_NOQUERY;
					break;

				    case CONF_RES_NOMODIFY:
					peerversion |= RES_NOMODIFY;
					break;

				    case CONF_RES_NOPEER:
					peerversion |= RES_NOPEER;
					break;

				    case CONF_RES_NOTRAP:
					peerversion |= RES_NOTRAP;
					break;

				    case CONF_RES_LPTRAP:
					peerversion |= RES_LPTRAP;
					break;

				    case CONF_RES_NTPPORT:
					peerkey |= RESM_NTPONLY;
					break;

				    case CONF_RES_VERSION:
					peerversion |= RES_VERSION;
					break;

				    case CONF_RES_DEMOBILIZE:
					peerversion |= RES_DEMOBILIZE;
					break;

				    case CONF_RES_LIMITED:
					peerversion |= RES_LIMITED;
					break;

				    case CONFIG_UNKNOWN:
					errflg++;
					break;
				}
			}
			if (SRCADR(&peeraddr) == htonl(INADDR_ANY))
			    maskaddr.sin_addr.s_addr = 0;
			if (!errflg)
			    hack_restrict(RESTRICT_FLAGS, &peeraddr, &maskaddr,
					  (int)peerkey, peerversion);
			break;

		    case CONFIG_BDELAY:
			if (ntokens >= 2) {
				double tmp;

				if (sscanf(tokens[1], "%lf", &tmp) != 1) {
					msyslog(LOG_ERR,
						"broadcastdelay value %s undecodable",
						tokens[1]);
				} else {
					proto_config(PROTO_BROADDELAY, 0, tmp);
				}
			}
			break;

		    case CONFIG_TRUSTEDKEY:
			for (i = 1; i < ntokens; i++) {
				keyid_t tkey;

				tkey = atol(tokens[i]);
				if (tkey == 0) {
					msyslog(LOG_ERR,
						"trusted key %s unlikely",
						tokens[i]);
				} else {
					authtrust(tkey, 1);
				}
			}
			break;

		    case CONFIG_REQUESTKEY:
			if (ntokens >= 2) {
				if (!atouint(tokens[1], &ul)) {
					msyslog(LOG_ERR,
						"%s is undecodable as request key",
						tokens[1]);
				} else if (ul == 0) {
					msyslog(LOG_ERR,
						"%s makes a poor request keyid",
						tokens[1]);
				} else {
#ifdef DEBUG
					if (debug > 3)
					    printf(
						    "set info_auth_key to %08lx\n", ul);
#endif
					info_auth_keyid = (keyid_t)ul;
				}
			}
			break;

		    case CONFIG_CONTROLKEY:
			if (ntokens >= 2) {
				keyid_t ckey;

				ckey = atol(tokens[1]);
				if (ckey == 0) {
					msyslog(LOG_ERR,
						"%s makes a poor control keyid",
						tokens[1]);
				} else {
					ctl_auth_keyid = ckey;
				}
			}
			break;

		    case CONFIG_TRAP:
			if (ntokens < 2) {
				msyslog(LOG_ERR,
					"no address for trap command, line ignored");
				break;
			}
			if (!getnetnum(tokens[1], &peeraddr, 1))
			    break;

			/*
			 * Use peerversion for port number.  Barf.
			 */
			errflg = 0;
			peerversion = 0;
			localaddr = 0;
			for (i = 2; i < ntokens-1; i++)
			    switch (matchkey(tokens[i], trap_keywords)) {
				case CONF_TRAP_PORT:
				    if (i >= ntokens-1) {
					    msyslog(LOG_ERR,
						    "trap port requires an argument");
					    errflg = 1;
					    break;
				    }
				    peerversion = atoi(tokens[++i]);
				    if (peerversion <= 0
					|| peerversion > 32767) {
					    msyslog(LOG_ERR,
						    "invalid port number %s, trap ignored",
						    tokens[i]);
					    errflg = 1;
				    }
				    break;

				case CONF_TRAP_INTERFACE:
				    if (i >= ntokens-1) {
					    msyslog(LOG_ERR,
						    "trap interface requires an argument");
					    errflg = 1;
					    break;
				    }

				    if (!getnetnum(tokens[++i],
						   &maskaddr, 1)) {
					    errflg = 1;
					    break;
				    }

				    localaddr = findinterface(&maskaddr);
				    if (localaddr == NULL) {
					    msyslog(LOG_ERR,
						    "can't find interface with address %s",
						    ntoa(&maskaddr));
					    errflg = 1;
				    }
				    break;

				case CONFIG_UNKNOWN:
				    errflg++;
				    break;
			    }

			if (!errflg) {
				if (peerversion != 0)
				    peeraddr.sin_port = htons( (u_short) peerversion);
				else
				    peeraddr.sin_port = htons(TRAPPORT);
				if (localaddr == NULL)
				    localaddr = any_interface;
				if (!ctlsettrap(&peeraddr, localaddr, 0,
						NTP_VERSION))
				    msyslog(LOG_ERR,
					    "can't set trap for %s, no resources",
					    ntoa(&peeraddr));
			}
			break;

		    case CONFIG_FUDGE:
			if (ntokens < 2) {
				msyslog(LOG_ERR,
					"no address for fudge command, line ignored");
				break;
			}
			if (!getnetnum(tokens[1], &peeraddr, 1))
			    break;

			if (!ISREFCLOCKADR(&peeraddr)) {
				msyslog(LOG_ERR,
					"%s is inappropriate address for the fudge command, line ignored",
					ntoa(&peeraddr));
				break;
			}

			memset((void *)&clock_stat, 0, sizeof clock_stat);
			fudgeflag = 0;
			errflg = 0;
			for (i = 2; i < ntokens-1; i++) {
				switch (c = matchkey(tokens[i],
						     fudge_keywords)) {
				    case CONF_FDG_TIME1:
					if (sscanf(tokens[++i], "%lf",
						   &clock_stat.fudgetime1) != 1) {
						msyslog(LOG_ERR,
							"fudge %s time1 value in error",
							ntoa(&peeraddr));
						errflg = i;
						break;
					}
					clock_stat.haveflags |= CLK_HAVETIME1;
					break;

				    case CONF_FDG_TIME2:
					if (sscanf(tokens[++i], "%lf",
						   &clock_stat.fudgetime2) != 1) {
						msyslog(LOG_ERR,
							"fudge %s time2 value in error",
							ntoa(&peeraddr));
						errflg = i;
						break;
					}
					clock_stat.haveflags |= CLK_HAVETIME2;
					break;


				    case CONF_FDG_STRATUM:
				      if (!atoint(tokens[++i], &stratum))
					{
						msyslog(LOG_ERR,
							"fudge %s stratum value in error",
							ntoa(&peeraddr));
						errflg = i;
						break;
					}
					clock_stat.fudgeval1 = stratum;
					clock_stat.haveflags |= CLK_HAVEVAL1;
					break;

				    case CONF_FDG_REFID:
					/* HMS: Endianness and 0 bytes? */
					/* XXX */
					strncpy((char *)&clock_stat.fudgeval2,
						tokens[++i], 4);
					clock_stat.haveflags |= CLK_HAVEVAL2;
					break;

				    case CONF_FDG_FLAG1:
				    case CONF_FDG_FLAG2:
				    case CONF_FDG_FLAG3:
				    case CONF_FDG_FLAG4:
					if (!atouint(tokens[++i], &fudgeflag)
					    || fudgeflag > 1) {
						msyslog(LOG_ERR,
							"fudge %s flag value in error",
							ntoa(&peeraddr));
						errflg = i;
						break;
					}
					switch(c) {
					    case CONF_FDG_FLAG1:
						c = CLK_FLAG1;
						clock_stat.haveflags|=CLK_HAVEFLAG1;
						break;
					    case CONF_FDG_FLAG2:
						c = CLK_FLAG2;
						clock_stat.haveflags|=CLK_HAVEFLAG2;
						break;
					    case CONF_FDG_FLAG3:
						c = CLK_FLAG3;
						clock_stat.haveflags|=CLK_HAVEFLAG3;
						break;
					    case CONF_FDG_FLAG4:
						c = CLK_FLAG4;
						clock_stat.haveflags|=CLK_HAVEFLAG4;
						break;
					}
					if (fudgeflag == 0)
					    clock_stat.flags &= ~c;
					else
					    clock_stat.flags |= c;
					break;

				    case CONFIG_UNKNOWN:
					errflg = -1;
					break;
				}
			}

#ifdef REFCLOCK
			/*
			 * If reference clock support isn't defined the
			 * fudge line will still be accepted and syntax
			 * checked, but will essentially do nothing.
			 */
			if (!errflg) {
				refclock_control(&peeraddr, &clock_stat,
				    (struct refclockstat *)0);
			}
#endif
			break;

		    case CONFIG_STATSDIR:
			if (ntokens >= 2)
				stats_config(STATS_STATSDIR,tokens[1]);
			break;

		    case CONFIG_STATISTICS:
			for (i = 1; i < ntokens; i++) {
				filegen = filegen_get(tokens[i]);

				if (filegen == NULL) {
					msyslog(LOG_ERR,
						"no statistics named %s available",
						tokens[i]);
					continue;
				}
#ifdef DEBUG
				if (debug > 3)
				    printf("enabling filegen for %s statistics \"%s%s\"\n",
					   tokens[i], filegen->prefix, filegen->basename);
#endif
				filegen->flag |= FGEN_FLAG_ENABLED;
			}
			break;

		    case CONFIG_FILEGEN:
			if (ntokens < 2) {
				msyslog(LOG_ERR,
					"no id for filegen command, line ignored");
				break;
			}

			filegen = filegen_get(tokens[1]);
			if (filegen == NULL) {
				msyslog(LOG_ERR,
					"unknown filegen \"%s\" ignored",
					tokens[1]);
				break;
			}
			/*
			 * peerversion is (ab)used for filegen file (index)
			 * peerkey	   is (ab)used for filegen type
			 * peerflags   is (ab)used for filegen flags
			 */
			peerversion = 0;
			peerkey =	  filegen->type;
			peerflags =   filegen->flag;
			errflg = 0;

			for (i = 2; i < ntokens; i++) {
				switch (matchkey(tokens[i], filegen_keywords)) {
				    case CONF_FGEN_FILE:
					if (i >= ntokens - 1) {
						msyslog(LOG_ERR,
							"filegen %s file requires argument",
							tokens[1]);
						errflg = i;
						break;
					}
					peerversion = ++i;
					break;
				    case CONF_FGEN_TYPE:
					if (i >= ntokens -1) {
						msyslog(LOG_ERR,
							"filegen %s type requires argument",
							tokens[1]);
						errflg = i;
						break;
					}
					peerkey = matchkey(tokens[++i], fgen_types);
					if (peerkey == CONFIG_UNKNOWN) {
						msyslog(LOG_ERR,
							"filegen %s unknown type \"%s\"",
							tokens[1], tokens[i]);
						errflg = i;
						break;
					}
					break;

				    case CONF_FGEN_FLAG_LINK:
					peerflags |= FGEN_FLAG_LINK;
					break;

				    case CONF_FGEN_FLAG_NOLINK:
					peerflags &= ~FGEN_FLAG_LINK;
					break;

				    case CONF_FGEN_FLAG_ENABLE:
					peerflags |= FGEN_FLAG_ENABLED;
					break;

				    case CONF_FGEN_FLAG_DISABLE:
					peerflags &= ~FGEN_FLAG_ENABLED;
					break;
				}
			}
			if (!errflg)
				filegen_config(filegen, tokens[peerversion],
			           (u_char)peerkey, (u_char)peerflags);
			break;

		    case CONFIG_SETVAR:
			if (ntokens < 2) {
				msyslog(LOG_ERR,
					"no value for setvar command - line ignored");
			} else {
				set_sys_var(tokens[1], strlen(tokens[1])+1,
					    RW |
					    ((((ntokens > 2)
					       && !strcmp(tokens[2],
							  "default")))
					     ? DEF
					     : 0));
			}
			break;

		    case CONFIG_CLIENTLIMIT:
			if (ntokens < 2) {
				msyslog(LOG_ERR,
					"no value for clientlimit command - line ignored");
			} else {
				u_long ui;

				if (!atouint(tokens[1], &ui) || !ui) {
					msyslog(LOG_ERR,
						"illegal value for clientlimit command - line ignored");
				} else {
					char bp[80];

#ifdef DEBUG
					if (debug)
						sprintf(bp, "client_limit=%lu", ui);
#endif
					set_sys_var(bp, strlen(bp)+1, RO);
					client_limit = ui;
				}
			}
			break;

		    case CONFIG_CLIENTPERIOD:
			if (ntokens < 2) {
				msyslog(LOG_ERR,
					"no value for clientperiod command - line ignored");
			} else {
				u_long ui;

				if (!atouint(tokens[1], &ui) || ui < 64) {
					msyslog(LOG_ERR,
						"illegal value for clientperiod command - line ignored");
				} else {
					char bp[80];

					sprintf(bp, "client_limit_period=%ld", ui);
					set_sys_var(bp, strlen(bp)+1, RO);
					client_limit_period = ui;
				}
			}
			break;

		    case CONFIG_ENABLE:
			for (i = 1; i < ntokens; i++) {
				int flag;

				flag = matchkey(tokens[i], flags_keywords);
				if (flag == CONFIG_UNKNOWN) {
					msyslog(LOG_ERR,
						"enable unknown flag %s",
						tokens[i]);
					errflg = 1;
					break;
				}
				proto_config(flag, 1, 0.);
			}
			break;

		    case CONFIG_DISABLE:
			for (i = 1; i < ntokens; i++) {
				int flag;

				flag = matchkey(tokens[i], flags_keywords);
				if (flag == CONFIG_UNKNOWN) {
					msyslog(LOG_ERR,
						"disable unknown flag %s",
						tokens[i]);
					errflg = 1;
					break;
				}
				proto_config(flag, 0, 0.);
			}
			break;

		    case CONFIG_PHONE:
			for (i = 1; i < ntokens && i < MAXPHONE; i++) {
				(void)strncpy(sys_phone[i - 1],
					      tokens[i], MAXDIAL);
			}
			sys_phone[i - 1][0] = '\0';
			break;

		    case CONFIG_PPS:
			if (ntokens < 2) {
				msyslog(LOG_ERR,
					"pps missing device name");
				break;
			}
			(void)strncpy(pps_device, tokens[1], MAXPPS);
			for (i = 2; i < ntokens; i++) {
				int flag;

				flag = matchkey(tokens[i], pps_keywords);
				switch(flag) {
				    case CONF_PPS_ASSERT:
					pps_assert = 0;
					break;
				    case CONF_PPS_CLEAR:
					pps_assert = 1;
					break;
				    case CONF_PPS_HARDPPS:
					pps_hardpps = 1;
					break;
				    default:
					msyslog(LOG_ERR,
						"pps unknown flag %s",
						tokens[i]);
					errflg = 1;
					break;
				}
				if(errflg)
				    break;
			}
			break;
		}
	}
	if (fp[0])
		(void)fclose(fp[0]);

#ifdef HAVE_NETINFO
	if (config_netinfo)
		free_netinfo_config(config_netinfo);
#endif /* HAVE_NETINFO */

#if !defined(VMS) && !defined(SYS_VXWORKS)
	/* find a keyid */
	if (info_auth_keyid == 0)
		req_keyid = 65535;
	else
		req_keyid = info_auth_keyid;

	/* if doesn't exist, make up one at random */
	if (!authhavekey(req_keyid)) {
		char rankey[9];
		int j;

		for (i = 0; i < 8; i++)
			for (j = 1; j < 100; ++j) {
				rankey[i] = RANDOM & 0xff;
				if (rankey[i] != 0) break;
			}
		rankey[8] = 0;
		authusekey(req_keyid, KEY_TYPE_MD5, (u_char *)rankey);
		authtrust(req_keyid, 1);
		if (!authhavekey(req_keyid)) {
			msyslog(LOG_ERR, "getconfig: Couldn't generate a valid random key!");
			/* HMS: Should this be fatal? */
		}
	}

	/* save keyid so we will accept config requests with it */
	info_auth_keyid = req_keyid;
#endif /* !defined(VMS) && !defined(SYS_VXWORKS) */

	if (res_fp != NULL) {
		/*
		 * Need name resolution
		 */
		do_resolve_internal();
	}
}


#ifdef HAVE_NETINFO

/* 
 * get_netinfo_config - find the nearest NetInfo domain with an ntp
 * configuration and initialize the configuration state.
 */
static struct netinfo_config_state *
get_netinfo_config()
{
	ni_status status;
	void *domain;
	ni_id config_dir;
       	struct netinfo_config_state *config;

	if (ni_open(NULL, ".", &domain) != NI_OK) return NULL;

	while ((status = ni_pathsearch(domain, &config_dir, NETINFO_CONFIG_DIR)) == NI_NODIR) {
		void *next_domain;
		if (ni_open(domain, "..", &next_domain) != NI_OK) {
			ni_free(next_domain);
			break;
		}
		ni_free(domain);
		domain = next_domain;
	}
	if (status != NI_OK) {
		ni_free(domain);
		return NULL;
	}

       	config = (struct netinfo_config_state *)malloc(sizeof(struct netinfo_config_state));
       	config->domain = domain;
       	config->config_dir = config_dir;
       	config->prop_index = 0;
       	config->val_index = 0;
       	config->val_list = NULL;

	return config;
}



/*
 * free_netinfo_config - release NetInfo configuration state
 */
static void
free_netinfo_config(struct netinfo_config_state *config)
{
	ni_free(config->domain);
	free(config);
}



/*
 * gettokens_netinfo - return tokens from NetInfo
 */
static int
gettokens_netinfo (
	struct netinfo_config_state *config,
	char **tokenlist,
	int *ntokens
	)
{
	int prop_index = config->prop_index;
	int val_index = config->val_index;
	char **val_list = config->val_list;

	/*
	 * Iterate through each keyword and look for a property that matches it.
	 */
	again:
	if (!val_list) {
	       	for (; prop_index < (sizeof(keywords)/sizeof(keywords[0])); prop_index++)
	       	{
		       	ni_namelist namelist;
			struct keyword current_prop = keywords[prop_index];

			/*
			 * For each value associated in the property, we're going to return
			 * a separate line. We squirrel away the values in the config state
			 * so the next time through, we don't need to do this lookup.
			 */
		       	NI_INIT(&namelist);
	       		if (ni_lookupprop(config->domain, &config->config_dir, current_prop.text, &namelist) == NI_OK) {
				ni_index index;

				/* Found the property, but it has no values */
				if (namelist.ni_namelist_len == 0) continue;

				if (! (val_list = config->val_list = (char**)malloc(sizeof(char*) * (namelist.ni_namelist_len + 1))))
					{ msyslog(LOG_ERR, "out of memory while configuring"); break; }

				for (index = 0; index < namelist.ni_namelist_len; index++) {
					char *value = namelist.ni_namelist_val[index];

					if (! (val_list[index] = (char*)malloc(strlen(value+1))))
						{ msyslog(LOG_ERR, "out of memory while configuring"); break; }

					strcpy(val_list[index], value);
				}
				val_list[index] = NULL;

				break;
			}
			ni_namelist_free(&namelist);
		}
		config->prop_index = prop_index;
	}

	/* No list; we're done here. */
       	if (!val_list) return CONFIG_UNKNOWN;

	/*
	 * We have a list of values for the current property.
	 * Iterate through them and return each in order.
	 */
	if (val_list[val_index])
	{
		int ntok = 1;
		int quoted = 0;
		char *tokens = val_list[val_index];

		msyslog(LOG_INFO, "%s %s", keywords[prop_index].text, val_list[val_index]);

		(const char*)tokenlist[0] = keywords[prop_index].text;
		for (ntok = 1; ntok < MAXTOKENS; ntok++) {
			tokenlist[ntok] = tokens;
			while (!ISEOL(*tokens) && (!ISSPACE(*tokens) || quoted))
				quoted ^= (*tokens++ == '"');

			if (ISEOL(*tokens)) {
				*tokens = '\0';
				break;
			} else {		/* must be space */
				*tokens++ = '\0';
				while (ISSPACE(*tokens)) tokens++;
				if (ISEOL(*tokens)) break;
			}
		}
		*ntokens = ntok + 1;
		
		config->val_index++;

		return keywords[prop_index].keytype;
	}

	/* We're done with the current property. */
	prop_index = ++config->prop_index;

	/* Free val_list and reset counters. */
	for (val_index = 0; val_list[val_index]; val_index++)
		free(val_list[val_index]);
       	free(val_list);	val_list = config->val_list = NULL; val_index = config->val_index = 0;

	goto again;
}

#endif /* HAVE_NETINFO */


/*
 * gettokens - read a line and return tokens
 */
static int
gettokens (
	FILE *fp,
	char *line,
	char **tokenlist,
	int *ntokens
	)
{
	register char *cp;
	register int ntok;
	register int quoted = 0;

	/*
	 * Find start of first token
	 */
	again:
	while ((cp = fgets(line, MAXLINE, fp)) != NULL) {
		cp = line;
		while (ISSPACE(*cp))
			cp++;
		if (!ISEOL(*cp))
			break;
	}
	if (cp == NULL) {
		*ntokens = 0;
		return CONFIG_UNKNOWN;	/* hack.  Is recognized as EOF */
	}

	/*
	 * Now separate out the tokens
	 */
	for (ntok = 0; ntok < MAXTOKENS; ntok++) {
		tokenlist[ntok] = cp;
		while (!ISEOL(*cp) && (!ISSPACE(*cp) || quoted))
			quoted ^= (*cp++ == '"');

		if (ISEOL(*cp)) {
			*cp = '\0';
			break;
		} else {		/* must be space */
			*cp++ = '\0';
			while (ISSPACE(*cp))
				cp++;
			if (ISEOL(*cp))
				break;
		}
	}

	/*
	 * Return the match
	 */
	*ntokens = ntok + 1;
	ntok = matchkey(tokenlist[0], keywords);
	if (ntok == CONFIG_UNKNOWN)
		goto again;
	return ntok;
}



/*
 * matchkey - match a keyword to a list
 */
static int
matchkey(
	register char *word,
	register struct keyword *keys
	)
{
	for (;;) {
		if (keys->keytype == CONFIG_UNKNOWN) {
			msyslog(LOG_ERR,
				"configure: keyword \"%s\" unknown, line ignored",
				word);
			return CONFIG_UNKNOWN;
		}
		if (STRSAME(word, keys->text))
			return keys->keytype;
		keys++;
	}
}


/*
 * getnetnum - return a net number (this is crude, but careful)
 */
static int
getnetnum(
	const char *num,
	struct sockaddr_in *addr,
	int complain
	)
{
	register const char *cp;
	register char *bp;
	register int i;
	register int temp;
	char buf[80];		/* will core dump on really stupid stuff */
	u_int32 netnum;

	/* XXX ELIMINATE replace with decodenetnum */
	cp = num;
	netnum = 0;
	for (i = 0; i < 4; i++) {
		bp = buf;
		while (isdigit((int)*cp))
			*bp++ = *cp++;
		if (bp == buf)
			break;

		if (i < 3) {
			if (*cp++ != '.')
				break;
		} else if (*cp != '\0')
			break;

		*bp = '\0';
		temp = atoi(buf);
		if (temp > 255)
			break;
		netnum <<= 8;
		netnum += temp;
#ifdef DEBUG
		if (debug > 3)
			printf("getnetnum %s step %d buf %s temp %d netnum %lu\n",
			   num, i, buf, temp, (u_long)netnum);
#endif
	}

	if (i < 4) {
		if (complain)
			msyslog(LOG_ERR,
				"getnetnum: \"%s\" invalid host number, line ignored",
				num);
#ifdef DEBUG
		if (debug > 3)
			printf(
				"getnetnum: \"%s\" invalid host number, line ignored\n",
				num);
#endif
		return 0;
	}

	/*
	 * make up socket address.	Clear it out for neatness.
	 */
	memset((void *)addr, 0, sizeof(struct sockaddr_in));
	addr->sin_family = AF_INET;
	addr->sin_port = htons(NTP_PORT);
	addr->sin_addr.s_addr = htonl(netnum);
#ifdef DEBUG
	if (debug > 1)
		printf("getnetnum given %s, got %s (%lx)\n",
		   num, ntoa(addr), (u_long)netnum);
#endif
	return 1;
}


#if !defined(VMS)
/*
 * catchchild - receive the resolver's exit status
 */
static RETSIGTYPE
catchchild(
	int sig
	)
{
	/*
	 * We only start up one child, and if we're here
	 * it should have already exited.  Hence the following
	 * shouldn't hang.  If it does, please tell me.
	 */
#if !defined (SYS_WINNT) && !defined(SYS_VXWORKS)
	(void) wait(0);
#endif /* SYS_WINNT  && VXWORKS*/
}
#endif /* VMS */


/*
 * save_resolve - save configuration info into a file for later name resolution
 */
static void
save_resolve(
	char *name,
	int mode,
	int version,
	int minpoll,
	int maxpoll,
	u_int flags,
	int ttl,
	keyid_t keyid,
	u_char *keystr
	)
{
#ifndef SYS_VXWORKS
	if (res_fp == NULL) {
#ifndef SYS_WINNT
		(void) strcpy(res_file, RES_TEMPFILE);
#else
		/* no /tmp directory under NT */
		{
			DWORD len;
			if(!(len = GetTempPath((DWORD)MAX_PATH, (LPTSTR)res_file))) {
				msyslog(LOG_ERR, "cannot get pathname for temporary directory: %m");
				return;
			}
			(void) strcat(res_file, "ntpdXXXXXX");
		}
#endif /* SYS_WINNT */
#ifdef HAVE_MKSTEMP
		{
			int fd;

			res_fp = NULL;
			if ((fd = mkstemp(res_file)) != -1)
				res_fp = fdopen(fd, "r+");
		}
#else
		(void) mktemp(res_file);
		res_fp = fopen(res_file, "w");
#endif
		if (res_fp == NULL) {
			msyslog(LOG_ERR, "open failed for %s: %m", res_file);
			return;
		}
	}
#ifdef DEBUG
	if (debug) {
		printf("resolving %s\n", name);
	}
#endif

	(void)fprintf(res_fp, "%s %d %d %d %d %d %d %u %s\n", name,
	    mode, version, minpoll, maxpoll, flags, ttl, keyid, keystr);
#ifdef DEBUG
	if (debug > 1)
		printf("config: %s %d %d %d %d %x %d %u %s\n", name, mode,
		    version, minpoll, maxpoll, flags, ttl, keyid, keystr);
#endif

#else  /* SYS_VXWORKS */
	/* save resolve info to a struct */
#endif /* SYS_VXWORKS */
}


/*
 * abort_resolve - terminate the resolver stuff and delete the file
 */
static void
abort_resolve(void)
{
	/*
	 * In an ideal world we would might reread the file and
	 * log the hosts which aren't getting configured.  Since
	 * this is too much work, however, just close and delete
	 * the temp file.
	 */
	if (res_fp != NULL)
		(void) fclose(res_fp);
	res_fp = NULL;

#ifndef SYS_VXWORKS		/* we don't open the file to begin with */
#if !defined(VMS)
	(void) unlink(res_file);
#else
	(void) delete(res_file);
#endif /* VMS */
#endif /* SYS_VXWORKS */
}


/*
 * do_resolve_internal - start up the resolver function (not program)
 */
/*
 * On VMS, this routine will simply refuse to resolve anything.
 *
 * Possible implementation: keep `res_file' in memory, do async
 * name resolution via QIO, update from within completion AST.
 * I'm unlikely to find the time for doing this, though. -wjm
 */
static void
do_resolve_internal(void)
{
	int i;

	if (res_fp == NULL) {
		/* belch */
		msyslog(LOG_ERR,
			"do_resolve_internal: Fatal: res_fp == NULL");
		exit(1);
	}

	/* we are done with this now */
	(void) fclose(res_fp);
	res_fp = NULL;

#if !defined(VMS) && !defined (SYS_VXWORKS)
	req_file = res_file;	/* set up pointer to res file */
#ifndef SYS_WINNT
	(void) signal_no_reset(SIGCHLD, catchchild);

#ifndef SYS_VXWORKS
	i = fork();
	if (i == 0) {
		/*
		 * this used to close everything
		 * I don't think this is necessary
		 */
		/*
		 * To the unknown commenter above:
		 * Well, I think it's better to clean up
		 * after oneself. I have had problems with
		 * refclock-io when intres was running - things
		 * where fine again when ntpintres was gone.
		 * So some systems react erratic at least.
		 *
		 *			Frank Kardel
		 *
		 * 94-11-16:
		 * Further debugging has proven that the above is
		 * absolutely harmful. The internal resolver
		 * is still in the SIGIO process group and the lingering
		 * async io information causes it to process requests from
		 * all file decriptor causing a race between the NTP daemon
		 * and the resolver. which then eats data when it wins 8-(.
		 * It is absolutly necessary to kill any IO associations
		 * shared with the NTP daemon.
		 *
		 * We also block SIGIO (currently no ports means to
		 * disable the signal handle for IO).
		 *
		 * Thanks to wgstuken@informatik.uni-erlangen.de to notice
		 * that it is the ntp-resolver child running into trouble.
		 *
		 * THUS:
		 */

		closelog();
		kill_asyncio();

		(void) signal_no_reset(SIGCHLD, SIG_DFL);

#ifdef DEBUG
		if (0)
		    debug = 2;
#endif

# ifndef LOG_DAEMON
		openlog("ntpd_initres", LOG_PID);
# else /* LOG_DAEMON */

#  ifndef LOG_NTP
#   define	LOG_NTP LOG_DAEMON
#  endif
		openlog("ntpd_initres", LOG_PID | LOG_NDELAY, LOG_NTP);
#ifndef SYS_CYGWIN32
#  ifdef DEBUG
		if (debug)
		    setlogmask(LOG_UPTO(LOG_DEBUG));
		else
#  endif /* DEBUG */
		    setlogmask(LOG_UPTO(LOG_DEBUG)); /* @@@ was INFO */
# endif /* LOG_DAEMON */
#endif

		ntp_intres();

		/*
		 * If we got here, the intres code screwed up.
		 * Print something so we don't die without complaint
		 */
		msyslog(LOG_ERR, "call to ntp_intres lost");
		abort_resolve();
		exit(1);
	}
#else
	 /* vxWorks spawns a thread... -casey */
	 i = sp (ntp_intres);
	 /*i = taskSpawn("ntp_intres",100,VX_FP_TASK,20000,ntp_intres);*/
#endif
	if (i == -1) {
		msyslog(LOG_ERR, "fork() failed, can't start ntp_intres: %m");
		(void) signal_no_reset(SIGCHLD, SIG_DFL);
		abort_resolve();
	}
#else /* SYS_WINNT */
	{
		/* NT's equivalent of fork() is _spawn(), but the start point
		 * of the new process is an executable filename rather than
		 * a function name as desired here.
		 */
		DWORD dwThreadId;
		fflush(stdout);
		if (!(ResolverThreadHandle = CreateThread(
			NULL,								 /* no security attributes	*/
			0,									 /* use default stack size	*/
			(LPTHREAD_START_ROUTINE) ntp_intres, /* thread function		*/
			NULL,								 /* argument to thread function   */
			0,									 /* use default creation flags	  */
			&dwThreadId))) {					 /* returns the thread identifier */
			msyslog(LOG_ERR, "CreateThread() failed, can't start ntp_intres");
			abort_resolve();
		}
	}
#endif /* SYS_WINNT */
#else /* VMS  VX_WORKS */
	msyslog(LOG_ERR,
		"Name resolution not implemented for VMS - use numeric addresses");
	abort_resolve();
#endif /* VMS VX_WORKS */
}
