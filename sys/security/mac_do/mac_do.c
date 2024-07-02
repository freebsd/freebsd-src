/*-
 * Copyright(c) 2024 Baptiste Daroussin <bapt@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/sx.h>
#include <sys/sysctl.h>
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

struct rules {
	char string[MAC_RULE_STRING_LEN];
	TAILQ_HEAD(rulehead, rule) head;
};

static struct rules rules0;

static void
toast_rules(struct rulehead *head)
{
	struct rule *r;

	while ((r = TAILQ_FIRST(head)) != NULL) {
		TAILQ_REMOVE(head, r, r_entries);
		free(r, M_DO);
	}
	TAILQ_INIT(head);
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

/*
 * Parse rules specification and produce rule structures out of it.
 *
 * 'head' must be an empty list head. Returns 0 on success, with 'head' filled
 * with structures representing the rules.  On error, 'head' is left empty and
 * the returned value is non-zero.  If 'string' has length greater or equal to
 * MAC_RULE_STRING_LEN, ENAMETOOLONG is returned.  If it is not in the expected
 * format (comma-separated list of clauses of the form "<type>=<val>:<target>",
 * where <type> is "uid" or "gid", <val> an UID or GID (depending on <type>) and
 * <target> is "*", "any" or some UID), EINVAL is returned.
 */
static int
parse_rules(const char *const string, struct rulehead *const head)
{
	const size_t len = strlen(string);
	char *copy;
	char *p;
	char *element;
	struct rule *new;
	int error = 0;

	QMD_TAILQ_CHECK_TAIL(head, r_entries);
	MPASS(TAILQ_EMPTY(head));

	if (len >= MAC_RULE_STRING_LEN)
		return (ENAMETOOLONG);

	copy = malloc(len + 1, M_DO, M_WAITOK);
	bcopy(string, copy, len + 1);
	MPASS(copy[len] == '\0'); /* Catch some races. */

	p = copy;
	while ((element = strsep(&p, ",")) != NULL) {
		if (element[0] == '\0')
			continue;
		error = parse_rule_element(element, &new);
		if (error != 0) {
			toast_rules(head);
			goto out;
		}
		TAILQ_INSERT_TAIL(head, new, r_entries);
	}
out:
	free(copy, M_DO);
	return (error);
}

/*
 * Find rules applicable to the passed prison.
 *
 * Returns the applicable rules (and never NULL).  'pr' must be unlocked.
 * 'aprp' is set to the (ancestor) prison holding these, and it must be unlocked
 * once the caller is done accessing the rules.  '*aprp' is equal to 'pr' if and
 * only if the current jail has its own set of rules.
 */
static struct rules *
find_rules(struct prison *const pr, struct prison **const aprp)
{
	struct prison *cpr;
	struct rules *rules;

	for (cpr = pr;; cpr = cpr->pr_parent) {
		prison_lock(cpr);
		if (cpr == &prison0) {
			rules = &rules0;
			break;
		}
		rules = osd_jail_get(cpr, mac_do_osd_jail_slot);
		if (rules != NULL)
			break;
		prison_unlock(cpr);
	}
	*aprp = cpr;

	return (rules);
}

/*
 * Ensure the passed prison has its own 'struct rules'.
 *
 * On entry, the prison must be unlocked, but will be returned locked.  Returns
 * the newly allocated and initialized 'struct rules', or the existing one.
 */
static struct rules *
ensure_rules(struct prison *const pr)
{
	struct rules *rules, *new_rules;
	void **rsv;

	if (pr == &prison0) {
		prison_lock(pr);
		return (&rules0);
	}

	/* Optimistically try to avoid memory allocations. */
restart:
	prison_lock(pr);
	rules = osd_jail_get(pr, mac_do_osd_jail_slot);
	if (rules != NULL)
		return (rules);
	prison_unlock(pr);

	new_rules = malloc(sizeof(*new_rules), M_DO, M_WAITOK|M_ZERO);
	TAILQ_INIT(&new_rules->head);
	rsv = osd_reserve(mac_do_osd_jail_slot);
	prison_lock(pr);
	rules = osd_jail_get(pr, mac_do_osd_jail_slot);
	if (rules != NULL) {
		/*
		 * We could cleanup while holding the prison lock (given the
		 * current implementation of osd_free_reserved()), but be safe
		 * and a good citizen by not keeping it more than strictly
		 * necessary.  The only consequence is that we have to relookup
		 * the rules.
		 */
		prison_unlock(pr);
		osd_free_reserved(rsv);
		free(new_rules, M_DO);
		goto restart;
	}
	osd_jail_set_reserved(pr, mac_do_osd_jail_slot, rsv, new_rules);
	return (new_rules);
}

/*
 * OSD destructor for slot 'mac_do_osd_jail_slot'.
 *
 * Called with 'value' not NULL.
 */
static void
dealloc_osd(void *const value)
{
	struct rules *const rules = value;

	toast_rules(&rules->head);
	free(rules, M_DO);
}

/*
 * Deallocate the rules associated to a prison.
 *
 * Destroys the 'mac_do_osd_jail_slot' slot of the passed jail.
 */
static void
dealloc_rules(struct prison *const pr)
{
	prison_lock(pr);
	/* This calls destructor dealloc_osd(). */
	osd_jail_del(pr, mac_do_osd_jail_slot);
	prison_unlock(pr);
}

/*
 * Assign already parsed rules to a jail.
 */
static void
set_rules(struct prison *const pr, const char *const rules_string,
    struct rulehead *const head)
{
	struct rules *rules;
	struct rulehead old_head;

	MPASS(rules_string != NULL);
	MPASS(strlen(rules_string) < MAC_RULE_STRING_LEN);

	TAILQ_INIT(&old_head);
	rules = ensure_rules(pr);
	strlcpy(rules->string, rules_string, MAC_RULE_STRING_LEN);
	TAILQ_CONCAT(&old_head, &rules->head, r_entries);
	TAILQ_CONCAT(&rules->head, head, r_entries);
	prison_unlock(pr);
	toast_rules(&old_head);
}

/*
 * Parse a rules specification and assign them to a jail.
 *
 * Returns the same error code as parse_rules() (which see).
 */
static int
parse_and_set_rules(struct prison *const pr, const char *rules_string)
{
	struct rulehead head;
	int error;

	error = parse_rules(rules_string, &head);
	if (error != 0)
		return (error);
	set_rules(pr, rules_string, &head);
	return (0);
}

static int
sysctl_rules(SYSCTL_HANDLER_ARGS)
{
	char *new_string;
	struct prison *pr;
	struct rules *rules;
	int error;

	rules = find_rules(req->td->td_ucred->cr_prison, &pr);
	prison_unlock(pr);
	if (req->newptr == NULL)
		return (sysctl_handle_string(oidp, rules->string, MAC_RULE_STRING_LEN, req));

	new_string = malloc(MAC_RULE_STRING_LEN, M_DO,
	    M_WAITOK|M_ZERO);
	prison_lock(pr);
	strlcpy(new_string, rules->string, MAC_RULE_STRING_LEN);
	prison_unlock(pr);

	error = sysctl_handle_string(oidp, new_string, MAC_RULE_STRING_LEN, req);
	if (error)
		goto out;

	error = parse_and_set_rules(pr, new_string);

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

static int
mac_do_prison_set(void *obj, void *data)
{
	struct prison *pr = obj;
	struct vfsoptlist *opts = data;
	char *rules_string;
	int error, jsys, len;

	error = vfs_copyopt(opts, "mdo", &jsys, sizeof(jsys));
	if (error == ENOENT)
		jsys = -1;
	error = vfs_getopt(opts, "mdo.rules", (void **)&rules_string, &len);
	if (error == ENOENT)
		rules_string = "";
	else
		jsys = JAIL_SYS_NEW;
	switch (jsys) {
	case JAIL_SYS_INHERIT:
		dealloc_rules(pr);
		error = 0;
		break;
	case JAIL_SYS_NEW:
		error = parse_and_set_rules(pr, rules_string);
		break;
	}
	return (error);
}

SYSCTL_JAIL_PARAM_SYS_NODE(mdo, CTLFLAG_RW, "Jail MAC/do parameters");
SYSCTL_JAIL_PARAM_STRING(_mdo, rules, CTLFLAG_RW, MAC_RULE_STRING_LEN,
    "Jail MAC/do rules");

static int
mac_do_prison_get(void *obj, void *data)
{
	struct prison *ppr, *pr = obj;
	struct vfsoptlist *opts = data;
	struct rules *rules;
	int jsys, error;

	rules = find_rules(pr, &ppr);
	error = vfs_setopt(opts, "mdo", &jsys, sizeof(jsys));
	if (error != 0 && error != ENOENT)
		goto done;
	error = vfs_setopts(opts, "mdo.rules", rules->string);
	if (error != 0 && error != ENOENT)
		goto done;
	prison_unlock(ppr);
	error = 0;
done:
	return (0);
}

static int
mac_do_prison_create(void *obj, void *data __unused)
{
	struct prison *const pr = obj;

	(void)ensure_rules(pr);
	prison_unlock(pr);
	return (0);
}

static int
mac_do_prison_remove(void *obj, void *data __unused)
{
	struct prison *pr = obj;
	struct rules *r;

	prison_lock(pr);
	r = osd_jail_get(pr, mac_do_osd_jail_slot);
	prison_unlock(pr);
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

	mac_do_osd_jail_slot = osd_jail_register(dealloc_osd, methods);
	TAILQ_INIT(&rules0.head);
	sx_slock(&allprison_lock);
	TAILQ_FOREACH(pr, &allprison, pr_list) {
		(void)ensure_rules(pr);
		prison_unlock(pr);
	}
	sx_sunlock(&allprison_lock);
}

static bool
rule_applies(struct ucred *cred, struct rule *r)
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
	struct rules *rule;

	if (do_enabled == 0)
		return (EPERM);

	rule = find_rules(cred->cr_prison, &pr);
	TAILQ_FOREACH(r, &rule->head, r_entries) {
		if (rule_applies(cred, r)) {
			switch (priv) {
			case PRIV_CRED_SETGROUPS:
			case PRIV_CRED_SETUID:
				prison_unlock(pr);
				return (0);
			default:
				break;
			}
		}
	}
	prison_unlock(pr);
	return (EPERM);
}

static int
check_setgroups(struct ucred *cred, int ngrp, gid_t *groups)
{
	struct rule *r;
	char *fullpath = NULL;
	char *freebuf = NULL;
	struct prison *pr;
	struct rules *rule;

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

	rule = find_rules(cred->cr_prison, &pr);
	TAILQ_FOREACH(r, &rule->head, r_entries) {
		if (rule_applies(cred, r)) {
			prison_unlock(pr);
			return (0);
		}
	}
	prison_unlock(pr);

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
	struct rules *rule;

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
	rule = find_rules(cred->cr_prison, &pr);
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
	prison_unlock(pr);
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
