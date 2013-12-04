/* ntp_config.c
 *
 * This file contains the ntpd configuration code.
 *
 * Written By:	Sachin Kamboj
 *		University of Delaware
 *		Newark, DE 19711
 * Some parts borrowed from the older ntp_config.c
 * Copyright (c) 2006
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef HAVE_NETINFO
# include <netinfo/ni.h>
#endif

#include "ntp.h"
#include "ntpd.h"
#include "ntp_io.h"
#include "ntp_unixtime.h"
#include "ntp_refclock.h"
#include "ntp_filegen.h"
#include "ntp_stdlib.h"
#include "ntp_assert.h"
#include "ntpd-opts.h"
/*
 * Sim header. Currently unconditionally included
 * PDMXXX This needs to be a conditional include
 */
#include "ntpsim.h"

#include <ntp_random.h>
#include "ntp_intres.h"
#include <isc/net.h>
#include <isc/result.h>

#include <stdio.h>
#include <ctype.h>
#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>
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
HANDLE ResolverEventHandle;
#else
int resolver_pipe_fd[2];  /* used to let the resolver process alert the parent process */
#endif /* SYS_WINNT */

/*
 * [Bug 467]: Some linux headers collide with CONFIG_PHONE and CONFIG_KEYS
 * so #include these later.
 */

#include "ntp_config.h"
#include "ntp_cmdargs.h"

#include "ntp_scanner.h"
#include "ntp_parser.h"
#include "ntp_data_structures.h"


/*
 * "logconfig" building blocks
 */
struct masks {
	const char * const	name;
	const u_int32		mask;
};

static struct masks logcfg_class[] = {
	{ "clock",	NLOG_OCLOCK },
	{ "peer",	NLOG_OPEER },
	{ "sync",	NLOG_OSYNC },
	{ "sys",	NLOG_OSYS },
	{ NULL,		0 }
};

/* logcfg_noclass_items[] masks are complete and must not be shifted */
static struct masks logcfg_noclass_items[] = {
	{ "allall",		NLOG_SYSMASK | NLOG_PEERMASK | NLOG_CLOCKMASK | NLOG_SYNCMASK },
	{ "allinfo",		NLOG_SYSINFO | NLOG_PEERINFO | NLOG_CLOCKINFO | NLOG_SYNCINFO },
	{ "allevents",		NLOG_SYSEVENT | NLOG_PEEREVENT | NLOG_CLOCKEVENT | NLOG_SYNCEVENT },
	{ "allstatus",		NLOG_SYSSTATUS | NLOG_PEERSTATUS | NLOG_CLOCKSTATUS | NLOG_SYNCSTATUS },
	{ "allstatistics",	NLOG_SYSSTATIST | NLOG_PEERSTATIST | NLOG_CLOCKSTATIST | NLOG_SYNCSTATIST },
	/* the remainder are misspellings of clockall, peerall, sysall, and syncall. */
	{ "allclock",		(NLOG_INFO | NLOG_STATIST | NLOG_EVENT | NLOG_STATUS) << NLOG_OCLOCK },
	{ "allpeer",		(NLOG_INFO | NLOG_STATIST | NLOG_EVENT | NLOG_STATUS) << NLOG_OPEER },
	{ "allsys",		(NLOG_INFO | NLOG_STATIST | NLOG_EVENT | NLOG_STATUS) << NLOG_OSYS },
	{ "allsync",		(NLOG_INFO | NLOG_STATIST | NLOG_EVENT | NLOG_STATUS) << NLOG_OSYNC },
	{ NULL,			0 }
};

/* logcfg_class_items[] masks are shiftable by NLOG_O* counts */
static struct masks logcfg_class_items[] = {
	{ "all",		NLOG_INFO | NLOG_EVENT | NLOG_STATUS | NLOG_STATIST },
	{ "info",		NLOG_INFO },
	{ "events",		NLOG_EVENT },
	{ "status",		NLOG_STATUS },
	{ "statistics",		NLOG_STATIST },
	{ NULL,			0 }
};

/* Limits */
#define MAXPHONE	10	/* maximum number of phone strings */
#define MAXPPS		20	/* maximum length of PPS device string */

/*
 * Miscellaneous macros
 */
#define STRSAME(s1, s2)	(*(s1) == *(s2) && strcmp((s1), (s2)) == 0)
#define ISEOL(c)	((c) == '#' || (c) == '\n' || (c) == '\0')
#define ISSPACE(c)	((c) == ' ' || (c) == '\t')
#define STREQ(a, b)	(*(a) == *(b) && strcmp((a), (b)) == 0)

/*
 * File descriptor used by the resolver save routines, and temporary file
 * name.
 */
int call_resolver = 1;		/* ntp-genkeys sets this to 0, for example */
#ifndef SYS_WINNT
static char res_file[20];	/* enough for /tmp/ntpXXXXXX\0 */
#define RES_TEMPFILE	"/tmp/ntpXXXXXX"
#else
static char res_file[MAX_PATH];
#endif /* SYS_WINNT */

/*
 * Definitions of things either imported from or exported to outside
 */
extern int yydebug;			/* ntp_parser.c (.y) */
int curr_include_level;			/* The current include level */
struct FILE_INFO *fp[MAXINCLUDELEVEL+1];
FILE *res_fp;
struct config_tree cfgt;		/* Parser output stored here */
struct config_tree *cfg_tree_history = NULL;	/* History of configs */
char	*sys_phone[MAXPHONE] = {NULL};	/* ACTS phone numbers */
char	default_keysdir[] = NTP_KEYSDIR;
char	*keysdir = default_keysdir;	/* crypto keys directory */
char *	saveconfigdir;
#if defined(HAVE_SCHED_SETSCHEDULER)
int	config_priority_override = 0;
int	config_priority;
#endif

const char *config_file;
char default_ntp_signd_socket[] =
#ifdef NTP_SIGND_PATH
					NTP_SIGND_PATH;
#else
					"";
#endif
char *ntp_signd_socket = default_ntp_signd_socket;
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

struct REMOTE_CONFIG_INFO remote_config;  /* Remote configuration buffer and
					     pointer info */
int input_from_file = 1;     /* A boolean flag, which when set, indicates that
			        the input is to be taken from the configuration
			        file, instead of the remote-configuration buffer
			     */

int old_config_style = 1;    /* A boolean flag, which when set,
			      * indicates that the old configuration
			      * format with a newline at the end of
			      * every command is being used
			      */
int	cryptosw;		/* crypto command called */

extern int sys_maxclock;
extern char *stats_drift_file;	/* name of the driftfile */
extern char *leapseconds_file_name; /*name of the leapseconds file */
#ifdef HAVE_IPTOS_SUPPORT
extern unsigned int qos;				/* QoS setting */
#endif /* HAVE_IPTOS_SUPPORT */

#ifdef BC_LIST_FRAMEWORK_NOT_YET_USED
/*
 * backwards compatibility flags
 */
bc_entry bc_list[] = {
	{ T_Bc_bugXXXX,		1	}	/* default enabled */
};

/*
 * declare an int pointer for each flag for quick testing without
 * walking bc_list.  If the pointer is consumed by libntp rather
 * than ntpd, declare it in a libntp source file pointing to storage
 * initialized with the appropriate value for other libntp clients, and
 * redirect it to point into bc_list during ntpd startup.
 */
int *p_bcXXXX_enabled = &bc_list[0].enabled;
#endif

/* FUNCTION PROTOTYPES */

static void apply_enable_disable(queue *q, int enable);
static void init_syntax_tree(struct config_tree *);

#ifdef FREE_CFG_T
static void free_auth_node(struct config_tree *);

static void free_config_other_modes(struct config_tree *);
static void free_config_auth(struct config_tree *);
static void free_config_tos(struct config_tree *);
static void free_config_monitor(struct config_tree *);
static void free_config_access(struct config_tree *);
static void free_config_tinker(struct config_tree *);
static void free_config_system_opts(struct config_tree *);
static void free_config_logconfig(struct config_tree *);
static void free_config_phone(struct config_tree *);
static void free_config_qos(struct config_tree *);
static void free_config_setvar(struct config_tree *);
static void free_config_ttl(struct config_tree *);
static void free_config_trap(struct config_tree *);
static void free_config_fudge(struct config_tree *);
static void free_config_vars(struct config_tree *);
static void free_config_peers(struct config_tree *);
static void free_config_unpeers(struct config_tree *);
static void free_config_nic_rules(struct config_tree *);
#ifdef SIM
static void free_config_sim(struct config_tree *);
#endif

       void free_all_config_trees(void);	/* atexit() */
static void free_config_tree(struct config_tree *ptree);
#endif	/* FREE_CFG_T */

double *create_dval(double val);
void destroy_restrict_node(struct restrict_node *my_node);
static int is_sane_resolved_address(sockaddr_u *peeraddr, int hmode);
static int get_correct_host_mode(int hmode);
static void save_and_apply_config_tree(void);
void getconfig(int argc,char *argv[]);
#if !defined(SIM)
static sockaddr_u *get_next_address(struct address_node *addr);
#endif

static void config_other_modes(struct config_tree *);
static void config_auth(struct config_tree *);
static void config_tos(struct config_tree *);
static void config_monitor(struct config_tree *);
static void config_access(struct config_tree *);
static void config_tinker(struct config_tree *);
static void config_system_opts(struct config_tree *);
static void config_logconfig(struct config_tree *);
static void config_phone(struct config_tree *);
static void config_qos(struct config_tree *);
static void config_setvar(struct config_tree *);
static void config_ttl(struct config_tree *);
static void config_trap(struct config_tree *);
static void config_fudge(struct config_tree *);
static void config_vars(struct config_tree *);
static void config_peers(struct config_tree *);
static void config_unpeers(struct config_tree *);
static void config_nic_rules(struct config_tree *);

#ifdef SIM
static void config_sim(struct config_tree *);
static void config_ntpdsim(struct config_tree *);
#else
static void config_ntpd(struct config_tree *);
#endif

enum gnn_type {
	t_UNK,		/* Unknown */
	t_REF,		/* Refclock */
	t_MSK		/* Network Mask */
};

#define DESTROY_QUEUE(q)		\
do {					\
	if (q) {			\
		destroy_queue(q);	\
		(q) = NULL;		\
	}				\
} while (0)

void ntpd_set_tod_using(const char *);
static u_int32 get_pfxmatch(const char **, struct masks *);
static u_int32 get_match(const char *, struct masks *);
static u_int32 get_logmask(const char *);
static int getnetnum(const char *num,sockaddr_u *addr, int complain,
		     enum gnn_type a_type);
static int get_multiple_netnums(const char *num, sockaddr_u *addr,
				struct addrinfo **res, int complain,
				enum gnn_type a_type);
static void save_resolve(char *name, int no_needed, int type,
			 int mode, int version, int minpoll, int maxpoll,
			 u_int flags, int ttl, keyid_t keyid, u_char *keystr);
static void abort_resolve(void);
static void do_resolve_internal(void);



/* FUNCTIONS FOR INITIALIZATION
 * ----------------------------
 */

#ifdef FREE_CFG_T
static void
free_auth_node(
	struct config_tree *ptree
	)
{
	if (ptree->auth.keys) {
		free(ptree->auth.keys);
		ptree->auth.keys = NULL;
	}

	if (ptree->auth.keysdir) {
		free(ptree->auth.keysdir);
		ptree->auth.keysdir = NULL;
	}

	if (ptree->auth.ntp_signd_socket) {
		free(ptree->auth.ntp_signd_socket);
		ptree->auth.ntp_signd_socket = NULL;
	}
}
#endif /* DEBUG */


static void
init_syntax_tree(
	struct config_tree *ptree
	)
{
	memset(ptree, 0, sizeof(*ptree));

	ptree->peers = create_queue();
	ptree->unpeers = create_queue();
	ptree->orphan_cmds = create_queue();
	ptree->manycastserver = create_queue();
	ptree->multicastclient = create_queue();
	ptree->stats_list = create_queue();
	ptree->filegen_opts = create_queue();
	ptree->discard_opts = create_queue();
	ptree->restrict_opts = create_queue();
	ptree->enable_opts = create_queue();
	ptree->disable_opts = create_queue();
	ptree->tinker = create_queue();
	ptree->fudge = create_queue();
	ptree->logconfig = create_queue();
	ptree->phone = create_queue();
	ptree->qos = create_queue();
	ptree->setvar = create_queue();
	ptree->ttl = create_queue();
	ptree->trap = create_queue();
	ptree->vars = create_queue();
	ptree->nic_rules = create_queue();
	ptree->auth.crypto_cmd_list = create_queue();
	ptree->auth.trusted_key_list = create_queue();
}


#ifdef FREE_CFG_T
void
free_all_config_trees(void)
{
	struct config_tree *ptree;
	struct config_tree *pnext;

	ptree = cfg_tree_history;

	while (ptree != NULL) {
		pnext = ptree->link;
		free_config_tree(ptree);
		ptree = pnext;
	}
}


static void
free_config_tree(
	struct config_tree *ptree
	)
{
#if defined(_MSC_VER) && defined (_DEBUG)
	_CrtCheckMemory();
#endif

	if (ptree->source.value.s != NULL)
		free(ptree->source.value.s);

	free_config_other_modes(ptree);
	free_config_auth(ptree);
	free_config_tos(ptree);
	free_config_monitor(ptree);
	free_config_access(ptree);
	free_config_tinker(ptree);
	free_config_system_opts(ptree);
	free_config_logconfig(ptree);
	free_config_phone(ptree);
	free_config_qos(ptree);
	free_config_setvar(ptree);
	free_config_ttl(ptree);
	free_config_trap(ptree);
	free_config_fudge(ptree);
	free_config_vars(ptree);
	free_config_peers(ptree);
	free_config_unpeers(ptree);
	free_config_nic_rules(ptree);
#ifdef SIM
	free_config_sim(ptree);
#endif
	/*
	 * Most of these DESTROY_QUEUE()s are handled already by the
	 * free_config_*() routines above but it's safe to use twice.
	 * Please feel free to remove ones you verified are handled
	 * in a free_config_*() routine.
	 */
	DESTROY_QUEUE(ptree->peers);
	DESTROY_QUEUE(ptree->unpeers);
	DESTROY_QUEUE(ptree->orphan_cmds);
	DESTROY_QUEUE(ptree->manycastserver);
	DESTROY_QUEUE(ptree->multicastclient);
	DESTROY_QUEUE(ptree->stats_list);
	DESTROY_QUEUE(ptree->filegen_opts);
	DESTROY_QUEUE(ptree->discard_opts);
	DESTROY_QUEUE(ptree->restrict_opts);
	DESTROY_QUEUE(ptree->enable_opts);
	DESTROY_QUEUE(ptree->disable_opts);
	DESTROY_QUEUE(ptree->tinker);
	DESTROY_QUEUE(ptree->fudge);
	DESTROY_QUEUE(ptree->logconfig);
	DESTROY_QUEUE(ptree->phone);
	DESTROY_QUEUE(ptree->qos);
	DESTROY_QUEUE(ptree->setvar);
	DESTROY_QUEUE(ptree->ttl);
	DESTROY_QUEUE(ptree->trap);
	DESTROY_QUEUE(ptree->vars);

	free_auth_node(ptree);

	free(ptree);

#if defined(_MSC_VER) && defined (_DEBUG)
	_CrtCheckMemory();
#endif
}
#endif /* FREE_CFG_T */


#ifdef SAVECONFIG
/* Dump all trees */
int
dump_all_config_trees(
	FILE *df,
	int comment
	) 
{
	struct config_tree *cfg_ptr = cfg_tree_history;
	int return_value = 0;

	for (cfg_ptr = cfg_tree_history;
	     cfg_ptr != NULL; 
	     cfg_ptr = cfg_ptr->link) 
		return_value |= dump_config_tree(cfg_ptr, df, comment);

	return return_value;
}


/* The config dumper */
int
dump_config_tree(
	struct config_tree *ptree,
	FILE *df,
	int comment
	)
{
	struct peer_node *peer = NULL;
	struct unpeer_node *unpeers = NULL;
	struct attr_val *atrv = NULL;
	struct address_node *addr = NULL;
	struct address_node *peer_addr;
	struct address_node *fudge_addr;
	struct filegen_node *fgen_node = NULL;
	struct restrict_node *rest_node = NULL;
	struct addr_opts_node *addr_opts = NULL;
	struct setvar_node *setv_node = NULL;
	nic_rule_node *rule_node;

	char **pstr = NULL;
	char *s1;
	char *s2;
	int *intp = NULL;
	void *fudge_ptr;
	void *list_ptr = NULL;
	void *options = NULL;
	void *opt_ptr = NULL;
	int *flags = NULL;
	void *opts = NULL;
	char timestamp[80];
	int enable;

	DPRINTF(1, ("dump_config_tree(%p)\n", ptree));

	if (comment) {
		if (!strftime(timestamp, sizeof(timestamp),
			      "%Y-%m-%d %H:%M:%S",
			      localtime(&ptree->timestamp)))
			timestamp[0] = '\0';

		fprintf(df, "# %s %s %s\n",
			timestamp,
			(CONF_SOURCE_NTPQ == ptree->source.attr)
			    ? "ntpq remote config from"
			    : "startup configuration file",
			ptree->source.value.s);
	}

	/* For options I didn't find documentation I'll just output its name and the cor. value */
	list_ptr = queue_head(ptree->vars);
	for(;	list_ptr != NULL;
		list_ptr = next_node(list_ptr)) {

		atrv = (struct attr_val *) list_ptr;

		switch (atrv->attr) {

		default:
			fprintf(df, "\n# dump error:\n"
				"# unknown vars token %s\n",
				token_name(atrv->attr));
			break;

		/* doubles */
		case T_Broadcastdelay:
		case T_Tick:
		case T_WanderThreshold:
			fprintf(df, "%s %g\n",
				keyword(atrv->attr),
				atrv->value.d);
			break;
			
		/* ints */
		case T_Calldelay:
#ifdef OPENSSL
		case T_Automax:
#endif
			fprintf(df, "%s %d\n",
				keyword(atrv->attr),
				atrv->value.i);
			break;

		/* strings */
		case T_Driftfile:
		case T_Leapfile:
		case T_Logfile:
		case T_Pidfile:
		case T_Saveconfigdir:
			fprintf(df, "%s \"%s\"\n",
				keyword(atrv->attr),
				atrv->value.s);
			break;
		}
	}

	list_ptr = queue_head(ptree->logconfig);
	if (list_ptr != NULL) {
		
		fprintf(df, "logconfig");

		for(;	list_ptr != NULL;
			list_ptr = next_node(list_ptr)) {

			atrv = list_ptr;
			fprintf(df, " %c%s", atrv->attr, atrv->value.s);
		}
		fprintf(df, "\n");
	}

	if (ptree->stats_dir)
		fprintf(df, "statsdir \"%s\"\n", ptree->stats_dir);

	list_ptr = queue_head(ptree->stats_list);
	if (list_ptr != NULL) {

		fprintf(df, "statistics");
		for(; 	list_ptr != NULL;
			list_ptr = next_node(list_ptr)) {

			intp = list_ptr;
			
			fprintf(df, " %s", keyword(*intp));	
		}

		fprintf(df, "\n");
	}

	list_ptr = queue_head(ptree->filegen_opts);
	for(; 	list_ptr != NULL;
		list_ptr = next_node(list_ptr)) {

		fgen_node = list_ptr;
		opt_ptr = queue_head(fgen_node->options);

		if (opt_ptr != NULL)
			fprintf(df, "filegen %s", 
				keyword(fgen_node->filegen_token));

		for(;	opt_ptr != NULL;
			opt_ptr = next_node(opt_ptr)) {
			
			atrv = opt_ptr;

			switch (atrv->attr) {

			default:
				fprintf(df, "\n# dump error:\n"
					"# unknown filegen option token %s\n"
					"filegen %s",
					token_name(atrv->attr),
					keyword(fgen_node->filegen_token));
				break;

			case T_File:
				fprintf(df, " file %s",
					atrv->value.s);
				break;

			case T_Type:
				fprintf(df, " type %s",
					keyword(atrv->value.i));
				break;

			case T_Flag:
				fprintf(df, " %s",
					keyword(atrv->value.i));
				break;
			}

		}

		fprintf(df, "\n");
	}

	list_ptr = queue_head(ptree->auth.crypto_cmd_list);
	if (list_ptr != NULL) {
		fprintf(df, "crypto");

		for (;	list_ptr != NULL;
			list_ptr = next_node(list_ptr)) {

			atrv = list_ptr;
			fprintf(df, " %s %s", keyword(atrv->attr),
				atrv->value.s);
		}
		fprintf(df, "\n");
	}

	if (ptree->auth.revoke != 0)
		fprintf(df, "revoke %d\n", ptree->auth.revoke);

	if (NULL != ptree->auth.keysdir)
		fprintf(df, "keysdir \"%s\"\n", ptree->auth.keysdir);

	if (NULL != ptree->auth.keys)
		fprintf(df, "keys \"%s\"\n", ptree->auth.keys);

	atrv = queue_head(ptree->auth.trusted_key_list);
	if (atrv != NULL) {
		fprintf(df, "trustedkey");
		do {
			if ('i' == atrv->attr)
				fprintf(df, " %d", atrv->value.i);
			else if ('-' == atrv->attr)
				fprintf(df, " (%u ... %u)",
					atrv->value.u >> 16,
					atrv->value.u & 0xffff);
			else
				fprintf(df, "\n# dump error:\n"
					"# unknown trustedkey attr %d\n"
					"trustedkey", atrv->attr);
		} while (NULL != (atrv = next_node(atrv)));
		fprintf(df, "\n");
	}

	if (ptree->auth.control_key)
		fprintf(df, "controlkey %d\n", ptree->auth.control_key);

	if (ptree->auth.request_key)
		fprintf(df, "requestkey %d\n", ptree->auth.request_key);

	/* dump enable list, then disable list */
	for (enable = 1; enable >= 0; enable--) {

		list_ptr = (enable)
			       ? queue_head(ptree->enable_opts)
			       : queue_head(ptree->disable_opts);

		if (list_ptr != NULL) {
			fprintf(df, (enable)
					? "enable"
					: "disable");

			for(;	list_ptr != NULL;
				list_ptr = next_node(list_ptr)) {

				atrv = (struct attr_val *) list_ptr;

				fprintf(df, " %s",
					keyword(atrv->value.i));
			}
			fprintf(df, "\n");
		}
	}

	list_ptr = queue_head(ptree->orphan_cmds);
	if (list_ptr != NULL)
		fprintf(df, "tos");

	for(; 	list_ptr != NULL;
		list_ptr = next_node(list_ptr)) {

		atrv = list_ptr;

		switch (atrv->attr) {

		default:
			fprintf(df, "\n# dump error:\n"
				"# unknown tos token %s\n"
				"tos", token_name(atrv->attr));
			break;

		/* ints */
		case T_Ceiling:
		case T_Floor:
		case T_Cohort:
		case T_Orphan:
		case T_Minclock:
		case T_Maxclock:
		case T_Minsane:
		case T_Beacon:
			fprintf(df, " %s %d", keyword(atrv->attr),
				(int)atrv->value.d);
			break;

		/* doubles */
		case T_Mindist:
		case T_Maxdist:
			fprintf(df, " %s %g", keyword(atrv->attr),
				atrv->value.d);
			break;
		}
	}
	if (queue_head(ptree->orphan_cmds) != NULL)
		fprintf(df, "\n");

	list_ptr = queue_head(ptree->tinker);
	if (list_ptr != NULL) {

		fprintf(df, "tinker");

		for(;	list_ptr != NULL;
			list_ptr = next_node(list_ptr)) {

			atrv = list_ptr;
			fprintf(df, " %s %g", keyword(atrv->attr),
				atrv->value.d);
		}

		fprintf(df, "\n");
	}

	if (ptree->broadcastclient)
		fprintf(df, "broadcastclient\n");

	list_ptr = queue_head(ptree->peers);
	for (; 	list_ptr != NULL;
	 	list_ptr = next_node(list_ptr)) {

		peer = list_ptr; 
		addr = peer->addr;
		fprintf(df, "%s", keyword(peer->host_mode));

		switch (addr->type) {

		default:
			fprintf(df, "# dump error:\n"
				"# unknown peer family %d for:\n"
				"peer", addr->type);
			break;

		case AF_UNSPEC:
			break;

		case AF_INET:
			fprintf(df, " -4");
			break;

		case AF_INET6:
			fprintf(df, " -6");
			break;
		}
		fprintf(df, " %s", addr->address);
		
		if (peer->minpoll != 0)
			fprintf(df, " minpoll %d", peer->minpoll);

		if (peer->maxpoll != 0)
			fprintf(df, " maxpoll %d", peer->maxpoll);

		if (peer->ttl != 0) {
			if (strlen(addr->address) > 8
			    && !memcmp(addr->address, "127.127.", 8))
				fprintf(df, " mode %d", peer->ttl);
			else
				fprintf(df, " ttl %d", peer->ttl);
		}

		if (peer->peerversion != NTP_VERSION)
			fprintf(df, " version %d", peer->peerversion);

		if (peer->peerkey != 0)
			fprintf(df, " key %d", peer->peerkey);

		if (peer->bias != 0.)
			fprintf(df, " bias %g", peer->bias);

		for (atrv = queue_head(peer->peerflags);
		     atrv != NULL;
		     atrv = next_node(atrv)) {

			NTP_INSIST(T_Flag == atrv->attr);
			NTP_INSIST(T_Integer == atrv->type);

			fprintf(df, " %s", keyword(atrv->value.i));
		}

		fprintf(df, "\n");

		fudge_ptr = queue_head(ptree->fudge);
		for(; 	fudge_ptr != NULL;
			fudge_ptr = next_node(fudge_ptr)) {


			addr_opts = (struct addr_opts_node *) fudge_ptr; 
			peer_addr = peer->addr;
			fudge_addr = addr_opts->addr;

			s1 = peer_addr->address;
			s2 = fudge_addr->address;

			if (!strcmp(s1, s2)) {

				fprintf(df, "fudge %s", addr_opts->addr->address);
	
				opts = queue_head(addr_opts->options);

				for(; opts != NULL; opts = next_node(opts)) {
					atrv = (struct attr_val *) opts; 
				
					switch (atrv->attr) {

					default:
						fprintf(df, "\n# dump error:\n"
							"# unknown fudge option %s\n"
							"fudge %s",
							token_name(atrv->attr),
							addr_opts->addr->address);
						break;

					/* doubles */
					case T_Time1:
					case T_Time2:
						fprintf(df, " %s %g",
							keyword(atrv->attr),
							atrv->value.d);
						break;
		
					/* ints */
					case T_Stratum:
					case T_Flag1:
					case T_Flag2:
					case T_Flag3:
					case T_Flag4:
						fprintf(df, " %s %d",
							keyword(atrv->attr),
							atrv->value.i);
						break;

					/* strings */
					case T_Refid:
						fprintf(df, " %s %s",
							keyword(atrv->attr),
							atrv->value.s);
						break;
					}
				}
				fprintf(df, "\n");
			}
		}
	}

	list_ptr = queue_head(ptree->manycastserver);
	if (list_ptr != NULL) {
		addr = list_ptr;
		fprintf(df, "manycastserver %s", addr->address);
		for (addr = next_node(addr);
		     addr != NULL;
		     addr = next_node(addr))
			fprintf(df, " %s", addr->address);
		fprintf(df, "\n");
	}

	list_ptr = queue_head(ptree->multicastclient);
	if (list_ptr != NULL) {
		addr = list_ptr;
		fprintf(df, "multicastclient %s", addr->address);
		for (addr = next_node(addr);
		     addr != NULL;
		     addr = next_node(addr))
			fprintf(df, " %s", addr->address);
		fprintf(df, "\n");
	}

	list_ptr = queue_head(ptree->unpeers);
	for (; 	list_ptr != NULL;
		list_ptr = next_node(list_ptr)) {
		
		unpeers = (struct unpeer_node *) list_ptr;
		
		fprintf(df, "unpeer %s\n", (unpeers->addr)->address);
	}

	list_ptr = queue_head(ptree->discard_opts);
	if (list_ptr != NULL) {

		fprintf(df, "discard");

		for(;	list_ptr != NULL;
			list_ptr = next_node(list_ptr)) {

			atrv = list_ptr;
			fprintf(df, " %s %d", keyword(atrv->attr),
				atrv->value.i);
		}
		fprintf(df, "\n");
	}

	list_ptr = queue_head(ptree->restrict_opts);
	for (;	list_ptr != NULL; 
		list_ptr = next_node(list_ptr)) {

		rest_node = list_ptr;
		if (NULL == rest_node->addr)
			s1 = "default";
		else
			s1 = rest_node->addr->address;

		fprintf(df, "restrict %s", s1);

		if (rest_node->mask != NULL)
			fprintf(df, " mask %s",
				rest_node->mask->address);

		flags = queue_head(rest_node->flags);
		for (; 	flags != NULL; flags = next_node(flags))
			fprintf(df, " %s", keyword(*flags));

		fprintf(df, "\n");
	}

	list_ptr = queue_head(ptree->nic_rules);
	for (;	list_ptr != NULL;
		list_ptr = next_node(list_ptr)) {

		rule_node = list_ptr;
		fprintf(df, "interface %s %s\n",
			keyword(rule_node->action),
			(rule_node->match_class)
			    ? keyword(rule_node->match_class)
			    : rule_node->if_name);
	}

	list_ptr = queue_head(ptree->phone);
	if (list_ptr != NULL) {

		fprintf(df, "phone");

		for(; 	list_ptr != NULL;
			list_ptr = next_node(list_ptr)) {

			pstr = list_ptr;
			fprintf(df, " %s", *pstr);
		}

		fprintf(df, "\n");
	}

	list_ptr = queue_head(ptree->qos);
	if (list_ptr != NULL) {
		
		fprintf(df, "qos");

		for(;	list_ptr != NULL;
			list_ptr = next_node(list_ptr)) {

			atrv = list_ptr;
			fprintf(df, " %s", atrv->value.s);
		}

		fprintf(df, "\n");
	}

	list_ptr = queue_head(ptree->setvar);
	for(;	list_ptr != NULL;
		list_ptr = next_node(list_ptr)) {

		setv_node = list_ptr;
		s1 = quote_if_needed(setv_node->var);
		s2 = quote_if_needed(setv_node->val);
		fprintf(df, "setvar %s = %s", s1, s2);
		free(s1);
		free(s2);

		if (setv_node->isdefault)
			fprintf(df, " default");

		fprintf(df, "\n");
	}


	list_ptr = queue_head(ptree->ttl);
	if (list_ptr != NULL) {

		fprintf(df, "ttl");

		for(; 	list_ptr != NULL;
			list_ptr = next_node(list_ptr)) {

			intp = list_ptr;
			fprintf(df, " %d", *intp);
		}
		
		fprintf(df, "\n");
	}
	
	list_ptr = queue_head(ptree->trap);
	for(;	list_ptr != NULL;
		list_ptr = next_node(list_ptr)) {

		addr_opts = list_ptr;
		addr = addr_opts->addr;

		fprintf(df, "trap %s", addr->address);

		options = queue_head(addr_opts->options);

		for(; 	options != NULL; 
			options = next_node(options)) {

			atrv = options;

			switch (atrv->attr) {

			default:
				fprintf(df, "\n# dump error:\n"
					"# unknown trap token %d\n"
					"trap %s", atrv->attr,
					addr->address);
				break;

			case T_Port:
				fprintf(df, " port %d", atrv->value.i);
				break;

			case T_Interface:
				addr = (struct address_node *) atrv->value.p;
				fprintf(df, " interface %s", addr->address);
				break;
			}
		}

		fprintf(df, "\n");
	}

	return 0;
}
#endif	/* SAVECONFIG */
	

/* FUNCTIONS FOR CREATING NODES ON THE SYNTAX TREE
 * -----------------------------------------------
 */

queue *
enqueue_in_new_queue(
	void *my_node
	)
{
	queue *my_queue = create_queue();

	enqueue(my_queue, my_node);
	return my_queue;
}

struct attr_val *
create_attr_dval(
	int attr,
	double value
	)
{
	struct attr_val *my_val;

	my_val = get_node(sizeof *my_val);
	my_val->attr = attr;
	my_val->value.d = value;
	my_val->type = T_Double;
	return my_val;
}

struct attr_val *
create_attr_ival(
	int attr,
	int value
	)
{
	struct attr_val *my_val;

	my_val = get_node(sizeof *my_val);
	my_val->attr = attr;
	my_val->value.i = value;
	my_val->type = T_Integer;
	return my_val;
}

struct attr_val *
create_attr_shorts(
	int		attr,
	ntp_u_int16_t	val1,
	ntp_u_int16_t	val2
	)
{
	struct attr_val *my_val;

	my_val = get_node(sizeof *my_val);
	my_val->attr = attr;
	my_val->value.u = (val1 << 16) | val2;
	my_val->type = T_Integer;
	return my_val;
}

struct attr_val *
create_attr_sval(
	int attr,
	char *s
	)
{
	struct attr_val *my_val;

	my_val = get_node(sizeof *my_val);
	my_val->attr = attr;
	if (NULL == s)			/* free() hates NULL */
		s = estrdup("");
	my_val->value.s = s;
	my_val->type = T_String;
	return my_val;
}

struct attr_val *
create_attr_pval(
	int attr,
	void *p
	)
{
	struct attr_val *my_val;

	my_val = get_node(sizeof *my_val);
	my_val->attr = attr;
	my_val->value.p = p;
	my_val->type = T_Void;
	return my_val;
}

int *
create_ival(
	int val
	)
{
	int *p = get_node(sizeof *p);

	*p = val;
	return p;
}

double *
create_dval(
	double val
	)
{
	double *p = get_node(sizeof *p);

	*p = val;
	return p;
}

void **
create_pval(
	void *val
	)
{
	void **p = get_node(sizeof *p);

	*p = val;
	return p;
}

struct address_node *
create_address_node(
	char *addr,
	int type
	)
{
	struct address_node *my_node;

	NTP_REQUIRE(NULL != addr);
	
	my_node = get_node(sizeof *my_node);

	my_node->address = addr;
	my_node->type = type;

	return my_node;
}


void
destroy_address_node(
	struct address_node *my_node
	)
{
	NTP_REQUIRE(NULL != my_node);
	NTP_REQUIRE(NULL != my_node->address);

	free(my_node->address);
	free_node(my_node);
}


struct peer_node *
create_peer_node(
	int hmode,
	struct address_node *addr,
	queue *options
	)
{
	struct peer_node *my_node;
	struct attr_val *option;
	int freenode;
	int errflag = 0;

	my_node = get_node(sizeof(*my_node));

	/* Initialize node values to default */
	my_node->minpoll = 0;
	my_node->maxpoll = 0;
	my_node->ttl = 0;
	my_node->peerversion = NTP_VERSION;
	my_node->peerkey = 0;
	my_node->bias = 0;
	my_node->peerflags = create_queue();

	/* Now set the node to the read values */
	my_node->host_mode = hmode;
	my_node->addr = addr;

	/*
	 * the options list mixes items that will be saved in the
	 * peer_node as explicit members, such as minpoll, and
	 * those that are moved from the options queue intact
	 * to the peer_node's peerflags queue.  The options
	 * queue is consumed and destroyed here.
	 */

	while (options && NULL != (option = dequeue(options))) {

		freenode = 1;
		/* Check the kind of option being set */
		switch (option->attr) {

		case T_Flag:
			enqueue(my_node->peerflags, option); 
			freenode = 0;
			break;

		case T_Minpoll:
			if (option->value.i < NTP_MINPOLL) {
				msyslog(LOG_INFO,
					"minpoll: provided value (%d) is below minimum (%d)",
					option->value.i, NTP_MINPOLL);
				my_node->minpoll = NTP_MINPOLL;
			}
			else
				my_node->minpoll = option->value.i;
			break;

		case T_Maxpoll:
			if (option->value.i > NTP_MAXPOLL) {
				msyslog(LOG_INFO,
					"maxpoll: provided value (%d) is above maximum (%d)",
					option->value.i, NTP_MAXPOLL);
				my_node->maxpoll = NTP_MAXPOLL;
			}
			else
				my_node->maxpoll = option->value.i;
			break;

		case T_Ttl:
			if (my_node->ttl >= MAX_TTL) {
				msyslog(LOG_ERR, "ttl: invalid argument");
				errflag = 1;
			}
			else
				my_node->ttl = option->value.i;
			break;

		case T_Mode:
			my_node->ttl = option->value.i;
			break;

		case T_Key:
			my_node->peerkey = option->value.i;
			break;

		case T_Version:
			my_node->peerversion = option->value.i;
			break;

		case T_Bias:
			my_node->bias = option->value.d;
			break;

		default:
			msyslog(LOG_ERR, 
				"Unknown peer/server option token %s",
				token_name(option->attr));
			errflag = 1;
		}
		if (freenode)
			free_node(option);
	}
	DESTROY_QUEUE(options);

	/* Check if errors were reported. If yes, ignore the node */
	if (errflag) {
		free_node(my_node);
		my_node = NULL;
	}
	return my_node;
}


struct unpeer_node *
create_unpeer_node(
	struct address_node *addr
	)
{
	struct unpeer_node *	my_node;
	char *			pch;

	my_node = get_node(sizeof(*my_node));

	/*
	 * From the parser's perspective an association ID fits into
	 * its generic T_String definition of a name/address "address".
	 * We treat all valid 16-bit numbers as association IDs.
	 */
	pch = addr->address;
	while (*pch && isdigit(*pch))
		pch++;

	if (!*pch 
	    && 1 == sscanf(addr->address, "%u", &my_node->assocID)
	    && my_node->assocID <= USHRT_MAX) {
		
		destroy_address_node(addr);
		my_node->addr = NULL;
	} else {
		my_node->assocID = 0;
		my_node->addr = addr;
	}

	return my_node;
}

struct filegen_node *
create_filegen_node(
	int	filegen_token,
	queue *	options
	)
{
	struct filegen_node *my_node;
	
	my_node = get_node(sizeof *my_node);
	my_node->filegen_token = filegen_token;
	my_node->options = options;

	return my_node;
}


struct restrict_node *
create_restrict_node(
	struct address_node *addr,
	struct address_node *mask,
	queue *flags,
	int line_no
	)
{
	struct restrict_node *my_node;
	
	my_node = get_node(sizeof *my_node);

	my_node->addr = addr;
	my_node->mask = mask;
	my_node->flags = flags;
	my_node->line_no = line_no;

	return my_node;
}

void
destroy_restrict_node(
	struct restrict_node *my_node
	)
{
	/* With great care, free all the memory occupied by
	 * the restrict node
	 */
	if (my_node->addr)
		destroy_address_node(my_node->addr);
	if (my_node->mask)
		destroy_address_node(my_node->mask);
	DESTROY_QUEUE(my_node->flags);
	free_node(my_node);
}


struct setvar_node *
create_setvar_node(
	char *	var,
	char *	val,
	int	isdefault
	)
{
	char *	pch;
	struct setvar_node *my_node;

	/* do not allow = in the variable name */
	if (NULL != (pch = strchr(var, '=')))
		*pch = '\0';

	/* Now store the string into a setvar_node */
	my_node = get_node(sizeof *my_node);
	my_node->var = var;
	my_node->val = val;
	my_node->isdefault = isdefault;

	return my_node;
}


nic_rule_node *
create_nic_rule_node(
	int match_class,
	char *if_name,	/* interface name or numeric address */
	int action
	)
{
	nic_rule_node *my_node;
	
	NTP_REQUIRE(match_class != 0 || if_name != NULL);

	my_node = get_node(sizeof(*my_node));
	my_node->match_class = match_class;
	my_node->if_name = if_name;
	my_node->action = action;

	return my_node;
}


struct addr_opts_node *
create_addr_opts_node(
	struct address_node *addr,
	queue *options
	)
{
	struct addr_opts_node *my_node;

	my_node = get_node(sizeof *my_node);
	my_node->addr = addr;
	my_node->options = options;
	return my_node;
}

script_info *
create_sim_script_info(
	double duration,
	queue *script_queue
	)
{
#ifdef SIM
	return NULL;
#else
	script_info *my_info;
	struct attr_val *my_attr_val;

	my_info = get_node(sizeof *my_info);

	/* Initialize Script Info with default values*/
	my_info->duration = duration;
	my_info->freq_offset = 0;
	my_info->wander = 0;
	my_info->jitter = 0;
	my_info->prop_delay = NET_DLY;
	my_info->proc_delay = PROC_DLY;

	/* Traverse the script_queue and fill out non-default values */
	my_attr_val = queue_head(script_queue);
	while (my_attr_val != NULL) {
		/* Set the desired value */
		switch (my_attr_val->attr) {

		case T_Freq_Offset:
			my_info->freq_offset = my_attr_val->value.d;
			break;

		case T_Wander:
			my_info->wander = my_attr_val->value.d;
			break;

		case T_Jitter:
			my_info->jitter = my_attr_val->value.d;
			break;

		case T_Prop_Delay:
			my_info->prop_delay = my_attr_val->value.d;
			break;

		case T_Proc_Delay:
			my_info->proc_delay = my_attr_val->value.d;
			break;

		default:
			msyslog(LOG_ERR, 
				"Unknown script token %d",
				my_attr_val->attr);
		}
	}
	return (my_info);
#endif
}


#if !defined(SIM)

#define ADDR_LENGTH 16 + 1

static sockaddr_u *
get_next_address(
	struct address_node *addr
	)
{
	const char addr_prefix[] = "192.168.0.";
	static int curr_addr_no = 1;
	char addr_string[ADDR_LENGTH];
	sockaddr_u *final_addr;
	struct addrinfo *ptr;
	int retval;
	
	final_addr = emalloc(sizeof *final_addr);

	if (addr->type == T_String) {
		snprintf(addr_string, ADDR_LENGTH, "%s%d", addr_prefix, curr_addr_no++);
		printf("Selecting ip address %s for hostname %s\n", addr_string, addr->address);
		retval = getaddrinfo(addr_string, "ntp", NULL, &ptr);
	} else
		retval = getaddrinfo(addr->address, "ntp", NULL, &ptr);

	if (!retval) {
		memcpy(final_addr, ptr->ai_addr, ptr->ai_addrlen);
		fprintf(stderr, "Successful in setting ip address of simulated server to: %s\n", stoa(final_addr));
	}
	else {
		fprintf(stderr, "ERROR!! Could not get a new address\n");
		exit(1);
	}
	freeaddrinfo(ptr);
	return final_addr;
}
#endif /* !SIM */


server_info *
create_sim_server(
	struct address_node *addr,
	double server_offset,
	queue *script
	)
{
#ifdef SIM
	return NULL;
#else
	server_info *my_info;

	my_info = get_node(sizeof *my_info);

	my_info->server_time = server_offset;
	my_info->addr = get_next_address(addr);
	my_info->script = script;
	my_info->curr_script = dequeue(my_info->script);
	return my_info;
#endif /* SIM */
}

struct sim_node *
create_sim_node(
	queue *init_opts,
	queue *servers
	)
{
	struct sim_node *my_node;
	
	my_node = get_node(sizeof *my_node);

	my_node->init_opts = init_opts;
	my_node->servers = servers;
	return my_node;
}




/* FUNCTIONS FOR PERFORMING THE CONFIGURATION
 * ------------------------------------------
 */

static void
config_other_modes(
	struct config_tree *ptree
	)
{
	sockaddr_u addr_sock;
	struct address_node *addr_node;

	if (ptree->broadcastclient)
		proto_config(PROTO_BROADCLIENT, ptree->broadcastclient, 0., NULL);

	/* Configure the many-cast servers */
	addr_node = queue_head(ptree->manycastserver);
	if (addr_node != NULL) {
		do {
			ZERO_SOCK(&addr_sock);
			AF(&addr_sock) = (u_short)addr_node->type;

			if (getnetnum(addr_node->address, &addr_sock, 1, t_UNK)  == 1)
				proto_config(PROTO_MULTICAST_ADD, 0, 0., &addr_sock);

			addr_node = next_node(addr_node);
		} while (addr_node != NULL);
		sys_manycastserver = 1;
	}

	/* Configure the multicast clients */
	addr_node = queue_head(ptree->multicastclient);
	if (addr_node != NULL) {
		do {
			ZERO_SOCK(&addr_sock);
			AF(&addr_sock) = (u_short)addr_node->type;

			if (getnetnum(addr_node->address, &addr_sock, 1, t_UNK)  == 1)
				proto_config(PROTO_MULTICAST_ADD, 0, 0., &addr_sock);

			addr_node = next_node(addr_node);
		} while (addr_node != NULL);
		proto_config(PROTO_MULTICAST_ADD, 1, 0., NULL);
	}
}


#ifdef FREE_CFG_T
static void
free_config_other_modes(
	struct config_tree *ptree
	)
{
	struct address_node *addr_node;

	while (NULL != (addr_node = dequeue(ptree->manycastserver)))
		destroy_address_node(addr_node);

	while (NULL != (addr_node = dequeue(ptree->multicastclient)))
		destroy_address_node(addr_node);
}
#endif	/* FREE_CFG_T */


static void
config_auth(
	struct config_tree *ptree
	)
{
	ntp_u_int16_t	ufirst;
	ntp_u_int16_t	ulast;
	ntp_u_int16_t	u;
	struct attr_val *my_val;
#ifdef OPENSSL
#ifndef NO_INTRES
	u_char		digest[EVP_MAX_MD_SIZE];
	u_int		digest_len;
	EVP_MD_CTX	ctx;
#endif
	int		item;
#endif

	/* Crypto Command */
#ifdef OPENSSL
	item = -1;	/* quiet warning */
	my_val = queue_head(ptree->auth.crypto_cmd_list);
	while (my_val != NULL) {
		switch (my_val->attr) {

		default:
			NTP_INSIST(0);
			break;

		case T_Host:
			item = CRYPTO_CONF_PRIV;
			break;

		case T_Ident:
			item = CRYPTO_CONF_IDENT;
			break;

		case T_Pw:
			item = CRYPTO_CONF_PW;
			break;

		case T_Randfile:
			item = CRYPTO_CONF_RAND;
			break;

		case T_Sign:
			item = CRYPTO_CONF_SIGN;
			break;

		case T_Digest:
			item = CRYPTO_CONF_NID;
			break;
		}
		crypto_config(item, my_val->value.s);
		my_val = next_node(my_val);
	}
#endif /* OPENSSL */

	/* Keysdir Command */
	if (ptree->auth.keysdir) {
		if (keysdir != default_keysdir)
			free(keysdir);
		keysdir = estrdup(ptree->auth.keysdir);
	}


	/* ntp_signd_socket Command */
	if (ptree->auth.ntp_signd_socket) {
		if (ntp_signd_socket != default_ntp_signd_socket)
			free(ntp_signd_socket);
		ntp_signd_socket = estrdup(ptree->auth.ntp_signd_socket);
	}

#ifdef OPENSSL
	if (ptree->auth.cryptosw && !cryptosw) {
		crypto_setup();
		cryptosw = 1;
	}
#endif /* OPENSSL */

	/* Keys Command */
	if (ptree->auth.keys)
		getauthkeys(ptree->auth.keys);

	/* Control Key Command */
	if (ptree->auth.control_key)
		ctl_auth_keyid = (keyid_t)ptree->auth.control_key;

	/* Requested Key Command */
	if (ptree->auth.request_key) {
		DPRINTF(4, ("set info_auth_keyid to %08lx\n",
			    (u_long) ptree->auth.request_key));
		info_auth_keyid = (keyid_t)ptree->auth.request_key;
	}

	/* Trusted Key Command */
	my_val = queue_head(ptree->auth.trusted_key_list);
	for (; my_val != NULL; my_val = next_node(my_val)) {
		if ('i' == my_val->attr)
			authtrust(my_val->value.i, 1);
		else if ('-' == my_val->attr) {
			ufirst = my_val->value.u >> 16;
			ulast = my_val->value.u & 0xffff;
			for (u = ufirst; u <= ulast; u++)
				authtrust(u, 1);
		}
	}

#ifdef OPENSSL
	/* crypto revoke command */
	if (ptree->auth.revoke)
		sys_revoke = ptree->auth.revoke;
#endif /* OPENSSL */

#ifndef NO_INTRES
	/* find a keyid */
	if (info_auth_keyid == 0)
		req_keyid = 65535;
	else
		req_keyid = info_auth_keyid;

	/* if doesn't exist, make up one at random */
	if (authhavekey(req_keyid)) {
		req_keytype = cache_type;
#ifndef OPENSSL
		req_hashlen = 16;
#else	/* OPENSSL follows */
		EVP_DigestInit(&ctx, EVP_get_digestbynid(req_keytype));
		EVP_DigestFinal(&ctx, digest, &digest_len);
		req_hashlen = digest_len;
#endif
	} else {
		int	rankey;

		rankey = ntp_random();
		req_keytype = NID_md5;
		req_hashlen = 16;
		MD5auth_setkey(req_keyid, req_keytype,
		    (u_char *)&rankey, sizeof(rankey));
		authtrust(req_keyid, 1);
	}

	/* save keyid so we will accept config requests with it */
	info_auth_keyid = req_keyid;
#endif /* !NO_INTRES */

}


#ifdef FREE_CFG_T
static void
free_config_auth(
	struct config_tree *ptree
	)
{
	struct attr_val *my_val;

	while (NULL != 
	       (my_val = dequeue(ptree->auth.crypto_cmd_list))) {

		free(my_val->value.s);
		free_node(my_val);
	}
	DESTROY_QUEUE(ptree->auth.crypto_cmd_list);

	DESTROY_QUEUE(ptree->auth.trusted_key_list);
}
#endif	/* FREE_CFG_T */


static void
config_tos(
	struct config_tree *ptree
	)
{
	struct attr_val *tos;
	int item;

	item = -1;	/* quiet warning */
	tos = queue_head(ptree->orphan_cmds);
	while (tos != NULL) {
		switch(tos->attr) {

		default:
			NTP_INSIST(0);
			break;

		case T_Ceiling:
			item = PROTO_CEILING;
			break;

		case T_Floor:
			item = PROTO_FLOOR;
			break;

		case T_Cohort:
			item = PROTO_COHORT;
			break;

		case T_Orphan:
			item = PROTO_ORPHAN;
			break;

		case T_Mindist:
			item = PROTO_MINDISP;
			break;

		case T_Maxdist:
			item = PROTO_MAXDIST;
			break;

		case T_Minclock:
			item = PROTO_MINCLOCK;
			break;

		case T_Maxclock:
			item = PROTO_MAXCLOCK;
			break;

		case T_Minsane:
			item = PROTO_MINSANE;
			break;

		case T_Beacon:
			item = PROTO_BEACON;
			break;
		}
		proto_config(item, 0, tos->value.d, NULL);
		tos = next_node(tos);
	}
}


#ifdef FREE_CFG_T
static void
free_config_tos(
	struct config_tree *ptree
	)
{
	struct attr_val *tos;

	while (!empty(ptree->orphan_cmds)) {
		tos = dequeue(ptree->orphan_cmds);
		free_node(tos);
	}
}
#endif	/* FREE_CFG_T */


static void
config_monitor(
	struct config_tree *ptree
	)
{
	int *pfilegen_token;
	const char *filegen_string;
	const char *filegen_file;
	FILEGEN *filegen;
	struct filegen_node *my_node;
	struct attr_val *my_opts;
	int filegen_type;
	int filegen_flag;

	/* Set the statistics directory */
	if (ptree->stats_dir)
		stats_config(STATS_STATSDIR, ptree->stats_dir);

	/* NOTE:
	 * Calling filegen_get is brain dead. Doing a string
	 * comparison to find the relavant filegen structure is
	 * expensive.
	 *
	 * Through the parser, we already know which filegen is
	 * being specified. Hence, we should either store a
	 * pointer to the specified structure in the syntax tree
	 * or an index into a filegen array.
	 *
	 * Need to change the filegen code to reflect the above.
	 */

	/* Turn on the specified statistics */
	pfilegen_token = queue_head(ptree->stats_list);
	while (pfilegen_token != NULL) {
		filegen_string = keyword(*pfilegen_token);
		filegen = filegen_get(filegen_string);

		DPRINTF(4, ("enabling filegen for %s statistics '%s%s'\n",
			    filegen_string, filegen->prefix, 
			    filegen->basename));
		filegen->flag |= FGEN_FLAG_ENABLED;
		pfilegen_token = next_node(pfilegen_token);
	}

	/* Configure the statistics with the options */
	my_node = queue_head(ptree->filegen_opts);
	while (my_node != NULL) {
		filegen_file = keyword(my_node->filegen_token);
		filegen = filegen_get(filegen_file);

		/* Initialize the filegen variables to their pre-configurtion states */
		filegen_flag = filegen->flag;
		filegen_type = filegen->type;

		/* "filegen ... enabled" is the default (when filegen is used) */
		filegen_flag |= FGEN_FLAG_ENABLED;

		my_opts = queue_head(my_node->options);
		while (my_opts != NULL) {

			switch (my_opts->attr) {

			case T_File:
				filegen_file = my_opts->value.p;
				break;

			case T_Type:
				switch (my_opts->value.i) {

				default:
					NTP_INSIST(0);
					break;

				case T_None:
					filegen_type = FILEGEN_NONE;
					break;

				case T_Pid:
					filegen_type = FILEGEN_PID;
					break;

				case T_Day:
					filegen_type = FILEGEN_DAY;
					break;

				case T_Week:
					filegen_type = FILEGEN_WEEK;
					break;

				case T_Month:
					filegen_type = FILEGEN_MONTH;
					break;

				case T_Year:
					filegen_type = FILEGEN_YEAR;
					break;

				case T_Age:
					filegen_type = FILEGEN_AGE;
					break;
				}
				break;

			case T_Flag:
				switch (my_opts->value.i) {

				case T_Link:
					filegen_flag |= FGEN_FLAG_LINK;
					break;

				case T_Nolink:
					filegen_flag &= ~FGEN_FLAG_LINK;
					break;

				case T_Enable:
					filegen_flag |= FGEN_FLAG_ENABLED;
					break;

				case T_Disable:
					filegen_flag &= ~FGEN_FLAG_ENABLED;
					break;

				default:
					msyslog(LOG_ERR, 
						"Unknown filegen flag token %d",
						my_opts->value.i);
					exit(1);
				}
				break;
			default:
				msyslog(LOG_ERR,
					"Unknown filegen option token %d",
					my_opts->attr);
				exit(1);
			}
			my_opts = next_node(my_opts);
		}
		filegen_config(filegen, filegen_file, filegen_type,
			       filegen_flag);
		my_node = next_node(my_node);
	}
}


#ifdef FREE_CFG_T
static void
free_config_monitor(
	struct config_tree *ptree
	)
{
	char **filegen_string;
	struct filegen_node *my_node;
	struct attr_val *my_opts;

	if (ptree->stats_dir) {
		free(ptree->stats_dir);
		ptree->stats_dir = NULL;
	}

	while (NULL != (filegen_string = dequeue(ptree->stats_list)))
		free_node(filegen_string);

	while (NULL != (my_node = dequeue(ptree->filegen_opts))) {

		while (NULL != (my_opts = dequeue(my_node->options)))
			free_node(my_opts);

		free_node(my_node);
	}
}
#endif	/* FREE_CFG_T */


static void
config_access(
	struct config_tree *ptree
	)
{
	static int		warned_signd;
	struct attr_val *	my_opt;
	struct restrict_node *	my_node;
	int *			curr_flag;
	sockaddr_u		addr_sock;
	sockaddr_u		addr_mask;
	u_short			flags;
	u_short			mflags;
	int			restrict_default;
	const char *		signd_warning =
#ifdef HAVE_NTP_SIGND
	    "MS-SNTP signd operations currently block ntpd degrading service to all clients.";
#else
	    "mssntp restrict bit ignored, this ntpd was configured without --enable-ntp-signd.";
#endif

	/* Configure the discard options */
	my_opt = queue_head(ptree->discard_opts);
	while (my_opt != NULL) {

		switch(my_opt->attr) {

		case T_Average:
			ntp_minpoll = my_opt->value.i;
			break;

		case T_Minimum:
			ntp_minpkt = my_opt->value.i;
			break;

		case T_Monitor:
			mon_age = my_opt->value.i;
			break;

		default:
			msyslog(LOG_ERR,
				"Unknown discard option token %d",
				my_opt->attr);
			exit(1);
		}
		my_opt = next_node(my_opt);
	}

	/* Configure the restrict options */
	for (my_node = queue_head(ptree->restrict_opts);
	     my_node != NULL;
	     my_node = next_node(my_node)) {

		ZERO_SOCK(&addr_sock);

		if (NULL == my_node->addr) {
			/*
			 * The user specified a default rule without a
			 * -4 / -6 qualifier, add to both lists
			 */
			restrict_default = 1;
			ZERO_SOCK(&addr_mask);
		} else {
			restrict_default = 0;
			/* Resolve the specified address */
			AF(&addr_sock) = (u_short)my_node->addr->type;

			if (getnetnum(my_node->addr->address,
				      &addr_sock, 1, t_UNK) != 1) {

				msyslog(LOG_ERR,
					"restrict: error in address '%s' on line %d. Ignoring...",
					my_node->addr->address, my_node->line_no);
				continue;
			}

			SET_HOSTMASK(&addr_mask, AF(&addr_sock));

			/* Resolve the mask */
			if (my_node->mask) {
				ZERO_SOCK(&addr_mask);
				AF(&addr_mask) = (u_short)my_node->mask->type;
				if (getnetnum(my_node->mask->address, &addr_mask, 1, t_MSK) != 1) {

					msyslog(LOG_ERR,
						"restrict: error in mask '%s' on line %d. Ignoring...",
						my_node->mask->address, my_node->line_no);
					continue;
				}
			}
		}

		/* Parse the flags */
		flags = 0;
		mflags = 0;

		curr_flag = queue_head(my_node->flags);
		while (curr_flag != NULL) {
			switch (*curr_flag) {

			default:
				NTP_INSIST(0);
				break;

			case T_Ntpport:
				mflags |= RESM_NTPONLY;
				break;

			case T_Flake:
				flags |= RES_TIMEOUT;
				break;

			case T_Ignore:
				flags |= RES_IGNORE;
				break;

			case T_Kod:
				flags |= RES_KOD;
				break;

			case T_Mssntp:
				flags |= RES_MSSNTP;
				break;

			case T_Limited:
				flags |= RES_LIMITED;
				break;

			case T_Lowpriotrap:
				flags |= RES_LPTRAP;
				break;

			case T_Nomodify:
				flags |= RES_NOMODIFY;
				break;

			case T_Nopeer:
				flags |= RES_NOPEER;
				break;

			case T_Noquery:
				flags |= RES_NOQUERY;
				break;

			case T_Noserve:
				flags |= RES_DONTSERVE;
				break;

			case T_Notrap:
				flags |= RES_NOTRAP;
				break;

			case T_Notrust:
				flags |= RES_DONTTRUST;
				break;

			case T_Version:
				flags |= RES_VERSION;
				break;
			}
			curr_flag = next_node(curr_flag);
		}

		/* Set the flags */
		if (restrict_default) {
			AF(&addr_sock) = AF_INET;
			hack_restrict(RESTRICT_FLAGS, &addr_sock, &addr_mask,
				      mflags, flags);

			AF(&addr_sock) = AF_INET6;
		}

		hack_restrict(RESTRICT_FLAGS, &addr_sock, &addr_mask,
			      mflags, flags);

		if ((RES_MSSNTP & flags) && !warned_signd) {
			warned_signd = 1;
			fprintf(stderr, "%s\n", signd_warning);
			msyslog(LOG_WARNING, signd_warning);
		}
	}
}


#ifdef FREE_CFG_T
static void
free_config_access(
	struct config_tree *ptree
	)
{
	struct attr_val *	my_opt;
	struct restrict_node *	my_node;
	int *			curr_flag;

	while (NULL != (my_opt = dequeue(ptree->discard_opts)))
		free_node(my_opt);

	while (NULL != (my_node = dequeue(ptree->restrict_opts))) {
		while (NULL != (curr_flag = dequeue(my_node->flags)))
			free_node(curr_flag);

		destroy_restrict_node(my_node);
	}
}
#endif	/* FREE_CFG_T */


static void
config_tinker(
	struct config_tree *ptree
	)
{
	struct attr_val *tinker;
	int item;

	item = -1;	/* quiet warning */
	tinker = queue_head(ptree->tinker);
	while (tinker != NULL) {
		switch (tinker->attr) {

		default:
			NTP_INSIST(0);
			break;

		case T_Allan:
			item = LOOP_ALLAN;
			break;

		case T_Dispersion:
			item = LOOP_PHI;
			break;

		case T_Freq:
			item = LOOP_FREQ;
			break;

		case T_Huffpuff:
			item = LOOP_HUFFPUFF;
			break;

		case T_Panic:
			item = LOOP_PANIC;
			break;

		case T_Step:
			item = LOOP_MAX;
			break;

		case T_Stepout:
			item = LOOP_MINSTEP;
			break;
		}
		loop_config(item, tinker->value.d);
		tinker = next_node(tinker);
	}
}


#ifdef FREE_CFG_T
static void
free_config_tinker(
	struct config_tree *ptree
	)
{
	struct attr_val *tinker;

	while (NULL != (tinker = dequeue(ptree->tinker)))
		free_node(tinker);
}
#endif	/* FREE_CFG_T */


/*
 * config_nic_rules - apply interface listen/ignore/drop items
 */
void
config_nic_rules(
	struct config_tree *ptree
	)
{
	nic_rule_node *	curr_node;
	sockaddr_u	addr;
	nic_rule_match	match_type;
	nic_rule_action	action;
	char *		if_name;
	char *		pchSlash;
	int		prefixlen;
	int		addrbits;

	curr_node = queue_head(ptree->nic_rules);

	if (curr_node != NULL
	    && (HAVE_OPT( NOVIRTUALIPS ) || HAVE_OPT( INTERFACE ))) {
		msyslog(LOG_ERR,
			"interface/nic rules are not allowed with --interface (-I) or --novirtualips (-L)%s",
			(input_from_file) ? ", exiting" : "");
		if (input_from_file)
			exit(1);
		else
			return;
	}

	for (;
	     curr_node != NULL;
	     curr_node = next_node(curr_node)) {

		prefixlen = -1;
		if_name = curr_node->if_name;
		if (if_name != NULL)
			if_name = estrdup(if_name);

		switch (curr_node->match_class) {

		default:
			/*
			 * this assignment quiets a gcc "may be used
			 * uninitialized" warning and is here for no
			 * other reason.
			 */
			match_type = MATCH_ALL;
			NTP_INSIST(0);
			break;

		case 0:
			/*
			 * 0 is out of range for valid token T_...
			 * and in a nic_rules_node indicates the
			 * interface descriptor is either a name or
			 * address, stored in if_name in either case.
			 */
			NTP_INSIST(if_name != NULL);
			pchSlash = strchr(if_name, '/');
			if (pchSlash != NULL)
				*pchSlash = '\0';
			if (is_ip_address(if_name, &addr)) {
				match_type = MATCH_IFADDR;
				if (pchSlash != NULL) {
					sscanf(pchSlash + 1, "%d",
					    &prefixlen);
					addrbits = 8 *
					    SIZEOF_INADDR(AF(&addr));
					prefixlen = max(-1, prefixlen);
					prefixlen = min(prefixlen,
							addrbits);
				}
			} else {
				match_type = MATCH_IFNAME;
				if (pchSlash != NULL)
					*pchSlash = '/';
			}
			break;

		case T_All:
			match_type = MATCH_ALL;
			break;

		case T_Ipv4:
			match_type = MATCH_IPV4;
			break;

		case T_Ipv6:
			match_type = MATCH_IPV6;
			break;

		case T_Wildcard:
			match_type = MATCH_WILDCARD;
			break;
		}

		switch (curr_node->action) {

		default:
			/*
			 * this assignment quiets a gcc "may be used
			 * uninitialized" warning and is here for no
			 * other reason.
			 */
			action = ACTION_LISTEN;
			NTP_INSIST(0);
			break;

		case T_Listen:
			action = ACTION_LISTEN;
			break;

		case T_Ignore:
			action = ACTION_IGNORE;
			break;

		case T_Drop:
			action = ACTION_DROP;
			break;
		}

		add_nic_rule(match_type, if_name, prefixlen,
			     action);
		timer_interfacetimeout(current_time + 2);
		if (if_name != NULL)
			free(if_name);
	}
}


#ifdef FREE_CFG_T
static void
free_config_nic_rules(
	struct config_tree *ptree
	)
{
	nic_rule_node *curr_node;

	while (NULL != (curr_node = dequeue(ptree->nic_rules))) {
		if (curr_node->if_name != NULL)
			free(curr_node->if_name);
		free_node(curr_node);
	}
	DESTROY_QUEUE(ptree->nic_rules);
}
#endif	/* FREE_CFG_T */


static void
apply_enable_disable(
	queue *	q,
	int	enable
	)
{
	struct attr_val *curr_flag;
	int option;
#ifdef BC_LIST_FRAMEWORK_NOT_YET_USED
	bc_entry *pentry;
#endif

	for (curr_flag = queue_head(q);
	     curr_flag != NULL;
	     curr_flag = next_node(curr_flag)) {

		option = curr_flag->value.i;
		switch (option) {

		default:
			msyslog(LOG_ERR,
				"can not apply enable/disable token %d, unknown",
				option);
			break;

		case T_Auth:
			proto_config(PROTO_AUTHENTICATE, enable, 0., NULL);
			break;

		case T_Bclient:
			proto_config(PROTO_BROADCLIENT, enable, 0., NULL);
			break;

		case T_Calibrate:
			proto_config(PROTO_CAL, enable, 0., NULL);
			break;

		case T_Kernel:
			proto_config(PROTO_KERNEL, enable, 0., NULL);
			break;

		case T_Monitor:
			proto_config(PROTO_MONITOR, enable, 0., NULL);
			break;

		case T_Ntp:
			proto_config(PROTO_NTP, enable, 0., NULL);
			break;

		case T_Stats:
			proto_config(PROTO_FILEGEN, enable, 0., NULL);
			break;

#ifdef BC_LIST_FRAMEWORK_NOT_YET_USED
		case T_Bc_bugXXXX:
			pentry = bc_list;
			while (pentry->token) {
				if (pentry->token == option)
					break;
				pentry++;
			}
			if (!pentry->token) {
				msyslog(LOG_ERR, 
					"compat token %d not in bc_list[]",
					option);
				continue;
			}
			pentry->enabled = enable;
			break;
#endif
		}
	}
}


static void
config_system_opts(
	struct config_tree *ptree
	)
{
	apply_enable_disable(ptree->enable_opts, 1);
	apply_enable_disable(ptree->disable_opts, 0);
}


#ifdef FREE_CFG_T
static void
free_config_system_opts(
	struct config_tree *ptree
	)
{
	struct attr_val *flag;

	while (NULL != (flag = dequeue(ptree->enable_opts)))
		free_node(flag);

	while (NULL != (flag = dequeue(ptree->disable_opts)))
		free_node(flag);
}
#endif	/* FREE_CFG_T */


static void
config_logconfig(
	struct config_tree *ptree
	)
{
	struct attr_val *my_logconfig;

	my_logconfig = queue_head(ptree->logconfig);
	while (my_logconfig != NULL) {

		switch (my_logconfig->attr) {
		case '+':
			ntp_syslogmask |= get_logmask(my_logconfig->value.s);
			break;
		case '-':
			ntp_syslogmask &= ~get_logmask(my_logconfig->value.s);
			break;
		case '=':
			ntp_syslogmask = get_logmask(my_logconfig->value.s);
			break;
		}
		my_logconfig = next_node(my_logconfig);
	}
}


#ifdef FREE_CFG_T
static void
free_config_logconfig(
	struct config_tree *ptree
	)
{
	struct attr_val *my_logconfig;

	while (NULL != (my_logconfig = dequeue(ptree->logconfig))) {		
		free(my_logconfig->value.s);
		free_node(my_logconfig);
	}
}
#endif	/* FREE_CFG_T */


static void
config_phone(
	struct config_tree *ptree
	)
{
	int i = 0;
	char **s;

	s = queue_head(ptree->phone);
	while (s != NULL) {
		if (i < COUNTOF(sys_phone) - 1) {
			sys_phone[i++] = estrdup(*s);
			sys_phone[i] = NULL;
		} else {
			msyslog(LOG_INFO,
				"phone: Number of phone entries exceeds %lu. Ignoring phone %s...",
				(u_long)(COUNTOF(sys_phone) - 1), *s);
		}
		s = next_node(s);
	}
}


#ifdef FREE_CFG_T
static void
free_config_phone(
	struct config_tree *ptree
	)
{
	char **s;

	while (NULL != (s = dequeue(ptree->phone))) {
		free(*s);
		free_node(s);
	}
}
#endif	/* FREE_CFG_T */


static void
config_qos(
	struct config_tree *ptree
	)
{
	struct attr_val *my_qosconfig;
	char *s;
#ifdef HAVE_IPTOS_SUPPORT
	unsigned int qtos = 0;
#endif

	my_qosconfig = queue_head(ptree->qos);
	while (my_qosconfig != NULL) {
		s = my_qosconfig->value.s;
#ifdef HAVE_IPTOS_SUPPORT
		if (!strcmp(s, "lowdelay"))
			qtos = CONF_QOS_LOWDELAY;
		else if (!strcmp(s, "throughput"))
			qtos = CONF_QOS_THROUGHPUT;
		else if (!strcmp(s, "reliability"))
			qtos = CONF_QOS_RELIABILITY;
		else if (!strcmp(s, "mincost"))
			qtos = CONF_QOS_MINCOST;
#ifdef IPTOS_PREC_INTERNETCONTROL
		else if (!strcmp(s, "routine") || !strcmp(s, "cs0"))
			qtos = CONF_QOS_CS0;
		else if (!strcmp(s, "priority") || !strcmp(s, "cs1"))
			qtos = CONF_QOS_CS1;
		else if (!strcmp(s, "immediate") || !strcmp(s, "cs2"))
			qtos = CONF_QOS_CS2;
		else if (!strcmp(s, "flash") || !strcmp(s, "cs3"))
			qtos = CONF_QOS_CS3; 	/* overlapping prefix on keyword */
		if (!strcmp(s, "flashoverride") || !strcmp(s, "cs4"))
			qtos = CONF_QOS_CS4;
		else if (!strcmp(s, "critical") || !strcmp(s, "cs5"))
			qtos = CONF_QOS_CS5;
		else if(!strcmp(s, "internetcontrol") || !strcmp(s, "cs6"))
			qtos = CONF_QOS_CS6;
		else if (!strcmp(s, "netcontrol") || !strcmp(s, "cs7"))
			qtos = CONF_QOS_CS7;
#endif  /* IPTOS_PREC_INTERNETCONTROL */
		if (qtos == 0)
			msyslog(LOG_ERR, "parse error, qos %s not accepted\n", s);
		else
			qos = qtos;
#endif  /* HAVE IPTOS_SUPPORT */
		/*
		 * value is set, but not being effective. Need code to
		 * change   the current connections to notice. Might
		 * also  consider logging a message about the action.
		 * XXX msyslog(LOG_INFO, "QoS %s requested by config\n", s);
		 */
		my_qosconfig = next_node(my_qosconfig);
	}
}


#ifdef FREE_CFG_T
static void
free_config_qos(
	struct config_tree *ptree
	)
{
	struct attr_val *my_qosconfig;

	while (NULL != (my_qosconfig = dequeue(ptree->qos))) {
		free(my_qosconfig->value.s);
		free_node(my_qosconfig);
	}
}
#endif	/* FREE_CFG_T */


static void
config_setvar(
	struct config_tree *ptree
	)
{
	struct setvar_node *my_node;
	size_t	varlen, vallen, octets;
	char *	str;

	str = NULL;
	my_node = queue_head(ptree->setvar);
	while (my_node != NULL) {
		varlen = strlen(my_node->var);
		vallen = strlen(my_node->val);
		octets = varlen + vallen + 1 + 1;
		str = erealloc(str, octets);
		snprintf(str, octets, "%s=%s", my_node->var,
			 my_node->val);
		set_sys_var(str, octets, (my_node->isdefault)
						? DEF 
						: 0);
		my_node = next_node(my_node);
	}
	if (str != NULL)
		free(str);
}


#ifdef FREE_CFG_T
static void
free_config_setvar(
	struct config_tree *ptree
	)
{
	struct setvar_node *my_node;

	while (NULL != (my_node = dequeue(ptree->setvar))) {
		free(my_node->var);
		free(my_node->val);
		free_node(my_node);
	}
}
#endif	/* FREE_CFG_T */


static void
config_ttl(
	struct config_tree *ptree
	)
{
	int i = 0;
	int *curr_ttl;

	curr_ttl = queue_head(ptree->ttl);
	while (curr_ttl != NULL) {
		if (i < COUNTOF(sys_ttl))
			sys_ttl[i++] = (u_char)*curr_ttl;
		else
			msyslog(LOG_INFO,
				"ttl: Number of TTL entries exceeds %lu. Ignoring TTL %d...",
				(u_long)COUNTOF(sys_ttl), *curr_ttl);

		curr_ttl = next_node(curr_ttl);
	}
	sys_ttlmax = i - 1;
}


#ifdef FREE_CFG_T
static void
free_config_ttl(
	struct config_tree *ptree
	)
{
	/* coming DESTROY_QUEUE(ptree->ttl) is enough */
}
#endif	/* FREE_CFG_T */


static void
config_trap(
	struct config_tree *ptree
	)
{
	struct addr_opts_node *curr_trap;
	struct attr_val *curr_opt;
	sockaddr_u addr_sock;
	sockaddr_u peeraddr;
	struct address_node *addr_node;
	struct interface *localaddr;
	u_short port_no;
	int err_flag;

	/* silence warning about addr_sock potentially uninitialized */
	AF(&addr_sock) = AF_UNSPEC;

	for (curr_trap = queue_head(ptree->trap);
	     curr_trap != NULL;
	     curr_trap = next_node(curr_trap)) {

		err_flag = 0;
		port_no = 0;
		localaddr = NULL;

		curr_opt = queue_head(curr_trap->options);
		while (curr_opt != NULL) {
			if (T_Port == curr_opt->attr) {
				if (curr_opt->value.i < 1 
				    || curr_opt->value.i > USHRT_MAX) {
					msyslog(LOG_ERR,
						"invalid port number "
						"%d, trap ignored", 
						curr_opt->value.i);
					err_flag = 1;
				}
				port_no = (u_short)curr_opt->value.i;
			}
			else if (T_Interface == curr_opt->attr) {
				addr_node = curr_opt->value.p;

				/* Resolve the interface address */
				ZERO_SOCK(&addr_sock);
				AF(&addr_sock) = (u_short)addr_node->type;

				if (getnetnum(addr_node->address,
					      &addr_sock, 1, t_UNK) != 1) {
					err_flag = 1;
					break;
				}

				localaddr = findinterface(&addr_sock);

				if (NULL == localaddr) {
					msyslog(LOG_ERR,
						"can't find interface with address %s",
						stoa(&addr_sock));
					err_flag = 1;
				}
			}
			curr_opt = next_node(curr_opt);
		}

		/* Now process the trap for the specified interface
		 * and port number
		 */
		if (!err_flag) {
			ZERO_SOCK(&peeraddr);
			if (1 != getnetnum(curr_trap->addr->address,
					   &peeraddr, 1, t_UNK))
				continue;

			/* port is at same location for v4 and v6 */
			SET_PORT(&peeraddr, port_no ? port_no : TRAPPORT);

			if (NULL == localaddr)
				localaddr = ANY_INTERFACE_CHOOSE(&peeraddr);
			else
				AF(&peeraddr) = AF(&addr_sock);

			if (!ctlsettrap(&peeraddr, localaddr, 0,
					NTP_VERSION))
				msyslog(LOG_ERR,
					"can't set trap for %s",
					stoa(&peeraddr));
		}
	}
}


#ifdef FREE_CFG_T
static void
free_config_trap(
	struct config_tree *ptree
	)
{
	struct addr_opts_node *curr_trap;
	struct attr_val *curr_opt;
	struct address_node *addr_node;

	while (NULL != (curr_trap = dequeue(ptree->trap))) {
		while (curr_trap->options != NULL && NULL != 
		       (curr_opt = dequeue(curr_trap->options))) {

			if (T_Interface == curr_opt->attr) {
				addr_node = curr_opt->value.p;
				destroy_address_node(addr_node);
			}
			free_node(curr_opt);
		}
		DESTROY_QUEUE(curr_trap->options);
		free_node(curr_trap);
	}
}
#endif	/* FREE_CFG_T */


static void
config_fudge(
	struct config_tree *ptree
	)
{
	struct addr_opts_node *curr_fudge;
	struct attr_val *curr_opt;
	sockaddr_u addr_sock;
	struct address_node *addr_node;
	struct refclockstat clock_stat;
	int err_flag;

	curr_fudge = queue_head(ptree->fudge);
	while (curr_fudge != NULL) {
		err_flag = 0;

		/* Get the reference clock address and
		 * ensure that it is sane
		 */
		addr_node = curr_fudge->addr;
		ZERO_SOCK(&addr_sock);
		if (getnetnum(addr_node->address, &addr_sock, 1, t_REF)
		    != 1)
			err_flag = 1;

		if (!ISREFCLOCKADR(&addr_sock)) {
			msyslog(LOG_ERR,
				"inappropriate address %s for the fudge command, line ignored",
				stoa(&addr_sock));
			err_flag = 1;
		}

		/* Parse all the options to the fudge command */
		memset(&clock_stat, 0, sizeof(clock_stat));
		curr_opt = queue_head(curr_fudge->options);
		while (curr_opt != NULL) {
			switch (curr_opt->attr) {
			case T_Time1:
				clock_stat.haveflags |= CLK_HAVETIME1;
				clock_stat.fudgetime1 = curr_opt->value.d;
				break;
			case T_Time2:
				clock_stat.haveflags |= CLK_HAVETIME2;
				clock_stat.fudgetime2 = curr_opt->value.d;
				break;
			case T_Stratum:
				clock_stat.haveflags |= CLK_HAVEVAL1;
				clock_stat.fudgeval1 = curr_opt->value.i;
				break;
			case T_Refid:
				clock_stat.haveflags |= CLK_HAVEVAL2;
				clock_stat.fudgeval2 = 0;
				memcpy(&clock_stat.fudgeval2,
				       curr_opt->value.s,
				       min(strlen(curr_opt->value.s), 4));
				break;
			case T_Flag1:
				clock_stat.haveflags |= CLK_HAVEFLAG1;
				if (curr_opt->value.i)
					clock_stat.flags |= CLK_FLAG1;
				else
					clock_stat.flags &= ~CLK_FLAG1;
				break;
			case T_Flag2:
				clock_stat.haveflags |= CLK_HAVEFLAG2;
				if (curr_opt->value.i)
					clock_stat.flags |= CLK_FLAG2;
				else
					clock_stat.flags &= ~CLK_FLAG2;
				break;
			case T_Flag3:
				clock_stat.haveflags |= CLK_HAVEFLAG3;
				if (curr_opt->value.i)
					clock_stat.flags |= CLK_FLAG3;
				else
					clock_stat.flags &= ~CLK_FLAG3;
				break;
			case T_Flag4:
				clock_stat.haveflags |= CLK_HAVEFLAG4;
				if (curr_opt->value.i)
					clock_stat.flags |= CLK_FLAG4;
				else
					clock_stat.flags &= ~CLK_FLAG4;
				break;
			default:
				msyslog(LOG_ERR,
					"Unexpected fudge internal flag 0x%x for %s\n",
					curr_opt->attr, stoa(&addr_sock));
				exit(curr_opt->attr ? curr_opt->attr : 1);
			}

			curr_opt = next_node(curr_opt);
		}

#ifdef REFCLOCK
		if (!err_flag)
			refclock_control(&addr_sock, &clock_stat,
					 (struct refclockstat *)0);
#endif

		curr_fudge = next_node(curr_fudge);
	}
}


#ifdef FREE_CFG_T
static void
free_config_fudge(
	struct config_tree *ptree
	)
{
	struct addr_opts_node *curr_fudge;
	struct attr_val *curr_opt;

	while (NULL != (curr_fudge = dequeue(ptree->fudge))) {
		while (NULL != (curr_opt = dequeue(curr_fudge->options))) {
			
			switch (curr_opt->attr) {
			case CLK_HAVEVAL2:
				free(curr_opt->value.s);
			}

			free_node(curr_opt);
		}

		DESTROY_QUEUE(curr_fudge->options);
		free_node(curr_fudge);
	}
}
#endif	/* FREE_CFG_T */


static void
config_vars(
	struct config_tree *ptree
	)
{
	struct attr_val *curr_var;
	FILE *new_file;
	int len;

	curr_var = queue_head(ptree->vars);
	while (curr_var != NULL) {
		/* Determine which variable to set and set it */
		switch (curr_var->attr) {
		case T_Broadcastdelay:
			proto_config(PROTO_BROADDELAY, 0, curr_var->value.d, NULL);
			break;
		case T_Calldelay:
			proto_config(PROTO_CALLDELAY, curr_var->value.i, 0, NULL);
			break;
		case T_Tick:
			proto_config(PROTO_ADJ, 0, curr_var->value.d, NULL);
			break;
		case T_Driftfile:
			if ('\0' == curr_var->value.s[0]) {
				stats_drift_file = 0;
				msyslog(LOG_INFO, "config: driftfile disabled\n");
			} else
				stats_config(STATS_FREQ_FILE, curr_var->value.s);
			break;
		case T_WanderThreshold:
			wander_threshold = curr_var->value.d;
			break;
		case T_Leapfile:
			stats_config(STATS_LEAP_FILE, curr_var->value.s);
			break;
		case T_Pidfile:
			stats_config(STATS_PID_FILE, curr_var->value.s);
			break;
		case T_Logfile:
			new_file = fopen(curr_var->value.s, "a");
			if (new_file != NULL) {
				NLOG(NLOG_SYSINFO) /* conditional if clause for conditional syslog */
				    msyslog(LOG_NOTICE, "logging to file %s", curr_var->value.s);
				if (syslog_file != NULL &&
				    fileno(syslog_file) != fileno(new_file))
					(void)fclose(syslog_file);

				syslog_file = new_file;
				syslogit = 0;
			}
			else
				msyslog(LOG_ERR,
					"Cannot open log file %s",
					curr_var->value.s);
			break;

		case T_Saveconfigdir:
			if (saveconfigdir != NULL)
				free(saveconfigdir);
			len = strlen(curr_var->value.s);
			if (0 == len)
				saveconfigdir = NULL;
			else if (DIR_SEP != curr_var->value.s[len - 1]
#ifdef SYS_WINNT	/* slash is also a dir. sep. on Windows */
				 && '/' != curr_var->value.s[len - 1]
#endif
				    ) {
					len++;
					saveconfigdir = emalloc(len + 1);
					snprintf(saveconfigdir, len + 1,
						 "%s%c",
						 curr_var->value.s,
						 DIR_SEP);
				} else
					saveconfigdir = estrdup(
					    curr_var->value.s);
			break;

		case T_Automax:
#ifdef OPENSSL
			sys_automax = curr_var->value.i;
#endif
			break;

		default:
			msyslog(LOG_ERR,
				"config_vars(): unexpected token %d",
				curr_var->attr);
		}
		curr_var = next_node(curr_var);
	}
}


#ifdef FREE_CFG_T
static void
free_config_vars(
	struct config_tree *ptree
	)
{
	struct attr_val *curr_var;

	while (NULL != (curr_var = dequeue(ptree->vars))) {
		/* Determine which variable to set and set it */
		switch (curr_var->attr) {
		case T_Driftfile:
		case T_Leapfile:
		case T_Pidfile:
		case T_Logfile:
			free(curr_var->value.s);
		}
		free_node(curr_var);
	}
}
#endif	/* FREE_CFG_T */


/* Define a function to check if a resolved address is sane.
 * If yes, return 1, else return 0;
 */
static int
is_sane_resolved_address(
	sockaddr_u *	peeraddr,
	int		hmode
	)
{
	if (!ISREFCLOCKADR(peeraddr) && ISBADADR(peeraddr)) {
		msyslog(LOG_ERR,
			"attempt to configure invalid address %s",
			stoa(peeraddr));
		return 0;
	}
	/*
	 * Shouldn't be able to specify multicast
	 * address for server/peer!
	 * and unicast address for manycastclient!
	 */
	if ((T_Server == hmode || T_Peer == hmode || T_Pool == hmode)
	    && IS_MCAST(peeraddr)) {
		msyslog(LOG_ERR,
			"attempt to configure invalid address %s",
			stoa(peeraddr));
		return 0;
	}
	if (T_Manycastclient == hmode && !IS_MCAST(peeraddr)) {
		msyslog(LOG_ERR,
			"attempt to configure invalid address %s",
			stoa(peeraddr));
		return 0;
	}

	if (IS_IPV6(peeraddr) && !ipv6_works)
		return 0;

	/* Ok, all tests succeeded, now we can return 1 */
	return 1;
}

static int
get_correct_host_mode(
	int hmode
	)
{
	switch (hmode) {
	    case T_Server:
	    case T_Pool:
	    case T_Manycastclient:
		return MODE_CLIENT;
		break;
	    case T_Peer:
		return MODE_ACTIVE;
		break;
	    case T_Broadcast:
		return MODE_BROADCAST;
		break;
	    default:
		return -1;
	}
}

static void
config_peers(
	struct config_tree *ptree
	)
{
	struct addrinfo *res, *res_bak;
	sockaddr_u peeraddr;
	struct peer_node *curr_peer;
	struct attr_val *option;
	int hmode;
	int peerflags;
	int status;
	int no_needed;
	int i;

	curr_peer = queue_head(ptree->peers);
	while (curr_peer != NULL) {
		/* Find the number of associations needed.
		 * If a pool coomand is specified, then sys_maxclock needed
		 * else, only one is needed
		 */
		no_needed = (T_Pool == curr_peer->host_mode)
				? sys_maxclock
				: 1;

		/* Find the correct host-mode */
		hmode = get_correct_host_mode(curr_peer->host_mode);
		NTP_INSIST(hmode != -1);

		/* translate peerflags options to bits */
		peerflags = 0;
		option = queue_head(curr_peer->peerflags);
		for (;	option != NULL; option = next_node(option))
			switch (option->value.i) {

			default:
				NTP_INSIST(0);
				break;

			case T_Autokey:
				peerflags |= FLAG_SKEY;
				break;

			case T_Burst:
				peerflags |= FLAG_BURST;
				break;

			case T_Iburst:
				peerflags |= FLAG_IBURST;
				break;

			case T_Noselect:
				peerflags |= FLAG_NOSELECT;
				break;

			case T_Preempt:
				peerflags |= FLAG_PREEMPT;
				break;

			case T_Prefer:
				peerflags |= FLAG_PREFER;
				break;

			case T_True:
				peerflags |= FLAG_TRUE;
				break;

			case T_Xleave:
				peerflags |= FLAG_XLEAVE;
				break;
			}

		/* Attempt to resolve the address */
		ZERO_SOCK(&peeraddr);
		AF(&peeraddr) = (u_short)curr_peer->addr->type;

		status = get_multiple_netnums(curr_peer->addr->address,
		    &peeraddr, &res, 0, t_UNK);

#ifdef FORCE_DEFER_DNS
		/* Hack for debugging Deferred DNS
		 * Pretend working names didn't work.
		 */
		if (status == 1) {
			/* Deferring everything breaks refclocks. */
			memcpy(&peeraddr, res->ai_addr, res->ai_addrlen);
			if (!ISREFCLOCKADR(&peeraddr)) {
				status = 0;  /* force deferred DNS path */
				msyslog(LOG_INFO, "Forcing Deferred DNS for %s, %s",
					curr_peer->addr->address, stoa(&peeraddr));
			} else {
				msyslog(LOG_INFO, "NOT Deferring DNS for %s, %s",
					curr_peer->addr->address, stoa(&peeraddr));
			}
		}
#endif

		/* I don't know why getnetnum would return -1.
		 * The old code had this test, so I guess it must be
		 * useful
		 */
		if (status == -1) {
			/* Do nothing, apparently we found an IPv6
			 * address and can't do anything about it */
		}
		/* Check if name resolution failed. If yes, store the
		 * peer information in a file for asynchronous
		 * resolution later
		 */
		else if (status != 1) {
			msyslog(LOG_INFO, "Deferring DNS for %s %d", curr_peer->addr->address, no_needed);
			save_resolve(curr_peer->addr->address,
				     no_needed,
				     curr_peer->addr->type,
				     hmode,
				     curr_peer->peerversion,
				     curr_peer->minpoll,
				     curr_peer->maxpoll,
				     peerflags,
				     curr_peer->ttl,
				     curr_peer->peerkey,
				     (u_char *)"*");
		}
		/* Yippie!! Name resolution has succeeded!!!
		 * Now we can proceed to some more sanity checks on
		 * the resolved address before we start to configure
		 * the peer
		 */
		else {
			res_bak = res;

			/*
			 * Loop to configure the desired number of
			 * associations
			 */
			for (i = 0; (i < no_needed) && res; res =
			    res->ai_next) {
				++i;
				memcpy(&peeraddr, res->ai_addr,
				    res->ai_addrlen);
				if (is_sane_resolved_address(
					&peeraddr,
					curr_peer->host_mode))

					peer_config(&peeraddr,
					    NULL,
					    hmode,
					    curr_peer->peerversion,
					    curr_peer->minpoll,
					    curr_peer->maxpoll,
					    peerflags,
					    curr_peer->ttl,
					    curr_peer->peerkey,
					    (u_char *)"*");
			}
			freeaddrinfo(res_bak);
		}
		curr_peer = next_node(curr_peer);
	}
}


#ifdef FREE_CFG_T
static void
free_config_peers(
	struct config_tree *ptree
	)
{
	struct peer_node *curr_peer;

	while (NULL != (curr_peer = dequeue(ptree->peers))) {
		destroy_address_node(curr_peer->addr);
		DESTROY_QUEUE(curr_peer->peerflags);
		free_node(curr_peer);
	}
}
#endif	/* FREE_CFG_T */


static void
config_unpeers(
	struct config_tree *ptree
	)
{
	struct addrinfo *res, *res_bak;
	sockaddr_u peeraddr;
	struct unpeer_node *curr_unpeer;
	struct peer *peer;
	int status;
	int found;

	for (curr_unpeer = queue_head(ptree->unpeers);
	     curr_unpeer != NULL;
	     curr_unpeer = next_node(curr_unpeer)) {

		/*
		 * Either AssocID will be zero, and we unpeer by name/
		 * address addr, or it is nonzero and addr NULL.
		 */
		if (curr_unpeer->assocID) {
			peer = findpeerbyassoc((u_int)curr_unpeer->assocID);
			if (peer != NULL) {
				peer_clear(peer, "GONE");
				unpeer(peer);
			}	

			continue;
		}

		/* Attempt to resolve the name or address */
		ZERO_SOCK(&peeraddr);
		AF(&peeraddr) = (u_short)curr_unpeer->addr->type;

		status = get_multiple_netnums(
			curr_unpeer->addr->address, &peeraddr, &res, 0,
			t_UNK);

		/* I don't know why getnetnum would return -1.
		 * The old code had this test, so I guess it must be
		 * useful
		 */
		if (status == -1) {
			/* Do nothing, apparently we found an IPv6
			 * address and can't do anything about it */
		}
		/* Check if name resolution failed. If yes, throw
		 * up our hands.
		 */
		else if (status != 1) {
			/* Do nothing */
		}
		/* Yippie!! Name resolution has succeeded!!!
		 */
		else {
			res_bak = res;

			/*
			 * Loop through the addresses found
			 */
			while (res) {
				memcpy(&peeraddr, res->ai_addr, res->ai_addrlen);

				found = 0;
				peer = NULL;

				DPRINTF(1, ("searching for %s\n", stoa(&peeraddr)));

				while (!found) {
					peer = findexistingpeer(&peeraddr, peer, -1, 0);
					if (!peer)
						break;
					if (peer->flags & FLAG_CONFIG)
						found = 1;
				}

				if (found) {
					peer_clear(peer, "GONE");
					unpeer(peer);
				}

				res = res->ai_next;
			}
			freeaddrinfo(res_bak);
		}
	}
}


#ifdef FREE_CFG_T
static void
free_config_unpeers(
	struct config_tree *ptree
	)
{
	struct unpeer_node *curr_unpeer;

	while (NULL != (curr_unpeer = dequeue(ptree->unpeers))) {
		destroy_address_node(curr_unpeer->addr);
		free_node(curr_unpeer);
	}
}
#endif	/* FREE_CFG_T */


#ifdef SIM
static void
config_sim(
	struct config_tree *ptree
	)
{
	int i;
	server_info *serv_info;
	struct attr_val *init_stmt;

	/* Check if a simulate block was found in the configuration code.
	 * If not, return an error and exit
	 */
	if (NULL == ptree->sim_details) {
		fprintf(stderr, "ERROR!! I couldn't find a \"simulate\" block for configuring the simulator.\n");
		fprintf(stderr, "\tCheck your configuration file.\n");
		exit(1);
	}

	/* Process the initialization statements
	 * -------------------------------------
	 */
	init_stmt = queue_head(ptree->sim_details->init_opts);
	while (init_stmt != NULL) {

		switch(init_stmt->attr) {

		case T_Beep_Delay:
			simulation.beep_delay = init_stmt->value.d;
			break;

		case T_Sim_Duration:
			simulation.end_time = init_stmt->value.d;
			break;

		default:
			fprintf(stderr,
				"Unknown simulator init token %d\n",
				init_stmt->attr);
			exit(1);
		}
		init_stmt = next_node(init_stmt);
	}


	/* Process the server list
	 * -----------------------
	 */
	simulation.num_of_servers = 
		get_no_of_elements(ptree->sim_details->servers);
	simulation.servers = emalloc(simulation.num_of_servers 
				     * sizeof(server_info));

	serv_info = queue_head(ptree->sim_details->servers);
	for (i = 0;i < simulation.num_of_servers; i++) {
		if (NULL == serv_info) {
			fprintf(stderr, "Simulator server list is corrupt\n");
			exit(1);
		} else
			memcpy(&simulation.servers[i], serv_info, sizeof(server_info));
		serv_info = next_node(serv_info);
	}

	/* Create server associations */
	printf("Creating server associations\n");
	create_server_associations();
	fprintf(stderr,"\tServer associations successfully created!!\n");
}


#ifdef FREE_CFG_T
static void
free_config_sim(
	struct config_tree *ptree
	)
{
	if (NULL == ptree->sim_details)
		return;

	DESTROY_QUEUE(ptree->sim_details->init_opts);
	DESTROY_QUEUE(ptree->sim_details->servers);

	/* Free the sim_node memory and set the sim_details as NULL */
	free_node(ptree->sim_details);
	ptree->sim_details = NULL;
}
#endif	/* FREE_CFG_T */
#endif	/* SIM */


/* Define two different config functions. One for the daemon and the other for
 * the simulator. The simulator ignores a lot of the standard ntpd configuration
 * options
 */
#ifndef SIM
static void
config_ntpd(
	struct config_tree *ptree
	)
{
	config_nic_rules(ptree);
	io_open_sockets();
	config_monitor(ptree);
	config_auth(ptree);
	config_tos(ptree);
	config_access(ptree);
	config_tinker(ptree);
	config_system_opts(ptree);
	config_logconfig(ptree);
	config_phone(ptree);
	config_setvar(ptree);
	config_ttl(ptree);
	config_trap(ptree);
	config_vars(ptree);
	config_other_modes(ptree);
	config_peers(ptree);
	config_unpeers(ptree);
	config_fudge(ptree);
	config_qos(ptree);
}
#endif	/* !SIM */


#ifdef SIM
static void
config_ntpdsim(
	struct config_tree *ptree
	)
{
	printf("Configuring Simulator...\n");
	printf("Some ntpd-specific commands in the configuration file will be ignored.\n");

	config_tos(ptree);
	config_monitor(ptree);
	config_tinker(ptree);
	config_system_opts(ptree);
	config_logconfig(ptree);
	config_vars(ptree);
	config_sim(ptree);
}
#endif /* SIM */


/*
 * config_remotely() - implements ntpd side of ntpq :config
 */
void
config_remotely(
	sockaddr_u *	remote_addr
	)
{
	struct FILE_INFO remote_cuckoo;
	char origin[128];

	snprintf(origin, sizeof(origin), "remote config from %s",
		 stoa(remote_addr));
	memset(&remote_cuckoo, 0, sizeof(remote_cuckoo));
	remote_cuckoo.fname = origin;
	remote_cuckoo.line_no = 1;
	remote_cuckoo.col_no = 1;
	ip_file = &remote_cuckoo;
	input_from_file = 0;

	init_syntax_tree(&cfgt);
	yyparse();
	cfgt.source.attr = CONF_SOURCE_NTPQ;
	cfgt.timestamp = time(NULL);
	cfgt.source.value.s = estrdup(stoa(remote_addr));

	DPRINTF(1, ("Finished Parsing!!\n"));

	save_and_apply_config_tree();

	input_from_file = 1;
}


/*
 * getconfig() - process startup configuration file e.g /etc/ntp.conf
 */
void
getconfig(
	int argc,
	char *argv[]
	)
{
	char line[MAXLINE];

#ifdef DEBUG
	atexit(free_all_config_trees);
#endif
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
	res_fp = NULL;
	ntp_syslogmask = NLOG_SYNCMASK; /* set more via logconfig */

	/*
	 * install a non default variable with this daemon version
	 */
	snprintf(line, sizeof(line), "daemon_version=\"%s\"", Version);
	set_sys_var(line, strlen(line)+1, RO);

	/*
	 * Set up for the first time step to install a variable showing
	 * which syscall is being used to step.
	 */
	set_tod_using = &ntpd_set_tod_using;

	/*
	 * On Windows, the variable has already been set, on the rest,
	 * initialize it to "UNKNOWN".
	 */
#ifndef SYS_WINNT
	strncpy(line, "settimeofday=\"UNKNOWN\"", sizeof(line));
	set_sys_var(line, strlen(line) + 1, RO);
#endif

	/*
	 * Initialize the loop.
	 */
	loop_config(LOOP_DRIFTINIT, 0.);

	getCmdOpts(argc, argv);

	init_syntax_tree(&cfgt);

	curr_include_level = 0;
	if (
		(fp[curr_include_level] = F_OPEN(FindConfig(config_file), "r")) == NULL
#ifdef HAVE_NETINFO
		/* If there is no config_file, try NetInfo. */
		&& check_netinfo && !(config_netinfo = get_netinfo_config())
#endif /* HAVE_NETINFO */
		) {
		msyslog(LOG_INFO, "getconfig: Couldn't open <%s>", FindConfig(config_file));
#ifndef SYS_WINNT
		io_open_sockets();

		return;
#else
		/* Under WinNT try alternate_config_file name, first NTP.CONF, then NTP.INI */

		if ((fp[curr_include_level] = F_OPEN(FindConfig(alt_config_file), "r")) == NULL) {

			/*
			 * Broadcast clients can sometimes run without
			 * a configuration file.
			 */
			msyslog(LOG_INFO, "getconfig: Couldn't open <%s>", FindConfig(alt_config_file));
			io_open_sockets();

			return;
		}
		cfgt.source.value.s = estrdup(alt_config_file);
#endif	/* SYS_WINNT */
	} else
		cfgt.source.value.s = estrdup(config_file);


	/*** BULK OF THE PARSER ***/
#ifdef DEBUG
	yydebug = !!(debug >= 5);
#endif
	ip_file = fp[curr_include_level];
	yyparse();
	
	DPRINTF(1, ("Finished Parsing!!\n"));

	cfgt.source.attr = CONF_SOURCE_FILE;	
	cfgt.timestamp = time(NULL);

	save_and_apply_config_tree();

	while (curr_include_level != -1)
		FCLOSE(fp[curr_include_level--]);

#ifdef HAVE_NETINFO
	if (config_netinfo)
		free_netinfo_config(config_netinfo);
#endif /* HAVE_NETINFO */

	/*
	printf("getconfig: res_fp <%p> call_resolver: %d", res_fp, call_resolver);
	*/

	if (res_fp != NULL) {
		if (call_resolver) {
			/*
			 * Need name resolution
			 */
			do_resolve_internal();
		}
	}
}


void
save_and_apply_config_tree(void)
{
	struct config_tree *ptree;
#ifndef SAVECONFIG
	struct config_tree *punlinked;
#endif

	/*
	 * Keep all the configuration trees applied since startup in
	 * a list that can be used to dump the configuration back to
	 * a text file.
	 */
	ptree = emalloc(sizeof(*ptree));
	memcpy(ptree, &cfgt, sizeof(*ptree));
	memset(&cfgt, 0, sizeof(cfgt));
	
	LINK_TAIL_SLIST(cfg_tree_history, ptree, link,
			struct config_tree);

#ifdef SAVECONFIG
	if (HAVE_OPT( SAVECONFIGQUIT )) {
		FILE *dumpfile;
		int err;
		int dumpfailed;

		dumpfile = fopen(OPT_ARG( SAVECONFIGQUIT ), "w");
		if (NULL == dumpfile) {
			err = errno;
			fprintf(stderr,
				"can not create save file %s, error %d %s\n",
				OPT_ARG( SAVECONFIGQUIT ), err,
				strerror(err));
			exit(err);
		}
		
		dumpfailed = dump_all_config_trees(dumpfile, 0);
		if (dumpfailed)
			fprintf(stderr,
				"--saveconfigquit %s error %d\n",
				OPT_ARG( SAVECONFIGQUIT ),
				dumpfailed);
		else
			fprintf(stderr,
				"configuration saved to %s\n",
				OPT_ARG( SAVECONFIGQUIT ));

		exit(dumpfailed);
	}
#endif	/* SAVECONFIG */

	/* The actual configuration done depends on whether we are configuring the
	 * simulator or the daemon. Perform a check and call the appropriate
	 * function as needed.
	 */

#ifndef SIM
	config_ntpd(ptree);
#else
	config_ntpdsim(ptree);
#endif

	/*
	 * With configure --disable-saveconfig, there's no use keeping
	 * the config tree around after application, so free it.
	 */
#ifndef SAVECONFIG
	UNLINK_SLIST(punlinked, cfg_tree_history, ptree, link,
		     struct config_tree);
	NTP_INSIST(punlinked == ptree);
	free_config_tree(ptree);
#endif
}


void
ntpd_set_tod_using(
	const char *which
	)
{
	char line[128];

	snprintf(line, sizeof(line), "settimeofday=\"%s\"", which);
	set_sys_var(line, strlen(line) + 1, RO);
}


/* FUNCTIONS COPIED FROM THE OLDER ntp_config.c
 * --------------------------------------------
 */


/*
 * get_pfxmatch - find value for prefixmatch
 * and update char * accordingly
 */
static u_int32
get_pfxmatch(
	const char **	pstr,
	struct masks *	m
	)
{
	while (m->name != NULL) {
		if (strncmp(*pstr, m->name, strlen(m->name)) == 0) {
			*pstr += strlen(m->name);
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
static u_int32
get_match(
	const char *	str,
	struct masks *	m
	)
{
	while (m->name != NULL) {
		if (strcmp(str, m->name) == 0)
			return m->mask;
		else
			m++;
	}
	return 0;
}

/*
 * get_logmask - build bitmask for ntp_syslogmask
 */
static u_int32
get_logmask(
	const char *	str
	)
{
	const char *	t;
	u_int32		offset;
	u_int32		mask;

	mask = get_match(str, logcfg_noclass_items);
	if (mask != 0)
		return mask;

	t = str;
	offset = get_pfxmatch(&t, logcfg_class);
	mask   = get_match(t, logcfg_class_items);

	if (mask)
		return mask << offset;
	else
		msyslog(LOG_ERR, "logconfig: '%s' not recognized - ignored",
			str);

	return 0;
}


#ifdef HAVE_NETINFO

/*
 * get_netinfo_config - find the nearest NetInfo domain with an ntp
 * configuration and initialize the configuration state.
 */
static struct netinfo_config_state *
get_netinfo_config(void)
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

	config = emalloc(sizeof(*config));
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
free_netinfo_config(
	struct netinfo_config_state *config
	)
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
		for (; prop_index < COUNTOF(keywords); prop_index++)
		{
			ni_namelist namelist;
			struct keyword current_prop = keywords[prop_index];
			ni_index index;

			/*
			 * For each value associated in the property, we're going to return
			 * a separate line. We squirrel away the values in the config state
			 * so the next time through, we don't need to do this lookup.
			 */
			NI_INIT(&namelist);
			if (NI_OK == ni_lookupprop(config->domain,
			    &config->config_dir, current_prop.text,
			    &namelist)) {

				/* Found the property, but it has no values */
				if (namelist.ni_namelist_len == 0) continue;

				config->val_list = 
				    emalloc(sizeof(char*) *
				    (namelist.ni_namelist_len + 1));
				val_list = config->val_list;

				for (index = 0;
				     index < namelist.ni_namelist_len;
				     index++) {
					char *value;
					
					value = namelist.ni_namelist_val[index];
					val_list[index] = estrdup(value);
				}
				val_list[index] = NULL;

				break;
			}
			ni_namelist_free(&namelist);
		}
		config->prop_index = prop_index;
	}

	/* No list; we're done here. */
	if (!val_list)
		return CONFIG_UNKNOWN;

	/*
	 * We have a list of values for the current property.
	 * Iterate through them and return each in order.
	 */
	if (val_list[val_index]) {
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
				while (ISSPACE(*tokens))
					tokens++;
				if (ISEOL(*tokens))
					break;
			}
		}

		if (ntok == MAXTOKENS) {
			/* HMS: chomp it to lose the EOL? */
			msyslog(LOG_ERR,
				"gettokens_netinfo: too many tokens.  Ignoring: %s",
				tokens);
		} else {
			*ntokens = ntok + 1;
		}

		config->val_index++;	/* HMS: Should this be in the 'else'? */

		return keywords[prop_index].keytype;
	}

	/* We're done with the current property. */
	prop_index = ++config->prop_index;

	/* Free val_list and reset counters. */
	for (val_index = 0; val_list[val_index]; val_index++)
		free(val_list[val_index]);
	free(val_list);
	val_list = config->val_list = NULL;
	val_index = config->val_index = 0;

	goto again;
}

#endif /* HAVE_NETINFO */

/*
 * getnetnum - return a net number (this is crude, but careful)
 *
 * returns 1 for success, and mysteriously, 0 or -1 for failure
 */
static int
getnetnum(
	const char *num,
	sockaddr_u *addr,
	int complain,
	enum gnn_type a_type
	)
{
	int retval;
	struct addrinfo *res;

	/* Get all the addresses that resolve to this name */
	retval = get_multiple_netnums(num, addr, &res, complain, a_type);

	if (retval != 1) {
		/* Name resolution failed */
		return retval;
	}

	memcpy(addr, res->ai_addr, res->ai_addrlen);

	DPRINTF(2, ("getnetnum given %s, got %s\n", num, stoa(addr)));

	freeaddrinfo(res);
	return 1;
}


/*
 * get_multiple_netnums
 *
 * returns 1 for success, and mysteriously, 0 or -1 for failure
 */
static int
get_multiple_netnums(
	const char *nameornum,
	sockaddr_u *addr,
	struct addrinfo **res,
	int complain,
	enum gnn_type a_type
	)
{
	char lookbuf[1024];
	const char *lookup;
	char *pch;
	struct addrinfo hints;
	struct addrinfo *ptr;
	int retval;
	sockaddr_u ipaddr;

	memset(&hints, 0, sizeof(hints));

	if (strlen(nameornum) >= sizeof(lookbuf)) {
		NTP_INSIST(strlen(nameornum) < sizeof(lookbuf));
		return 0;
	}

	lookup = nameornum;
	if (is_ip_address(nameornum, &ipaddr)) {
		hints.ai_flags = AI_NUMERICHOST;
		hints.ai_family = AF(&ipaddr);
		if ('[' == nameornum[0]) {
			lookup = lookbuf;
			strncpy(lookbuf, &nameornum[1],
				sizeof(lookbuf));
			pch = strchr(lookbuf, ']');
			if (pch != NULL)
				*pch = '\0';
		}
		pch = strchr(lookup, '%');
		if (pch != NULL) {
			if (lookup != lookbuf) {
				lookup = lookbuf;
				strncpy(lookbuf, nameornum,
					sizeof(lookbuf));
				pch = strchr(lookup, '%');
			}
			*pch = '\0';
		}
	}

	if (AF_INET6 == hints.ai_family && !ipv6_works)
		return 0;

	if (AF_UNSPEC == hints.ai_family) {
		if (!ipv6_works)
			hints.ai_family = AF_INET;
		else if (!ipv4_works)
			hints.ai_family = AF_INET6;
		else if (IS_IPV4(addr) || IS_IPV6(addr))
			hints.ai_family = AF(addr);
	}

	/* Get host address. Looking for UDP datagram connection */
	hints.ai_socktype = SOCK_DGRAM;

	DPRINTF(4, ("getaddrinfo %s%s\n", 
		    (AF_UNSPEC == hints.ai_family)
			? ""
			: (AF_INET == hints.ai_family)
				? "v4 "
				: "v6 ",
		    lookup));

	retval = getaddrinfo(lookup, "ntp", &hints, &ptr);

	if (retval || (AF_INET6 == ptr->ai_family && !ipv6_works)) {
		if (complain)
			msyslog(LOG_ERR,
				"getaddrinfo: \"%s\" invalid host address, ignored",
				lookup);
		else
			DPRINTF(1, ("getaddrinfo: \"%s\" invalid host address.\n",
				    lookup));

		if (!retval) {
			freeaddrinfo(ptr);
			return -1;
		} else {
			return 0;
		}
	}
	*res = ptr;

	return 1;
}


#if !defined(VMS) && !defined(SYS_WINNT)
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
	int no_needed,
	int type,
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
		strcpy(res_file, RES_TEMPFILE);
#else
		int len;

		/* no /tmp directory under NT */
		if (!GetTempPath(sizeof res_file, res_file)) {
			msyslog(LOG_ERR, "can not get temp dir: %m");
			exit(1);
		}
		
		len = strlen(res_file);
		if (sizeof res_file < len + sizeof "ntpdXXXXXX") {
			msyslog(LOG_ERR,
				"temporary directory path %s too long",
				res_file);
			exit(1);
		}

		memmove(res_file + len, "ntpdXXXXXX",
			sizeof "ntpdXXXXXX");
#endif /* SYS_WINNT */
#ifdef HAVE_MKSTEMP
		{
			int fd;

			res_fp = NULL;
			if ((fd = mkstemp(res_file)) != -1)
				res_fp = fdopen(fd, "r+");
		}
#else
		mktemp(res_file);
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

	(void)fprintf(res_fp, "%s %d %d %d %d %d %d %d %d %u %s\n",
		name, no_needed, type,
		mode, version, minpoll, maxpoll, flags, ttl, keyid, keystr);
#ifdef DEBUG
	if (debug > 1)
		printf("config: %s %d %d %d %d %d %d %x %d %u %s\n",
			name, no_needed, type,
			mode, version, minpoll, maxpoll, flags,
			ttl, keyid, keystr);
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
	if (unlink(res_file))
		msyslog(LOG_WARNING, 
			"Unable to remove temporary resolver file %s, %m",
			res_file);
#else
	(void) delete(res_file);
#endif /* VMS */
#endif /* SYS_VXWORKS */
}


/*
 * do_resolve_internal - start up the resolver function (not program)
 *
 * On VMS, VxWorks, and Unix-like systems lacking fork(), this routine
 * will simply refuse to resolve anything.
 *
 * Possible implementation: keep `res_file' in memory, do async
 * name resolution via QIO, update from within completion AST.
 * I'm unlikely to find the time for doing this, though. -wjm
 */
static void
do_resolve_internal(void)
{
#ifndef SYS_WINNT
	int i;
#endif

	if (res_fp == NULL) {
		/* belch */
		msyslog(LOG_ERR,
			"do_resolve_internal: Fatal: res_fp == NULL");
		exit(1);
	}

	/* we are done with this now */
	(void) fclose(res_fp);
	res_fp = NULL;

#ifndef NO_INTRES
	req_file = res_file;	/* set up pointer to res file */
#ifndef SYS_WINNT
	(void) signal_no_reset(SIGCHLD, catchchild);

	/* the parent process will write to the pipe
	 * in order to wake up to child process
	 * which may be waiting in a select() call
	 * on the read fd */
	if (pipe(resolver_pipe_fd) < 0) {
		msyslog(LOG_ERR,
			"unable to open resolver pipe");
		exit(1);
	}

	i = fork();
	/* Shouldn't the code below be re-ordered?
	 * I.e. first check if the fork() returned an error, then
	 * check whether we're parent or child.
	 *     Martin Burnicki
	 */
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

		/*
		msyslog(LOG_INFO, "do_resolve_internal: pre-closelog");
		*/
		closelog();
		kill_asyncio(0);

		(void) signal_no_reset(SIGCHLD, SIG_DFL);

		init_logging("ntpd_intres", 0);
		setup_logfile();
		/*
		msyslog(LOG_INFO, "do_resolve_internal: post-closelog");
		*/

		ntp_intres();

		/*
		 * If we got here, the intres code screwed up.
		 * Print something so we don't die without complaint
		 */
		msyslog(LOG_ERR, "call to ntp_intres lost");
		abort_resolve();
		exit(1);
	}
	if (i == -1) {
		msyslog(LOG_ERR, "fork() failed, can't start ntp_intres: %m");
		(void) signal_no_reset(SIGCHLD, SIG_DFL);
		abort_resolve();
	} else
		/* This is the parent process who will write to the pipe,
		 * so we close the read fd */
		close(resolver_pipe_fd[0]);
#else /* SYS_WINNT */
	{
		/* NT's equivalent of fork() is _spawn(), but the start point
		 * of the new process is an executable filename rather than
		 * a function name as desired here.
		 */
		unsigned thread_id;
		uintptr_t res_thd_handle;

		fflush(stdout);
		ResolverEventHandle = CreateEvent(NULL, FALSE, FALSE, NULL);
		if (ResolverEventHandle == NULL) {
			msyslog(LOG_ERR, "Unable to create resolver event object, can't start ntp_intres");
			abort_resolve();
		}
		res_thd_handle = _beginthreadex(
			NULL,			/* no security attributes	*/
			0,			/* use default stack size	*/
			ntp_intres_thread,	/* thread function		*/
			NULL,			/* argument to thread function	*/
			0,			/* use default creation flags	*/
			&thread_id);		/* receives thread identifier	*/
		if (!res_thd_handle) {
			msyslog(LOG_ERR, "_beginthreadex ntp_intres_thread failed %m");
			CloseHandle(ResolverEventHandle);
			ResolverEventHandle = NULL;
			abort_resolve();
		}
	}
#endif /* SYS_WINNT */
#else /* NO_INTRES follows */
	msyslog(LOG_ERR,
		"Deferred DNS not implemented - use numeric addresses");
	abort_resolve();
#endif
}
