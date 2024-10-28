/*-
 * Copyright(c) 2024 Baptiste Daroussin <bapt@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/sx.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/ucred.h>
#include <sys/vnode.h>

#include <security/mac/mac_policy.h>

SYSCTL_DECL(_security_mac);

static SYSCTL_NODE(_security_mac, OID_AUTO, do,
    CTLFLAG_RW|CTLFLAG_MPSAFE, 0, "mac_do policy controls");

static int	do_enabled = 1;
SYSCTL_INT(_security_mac_do, OID_AUTO, enabled, CTLFLAG_RWTUN,
    &do_enabled, 0, "Enforce do policy");

static MALLOC_DEFINE(M_DO, "do_rule", "Rules for mac_do");

#define MAC_RULE_STRING_LEN	1024

static unsigned		mac_do_osd_jail_slot;

#define RULE_UID	1
#define RULE_GID	2
#define RULE_ANY	3

struct rule {
	int	from_type;
	union {
		uid_t f_uid;
		gid_t f_gid;
	};
	int	to_type;
	uid_t t_uid;
	TAILQ_ENTRY(rule) r_entries;
};

struct mac_do_rule {
	char string[MAC_RULE_STRING_LEN];
	TAILQ_HEAD(rulehead, rule) head;
};

static struct mac_do_rule rules0;

static void
toast_rules(struct rulehead *head)
{
	struct rule *r;

	while ((r = TAILQ_FIRST(head)) != NULL) {
		TAILQ_REMOVE(head, r, r_entries);
		free(r, M_DO);
	}
}

static int
parse_rule_element(char *element, struct rule **rule)
{
	int error = 0;
	char *type, *id, *p;
	struct rule *new;

	new = malloc(sizeof(*new), M_DO, M_ZERO|M_WAITOK);

	type = strsep(&element, "=");
	if (type == NULL) {
		error = EINVAL;
		goto out;
	}
	if (strcmp(type, "uid") == 0) {
		new->from_type = RULE_UID;
	} else if (strcmp(type, "gid") == 0) {
		new->from_type = RULE_GID;
	} else {
		error = EINVAL;
		goto out;
	}
	id = strsep(&element, ":");
	if (id == NULL) {
		error = EINVAL;
		goto out;
	}
	if (new->from_type == RULE_UID)
		new->f_uid = strtol(id, &p, 10);
	if (new->from_type == RULE_GID)
		new->f_gid = strtol(id, &p, 10);
	if (*p != '\0') {
		error = EINVAL;
		goto out;
	}
	if (*element == '\0') {
		error = EINVAL;
		goto out;
	}
	if (strcmp(element, "any") == 0 || strcmp(element, "*") == 0) {
		new->to_type = RULE_ANY;
	} else {
		new->to_type = RULE_UID;
		new->t_uid = strtol(element, &p, 10);
		if (*p != '\0') {
			error = EINVAL;
			goto out;
		}
	}
out:
	if (error != 0) {
		free(new, M_DO);
		*rule = NULL;
	} else
		*rule = new;
	return (error);
}

static int
parse_rules(char *string, struct rulehead *head)
{
	struct rule *new;
	char *element;
	int error = 0;

	while ((element = strsep(&string, ",")) != NULL) {
		if (strlen(element) == 0)
			continue;
		error = parse_rule_element(element, &new);
		if (error)
			goto out;
		TAILQ_INSERT_TAIL(head, new, r_entries);
	}
out:
	if (error != 0)
		toast_rules(head);
	return (error);
}

static struct mac_do_rule *
mac_do_rule_find(struct prison *spr, struct prison **prp)
{
	struct prison *pr;
	struct mac_do_rule *rules;

	for (pr = spr;; pr = pr->pr_parent) {
		mtx_lock(&pr->pr_mtx);
		if (pr == &prison0) {
			rules = &rules0;
			break;
		}
		rules = osd_jail_get(pr, mac_do_osd_jail_slot);
		if (rules != NULL)
			break;
		mtx_unlock(&pr->pr_mtx);
	}
	*prp = pr;

	return (rules);
}

static int
sysctl_rules(SYSCTL_HANDLER_ARGS)
{
	char *copy_string, *new_string;
	struct rulehead head, saved_head;
	struct prison *pr;
	struct mac_do_rule *rules;
	int error;

	rules = mac_do_rule_find(req->td->td_ucred->cr_prison, &pr);
	mtx_unlock(&pr->pr_mtx);
	if (req->newptr == NULL)
		return (sysctl_handle_string(oidp, rules->string, MAC_RULE_STRING_LEN, req));

	new_string = malloc(MAC_RULE_STRING_LEN, M_DO,
	    M_WAITOK|M_ZERO);
	mtx_lock(&pr->pr_mtx);
	strlcpy(new_string, rules->string, MAC_RULE_STRING_LEN);
	mtx_unlock(&pr->pr_mtx);

	error = sysctl_handle_string(oidp, new_string, MAC_RULE_STRING_LEN, req);
	if (error)
		goto out;

	copy_string = strdup(new_string, M_DO);
	TAILQ_INIT(&head);
	error = parse_rules(copy_string, &head);
	free(copy_string, M_DO);
	if (error)
		goto out;
	TAILQ_INIT(&saved_head);
	mtx_lock(&pr->pr_mtx);
	TAILQ_CONCAT(&saved_head, &rules->head, r_entries);
	TAILQ_CONCAT(&rules->head, &head, r_entries);
	strlcpy(rules->string, new_string, MAC_RULE_STRING_LEN);
	mtx_unlock(&pr->pr_mtx);
	toast_rules(&saved_head);

out:
	free(new_string, M_DO);
	return (error);
}

SYSCTL_PROC(_security_mac_do, OID_AUTO, rules,
    CTLTYPE_STRING|CTLFLAG_RW|CTLFLAG_MPSAFE,
    0, 0, sysctl_rules, "A",
    "Rules");

static void
destroy(struct mac_policy_conf *mpc)
{
	osd_jail_deregister(mac_do_osd_jail_slot);
	toast_rules(&rules0.head);
}

static void
mac_do_alloc_prison(struct prison *pr, struct mac_do_rule **lrp)
{
	struct prison *ppr;
	struct mac_do_rule *rules, *new_rules;
	void **rsv;

	rules = mac_do_rule_find(pr, &ppr);
	if (ppr == pr)
		goto done;

	mtx_unlock(&ppr->pr_mtx);
	new_rules = malloc(sizeof(*new_rules), M_PRISON, M_WAITOK|M_ZERO);
	rsv = osd_reserve(mac_do_osd_jail_slot);
	rules = mac_do_rule_find(pr, &ppr);
	if (ppr == pr) {
		free(new_rules, M_PRISON);
		osd_free_reserved(rsv);
		goto done;
	}
	mtx_lock(&pr->pr_mtx);
	osd_jail_set_reserved(pr, mac_do_osd_jail_slot, rsv, new_rules);
	TAILQ_INIT(&new_rules->head);
done:
	if (lrp != NULL)
		*lrp = rules;
	mtx_unlock(&pr->pr_mtx);
	mtx_unlock(&ppr->pr_mtx);
}

static void
mac_do_dealloc_prison(void *data)
{
	struct mac_do_rule *r = data;

	toast_rules(&r->head);
}

static int
mac_do_prison_set(void *obj, void *data)
{
	struct prison *pr = obj;
	struct vfsoptlist *opts = data;
	struct rulehead head, saved_head;
	struct mac_do_rule *rules;
	char *rules_string, *copy_string;
	int error, jsys, len;

	error = vfs_copyopt(opts, "mdo", &jsys, sizeof(jsys));
	if (error == ENOENT)
		jsys = -1;
	error = vfs_getopt(opts, "mdo.rules", (void **)&rules_string, &len);
	if (error == ENOENT)
		rules = NULL;
	else
		jsys = JAIL_SYS_NEW;
	switch (jsys) {
	case JAIL_SYS_INHERIT:
		mtx_lock(&pr->pr_mtx);
		osd_jail_del(pr, mac_do_osd_jail_slot);
		mtx_unlock(&pr->pr_mtx);
		break;
	case JAIL_SYS_NEW:
		mac_do_alloc_prison(pr, &rules);
		if (rules_string == NULL)
			break;
		copy_string = strdup(rules_string, M_DO);
		TAILQ_INIT(&head);
		error = parse_rules(copy_string, &head);
		free(copy_string, M_DO);
		if (error)
			return (1);
		TAILQ_INIT(&saved_head);
		mtx_lock(&pr->pr_mtx);
		TAILQ_CONCAT(&saved_head, &rules->head, r_entries);
		TAILQ_CONCAT(&rules->head, &head, r_entries);
		strlcpy(rules->string, rules_string, MAC_RULE_STRING_LEN);
		mtx_unlock(&pr->pr_mtx);
		toast_rules(&saved_head);
		break;
	}
	return (0);
}

SYSCTL_JAIL_PARAM_SYS_NODE(mdo, CTLFLAG_RW, "Jail MAC/do parameters");
SYSCTL_JAIL_PARAM_STRING(_mdo, rules, CTLFLAG_RW, MAC_RULE_STRING_LEN,
    "Jail MAC/do rules");

static int
mac_do_prison_get(void *obj, void *data)
{
	struct prison *ppr, *pr = obj;
	struct vfsoptlist *opts = data;
	struct mac_do_rule *rules;
	int jsys, error;

	rules = mac_do_rule_find(pr, &ppr);
	error = vfs_setopt(opts, "mdo", &jsys, sizeof(jsys));
	if (error != 0 && error != ENOENT)
		goto done;
	error = vfs_setopts(opts, "mdo.rules", rules->string);
	if (error != 0 && error != ENOENT)
		goto done;
	mtx_unlock(&ppr->pr_mtx);
	error = 0;
done:
	return (0);
}

static int
mac_do_prison_create(void *obj, void *data __unused)
{
	struct prison *pr = obj;

	mac_do_alloc_prison(pr, NULL);
	return (0);
}

static int
mac_do_prison_remove(void *obj, void *data __unused)
{
	struct prison *pr = obj;
	struct mac_do_rule *r;

	mtx_lock(&pr->pr_mtx);
	r = osd_jail_get(pr, mac_do_osd_jail_slot);
	mtx_unlock(&pr->pr_mtx);
	toast_rules(&r->head);
	return (0);
}

static int
mac_do_prison_check(void *obj, void *data)
{
	struct vfsoptlist *opts = data;
	char *rules_string;
	int error, jsys, len;

	error = vfs_copyopt(opts, "mdo", &jsys, sizeof(jsys));
	if (error != ENOENT) {
		if (error != 0)
			return (error);
		if (jsys != JAIL_SYS_NEW && jsys != JAIL_SYS_INHERIT)
			return (EINVAL);
	}
	error = vfs_getopt(opts, "mdo.rules", (void **)&rules_string, &len);
	if (error != ENOENT) {
		if (error != 0)
			return (error);
		if (len > MAC_RULE_STRING_LEN) {
			vfs_opterror(opts, "mdo.rules too long");
			return (ENAMETOOLONG);
		}
	}
	if (error == ENOENT)
		error = 0;
	return (error);
}

static void
init(struct mac_policy_conf *mpc)
{
	static osd_method_t methods[PR_MAXMETHOD] = {
		[PR_METHOD_CREATE] = mac_do_prison_create,
		[PR_METHOD_GET] = mac_do_prison_get,
		[PR_METHOD_SET] = mac_do_prison_set,
		[PR_METHOD_CHECK] = mac_do_prison_check,
		[PR_METHOD_REMOVE] = mac_do_prison_remove,
	};
	struct prison *pr;

	mac_do_osd_jail_slot = osd_jail_register(mac_do_dealloc_prison, methods);
	TAILQ_INIT(&rules0.head);
	sx_slock(&allprison_lock);
	TAILQ_FOREACH(pr, &allprison, pr_list)
		mac_do_alloc_prison(pr, NULL);
	sx_sunlock(&allprison_lock);
}

static bool
rule_is_valid(struct ucred *cred, struct rule *r)
{
	if (r->from_type == RULE_UID && r->f_uid == cred->cr_uid)
		return (true);
	if (r->from_type == RULE_GID && groupmember(r->f_gid, cred))
		return (true);
	return (false);
}

static int
priv_grant(struct ucred *cred, int priv)
{
	struct rule *r;
	struct prison *pr;
	struct mac_do_rule *rule;

	if (do_enabled == 0)
		return (EPERM);

	rule = mac_do_rule_find(cred->cr_prison, &pr);
	TAILQ_FOREACH(r, &rule->head, r_entries) {
		if (rule_is_valid(cred, r)) {
			switch (priv) {
			case PRIV_CRED_SETGROUPS:
			case PRIV_CRED_SETUID:
				mtx_unlock(&pr->pr_mtx);
				return (0);
			default:
				break;
			}
		}
	}
	mtx_unlock(&pr->pr_mtx);
	return (EPERM);
}

static int
check_setgroups(struct ucred *cred, int ngrp, gid_t *groups)
{
	struct rule *r;
	char *fullpath = NULL;
	char *freebuf = NULL;
	struct prison *pr;
	struct mac_do_rule *rule;

	if (do_enabled == 0)
		return (0);
	if (cred->cr_uid == 0)
		return (0);

	if (vn_fullpath(curproc->p_textvp, &fullpath, &freebuf) != 0)
		return (EPERM);
	if (strcmp(fullpath, "/usr/bin/mdo") != 0) {
		free(freebuf, M_TEMP);
		return (EPERM);
	}
	free(freebuf, M_TEMP);

	rule = mac_do_rule_find(cred->cr_prison, &pr);
	TAILQ_FOREACH(r, &rule->head, r_entries) {
		if (rule_is_valid(cred, r)) {
			mtx_unlock(&pr->pr_mtx);
			return (0);
		}
	}
	mtx_unlock(&pr->pr_mtx);

	return (EPERM);
}

static int
check_setuid(struct ucred *cred, uid_t uid)
{
	struct rule *r;
	int error;
	char *fullpath = NULL;
	char *freebuf = NULL;
	struct prison *pr;
	struct mac_do_rule *rule;

	if (do_enabled == 0)
		return (0);
	if (cred->cr_uid == uid || cred->cr_uid == 0 || cred->cr_ruid == 0)
		return (0);

	if (vn_fullpath(curproc->p_textvp, &fullpath, &freebuf) != 0)
		return (EPERM);
	if (strcmp(fullpath, "/usr/bin/mdo") != 0) {
		free(freebuf, M_TEMP);
		return (EPERM);
	}
	free(freebuf, M_TEMP);

	error = EPERM;
	rule = mac_do_rule_find(cred->cr_prison, &pr);
	TAILQ_FOREACH(r, &rule->head, r_entries) {
		if (r->from_type == RULE_UID) {
			if (cred->cr_uid != r->f_uid)
				continue;
			if (r->to_type == RULE_ANY) {
				error = 0;
				break;
			}
			if (r->to_type == RULE_UID && uid == r->t_uid) {
				error = 0;
				break;
			}
		}
		if (r->from_type == RULE_GID) {
			if (!groupmember(r->f_gid, cred))
				continue;
			if (r->to_type == RULE_ANY) {
				error = 0;
				break;
			}
			if (r->to_type == RULE_UID && uid == r->t_uid) {
				error = 0;
				break;
			}
		}
	}
	mtx_unlock(&pr->pr_mtx);
	return (error);
}

static struct mac_policy_ops do_ops = {
	.mpo_destroy = destroy,
	.mpo_init = init,
	.mpo_cred_check_setuid = check_setuid,
	.mpo_cred_check_setgroups = check_setgroups,
	.mpo_priv_grant = priv_grant,
};

MAC_POLICY_SET(&do_ops, mac_do, "MAC/do",
   MPC_LOADTIME_FLAG_UNLOADOK, NULL);
MODULE_VERSION(mac_do, 1);
