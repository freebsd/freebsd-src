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

/*
 * We assume that 'uid_t' and 'gid_t' are aliases to 'u_int' in conversions
 * required for parsing rules specification strings.
 */
_Static_assert(sizeof(uid_t) == sizeof(u_int) && (uid_t)-1 >= 0 &&
    sizeof(gid_t) == sizeof(u_int) && (gid_t)-1 >= 0,
    "mac_do(4) assumes that 'uid_t' and 'gid_t' are aliases to 'u_int'");

struct rule {
	u_int	from_type;
	u_int	from_id;
	u_int	to_type;
	u_int	to_id;
	TAILQ_ENTRY(rule) r_entries;
};

struct rules {
	char string[MAC_RULE_STRING_LEN];
	TAILQ_HEAD(rulehead, rule) head;
};

static void
toast_rules(struct rules *const rules)
{
	struct rulehead *const head = &rules->head;
	struct rule *rule;

	while ((rule = TAILQ_FIRST(head)) != NULL) {
		TAILQ_REMOVE(head, rule, r_entries);
		free(rule, M_DO);
	}
	free(rules, M_DO);
}

static struct rules *
alloc_rules(void)
{
	struct rules *const rules = malloc(sizeof(*rules), M_DO, M_WAITOK);

	_Static_assert(MAC_RULE_STRING_LEN > 0, "MAC_RULE_STRING_LEN <= 0!");
	rules->string[0] = 0;
	TAILQ_INIT(&rules->head);
	return (rules);
}

static int
parse_rule_element(char *element, struct rule **rule)
{
	const char *from_type, *from_id, *to;
	char *p;
	struct rule *new;

	new = malloc(sizeof(*new), M_DO, M_ZERO|M_WAITOK);

	from_type = strsep(&element, "=");
	if (from_type == NULL)
		goto einval;

	if (strcmp(from_type, "uid") == 0)
		new->from_type = RULE_UID;
	else if (strcmp(from_type, "gid") == 0)
		new->from_type = RULE_GID;
	else
		goto einval;

	from_id = strsep(&element, ":");
	if (from_id == NULL || *from_id == '\0')
		goto einval;

	new->from_id = strtol(from_id, &p, 10);
	if (*p != '\0')
		goto einval;

	to = element;
	if (to == NULL || *to == '\0')
		goto einval;

	if (strcmp(to, "any") == 0 || strcmp(to, "*") == 0)
		new->to_type = RULE_ANY;
	else {
		new->to_type = RULE_UID;
		new->to_id = strtol(to, &p, 10);
		if (*p != '\0')
			goto einval;
	}

	*rule = new;
	return (0);
einval:
	free(new, M_DO);
	*rule = NULL;
	return (EINVAL);
}

/*
 * Parse rules specification and produce rule structures out of it.
 *
 * Returns 0 on success, with '*rulesp' made to point to a 'struct rule'
 * representing the rules.  On error, the returned value is non-zero and
 * '*rulesp' is unchanged.  If 'string' has length greater or equal to
 * MAC_RULE_STRING_LEN, ENAMETOOLONG is returned.  If it is not in the expected
 * format (comma-separated list of clauses of the form "<type>=<val>:<target>",
 * where <type> is "uid" or "gid", <val> an UID or GID (depending on <type>) and
 * <target> is "*", "any" or some UID), EINVAL is returned.
 */
static int
parse_rules(const char *const string, struct rules **const rulesp)
{
	const size_t len = strlen(string);
	char *copy;
	char *p;
	char *element;
	struct rules *rules;
	struct rule *new;
	int error = 0;

	if (len >= MAC_RULE_STRING_LEN)
		return (ENAMETOOLONG);

	rules = alloc_rules();
	bcopy(string, rules->string, len + 1);
	MPASS(rules->string[len] == '\0'); /* Catch some races. */

	copy = malloc(len + 1, M_DO, M_WAITOK);
	bcopy(string, copy, len + 1);
	MPASS(copy[len] == '\0'); /* Catch some races. */

	p = copy;
	while ((element = strsep(&p, ",")) != NULL) {
		if (element[0] == '\0')
			continue;
		error = parse_rule_element(element, &new);
		if (error != 0) {
			toast_rules(rules);
			goto out;
		}
		TAILQ_INSERT_TAIL(&rules->head, new, r_entries);
	}

	*rulesp = rules;
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
	struct prison *cpr, *ppr;
	struct rules *rules;

	cpr = pr;
	for (;;) {
		prison_lock(cpr);
		rules = osd_jail_get(cpr, mac_do_osd_jail_slot);
		if (rules != NULL)
			break;
		prison_unlock(cpr);

		ppr = cpr->pr_parent;
		MPASS(ppr != NULL); /* prison0 always has rules. */
		cpr = ppr;
	}
	*aprp = cpr;

	return (rules);
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

	toast_rules(rules);
}

/*
 * Remove the rules specifically associated to a prison.
 *
 * In practice, this means that the rules become inherited (from the closest
 * ascendant that has some).
 *
 * Destroys the 'mac_do_osd_jail_slot' slot of the passed jail.
 */
static void
remove_rules(struct prison *const pr)
{
	prison_lock(pr);
	/* This calls destructor dealloc_osd(). */
	osd_jail_del(pr, mac_do_osd_jail_slot);
	prison_unlock(pr);
}

/*
 * Assign already built rules to a jail.
 */
static void
set_rules(struct prison *const pr, struct rules *const rules)
{
	struct rules *old_rules;
	void **rsv;

	rsv = osd_reserve(mac_do_osd_jail_slot);

	prison_lock(pr);
	old_rules = osd_jail_get(pr, mac_do_osd_jail_slot);
	osd_jail_set_reserved(pr, mac_do_osd_jail_slot, rsv, rules);
	prison_unlock(pr);
	if (old_rules != NULL)
		toast_rules(old_rules);
}

/*
 * Assigns empty rules to a jail.
 */
static void
set_empty_rules(struct prison *const pr)
{
	struct rules *const rules = alloc_rules();

	set_rules(pr, rules);
}

/*
 * Parse a rules specification and assign them to a jail.
 *
 * Returns the same error code as parse_rules() (which see).
 */
static int
parse_and_set_rules(struct prison *const pr, const char *rules_string)
{
	struct rules *rules;
	int error;

	error = parse_rules(rules_string, &rules);
	if (error != 0)
		return (error);
	set_rules(pr, rules);
	return (0);
}

static int
mac_do_sysctl_rules(SYSCTL_HANDLER_ARGS)
{
	char *const buf = malloc(MAC_RULE_STRING_LEN, M_DO, M_WAITOK);
	struct prison *const td_pr = req->td->td_ucred->cr_prison;
	struct prison *pr;
	struct rules *rules;
	int error;

	rules = find_rules(td_pr, &pr);
	strlcpy(buf, rules->string, MAC_RULE_STRING_LEN);
	prison_unlock(pr);

	error = sysctl_handle_string(oidp, buf, MAC_RULE_STRING_LEN, req);
	if (error != 0 || req->newptr == NULL)
		goto out;

	/* Set our prison's rules, not that of the jail we inherited from. */
	error = parse_and_set_rules(td_pr, buf);
out:
	free(buf, M_DO);
	return (error);
}

SYSCTL_PROC(_security_mac_do, OID_AUTO, rules,
    CTLTYPE_STRING|CTLFLAG_RW|CTLFLAG_PRISON|CTLFLAG_MPSAFE,
    0, 0, mac_do_sysctl_rules, "A",
    "Rules");


SYSCTL_JAIL_PARAM_SYS_SUBNODE(mac, do, CTLFLAG_RW, "Jail MAC/do parameters");
SYSCTL_JAIL_PARAM_STRING(_mac_do, rules, CTLFLAG_RW, MAC_RULE_STRING_LEN,
    "Jail MAC/do rules");


static int
mac_do_jail_create(void *obj, void *data __unused)
{
	struct prison *const pr = obj;

	set_empty_rules(pr);
	return (0);
}

static int
mac_do_jail_get(void *obj, void *data)
{
	struct prison *ppr, *const pr = obj;
	struct vfsoptlist *const opts = data;
	struct rules *rules;
	int jsys, error;

	rules = find_rules(pr, &ppr);

	jsys = pr == ppr ?
	    (TAILQ_EMPTY(&rules->head) ? JAIL_SYS_DISABLE : JAIL_SYS_NEW) :
	    JAIL_SYS_INHERIT;
	error = vfs_setopt(opts, "mac.do", &jsys, sizeof(jsys));
	if (error != 0 && error != ENOENT)
		goto done;

	error = vfs_setopts(opts, "mac.do.rules", rules->string);
	if (error != 0 && error != ENOENT)
		goto done;

	error = 0;
done:
	prison_unlock(ppr);
	return (error);
}

/*
 * -1 is used as a sentinel in mac_do_jail_check() and mac_do_jail_set() below.
 */
_Static_assert(-1 != JAIL_SYS_DISABLE && -1 != JAIL_SYS_NEW &&
    -1 != JAIL_SYS_INHERIT,
    "mac_do(4) uses -1 as a sentinel for uninitialized 'jsys'.");

/*
 * We perform only cheap checks here, i.e., we do not really parse the rules
 * specification string, if any.
 */
static int
mac_do_jail_check(void *obj, void *data)
{
	struct vfsoptlist *opts = data;
	char *rules_string;
	int error, jsys, size;

	error = vfs_copyopt(opts, "mac.do", &jsys, sizeof(jsys));
	if (error == ENOENT)
		jsys = -1;
	else {
		if (error != 0)
			return (error);
		if (jsys != JAIL_SYS_DISABLE && jsys != JAIL_SYS_NEW &&
		    jsys != JAIL_SYS_INHERIT)
			return (EINVAL);
	}

	/*
	 * We use vfs_getopt() here instead of vfs_getopts() to get the length.
	 * We perform the additional checks done by the latter here, even if
	 * jail_set() calls vfs_getopts() itself later (they becoming
	 * inconsistent wouldn't cause any security problem).
	 */
	error = vfs_getopt(opts, "mac.do.rules", (void**)&rules_string, &size);
	if (error == ENOENT) {
		/*
		 * Default (in absence of "mac.do.rules") is to disable (and, in
		 * particular, not inherit).
		 */
		if (jsys == -1)
			jsys = JAIL_SYS_DISABLE;

		if (jsys == JAIL_SYS_NEW) {
			vfs_opterror(opts, "'mac.do.rules' must be specified "
			    "given 'mac.do''s value");
			return (EINVAL);
		}

		/* Absence of "mac.do.rules" at this point is OK. */
		error = 0;
	} else {
		if (error != 0)
			return (error);

		/* Not a proper string. */
		if (size == 0 || rules_string[size - 1] != '\0') {
			vfs_opterror(opts, "'mac.do.rules' not a proper string");
			return (EINVAL);
		}

		if (size > MAC_RULE_STRING_LEN) {
			vfs_opterror(opts, "'mdo.rules' too long");
			return (ENAMETOOLONG);
		}

		if (jsys == -1)
			/* Default (if "mac.do.rules" is present). */
			jsys = rules_string[0] == '\0' ? JAIL_SYS_DISABLE :
			    JAIL_SYS_NEW;

		/*
		 * Be liberal and accept JAIL_SYS_DISABLE and JAIL_SYS_INHERIT
		 * with an explicit empty rules specification.
		 */
		switch (jsys) {
		case JAIL_SYS_DISABLE:
		case JAIL_SYS_INHERIT:
			if (rules_string[0] != '\0') {
				vfs_opterror(opts, "'mac.do.rules' specified "
				    "but should not given 'mac.do''s value");
				return (EINVAL);
			}
			break;
		}
	}

	return (error);
}

static int
mac_do_jail_set(void *obj, void *data)
{
	struct prison *pr = obj;
	struct vfsoptlist *opts = data;
	char *rules_string;
	int error, jsys;

	/*
	 * The invariants checks used below correspond to what has already been
	 * checked in jail_check() above.
	 */

	error = vfs_copyopt(opts, "mac.do", &jsys, sizeof(jsys));
	MPASS(error == 0 || error == ENOENT);
	if (error != 0)
		jsys = -1; /* Mark unfilled. */

	rules_string = vfs_getopts(opts, "mac.do.rules", &error);
	MPASS(error == 0 || error == ENOENT);
	if (error == 0) {
		MPASS(strlen(rules_string) < MAC_RULE_STRING_LEN);
		if (jsys == -1)
			/* Default (if "mac.do.rules" is present). */
			jsys = rules_string[0] == '\0' ? JAIL_SYS_DISABLE :
			    JAIL_SYS_NEW;
		else
			MPASS(jsys == JAIL_SYS_NEW ||
			    ((jsys == JAIL_SYS_DISABLE ||
			    jsys == JAIL_SYS_INHERIT) &&
			    rules_string[0] == '\0'));
	} else {
		MPASS(jsys != JAIL_SYS_NEW);
		if (jsys == -1)
			/*
			 * Default (in absence of "mac.do.rules") is to disable
			 * (and, in particular, not inherit).
			 */
			jsys = JAIL_SYS_DISABLE;
		/* If disabled, we'll store an empty rule specification. */
		if (jsys == JAIL_SYS_DISABLE)
			rules_string = "";
	}

	switch (jsys) {
	case JAIL_SYS_INHERIT:
		remove_rules(pr);
		error = 0;
		break;
	case JAIL_SYS_DISABLE:
	case JAIL_SYS_NEW:
		error = parse_and_set_rules(pr, rules_string);
		break;
	default:
		__assert_unreachable();
	}
	return (error);
}

/*
 * OSD jail methods.
 *
 * There is no PR_METHOD_REMOVE, as OSD storage is destroyed by the common jail
 * code (see prison_cleanup()), which triggers a run of our dealloc_osd()
 * destructor.
 */
static const osd_method_t osd_methods[PR_MAXMETHOD] = {
	[PR_METHOD_CREATE] = mac_do_jail_create,
	[PR_METHOD_GET] = mac_do_jail_get,
	[PR_METHOD_CHECK] = mac_do_jail_check,
	[PR_METHOD_SET] = mac_do_jail_set,
};


static void
mac_do_init(struct mac_policy_conf *mpc)
{
	struct prison *pr;

	mac_do_osd_jail_slot = osd_jail_register(dealloc_osd, osd_methods);
	set_empty_rules(&prison0);
	sx_slock(&allprison_lock);
	TAILQ_FOREACH(pr, &allprison, pr_list)
	    set_empty_rules(pr);
	sx_sunlock(&allprison_lock);
}

static void
mac_do_destroy(struct mac_policy_conf *mpc)
{
	osd_jail_deregister(mac_do_osd_jail_slot);
}

static bool
rule_applies(struct ucred *cred, struct rule *r)
{
	if (r->from_type == RULE_UID && r->from_id == cred->cr_uid)
		return (true);
	if (r->from_type == RULE_GID && groupmember(r->from_id, cred))
		return (true);
	return (false);
}

static int
mac_do_priv_grant(struct ucred *cred, int priv)
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
mac_do_check_setgroups(struct ucred *cred, int ngrp, gid_t *groups)
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
mac_do_check_setuid(struct ucred *cred, uid_t uid)
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
			if (cred->cr_uid != r->from_id)
				continue;
			if (r->to_type == RULE_ANY) {
				error = 0;
				break;
			}
			if (r->to_type == RULE_UID && uid == r->to_id) {
				error = 0;
				break;
			}
		}
		if (r->from_type == RULE_GID) {
			if (!groupmember(r->from_id, cred))
				continue;
			if (r->to_type == RULE_ANY) {
				error = 0;
				break;
			}
			if (r->to_type == RULE_UID && uid == r->to_id) {
				error = 0;
				break;
			}
		}
	}
	prison_unlock(pr);
	return (error);
}

static struct mac_policy_ops do_ops = {
	.mpo_destroy = mac_do_destroy,
	.mpo_init = mac_do_init,
	.mpo_cred_check_setuid = mac_do_check_setuid,
	.mpo_cred_check_setgroups = mac_do_check_setgroups,
	.mpo_priv_grant = mac_do_priv_grant,
};

MAC_POLICY_SET(&do_ops, mac_do, "MAC/do",
   MPC_LOADTIME_FLAG_UNLOADOK, NULL);
MODULE_VERSION(mac_do, 1);
