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

/*
 * Machine learning classsifier and remote action nodes support (DIFFUSE)
 * http://www.caia.swin.edu.au/urp/diffuse
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>

#include <net/if.h>

#include <netinet/in.h>
#include <netinet/ip_var.h>
#include <netinet/ip_fw.h>
#include <netinet/ip_diffuse.h>
#include <netinet/ip_diffuse_export.h>

#include <netinet/ipfw/diffuse_common.h>
#include <netinet/ipfw/diffuse_feature.h>
#include <netinet/ipfw/diffuse_feature_module.h>
#include <netinet/ipfw/diffuse_classifier.h>
#include <netinet/ipfw/diffuse_classifier_module.h>
#include <netinet/ipfw/diffuse_private.h>
#include <netinet/ipfw/ip_fw_private.h>

MALLOC_DEFINE(M_DIFFUSE, "DIFFUSE", "DIFFUSE heap");

/* Global config. */
struct di_parms di_config;

/* Callout stuff for timeouts. */
static struct callout di_timeout;

static void
diffuse_timeout(void *unused __unused)
{
	struct di_export *ex;

	if (di_config.an_rule_removal == DIP_TIMEOUT_NONE)
		diffuse_ft_check_timeouts(diffuse_export_add_rec);

	DI_RLOCK();

	/* Kick the actual export function for each export. */
	LIST_FOREACH(ex, &di_config.export_list, next) {
		diffuse_export_send(ex);
	}

	DI_UNLOCK();

	callout_reset(&di_timeout, hz / 10, diffuse_timeout, NULL);
}

#ifdef SYSCTL_NODE
SYSBEGIN(xxx)
SYSCTL_DECL(_net_inet);
SYSCTL_DECL(_net_inet_ip);
SYSCTL_DECL(_net_inet_ip_diffuse);
SYSCTL_NODE(_net_inet_ip, OID_AUTO, diffuse, CTLFLAG_RW, 0, "DIFFUSE");
SYSEND
#endif

/*
 * Feature code.
 */

static struct di_feature *
search_feature_instance(const char *name)
{
	struct di_feature *s = NULL;

	DI_LOCK_ASSERT();

	LIST_FOREACH(s, &di_config.feature_inst_list, next) {
		if (strcmp(s->name, name) == 0)
			return (s);
	}

	return (NULL);
}

/* Return 1 if there is any feature instance that is still used. */
static int
feature_instance_used(void)
{
	struct di_feature *s;

	DI_RLOCK();

	LIST_FOREACH(s, &di_config.feature_inst_list, next) {
		if (s->ref_count > 0) {
			DI_UNLOCK();
			return (1);
		}
	}

	DI_UNLOCK();

	return (0);
}

static struct di_feature_alg *
search_feature_alg(const char *name)
{
	struct di_feature_alg *s;

	DI_LOCK_ASSERT();

	SLIST_FOREACH(s, &di_config.feature_list, next) {
		if (strcmp(s->name, name) == 0)
			return (s);
	}

	return (NULL);
}

/* Return index to feature, used for flowtable show. */
int
diffuse_get_feature_idx(const char *name)
{
	struct di_feature *s;
	int c;

	c = 0; /* Index into feature list. */

	DI_LOCK_ASSERT();

	LIST_FOREACH(s, &di_config.feature_inst_list, next) {
		if (strcmp(s->name, name) == 0)
			return (c);

		c++;
	}

	return (-1);
}

static int
add_feature_instance(const char *alg_name, const char *name,
    struct di_oid *params)
{
	struct di_feature *s, *f;
	struct di_feature_alg *a;

	DI_WLOCK_ASSERT();

	DID("feature add %s %s", alg_name, name);

	/* Check if instance exists. */
	s = search_feature_instance(name);

	if (s == NULL) {
		/* Check algo exits. */
		a = search_feature_alg(alg_name);
		if (a == NULL) {
			DID("feature %s algo doesn't exist", alg_name);
			return (1);
		}

		f = malloc(sizeof(struct di_feature), M_DIFFUSE,
		    M_NOWAIT | M_ZERO);
		if (f == NULL)
			return (ENOMEM);

		f->name = malloc(strlen(name) + 1, M_DIFFUSE,
		    M_NOWAIT | M_ZERO);
		if (f->name == NULL) {
			free(f, M_DIFFUSE);
			return (ENOMEM);
		}
		strcpy(f->name, name);
		f->ref_count = 0;
		f->alg = a;
		if (f->alg->init_instance(&f->conf, params)) {
			free(f->name, M_DIFFUSE);
			free(f, M_DIFFUSE);
			return (ENOMEM);
		}

		LIST_INSERT_HEAD(&di_config.feature_inst_list, f, next);

		a->ref_count++;
		di_config.feature_count++;

		return (0);
	} else {
		DID("feature %s already exists", name);
		return (1);
	}
}

static int
remove_feature_instance(char *name)
{
	struct di_feature *s, *tmp;

	DI_WLOCK_ASSERT();

	LIST_FOREACH_SAFE(s, &di_config.feature_inst_list, next, tmp) {
		if (!strcmp(s->name, name) && s->ref_count == 0) {
			LIST_REMOVE(s, next);
			free(s->name, M_DIFFUSE);
			s->alg->destroy_instance(&s->conf);
			s->alg->ref_count--;
			free(s, M_DIFFUSE);
			di_config.feature_count--;
			return (0);
		}
	}

	DID("feature %s can't remove", name);
	return (1);
}

/*
 * If name == NULL we try to remove all instances, otherwise we only remove
 * instances of the particular module.
 */
static int
diffuse_remove_feature_instances(const char *name, int force)
{
	struct di_feature *tmp, *r;

	DI_WLOCK();

	LIST_FOREACH_SAFE(r, &di_config.feature_inst_list, next, tmp) {
		if ((name == NULL || !strcmp(r->alg->name, name)) &&
		    (r->ref_count == 0 || force)) {
			LIST_REMOVE(r, next);
			free(r->name, M_DIFFUSE);
			r->alg->destroy_instance(&r->conf);
			r->alg->ref_count--;
			free(r, M_DIFFUSE);
			di_config.feature_count--;
		}
	}

	DI_UNLOCK();

	return (0);
}

/*
 * Classifier code.
 */

static struct di_classifier *
search_classifier_instance(const char *name)
{
	struct di_classifier *s;

	DI_LOCK_ASSERT();

	LIST_FOREACH(s, &di_config.classifier_inst_list, next) {
		if (strcmp(s->name, name) == 0)
			return (s);
	}

	return (NULL);
}

static struct di_classifier_alg *
search_classifier_alg(const char *name)
{
	struct di_classifier_alg *s;

	DI_LOCK_ASSERT();

	SLIST_FOREACH(s, &di_config.classifier_list, next) {
		if (strcmp(s->name, name) == 0)
			return (s);
	}

	return (NULL);
}

/* Return 1 if there is any feature instance that is still used. */
static int
classifier_instance_used(void)
{
	struct di_classifier *s;

	DI_WLOCK();

	LIST_FOREACH(s, &di_config.classifier_inst_list, next) {
		if (s->ref_count > 0) {
			DI_UNLOCK();
			return (1);
		}
	}

	DI_UNLOCK();

	return (0);
}

static int
add_classifier_instance(struct di_ctl_classifier *class, struct di_oid *params)
{
	static uint32_t clid_counter = 0;
	struct di_classifier *s, *c;
	struct di_classifier_alg *a;
	char *alg_name, *name;
	int needed, ret;

	alg_name = class->mod_name;
	name = class->name;
	ret = 0;

	DI_WLOCK_ASSERT();

	DID("classifier add %s %s", alg_name, name);

	/* Check if instance exists. */
	s = search_classifier_instance(name);

	if (s == NULL) {
		/* Check algo exits. */
		a = search_classifier_alg(alg_name);
		if (a == NULL) {
			DID("classifier %s algo doesn't exist", alg_name);
			return (1);
		}

		c = malloc(sizeof(struct di_classifier), M_DIFFUSE,
		    M_NOWAIT | M_ZERO);
		if (c == NULL)
			return (ENOMEM);

		if ((ret = a->init_instance(&c->conf, params)))
			goto error;

		/* Check number of features. */
		needed = a->get_feature_cnt(&c->conf);
		if (class->fscnt < needed) {
			DID("classifier %s needs %d features", name, needed);
			ret = EINVAL;
			goto error;
		}
		if (class->fscnt > needed)
			DID("classifier %s needs %d features, ignoring rest",
			    name, needed);

		c->name = malloc(strlen(name) + 1, M_DIFFUSE,
		    M_NOWAIT | M_ZERO);
		if (c->name == NULL) {
			ret = ENOMEM;
			goto error;
		}

		strcpy(c->name, name);
		c->fscnt = class->fscnt;
		c->ccnt = class->ccnt;
		c->fstats = malloc((c->fscnt + c->ccnt) *
		    sizeof(struct di_feature_stat), M_DIFFUSE,
		    M_NOWAIT | M_ZERO);
		if (c->fstats == NULL) {
			ret = ENOMEM;
			goto error;
		}

		memcpy(c->fstats, class->fstats, (c->fscnt + c->ccnt) *
		    sizeof(struct di_feature_stat));

		c->fstats_ptr = malloc(c->fscnt *
		    sizeof(struct di_feature_stat_ptr), M_DIFFUSE,
		    M_NOWAIT | M_ZERO);
		if (c->fstats_ptr == NULL) {
			ret = ENOMEM;
			goto error;
		}

		c->ref_count = 0;
		c->alg = a;
		c->confirm = class->confirm;
		c->id = clid_counter++;

		LIST_INSERT_HEAD(&di_config.classifier_inst_list, c, next);

		a->ref_count++;
		di_config.classifier_count++;

		return (0);

error:
		free(c->name, M_DIFFUSE);
		free(c->fstats, M_DIFFUSE);
		free(c->fstats_ptr, M_DIFFUSE);
		a->destroy_instance(&c->conf);
		free(c, M_DIFFUSE);

		return (ret);
	} else {
		DID("classifier %s already exists", name);
		return (1);
	}
}

static int
remove_classifier_instance(char *name)
{
	struct di_classifier *s, *tmp;

	DI_WLOCK_ASSERT();

	LIST_FOREACH_SAFE(s, &di_config.classifier_inst_list, next, tmp) {
		if (!strcmp(s->name, name) && s->ref_count == 0) {
			LIST_REMOVE(s, next);
			free(s->name, M_DIFFUSE);
			free(s->fstats, M_DIFFUSE);
			free(s->fstats_ptr, M_DIFFUSE);
			s->alg->destroy_instance(&s->conf);
			s->alg->ref_count--;
			free(s, M_DIFFUSE);
			di_config.classifier_count--;

			return (0);
		}
	}

	DID("classifier %s can't remove", name);
	return (1);
}

/*
 * If name == NULL we try to remove all instances, otherwise we only remove
 * instances of the particular module.
 */
static int
diffuse_remove_classifier_instances(const char *name, int force)
{
	struct di_classifier *tmp, *r;

	DI_WLOCK();

	LIST_FOREACH_SAFE(r, &di_config.classifier_inst_list, next, tmp) {
		if ((name == NULL || !strcmp(r->alg->name, name)) &&
		    (r->ref_count == 0 || force)) {
			LIST_REMOVE(r, next);
			free(r->name, M_DIFFUSE);
			free(r->fstats, M_DIFFUSE);
			free(r->fstats_ptr, M_DIFFUSE);
			r->alg->destroy_instance(&r->conf);
			r->alg->ref_count--;
			free(r, M_DIFFUSE);
			di_config.classifier_count--;
		}
	}

	DI_UNLOCK();

	return (0);
}

/*
 * Export code.
 */

static struct di_export *
search_export_instance(const char *name)
{
	struct di_export *s;

	DI_LOCK_ASSERT();

	LIST_FOREACH(s, &di_config.export_list, next) {
		if (strcmp(s->name, name) == 0)
			return (s);
	}

	return (NULL);
}

/* Return 1 if there is any feature instance that is still used. */
static int
export_instance_used(void)
{
	struct di_export *s;

	DI_WLOCK();

	LIST_FOREACH(s, &di_config.export_list, next) {
		if (s->ref_count > 0) {
			DI_UNLOCK();
			return (1);
		}
	}

	DI_UNLOCK();

	return (0);
}

static int
add_export_instance(const char *name, struct di_export *e_conf,
    struct socket *sock)
{
	struct di_export *s, *e;

	DI_WLOCK_ASSERT();

	DID("export add %s", name);

	/* Check if instance exists. */
	s = search_export_instance(name);

	if (s == NULL) {
		e = malloc(sizeof(struct di_export), M_DIFFUSE,
		    M_NOWAIT | M_ZERO);
		if (e == NULL)
			return (ENOMEM);

		e->name = malloc(strlen(name) + 1, M_DIFFUSE,
		    M_NOWAIT | M_ZERO);
		if (e->name == NULL) {
			free(e, M_DIFFUSE);
			return (ENOMEM);
		}

		strcpy(e->name, name);
		e->ref_count = 0;
		e->conf = e_conf->conf;
		e->sock = NULL;
		e->seq_no = 0;
		e->last_pkt_time.tv_sec = 0;
		e->last_pkt_time.tv_usec = 0;
		e->mh = NULL;
		e->mt = NULL;
		e->sock = sock;

		LIST_INSERT_HEAD(&di_config.export_list, e, next);
		di_config.export_count++;

		return (0);
	} else {
		DID("export %s already exists", name);
		return (1);
	}
}

static int
remove_export_instance(char *name)
{
	struct di_export *s, *tmp;

	DI_WLOCK_ASSERT();

	LIST_FOREACH_SAFE(s, &di_config.export_list, next, tmp) {
		if (s != NULL && s->ref_count == 0) {
			LIST_REMOVE(s, next);

			diffuse_export_close(s->sock);

			free(s->name, M_DIFFUSE);
			if (s->mh != NULL)
				m_freem(s->mh);
			free(s, M_DIFFUSE);
			di_config.export_count--;

			return (0);
		}
	}

	DID("export %s can't remove", name);
	return (1);
}

static int
diffuse_remove_export_instances(int force)
{
	struct di_export *tmp, *r;

	DI_WLOCK();

	LIST_FOREACH_SAFE(r, &di_config.export_list, next, tmp) {
		if (r->ref_count == 0 || force) {
			LIST_REMOVE(r, next);
			DID("closing socket for export %s\n", r->name);
			diffuse_export_close(r->sock);
			free(r->name, M_DIFFUSE);
			if (r->mh != NULL)
				m_freem(r->mh);
			free(r, M_DIFFUSE);
			di_config.export_count--;
		}
	}

	DI_UNLOCK();

	return (0);
}

/*
 * Code called from IPFW.
 */

static int
chk_features(ipfw_insn_features *cmd)
{
	int c;

	DI_WLOCK_ASSERT();

	for (c = 0; c < cmd->fcnt; c++) {
		cmd->fptrs[c] = search_feature_instance(cmd->fnames[c]);
		if (cmd->fptrs[c] == NULL) {
			DID("feature %s doesn't exist", cmd->fnames[c]);
			return (1);
		}
	}

	return (0);
}

static int
chk_export(ipfw_insn_export *cmd)
{
	DI_WLOCK_ASSERT();

	cmd->eptr = search_export_instance(cmd->ename);
	if (cmd->eptr == NULL) {
		DID("export %s doesn't exist", cmd->ename);
		return (1);
	}

	return (0);
}

static int
chk_classifier(ipfw_insn_ml_classify *cmd, ipfw_insn_features *fcmd)
{
	struct di_classifier *class;
	char **snames;
	int i, j, scnt;

	scnt = 0;

	DI_WLOCK_ASSERT();

	class = cmd->clptr = search_classifier_instance(cmd->cname);
	if (cmd->clptr == NULL) {
		DID("classifier %s doesn't exist", cmd->cname);
		return (1);
	}

	/* Add features to list and get indices. */
	for (i = 0; i < class->fscnt; i++) {
		class->fstats_ptr[i].fptr = NULL;

		for (j = 0; j < fcmd->fcnt; j++) {
			if (!strcmp(class->fstats[i].fname, fcmd->fnames[j])) {
				class->fstats_ptr[i].fptr = fcmd->fptrs[j];
				break;
			}
		}

		if (class->fstats_ptr[i].fptr == NULL) {
			/* Add feature to feature list if exists. */
			fcmd->fptrs[fcmd->fcnt] =
			    search_feature_instance(class->fstats[i].fname);
			if (fcmd->fptrs[fcmd->fcnt] == NULL) {
				DID("feature %s doesn't exist",
				    class->fstats[i].fname);
				return (1);
			}

			strcpy(fcmd->fnames[fcmd->fcnt],
			    class->fstats[i].fname);
			class->fstats_ptr[i].fptr = fcmd->fptrs[fcmd->fcnt];
			fcmd->fcnt++;
		}

		/*
		 * If direction is indicated or feature module expects
		 * bidirectional, must set bidirectional.
		 */
		if (class->fstats[i].fdir == DI_MATCH_DIR_BCK ||
		    class->fstats[i].fdir == DI_MATCH_DIR_FWD ||
		    class->fstats_ptr[i].fptr->alg->type &
		    DI_FEATURE_ALG_BIDIRECTIONAL) {
			fcmd->ftype |= DI_FLOW_TYPE_BIDIRECTIONAL;
		}
		/* Can't have direction with bidirectional module. */
		if (class->fstats[i].fdir != DI_MATCH_DIR_NONE &&
		    class->fstats_ptr[i].fptr->alg->type &
		    DI_FEATURE_ALG_BIDIRECTIONAL) {
			class->fstats[i].fdir = DI_MATCH_DIR_NONE;
		}
	}

	/* Get statistics indices. */
	for (i = 0; i < class->fscnt; i++) {
		scnt = class->fstats_ptr[i].fptr->alg->get_stat_names(&snames);
		class->fstats_ptr[i].sidx = 0;

		for (j = 0; j < scnt; j++) {
			if (!strcmp(class->fstats[i].sname, snames[j]))
				break;

			class->fstats_ptr[i].sidx++;
		}
		if (class->fstats_ptr[i].sidx >= scnt) {
			DID("feature %s has no statistic %s",
			    class->fstats[i].fname, class->fstats[i].sname);

			return (1);
		}
	}

	return (0);
}

static int
chk_class_tags(ipfw_insn_class_tags *cmd)
{
	int class_cnt;

	DI_WLOCK_ASSERT();

	cmd->clptr = search_classifier_instance(cmd->cname);
	if (cmd->clptr == NULL) {
		DID("classifier %s doesn't exist", cmd->cname);
		return (1);
	}

	class_cnt = cmd->clptr->alg->get_class_cnt(&cmd->clptr->conf);
	if (cmd->tcnt > class_cnt) {
		DID("cannot have more tags (%d) than classifier %s has "
		    "classes (%d)", cmd->tcnt, cmd->cname, class_cnt);
		return (1);
	}

	return (0);
}

static int
chk_match_if_class(ipfw_insn_match_if_class *cmd)
{
	int i, j;

	DI_WLOCK_ASSERT();

	cmd->clptr = search_classifier_instance(cmd->cname);
	if (cmd->clptr == NULL) {
		DID("classifier %s doesn't exist", cmd->cname);
		return (1);
	}

	/* Get class numbers from names. */
	for (i = 0; i < cmd->mcnt; i++) {
		if (cmd->clnames[i][0] != DI_CLASS_NO_CHAR) {
			for (j = 0; j < cmd->clptr->ccnt; j++) {
				if (!strcmp(cmd->clnames[i],
				    cmd->clptr->
				    fstats[cmd->clptr->fscnt + j].fname)) {
					break;
				}
			}
			if (j < cmd->clptr->ccnt) {
				cmd->match_classes[i] = j;
			} else {
				DID("classifier %s has no class %s",
				    cmd->cname, cmd->clnames[i]);
				return (1);
			}
		}
	}

	return (0);
}

/* We can assume that we have had a prior ipfw_insn_features. */
static int
chk_feature_match(ipfw_insn_feature_match *cmd, ipfw_insn_features *fcmd)
{
	char **snames;
	int i, scnt;

	scnt = 0;

	DI_WLOCK_ASSERT();

	cmd->fptr = NULL;
	for (i = 0; i < fcmd->fcnt; i++) {
		if (!strcmp(cmd->fname, fcmd->fnames[i])) {
			cmd->fptr = fcmd->fptrs[i];
			break;
		}
	}
	if (cmd->fptr == NULL) {
		DID("feature %s doesn't exist", cmd->fname);
		return (1);
	}

	scnt = cmd->fptr->alg->get_stat_names(&snames);
	cmd->sidx = 0;
	for (i = 0; i < scnt; i++) {
		if (!strcmp(cmd->sname, snames[i]))
			break;

		cmd->sidx++;
	}
	if (cmd->sidx >= scnt) {
		DID("feature %s has no statistic called %s",
		    cmd->fname, cmd->sname);

		return (1);
	}

	/* Can't have direction with bidirectional module. */
	if (cmd->fdir != DI_MATCH_DIR_NONE &&
	    cmd->fptr->alg->type & DI_FEATURE_ALG_BIDIRECTIONAL) {
		cmd->fdir = DI_MATCH_DIR_NONE;
	}

	return (0);
}

static int
ref_cnt_features(ipfw_insn_features *cmd, int inc)
{
	int c;

	DI_WLOCK_ASSERT();

	for (c = 0; c < cmd->fcnt; c++) {
		/* Pointers have been set before in chk_features. */
		if (inc)
			cmd->fptrs[c]->ref_count++;
		else
			cmd->fptrs[c]->ref_count--;

		DID("feature %s ref cnt %d", cmd->fptrs[c]->name,
		    cmd->fptrs[c]->ref_count);
	}

	return (0);
}

static int
ref_cnt_classifier(ipfw_insn_ml_classify *cmd, int inc)
{
	DI_WLOCK_ASSERT();

	/* Pointer has been set before in chk_classifier. */
	if (inc)
		cmd->clptr->ref_count++;
	else
		cmd->clptr->ref_count--;

	DID("classifier %s ref cnt %d", cmd->cname, cmd->clptr->ref_count);

	return (0);
}

static int
ref_cnt_export(ipfw_insn_export *cmd, int inc)
{
	DI_WLOCK_ASSERT();

	/* Pointer has been set before in chk_export */
	if (inc)
		cmd->eptr->ref_count++;
	else
		cmd->eptr->ref_count--;

	DID("export %s ref cnt %d\n", cmd->ename, cmd->eptr->ref_count);

	return (0);
}

static int
diffuse_chk_rule_cmd(struct di_chk_rule_cmd_args *di_args, ipfw_insn *cmd,
    int *have_action)
{
	switch(cmd->opcode) {
	case O_DI_BEFORE_RULE_CHK:
		/* Prior to checking any commands. */
		break;

	case O_DI_AFTER_RULE_CHK:
		/* All rule instructions have been successfully checked. */
		break;

	case O_DI_FEATURES:
	case O_DI_FEATURES_IMPLICIT:
		if (cmd->len != F_INSN_SIZE(ipfw_insn_features))
                	goto bad_size;
		break;

	case O_DI_FLOW_TABLE:
		if (cmd->len != F_INSN_SIZE(ipfw_insn))
			goto bad_size;
		break;

	case O_DI_FEATURE_MATCH:
		if (cmd->len != F_INSN_SIZE(ipfw_insn_feature_match))
			goto bad_size;
		break;

	case O_DI_ML_CLASSIFY:
	case O_DI_ML_CLASSIFY_IMPLICIT:
		if (cmd->len != F_INSN_SIZE(ipfw_insn_ml_classify))
			goto bad_size;
		if (cmd->opcode == O_DI_ML_CLASSIFY)
			*have_action = 1;
		break;

	case O_DI_CLASS_TAGS:
		break;

	case O_DI_EXPORT:
		if (cmd->len != F_INSN_SIZE(ipfw_insn_export))
			goto bad_size;
		*have_action = 1;
		break;

	case O_DI_MATCH_IF_CLASS:
		if (cmd->len <= F_INSN_SIZE(ipfw_insn_match_if_class))
			goto bad_size;
		break;

	default:
		return (-1);
	}

	return (0);

bad_size:
	DID("opcode %d size %d wrong", cmd->opcode, cmd->len);

	return (EINVAL);
}

/* Add IPFW tag to packet. */
static void
ipfw_tag_packet(struct mbuf *m, uint16_t tag)
{
	struct m_tag *mtag;

	mtag = m_tag_locate(m, MTAG_IPFW, tag, NULL);
	if (mtag == NULL) {
		if ((mtag = m_tag_alloc(MTAG_IPFW, tag, 0, M_NOWAIT)) != NULL)
			m_tag_prepend(m, mtag);
	}
}

static int
diffuse_chk_pkt_cmd(struct di_chk_pkt_args *di_args, struct ip_fw *f,
    int opcode, ipfw_insn **cmd, int *cmdlen, struct ip_fw_args *args,
    void *ulp, int pktlen, int *dir, int *match, int *l, int *done, int *retval)
{

	switch(opcode) {
	case O_DI_BEFORE_ALL_RULES:
		/*
		 * We cache flow entry q for quick access to feature data, as q
		 * cannot be deleted while in this function. Need to take care
		 * when reading/writing from q!
		 */
		di_args->q = NULL;
		di_args->tcmd = NULL;
		di_args->no_class = 0;
		DI_RLOCK();
		break;

        case O_DI_FEATURES:
        case O_DI_FEATURES_IMPLICIT:
		{
		*match = 1;

		if (*dir == MATCH_UNKNOWN || *dir == MATCH_NONE) {
			if ((di_args->q = diffuse_ft_install_state(f,
			    (ipfw_insn_features *)*cmd, args, ulp,
			    pktlen)) == NULL) {
				/* Exit. */
				*retval = IP_FW_DENY;
				*l = 0;
				*done = 1;
			}
		} else if (di_args->q) {
			/*
			 * Check if we need to add further features for
			 * this rule.
			 */
			if (diffuse_ft_update_features(di_args->q,
			    (ipfw_insn_features *)*cmd, args,
			    ulp) < 0) {
				*match = 0;
				*l = 0;
			}
		}
		}
		break;

	case O_DI_FLOW_TABLE:
		{
		*match = 1;

		/* Lookup will trigger feature update. */
		if (*dir != MATCH_UNKNOWN ||
		    (di_args->q = diffuse_ft_lookup_entry(&args->f_id, args,
		    ulp, pktlen, dir)) == NULL) {
			break;
		}

		if (di_args->q->ftype & DI_MATCH_ONCE) {
			di_args->no_class = 1;

		} else if (di_args->q->ftype & DI_MATCH_SAMPLE_REG) {
			if (di_args->q->pkts_after_last <
			    di_args->q->sample_int) {
				di_args->q->pkts_after_last++;
				di_args->no_class = 1;
			} else {
				di_args->q->pkts_after_last = 1;
			}

		} else if (di_args->q->ftype & DI_MATCH_SAMPLE_RAND) {
			if (random() >= di_args->q->sample_prob)
				di_args->no_class = 1;

		} else if (di_args->q->ftype & DI_MATCH_ONCE_CLASS) {
			if (!SLIST_EMPTY(&di_args->q->flow_classes))
				di_args->no_class = 1;

		} else if (di_args->q->ftype & DI_MATCH_ONCE_EXP) {
			if (!SLIST_EMPTY(&di_args->q->ex_list))
				di_args->no_class = 1;
		}

		diffuse_ft_unlock();
		}
		break;

	case O_DI_AFTER_EACH_RULE:
		/* Called after every rule. */
		break;

	case O_DI_FEATURE_MATCH:
		{
		ipfw_insn_feature_match *fm;
		int ret;
		int32_t val;

		ret = 0;
		*match = 0;
		fm = (ipfw_insn_feature_match *)*cmd;

		ret = diffuse_ft_get_stat(di_args->q, fm->fdir, fm->fptr,
		    fm->sidx, &val);
		if (!ret)
			break;

		switch(fm->comp) {
		case DI_COMP_LT:
			*match = val < fm->thresh ? 1 : 0;
			break;
		case DI_COMP_LE:
			*match = val <= fm->thresh ? 1 : 0;
			break;
		case DI_COMP_EQ:
			*match = val == fm->thresh ? 1 : 0;
			break;
		case DI_COMP_GE:
			*match = val >= fm->thresh ? 1 : 0;
			break;
		case DI_COMP_GT:
			*match = val > fm->thresh ? 1 : 0;
			break;
		default:
			printf("invalid comparator\n");
		}
		}
		break;

	case O_DI_ML_CLASSIFY:
	case O_DI_ML_CLASSIFY_IMPLICIT:
		{
		ipfw_insn_ml_classify *clcmd = (ipfw_insn_ml_classify *)*cmd;
		struct di_class_tag *ctag;
		struct di_classifier *cl;
		int32_t fvec[clcmd->clptr->fscnt];
		int class, confirm, prev_class, ret;

		cl = clcmd->clptr;
		confirm = ret = 0;
		class = prev_class = -1;
		*match = 1;

		/*
		 * We can have multiple tags per mbuf, but only one tag per
		 * classifier.
		 */
		ctag = (struct di_class_tag *)m_tag_locate(args->m,
		    MTAG_DIFFUSE_CLASS, cl->id, NULL);
		if (ctag != NULL) {
			if (di_args->tcmd &&
			    di_args->tcmd->tcnt > ctag->class) {
				/*
				 * Ensure we get IPFW tags based on previous
				 * classification.
				 */
				ipfw_tag_packet(args->m,
				    di_args->tcmd->tags[ctag->class]);
			}
			goto done;
		}

		/* Get existing class (if any). */
		class = diffuse_ft_get_class(di_args->q, cl->name, &prev_class,
		    &confirm);
		DID2("old class: %d", class);
		if (!di_args->no_class || class == -1) {
			ret = diffuse_ft_get_stats(di_args->q, cl->fscnt,
			    cl->fstats, cl->fstats_ptr, fvec);
			if (ret) {
				class = cl->alg->classify(&cl->conf, fvec,
				    cl->fscnt);
				DID2("new class: %d", class);

				/* Add class tag in flow table. */
				confirm = cl->confirm;
				diffuse_ft_add_class(di_args->q, cl->name,
				    class, &prev_class, &confirm);
			}
		}

		if (class != -1) {
			if ((ctag = (struct di_class_tag *)m_tag_alloc(
			    MTAG_DIFFUSE_CLASS,
			    cl->id,
			    sizeof(struct di_class_tag) - sizeof(struct m_tag),
			    M_NOWAIT)) != NULL) {
				ctag->class = class;
				ctag->prev_class = prev_class;
				ctag->confirm = confirm;
				m_tag_prepend(args->m, (struct m_tag *)ctag);
			}

			if (di_args->tcmd && di_args->tcmd->tcnt > class) {
				/* Tag using IPFW tag. */
				ipfw_tag_packet(args->m,
				    di_args->tcmd->tags[class]);
			}
		}

done:
		if (opcode == O_DI_ML_CLASSIFY) {
			/* Update stats, but only if explicit action.*/
			f->pcnt++;
			f->bcnt += pktlen;
			f->timestamp = time_uptime;
			*l = 0; /* Continue with next rule. */
		}

		}
		break;

	case O_DI_CLASS_TAGS:
		*match = 1;
		di_args->tcmd = (ipfw_insn_class_tags *)*cmd;
		break;

	case O_DI_EXPORT:
		{
		ipfw_insn_export *ex;
		struct di_export_rec *ex_rec;

		ex = (ipfw_insn_export *)*cmd;
		*match = 1;

		if (di_args->q != NULL) {
			ex_rec = NULL;

			if (diffuse_ft_do_export(di_args->q,
			    ex->eptr->conf.confirm)) {
				ex_rec = diffuse_export_add_rec(di_args->q,
				    ex->eptr, 1);
			}
			diffuse_ft_unlock();
			diffuse_ft_add_export(di_args->q, ex_rec, ex->eptr);

			/* Update stats. */
			f->pcnt++;
			f->bcnt += pktlen;
			f->timestamp = time_uptime;
		}

		*l = 0; /* Continue with next rule. */
		}
		break;

	case O_DI_AFTER_ALL_RULES:
		{
		/*
		 * Do this bit after rule parsing outside lock, if we wanted to
		 * use in-kernel shortcut to add/remove rules it must be done
		 * after IPFW_RUNLOCK(&V_layer3_chain)
		 */

		DI_UNLOCK();

		/* Limit total number of recs. */
		diffuse_export_prune_recs();

		DID2("export recs %u", di_config.export_rec_count);

		}
		break;

	case O_DI_MATCH_IF_CLASS:
		{
		ipfw_insn_match_if_class *mic;
		struct di_class_tag *ctag;
		int i;

		mic = (ipfw_insn_match_if_class *)*cmd;
		*match = 0;

#ifdef DIFFUSE_DEBUG2
		{
			struct m_tag *t;
			for (t = m_tag_first(args->m); t != NULL;
			    t = m_tag_next(args->m, t)) {
				DID2("tag id %d len %d", t->m_tag_id,
				    t->m_tag_len);
			}
		}
#endif

		ctag = (struct di_class_tag *)m_tag_locate(args->m,
		    MTAG_DIFFUSE_CLASS, mic->clptr->id, NULL);
		if (ctag != NULL) {
			for (i = 0; i < mic->mcnt; i++) {
				if ((ctag->confirm >= mic->clptr->confirm &&
				    ctag->class == mic->match_classes[i]) ||
				    (mic->clptr->confirm > 0 &&
				    ctag->confirm < mic->clptr->confirm &&
				    ctag->prev_class ==
				    mic->match_classes[i])) {
					*match = 1;
					break;
				}
			}
		}
		}
		break;

	default:
		return (-1);
	}

	return (0);
}

static void
diffuse_remove_rule(struct ip_fw *rule)
{
	int l, cmdlen;
	ipfw_insn *cmd;

	cmdlen = 0;

	/* Decrease features ref counters. */
	for (l = rule->cmd_len, cmd = rule->cmd; l > 0; l -= cmdlen,
	    cmd += cmdlen) {
		cmdlen = F_LEN(cmd);

		/*
		 * We don't need a check here on cmdlen because the checks were
		 * done previously in ipfw_chk_struct.
		 */

		/*
		 * Don't need to handle O_DI_FEATURE_MATCH or O_DI_MATCH_IF
		 * because they generate O_DI_FEATURES, O_DI_ML_CLASSIFY.
		 */

		switch(cmd->opcode) {
		case O_DI_FEATURES:
		case O_DI_FEATURES_IMPLICIT:
			DI_WLOCK();
			ref_cnt_features((ipfw_insn_features *)cmd, 0);
			DI_UNLOCK();
			break;

		case O_DI_ML_CLASSIFY:
		case O_DI_ML_CLASSIFY_IMPLICIT:
			DI_WLOCK();
			ref_cnt_classifier((ipfw_insn_ml_classify *)cmd, 0);
			DI_UNLOCK();
			break;

		case O_DI_EXPORT:
			DI_WLOCK();
			ref_cnt_export((ipfw_insn_export *)cmd, 0);
			DI_UNLOCK();
			/* Remove all export records associated with rule. */
			diffuse_export_remove_recs(
			    ((ipfw_insn_export *)cmd)->ename);
			break;
		}
	}

	/* Remove all entries in flow table associated with rule. */
	diffuse_ft_remove_entries(rule);
}

/* We assume that past this function rule adding WILL succeed. */
static int
diffuse_add_rule(struct ip_fw *rule)
{
	ipfw_insn_features *fcmd;
	ipfw_insn *cmd;
	int cmdlen, l;

	fcmd = NULL;
	cmdlen = 0;

	DI_WLOCK();

	/*
	 * First pass: link rules to instances of features, classifiers,
	 * exports.
	 */
	for (l = rule->cmd_len, cmd = rule->cmd; l > 0; l -= cmdlen,
	    cmd += cmdlen) {
		cmdlen = F_LEN(cmd);

		/*
		 * We don't need check here on cmdlen because the checks were
		 * done previously in ipfw_chk_struct.
		 */

		switch(cmd->opcode) {
		case O_DI_FEATURES:
		case O_DI_FEATURES_IMPLICIT:
			if (chk_features((ipfw_insn_features *)cmd))
				goto bad;

			fcmd = (ipfw_insn_features *)cmd;
			break;

		case O_DI_FEATURE_MATCH:
			if (fcmd == NULL) {
				/* Must have _previous_ feature instruction. */
				goto bad;
			}
			if (chk_feature_match((ipfw_insn_feature_match *)cmd,
			    fcmd)) {
				goto bad;
			}
			break;

		case O_DI_ML_CLASSIFY:
		case O_DI_ML_CLASSIFY_IMPLICIT:
			if (fcmd == NULL) {
				/* Must have _previous_ feature instruction. */
				goto bad;
			}

			if (chk_classifier((ipfw_insn_ml_classify *)cmd,
			    fcmd)) {
				goto bad;
			}
			break;

		case O_DI_CLASS_TAGS:
			if (chk_class_tags((ipfw_insn_class_tags *)cmd))
				goto bad;
			break;

		case O_DI_EXPORT:
			if (chk_export((ipfw_insn_export *)cmd))
				goto bad;
			break;

		case O_DI_MATCH_IF_CLASS:
			if (chk_match_if_class((ipfw_insn_match_if_class *)cmd))
				goto bad;
			break;
		}
	}

	cmdlen = 0;

	/* Second pass: increase ref counters. */
	for (l = rule->cmd_len, cmd = rule->cmd; l > 0; l -= cmdlen,
	    cmd += cmdlen) {
		cmdlen = F_LEN(cmd);

		switch(cmd->opcode) {
		case O_DI_FEATURES:
		case O_DI_FEATURES_IMPLICIT:
			ref_cnt_features((ipfw_insn_features *)cmd, 1);
			break;

		case O_DI_ML_CLASSIFY:
		case O_DI_ML_CLASSIFY_IMPLICIT:
			ref_cnt_classifier((ipfw_insn_ml_classify *)cmd, 1);
			break;

		case O_DI_EXPORT:
			ref_cnt_export((ipfw_insn_export *)cmd, 1);
			break;
		}
	}

	DI_UNLOCK();
	return (0);

bad:
	DI_UNLOCK();
	return (EINVAL);
}

/* Functions called from ipfw */
ipfw_ext_t diffuse_ext = {
	.chk_rule_cmd	= diffuse_chk_rule_cmd,
	.chk_pkt_cmd	= diffuse_chk_pkt_cmd,
	.remove_rule	= diffuse_remove_rule,
	.add_rule	= diffuse_add_rule,
};

/*
 * Configuration functions
 */

/* Config classifier. */
static int
config_classifier(struct di_ctl_classifier *c, struct di_oid *c_conf,
    struct di_oid *arg)
{

	return (add_classifier_instance(c, c_conf));
}

/* Configure a feature. */
static int
config_feature(struct di_ctl_feature *f, struct di_oid *f_conf, struct di_oid *arg)
{

	return (add_feature_instance(f->mod_name, f->name, f_conf));
}

/* Config export. */
static int
config_export(struct di_export *e, struct di_oid *arg)
{
	struct socket *sock;
	int ret;

	sock = diffuse_export_open(&e->conf);
	if (sock == NULL) {
		DID("can't open socket");
		return (1);
	}

	DI_WLOCK();
	ret = add_export_instance(e->name, e, sock);
	DI_UNLOCK();

	if (ret && sock) {
		/* If error need to close open socket. */
		diffuse_export_close(sock);
	}

	return (ret);
}

/* Delete/zero all objects. */
static void
diffuse_flush(int reset_counters_only)
{

	/* XXX: currently we only have flush/zero commands for flow table. */
	DID("flowtable flush %i", reset_counters_only);
	diffuse_ft_flush(reset_counters_only);
}

/* Compute space needed for feature config export. */
static int
compute_space_feature(struct di_oid *cmd)
{
	struct di_feature *s;
	char *name;
	int needed;

	needed = 0;

	if (cmd != NULL)
		name = ((struct di_ctl_feature *)cmd)->name;
	else
		name = "all";
	
	if (strcmp(name, "all")) {
		s = search_feature_instance(name);
		if (s == NULL)
			return (-1);

		needed += sizeof(struct di_ctl_feature);
		needed += s->alg->get_conf(&s->conf, NULL, 1);
	} else {
		/* All feature instances. */
		LIST_FOREACH(s, &di_config.feature_inst_list, next) {
			needed += sizeof(struct di_ctl_feature);
			needed += s->alg->get_conf(&s->conf, NULL, 1);
		}
	}

	return (needed);
}

/*
 * Compute space needed for classifier config export.
 * If name == "all", copy all names but not the confs.
 */
static int
compute_space_class(struct di_oid *cmd)
{
	struct di_classifier *s;
	char *name;
	int needed;

	needed = 0;
	name = ((struct di_ctl_classifier *)cmd)->name;

	if (strcmp(name, "all")) {
		s = search_classifier_instance(name);
		if (s == NULL)
			return (-1);

		needed += sizeof(struct di_ctl_classifier);
		needed += (s->fscnt + s->ccnt)* sizeof(struct di_feature_stat);
		needed += s->alg->get_conf(&s->conf, NULL, 1);
	} else {
		/* All classifier instances. */
		LIST_FOREACH(s, &di_config.classifier_inst_list, next) {
			needed += sizeof(struct di_ctl_classifier);
			needed += (s->fscnt + s->ccnt) * sizeof(struct di_feature_stat);
		}
	}

	return (needed);
}

/*
 * Compute space needed for export config export.
 * If name == "all", copy all names but not the confs.
 */
static int
compute_space_export(struct di_oid *cmd)
{
	struct di_export *s;
	char *name;
	int needed;

	needed = 0;
	name = ((struct di_ctl_export *)cmd)->name;

	if (strcmp(name, "all")) {
		s = search_export_instance(name);
		if (s == NULL)
			return (-1);

		needed += sizeof(struct di_ctl_export);
	} else {
		/* All export instances. */
		LIST_FOREACH(s, &di_config.export_list, next) {
			needed += sizeof(struct di_ctl_export);
		}
	}

	return (needed);
}

/* Copy feature. */
static int
copy_feature(char *buf, struct di_feature *s)
{
	struct di_ctl_feature *f;

	f = (struct di_ctl_feature *)buf;

	f->oid.type = DI_FEATURE;
	f->oid.subtype = 0;
	f->oid.len = sizeof(struct di_ctl_feature);
	strcpy(f->name, s->name);
	strcpy(f->mod_name, s->alg->name);

	return (sizeof(struct di_ctl_feature));
}

/* Copy feature configuration/parameters. */
static int
copy_feature_conf(char *buf, struct di_feature *s)
{
	int size;
	struct di_oid *fconf;

	fconf = (struct di_oid *)buf;

	size = s->alg->get_conf(&s->conf, fconf, 0);
	fconf->type = DI_FEATURE_CONFIG;
	fconf->subtype = 0;
	fconf->len = size;

	return (size);
}

/* Copy all features. */
static int
copy_features(struct di_oid *cmd, char **buf)
{
	struct di_feature *s;
	char *name;
	int size, r;

	size = 0;

	if (cmd != NULL)
		name = ((struct di_ctl_feature *)cmd)->name;
	else
		name = "all";

	if (strcmp(name, "all") != 0) {
		s = search_feature_instance(name);
		if (s == NULL)
			return (-1);

		r = copy_feature(*buf, s);
		*buf += r;
		size += r;
		r = copy_feature_conf(*buf, s);
		*buf += r;
		size += r;
	} else {
		/* All feature instances. */
		LIST_FOREACH(s, &di_config.feature_inst_list, next) {
			r = copy_feature(*buf, s);
			*buf += r;
			size += r;
			r = copy_feature_conf(*buf, s);
			*buf += r;
			size += r;
		}
	}

	return (size);
}

/* Copy classifier. */
static int
copy_classifier(char *buf, struct di_classifier *s)
{
	struct di_ctl_classifier *c;

	c = (struct di_ctl_classifier *)buf;

	c->oid.type = DI_CLASSIFIER;
	c->oid.subtype = 0;
	c->oid.len = sizeof(struct di_ctl_classifier) +
	    (s->fscnt + s->ccnt) * sizeof(struct di_feature_stat);
	strcpy(c->name, s->name);
	strcpy(c->mod_name, s->alg->name);
	c->fscnt = s->fscnt;
	c->ccnt = s->ccnt;
	c->confirm = s->confirm;
	memcpy(c->fstats, s->fstats, (s->fscnt + s->ccnt) *
	    sizeof(struct di_feature_stat));

	return (c->oid.len);
}

/* Copy classifier configuration/model. */
static int
copy_classifier_conf(char *buf, struct di_classifier *s)
{
	struct di_oid *cconf;
	int size;

	cconf = (struct di_oid *)buf;

	size = s->alg->get_conf(&s->conf, cconf, 0);
	cconf->type = DI_CLASSIFIER_CONFIG;
	cconf->subtype = 0;
	cconf->len = size;

	return (size);
}

/*
 * Copy classifiers.
 * If name == "all", copy all names but not the configurations.
 */
static int
copy_classifiers(struct di_oid *cmd, char **buf)
{
	struct di_classifier *s;
	char *name;
	int r, size;

	size = 0;
	name = ((struct di_ctl_classifier *)cmd)->name;

	if (strcmp(name, "all")) {
		s = search_classifier_instance(name);
		if (s == NULL)
			return (-1);

		r = copy_classifier(*buf, s);
		*buf += r;
		size += r;
		r = copy_classifier_conf(*buf, s);
		*buf += r;
		size += r;
	} else {
		/* All classifier instances. */
		LIST_FOREACH(s, &di_config.classifier_inst_list, next) {
			r = copy_classifier(*buf, s);
			*buf += r;
			size += r;
		}
	}

	return (size);
}

/* Copy export. */
static int
copy_export(char *buf, struct di_export *s)
{
	struct di_ctl_export *e;

	e = (struct di_ctl_export *)buf;

	e->oid.type = DI_EXPORT;
	e->oid.subtype = 0;
	e->oid.len = sizeof(struct di_ctl_export);
	strcpy(e->name, s->name);
	e->conf = s->conf;

	return (sizeof(struct di_ctl_export));
}

/*
 * Copy exports.
 * If name == "all", copy all names but not the configurations.
 */
static int
copy_exports(struct di_oid *cmd, char **buf)
{
	struct di_export *s;
	char *name;
	int r, size;

	size = 0;
	name = ((struct di_ctl_export *)cmd)->name;

	if (strcmp(name, "all")) {
		s = search_export_instance(name);
		if (s == NULL)
			return (-1);

		r = copy_export(*buf, s);
		*buf += r;
		size += r;
	} else {
		/* All export instances. */
		LIST_FOREACH(s, &di_config.export_list, next) {
			r = copy_export(*buf, s);
			*buf += r;
			size += r;
		}
	}

	return (size);
}

/*
 * Main handler for getting info.
 * XXX: Support comma separated lists of features/classifiers/exports.
 */
static int
get_info(struct sockopt *sopt)
{
#define	TRIES 10
	struct di_oid *base, *cmd, *o;
	struct di_oid oid;
	char *start, *buf;
	size_t sopt_valsize;
	int error, ftable_len, have, i, l, need;

	ftable_len = have = need = 0;
	l = sizeof(struct di_oid);
	start = NULL;
	base = cmd = NULL;

	/* Save and restore original sopt_valsize around copyin. */
	sopt_valsize = sopt->sopt_valsize;
	error = sooptcopyin(sopt, &oid, l, l);
	sopt->sopt_valsize = sopt_valsize;

	if (error)
		return (error);

	if (oid.type != DI_CMD_GET)
		return (EINVAL);

	if (oid.subtype == DI_FEATURE) {
		DID("show feature");
		l += sizeof(struct di_feature);
	} else if (oid.subtype == DI_CLASSIFIER) {
		DID("show classifier");
		l += sizeof(struct di_classifier);
	} else if (oid.subtype == DI_EXPORT) {
		DID("show export");
		l += sizeof(struct di_export);
	} else {
		DID("show flow table");
	}

	cmd = base = malloc(l, M_DIFFUSE, M_WAITOK);

	error = sooptcopyin(sopt, cmd, l, l);
	sopt->sopt_valsize = sopt_valsize;
	if (error)
		goto done;

	cmd = (struct di_oid *)((char *)cmd + cmd->len);

	/*
	 * Count space (under lock) and allocate (outside lock).
	 * Exit with lock held if we manage to get enough buffer.
	 * Try a few times then give up.
	 */
	/* XXX: Should rewrite this bit of code. */
	for (have = 0, i = 0; i < TRIES; i++) {
		DI_WLOCK();
		if (oid.subtype == DI_FEATURE) {
			need = compute_space_feature(cmd);
		} else if (oid.subtype == DI_CLASSIFIER) {
			need = compute_space_class(cmd);
		} else if (oid.subtype == DI_EXPORT) {
			need = compute_space_export(cmd);
		} else if (oid.subtype == DI_FLOW_TABLE) {
			need = compute_space_feature(NULL);
			if (need >= 0) {
				ftable_len = sizeof(struct di_oid) +
				    diffuse_ft_len(oid.flags &
				    DI_FT_GET_EXPIRED);
				need += ftable_len;
			}
		}

		if (need < 0) {
			DI_UNLOCK();
			error = EINVAL;
			goto done;
		}
		need += sizeof(struct di_oid);

		DID2("need %d bytes", need);
		DID2("have %d bytes", have);

		/* So we get this into userspace. */
		base->id = need;
		if (have >= need)
			break;

		DI_UNLOCK();

		if (start)
			free(start, M_DIFFUSE);

		start = NULL;
		/* Stop if socket option buffer still too small or last try. */
		if (need > sopt_valsize || i == TRIES - 1)
			break;

		have = need;
		start = malloc(have, M_DIFFUSE, M_WAITOK | M_ZERO);
		if (start == NULL) {
			error = ENOMEM;
			goto done;
		}
	}

	if (start == NULL) {
		/*
		 * If sopt buffer too small or run out of tries tell user space
		 * to allocate more memory.
		 */
		error = sooptcopyout(sopt, base, l);
		goto done;
	}

#ifdef DIFFUSE_DEBUG
	if (oid.subtype == DI_FEATURE || oid.subtype == DI_FLOW_TABLE) {
		DID("have %d features", di_config.feature_count);
		if (oid.subtype == DI_FLOW_TABLE)
			DID("have %d flows", diffuse_ft_entries());
	} else if (oid.subtype == DI_CLASSIFIER) {
		DID("have %d classifiers", di_config.classifier_count);
	} else if (oid.subtype == DI_EXPORT) {
		DID("have %d exports", di_config.export_count);
	}
#endif

	sopt->sopt_valsize = sopt_valsize;
	bcopy(base, start, sizeof(struct di_oid));
	((struct di_oid *)(start))->len = sizeof(struct di_oid);
	buf = start + sizeof(struct di_oid);
	/* Copy objects. */
	if (oid.subtype == DI_FEATURE) {
		if (copy_features(cmd, &buf) < 0) {
			error = EINVAL;
			DI_UNLOCK();
			goto done;
		}
	} else if (oid.subtype == DI_CLASSIFIER) {
		if (copy_classifiers(cmd, &buf) < 0) {
			error = EINVAL;
			DI_UNLOCK();
			goto done;
		}
	} else if (oid.subtype == DI_EXPORT) {
		if (copy_exports(cmd, &buf) < 0) {
			error = EINVAL;
			DI_UNLOCK();
			goto done;
		}
	} else if (oid.subtype == DI_FLOW_TABLE) {
		if (copy_features(NULL, &buf) < 0) {
			error = EINVAL;
			DI_UNLOCK();
			goto done;
		}

		o = (struct di_oid *)buf;
		o->type = DI_FLOW_TABLE;
		o->subtype = 0;
		o->len = ftable_len;
		buf += sizeof(struct di_oid);

		/* Copy flow table. */
		diffuse_get_ft(&buf, buf + have, oid.flags & DI_FT_GET_EXPIRED);
	}
	DID("total size %ld", (long int)(buf - start));

	DI_UNLOCK();

	error = sooptcopyout(sopt, start, buf - start);
done:
	if (base)
		free(base, M_DIFFUSE);

	if (start)
		free(start, M_DIFFUSE);

	return (error);
}

/*
 * Main handler for configuration. We are guaranteed to be called with an oid
 * which is at least a di_oid. The first object is the command (config,
 * delete, flush, ...)
 */
static int
do_config(void *p, int l)
{
	struct di_oid *arg, *next, *o, *prev;
	int action, err;

	arg = NULL;
	action = err = 0;

	o = p;
	if (o->id != DI_API_VERSION) {
		DID("invalid api version got %d need %d", o->id,
		    DI_API_VERSION);
		return (EINVAL);
	}

	for (; l >= sizeof(*o); o = next) {
		prev = arg;

		if (o->len < sizeof(*o) || l < o->len) {
			DID("bad len o->len %d len %d", o->len, l);
			err = EINVAL;
			break;
		}
		l -= o->len;
		next = (struct di_oid *)((char *)o + o->len);
		err = 0;

		switch (o->type) {
		default:
			DID("cmd %d not implemented", o->type);
			break;

		case DI_CMD_CONFIG: /* Simply a header. */
			action = DI_CMD_CONFIG;
			break;

		case DI_CMD_DELETE:
			action = DI_CMD_DELETE;
			break;

		case DI_CMD_ZERO:
			DI_WLOCK();
			diffuse_flush(1);
			DI_UNLOCK();
			break;

		case DI_CMD_FLUSH:
			DI_WLOCK();
			diffuse_flush(0);
			DI_UNLOCK();
			break;

		case DI_FEATURE:
			if (action == DI_CMD_CONFIG) {
				DID("configure feature");
				DI_WLOCK();
				err = config_feature((struct di_ctl_feature *)o,
				    next, arg);
				DI_UNLOCK();

				/* Eat the feature conf. */
				next = (struct di_oid *)((char *)o + o->len);
				l -= next->len;
			} else if (action == DI_CMD_DELETE) {
				DID("delete feature");
				DI_WLOCK();
				err = remove_feature_instance(
				    ((struct di_feature *)o)->name);
				DI_UNLOCK();
			}
			break;

		case DI_CLASSIFIER:
			if (action == DI_CMD_CONFIG) {
				DID("configure classifier");
				DI_WLOCK();
				err = config_classifier(
				    (struct di_ctl_classifier *)o, next, arg);
				DI_UNLOCK();

				/* Eat the classifier conf. */
				next = (struct di_oid *)((char *)o + o->len);
				l -= next->len;
			} else if (action == DI_CMD_DELETE) {
				DID("delete classifier");
				DI_WLOCK();
				err = remove_classifier_instance(
				    ((struct di_classifier *)o)->name);
				DI_UNLOCK();
			}
			break;

		case DI_EXPORT:
			if (action == DI_CMD_CONFIG) {
				DID("configure export");
				err = config_export((struct di_export *)o, arg);
			} else if (action == DI_CMD_DELETE) {
				DID("delete export");
				DI_WLOCK();
				err = remove_export_instance(
				    ((struct di_export *)o)->name);
				DI_UNLOCK();
			}
			break;
		}

		if (prev)
			arg = NULL;

		if (err != 0)
			break;
	}

	return (err);
}

/* Handler for the various socket options (userspace commands). */
static int
diffuse_ctl(struct sockopt *sopt)
{
	void *p;
	int error, l;

	p = NULL;

	/* Use same privilieges as ipfw (see sys/priv.h). */
	error = priv_check(sopt->sopt_td, PRIV_NETINET_IPFW);
	if (error)
		return (error);

	/* Disallow sets in really-really secure mode. */
	if (sopt->sopt_dir == SOPT_SET) {
		error =  securelevel_ge(sopt->sopt_td->td_ucred, 3);
		if (error)
			return (error);
	}

	switch (sopt->sopt_name) {
	default :
		DID("unknown option %d", sopt->sopt_name);
		error = EINVAL;
		break;

	case IP_DIFFUSE :
		if (sopt->sopt_dir == SOPT_GET) {
			error = get_info(sopt);
			break;
		}
		l = sopt->sopt_valsize;
		if (l < sizeof(struct di_oid) || l > 65535) {
			/* XXX: What max value? */
			error = EINVAL;
			DID("argument len %d invalid", l);
			break;
		}
		p = malloc(l, M_TEMP, M_WAITOK);
		error = sooptcopyin(sopt, p, l, l);
		if (error)
			break;

		error = do_config(p, l);
		break;
	}

	if (p != NULL)
		free(p, M_TEMP);

	return (error);
}

static void
diffuse_init(void)
{

	if (di_config.init_done)
		return;

	/* Initialise everything. */

	diffuse_ft_attach();
	diffuse_ft_init();
	diffuse_export_init();

	/* By default use explicit removal messages. */
	di_config.an_rule_removal = DIP_TIMEOUT_NONE;

	SLIST_INIT(&di_config.feature_list);
	LIST_INIT(&di_config.feature_inst_list);
	SLIST_INIT(&di_config.classifier_list);
	LIST_INIT(&di_config.classifier_inst_list);
	LIST_INIT(&di_config.export_list);
	TAILQ_INIT(&di_config.export_rec_list);

	DI_LOCK_INIT();

	callout_init(&di_timeout, CALLOUT_MPSAFE);
	callout_reset(&di_timeout, hz / 10, diffuse_timeout, NULL);

	di_config.init_done = 1;
	printf("DIFFUSE initialised\n");
}

#ifdef KLD_MODULE
static void
diffuse_destroy(int last)
{

	callout_drain(&di_timeout);

	DI_WLOCK();
	if (last) {
		DID("DIFFUSE removed last instance\n");
		diffuse_ctl_ptr = NULL;
		ipfw_diffuse_ext = NULL;
	}
	DI_UNLOCK();

	diffuse_remove_feature_instances(NULL, 1);
	diffuse_remove_classifier_instances(NULL, 1);
	diffuse_export_remove_recs(NULL);
	diffuse_remove_export_instances(1);
	diffuse_export_uninit();
	diffuse_ft_uninit();
	diffuse_ft_detach();
	DI_LOCK_DESTROY();
}
#endif /* KLD_MODULE */

/* The main module event function. */
static int
diffuse_modevent(module_t mod, int type, void *data)
{

	if (type == MOD_LOAD) {
		if (diffuse_ctl_ptr != NULL) {
			DID("DIFFUSE already loaded");
			return (EEXIST);
		}
		diffuse_init();
		diffuse_ctl_ptr = diffuse_ctl;
		ipfw_diffuse_ext = &diffuse_ext;

		return (0);
	} else if (type == MOD_QUIESCE || type == MOD_SHUTDOWN) {
		if (feature_instance_used()) {
			DID("feature(s) referenced by rules, cannot unload");
			return (EBUSY);
		}

		if (classifier_instance_used()) {
			DID("classifier(s) referenced by rules, cannot unload");
			return (EBUSY);
		}

		if (export_instance_used()) {
			DID("export(s) referenced by rules, cannot unload");
			return (EBUSY);
		}

		return (0);
	} else if (type == MOD_UNLOAD) {
#if !defined(KLD_MODULE)
		DID("statically compiled, cannot unload");
		return (EINVAL);
#else
		diffuse_destroy(1 /* last */);

		return (0);
#endif
	} else
		return (EOPNOTSUPP);
}

/* Modevent stuff for feature modules. */

static int
load_feature(struct di_feature_alg *f)
{
	struct di_feature_alg *s;
	int error;

	s = NULL;
	error = 0;

	if (f == NULL)
		return (EINVAL);

	diffuse_init(); /* Just in case, we need the lock. */

	/* Check that mandatory funcs exist. */
	if (f->init_instance == NULL ||
	    f->destroy_instance == NULL ||
	    f->init_stats == NULL ||
	    f->destroy_stats == NULL ||
	    f->update_stats == NULL ||
	    f->reset_stats == NULL ||
	    f->get_stats == NULL ||
	    f->get_stat_names == NULL) {
		DID("missing function for %s", f->name);
		return (EINVAL);
	}

	/* Search if feature already exists. */
	DI_WLOCK();
	SLIST_FOREACH(s, &di_config.feature_list, next) {
		if (strcmp(s->name, f->name) == 0) {
			/* Feature already exists. */
			DID("%s already loaded", f->name);
			error = EEXIST;
			break;
		}
	}
	if (!error) {
		SLIST_INSERT_HEAD(&di_config.feature_list, f, next);
		error = add_feature_instance(f->name, f->name, NULL);
	}

	DI_UNLOCK();

	if (error)
		DID("%s instantiating failed", f->name);
	else
		DID("feature %s %sloaded", f->name, s ? "not ":"");

	return (error);
}

static int
unload_feature(struct di_feature_alg *f)
{
	struct di_feature_alg *tmp, *r;
	int err;

	err = EINVAL;

	DID("called for %s", f->name);

	DI_WLOCK();
	SLIST_FOREACH_SAFE(r, &di_config.feature_list, next, tmp) {
		if (strcmp(f->name, r->name) != 0)
			continue;

		DID("ref_count = %d", r->ref_count);
		err = (r->ref_count != 0) ? EBUSY : 0;
		if (err == 0) {
			SLIST_REMOVE(&di_config.feature_list, r,
			    di_feature_alg, next);
		}
		break;
	}
	DI_UNLOCK();

	DID("feature %s %sunloaded", f->name, err ? "not " : "");

	return (err);
}

int
diffuse_feature_modevent(module_t mod, int cmd, void *arg)
{
	struct di_feature_alg *f;

	f = arg;

	if (cmd == MOD_LOAD) {
		return (load_feature(f));
	} else if (cmd == MOD_UNLOAD) {
		diffuse_remove_feature_instances(f->name, 1);
		return (unload_feature(f));
	} else  {
		return (EOPNOTSUPP);
	}
}

/* Modevent stuff for classifier modules. */

static int
load_classifier(struct di_classifier_alg *c)
{
	struct di_classifier_alg *s;
	int error;

	s = NULL;
	error = 0;

	if (c == NULL)
		return (EINVAL);

	diffuse_init(); /* Just in case, we need the lock. */

	/* Check that mandatory funcs exist. */
	if (c->init_instance == NULL ||
	    c->destroy_instance == NULL ||
	    c->classify == NULL ||
	    c->get_class_cnt == NULL ||
	    c->get_feature_cnt == NULL) {
		DID("missing function for %s", c->name);
		return (EINVAL);
	}

	/* Search if classifier already exists. */
	DI_WLOCK();
	SLIST_FOREACH(s, &di_config.classifier_list, next) {
		if (strcmp(s->name, c->name) == 0) {
			DID("%s already loaded", c->name);
			error = EEXIST;
			break;
		}
	}
	if (!error)
		SLIST_INSERT_HEAD(&di_config.classifier_list, c, next);

	/* Can't add default instance for classifiers. */

	DI_UNLOCK();

	DID("classifier %s %sloaded", c->name, s ? "not ":"");

	return (error);
}

static int
unload_classifier(struct di_classifier_alg *c)
{
	struct di_classifier_alg *r, *tmp;
	int err;

	err = EINVAL;

	DID("called for %s", c->name);

	DI_WLOCK();
	SLIST_FOREACH_SAFE(r, &di_config.classifier_list, next, tmp) {
		if (strcmp(c->name, r->name) != 0)
			continue;

		DID("ref_count = %d", r->ref_count);
		err = (r->ref_count != 0) ? EBUSY : 0;
		if (err == 0) {
			SLIST_REMOVE(&di_config.classifier_list, r,
			    di_classifier_alg, next);
		}
		break;
	}
	DI_UNLOCK();

	DID("classifier %s %sunloaded", c->name, err ? "not " : "");
	return (err);
}

int
diffuse_classifier_modevent(module_t mod, int cmd, void *arg)
{
	struct di_classifier_alg *c;

	c = arg;

	if (cmd == MOD_LOAD) {
		return (load_classifier(c));
	} else if (cmd == MOD_UNLOAD) {
		diffuse_remove_classifier_instances(c->name, 1);
		return (unload_classifier(c));
	} else  {
		return (EOPNOTSUPP);
	}
}

static moduledata_t diffuse_mod = {
	.name	= "diffuse",
	.evhand	= diffuse_modevent,
	.priv	= NULL
};

#define	DI_SI_SUB	SI_SUB_PROTO_IFATTACHDOMAIN
#define	DI_MODEV_ORD	(SI_ORDER_ANY - 128) /* After ipfw. */
DECLARE_MODULE(diffuse, diffuse_mod, DI_SI_SUB, DI_MODEV_ORD);
MODULE_DEPEND(diffuse, ipfw, 2, 2, 2);
MODULE_VERSION(diffuse, 1);
