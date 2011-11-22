/*-
 * Copyright (c) 2010-2011
 * 	Swinburne University of Technology, Melbourne, Australia.
 * All rights reserved.
 *
 * This software was developed at the Centre for Advanced Internet
 * Architectures, Swinburne University of Technology, by Sebastian Zander, made
 * possible in part by a gift from The Cisco University Research Program Fund, a
 * corporate advised fund of Silicon Valley Community Foundation.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/tree.h>
#include <sys/types.h>

#include <arpa/inet.h>

#include <net/if.h>

#include <netinet/in.h>
#include <netinet/ip_fw.h>
#include <netinet/ip_diffuse.h>
#include <netinet/ip_diffuse_export.h>

#include <netinet/ipfw/diffuse_common.h>
#include <netinet/ipfw/diffuse_user_compat.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <math.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

#include "diffuse_ui.h"
#include "diffuse_proto.h"
#include "ipfw2.h"

/* Takes an *ipfw_insn as cmd. */
#define	GENERATE_FEATURES(cmd) do {					\
	if (!features) {						\
		(cmd)->opcode = O_DI_FEATURES_IMPLICIT;			\
		fill_features((ipfw_insn_features *)(cmd), NULL);	\
		features = (ipfw_insn_features *)(cmd);			\
		(cmd) = next_cmd(cmd);					\
	}								\
} while(0)

/* New tokens used by DIFFUSE in rule sets. */
static struct di_option diffuse_main_params[] = {
	{ "module",		DI_OPTION_ARG_STR,	0, 0,
	    TOK_DI_MODULE },
	{ "algorithm",		DI_OPTION_ARG_STR,	0, 0,
	    TOK_DI_ALGORITHM },
	{ "use-feature-stats",	DI_OPTION_ARG_STR,	0, 0,
	    TOK_DI_USE_FEATURE_STATS },
	{ "class-names",	DI_OPTION_ARG_STR,	0, 0,
	    TOK_DI_CLASS_NAMES },
	{ "expired",		DI_OPTION_ARG_NOARG,	0, 0,
	    TOK_DI_EXPIRED },
	{ "target",		DI_OPTION_ARG_STR,	0, 0,
	    TOK_DI_EXP_TARGET },
	{ "confirm",		DI_OPTION_ARG_UINT,	1, 65535,
	    TOK_DI_CONFIRM },
	{ "min-batch",		DI_OPTION_ARG_UINT,	1, 65535,
	    TOK_DI_EXP_MIN_BATCH },
	{ "max-batch",		DI_OPTION_ARG_UINT,	1, 65535,
	    TOK_DI_EXP_MAX_BATCH },
	{ "max-delay",		DI_OPTION_ARG_UINT,	1, 65535,
	    TOK_DI_EXP_MAX_DELAY },
	{ "flow-key",		DI_OPTION_ARG_STR,	0, 0,
	    TOK_DI_EXP_FLOW_KEY },
	{ "features",		DI_OPTION_ARG_STR,	0, 0,
	    TOK_DI_EXP_FEATURES },
	{ "action",		DI_OPTION_ARG_STR,	0, 0,
	    TOK_DI_EXP_ACTION },
	{ "action-params",	DI_OPTION_ARG_STR,	0, 0,
	    TOK_DI_EXP_ACTION_PARAMS },
	{ "unidirectional",	DI_OPTION_ARG_NOARG,	0, 0,
	    TOK_DI_EXP_ACTION_UNIDIR },
	{ NULL, 0, 0, 0, 0 }
};

/* Main parameters plus parameters of registered features/classifiers. */
static struct di_option *diffuse_all_params = NULL;

/* Globals for parsing functions. */
ipfw_insn_features *features = NULL;		/* Ptr to features cmd. */
ipfw_insn_ml_classify *classifier = NULL;	/* Ptr to classify action. */
int have_feature_match;

/*
 * Merge feature module params (called from module).
 * opts must be a NULL terminated array of struct di_option.
 */
int
diffuse_add_params(struct di_option *opts, int size)
{
	struct di_option *o;
	int old;

	if (diffuse_all_params == NULL) {
		diffuse_all_params = (struct di_option *)safe_calloc(1, size);
		memcpy(diffuse_all_params, opts, size);
	} else {
		old = sizeof(diffuse_main_params);
		diffuse_all_params =
		    (struct di_option *)safe_realloc(diffuse_all_params,
		    old + size);
		o = diffuse_all_params;
		/*
		 * The last element of diffuse_all_params is always an empty
		 * struct di_option so the below will copy opts over the top of
		 * the empty struct di_option in diffuse_all_params.
		 */
		while (o->name != NULL)
			o++;
		memcpy(o, opts, size);
	}

	return (0);
}

/*
 * Helper stuff for building commands, etc.
 */

#define	O_NEXT(p, len) ((void *)(((char *)(p)) + (len)))

void
diffuse_rec_fill(struct di_oid *oid, int len, int type, uintptr_t id)
{

	oid->len = len;
	oid->type = type;
	oid->subtype = 0;
	oid->id = id;
}

/* Make room in the buffer and move the pointer forward. */
void *
diffuse_rec_next(struct di_oid **o, int len, int type)
{
	struct di_oid *ret;

	ret = *o;
	diffuse_rec_fill(ret, len, type, 0);
	*o = O_NEXT(*o, len);

	return (ret);
}

/*
 * Takes a table and a string, returns the value associated
 * with the string (NULL in case of failure).
 */
struct di_option *
diffuse_match_token(struct di_option *table, char *string)
{
	struct di_option *pt;

	pt = NULL;
	if (strlen(string) > 0) {
		for (pt = table; pt->name != NULL; pt++)
			if (strcmp(pt->name, string) == 0)
				break;
	}

	return (pt);
}

void
print_tokens(void)
{
	struct di_option *pt;

	for (pt = diffuse_all_params; pt->name != NULL; pt++)
		printf("t %s\n", pt->name);
}

/* Test if string only contains alphanumeric characters. */
int
str_isalnum(char *s)
{

	while (isalnum(*s))
		++s;

	return (*s == '\0');
}

/*
 * List functions.
 */

/* List flow table. */
static void
list_ft(struct di_oid *oid, struct di_oid *end,
    struct di_feature_arr_entry *features, int counters)
{
	struct di_feature_module *f;
	struct di_ft_export_entry *p;
	struct in_addr a;
	struct protoent *pe;
	char *cname, *cp, *fname, *mod_name;
	char buf[INET6_ADDRSTRLEN];
	int i, j;
	int32_t *bck_svals, *fwd_svals;
	uint16_t *class;
	uint8_t *fidx, *scnt;

#ifdef DIFFUSE_DEBUG2
	printf("list_ft\n");
#endif
	if (oid->type != DI_FLOW_TABLE)
		return;

	cp = (char *)oid + sizeof(struct di_oid);
	while (cp < (char *)end) {
		p = (struct di_ft_export_entry *)cp;

		printf("%05d", p->ruleno);
		printf(" %05d", p->bucket);

		if (counters > 0) {
			printf(" ");
			pr_u64(&p->pcnt, pr_u64(&p->pcnt, 0));
			pr_u64(&p->bcnt, pr_u64(&p->bcnt, 0));
			printf("(%ds)", p->expire);
		}
		if ((pe = getprotobynumber(p->id.proto)) != NULL)
			printf(" %s", pe->p_name);
		else
			printf(" proto %u", p->id.proto);

		/*
		 * The DIFFUSE kernel code stores the flow ID address and port
		 * in host byte order.
		 */
		if (p->id.addr_type == 4) {
			a.s_addr = htonl(p->id.src_ip);
			printf(" %s %u", inet_ntoa(a), p->id.src_port);

			a.s_addr = htonl(p->id.dst_ip);
			if (p->ftype & DI_FLOW_TYPE_BIDIRECTIONAL)
				printf(" <-> ");
			else
				printf(" -> ");

			printf("%s %u", inet_ntoa(a), p->id.dst_port);
		} else if (p->id.addr_type == 6) {
			printf(" %s %u", inet_ntop(AF_INET6, &p->id.src_ip6,
			    buf, sizeof(buf)), p->id.src_port);
			if (p->ftype & DI_FLOW_TYPE_BIDIRECTIONAL)
				printf(" <-> ");
			else
				printf(" -> ");

			printf("%s %u", inet_ntop(AF_INET6, &p->id.dst_ip6,
			    buf, sizeof(buf)), p->id.dst_port);
		} else {
			printf(" UNKNOWN <-> UNKNOWN\n");
		}

		cp += sizeof(struct di_ft_export_entry);
		fidx = (uint8_t *)cp;
		cp += p->fcnt*sizeof(uint8_t);
		scnt = (uint8_t *)cp;
		cp += p->fcnt * sizeof(uint8_t);
		printf(" ");

		for (i = 0; i < p->fcnt; i++) {
			/*
			 * Need to have feature name and algo name
			 * lookup stat names via algo.
			 */
			fname = features[fidx[i]].name;
			mod_name = features[fidx[i]].mod_name;
			f = find_feature_module(mod_name);

			if (!(f->type & DI_FEATURE_ALG_BIDIRECTIONAL) &&
			    p->ftype & DI_FLOW_TYPE_BIDIRECTIONAL) {
				fwd_svals = (int32_t *)cp;
				cp += scnt[i] * sizeof(int32_t);
				bck_svals = (int32_t * )cp;
				cp += scnt[i] * sizeof(int32_t);

				for (j = 0; j < scnt[i]; j++) {
					printf("fwd.%s.%s=%d ",
					    f->get_stat_name(j), fname,
					    fwd_svals[j]);
				}
				for (j = 0; j < scnt[i]; j++) {
					printf("bck.%s.%s=%d ",
					    f->get_stat_name(j), fname,
					    bck_svals[j]);
				}
			} else {
				fwd_svals = (int32_t *)cp;
				cp += scnt[i] * sizeof(int32_t);

				for (j = 0; j < scnt[i]; j++) {
					printf("%s.%s=%d ", f->get_stat_name(j),
					    fname, fwd_svals[j]);
				}
			}
		}
		for (i = 0; i < p->tcnt; i++) {
			cname = cp;
			cp += DI_MAX_NAME_STR_LEN;
			class = (uint16_t *)cp;
			cp += sizeof(uint16_t);
			printf("%s:%c%u ", cname, DI_CLASS_NO_CHAR, *class);
		}
		printf("\n");
	}
}

/* List features and flow table. */
static void
list_features(struct di_oid *oid, struct di_oid *end)
{
	struct di_ctl_feature *f;
	struct di_feature_module *last_mod;
	struct di_feature_arr_entry feature_arr[DI_MAX_FEATURES];
	int fcnt;

	last_mod = NULL;

	for (fcnt = 0; oid != end; oid = O_NEXT(oid, oid->len)) {
		if (oid->len < sizeof(*oid))
			errx(1, "invalid oid len %d", oid->len);

		switch (oid->type) {
		case DI_CMD_GET:
			if (co.verbose)
				printf("answer for cmd %d, len %d\n", oid->type,
				    oid->id);
			break;

		case DI_FEATURE:
		{

			f = (struct di_ctl_feature *)oid;
			printf("feature %s (algorithm %s)\n", f->name,
			    f->mod_name);
			/*
			 * DI_FEATURE oid's are always paired with a
			 * DI_FEATURE_CONFIG oid, so we set last_mod here which
			 * will be used for the next oid which will be the
			 * corresponding DI_FEATURE_CONFIG.
			 */
			last_mod = find_feature_module(f->mod_name);
			strcpy(feature_arr[fcnt].name, f->name);
			strcpy(feature_arr[fcnt].mod_name, f->mod_name);
			fcnt++;
			break;
		}

		case DI_FEATURE_CONFIG:
			if (last_mod != NULL) {
				last_mod->print_opts(oid);
				last_mod = NULL;
			} else {
				printf("  unrecognized feature\n");
			}
			break;

		case DI_FLOW_TABLE:
			list_ft(oid, end, feature_arr, 1);
			break;

		default:
			printf("unrecognized object %d size %d\n", oid->type,
			    oid->len);
			break;
		}
	}
}

/* List classifiers. */
static void
list_classifiers(struct di_oid *oid, struct di_oid *end)
{
	struct di_classifier_module *last_mod;
	struct di_ctl_classifier *c;
	int i;

	last_mod = NULL;

	for (; oid != end; oid = O_NEXT(oid, oid->len)) {
		if (oid->len < sizeof(*oid))
			errx(1, "invalid oid len %d", oid->len);

		switch (oid->type) {
		case DI_CMD_CONFIG:
			break;

		case DI_CMD_GET:
			if (co.verbose)
				printf("answer for cmd %d, len %d\n", oid->type,
				    oid->id);
			break;

		case DI_CLASSIFIER:
		{
			c = (struct di_ctl_classifier *)oid;

			printf("classifier %s (algorithm %s)\n", c->name,
			    c->mod_name);
			printf("  features: %d\n", c->fscnt);
			for (i = 0; i < c->fscnt; i++) {
				printf("    %s%s.%s\n",
				    c->fstats[i].fdir == DI_MATCH_DIR_NONE ?
				    "" : (c->fstats[i].fdir == DI_MATCH_DIR_FWD ?
				    "fwd." : "bck."),
				    c->fstats[i].sname,
				    c->fstats[i].fname);
			}
			printf("  classes: %d\n", c->ccnt);
			for (i = 0; i < c->ccnt; i++)
				printf("    %s\n", c->fstats[c->fscnt+i].fname);

			printf("  confirm: %d\n", c->confirm);
			/*
			 * DI_CLASSIFIER oid's are always paired with a
			 * DI_CLASSIFIER_CONFIG oid, so we set last_mod here
			 * which will be used for the next oid which will be the
			 * corresponding DI_CLASSIFIER_CONFIG.
			 */
			last_mod = find_classifier_module(c->mod_name);
			break;
		}

		case DI_CLASSIFIER_CONFIG:
			if (last_mod != NULL) {
				last_mod->print_opts(oid);
				last_mod = NULL;
			} else {
				printf("  unrecognized feature\n");
			}
			break;

		default:
			printf("unrecognized object %d size %d\n", oid->type,
			    oid->len);
			break;
		}
	}
}

/* List exports. */
static void
list_exports(struct di_oid *oid, struct di_oid *end)
{
	struct di_ctl_export *e;
	struct in_addr a;

	for (; oid != end; oid = O_NEXT(oid, oid->len)) {
		if (oid->len < sizeof(*oid))
			errx(1, "invalid oid len %d", oid->len);

		switch (oid->type) {
		case DI_CMD_GET:
			if (co.verbose)
				printf("answer for cmd %d, len %d\n", oid->type,
				    oid->id);
			break;

		case DI_EXPORT:
		{
			e = (struct di_ctl_export *)oid;

			printf("export %s\n", e->name);
			/* XXX: IPv6 missing */
			a.s_addr = htonl(e->conf.ip.s_addr);
			printf("  target udp://%s:%d\n", inet_ntoa(a),
			    e->conf.port);
			printf("  confirm %d\n", e->conf.confirm);
			printf("  min_batch %d\n", e->conf.min_batch);
			printf("  max_batch %d\n", e->conf.max_batch);
			printf("  max_delay %d\n", e->conf.max_delay);
			printf("  action %s %s\n", e->conf.action,
			    e->conf.action_param); /* XXX: Define types. */
			if (e->conf.atype & DI_ACTION_TYPE_BIDIRECTIONAL)
				printf("  bidirectional\n");
			else
				printf("  unidirectional\n");
			break;
		}

		default:
			printf("unrecognized object %d size %d\n", oid->type,
			    oid->len);
			break;
		}
	}
}

/*
 * Main functions called from ipfw.
 */

void
print_feature_usage()
{

	printf("ipfw feature <name> config ");
}

void
print_classifier_usage()
{

	printf("ipfw mlclass <name> config [confirm]");
}

void
print_export_usage()
{

	printf("ipfw export <name> config target <proto>://<ip>:<port> ");
	printf("[confirm <number>] [action <name>] [action-params <string>] ");
	printf("[min-batch <number>] [max-batch <number>] ");
	printf("[max-delay <number>]\n");
}

void
diffuse_init()
{

	diffuse_modules_init();
	diffuse_add_params(diffuse_main_params, sizeof(diffuse_main_params));
	/* XXX: Modules are not deregistered at the end. */
}

/*
 * Only support one feature/classifier/export name now.
 * XXX: extend to comma separated list.
 */
void
diffuse_delete(int ac, char *av[], int type)
{
	struct di_ctl_classifier *class;
	struct di_ctl_export *exp;
	struct di_ctl_feature *feature;
	struct di_oid *buf, *base;
	char *name;
	int lmax;

	feature = NULL;
	class = NULL;
	exp = NULL;

#ifdef DIFFFUSE_DEBUG2
	printf("diffuse delete\n");
#endif

	lmax = sizeof(struct di_oid); /* Command header. */
	if (type == DI_FEATURE)
		lmax += sizeof(struct di_ctl_feature);
	else if (type == DI_CLASSIFIER)
		lmax += sizeof(struct di_ctl_classifier);
	else if (type == DI_EXPORT)
		lmax += sizeof(struct di_ctl_export);
	else
		errx(1, "invalid DIFFUSE deletion type %d", type);

	base = buf = safe_calloc(1, lmax);
	/* All commands start with a 'DELETE' and a version. */
	diffuse_rec_next(&buf, sizeof(struct di_oid), DI_CMD_DELETE);
	base->id = DI_API_VERSION;

	if (type == DI_FEATURE) {
		feature = diffuse_rec_next(&buf, sizeof(*feature), DI_FEATURE);
		name = feature->name;
	} else if (type == DI_CLASSIFIER) {
		class = diffuse_rec_next(&buf, sizeof(*class), DI_CLASSIFIER);
		name = class->name;
	} else if (type == DI_EXPORT) {
		exp = diffuse_rec_next(&buf, sizeof(*exp), DI_EXPORT);
		name = exp->name;
	}

	av++; ac--;

	/* Set name. */
	if (ac) {
		strncpy(name, *av, DI_MAX_NAME_STR_LEN - 1);
		name[DI_MAX_NAME_STR_LEN - 1] = '\0';
		av++; ac--;
	} else {
		errx(EX_USAGE, "need a feature/classifier/export name");
	}

	if (do_cmd(IP_DIFFUSE, base, (char *)buf - (char *)base))
		err(1, "%s: setsockopt(%s)", "IP_DIFFUSE_DELETE", name);
}

static void
check_option_val(int ac, char **av, struct di_option *opt)
{
	double val;
	char *endptr;

	if (opt == NULL)
		errx(EX_DATAERR, "unrecognised option ``%s''", av[-1]);

	if (opt->arg_type == DI_OPTION_ARG_NOARG)
		return;

	if (ac > 0 && (*av)) {
		if (opt->arg_type == DI_OPTION_ARG_UINT ||
		    opt->arg_type == DI_OPTION_ARG_INT ||
		    opt->arg_type == DI_OPTION_ARG_DOUBLE) {
			if (opt->arg_type == DI_OPTION_ARG_UINT)
				val = (double)strtoul(*av, &endptr, 0);
			else if (opt->arg_type == DI_OPTION_ARG_INT)
				val = (double)strtol(*av, &endptr, 0);
			else
				val = strtod(*av, &endptr);

			if (*endptr) {
				errx(EX_DATAERR, "value of option %s "
				    "has wrong format at '%s'",
				    opt->name, endptr);
			}

			if (val < opt->arg_min || val > opt->arg_max) {
				errx(EX_DATAERR, "value of option %s "
				    "not in allowed range %.2f to %.2f",
				    opt->name, opt->arg_min,
				    opt->arg_max);
			}
		}
	} else {
		if (opt->arg_max > opt->arg_min) {
			errx(EX_USAGE, "option %s needs an argument "
			    "%.2f ... %.2f", opt->name, opt->arg_min,
			    opt->arg_max);
		} else {
			errx(EX_USAGE, "option %s needs an argument",
			    opt->name);
		}
	}
}

/* Configuration of features. */
static void
feature_config(int ac, char **av)
{
	struct di_ctl_feature *feature;
	struct di_feature_module *f;
	struct di_oid *buf, *cmd, *base;
	struct di_option *opt, *opts;
	int fconf_len, len, size;

	feature = NULL;
	len = 0;
	
#ifdef DIFFUSE_DEBUG2
	printf("diffuse feature config\n");
#endif

	/* Allocate space for 1 header + 1 feature. */
	len = sizeof(struct di_oid) + sizeof(struct di_feature);
	base = buf = safe_calloc(1, len);

	/* All commands start with a CONFIG and a version. */
	diffuse_rec_next(&buf, sizeof(struct di_oid), DI_CMD_CONFIG);
	base->id = DI_API_VERSION;
	feature = diffuse_rec_next(&buf, sizeof(*feature), DI_FEATURE);
	av++; ac--;

	/* Get feature name. */
	if (ac) {
		if (!str_isalnum(*av)) {
			errx(EX_DATAERR, "feature name can only contain "
			    "alphanumeric characters");
		} else if (strlen(*av) > DI_MAX_NAME_STR_LEN - 1) {
			errx(EX_DATAERR, "feature name cannot be longer than "
			    "%d chars", DI_MAX_NAME_STR_LEN - 1);
		}
		strncpy(feature->name, *av, DI_MAX_NAME_STR_LEN - 1);
		feature->name[DI_MAX_NAME_STR_LEN - 1] = '\0';
		av++; ac--;
	} else {
		errx(EX_USAGE, "need a feature name");
	}

	/* Set feature configuration. */
	while (ac > 0) {
		if (*av && !strcmp("-h", *av)) {
			print_feature_usage();
			if (feature != NULL) {
				f = find_feature_module(feature->mod_name);
				if (f)
					f->print_usage();
				else
					printf("module <name>\n");
			}
			free(base);
			exit(0);
		}

		opt = diffuse_match_token(diffuse_all_params, *av);
		ac--; av++;
		check_option_val(ac, av, opt);

		switch(opt->token) {
		case TOK_DI_MODULE:
			NEED(feature, "module is only for feature config");
			NEED1("module name must be specified");
			if (strlen(*av) > DI_MAX_NAME_STR_LEN - 1) {
				errx(EX_DATAERR, "feature name cannot be "
				    "longer than %d chars",
				    DI_MAX_NAME_STR_LEN - 1);
			}
			strncpy(feature->mod_name, *av,
			    DI_MAX_NAME_STR_LEN - 1);
			feature->mod_name[DI_MAX_NAME_STR_LEN - 1] = '\0';

			f = find_feature_module(feature->mod_name);
			if (f == NULL) {
				errx(EX_DATAERR, "unrecognised module ``%s''",
				    feature->mod_name);
			}

			/* Add options. */
			size = f->get_opts(&opts);
			diffuse_add_params(opts, size);

			/* Add space for config record. */
			fconf_len = f->get_conf_size();
			len += fconf_len;
			buf = base = safe_realloc(base, len);
			buf = O_NEXT(buf, sizeof(struct di_oid));

			/* Restore pointer to feature. */
			feature = (struct di_ctl_feature *)buf;
			buf = O_NEXT(buf, sizeof(struct di_feature));
			memset(buf, 0, fconf_len);
			cmd = (struct di_oid *)buf;
			cmd->type = DI_FEATURE_CONFIG;
			cmd->subtype = 0;
			cmd->len = fconf_len;
			f->parse_opts(TOK_DI_OPTS_INIT, NULL, buf);

			ac--; av++;
			break;

		default:
			f = find_feature_module(feature->mod_name);
			if (!f) {
				errx(EX_DATAERR, "unrecognised option ``%s''",
				    av[-1]);
			} else {
				f->parse_opts(opt->token, *av, buf);
				if (opt->arg_type != DI_OPTION_ARG_NOARG) {
					ac--; av++;
				}
			}
			break;
		}
	}

	if (!strlen(feature->mod_name))
		errx(EX_DATAERR, "no feature module name specified");

	if (do_cmd(IP_DIFFUSE, base, len))
		err(1, "setsockopt(%s)", "IP_DIFFUSE_FEATURE_CONFIGURE");
}

/*
 * Parse list of feature stats.
 * Max number of feature stats is DI_MAX_FEATURE_STATS (netinet/ip_fw.h).
 */
static int
fill_feature_stats(struct di_ctl_classifier *class, char *arg, int check)
{
	char *name, *tmp;
	char *sep = ",";
	char *s1, *s2, *s3, *p;
	int len = 0;

	if (!check)
		class->fscnt = 0;

	tmp = safe_calloc(1, strlen(arg) + 1);
	strcpy(tmp, arg);

	for (name = strtok(tmp, sep); name != NULL; name = strtok(NULL, sep)) {
		if (!check) {
			s1 = s2 = s3 = NULL;

			if (class->fscnt + 1 > DI_MAX_FEATURE_STATS) {
				errx(EX_DATAERR, "maximum number of features "
				    "limited to %d", DI_MAX_FEATURE_STATS);
			}

			/* Parse the feature name. */
			s1 = name;
			p = strstr(name, ".");
			if (p == NULL) {
				errx(EX_USAGE, "feature stat must be specified "
				    "as [fwd|bck].stat_name.feature_name");
			}
			s2 = p + 1;
			*p = '\0';
			p = strstr(s2, ".");
			if (p != NULL) {
				s3 = p + 1;
				*p = '\0';
			}

			class->fstats[class->fscnt].fdir = DI_MATCH_DIR_NONE;
			if (s3 != NULL) {
				/* First string must be direction. */
				if (!strcmp(s1, "fwd")) {
					class->fstats[class->fscnt].fdir =
					    DI_MATCH_DIR_FWD;
				} else if (!strcmp(s1, "bck")) {
					class->fstats[class->fscnt].fdir =
					    DI_MATCH_DIR_BCK;
				} else {
					errx(EX_USAGE, "missing "
					    "feature/statistic name or "
					    "direction missing/mispelled");
				}
				s1 = s2;
				s2 = s3;
			}

			/* Second string is statistics. */
			if (strlen(s1) > DI_MAX_NAME_STR_LEN - 1) {
				errx(EX_DATAERR, "stat name cannot be "
				    "longer than %d chars",
				    DI_MAX_NAME_STR_LEN - 1);
			}
			strncpy(class->fstats[class->fscnt].sname, s1,
			    DI_MAX_NAME_STR_LEN - 1);
			class->fstats[class->fscnt].sname[DI_MAX_NAME_STR_LEN -
			    1] = '\0';

			/* Last string is feature name. */
			if (strlen(s1) > DI_MAX_NAME_STR_LEN - 1) {
				errx(EX_DATAERR, "feature name cannot be "
				    "longer than %d chars",
				    DI_MAX_NAME_STR_LEN - 1);
			}
			strncpy(class->fstats[class->fscnt].fname, s2,
			    DI_MAX_NAME_STR_LEN - 1);
			class->fstats[class->fscnt].fname[DI_MAX_NAME_STR_LEN -
			    1] = '\0';
			class->fscnt++;
		}
		len += sizeof(struct di_feature_stat);
	}

	free(tmp);

#ifdef DIFFUSE_DEBUG2
	printf("feature stats %d\n", class->fscnt);
#endif

	return (len);
}

/*
 * Parse list of classes.
 * Max number of classes is DI_MAX_CLASSES (netinet/ip_fw.h).
 */
static int
fill_class_names(struct di_ctl_classifier *class, char *arg, int check)
{
	char *name, *tmp;
	char *sep = ",";
	int i, len;

	len = 0;

	if (!check)
		class->ccnt = 0;

	tmp = safe_calloc(1, strlen(arg) + 1);
	strcpy(tmp, arg);

	for (name = strtok(tmp, sep); name != NULL; name = strtok(NULL, sep)) {
		if (!check) {
			if (!str_isalnum(name)) {
				errx(EX_DATAERR, "class names can only contain "
				    "alphanumeric characters");
			}

			if (class->ccnt + 1 > DI_MAX_CLASSES) {
				errx(EX_DATAERR, "maximum number of classes "
				    "limited to %d", DI_MAX_CLASSES);
			}

			/* Do not allow duplicates. */
			for (i = 0; i < class->ccnt; i++) {
				if (!strcmp(class->fstats[i].fname, name)) {
					errx(EX_DATAERR, "duplicate class "
					    "name %s", name);
				}
			}

			/* Copy class in feature name. */
			if (strlen(name) > DI_MAX_NAME_STR_LEN - 1) {
				errx(EX_DATAERR, "feature name cannot be "
				    "longer than %d chars",
				    DI_MAX_NAME_STR_LEN - 1);
			}
			strncpy(class->fstats[class->fscnt + class->ccnt].fname,
			    name, DI_MAX_NAME_STR_LEN - 1);
			class->fstats[class->fscnt + class->ccnt].fname[
			    DI_MAX_NAME_STR_LEN - 1] = '\0';
			class->ccnt++;
		}
		len += sizeof(struct di_feature_stat);
	}

	free(tmp);

#ifdef DIFFUSE_DEBUG2
	printf("classes %d\n", class->ccnt);
#endif

	return (len);
}

/* Configuration of classifier. */
static void
classifier_config(int ac, char **av)
{
	struct di_classifier_module *c;
	struct di_ctl_classifier *class;
	struct di_oid *buf, *base, *class_config, *cmd;
	struct di_option *opt, *opts;
	char *class_str, *feature_str, *model;
	int cconf_len, clen, flen, have_class_names, have_feature_stats, i, len;
	int mod_len, size;

	have_class_names = have_feature_stats = len = 0;
	class_config = NULL;
	class = NULL;
	class_str = feature_str = model = NULL;

#ifdef DIFFUSE_DEBUG2
	printf("diffuse classifier config\n");
#endif

	/* Allocate space for 1 header + 1 classifier */
	len = sizeof(struct di_oid) + sizeof(struct di_ctl_classifier);
	base = buf = safe_calloc(1, len);

	/* All commands start with a CONFIG and a version. */
	diffuse_rec_next(&buf, sizeof(struct di_oid), DI_CMD_CONFIG);
	base->id = DI_API_VERSION;
	class = diffuse_rec_next(&buf, sizeof(*class), DI_CLASSIFIER);

	av++; ac--;
	/* Classifier name. */
	if (ac) {
		if (!str_isalnum(*av)) {
			errx(EX_DATAERR, "classifier name can only contain "
			    "alphanumeric characters");
		} else if(strlen(*av) > DI_MAX_NAME_STR_LEN - 1) {
			errx(EX_DATAERR, "classifier name cannot be longer than "
			    "%d chars", DI_MAX_NAME_STR_LEN - 1);
		}
		strncpy(class->name, *av, DI_MAX_NAME_STR_LEN - 1);
		class->name[DI_MAX_NAME_STR_LEN - 1] = '\0';
		av++; ac--;
	} else {
		errx(EX_USAGE, "need a classifier name");
	}

	/* Set feature configuration. */
	while (ac > 0) {
		if (*av && !strcmp("-h", *av)) {
			print_classifier_usage();
			if (class != NULL) {
				c = find_classifier_module(class->mod_name);
				if (c)
					c->print_usage();
				else
					printf("algorithm <name>\n");
			}
			free(base);
			exit(0);
		}

		opt = diffuse_match_token(diffuse_all_params, *av);
		ac--; av++;
		check_option_val(ac, av, opt);

		switch(opt->token) {
		case TOK_DI_ALGORITHM:
			{
			NEED(class, "algorithm is only for classifier config");
			NEED1("algorithm name must be specified");
			if (strlen(*av) > DI_MAX_NAME_STR_LEN - 1) {
				errx(EX_DATAERR, "classifier name cannot be "
				    "longer than %d chars",
				    DI_MAX_NAME_STR_LEN - 1);
			}
			strncpy(class->mod_name, *av, DI_MAX_NAME_STR_LEN - 1);
			class->mod_name[DI_MAX_NAME_STR_LEN - 1] = '\0';
			class->confirm = 0;

			c = find_classifier_module(class->mod_name);
			if (c == NULL) {
				errx(EX_DATAERR, "unrecognised module ``%s''",
				    class->mod_name);
			}
			/* Add options. */
			size = c->get_opts(&opts);
			diffuse_add_params(opts, size);

			/* Alloc space for config. */
			cconf_len = c->get_conf_size();
			class_config = safe_calloc(1, cconf_len);
			class_config->type = DI_CLASSIFIER_CONFIG;
			class_config->subtype = 0;
			class_config->len = cconf_len;
			c->parse_opts(TOK_DI_OPTS_INIT, NULL, class_config);
			ac--; av++;
			}
			break;

		case TOK_DI_CONFIRM:
			NEED(class, "confirm is only for classifier/export "
			    "config");
			NEED1("confirm number must be specified");
			class->confirm = atoi(*av);
			ac--; av++;
			break;

		case TOK_DI_USE_FEATURE_STATS:
			{
			NEED(class && class_config,
			    "use-feature-stats is only for classifier config");
			NEED1("feature statistics must be specified");

			if (have_class_names) {
				errx(EX_USAGE, "class-names must be specified "
				    "after use-feature-stats");
			}

			flen = fill_feature_stats(class, av[0], 1);
			len += flen;
			buf = base = safe_realloc(base, len);
			buf = O_NEXT(buf, sizeof(struct di_oid));
			class = (struct di_ctl_classifier *)buf;
			class->oid.len += flen;
			fill_feature_stats(class, av[0], 0);
			have_feature_stats = flen;
			ac--; av++;
			}
			break;

		case TOK_DI_CLASS_NAMES:
			{
			NEED(class && class_config,
			    "class-names is only for classifier config");
			NEED1("class names must be specified");
			clen = fill_class_names(class, av[0], 1);
			len += clen;
			buf = base = safe_realloc(base, len);
			buf = O_NEXT(buf, sizeof(struct di_oid));
			class = (struct di_ctl_classifier *)buf;
			class->oid.len += clen;
			fill_class_names(class, av[0], 0);
			have_class_names = clen;
			ac--; av++;
			}
			break;

		default:
			c = find_classifier_module(class->mod_name);
			if (!c) {
				errx(EX_DATAERR, "unrecognised option ``%s''",
				    av[-1]);
			} else {
				c->parse_opts(opt->token, *av, class_config);
				if (opt->arg_type != DI_OPTION_ARG_NOARG) {
					ac--; av++;
				}
			}
			break;
		}
	}

	if (!strlen(class->mod_name))
		errx(EX_DATAERR, "no classifier algorithm name specified");

	/* Now try loading the model. */
	c = find_classifier_module(class->mod_name);
	if (c == NULL || (mod_len = c->load_model(class_config, &model,
	    &feature_str, &class_str)) < 0)
		errx(EX_DATAERR, "couldn't load classifier model");

	if (mod_len > 0) {
		/* Merge the fixed length conf and the model. */
		class_config = safe_realloc((char *)class_config,
		    class_config->len + mod_len);
		/* Copy model. */
		memcpy((char *)class_config + class_config->len, model,
		    mod_len);
		class_config->len += mod_len;

		/*
		 * If not previously specified use model features and
		 * classes.
		 */
		flen = fill_feature_stats(class, feature_str, 1);
		clen = fill_class_names(class, class_str, 1);

		if (have_feature_stats && have_feature_stats != flen) {
			errx(EX_USAGE, "number of feature stats specified in "
			    "use-feature-stats different from number in model "
			    "file");
		}
		if (have_class_names && have_class_names != clen) {
			errx(EX_USAGE, "number of class names specified in "
			    "class-names different from number in model file");
		}

		if (!have_feature_stats) {
			len += flen;
			buf = base = safe_realloc(base, len);
			buf = O_NEXT(buf, sizeof(struct di_oid));
			class = (struct di_ctl_classifier *)buf;
			cmd = (struct di_oid *)buf;
			cmd->len += flen;

			if (have_class_names) {
				/*
				 * Move specified class names to the back.
				 * XXX: Ugly, should really have separate list
				 * of class names.
				 */
				for (i = 0; i < class->ccnt; i++) {
					strcpy(class->fstats[flen /
					    sizeof(struct di_feature_stat) +
					    i].fname, class->fstats[i].fname);
				}
			}
			fill_feature_stats(class, feature_str, 0);
		}
		if (!have_class_names)  {
			len += clen;
			buf = base = safe_realloc(base, len);
			buf = O_NEXT(buf, sizeof(struct di_oid));
			class = (struct di_ctl_classifier *)buf;
			class->oid.len += clen;
			fill_class_names(class, class_str, 0);
		}
	}

	/* Build message. */
	len += class_config->len;
	buf = base = safe_realloc(base, len);
	buf = O_NEXT(buf, sizeof(struct di_oid));
	cmd = (struct di_oid *)buf;
	buf = O_NEXT(buf, cmd->len);
	memcpy(buf, class_config, class_config->len);
	free(class_config);
	class_config = (struct di_oid *)buf;
	list_classifiers(base, O_NEXT(buf, class_config->len));

	if (do_cmd(IP_DIFFUSE, base, len))
		err(1, "setsockopt(%s)", "IP_DIFFUSE_CLASSIFIER_CONFIGURE");
}

static void
export_config(int ac, char **av)
{
	struct di_ctl_export *exp;
	struct di_oid *buf, *base;
	struct di_option *opt;
	struct hostent *h;
	char *errptr, *ip, *p;
	int len;

	exp = NULL;
	len = 0;

#ifdef DIFFUSE_DEBUG2
	printf("diffuse export config\n");
#endif

	/* Allocate space for 1 header + 1 export */
	len = sizeof(struct di_oid) + sizeof(struct di_ctl_export);
	base = buf = safe_calloc(1, len);

	/* All commands start with a CONFIG and a version. */
	diffuse_rec_next(&buf, sizeof(struct di_oid), DI_CMD_CONFIG);
	base->id = DI_API_VERSION;
	exp = diffuse_rec_next(&buf, sizeof(*exp), DI_EXPORT);

	av++; ac--;
	/* Export name. */
	if (ac) {
		if (!str_isalnum(*av)) {
			errx(EX_USAGE, "export name can only contain "
			    "alphanumeric characters");
		}
		strncpy(exp->name, *av, DI_MAX_NAME_STR_LEN - 1);
		exp->name[DI_MAX_NAME_STR_LEN - 1] = '\0';
		av++; ac--;
	} else {
		errx(EX_USAGE, "need a export name");
	}

	/* Set defaults. */
	exp->conf.atype |= DI_ACTION_TYPE_BIDIRECTIONAL;

	/* Set feature configuration. */
	while (ac > 0) {
		if (*av && !strcmp("-h", *av)) {
			print_export_usage();
			free(base);
			exit(0);
		}

		opt = diffuse_match_token(diffuse_all_params, *av);
		ac--; av++;
		check_option_val(ac, av, opt);

		switch(opt->token) {
		case TOK_DI_EXP_TARGET:
			{
			NEED(exp, "target is only for export config");
			NEED1("target name must be specified");
			p = strstr(*av, "://");
			if (p == NULL) {
				errx(EX_USAGE, "target must be specified as "
				    "<proto>://<ip>:<port>");
			}
			/* Parse protocol. */
			if (strncmp(*av, "udp", 3))
				errx(EX_USAGE, "protocol must be UDP");
			exp->conf.proto = IPPROTO_UDP;

			ip = p + 3;
			p = strstr(ip, ":");
			if (p != NULL) {
				*p = '\0';
				p++;
				exp->conf.port = strtonum(p, 1, 65535,
				    (const char **)&errptr);
				if (errptr != NULL) {
					errx(EX_USAGE, "error parsing target "
					    "port: %s", errptr);
				}
			} else {
				exp->conf.port =
				    DI_COLLECTOR_DEFAULT_LISTEN_PORT;
			}

			/* Parse IP. */
			if ((h = gethostbyname(ip)) == NULL)
				errx(EX_USAGE, "can't resolve target address");

			if (h->h_addrtype != AF_INET ||
			    h->h_length != sizeof(struct in_addr)) {
				errx(EX_USAGE, "only IPv4 supported for now");
			}
			exp->conf.ip = *((struct in_addr *)h->h_addr_list[0]);
			ac--; av++;
			break;
			}

		case TOK_DI_CONFIRM:
			NEED(exp, "confirm is only for classifier/export "
			    "config");
			NEED1("confirm number must be specified");
			exp->conf.confirm = atoi(*av);
			ac--; av++;
			break;

		case TOK_DI_EXP_MIN_BATCH:
			NEED(exp, "min-batch is only for export config");
			NEED1("min-batch must be specified");
			exp->conf.min_batch = atoi(*av);
			if (exp->conf.max_batch == 0)
				exp->conf.max_batch = exp->conf.min_batch;
			if (exp->conf.min_batch > exp->conf.max_batch) {
				errx(EX_USAGE,
				    "min-batch must be <= max-batch");
			}
			ac--; av++;
			break;

		case TOK_DI_EXP_MAX_BATCH:
			NEED(exp, "max-batch is only for export config");
			NEED1("max-batch must be specified");
			exp->conf.max_batch = atoi(*av);
			if (exp->conf.min_batch == 0)
				exp->conf.min_batch = exp->conf.max_batch;
			if (exp->conf.max_batch < exp->conf.min_batch) {
				errx(EX_USAGE,
				    "max-batch must be >= min-batch");
			}
			ac--; av++;
			break;

		case TOK_DI_EXP_MAX_DELAY:
			NEED(exp, "max-delay is only for export config");
			NEED1("max-delay must be specified");
			exp->conf.max_delay = atoi(*av);
			ac--; av++;
			break;

#ifdef notyet
		case TOK_DI_EXP_FLOW_KEY:
			NEED(exp, "flow-key is only for export config");
			NEED1("flow-key name must be specified");
			/* XXX */
			ac--; av++;
			break;

		case TOK_DI_EXP_FEATURES:
			NEED(exp, "feature-key is only for export config");
			NEED1("feature-key name must be specified");
			/* XXX */
			ac--; av++;
			break;
#endif

		case TOK_DI_EXP_ACTION:
			NEED(exp, "action is only for export config");
			NEED1("action must be specified");
			/*
			 * XXX: Check whether action is valid ipfw action.
			 * XXX: As an optimisation use constants instead of
			 *  strings to identify actions.
			 */
			if (strlen(*av) > DI_MAX_NAME_STR_LEN - 1) {
				errx(EX_DATAERR, "action cannot be longer than "
				    "%d chars", DI_MAX_NAME_STR_LEN - 1);
			}
			strncpy(exp->conf.action, *av, DI_MAX_NAME_STR_LEN - 1);
			exp->conf.action[DI_MAX_NAME_STR_LEN - 1] = '\0';
			ac--; av++;
			break;

		case TOK_DI_EXP_ACTION_PARAMS:
			NEED(exp, "action-param is only for export config");
			NEED1("action-param must be specified");
			if (strlen(*av) > DI_MAX_NAME_STR_LEN - 1) {
				errx(EX_DATAERR, "action-param cannot be "
				    "longer than %d chars",
				    DI_MAX_NAME_STR_LEN - 1);
			}
			strncpy(exp->conf.action_param, *av,
			    DI_MAX_PARAM_STR_LEN - 1);
			exp->conf.action[DI_MAX_PARAM_STR_LEN - 1] = '\0';
			ac--; av++;
			break;
		
		case TOK_DI_EXP_ACTION_UNIDIR:
			NEED(exp, "unidirectional is only for export config");
			exp->conf.atype &= ~DI_ACTION_TYPE_BIDIRECTIONAL;
			break;

		default:
			errx(EX_DATAERR, "unrecognised option ``%s''", av[-1]);
			break;
		}
	}

	if (do_cmd(IP_DIFFUSE, base, len))
		err(1, "setsockopt(%s)", "IP_DIFFUSE_EXPORT_CONFIGURE");
}

void
diffuse_config(int ac, char **av, int type)
{

	switch(type) {
	case DI_FEATURE:
		feature_config(ac, av);
		break;

	case DI_CLASSIFIER:
		classifier_config(ac, av);
		break;

	case DI_EXPORT:
		export_config(ac, av);
		break;

	default:
		errx(EX_DATAERR, "unrecognised option");
		break;
	}
}

/*
 * Entry point for list functions. Currently only supports either one name or
 * 'all' (which returns all features, classifiers, exports).
 * XXX: Add comma separated lists.
 */
void
diffuse_list(int ac, char *av[], int type, int show_counters)
{
	struct di_ctl_classifier *class;
	struct di_ctl_export *exp;
	struct di_ctl_feature *feature;
	struct di_oid *buf, *base;
	struct di_option *opt;
	char *name;
	int buflen, i, lmax, ret, total_len;

	class = NULL;
	exp = NULL;
	feature = NULL;
	base = NULL;
	name = NULL;

#ifdef DIFFUSE_DEBUG2
	printf("diffuse_list\n");
#endif

	ac--; av++;

	if (!ac) {
		if (type == DI_FEATURE)
			errx(EX_USAGE, "need a feature name");
		else if (type == DI_CLASSIFIER)
			errx(EX_USAGE, "need a classifier name");
		else if (type == DI_EXPORT)
			errx(EX_USAGE, "need an export name");
	}

	lmax = sizeof(struct di_oid);
	if (type == DI_FEATURE)
		lmax += sizeof(struct di_ctl_feature);
	else if (type == DI_CLASSIFIER)
		lmax += sizeof(struct di_ctl_classifier);
	else if (type == DI_EXPORT)
		lmax += sizeof(struct di_ctl_export);

	base = buf = safe_calloc(1, lmax);
	diffuse_rec_next(&buf, sizeof(struct di_oid), DI_CMD_GET);
	base->id = DI_API_VERSION;

	if (type == DI_FEATURE) {
		base->subtype = DI_FEATURE;
		feature = diffuse_rec_next(&buf, sizeof(*feature), DI_FEATURE);
		name = feature->name;
	} else if (type == DI_CLASSIFIER) {
		base->subtype = DI_CLASSIFIER;
		class = diffuse_rec_next(&buf, sizeof(*class), DI_CLASSIFIER);
		name = class->name;
	} else if (type == DI_EXPORT) {
		base->subtype = DI_EXPORT;
		exp = diffuse_rec_next(&buf, sizeof(*exp), DI_EXPORT);
		name = exp->name;
	} else { /* Flow table. */
		base->subtype = DI_FLOW_TABLE;
		base->flags = DI_FT_GET_NONE;
	}

	if (name != NULL) {
		if (strlen(name) > DI_MAX_NAME_STR_LEN - 1) {
			errx(EX_DATAERR, "name cannot be longer than %d chars",
			    DI_MAX_NAME_STR_LEN - 1);
		}
		strncpy(name, *av, DI_MAX_NAME_STR_LEN - 1);
		name[DI_MAX_NAME_STR_LEN - 1] = '\0';
		av++; ac--;
	}

	while (ac > 0) {
		opt = diffuse_match_token(diffuse_all_params, *av);
		ac--; av++;
		check_option_val(ac, av, opt);

		switch(opt->token) {
		case TOK_DI_EXPIRED:
			if (type != DI_FLOW_TABLE) {
				errx(EX_USAGE,
				    "expired is only for flowtable show");
			}
			base->flags |= DI_FT_GET_EXPIRED;
			break;

		default:
			errx(EX_DATAERR, "unrecognised option ``%s''", av[-1]);
			break;
		}
	}

	/*
	 * Ask the kernel an estimate of the required space (result in oid.id).
	 * In any case, space might grow in the meantime due to the creation of
	 * new features, so we must be prepared to retry.
	 */
	total_len = (char *)buf - (char *)base;
	/* Arg 1 being -ve implies getsockopt instead of setsockopt. */
	ret = do_cmd(-IP_DIFFUSE, base, (uintptr_t)&total_len);
	if (ret != 0 || base->id < total_len)
		goto done;

	if (base->id > total_len) {
		buflen = base->id * 2;
		/* Try a few times, until the buffer fits. */
		for (i = 0; i < 20; i++) {
			total_len = buflen;
			base = safe_realloc(base, total_len);
			ret = do_cmd(-IP_DIFFUSE, base, (uintptr_t)&total_len);
			if (ret != 0 || base->id < total_len)
				goto done;

			if (total_len >= base->id)
				break; /* ok */

			buflen *= 2;  /* Double for next attempt. */
		}
	}

	if (type == DI_FEATURE || type == DI_FLOW_TABLE)
		list_features(base, O_NEXT(base, total_len));
	else if (type == DI_CLASSIFIER)
		list_classifiers(base, O_NEXT(base, total_len));
	else
		list_exports(base, O_NEXT(base, total_len));

done:
	free(base);
	if (ret)
		err(1, "setsockopt(%s)", "IP_DIFFUSE_SHOW");
}

void
diffuse_flush(int ac, char *av[], int reset_counters_only)
{
	struct di_oid *buf, *base;

#ifdef DIFFUSE_DEBUG2
	printf("diffuse_ft_flush\n");
#endif

	ac--; av++;
	base = buf = safe_calloc(1, sizeof(struct di_oid));

	if (reset_counters_only)
		diffuse_rec_next(&buf, sizeof(struct di_oid), DI_CMD_ZERO);
	else
		diffuse_rec_next(&buf, sizeof(struct di_oid), DI_CMD_FLUSH);

	base->id = DI_API_VERSION;
	base->subtype = DI_FLOW_TABLE;

	if (ac)
		errx(EX_DATAERR, "unrecognised option ``%s''", *av);

	if (do_cmd(IP_DIFFUSE, base, sizeof(struct di_oid)))
		err(1, "setsockopt(%s)", "IP_DIFFUSE_FLUSH");
}

/*
 * Functions to print out DIFFUSE rule instructions, called from ipfw.
 *
 * XXX: All the print methods should print token strings from a list of
 * tokens to ensure easy consistency if tokens are changed.
 */

void
print_features(ipfw_insn_features *f, int implicit)
{
	int i;

	if (implicit &&
	    !(f->ftype & DI_MATCH_ONCE) &&
	    !(f->ftype & DI_MATCH_ONCE_CLASS) &&
	    !(f->ftype & DI_MATCH_ONCE_EXP) &&
	    !(f->ftype & DI_MATCH_SAMPLE_REG) &&
	    !(f->ftype & DI_MATCH_SAMPLE_RAND)) {
		return;
	}

	printf(" features ");
	for(i = 0; i < f->fcnt; i++) {
		printf("%s", f->fnames[i]);
		if (i < f->fcnt - 1)
			printf(",");
	}
	if (f->ftype & DI_FLOW_TYPE_BIDIRECTIONAL)
		printf(" bidirectional");
	else
		printf(" unidirectional");

	if (f->ftype & DI_MATCH_ONCE)
		printf(" once");
	else if (f->ftype & DI_MATCH_SAMPLE_REG)
		printf(" sample %d", f->sample_int);
	else if (f->ftype & DI_MATCH_SAMPLE_RAND)
		printf(" rnd-sample %.3f", (double)f->sample_prob / 0xFFFFFFFF);
	else if (f->ftype & DI_MATCH_ONCE_CLASS)
		printf(" once-classified");
	else if (f->ftype & DI_MATCH_ONCE_EXP)
		printf(" once-exported");
}

void
print_feature_match(ipfw_insn_feature_match *fm)
{

	printf(" ");
	if (fm->fdir == DI_MATCH_DIR_BCK)
		printf("bck.");
	else if (fm->fdir == DI_MATCH_DIR_FWD)
		printf("fwd.");

	printf("%s.%s", fm->sname, fm->fname);

	switch(fm->comp) {
	case DI_COMP_LT:
		printf("<");
		break;

	case DI_COMP_LE:
		printf("<=");
		break;

	case DI_COMP_EQ:
		printf("=");
		break;

	case DI_COMP_GE:
		printf(">=");
		break;

	case DI_COMP_GT:
		printf(">");
		break;
	}

	printf("%d", fm->thresh);
}

void
print_mlclass(ipfw_insn_ml_classify *cl)
{

	printf("mlclass %s", cl->cname);
}

void
print_ctags(ipfw_insn_class_tags *ct)
{
	int i;

	if (ct->tcnt > 0) {
		printf(" class-tags ");
		for (i = 0; i < ct->tcnt; i++) {
			printf("%d", ct->tags[i]);
			if (i < ct->tcnt - 1)
				printf(",");
		}
	}
}

void
print_match_if(ipfw_insn_match_if_class *mic)
{
	int i;

	printf(" match-if-class %s:", mic->cname);
	for (i = 0; i < mic->mcnt; i++) {
#ifdef DIFFUSE_DEBUG2
		printf("%s(%d)", mic->clnames[i], mic->match_classes[i]);
#else
		printf("%s", mic->clnames[i]);
#endif
		if (i < mic->mcnt - 1)
			printf(",");
	}
}

void
print_export(ipfw_insn_export *ex)
{

	printf("export %s", ex->ename);
}

int
diffuse_show_cmd(ipfw_insn *cmd)
{

	switch(cmd->opcode) {
	case O_DI_FEATURES:
		print_features((ipfw_insn_features *)cmd, 0);
		break;

	case O_DI_FEATURES_IMPLICIT:
		print_features((ipfw_insn_features *)cmd, 1);
		break;

	case O_DI_FEATURE_MATCH:
		print_feature_match((ipfw_insn_feature_match *)cmd);
		break;

	case O_DI_ML_CLASSIFY:
		print_mlclass((ipfw_insn_ml_classify *)cmd);
		break;

	case O_DI_MATCH_IF_CLASS:
		print_match_if((ipfw_insn_match_if_class *)cmd);
		break;

	case O_DI_EXPORT:
		print_export((ipfw_insn_export *)cmd);
		break;

	case O_DI_CLASS_TAGS:
		print_ctags((ipfw_insn_class_tags *)cmd);
		break;

	case O_DI_FLOW_TABLE:
	case O_DI_ML_CLASSIFY_IMPLICIT:
		/* Ignore. */
		break;

	default:
		return (1);
	}

	return (0);
}

/*
 * Functions to parse diffuse instructions, called from ipfw.
 */

/*
 * Parse list of features and fill instructions. Max number of features is
 * DI_MAX_FEATURES (netinet/ip_fw.h).
 */
static void
fill_features(ipfw_insn_features *cmd, char *arg)
{
	char *name, *tmp;
	char *sep = ",";
	int slen;

	cmd->ftype = 0;
	cmd->ftype |= DI_FLOW_TYPE_BIDIRECTIONAL;
	cmd->fcnt = 0;
	cmd->sample_int = 0;
	cmd->o.len |= F_INSN_SIZE(ipfw_insn_features);

	if (!arg)
		return;

	tmp = safe_calloc(1, strlen(arg) + 1);
	strcpy(tmp, arg);

	for (name = strtok(tmp, sep); name; name = strtok(NULL, sep)) {
		if (cmd->fcnt + 1 > DI_MAX_FEATURES) {
			errx(EX_DATAERR,
			    "maximum number of features limited to %d",
			    DI_MAX_FEATURES);
		}

		slen = strlen(name);
		if (slen > DI_MAX_NAME_STR_LEN - 1)
			slen = DI_MAX_NAME_STR_LEN - 1;

		strncpy(cmd->fnames[cmd->fcnt], name, slen);
		cmd->fcnt++;
	}

	free(tmp);
	DID2("features %d", cmd->fcnt);
	DID2("command len %d", cmd->o.len);
}

static void
fill_feature_match(ipfw_insn_feature_match *cmd, char *arg)
{
	char *endptr, *p, *s1, *s2, *s3;
	char tmp[256];

	s1 = s2 = s3 = NULL;

	cmd->o.len |= F_INSN_SIZE(ipfw_insn_feature_match);
	cmd->comp = 0;
	cmd->thresh = 0;
	cmd->fdir = DI_MATCH_DIR_NONE;

	strncpy(tmp, arg, sizeof(tmp));

	/* Parse comparison op. */
	p = strstr(tmp, "<");
	if (p != NULL) {
		if (p[1] == '=')
			cmd->comp = DI_COMP_LE;
		else
			cmd->comp = DI_COMP_LT;
	} else {
		p = strstr(tmp, ">");
		if (p != NULL) {
			if (p[1] == '=')
				cmd->comp = DI_COMP_GE;
			else
				cmd->comp = DI_COMP_GT;
		} else {
			p = strstr(tmp, "=");
			if (p != NULL) {
				cmd->comp = DI_COMP_EQ;
			} else {
				errx(EX_USAGE,
				    "feature match has no comparison");
			}
		}
	}

	/*
	 * After determining the comparison operator, NULL terminate the string
	 * up to but not including the operator for subsequent parsing.
	 */
	*p = '\0';
	p++;
	if (cmd->comp == DI_COMP_LE || cmd->comp == DI_COMP_GE)
		p++;

	/* Parse the threshold value. */
	cmd->thresh = strtoul(p, &endptr, 10);
	if (*endptr) {
		errx(EX_USAGE, "feature match has invalid threshold value %s",
		    endptr);
	}

	/* Parse the feature name. */
	s1 = tmp;
	p = strstr(tmp, ".");
	if (p == NULL) {
		/* Should never happen. */
		errx(EX_USAGE, "feature match has invalid format");
	}
	s2 = p + 1;
	*p = '\0';
	p = strstr(s2, ".");
	if (p != NULL) {
		s3 = p + 1;
		*p = '\0';
	}

	if (s3 != NULL) {
		/* First string must be direction. */
		if (!strcmp(s1, "fwd")) {
			cmd->fdir = DI_MATCH_DIR_FWD;
		} else if (!strcmp(s1, "bck")) {
			cmd->fdir = DI_MATCH_DIR_BCK;
		} else {
			errx(EX_USAGE, "missing feature/statistic name or "
			    "direction mispelled");
		}
		s1 = s2;
		s2 = s3;
	}

	/* Second string is statistics. */
	strncpy(cmd->sname, s1, DI_MAX_NAME_STR_LEN - 1);
	cmd->sname[DI_MAX_NAME_STR_LEN - 1] = '\0';

	/* Last string is feature name. */
	strncpy(cmd->fname, s2, DI_MAX_NAME_STR_LEN - 1);
	cmd->fname[DI_MAX_NAME_STR_LEN - 1] = '\0';

	DID2("dir %d", cmd->fdir);
	DID2("stat %s", cmd->sname);
	DID2("feat %s", cmd->fname);
	DID2("comp %d", cmd->comp);
	DID2("thre %d", cmd->thresh);
}

static void
fill_class_tags(ipfw_insn_class_tags *cmd, ipfw_insn_ml_classify *classifier,
    char *arg)
{
	char *errptr, *tag, *tmp;
	char *sep = ",";
	int len;
	uint16_t v;

	strcpy(cmd->cname, classifier->cname);
	cmd->tcnt = 0;
	len = sizeof(ipfw_insn_class_tags);
	tmp = safe_calloc(1, strlen(arg) + 1);
	strcpy(tmp, arg);

	for (tag = strtok(tmp, sep); tag; tag = strtok(NULL, sep)) {
		if (cmd->tcnt + 1 > DI_MAX_CLASSES) {
			errx(EX_DATAERR, "maximum number of tags limited to %d",
			    DI_MAX_CLASSES);
		}

		v = strtonum(tag, 0, 65535, (const char **)&errptr);
		if (errptr != NULL)
			errx(EX_USAGE, "tag number: %s", errptr);

		cmd->tags[cmd->tcnt++] = v;
		len += sizeof(uint16_t);
	}

	free(tmp);
	cmd->o.len |= (len + 3) / sizeof(uint32_t);
	DID2("classifier %s", cmd->cname);
	DID2("tags %d", cmd->tcnt);
}

/* Parse the match-if-class match. */
static void
fill_match_if_class(ipfw_insn_match_if_class *cmd,
    ipfw_insn_ml_classify *classifier, char *arg)
{
	const char *sep = ",";
	char *class, *classes, *errptr, *p, *tmp;
	int len;
	uint16_t v;

	cmd->mcnt = 0;
	len = sizeof(ipfw_insn_match_if_class);
	tmp = safe_calloc(1, strlen(arg) + 1);
	strcpy(tmp, arg);

	p = strstr(tmp, ":");
	if (!p || p - tmp >= strlen(arg)) {
		errx(EX_USAGE,
		    "match must be specified as classifier:class[,class,...]");
	}
	classes = p + 1;
	*p = '\0';

	if (strlen(tmp) > DI_MAX_NAME_STR_LEN - 1) {
		errx(EX_DATAERR, "classifier name cannot be longer than %d "
		    "chars", DI_MAX_NAME_STR_LEN - 1);
	}
	strncpy(cmd->cname, tmp, DI_MAX_NAME_STR_LEN - 1);
	cmd->cname[DI_MAX_NAME_STR_LEN - 1] = '\0';
	strncpy(classifier->cname, tmp, DI_MAX_NAME_STR_LEN - 1);
	classifier->cname[DI_MAX_NAME_STR_LEN - 1] = '\0';

	/* Parse classes. */
	for (class = strtok(classes, sep); class; class = strtok(NULL, sep)) {
		if (cmd->mcnt + 1 > DI_MAX_MATCH_CLASSES) {
			errx(EX_DATAERR,
			    "maximum number of classes limited to %d",
			    DI_MAX_MATCH_CLASSES);
		}

		if (class[0] == DI_CLASS_NO_CHAR) {
			v = strtonum(&class[1], 0, 65535,
			    (const char **)&errptr);
			if (errptr != NULL) {
				errx(EX_USAGE, "invalid class number: %s",
				    errptr);
			}
			cmd->match_classes[cmd->mcnt] = v;

		} else if (!str_isalnum(class)) {
			errx(EX_USAGE, "class names can only contain "
			    "alphanumeric characters");
		}

		if (strlen(class) > DI_MAX_NAME_STR_LEN - 1) {
			errx(EX_DATAERR, "class name cannot be longer than %d "
			    "chars", DI_MAX_NAME_STR_LEN - 1);
		}
		strncpy(cmd->clnames[cmd->mcnt], class, DI_MAX_NAME_STR_LEN);
		cmd->clnames[cmd->mcnt][DI_MAX_NAME_STR_LEN - 1] = '\0';
		cmd->mcnt++;
		len += DI_MAX_NAME_STR_LEN;
	}

	free(tmp);
	cmd->o.len |= (len + 3) / sizeof(uint32_t);
	DID2("classifier %s", cmd->cname);
	DID2("classes %d", cmd->mcnt);
}

/*
 * Helper functions (from ipfw2.c).
 */

static void
fill_cmd(ipfw_insn *cmd, enum ipfw_opcodes opcode, int flags, uint16_t arg)
{

	cmd->opcode = opcode;
	cmd->len = ((cmd->len | flags) & (F_NOT | F_OR)) | 1;
	cmd->arg1 = arg;
}

static ipfw_insn *
next_cmd(ipfw_insn *cmd)
{

	cmd += F_LEN(cmd);
	bzero(cmd, sizeof(*cmd));

	return (cmd);
}

int
diffuse_parse_action(int token, ipfw_insn *action, char **avp[])
{
	char **av;

	av = *avp;

	switch(token) {
	case TOK_DI_ML_CLASSIFY:
		classifier = (ipfw_insn_ml_classify *)action;
		NEED1("missing classifier name");
		action->opcode = O_DI_ML_CLASSIFY;
		action->len = F_INSN_SIZE(ipfw_insn_ml_classify);
		if (strlen(av[0]) > DI_MAX_NAME_STR_LEN - 1) {
			errx(EX_DATAERR, "classifier name cannot be longer "
			    "than %d chars", DI_MAX_NAME_STR_LEN - 1);
		}
		strncpy(classifier->cname, av[0], DI_MAX_NAME_STR_LEN - 1);
		classifier->cname[DI_MAX_NAME_STR_LEN - 1] = '\0';
		(*avp)++;
		break;

	case TOK_DI_EXPORT:
		NEED1("missing export name");
		action->opcode = O_DI_EXPORT;
		action->len = F_INSN_SIZE(ipfw_insn_export);
		ipfw_insn_export *exp = (ipfw_insn_export *)action;
		if (strlen(av[0]) > DI_MAX_NAME_STR_LEN - 1) {
			errx(EX_DATAERR, "export name cannot be longer "
			    "than %d chars", DI_MAX_NAME_STR_LEN - 1);
		}
		strncpy(exp->ename, av[0], DI_MAX_NAME_STR_LEN - 1);
		exp->ename[DI_MAX_NAME_STR_LEN - 1] = '\0';
		(*avp)++;
		break;

	default:
		/* Don't know. */
		return (1);
	}

	return (0);
}

int
diffuse_parse_cmd(int token, int open_par, ipfw_insn **cmd, char **avp[])
{
	ipfw_insn_ml_classify *cl;
	char **av;
	char *errptr;
	double prob;

	av = *avp; /* av is used by the NEED macro. */

	switch(token) {
	case TOK_DI_FEATURES:
		NEED1("features, missing feature list");
		if (open_par) {
			errx(EX_USAGE, "features cannot be part of an or "
			    "block");
		}
		if (features) {
			errx(EX_USAGE, "only one features command allowed, "
			    "which must be defined at the start");
		}
		(*cmd)->opcode = O_DI_FEATURES;
		fill_features((ipfw_insn_features *)*cmd, av[0]);
		features = (ipfw_insn_features *)*cmd;
		(*avp)++;
		break;

	case TOK_DI_UNIDIRECTIONAL:
		if (open_par) {
			errx(EX_USAGE, "unidirectional cannot be part of an or "
			    "block");
		}
		GENERATE_FEATURES(*cmd);
		features->ftype &= ~DI_FLOW_TYPE_BIDIRECTIONAL;
		break;

	case TOK_DI_FEATURE_MATCH:
		(*avp)--;
		av = *avp;
		GENERATE_FEATURES(*cmd);
		(*cmd)->opcode = O_DI_FEATURE_MATCH;
		fill_feature_match((ipfw_insn_feature_match *)*cmd, av[0]);
		have_feature_match = 1;
		(*avp)++;
		break;

	case TOK_DI_EVERY:
		/* Default behaviour, nothing to do here. */
		break;

	case TOK_DI_ONCE:
		if (open_par) {
			errx(EX_USAGE, "once cannot be part of an or block");
		}
		GENERATE_FEATURES(*cmd);
		features->ftype |= DI_MATCH_ONCE;
		break;

	case TOK_DI_ONCE_CLASS:
		if (open_par) {
			errx(EX_USAGE, "once-classified cannot be part of an "
			    "or block");
		}
		GENERATE_FEATURES(*cmd);
		features->ftype |= DI_MATCH_ONCE_CLASS;
		break;

	case TOK_DI_ONCE_EXP:
		if (open_par) {
			errx(EX_USAGE, "once-classified cannot be part of an "
			    "or block");
		}
		GENERATE_FEATURES(*cmd);
		features->ftype |= DI_MATCH_ONCE_EXP;
		break;

	case TOK_DI_SAMPLE_REG:
		NEED1("sample, missing number of packets");
		if (open_par)
			errx(EX_USAGE, "sample cannot be part of an or block");
		GENERATE_FEATURES(*cmd);
		features->ftype |= DI_MATCH_SAMPLE_REG;
		errptr = NULL;
		features->sample_int = strtonum(av[0], 1, 65535,
		    (const char **)&errptr);
		if (errptr != NULL) {
			errx(EX_USAGE, "sample interval '%s' invalid, %s",
			    av[0], errptr);
		}
		(*avp)++;
		break;

	case TOK_DI_SAMPLE_RAND:
		NEED1("rnd-sample, missing probability");
		if (open_par) {
			errx(EX_USAGE, "rnd-sample cannot be part of an or "
			    "block");
		}
		GENERATE_FEATURES(*cmd);
		features->ftype |= DI_MATCH_SAMPLE_RAND;
		errptr = NULL;
		prob = strtod(av[0], &errptr);
		if (*errptr) {
			errx(EX_USAGE, "sample probability invalid at '%s'",
			    errptr);
		}
		features->sample_prob = (uint32_t)floor(prob * 0xFFFFFFFF);
		(*avp)++;
		break;

	case TOK_DI_MATCH_IF_CLASS:
		NEED1("match-if-class, missing classifier:class[,class...]");
		GENERATE_FEATURES(*cmd);
		/*
		 * If somebody (unnecessarily) uses multiple match-ifs on same
		 * classifier, we get multiple redundant
		 * O_DI_ML_CLASSIFY_IMPLICIT.
		 */
		(*cmd)->opcode = O_DI_ML_CLASSIFY_IMPLICIT;
		(*cmd)->len = F_INSN_SIZE(ipfw_insn_ml_classify);
		cl = (ipfw_insn_ml_classify *)*cmd;
		cl->cname[0] = '\0';
		*cmd = next_cmd(*cmd);
		(*cmd)->opcode = O_DI_MATCH_IF_CLASS;
		fill_match_if_class((ipfw_insn_match_if_class *)*cmd,
		    cl, av[0]);
		(*avp)++;
		break;

	case TOK_DI_CLASS_TAGS:
		NEED(classifier,
		    "class-tags can be only used with an mlclass action");
		NEED1("class-tags, missing list of class tags");
		if (open_par) {
			errx(EX_USAGE, "class-tags cannot be part of an or "
			    "block");
		}
		(*cmd)->opcode = O_DI_CLASS_TAGS;
		fill_class_tags((ipfw_insn_class_tags *)*cmd, classifier,
		    av[0]);
		(*avp)++;
		break;

	default:
		/* Don't know. */
		return (1);
	}

	return (0);
}

/* Called at the start before any other opcodes, except O_PROB. */
void
diffuse_rule_build_1(uint32_t cmdbuf[], ipfw_insn *cmd, ipfw_insn **dst)
{
	ipfw_insn *src;
	int i, j;

	/* Generate an O_DI_FLOW_TABLE at the start if rule uses features. */
	if (features || classifier) {
		fill_cmd(*dst, O_DI_FLOW_TABLE, 0, 0);
		*dst = next_cmd(*dst);
	}

	/*
	 * ML classifiers are linked to features in kernel because features are
	 * specified during config, so we only need to handle feature matches
	 * here.
	 */
	if (!have_feature_match)
		return;

	/*
	 * Make sure O_DI_FEATURES contains all features needed in matches. It
	 * can contain more if specified by user.
	 */
	for (src = (ipfw_insn *)cmdbuf; src != cmd; src += i) {
		i = F_LEN(src);

		if (src->opcode == O_DI_FEATURE_MATCH) {
			ipfw_insn_feature_match *fm =
			    (ipfw_insn_feature_match *)src;
			for (j = 0; j < features->fcnt; j++) {
				if (strcmp(fm->fname, features->fnames[j]) ==
				    0) {
					if (fm->fdir == DI_MATCH_DIR_BCK ||
					    fm->fdir == DI_MATCH_DIR_FWD) {
						features->ftype |=
						    DI_FLOW_TYPE_BIDIRECTIONAL;
					}
					break;
				}
			}
			if (j >= features->fcnt) {
				if (j >= DI_MAX_FEATURES) {
					errx(EX_DATAERR, "maximum number of "
					    "features limited to %d",
					    DI_MAX_FEATURES);
				}
				strcpy(features->fnames[features->fcnt++],
				    fm->fname);
			}
			if (fm->fdir == DI_MATCH_DIR_BCK ||
			    fm->fdir == DI_MATCH_DIR_FWD) {
				features->ftype |= DI_FLOW_TYPE_BIDIRECTIONAL;
			}
		}
	}
}

/*
 * Called after all non-action opcodes except O_CHECK_STATE, or in other words
 * before options.
 */
void
diffuse_rule_build_2(uint32_t cmdbuf[], ipfw_insn *cmd, ipfw_insn **dst)
{

	/* Generate an O_DI_FEATURES_IMPLICIT if we have an mlclass action. */
	if (classifier && !features) {
		(*dst)->opcode = O_DI_FEATURES_IMPLICIT;
		fill_features((ipfw_insn_features *)*dst, NULL);
		*dst = next_cmd(*dst);
	}
}
