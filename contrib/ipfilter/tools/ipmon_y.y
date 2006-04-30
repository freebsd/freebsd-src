/*	$FreeBSD$	*/

%{
#include "ipf.h"
#include <syslog.h>
#undef	OPT_NAT
#undef	OPT_VERBOSE
#include "ipmon_l.h"
#include "ipmon.h"

#define	YYDEBUG	1

extern	void	yyerror __P((char *));
extern	int	yyparse __P((void));
extern	int	yylex __P((void));
extern	int	yydebug;
extern	FILE	*yyin;
extern	int	yylineNum;

typedef	struct	opt	{
	struct	opt	*o_next;
	int		o_line;
	int		o_type;
	int		o_num;
	char		*o_str;
	struct in_addr	o_ip;
} opt_t;

static	void	build_action __P((struct opt *));
static	opt_t	*new_opt __P((int));
static	void	free_action __P((ipmon_action_t *));

static	ipmon_action_t	*alist = NULL;
%}

%union	{
	char	*str;
	u_32_t	num;
	struct in_addr	addr;
	struct opt	*opt;
	union	i6addr	ip6;
}

%token	<num>	YY_NUMBER YY_HEX
%token	<str>	YY_STR
%token	<ip6>	YY_IPV6
%token	YY_COMMENT 
%token	YY_CMP_EQ YY_CMP_NE YY_CMP_LE YY_CMP_GE YY_CMP_LT YY_CMP_GT
%token	YY_RANGE_OUT YY_RANGE_IN

%token	IPM_MATCH IPM_BODY IPM_COMMENT IPM_DIRECTION IPM_DSTIP IPM_DSTPORT
%token	IPM_EVERY IPM_EXECUTE IPM_GROUP IPM_INTERFACE IPM_IN IPM_NO IPM_OUT
%token	IPM_PACKET IPM_PACKETS IPM_POOL IPM_PROTOCOL IPM_RESULT IPM_RULE
%token	IPM_SECOND IPM_SECONDS IPM_SRCIP IPM_SRCPORT IPM_LOGTAG IPM_WITH
%token	IPM_DO IPM_SAVE IPM_SYSLOG IPM_NOTHING IPM_RAW IPM_TYPE IPM_NAT
%token	IPM_STATE IPM_NATTAG IPM_IPF
%type	<addr> ipv4
%type	<opt> direction dstip dstport every execute group interface
%type	<opt> protocol result rule srcip srcport logtag matching
%type	<opt> matchopt nattag type doopt doing save syslog nothing
%type	<num> saveopts saveopt typeopt

%%
file:	line
	| assign
	| file line
	| file assign
	;

line:	IPM_MATCH '{' matching '}' IPM_DO '{' doing '}' ';'
					{ build_action($3); resetlexer(); }
	| IPM_COMMENT
	| YY_COMMENT
	;

assign:	YY_STR assigning YY_STR ';'		{ set_variable($1, $3);
						  resetlexer();
						  free($1);
						  free($3);
						} 
	;

assigning:
	'='					{ yyvarnext = 1; }
	;

matching:
	matchopt				{ $$ = $1; }
	| matchopt ',' matching			{ $1->o_next = $3; $$ = $1; }
	;

matchopt:
	direction				{ $$ = $1; }
	| dstip					{ $$ = $1; }
	| dstport				{ $$ = $1; }
	| every					{ $$ = $1; }
	| group					{ $$ = $1; }
	| interface				{ $$ = $1; }
	| protocol				{ $$ = $1; }
	| result				{ $$ = $1; }
	| rule					{ $$ = $1; }
	| srcip					{ $$ = $1; }
	| srcport				{ $$ = $1; }
	| logtag				{ $$ = $1; }
	| nattag				{ $$ = $1; }
	| type					{ $$ = $1; }
	;

doing:
	doopt					{ $$ = $1; }
	| doopt ',' doing			{ $1->o_next = $3; $$ = $1; }
	;

doopt:
	execute					{ $$ = $1; }
	| save					{ $$ = $1; }
	| syslog				{ $$ = $1; }
	| nothing				{ $$ = $1; }
	;

direction:
	IPM_DIRECTION '=' IPM_IN		{ $$ = new_opt(IPM_DIRECTION);
						  $$->o_num = IPM_IN; }
	| IPM_DIRECTION '=' IPM_OUT		{ $$ = new_opt(IPM_DIRECTION);
						  $$->o_num = IPM_OUT; }
	;

dstip:	IPM_DSTIP '=' ipv4 '/' YY_NUMBER	{ $$ = new_opt(IPM_DSTIP);
						  $$->o_ip = $3;
						  $$->o_num = $5; }
	;

dstport:
	IPM_DSTPORT '=' YY_NUMBER		{ $$ = new_opt(IPM_DSTPORT);
						  $$->o_num = $3; }
	| IPM_DSTPORT '=' YY_STR		{ $$ = new_opt(IPM_DSTPORT);
						  $$->o_str = $3; }
	;

every:	IPM_EVERY IPM_SECOND			{ $$ = new_opt(IPM_SECOND);
						  $$->o_num = 1; }
	| IPM_EVERY YY_NUMBER IPM_SECONDS	{ $$ = new_opt(IPM_SECOND);
						  $$->o_num = $2; }
	| IPM_EVERY IPM_PACKET			{ $$ = new_opt(IPM_PACKET);
						  $$->o_num = 1; }
	| IPM_EVERY YY_NUMBER IPM_PACKETS	{ $$ = new_opt(IPM_PACKET);
						  $$->o_num = $2; }
	;

group:	IPM_GROUP '=' YY_NUMBER			{ $$ = new_opt(IPM_GROUP);
						  $$->o_num = $3; }
	| IPM_GROUP '=' YY_STR			{ $$ = new_opt(IPM_GROUP);
						  $$->o_str = $3; }
	;

interface:
	IPM_INTERFACE '=' YY_STR		{ $$ = new_opt(IPM_INTERFACE);
						  $$->o_str = $3; }
	;

logtag:	IPM_LOGTAG '=' YY_NUMBER		{ $$ = new_opt(IPM_LOGTAG);
						  $$->o_num = $3; }
	;

nattag:	IPM_NATTAG '=' YY_STR			{ $$ = new_opt(IPM_NATTAG);
						  $$->o_str = $3; }
	;

protocol:
	IPM_PROTOCOL '=' YY_NUMBER		{ $$ = new_opt(IPM_PROTOCOL);
						  $$->o_num = $3; }
	| IPM_PROTOCOL '=' YY_STR		{ $$ = new_opt(IPM_PROTOCOL);
						  $$->o_num = getproto($3);
						  free($3);
						}
	;

result:	IPM_RESULT '=' YY_STR			{ $$ = new_opt(IPM_RESULT);
						  $$->o_str = $3; }
	;

rule:	IPM_RULE '=' YY_NUMBER			{ $$ = new_opt(IPM_RULE);
						  $$->o_num = YY_NUMBER; }
	;

srcip:	IPM_SRCIP '=' ipv4 '/' YY_NUMBER	{ $$ = new_opt(IPM_SRCIP);
						  $$->o_ip = $3;
						  $$->o_num = $5; }
	;

srcport:
	IPM_SRCPORT '=' YY_NUMBER		{ $$ = new_opt(IPM_SRCPORT);
						  $$->o_num = $3; }
	| IPM_SRCPORT '=' YY_STR		{ $$ = new_opt(IPM_SRCPORT);
						  $$->o_str = $3; }
	;

type:	IPM_TYPE '=' typeopt			{ $$ = new_opt(IPM_TYPE);
						  $$->o_num = $3; }
	;

typeopt:
	IPM_IPF					{ $$ = IPL_MAGIC; }
	| IPM_NAT				{ $$ = IPL_MAGIC_NAT; }
	| IPM_STATE				{ $$ = IPL_MAGIC_STATE; }
	;

execute:
	IPM_EXECUTE YY_STR			{ $$ = new_opt(IPM_EXECUTE);
						  $$->o_str = $2; }
	;

save:	IPM_SAVE saveopts YY_STR		{ $$ = new_opt(IPM_SAVE);
						  $$->o_num = $2;
						  $$->o_str = $3; }
	;

saveopts:					{ $$ = 0; }
	| saveopt				{ $$ = $1; }
	| saveopt ',' saveopts			{ $$ = $1 | $3; }
	;

saveopt:
	IPM_RAW					{ $$ = IPMDO_SAVERAW; }
	;

syslog:	IPM_SYSLOG				{ $$ = new_opt(IPM_SYSLOG); }
	;

nothing:
	IPM_NOTHING				{ $$ = 0; }
	;

ipv4:   YY_NUMBER '.' YY_NUMBER '.' YY_NUMBER '.' YY_NUMBER
		{ if ($1 > 255 || $3 > 255 || $5 > 255 || $7 > 255) {
			yyerror("Invalid octet string for IP address");
			return 0;
		  }
		  $$.s_addr = ($1 << 24) | ($3 << 16) | ($5 << 8) | $7;
		  $$.s_addr = htonl($$.s_addr);
		}
%%
static	struct	wordtab	yywords[] = {
	{ "body",	IPM_BODY },
	{ "direction",	IPM_DIRECTION },
	{ "do",		IPM_DO },
	{ "dstip",	IPM_DSTIP },
	{ "dstport",	IPM_DSTPORT },
	{ "every",	IPM_EVERY },
	{ "execute",	IPM_EXECUTE },
	{ "group",	IPM_GROUP },
	{ "in",		IPM_IN },
	{ "interface",	IPM_INTERFACE },
	{ "ipf",	IPM_IPF },
	{ "logtag",	IPM_LOGTAG },
	{ "match",	IPM_MATCH },
	{ "nat",	IPM_NAT },
	{ "nattag",	IPM_NATTAG },
	{ "no",		IPM_NO },
	{ "nothing",	IPM_NOTHING },
	{ "out",	IPM_OUT },
	{ "packet",	IPM_PACKET },
	{ "packets",	IPM_PACKETS },
	{ "protocol",	IPM_PROTOCOL },
	{ "result",	IPM_RESULT },
	{ "rule",	IPM_RULE },
	{ "save",	IPM_SAVE },
	{ "second",	IPM_SECOND },
	{ "seconds",	IPM_SECONDS },
	{ "srcip",	IPM_SRCIP },
	{ "srcport",	IPM_SRCPORT },
	{ "state",	IPM_STATE },
	{ "syslog",	IPM_SYSLOG },
	{ "with",	IPM_WITH },
	{ NULL,		0 }
};

static int macflags[17][2] = {
	{ IPM_DIRECTION,	IPMAC_DIRECTION	},
	{ IPM_DSTIP,		IPMAC_DSTIP	},
	{ IPM_DSTPORT,		IPMAC_DSTPORT	},
	{ IPM_GROUP,		IPMAC_GROUP	},
	{ IPM_INTERFACE,	IPMAC_INTERFACE	},
	{ IPM_LOGTAG,		IPMAC_LOGTAG 	},
	{ IPM_NATTAG,		IPMAC_NATTAG 	},
	{ IPM_PACKET,		IPMAC_EVERY	},
	{ IPM_PROTOCOL,		IPMAC_PROTOCOL	},
	{ IPM_RESULT,		IPMAC_RESULT	},
	{ IPM_RULE,		IPMAC_RULE	},
	{ IPM_SECOND,		IPMAC_EVERY	},
	{ IPM_SRCIP,		IPMAC_SRCIP	},
	{ IPM_SRCPORT,		IPMAC_SRCPORT	},
	{ IPM_TYPE,		IPMAC_TYPE 	},
	{ IPM_WITH,		IPMAC_WITH 	},
	{ 0, 0 }
};

static opt_t *new_opt(type)
int type;
{
	opt_t *o;

	o = (opt_t *)malloc(sizeof(*o));
	o->o_type = type;
	o->o_line = yylineNum;
	o->o_num = 0;
	o->o_str = (char *)0;
	o->o_next = NULL;
	return o;
}

static void build_action(olist)
opt_t *olist;
{
	ipmon_action_t *a;
	opt_t *o;
	char c;
	int i;

	a = (ipmon_action_t *)calloc(1, sizeof(*a));
	if (a == NULL)
		return;
	while ((o = olist) != NULL) {
		/*
		 * Check to see if the same comparator is being used more than
		 * once per matching statement.
		 */
		for (i = 0; macflags[i][0]; i++)
			if (macflags[i][0] == o->o_type)
				break;
		if (macflags[i][1] & a->ac_mflag) {
			fprintf(stderr, "%s redfined on line %d\n",
				yykeytostr(o->o_type), yylineNum);
			if (o->o_str != NULL)
				free(o->o_str);
			olist = o->o_next;
			free(o);
			continue;
		}

		a->ac_mflag |= macflags[i][1];

		switch (o->o_type)
		{
		case IPM_DIRECTION :
			a->ac_direction = o->o_num;
			break;
		case IPM_DSTIP :
			a->ac_dip = o->o_ip.s_addr;
			a->ac_dmsk = htonl(0xffffffff << (32 - o->o_num));
			break;
		case IPM_DSTPORT :
			a->ac_dport = htons(o->o_num);
			break;
		case IPM_EXECUTE :
			a->ac_exec = o->o_str;
			c = *o->o_str;
			if (c== '"'|| c == '\'') {
				if (o->o_str[strlen(o->o_str) - 1] == c) {
					a->ac_run = strdup(o->o_str + 1);
					a->ac_run[strlen(a->ac_run) - 1] ='\0';
				} else
					a->ac_run = o->o_str;
			} else
				a->ac_run = o->o_str;
			o->o_str = NULL;
			break;
		case IPM_INTERFACE :
			a->ac_iface = o->o_str;
			o->o_str = NULL;
			break;
		case IPM_GROUP : 
			if (o->o_str != NULL)
				strncpy(a->ac_group, o->o_str, FR_GROUPLEN);
			else
				sprintf(a->ac_group, "%d", o->o_num);
			break;
		case IPM_LOGTAG :
			a->ac_logtag = o->o_num;
			break;
		case IPM_NATTAG :
			strncpy(a->ac_nattag, o->o_str, sizeof(a->ac_nattag));
			break;
		case IPM_PACKET :
			a->ac_packet = o->o_num;
			break;
		case IPM_PROTOCOL :
			a->ac_proto = o->o_num;
			break;
		case IPM_RULE :
			a->ac_rule = o->o_num;
			break;
		case IPM_RESULT :
			if (!strcasecmp(o->o_str, "pass"))
				a->ac_result = IPMR_PASS;
			else if (!strcasecmp(o->o_str, "block"))
				a->ac_result = IPMR_BLOCK;
			else if (!strcasecmp(o->o_str, "nomatch"))
				a->ac_result = IPMR_NOMATCH;
			else if (!strcasecmp(o->o_str, "log"))
				a->ac_result = IPMR_LOG;
			break;
		case IPM_SECOND :
			a->ac_second = o->o_num;
			break;
		case IPM_SRCIP :
			a->ac_sip = o->o_ip.s_addr;
			a->ac_smsk = htonl(0xffffffff << (32 - o->o_num));
			break;
		case IPM_SRCPORT :
			a->ac_sport = htons(o->o_num);
			break;
		case IPM_SAVE :
			if (a->ac_savefile != NULL) {
				fprintf(stderr, "%s redfined on line %d\n",
					yykeytostr(o->o_type), yylineNum);
				break;
			}
			a->ac_savefile = strdup(o->o_str);
			a->ac_savefp = fopen(o->o_str, "a");
			a->ac_dflag |= o->o_num & IPMDO_SAVERAW;
			break;
		case IPM_SYSLOG :
			if (a->ac_syslog != 0) {
				fprintf(stderr, "%s redfined on line %d\n",
					yykeytostr(o->o_type), yylineNum);
				break;
			}
			a->ac_syslog = 1;
			break;
		case IPM_TYPE :
			a->ac_type = o->o_num;
			break;
		case IPM_WITH :
			break;
		default :
			break;
		}

		olist = o->o_next;
		if (o->o_str != NULL)
			free(o->o_str);
		free(o);
	}
	a->ac_next = alist;
	alist = a;
}


int check_action(buf, log, opts, lvl)
char *buf, *log;
int opts, lvl;
{
	ipmon_action_t *a;
	struct timeval tv;
	ipflog_t *ipf;
	tcphdr_t *tcp;
	iplog_t *ipl;
	int matched;
	u_long t1;
	ip_t *ip;

	matched = 0;
	ipl = (iplog_t *)buf;
	ipf = (ipflog_t *)(ipl +1);
	ip = (ip_t *)(ipf + 1);
	tcp = (tcphdr_t *)((char *)ip + (IP_HL(ip) << 2));

	for (a = alist; a != NULL; a = a->ac_next) {
		if ((a->ac_mflag & IPMAC_DIRECTION) != 0) {
			if (a->ac_direction == IPM_IN) {
				if ((ipf->fl_flags & FR_INQUE) == 0)
					continue;
			} else if (a->ac_direction == IPM_OUT) {
				if ((ipf->fl_flags & FR_OUTQUE) == 0)
					continue;
			}
		}

		if ((a->ac_type != 0) && (a->ac_type != ipl->ipl_magic))
			continue;

		if ((a->ac_mflag & IPMAC_EVERY) != 0) {
			gettimeofday(&tv, NULL);
			t1 = tv.tv_sec - a->ac_lastsec;
			if (tv.tv_usec <= a->ac_lastusec)
				t1--;
			if (a->ac_second != 0) {
				if (t1 < a->ac_second)
					continue;
				a->ac_lastsec = tv.tv_sec;
				a->ac_lastusec = tv.tv_usec;
			}

			if (a->ac_packet != 0) {
				if (a->ac_pktcnt == 0)
					a->ac_pktcnt++;
				else if (a->ac_pktcnt == a->ac_packet) {
					a->ac_pktcnt = 0;
					continue;
				} else {
					a->ac_pktcnt++;
					continue;
				}
			}
		}

		if ((a->ac_mflag & IPMAC_DSTIP) != 0) {
			if ((ip->ip_dst.s_addr & a->ac_dmsk) != a->ac_dip)
				continue;
		}

		if ((a->ac_mflag & IPMAC_DSTPORT) != 0) {
			if (ip->ip_p != IPPROTO_UDP && ip->ip_p != IPPROTO_TCP)
				continue;
			if (tcp->th_dport != a->ac_dport)
				continue;
		}

		if ((a->ac_mflag & IPMAC_GROUP) != 0) {
			if (strncmp(a->ac_group, ipf->fl_group,
				    FR_GROUPLEN) != 0)
				continue;
		}

		if ((a->ac_mflag & IPMAC_INTERFACE) != 0) {
			if (strcmp(a->ac_iface, ipf->fl_ifname))
				continue;
		}

		if ((a->ac_mflag & IPMAC_PROTOCOL) != 0) {
			if (a->ac_proto != ip->ip_p)
				continue;
		}

		if ((a->ac_mflag & IPMAC_RESULT) != 0) {
			if ((ipf->fl_flags & FF_LOGNOMATCH) != 0) {
				if (a->ac_result != IPMR_NOMATCH)
					continue;
			} else if (FR_ISPASS(ipf->fl_flags)) {
				if (a->ac_result != IPMR_PASS)
					continue;
			} else if (FR_ISBLOCK(ipf->fl_flags)) {
				if (a->ac_result != IPMR_BLOCK)
					continue;
			} else {	/* Log only */
				if (a->ac_result != IPMR_LOG)
					continue;
			}
		}

		if ((a->ac_mflag & IPMAC_RULE) != 0) {
			if (a->ac_rule != ipf->fl_rule)
				continue;
		}

		if ((a->ac_mflag & IPMAC_SRCIP) != 0) {
			if ((ip->ip_src.s_addr & a->ac_smsk) != a->ac_sip)
				continue;
		}

		if ((a->ac_mflag & IPMAC_SRCPORT) != 0) {
			if (ip->ip_p != IPPROTO_UDP && ip->ip_p != IPPROTO_TCP)
				continue;
			if (tcp->th_sport != a->ac_sport)
				continue;
		}

		if ((a->ac_mflag & IPMAC_LOGTAG) != 0) {
			if (a->ac_logtag != ipf->fl_logtag)
				continue;
		}

		if ((a->ac_mflag & IPMAC_NATTAG) != 0) {
			if (strncmp(a->ac_nattag, ipf->fl_nattag.ipt_tag,
				    IPFTAG_LEN) != 0)
				continue;
		}

		matched = 1;

		/*
		 * It matched so now execute the command
		 */
		if (a->ac_syslog != 0) {
			syslog(lvl, "%s", log);
		}

		if (a->ac_savefp != NULL) {
			if (a->ac_dflag & IPMDO_SAVERAW)
				fwrite(ipl, 1, ipl->ipl_dsize, a->ac_savefp);
			else
				fputs(log, a->ac_savefp);
		}

		if (a->ac_exec != NULL) {
			switch (fork())
			{
			case 0 :
			{
				FILE *pi;

				pi = popen(a->ac_run, "w");
				if (pi != NULL) {
					fprintf(pi, "%s\n", log);
					if ((opts & OPT_HEXHDR) != 0) {
						dumphex(pi, 0, buf,
							sizeof(*ipl) +
							sizeof(*ipf));
					}
					if ((opts & OPT_HEXBODY) != 0) {
						dumphex(pi, 0, (char *)ip,
							ipf->fl_hlen +
							ipf->fl_plen);
					}
					pclose(pi);
				}
				exit(1);
			}
			case -1 :
				break;
			default :
				break;
			}
		}
	}

	return matched;
}


static void free_action(a)
ipmon_action_t *a;
{
	if (a->ac_savefile != NULL) {
		free(a->ac_savefile);
		a->ac_savefile = NULL;
	}
	if (a->ac_savefp != NULL) {
		fclose(a->ac_savefp);
		a->ac_savefp = NULL;
	}
	if (a->ac_exec != NULL) {
		free(a->ac_exec);
		if (a->ac_run == a->ac_exec)
			a->ac_run = NULL;
		a->ac_exec = NULL;
	}
	if (a->ac_run != NULL) {
		free(a->ac_run);
		a->ac_run = NULL;
	}
	if (a->ac_iface != NULL) {
		free(a->ac_iface);
		a->ac_iface = NULL;
	}
	a->ac_next = NULL;
	free(a);
}


int load_config(file)
char *file;
{
	ipmon_action_t *a;
	FILE *fp;
	char *s;

	s = getenv("YYDEBUG");
	if (s != NULL)
		yydebug = atoi(s);
	else
		yydebug = 0;

	while ((a = alist) != NULL) {
		alist = a->ac_next;
		free_action(a);
	}

	yylineNum = 1;

	(void) yysettab(yywords);

	fp = fopen(file, "r");
	if (!fp) {
		perror("load_config:fopen:");
		return -1;
	}
	yyin = fp;
	while (!feof(fp))
		yyparse();
	fclose(fp);
	return 0;
}
