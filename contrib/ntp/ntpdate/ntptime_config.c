/*
 * ntptime_config.c
 *
 * What follows is a simplified version of the config parsing code
 * in ntpd/ntp_config.c.  We only parse a subset of the configuration
 * syntax, and don't bother whining about things we don't understand.
 *
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

#include <stdio.h>
#include <signal.h>
#include <ctype.h>
#include <sys/time.h>

#include "ntp_fp.h"
#include "ntp.h"
#include "ntp_io.h"
#include "ntp_unixtime.h"
#include "ntp_filegen.h"
#include "ntpdate.h"
#include "ntp_syslog.h"
#include "ntp_stdlib.h"

/*
 * These routines are used to read the configuration file at
 * startup time.  An entry in the file must fit on a single line.
 * Entries are processed as multiple tokens separated by white space
 * Lines are considered terminated when a '#' is encountered.  Blank
 * lines are ignored.
 */

/*
 * Configuration file name
 */
#ifndef CONFIG_FILE
# ifndef SYS_WINNT
#  define	CONFIG_FILE "/etc/ntp.conf"
# else /* SYS_WINNT */
#  define	CONFIG_FILE 	"%windir%\\ntp.conf"
#  define	ALT_CONFIG_FILE "%windir%\\ntp.ini"
# endif /* SYS_WINNT */
#endif /* not CONFIG_FILE */

/*
 *
 * We understand the following configuration entries and defaults.
 *
 * peer [ addr ] [ version 3 ] [ key 0 ] [ minpoll 6 ] [ maxpoll 10 ]
 * server [ addr ] [ version 3 ] [ key 0 ] [ minpoll 6 ] [ maxpoll 10 ]
 * keys file_name
 */

#define CONFIG_UNKNOWN		0

#define CONFIG_PEER 		1
#define CONFIG_SERVER		2
#define CONFIG_KEYS		8

#define CONF_MOD_VERSION	1
#define CONF_MOD_KEY		2
#define CONF_MOD_MINPOLL	3
#define CONF_MOD_MAXPOLL	4
#define CONF_MOD_PREFER 	5
#define CONF_MOD_BURST		6
#define CONF_MOD_SKEY		7
#define CONF_MOD_TTL		8
#define CONF_MOD_MODE		9

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
	{ "peer",       CONFIG_PEER },
	{ "server",     CONFIG_SERVER },
	{ "keys",       CONFIG_KEYS },
	{ "",           CONFIG_UNKNOWN }
};

/*
 * "peer", "server", "broadcast" modifier keywords
 */
static	struct keyword mod_keywords[] = {
	{ "version",    CONF_MOD_VERSION },
	{ "key",    CONF_MOD_KEY },
	{ "minpoll",    CONF_MOD_MINPOLL },
	{ "maxpoll",    CONF_MOD_MAXPOLL },
	{ "prefer", CONF_MOD_PREFER },
	{ "burst",  CONF_MOD_BURST },
	{ "autokey",    CONF_MOD_SKEY },
	{ "mode",   CONF_MOD_MODE },    /* reference clocks */
	{ "ttl",    CONF_MOD_TTL },     /* NTP peers */
	{ "",       CONFIG_UNKNOWN }
};

/*
 * Limits on things
 */
#define MAXTOKENS	20	/* 20 tokens on line */
#define MAXLINE 	1024	/* maximum length of line */
#define MAXFILENAME 128 /* maximum length of a file name (alloca()?) */

/*
 * Miscellaneous macros
 */
#define STRSAME(s1, s2) 	(*(s1) == *(s2) && strcmp((s1), (s2)) == 0)
#define ISEOL(c)		((c) == '#' || (c) == '\n' || (c) == '\0')
#define ISSPACE(c)		((c) == ' ' || (c) == '\t')
#define STREQ(a, b) 	(*(a) == *(b) && strcmp((a), (b)) == 0)

/*
 * Systemwide parameters and flags
 */
extern struct server **sys_servers;	/* the server list */
extern int sys_numservers; 	/* number of servers to poll */
extern char *key_file;

/*
 * Function prototypes
 */
static	int gettokens	P((FILE *, char *, char **, int *));
static	int matchkey	P((char *, struct keyword *));
static	int getnetnum	P((const char *num, struct sockaddr_in *addr,
			   int complain));


/*
 * loadservers - load list of NTP servers from configuration file
 */
void
loadservers(
	char *cfgpath
	)
{
	register int i;
	int errflg;
	int peerversion;
	int minpoll;
	int maxpoll;
	/* int ttl; */
	int srvcnt;
	/* u_long peerkey; */
	int peerflags;
	struct sockaddr_in peeraddr;
	FILE *fp;
	char line[MAXLINE];
	char *(tokens[MAXTOKENS]);
	int ntokens;
	int tok;
	const char *config_file;
#ifdef SYS_WINNT
	char *alt_config_file;
	LPTSTR temp;
	char config_file_storage[MAX_PATH];
	char alt_config_file_storage[MAX_PATH];
#endif /* SYS_WINNT */
	struct server *server, *srvlist;

	/*
	 * Initialize, initialize
	 */
	srvcnt = 0;
	srvlist = 0;
	errflg = 0;
#ifdef DEBUG
	debug = 0;
#endif	/* DEBUG */
#ifndef SYS_WINNT
	config_file = cfgpath ? cfgpath : CONFIG_FILE;
#else
	if (cfgpath) {
		config_file = cfgpath;
	} else {
		temp = CONFIG_FILE;
		if (!ExpandEnvironmentStrings((LPCTSTR)temp, (LPTSTR)config_file_storage, (DWORD)sizeof(config_file_storage))) {
			msyslog(LOG_ERR, "ExpandEnvironmentStrings CONFIG_FILE failed: %m\n");
			exit(1);
		}
		config_file = config_file_storage;
	}

	temp = ALT_CONFIG_FILE;
	if (!ExpandEnvironmentStrings((LPCTSTR)temp, (LPTSTR)alt_config_file_storage, (DWORD)sizeof(alt_config_file_storage))) {
		msyslog(LOG_ERR, "ExpandEnvironmentStrings ALT_CONFIG_FILE failed: %m\n");
		exit(1);
	}
	alt_config_file = alt_config_file_storage;
M
#endif /* SYS_WINNT */

	if ((fp = fopen(FindConfig(config_file), "r")) == NULL)
	{
		fprintf(stderr, "getconfig: Couldn't open <%s>\n", FindConfig(config_file));
		msyslog(LOG_INFO, "getconfig: Couldn't open <%s>", FindConfig(config_file));
#ifdef SYS_WINNT
		/* Under WinNT try alternate_config_file name, first NTP.CONF, then NTP.INI */

		if ((fp = fopen(FindConfig(alt_config_file), "r")) == NULL) {

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

	while ((tok = gettokens(fp, line, tokens, &ntokens))
	       != CONFIG_UNKNOWN) {
		switch(tok) {
		    case CONFIG_PEER:
		    case CONFIG_SERVER:
			
			if (ntokens < 2) {
				msyslog(LOG_ERR,
					"No address for %s, line ignored",
					tokens[0]);
				break;
			}
			
			if (!getnetnum(tokens[1], &peeraddr, 1)) {
				/* Resolve now, or lose! */
				break;
			} else {
				errflg = 0;
				
				/* Shouldn't be able to specify multicast */
				if (IN_CLASSD(ntohl(peeraddr.sin_addr.s_addr))
				    || ISBADADR(&peeraddr)) {
					msyslog(LOG_ERR,
						"attempt to configure invalid address %s",
						ntoa(&peeraddr));
					break;
				}
			}

			peerversion = NTP_VERSION;
			minpoll = NTP_MINDPOLL;
			maxpoll = NTP_MAXDPOLL;
			/* peerkey = 0; */
			peerflags = 0;
			/* ttl = 0; */
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
				    ++i;
				    /* peerkey = (int)atol(tokens[i]); */
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
				    if (minpoll < NTP_MINPOLL)
					minpoll = NTP_MINPOLL;
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
				    if (maxpoll > NTP_MAXPOLL)
					maxpoll = NTP_MAXPOLL;
				    break;

				case CONF_MOD_PREFER:
				    peerflags |= FLAG_PREFER;
				    break;

				case CONF_MOD_BURST:
				    peerflags |= FLAG_BURST;
				    break;

				case CONF_MOD_SKEY:
				    peerflags |= FLAG_SKEY | FLAG_AUTHENABLE;
				    break;

				case CONF_MOD_TTL:
				    if (i >= ntokens-1) {
					    msyslog(LOG_ERR,
						    "ttl: argument required");
					    errflg = 1;
					    break;
				    }
				    ++i;
				    /* ttl = atoi(tokens[i]); */
				    break;

				case CONF_MOD_MODE:
				    if (i >= ntokens-1) {
					    msyslog(LOG_ERR,
						    "mode: argument required");
					    errflg = 1;
					    break;
				    }
				    ++i;
				    /* ttl = atoi(tokens[i]); */
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
				server = (struct server *)emalloc(sizeof(struct server));
				memset((char *)server, 0, sizeof(struct server));
				server->srcadr = peeraddr;
				server->version = peerversion;
				server->dispersion = PEER_MAXDISP;
				server->next_server = srvlist;
				srvlist = server;
				srvcnt++;
			}
			break;
			
			case CONFIG_KEYS:
			if (ntokens >= 2) {
				key_file = (char *) emalloc(strlen(tokens[1]) + 1);
				strcpy(key_file, tokens[1]);
			}
			break;
		}
	}
	(void) fclose(fp);

	/* build final list */
	sys_numservers = srvcnt;
	sys_servers = (struct server **) 
	    emalloc(sys_numservers * sizeof(struct server *));
	for(i=0;i<sys_numservers;i++) {
		sys_servers[i] = srvlist;
		srvlist = srvlist->next_server;
	}
}



/*
 * gettokens - read a line and return tokens
 */
static int
gettokens(
	FILE *fp,
	char *line,
	char **tokenlist,
	int *ntokens
	)
{
	register char *cp;
	register int eol;
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
	eol = 0;
	ntok = 0;
	while (!eol) {
		tokenlist[ntok++] = cp;
		while (!ISEOL(*cp) && (!ISSPACE(*cp) || quoted))
			quoted ^= (*cp++ == '"');

		if (ISEOL(*cp)) {
			*cp = '\0';
			eol = 1;
		} else {		/* must be space */
			*cp++ = '\0';
			while (ISSPACE(*cp))
				cp++;
			if (ISEOL(*cp))
				eol = 1;
		}
		if (ntok == MAXTOKENS)
			eol = 1;
	}

	/*
	 * Return the match
	 */
	*ntokens = ntok;
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
