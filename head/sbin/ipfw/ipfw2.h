/*
 * Copyright (c) 2002-2003 Luigi Rizzo
 * Copyright (c) 1996 Alex Nash, Paul Traina, Poul-Henning Kamp
 * Copyright (c) 1994 Ugen J.S.Antsilevich
 *
 * Idea and grammar partially left from:
 * Copyright (c) 1993 Daniel Boulet
 *
 * Redistribution and use in source forms, with and without modification,
 * are permitted provided that this entire comment appears intact.
 *
 * Redistribution in binary form may occur without any restrictions.
 * Obviously, it would be nice if you gave credit where credit is due
 * but requiring it would be too onerous.
 *
 * This software is provided ``AS IS'' without any warranties of any kind.
 *
 * NEW command line interface for IP firewall facility
 *
 * $FreeBSD$
 */

/*
 * Options that can be set on the command line.
 * When reading commands from a file, a subset of the options can also
 * be applied globally by specifying them before the file name.
 * After that, each line can contain its own option that changes
 * the global value.
 * XXX The context is not restored after each line.
 */

struct cmdline_opts {
	/* boolean options: */
	int	do_value_as_ip;	/* show table value as IP */
	int	do_resolv;	/* try to resolve all ip to names */
	int	do_time;	/* Show time stamps */
	int	do_quiet;	/* Be quiet in add and flush */
	int	do_pipe;	/* this cmd refers to a pipe/queue/sched */
	int	do_nat; 	/* this cmd refers to a nat config */
	int	do_dynamic;	/* display dynamic rules */
	int	do_expired;	/* display expired dynamic rules */
	int	do_compact;	/* show rules in compact mode */
	int	do_force;	/* do not ask for confirmation */
	int	show_sets;	/* display the set each rule belongs to */
	int	test_only;	/* only check syntax */
	int	comment_only;	/* only print action and comment */
	int	verbose;	/* be verbose on some commands */

	/* The options below can have multiple values. */

	int	do_sort;	/* field to sort results (0 = no) */
		/* valid fields are 1 and above */

	int	use_set;	/* work with specified set number */
		/* 0 means all sets, otherwise apply to set use_set - 1 */

};

extern struct cmdline_opts co;

/*
 * _s_x is a structure that stores a string <-> token pairs, used in
 * various places in the parser. Entries are stored in arrays,
 * with an entry with s=NULL as terminator.
 * The search routines are match_token() and match_value().
 * Often, an element with x=0 contains an error string.
 *
 */
struct _s_x {
	char const *s;
	int x;
};

enum tokens {
	TOK_NULL=0,

	TOK_OR,
	TOK_NOT,
	TOK_STARTBRACE,
	TOK_ENDBRACE,

	TOK_ACCEPT,
	TOK_COUNT,
	TOK_PIPE,
	TOK_LINK,
	TOK_QUEUE,
	TOK_FLOWSET,
	TOK_SCHED,
	TOK_DIVERT,
	TOK_TEE,
	TOK_NETGRAPH,
	TOK_NGTEE,
	TOK_FORWARD,
	TOK_SKIPTO,
	TOK_DENY,
	TOK_REJECT,
	TOK_RESET,
	TOK_UNREACH,
	TOK_CHECKSTATE,
	TOK_NAT,
	TOK_REASS,

	TOK_ALTQ,
	TOK_LOG,
	TOK_TAG,
	TOK_UNTAG,

	TOK_TAGGED,
	TOK_UID,
	TOK_GID,
	TOK_JAIL,
	TOK_IN,
	TOK_LIMIT,
	TOK_KEEPSTATE,
	TOK_LAYER2,
	TOK_OUT,
	TOK_DIVERTED,
	TOK_DIVERTEDLOOPBACK,
	TOK_DIVERTEDOUTPUT,
	TOK_XMIT,
	TOK_RECV,
	TOK_VIA,
	TOK_FRAG,
	TOK_IPOPTS,
	TOK_IPLEN,
	TOK_IPID,
	TOK_IPPRECEDENCE,
	TOK_DSCP,
	TOK_IPTOS,
	TOK_IPTTL,
	TOK_IPVER,
	TOK_ESTAB,
	TOK_SETUP,
	TOK_TCPDATALEN,
	TOK_TCPFLAGS,
	TOK_TCPOPTS,
	TOK_TCPSEQ,
	TOK_TCPACK,
	TOK_TCPWIN,
	TOK_ICMPTYPES,
	TOK_MAC,
	TOK_MACTYPE,
	TOK_VERREVPATH,
	TOK_VERSRCREACH,
	TOK_ANTISPOOF,
	TOK_IPSEC,
	TOK_COMMENT,

	TOK_PLR,
	TOK_NOERROR,
	TOK_BUCKETS,
	TOK_DSTIP,
	TOK_SRCIP,
	TOK_DSTPORT,
	TOK_SRCPORT,
	TOK_ALL,
	TOK_MASK,
	TOK_FLOW_MASK,
	TOK_SCHED_MASK,
	TOK_BW,
	TOK_DELAY,
	TOK_PROFILE,
	TOK_BURST,
	TOK_RED,
	TOK_GRED,
	TOK_DROPTAIL,
	TOK_PROTO,
	/* dummynet tokens */
	TOK_WEIGHT,
	TOK_LMAX,
	TOK_PRI,
	TOK_TYPE,
	TOK_SLOTSIZE,

	TOK_IP,
	TOK_IF,
 	TOK_ALOG,
 	TOK_DENY_INC,
 	TOK_SAME_PORTS,
 	TOK_UNREG_ONLY,
 	TOK_RESET_ADDR,
 	TOK_ALIAS_REV,
 	TOK_PROXY_ONLY,
	TOK_REDIR_ADDR,
	TOK_REDIR_PORT,
	TOK_REDIR_PROTO,	

	TOK_IPV6,
	TOK_FLOWID,
	TOK_ICMP6TYPES,
	TOK_EXT6HDR,
	TOK_DSTIP6,
	TOK_SRCIP6,

	TOK_IPV4,
	TOK_UNREACH6,
	TOK_RESET6,

	TOK_FIB,
	TOK_SETFIB,
	TOK_LOOKUP,
};
/*
 * the following macro returns an error message if we run out of
 * arguments.
 */
#define NEED(_p, msg)      {if (!_p) errx(EX_USAGE, msg);}
#define NEED1(msg)      {if (!(*av)) errx(EX_USAGE, msg);}

int pr_u64(uint64_t *pd, int width);

/* memory allocation support */
void *safe_calloc(size_t number, size_t size);
void *safe_realloc(void *ptr, size_t size);

/* string comparison functions used for historical compatibility */
int _substrcmp(const char *str1, const char* str2);
int _substrcmp2(const char *str1, const char* str2, const char* str3);

/* utility functions */
int match_token(struct _s_x *table, char *string);
char const *match_value(struct _s_x *p, int value);

int do_cmd(int optname, void *optval, uintptr_t optlen);

struct in6_addr;
void n2mask(struct in6_addr *mask, int n);
int contigmask(uint8_t *p, int len);

/*
 * Forward declarations to avoid include way too many headers.
 * C does not allow duplicated typedefs, so we use the base struct
 * that the typedef points to.
 * Should the typedefs use a different type, the compiler will
 * still detect the change when compiling the body of the
 * functions involved, so we do not lose error checking.
 */
struct _ipfw_insn;
struct _ipfw_insn_altq;
struct _ipfw_insn_u32;
struct _ipfw_insn_ip6;
struct _ipfw_insn_icmp6;

/*
 * The reserved set numer. This is a constant in ip_fw.h
 * but we store it in a variable so other files do not depend
 * in that header just for one constant.
 */
extern int resvd_set_number;

/* first-level command handlers */
void ipfw_add(char *av[]);
void ipfw_show_nat(int ac, char **av);
void ipfw_config_pipe(int ac, char **av);
void ipfw_config_nat(int ac, char **av);
void ipfw_sets_handler(char *av[]);
void ipfw_table_handler(int ac, char *av[]);
void ipfw_sysctl_handler(char *av[], int which);
void ipfw_delete(char *av[]);
void ipfw_flush(int force);
void ipfw_zero(int ac, char *av[], int optname);
void ipfw_list(int ac, char *av[], int show_counters);

/* altq.c */
void altq_set_enabled(int enabled);
u_int32_t altq_name_to_qid(const char *name);

void print_altq_cmd(struct _ipfw_insn_altq *altqptr);

/* dummynet.c */
void dummynet_list(int ac, char *av[], int show_counters);
void dummynet_flush(void);
int ipfw_delete_pipe(int pipe_or_queue, int n);

/* ipv6.c */
void print_unreach6_code(uint16_t code);
void print_ip6(struct _ipfw_insn_ip6 *cmd, char const *s);
void print_flow6id(struct _ipfw_insn_u32 *cmd);
void print_icmp6types(struct _ipfw_insn_u32 *cmd);
void print_ext6hdr(struct _ipfw_insn *cmd );

struct _ipfw_insn *add_srcip6(struct _ipfw_insn *cmd, char *av);
struct _ipfw_insn *add_dstip6(struct _ipfw_insn *cmd, char *av);

void fill_flow6(struct _ipfw_insn_u32 *cmd, char *av );
void fill_unreach6_code(u_short *codep, char *str);
void fill_icmp6types(struct _ipfw_insn_icmp6 *cmd, char *av);
int fill_ext6hdr(struct _ipfw_insn *cmd, char *av);
