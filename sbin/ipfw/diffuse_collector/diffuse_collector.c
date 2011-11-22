/*-
 * Copyright (c) 2010-2011
 * 	Swinburne University of Technology, Melbourne, Australia.
 * Copyright (c) 2011 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed at the Centre for Advanced Internet
 * Architectures, Swinburne University of Technology, by Sebastian Zander, made
 * possible in part by a gift from The Cisco University Research Program Fund, a
 * corporate advised fund of Silicon Valley Community Foundation.
 *
 * Portions of this software were developed at the Centre for Advanced
 * Internet Architectures, Swinburne University of Technology, Melbourne,
 * Australia by Lawrence Stewart under sponsorship from the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Description:
 * Rule/flow collector.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/tree.h>

#include <arpa/inet.h>

#include <net/if.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_fw.h>
#include <netinet/ip_diffuse.h>
#define	WITH_DIP_INFO
#include <netinet/ip_diffuse_export.h>
#include <netinet/sctp.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "../diffuse_ui.h"
#include "../diffuse_proto.h"

#define	STRLEN_LITERAL(literal) (sizeof((literal)) - 1)

#define	MAX_ERRORS_BEFORE_IGNORE	12

/* Flow/rule timeouts. */
/* XXX: Make configurable. */
#define	DEFAULT_TIMEOUT			60
#define	DEFAULT_TIMEOUT2		60

/* Max number of rules to use by default. */
#define	DEFAULT_MAX_RULES		1000

static const char *usage = "Usage: diffuse_collector [-hnv] "
    "[-c <collector-config>] [-s <sctp-port>] [-t <tcp-port>] [-u <udp-port>]";

static char *config_sections[] = {
#define	INI_SECTION_GENERAL		0
	"general",
#define	INI_SECTION_CLASSACTIONS	1
	"classactions",
#define	INI_SECTION_FIREWALL		2
	"firewall"
};
#define	NUM_INI_SECTIONS (sizeof(config_sections) / sizeof(*config_sections))

RB_GENERATE(di_template_head, di_template, node, template_compare);

/* Classifier node. */
struct class_node {
	int			proto;
	struct in_addr		ip;
	uint16_t		port;
	int			sock;
	int			closed;
	int			errors;
	struct di_template_head	templ_list;
	LIST_ENTRY(class_node)	next;
};

LIST_HEAD(class_node_head, class_node);
static struct class_node_head cnodes; /* Accepted TCP or SCTP clients. */
static struct class_node_head mains; /* Main ports (max one for each proto). */
static int cnode_cnt;

/* List of flow classes per flow entry. */
struct flow_class {
	char			cname[DI_MAX_NAME_STR_LEN];
	uint16_t		class;
	SLIST_ENTRY(flow_class)	next;
};

SLIST_HEAD(flow_class_head, flow_class);

struct rule_entry;

struct timeout_entry {
	struct rule_entry		*rule;
	LIST_ENTRY(timeout_entry)	next;
};

LIST_HEAD(timeout_entry_head, timeout_entry);

/*
 * Firewall rule numbers to use when adding rules. This is essentially a free
 * list using a RB tree, so that the latest rule always gets the lowest number.
 */
struct rule_no {
	uint16_t		no;
	RB_ENTRY(rule_no)	node;
};

static inline int
rule_no_compare(struct rule_no *a, struct rule_no *b)
{

	return ((a->no != b->no) ? (a->no < b->no ? -1 : 1) : 0);
}

RB_HEAD(rule_no_head, rule_no);
static RB_PROTOTYPE(rule_no_head, rule_no, node, rule_no_compare);
static RB_GENERATE(rule_no_head, rule_no, node, rule_no_compare);

struct rule_entry {
	struct class_node	*cnode;		/* Sender/generator of rule. */
	char			export_set[DI_MAX_NAME_STR_LEN];
	struct ipfw_flow_id	id;		/* (masked) flow id. */
	char			action[DI_MAX_NAME_STR_LEN];
	char			act_params[DI_MAX_PARAM_STR_LEN];
	uint64_t		pcnt;           /* Packets matched counter. */
	uint64_t		bcnt;           /* Bytes matched counter. */
	uint8_t			expire_type;	/* Rule vs. flow timeout. */
	uint32_t		expire;         /* Expire timeout. */
	uint32_t		bucket;         /* Which hash table bucket. */
	uint16_t		rtype;          /* {Bi|Uni}directional. */
	uint16_t		tcnt;		/* Number of tags. */
	struct flow_class_head	flow_classes;	/* List of class tags. */
	struct timeout_entry	*to;		/* Ptr to timeout list entry. */
	struct rule_no		*rule_no;	/* Firewall rule number. */
	struct rule_entry	*next;		/* Next rule in list. */
};

#define	DEFAULT_RULE_TABLE_SIZE	4096
static int rule_table_size = DEFAULT_RULE_TABLE_SIZE;
static struct rule_entry **rule_table;

#define	DEFAULT_TIMEOUT_SIZE	512
static struct timeout_entry_head timeouts[DEFAULT_TIMEOUT_SIZE];
static uint16_t timeout_now;

static struct rule_no_head rule_nos;
#define	DEFAULT_MIN_RULE_NO	1000
#define	DEFAULT_MAX_RULE_NO	2000
#define	FW_MAX_RULE_NO		65535

/* Firewall rule actions. */
#define	FW_ADD_RULE		1 /* Add rule. */
#define	FW_DEL_RULE		2 /* Delete rule. */
#define	FW_GET_RULE_COUNTERS	3 /* Get rule counters. */

/* How many rule counters are gathered by the FW_GET_RULE_COUNTERS action. */
#define	NUM_RULE_COUNTERS	2

/*
 * Array indexes for rule counters gathered by the FW_GET_RULE_COUNTERS
 * action.
 */
#define	RULE_COUNTER_NPKTS	0
#define	RULE_COUNTER_NBYTES	1

struct fw_action {
	FILE			*file;
	int			type;
	struct rule_entry	*rule;
	TAILQ_ENTRY(fw_action)	next;
};
TAILQ_HEAD(fw_action_head, fw_action);

static struct fw_action_head fw_actions;

/*
 * Local list of classes and actions to execute. Overrides actions send by
 * classifier node.
 */
struct class_action {
	char			cname[DI_MAX_NAME_STR_LEN];
	uint16_t		class;
	char			action[DI_MAX_NAME_STR_LEN];
	char			act_params[DI_MAX_PARAM_STR_LEN];
	RB_ENTRY(class_action)	node;
};

static inline int
class_action_compare(struct class_action *a, struct class_action *b)
{
	int cc;

	/* Sort by classes first as its potentially quicker. */
	cc = a->class != b->class ? (a->class < b->class ? -1 : 1) : 0;
	if (cc != 0)
		return (cc);
	else
		return (strcmp(a->cname, b->cname));
}

RB_HEAD(class_action_head, class_action);
static RB_PROTOTYPE(class_action_head, class_action, node,
    class_action_compare);
static RB_GENERATE(class_action_head, class_action, node, class_action_compare);

static struct class_action_head class_actions;
static struct class_action def_action = { "", 0, "count", "" };

struct pair {
	char	token[32];
	char	value[64];
};

/* Template tokens available in config file for substitution. */
static struct pair tokenpairs[] = {
	{ "action",	"" },
#define	TOK_ACTION	0
	{ "exec",	"" },
#define	TOK_EXEC	1
	{ "dstip",	"" },
#define	TOK_DSTIP	2
	{ "dstport",	"" },
#define	TOK_DSTPORT	3
	{ "keepstate",	"" },
#define	TOK_KEEPSTATE	4
	{ "proto",	"" },
#define	TOK_PROTO	5
	{ "ruleno",	"" },
#define	TOK_RULENO	6
	{ "srcip",	"" },
#define	TOK_SRCIP	7
	{ "srcport",	"" }
#define	TOK_SRCPORT	8
};

#define	NUM_TOKEN_PAIRS (sizeof(tokenpairs) / sizeof(tokenpairs[0]))

struct di_collector_config {
	char	cfpath[PATH_MAX];		/* Config file path. */
	char	fw_exe[PATH_MAX];		/* Firewall executable path. */
	char	cli_add_rule[1024];		/* Shell command to execute when
						 * adding a rule. */
	char	cli_del_rule[1024];		/* Shell command to execute when
						 * deleting a rule. */
	char	cli_get_rule_counters[1024];	/* Shell command to execute
						 * to get the required rule
						 * counters. */
	struct sockaddr *classifiers;		/* Details for classifiers. */
	int	num_classifiers;		/* Size of classifiers array. */
	int	min_rule_no;			/* Minimum rule number available
						 * for use by colelctor. */
	int	max_rule_no;			/* Maximum rule number available
						 * for use by colelctor. */
	uint8_t	fw_rule_mgmt_type;		/* Rule management method. */
	uint8_t	verbose;			/* If non-zero, write runtime
						 * info to console. */
	uint8_t	no_fw;				/* Don't execute firewall
						 * commands. */
	uint8_t	fw_default_action;		/* Default specified? */
};

static struct di_collector_config config;

#define	FW_CLI_RULE_MGMT	1
#define	FW_FILE_RULE_MGMT	2

/* Global exit flag. */
static int stop;

static void
print_collector_config(struct di_collector_config *conf)
{
	struct sockaddr *cdetails;
	int i;

	printf("%-25s %s\n", "Config file path", conf->cfpath);
	printf("%-25s %s\n", "Firewall executable path", conf->fw_exe);
	printf("%-25s %s\n", "Add rule cmd", conf->cli_add_rule);
	printf("%-25s %s\n", "Delete rule cmd", conf->cli_del_rule);
	printf("%-25s %s\n", "Get rule counters cmd",
	    conf->cli_get_rule_counters);
	printf("%-25s %s\n", "Firewall rule mgmt type", "FW_CLI_RULE_MGMT");
	printf("%-25s %d\n", "Min rule number", conf->min_rule_no);
	printf("%-25s %d\n", "Max rule number", conf->max_rule_no);

	for (i = 0; i < conf->num_classifiers; i++) {
		cdetails = &conf->classifiers[i];
		printf("Details for classifier %d: %s;%u\n", i + 1,
		    inet_ntoa(((struct sockaddr_in *)cdetails)->sin_addr),
		    ntohs(((struct sockaddr_in *)cdetails)->sin_port));
	}
}

static void
expand_tokenised_str(char const *instr, char *outstr, int outlen,
    struct pair const *pairs, int npairs)
{
#define	TOK_LHS_SEP "@@"		/* Left Hand Side token delimiter. */
#define	TOK_RHS_SEP TOK_LHS_SEP		/* Right Hand Side token delimiter. */

	char *tmp, *tok;
	int i, nchars;

	/*
	 * Keep looping until we've reached the end of instr or we've filled
	 * outstr to capacity.
	 */
	while (*instr != '\0' && outlen > 1) {
		tok = strstr(instr, TOK_LHS_SEP);
		/*
		 * Copy all chars from the current position of instr up to the
		 * token separator into outstr. If no token separator was found,
		 * instr has no tokens to replace so copy entire string.
		 */
		nchars = (tok == NULL ? outlen - 1 : tok - instr);
		if (nchars >= outlen)
			nchars = outlen - 1;
		tmp = stpncpy(outstr, instr, nchars);
		nchars = tmp - outstr;
		outlen -= nchars;
		outstr = tmp;
		/* stpncpy() doesn't always NULL-terminate. */
		*outstr = '\0';

		if (tok != NULL && outlen > 0) {
			/* Move instr to point to start of token. */
			instr = tok + STRLEN_LITERAL(TOK_LHS_SEP);

			/* Find the end-of-token delimter. */
			tok = strstr(instr, TOK_RHS_SEP);
			if (tok != NULL) {
				/*
				 * Found delimiter, so match token in pairs
				 * array and replace with value if found.
				 */
				for (i = 0; i < npairs; i++) {
					if (strncmp(pairs[i].token, instr,
					    tok - instr) == 0) {
						nchars = snprintf(outstr,
						    outlen, "%s",
						    pairs[i].value);
						outlen -= nchars;
						outstr += nchars;
					}
				}
				/*
				 * Finished substitution for this token. Set
				 * nchars such that the token and end-of-token
				 * delimter in instr will be skipped over next
				 * time through the loop.
				 */
				nchars = (tok - instr) +
				    STRLEN_LITERAL(TOK_RHS_SEP);
			} else {
				/*
				 * No end-of-token delimiter found after the
				 * start-of-token delimiter. Most likely an
				 * error in instr, but treat as though the
				 * start-of-token delimiter we found is regular
				 * text and copy to outstr all chars from
				 * start-of-token delimiter through to end of
				 * instr.
				 */

				/*
				 * Backtrack instr so we copy the start-of-token
				 * delimter.
				 */
				instr -= STRLEN_LITERAL(TOK_LHS_SEP);

				/*
				 * Copy as many chars from instr to
				 * outstr as outlen will allow, leaving
				 * room for the '\0'.
				 */
				tmp = stpncpy(outstr, instr, outlen - 1);
				nchars = tmp - outstr;
				outlen -= nchars;
				outstr = tmp;
				/* stpncpy() doesn't always NULL-terminate. */
				*outstr = '\0';
			}
		}

		/*
		 * Shuffle instr along. If we made a substitution this time
		 * through the loop, this will position instr just after the
		 * end-of-token delimiter of the token we just processed. If no
		 * substitution was made, this should position instr at its
		 * NULL-terminator.
		 */
		instr += nchars;
	}
}

/*
 * Execute a shell command in a non-blocking pipe and return the new file
 * descriptor.
 */
static FILE *
exec(char *command)
{
	FILE *f;

	if (config.no_fw)
		command = "test";
	if (config.verbose)
		printf("Executing shell command: %s\n", command);
	f = popen(command, "r");
	fcntl(fileno(f), F_SETFL, O_NONBLOCK);

	return (f);
}

/*
 * Build firewall command, exec using a pipe and add pipe fd to our file
 * descriptor set to check for completion at a later point.
 */
static void
exec_fw(struct rule_entry *r, int type, fd_set *rset, int *max_fd,
    int no_race_check)
{
	struct fw_action *action, *cur, *tmp;
	struct in_addr addr;
	FILE *f;
	char buf[256];
	int fd;

	/* Avoid race conditions between commands for one rule. */
	if (!no_race_check) {
		TAILQ_FOREACH_SAFE(cur, &fw_actions, next, tmp) {
			if (cur->rule != r)
				continue;

			if (type == FW_GET_RULE_COUNTERS &&
			    (cur->type == FW_DEL_RULE ||
			    cur->type == FW_ADD_RULE)) {
				/*
				 * Don't get rule counters if add/delete
				 * is already in progress.
				 */
				return;
			} else if (type == FW_DEL_RULE &&
			    cur->type == FW_GET_RULE_COUNTERS) {
				/* Cancel get rule counters command. */
				TAILQ_REMOVE(&fw_actions, cur, next);
				pclose(cur->file);
				free(cur);
			}
		}
	}

	if (config.fw_rule_mgmt_type == FW_CLI_RULE_MGMT) {
		/*
		 * Fill in our token-value pairs that will be used for
		 * substitution in the appropriate tokenised string.
		 */
		strlcpy(tokenpairs[TOK_ACTION].value, r->action,
		    sizeof(tokenpairs[TOK_ACTION].value));
		addr.s_addr = r->id.dst_ip;
		sprintf(tokenpairs[TOK_DSTIP].value, "%s",
		    inet_ntoa(addr));
		sprintf(tokenpairs[TOK_DSTPORT].value, "%u", r->id.dst_port);

		if (r->rtype == DI_ACTION_TYPE_BIDIRECTIONAL) {
			strlcpy(tokenpairs[TOK_KEEPSTATE].value, "keep-state",
			    sizeof(tokenpairs[TOK_KEEPSTATE].value));
		} else {
			tokenpairs[TOK_KEEPSTATE].value[0] = '\0';
		}

		sprintf(tokenpairs[TOK_PROTO].value, "%u", r->id.proto);
		sprintf(tokenpairs[TOK_RULENO].value, "%u", r->rule_no->no);
		addr.s_addr = r->id.src_ip;
		sprintf(tokenpairs[TOK_SRCIP].value, "%s",
		    inet_ntoa(addr));
		sprintf(tokenpairs[TOK_SRCPORT].value, "%u", r->id.src_port);

		switch (type) {
		case FW_ADD_RULE:
			expand_tokenised_str(config.cli_add_rule, buf,
			    sizeof(buf), tokenpairs, NUM_TOKEN_PAIRS);
			break;

		case FW_DEL_RULE:
			expand_tokenised_str(config.cli_del_rule, buf,
			    sizeof(buf), tokenpairs, NUM_TOKEN_PAIRS);
			break;

		case FW_GET_RULE_COUNTERS:
			expand_tokenised_str(config.cli_get_rule_counters, buf,
			    sizeof(buf), tokenpairs, NUM_TOKEN_PAIRS);
			break;
		}

	} else if (config.fw_rule_mgmt_type == FW_FILE_RULE_MGMT) {
		/*
		 * XXX: Add support for firewalls which use file-based rule
		 * management.
		 */
	}

	DID("fw command %s", buf);
	f = exec(buf);

	if (f != NULL) {
		action = (struct fw_action *)malloc(sizeof(struct fw_action));
		if (action == NULL)
			errx(EX_OSERR, "can't alloc mem for fw_action");
		action->file = f;
		action->type = type;
		action->rule = r;
		TAILQ_INSERT_TAIL(&fw_actions, action, next);
		fd = fileno(action->file);
		FD_SET(fd, rset);
		if (fd > *max_fd)
			*max_fd = fd;
	}

	/* Output handling is done in process_fwaction_sockets(). */
}

static void
free_fw_actions(void)
{
	struct fw_action *a;

	while (!TAILQ_EMPTY(&fw_actions)) {
		a = TAILQ_FIRST(&fw_actions);
		TAILQ_REMOVE(&fw_actions, a, next);
		pclose(a->file);
		free(a);
	}
}

static void
free_rule_nos(void)
{
	struct rule_no *r, *n;

	for (r = RB_MIN(rule_no_head, &rule_nos); r != NULL; r = n) {
		n = RB_NEXT(rule_no_head, &rule_nos, r);
		RB_REMOVE(rule_no_head, &rule_nos, r);
		free(r);
	}
}

static void
free_class_actions(void)
{
	struct class_action *r, *n;

	for (r = RB_MIN(class_action_head, &class_actions); r != NULL; r = n) {
		n = RB_NEXT(class_action_head, &class_actions, r);
		RB_REMOVE(class_action_head, &class_actions, r);
		free(r);
	}
}

static inline uint32_t
hash_packet(struct ipfw_flow_id *id)
{
	uint32_t hash;

	hash = id->dst_ip ^ id->src_ip ^ id->dst_port ^ id->src_port;
	hash &= (rule_table_size - 1);

	return (hash);
}

/* Find rule in hash table. */
struct rule_entry *
find_rule(struct ipfw_flow_id *f, struct rule_entry **prev)
{
	struct rule_entry *_prev, *q;
	uint32_t h;

	_prev = q = NULL;
	h = hash_packet(f);

	for (q = rule_table[h]; q != NULL;) {
		if (f->proto == q->id.proto) {
			if (IS_IP6_FLOW_ID(f)) {
				if (IN6_ARE_ADDR_EQUAL(&f->src_ip6,
				    &q->id.src_ip6) &&
				    IN6_ARE_ADDR_EQUAL(&f->dst_ip6,
				    &q->id.dst_ip6) &&
				    f->src_port == q->id.src_port &&
				    f->dst_port == q->id.dst_port) {
					break;
				}
				if (q->rtype & DI_FLOW_TYPE_BIDIRECTIONAL) {
					if (IN6_ARE_ADDR_EQUAL(&f->src_ip6,
					    &q->id.dst_ip6) &&
					    IN6_ARE_ADDR_EQUAL(&f->dst_ip6,
					    &q->id.src_ip6) &&
					    f->src_port == q->id.dst_port &&
					    f->dst_port == q->id.src_port) {
						break;
					}
				}
			} else {
				if (f->src_ip == q->id.src_ip &&
				    f->dst_ip == q->id.dst_ip &&
				    f->src_port == q->id.src_port &&
				    f->dst_port == q->id.dst_port) {
					break;
				}
				if (q->rtype & DI_FLOW_TYPE_BIDIRECTIONAL) {
					if (f->src_ip == q->id.dst_ip &&
					    f->dst_ip == q->id.src_ip &&
					    f->src_port == q->id.dst_port &&
					    f->dst_port == q->id.src_port) {
						break;
					}
				}
			}
		}
		_prev = q;
		q = q->next;
	}

	if (q == NULL)
		goto done;

	if (_prev != NULL) {
		/* Found and not in front. */
		_prev->next = q->next;
		q->next = rule_table[h];
		rule_table[h] = q;
		_prev = NULL;
	}

done:
	*prev = _prev;

	return (q);
}

static void
free_rule(struct rule_entry *r)
{
	struct flow_class *s;

	while (!SLIST_EMPTY(&r->flow_classes)) {
		s = SLIST_FIRST(&r->flow_classes);
		SLIST_REMOVE_HEAD(&r->flow_classes, next);
		free(s);
	}

	if (r->to) {
		LIST_REMOVE(r->to, next);
		free(r->to);
	}

	free(r->rule_no);
	free(r);
}

/* Remove rule from hash table and free rule memory if free is set. */
static void
remove_rule(struct rule_entry *r, struct rule_entry *prev, int free)
{

	if (prev != NULL)
		prev->next = r->next;
	else
		rule_table[r->bucket] = r->next;

	if (free)
		free_rule(r);
}

/* Find rule and remove if it exists. */
static struct rule_entry *
find_remove_rule(struct rule_entry *n, int free)
{
	struct rule_entry *prev, *r;

	prev = NULL;

	r = find_rule(&n->id, &prev);
	if (r != NULL) {
		remove_rule(r, prev, free);
		if (free)
			r = NULL;
	}

	return (r);
}

static void
free_rules(fd_set *rset, int *max_fd)
{
	struct rule_entry *next, *q;
	int i;

	for (i = 0; i < rule_table_size; i++) {
		for (q = rule_table[i]; q != NULL ; ) {
			next = q->next;
			exec_fw(q, FW_DEL_RULE, rset, max_fd, 1);
			remove_rule(q, NULL, 1);
			q = next;
		}
	}
	free(rule_table);
}

/* Add rule in table, add timeout and return pointer to new entry. */
static struct rule_entry *
add_rule(struct rule_entry *n)
{
	struct rule_no *no;
	int h;
	uint16_t t;

	DID2("add rule");

	if (RB_EMPTY(&rule_nos))
		return (NULL); /* XXX: Do more? e.g. get rid of old rules? */

	h = hash_packet(&n->id);
	n->bucket = h;
	n->next = rule_table[h];
	rule_table[h] = n;
	no = RB_MIN(rule_no_head, &rule_nos);
	RB_REMOVE(rule_no_head, &rule_nos, no);
	n->rule_no = no;
	t = (timeout_now + n->expire) & (DEFAULT_TIMEOUT_SIZE - 1);
	n->to = (struct timeout_entry *)malloc(sizeof(struct timeout_entry));
	if (n->to == NULL)
		errx(EX_OSERR, "can't alloc mem for a timeout_entry");
	n->to->rule = n;
	LIST_INSERT_HEAD(&timeouts[t], n->to, next);

	return (n);
}

static struct rule_entry *
update_timeout(struct rule_entry *n, struct rule_entry *r)
{
	uint16_t t;

	DID2("update timeout");

	r->expire_type = n->expire_type;
	r->expire = n->expire;
	t = (timeout_now + r->expire) & (DEFAULT_TIMEOUT_SIZE - 1);
	LIST_REMOVE(r->to, next);
	LIST_INSERT_HEAD(&timeouts[t], r->to, next);

	return (r);
}

/* Set action based on class and set timeout. */
static void
set_action_timeout(struct rule_entry *n)
{
	struct class_action s, *r;
        struct flow_class *c;

	r = NULL;

	/*
	 * Select action in this priority:
	 * 1. Locally defined for first class that has one
	 * 2. Locally defined default action
	 * 3. Defined by classifier node
	 * 4. Build-in default action
	 */

	SLIST_FOREACH(c, &n->flow_classes, next) {
		strcpy(s.cname, c->cname); /* XXX: Fix so no need to copy. */
		s.class = c->class;
		r = RB_FIND(class_action_head, &class_actions, &s);
		if (r) {
			strcpy(n->action, r->action);
			strcpy(n->act_params, r->act_params);
			break;
		}
	}

	if (!r && (config.fw_default_action || !strlen(n->action))) {
		/* Apply default action. */
		strcpy(n->action, def_action.action);
		strcpy(n->act_params, def_action.act_params);
	}

	if (n->expire == 0)
		n->expire = DEFAULT_TIMEOUT;

	if (n->expire_type == DIP_TIMEOUT_NONE)
		n->expire_type = DIP_TIMEOUT_FLOW; /* Local timeout. */

	/*
	 * If we use flow timeouts the classifier node may set a very long
	 * timeout, e.g. TCP, but the flow may end much quicker. Action node's
	 * firewall times out flow according to packets (e.g. tcp flags), but
	 * collector doesn't now.
	 * Solution: do more regular checking.
	 */
	/* XXX: Get firewall expire value. May work for keep-state flows. */
	if (n->expire_type == DIP_TIMEOUT_FLOW &&
	    n->expire > DEFAULT_TIMEOUT2) {
		n->expire = DEFAULT_TIMEOUT2;
	}
}

/* Parse rule and remove/add it in hash table and fw. */
static int
parse_rule(struct class_node *cnode, struct di_template *t, char *rb,
    fd_set *rset, int *max_fd)
{
	struct flow_class *c;
	struct rule_entry *n, *prev, *r;
	int dlen, i, offs, toffs, type;

	offs = 0;
	type = -1;

	DID2("parse rule");

	n = (struct rule_entry *)calloc(1, sizeof(struct rule_entry));
	if (n == NULL)
		errx(EX_OSERR, "can't alloc mem for a rule_entry");

	n->cnode = cnode;
	SLIST_INIT(&n->flow_classes);

	for (i = 0; i < t->fcnt; i++) {
		DID2("field %u(%u)\n", t->fields[i].id, offs);

		if (t->fields[i].len == -1) {
			/* Read dynamic length. */
			dlen = *((unsigned char *)(rb + offs));
			offs++;

			switch(t->fields[i].idx) {
			case DIP_IE_CLASSES:
				while (offs - toffs < dlen - 1) {
					c = (struct flow_class *)malloc(
					    sizeof(struct flow_class));
					if (c == NULL)
						errx(EX_OSERR, "can't alloc "
						    "mem for a flow_class");
					strncpy(c->cname, rb + offs,
					    DI_MAX_NAME_STR_LEN);
					c->cname[DI_MAX_NAME_STR_LEN - 1] =
					    '\0';
					offs += strlen(c->cname) + 1;
					c->class =
					    ntohs(*((uint16_t *)(rb + offs)));
					offs += sizeof(uint16_t);
					SLIST_INSERT_HEAD(&n->flow_classes, c,
					    next);
				}
				break;

			default:
				offs += dlen - 1;
			}
		} else {
			switch(t->fields[i].id) {
			case DIP_IE_SRC_IPV4:
				n->id.src_ip = *((uint32_t *)(rb + offs));
				n->id.addr_type = 4;
				break;

			case DIP_IE_DST_IPV4:
				n->id.dst_ip = *((uint32_t *)(rb + offs));
				n->id.addr_type = 4;
				break;

			case DIP_IE_SRC_PORT:
				n->id.src_port = ntohs(*((uint16_t *)(rb + offs)));
				break;

			case DIP_IE_DST_PORT:
				n->id.dst_port = ntohs(*((uint16_t *)(rb + offs)));
				break;

			case DIP_IE_PROTO:
				n->id.proto = *((uint8_t *)(rb + offs));
				break;

			case DIP_IE_TIMEOUT_TYPE:
				n->expire_type = *((uint8_t *)(rb + offs));
				break;

			case DIP_IE_TIMEOUT:
				n->expire = ntohs(*((uint16_t *)(rb + offs)));
				break;

			case DIP_IE_EXPORT_NAME:
				strncpy(n->export_set, rb + offs,
				    DI_MAX_NAME_STR_LEN);
				n->export_set[DI_MAX_NAME_STR_LEN - 1] = '\0';
				break;

			case DIP_IE_ACTION:
				strncpy(n->action, rb + offs,
				    DI_MAX_NAME_STR_LEN);
				n->action[DI_MAX_NAME_STR_LEN - 1] = '\0';
				break;

			case DIP_IE_ACTION_FLAGS:
				n->rtype = ntohs(*((uint16_t *)(rb + offs)));
				break;

			case DIP_IE_ACTION_PARAMS:
				strncpy(n->act_params, rb + offs,
				    DI_MAX_NAME_STR_LEN);
				n->act_params[DI_MAX_NAME_STR_LEN - 1] = '\0';
				break;

			case DIP_IE_MSG_TYPE:
				type = *((uint8_t *)(rb + offs));
				break;
			}

			offs += t->fields[i].len;
		}
	}

	if (type == DIP_MSG_REMOVE) {
		r = find_remove_rule(n, 0);
		if (r != NULL)
			exec_fw(r, FW_DEL_RULE, rset, max_fd, 0);
		free_rule(n);
	} else if (type == DIP_MSG_ADD) {
		set_action_timeout(n);
		prev = NULL;
		r = find_rule(&n->id, &prev);

		if (r == NULL) {
			/* Add new rule. */
			if (add_rule(n))
				exec_fw(n, FW_ADD_RULE, rset, max_fd, 0);
			else
				free_rule(n);
		} else {
			/* Update rule, including action if it differs. */
			if (strcmp(n->action, r->action) ||
			    strcmp(n->act_params, r->act_params) ||
			    n->rtype != r->rtype) {
				/*
				 * XXX: Not a very good way of "changing a
				 * rule", but IPFW does not support the concept
				 * of updating an existing rule. Other firewalls
				 * might be better at this, but for IPFW at
				 * least, we could have two actual rules per
				 * received rule - the first with matching part
				 * and skipto, second with any and action.
				 * Alternatively, could modify IPFW.
				 */

				remove_rule(r, prev, 0);
				if (add_rule(n)) {
					exec_fw(n, FW_ADD_RULE, rset, max_fd,
					    0);
				} else {
					free_rule(n);
				}
				exec_fw(r, FW_DEL_RULE, rset, max_fd, 0);
			} else {
				if (n->expire_type != r->expire_type ||
				    n->expire != r->expire) {
					update_timeout(n, r);
				}
				free_rule(n);
			}
		}
	} else {
		free_rule(n);
	}

	return (offs);
}

/* Parse rule and call add, delete etc. */
static void
parse_msg(struct class_node *cnode, char *buf, fd_set *rset, int *max_fd)
{
	static uint32_t last_seq = 0;
	struct dip_header *hdr;
	struct dip_info_descr info;
	struct dip_set_header *shdr;
	struct dip_templ_header *thdr;
	struct di_template s, *r;
	int offs, toffs;
	uint32_t seq;
	uint16_t ver;

	hdr = (struct dip_header *)buf;
	ver = ntohs(hdr->version);
	seq = ntohl(hdr->seq_no);
	offs = 0;

	DID2("parse msg");

	/* Version defined in ip_diffuse_export.h. */
	if (ver != DIP_VERSION)
		return;

	/* Ignore out of sequence messages. */
	if (last_seq > 0 && seq <= last_seq) {
		last_seq = seq;
		return;
	}
	last_seq = seq;

	offs += sizeof(struct dip_header);

	while (offs < ntohs(hdr->msg_len)) {
		shdr = (struct dip_set_header *)(buf + offs);
		offs += sizeof(struct dip_set_header);

		DID2("set %u len %u", ntohs(shdr->set_id), ntohs(shdr->set_len));

		if (ntohs(shdr->set_id) <= DIP_SET_ID_FLOWRULE_TPL) {
			/* Process template. */
			thdr = (struct dip_templ_header *)(buf + offs);
			offs += sizeof(struct dip_templ_header);

			s.id = ntohs(thdr->templ_id);
			r = RB_FIND(di_template_head, &cnode->templ_list, &s);

			if (r == NULL) {
				/* Store template. */
				toffs = offs;
				r = (struct di_template *)malloc(
				    sizeof(struct di_template));
				if (r == NULL)
					errx(EX_OSERR, "can't alloc mem for a "
					    "di_template");

				memset(r, 0, sizeof(struct di_template));
				r->id = s.id;

				while (offs - toffs < ntohs(shdr->set_len) -
				    sizeof(struct dip_set_header) -
				    sizeof(struct dip_templ_header)) {
					r->fields[r->fcnt].id = ntohs(
					    *((uint16_t *)(buf + offs)));
					offs += sizeof(uint16_t);
					info = diffuse_proto_get_info(
					    r->fields[r->fcnt].id);
					r->fields[r->fcnt].idx = info.idx;
					r->fields[r->fcnt].len = info.len;
					if (r->fields[r->fcnt].len == 0) {
						r->fields[r->fcnt].len =
						    ntohs(*((uint16_t *)(buf +
						    offs)));
						offs += sizeof(uint16_t);
					}
					r->fcnt++;
				}
				RB_INSERT(di_template_head, &cnode->templ_list, r);
			} else {
				offs += ntohs(shdr->set_len) -
				    sizeof(struct dip_set_header) -
				    sizeof(struct dip_templ_header);
			}
		} else if (ntohs(shdr->set_id) >= DIP_SET_ID_DATA) {
			s.id = ntohs(shdr->set_id);
			r = RB_FIND(di_template_head, &cnode->templ_list, &s);

			if (r == NULL) {
				DID2("missing template %u!", s.id);
				offs += ntohs(shdr->set_len) -
				    sizeof(struct dip_set_header);
			} else {
				toffs = offs;

				while (offs - toffs < ntohs(shdr->set_len) -
				    sizeof(struct dip_set_header)) {
					offs += parse_rule(cnode, r, buf + offs,
					    rset, max_fd);
				}
			}
		} else {
			DID2("unknown set type");
			offs += ntohs(shdr->set_len);
		}
	}
}

static void
handle_msg(struct class_node *s, char *buf, int n, fd_set *rset, int *max_fd)
{
#ifdef DIFFUSE_DEBUG2
	int i;

	printf("message %u\n", n);
	for (i = 0; i < n; i++)
		printf("%u ", (uint8_t)buf[i]);
	printf("\n");
#endif
	if (config.verbose)
		diffuse_proto_print_msg(buf, &s->templ_list);

	parse_msg(s, buf, rset, max_fd);
}

static void
check_timeouts(fd_set *rset, int *max_fd)
{
	static uint32_t last_time = 0;
	struct timeout_entry *t, *tmp;
	struct timeval now;
	int i;

	DID2("timeouts");

	gettimeofday(&now, NULL);

	/* First time through? */
	if (last_time == 0) {
		last_time = now.tv_sec;
		return;
	}

	/* For each timeout in timeouts[now], remove rule. */
	if (now.tv_sec - last_time <= 0)
		return;

	for (i = 0; i < now.tv_sec - last_time; i++) {
		LIST_FOREACH_SAFE(t, &timeouts[timeout_now], next, tmp) {
			if (t->rule->expire_type == DIP_TIMEOUT_RULE) {
				LIST_REMOVE(t, next);
				find_remove_rule(t->rule, 0);
				exec_fw(t->rule, FW_DEL_RULE, rset, max_fd, 0);
			} else if (t->rule->expire_type == DIP_TIMEOUT_FLOW) {
				/* Check counters. */
				exec_fw(t->rule, FW_GET_RULE_COUNTERS, rset,
				    max_fd, 0);
			}
		}
		timeout_now = (timeout_now + 1) & (DEFAULT_TIMEOUT_SIZE - 1);
	}

	last_time = now.tv_sec;
}

static void
close_cnode_socket(struct class_node *cnode)
{
	struct sctp_sndrcvinfo sinfo;

	if (cnode->proto == IPPROTO_SCTP) {
		sinfo.sinfo_flags = SCTP_EOF;
		sctp_send(cnode->sock, NULL, 0, &sinfo, 0);
	}

	close(cnode->sock);
}

static void
remove_cnode(struct class_node *cnode)
{
	struct di_template *r, *n;

	LIST_REMOVE(cnode, next);
	for (r = RB_MIN(di_template_head, &cnode->templ_list); r != NULL;
	    r = n) {
		n = RB_NEXT(di_template_head, &cnode->templ_list, r);
		RB_REMOVE(di_template_head, &cnode->templ_list, r);
		free(r);
	}
	free(cnode);
}

static void
free_cnodes(struct class_node_head *cn_list)
{
	struct class_node *s;

	while (!LIST_EMPTY(cn_list)) {
		s = LIST_FIRST(cn_list);
		close_cnode_socket(s);
		remove_cnode(s);
	}
}

/* Open socket for action node. */
static int
init_cnode(struct class_node *cnode)
{
	struct sctp_initmsg initmsg;
	struct sockaddr_in sin;
	int type;
	int optval;

	optval = 1;

	if (cnode->proto == IPPROTO_UDP)
		type = SOCK_DGRAM;
	else
		type = SOCK_STREAM;

	if ((cnode->sock = socket(AF_INET, type, cnode->proto)) < 0)
		errx(EX_OSERR, "create class socket: %s", strerror(errno));

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port  = htons(cnode->port);
	sin.sin_addr.s_addr = INADDR_ANY;

	if (bind(cnode->sock, (struct sockaddr *)&sin, sizeof(sin)) < 0)
		errx(EX_OSERR, "bind class socket: %s", strerror(errno));

	if (setsockopt(cnode->sock, SOL_SOCKET, SO_REUSEADDR, &optval,
	    sizeof(optval)) < 0) {
		errx(EX_OSERR, "set sock opt reuse");
	}

	if (cnode->proto == IPPROTO_SCTP) {
		/* Must have two streams. */
		memset(&initmsg, 0, sizeof(initmsg));
		initmsg.sinit_max_instreams = 2;
		initmsg.sinit_num_ostreams = 2;

		if (setsockopt(cnode->sock, IPPROTO_SCTP, SCTP_INITMSG, &initmsg,
		    sizeof(initmsg))) {
			errx(EX_OSERR, "set sock option initmsg");
		}
	}

	if (cnode->proto != IPPROTO_UDP) {
		if (listen(cnode->sock, 10) < 0) {
			errx(EX_OSERR, "listen class node socket: %s",
			    strerror(errno));
		}
	}

	cnode->closed = 1;
	cnode->errors = 0;
	RB_INIT(&cnode->templ_list);

	printf("Listening on %s port %d\n",
	    (cnode->proto == IPPROTO_UDP) ? "udp" :
	    ((cnode->proto == IPPROTO_TCP) ? "tcp" : "sctp"),
	    cnode->port);

	return (cnode->sock);
}

static void
parse_rule_nos(char *optarg)
{
	char tmp[128];
	char *errptr, *p;

	strncpy(tmp, optarg, sizeof(tmp));
	p = strstr(tmp, "-");

	if (p != NULL) {
		*p = '\0';
		p++;
	}

	config.min_rule_no = strtonum(tmp, 1, FW_MAX_RULE_NO,
	    (const char **)&errptr);
	if (errptr != NULL) {
		errx(EX_USAGE, "error parsing rule numbers '%s': %s",
		    tmp, errptr);
	}

	if (p != NULL) {
		config.max_rule_no = strtonum(p, 1, FW_MAX_RULE_NO,
		    (const char **)&errptr);
		if (errptr != NULL) {
			errx(EX_USAGE, "error parsing rule numbers '%s': %s",
			    p, errptr);
		}

		if (config.max_rule_no <= config.min_rule_no) {
			errx(EX_USAGE, "max rule number must be larger than "
			    "min rule number");
		}
	} else {
		config.max_rule_no = config.min_rule_no + DEFAULT_MAX_RULES;
		if (config.max_rule_no > FW_MAX_RULE_NO)
			config.max_rule_no = FW_MAX_RULE_NO;
	}
}

static void
parse_firewall_config(int line_no, char *key, char *value)
{

	if (strcmp(key, "exec") == 0) {
		strlcpy(config.fw_exe, value, sizeof(config.fw_exe));
		strlcpy(tokenpairs[TOK_EXEC].value, config.fw_exe,
		    sizeof(tokenpairs[TOK_EXEC].value));

	} else if (strcmp(key, "cli_add_rule") == 0) {
		strlcpy(config.cli_add_rule, value,
		    sizeof(config.cli_add_rule));

	} else if (strcmp(key, "cli_del_rule") == 0) {
		strlcpy(config.cli_del_rule, value,
		    sizeof(config.cli_del_rule));

	} else if (strcmp(key, "cli_get_rule_counters") == 0) {
		strlcpy(config.cli_get_rule_counters, value,
		    sizeof(config.cli_get_rule_counters));

	} else if (strcmp(key, "rule_mgmt") == 0) {
		if (strcmp(value, "cli") == 0) {
			config.fw_rule_mgmt_type = FW_CLI_RULE_MGMT;
		} else {
			errx(EX_CONFIG, "'cli' is the only firewall rule "
			    "management type currently supported.");
		}

	} else if (strcmp(key, "rule_nums") == 0) {
		parse_rule_nos(value);

	} else {
		errx(EX_CONFIG, "unknown config in firewall section");
	}
}

static void
parse_classaction_config(int line_no, char *key, char *value)
{
	struct class_action *classact;
	char *action, *action_params, *class, *cnode, *errstr;
	int class_num;

	if (strcmp(key, "action") != 0)
		errx(EX_CONFIG, "\"action\" must be used as the key");

	class = cnode = strsep(&value, " \t\r\n");
	strsep(&class, ":");
	action_params = value;
	action = strsep(&action_params, " \t\r\n");

	if (strcmp(cnode, "default") == 0) {
		if (class != NULL)
			errx(EX_CONFIG, "default action should not have a "
			    "class number specified");
		strlcpy(def_action.action, action, sizeof(def_action.action));
		*def_action.act_params = '\0';
		if (action_params != NULL) {
			strlcpy(def_action.act_params, action_params,
			    sizeof(def_action.act_params));
		}
		config.fw_default_action = 1;

	} else {
		class_num = strtonum(class, 0, 65535, (const char **)&errstr);
		if (errstr != NULL)
			errx(EX_CONFIG, "can't parse class number");

		if (cnode == NULL || strlen(cnode) == 0 || action == NULL ||
		    strlen(action) == 0) {
			errx(EX_USAGE, "action format must be "
			    "<classifier>:<class> <action> [<action-param>]");
		}

		classact = (struct class_action *)malloc(
		    sizeof(struct class_action));
		if (classact == NULL)
			errx(EX_OSERR, "can't alloc mem for a class_action");

		strlcpy(classact->cname, cnode, DI_MAX_NAME_STR_LEN);
		classact->class = class_num;
		strlcpy(classact->action, action, DI_MAX_NAME_STR_LEN);
		*classact->act_params = '\0';
		if (action_params != NULL) {
			strlcpy(classact->act_params, action_params,
			    DI_MAX_NAME_STR_LEN);
		}
		RB_INSERT(class_action_head, &class_actions, classact);
	}
}

static void
parse_general_config(int line_no, char *key, char *value)
{
	struct sockaddr *cdetails;
	char *errstr, *ip, *port;
	int nclassifiers;

	if (strcmp(key, "classifiers") == 0) {
		config.classifiers = NULL;
		nclassifiers = 0;
		do {
			ip = strsep(&value, " ");
			port = ip;
			ip = strsep(&port, ";");

			if (strlen(ip) == 0)
				continue; /* Skip extra space between IPs. */

			config.classifiers = realloc(config.classifiers,
			    ++nclassifiers * sizeof(struct sockaddr_storage));
			cdetails = &config.classifiers[nclassifiers - 1];
			bzero(cdetails, sizeof(struct sockaddr_storage));
			if (inet_pton(AF_INET, ip,
			    &((struct sockaddr_in *)cdetails)->sin_addr) < 1) {
				/* XXX: Try AF_INET6 on fail. */
				errx(EX_CONFIG, "can't parse IP address: %s",
				    ip);
			}
			cdetails->sa_family = AF_INET;

			if (port != NULL) {
				((struct sockaddr_in *)cdetails)->sin_port =
				    htons(strtonum(port, 0, 65535,
				    (const char **)&errstr));

				if (errstr != NULL) {
					errx(EX_CONFIG,
					    "can't parse port number: %s",
					    port);
				}
			} else {
				((struct sockaddr_in *)cdetails)->sin_port =
				    htons(DI_EXPORTER_DEFAULT_LISTEN_PORT);
			}
		} while (value != NULL);

		config.num_classifiers = nclassifiers;
	}
}

static void
parse_config_file(char *cfpath)
{
	FILE *f;
	char line[1024];
	char *key, *p, *val;
	int cursection, i, line_no;

	line_no = 0;
	cursection = INI_SECTION_GENERAL;

	f = fopen(cfpath, "r");
	if (f == NULL)
		errx(EX_USAGE, "can't load config file %s", cfpath);

	while (fgets(line, sizeof(line), f)) {
		line_no++;
		p = line;

		/* Remove leading whitespace from line. */
		while (isspace(*p))
			p++;
		/* Remove trailing whitespace from line. */
		i = strlen(p) - 1;
		while (i >= 0 && isspace(p[i]))
			p[i--] = '\0';
		/* Skip comments and whitespace-only lines. */
		if (!strlen(p) || !sscanf(p, "%[^\r\n]\n", p) || *p == '#' ||
		    *p == ';')
			continue;

		if (*p == '[') {
			/* Parse section name e.g. "[blah]". */
			p += 1; /* Make line point to char after '['. */
			if (p[strlen(p) - 1] != ']')
				errx(EX_CONFIG, "invalid section format");
			p[strlen(p) - 1] = '\0'; /* Remove trailing ']'. */

			for (i = 0, cursection = -1; i < NUM_INI_SECTIONS;
			    i++) {
				if (strcmp(p, config_sections[i]) == 0) {
					cursection = i;
					break;
				}
			}

			if (cursection == -1) {
				errx(EX_CONFIG, "invalid config section \"%s\"",
				    p);
			}
		} else {
			/* Parsing a "property" line e.g. "key = val". */
			key = val = p;
			strsep(&val, "=");
			if (val == NULL) {
				errx(EX_CONFIG, "config line %d is invalid",
				    line_no);
			}

			/* Remove trailing whitespace from key. */
			i = strlen(key) - 1;
			while (i >= 0 && isspace(key[i]))
				key[i--] = '\0';

			/* Remove leading whitespace from val. */
			i = 0;
			while (isspace(val[i]))
				val += 1;

			if (strlen(key) == 0 || strlen(val) == 0) {
				errx(EX_CONFIG, "config line %d is invalid",
				    line_no);
			}

			switch (cursection) {
			case INI_SECTION_GENERAL:
				parse_general_config(line_no, key, val);
				break;

			case INI_SECTION_FIREWALL:
				parse_firewall_config(line_no, key, val);
				break;

			case INI_SECTION_CLASSACTIONS:
				parse_classaction_config(line_no, key, val);
				break;
			}
		}
	}

	fclose(f);

	if (config.verbose)
		print_collector_config(&config);
}

static inline void
add_main(int proto, char *port)
{
	struct class_node *cnode;
	char *errptr;

	cnode = (struct class_node *)malloc(sizeof(struct class_node));
	if (cnode == NULL)
		errx(EX_OSERR, "can't alloc mem for a main node");

	cnode->proto = proto;
	cnode->port = strtonum(port, 1, 65535, (const char **)&errptr);
	if (errptr != NULL) {
		errx(EX_USAGE, "error parsing %s port '%s': %s",
			(proto == IPPROTO_UDP) ? "udp" :
			((proto == IPPROTO_TCP) ? "tcp" : "sctp"), port,
			errptr);
	}
	init_cnode(cnode);
	LIST_INSERT_HEAD(&mains, cnode, next);
}

/* Returns 0 on failure. */
static int
parse_rule_counters(char *buf, int64_t *rule_counters)
{
	const char *sep = ",";
	char *tok, *endptr;
	int count;

	DID2("fw get rule counters cmd returned: %s", buf);

	for (tok = strtok(buf, sep), count = 0;
	    tok != NULL && count < NUM_RULE_COUNTERS;
	    tok = strtok(NULL, sep), count++) {
		rule_counters[count] = strtoll(tok, &endptr, 10);
		if (rule_counters[count] == 0 && errno == EINVAL)
			break;
	}

	return (count == NUM_RULE_COUNTERS && *endptr == '\0');
}

static inline void
process_main_sockets(fd_set *rset, fd_set *_rset, int *max_fd, char *buf,
    int buflen)
{
	struct class_node *cnode, *newcnode;
	struct sctp_status status;
	struct sockaddr_in *cdetails, client_addr;
	socklen_t len;
	ssize_t nbytes;
	int client_sock, i;

	/*
	 * Loop through all "main" sockets and handle any required work.
	 * For TCP/SCTP main sockets, work means accepting a new connection on
	 * and adding the new socket to the cnodes list. For UDP main sockets,
	 * work simply means reading the datagram and handling it.
	 */
	LIST_FOREACH(cnode, &mains, next) {
		if (!FD_ISSET(cnode->sock, _rset))
			continue;

		if (cnode->proto == IPPROTO_UDP) {
			nbytes = recvfrom(cnode->sock, buf, buflen, 0,
			    (struct sockaddr *)&client_addr, &len);
			for (i = 0; i < config.num_classifiers; i++) {
				cdetails = (struct sockaddr_in *)&config.classifiers[i];
				/* Accept packets from known classifiers. */
				if (bcmp(&cdetails->sin_addr.s_addr,
				    &client_addr.sin_addr.s_addr,
				    sizeof(cdetails->sin_addr.s_addr)) == 0) {
					handle_msg(cnode, buf, nbytes, rset,
					    max_fd);
				}
			}
		} else {
			/* TCP or SCTP. */
			len = sizeof(client_addr);
			client_sock = accept(cnode->sock,
			    (struct sockaddr *)&client_addr, &len);
			if (client_sock == -1)
				continue;

			for (i = 0; i < config.num_classifiers; i++) {
				cdetails = (struct sockaddr_in *)&config.classifiers[i];
				/* Accept packets from known classifiers. */
				if (bcmp(&cdetails->sin_addr.s_addr,
				    &client_addr.sin_addr.s_addr,
				    sizeof(cdetails->sin_addr.s_addr)) == 0) {
					break;
				} else {
					cdetails = NULL;
				}
			}

			if (cdetails == NULL)
				close(client_sock); /* Unknown classifier. */

			if (cnode->proto == IPPROTO_SCTP) {
				memset(&status, 0, sizeof(status));
				len = sizeof(status);

				if (getsockopt(client_sock, IPPROTO_SCTP,
				    SCTP_STATUS, &status, &len) == -1) {
					errx(EX_OSERR, "get sock option "
					    "status: %s", strerror(errno));
				}
				if (status.sstat_instrms < 2 ||
				    status.sstat_outstrms < 2) {
					close(client_sock);
				}
			}

			/* Add new cnode to the "connected" list. */
			newcnode = (struct class_node *)malloc(
			    sizeof(struct class_node));
			if (newcnode == NULL) {
				errx(EX_OSERR,
				    "can't alloc mem for classifier node");
			}
			newcnode->proto = cnode->proto;
			newcnode->sock = client_sock;
			newcnode->closed = 0;
			newcnode->errors = 0;
			RB_INIT(&newcnode->templ_list);
			LIST_INSERT_HEAD(&cnodes, newcnode, next);

			if (client_sock > *max_fd)
				*max_fd = client_sock;
			FD_SET(client_sock, rset);
		}
	}
}

static inline void
process_fwaction_sockets(fd_set *rset, fd_set *_rset, int *max_fd, char *buf,
    int buflen)
{
	struct fw_action *action, *tmp;
	ssize_t nbytes;
	int64_t rule_counters[NUM_RULE_COUNTERS];
	int fd;
	uint16_t t;

	/*
	 * Loop through all pending firewall actions to see if any descriptors
	 * have completed and possibly produced a result for us to consume.
	 */
	TAILQ_FOREACH_SAFE(action, &fw_actions, next, tmp) {
		fd = fileno(action->file);
		if (!FD_ISSET(fd, _rset))
			continue;

		DID2("fw fd action %d", action->type);

		switch(action->type) {
		case FW_ADD_RULE:
			/*
			 * Assume we got the correct rule number, so do nothing.
			 */
			break;

		case FW_DEL_RULE:
			/* Put rule number back in free list. */
			DID("delete rule no %d", action->rule->rule_no->no);
			RB_INSERT(rule_no_head, &rule_nos, action->rule->rule_no);
			action->rule->rule_no = NULL;
			free_rule(action->rule);
			break;

		case FW_GET_RULE_COUNTERS:
			/* Expecting text which we need to NULL-terminate. */
			nbytes = read(fd, &buf, buflen);
			if (nbytes > 0 && nbytes < buflen) {
				buf[nbytes] = '\0';
				if (parse_rule_counters(buf, rule_counters) &&
				    rule_counters[RULE_COUNTER_NPKTS] >
				    action->rule->pcnt) {
					/*
					 * Rule has matched packets since the
					 * last time we checked, so reset the
					 * timeout and leave as is.
					 */
					action->rule->pcnt =
					    rule_counters[RULE_COUNTER_NPKTS];
					action->rule->bcnt =
					    rule_counters[RULE_COUNTER_NBYTES];
					t = (timeout_now +
					    action->rule->expire) &
					    (DEFAULT_TIMEOUT_SIZE - 1);
					LIST_REMOVE(action->rule->to, next);
					LIST_INSERT_HEAD(&timeouts[t],
					    action->rule->to, next);
					break;
				}
			}
			/* Rule is old or an error occurred, so remove rule. */
			find_remove_rule(action->rule, 0);
			exec_fw(action->rule, FW_DEL_RULE, rset, max_fd,
			    1);
			break;
		}

		pclose(action->file);
		FD_CLR(fd, rset);
		TAILQ_REMOVE(&fw_actions, action, next);
		free(action);
	}
}

static inline void
process_connected_sockets(fd_set *rset, fd_set *_rset, int *max_fd, char *buf,
    int buflen)
{
	struct class_node *cnode, *tmp;
	struct sctp_sndrcvinfo sinfo;
	struct sockaddr_in client_addr;
	socklen_t len;
	ssize_t nbytes;
	int flags;

	/* Nodes with connected sockets. */
	LIST_FOREACH_SAFE(cnode, &cnodes, next, tmp) {
		if (FD_ISSET(cnode->sock, _rset)) {
			if (cnode->sock == IPPROTO_SCTP) {
				len = sizeof(client_addr);
				bzero(&sinfo, sizeof(sinfo));
				nbytes = sctp_recvmsg(cnode->sock, &buf,
				    sizeof(buf),
				    (struct sockaddr *)&client_addr, &len,
				    &sinfo, &flags);

				/* XXX: Handle SCTP events? */
			} else {
				nbytes = read(cnode->sock, buf, buflen);
			}

			if (nbytes == 0) {
				close_cnode_socket(cnode);
				remove_cnode(cnode);
				continue;
			}

			handle_msg(cnode, buf, nbytes, rset, max_fd);
		}
	}
}

static void
run(void)
{
	fd_set rset, wset, _rset, _wset;
	struct class_node *cnode;
	struct timeval tv;
	char buf[IP_MAXPACKET];
	int count, max_fd;

	tv.tv_sec = 1;
	tv.tv_usec = 0;
	max_fd = 0;

	/* XXX: max_fd is only increased, but never decreased. */
	LIST_FOREACH(cnode, &mains, next) {
		if (cnode->sock > max_fd)
			max_fd = cnode->sock;
	}

	FD_ZERO(&rset);
	FD_ZERO(&wset);
	LIST_FOREACH(cnode, &mains, next) {
		FD_SET(cnode->sock, &rset);
	}

	do {
		_rset = rset;
		_wset = wset;

		if ((count = select(max_fd + 1, &_rset, &_wset, NULL, &tv)) <
		    0) {
			if (errno != EINTR)
				errx(EX_OSERR, "select error");
		}

		check_timeouts(&rset, &max_fd);
		if (count <= 0)
			continue;

		process_main_sockets(&rset, &_rset, &max_fd, buf, sizeof(buf));
		process_fwaction_sockets(&rset, &_rset, &max_fd, buf,
		    sizeof(buf));
		process_connected_sockets(&rset, &_rset, &max_fd, buf,
		    sizeof(buf));
	} while (!stop);

	free_cnodes(&mains);
	free_cnodes(&cnodes);
	free_rules(&rset, &max_fd);
	free_fw_actions();
	free_rule_nos();
	free_class_actions();
}

static void
request_classifier_state()
{
	struct dip_header *hdr;
	struct dip_set_header *shdr;
	struct dip_templ_header *thdr;
	struct sockaddr *cdetails;
	char reqstatepkt[64];
	int i, offs, ret, tmpsock;

	bzero(reqstatepkt, sizeof(reqstatepkt));

	/* Prepare the state request packet. */
	hdr = (struct dip_header *)reqstatepkt;
	offs = sizeof(struct dip_header);

	shdr = (struct dip_set_header *)(reqstatepkt + offs);
	shdr->set_id = htons((uint16_t)DIP_SET_ID_CMD_TPL);
	offs += sizeof(struct dip_set_header);

	thdr = (struct dip_templ_header *)(reqstatepkt +
	    offs);
	thdr->templ_id = htons((uint16_t)DIP_SET_ID_DATA);
	thdr->flags = 0;
	offs += sizeof(struct dip_templ_header);

	*((uint16_t *)(reqstatepkt + offs)) =
	    htons(dip_info[DIP_IE_MSG_TYPE].id);
	offs += sizeof(uint16_t);
	shdr->set_len = htons(offs - sizeof(struct dip_header));

	shdr = (struct dip_set_header *)(reqstatepkt + offs);
	shdr->set_id = htons((uint16_t)DIP_SET_ID_DATA);
	offs += sizeof(struct dip_set_header);
	
	*((uint8_t *)(reqstatepkt + offs)) = DIP_MSG_REQSTATE;
	offs += sizeof(uint8_t);

	shdr->set_len = htons(sizeof(struct dip_set_header) + sizeof(uint8_t));
	hdr->version = htons((uint16_t)DIP_VERSION);
	hdr->msg_len = htons(offs);
	hdr->seq_no = htonl((uint32_t)1);
	/*
	 * XXX: We may want to set and make use of the timestamp field at some
	 * point.
	 */

	/* Send the state request packet to all configured classifier nodes. */
	for (i = 0; i < config.num_classifiers; i++) {
		cdetails = &config.classifiers[i];

		tmpsock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (tmpsock == -1) {
			printf("WARNING: Unable to create socket to send state "
			    "request packet to classifier %s;%u\n", inet_ntoa(
			    ((struct sockaddr_in *)cdetails)->sin_addr),
			    ntohs(((struct sockaddr_in *)cdetails)->sin_port));
			continue;
		}

		ret = connect(tmpsock, cdetails, cdetails->sa_family == AF_INET ?
		    sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6));
		if (ret != 0) {
			printf("WARNING: Unable to connect to classifier "
			    "%s;%u\n", inet_ntoa(
			    ((struct sockaddr_in *)cdetails)->sin_addr),
			    ntohs(((struct sockaddr_in *)cdetails)->sin_port));
			close(tmpsock);
			continue;
		}

		ret = send(tmpsock, reqstatepkt, offs, MSG_EOF);
		if (ret == -1 || ret != offs) {
			printf("WARNING: Unable to successfully transmit state "
			    "request packet to classifier %s;%u\n", inet_ntoa(
			    ((struct sockaddr_in *)cdetails)->sin_addr),
			    ntohs(((struct sockaddr_in *)cdetails)->sin_port));
		}

		close(tmpsock);
	}
}

/* run() will terminate when stop is set to 1. */
static void
sigint_handler(int i)
{

	stop = 1;
}

int
main(int argc, char *argv[])
{
	struct class_node *cnode;
	struct rule_no *r;
	int ch, i;

	LIST_INIT(&mains);
	LIST_INIT(&cnodes);
	RB_INIT(&rule_nos);
	RB_INIT(&class_actions);
	TAILQ_INIT(&fw_actions);

	if (argc < 1)
		errx(EX_USAGE, "%s\n", usage);

	while ((ch = getopt(argc, argv, "c:hns:t:u:v")) != EOF) {
		switch (ch) {
		case 'c':
			strlcpy(config.cfpath, optarg, sizeof(config.cfpath));
			break;

		case 'h':
			errx(0, "%s\n", usage);
			break;

		case 'n':
			config.no_fw++;
			break;

		case 's':
			add_main(IPPROTO_SCTP, optarg);
			break;

		case 't':
			add_main(IPPROTO_TCP, optarg);
			break;

		case 'u':
			add_main(IPPROTO_UDP, optarg);
			break;

		case 'v':
			config.verbose++;
			break;

		default:
			errx(EX_USAGE, "%s\n", usage);
			break;
		}
	}

	parse_config_file(config.cfpath);

	request_classifier_state();

	if (LIST_EMPTY(&mains)) {
		/* Use default UDP port. */
		cnode = (struct class_node *)malloc(sizeof(*cnode));
		if (cnode == NULL)
			errx(EX_OSERR, "can't alloc mem for classifier node");
		cnode->proto = IPPROTO_UDP;
		cnode->port = DI_COLLECTOR_DEFAULT_LISTEN_PORT;
		init_cnode(cnode);
		LIST_INSERT_HEAD(&mains, cnode, next);
	}

	rule_table = (struct rule_entry **)calloc(rule_table_size,
	    sizeof(struct rule_entry));
	if (rule_table == NULL)
		errx(EX_OSERR, "can't alloc mem for rule table");

	if (geteuid() != 0) {
		printf("WARNING: can't run firewall commands as non-root\n");
		config.no_fw++;
	}

	for (i = config.min_rule_no; i <= config.max_rule_no; i++) {
		r = (struct rule_no *)malloc(sizeof(struct rule_no));
		if (r == NULL)
			errx(EX_OSERR, "can't alloc mem for rule_no");
		r->no = i;
		RB_INSERT(rule_no_head, &rule_nos, r);
	}

	/* Install signal handlers. */
	signal(SIGINT, sigint_handler);
	signal(SIGTERM, sigint_handler);

	/* Run the packet and rule processing loop. */
	run();

	return (0);
}
