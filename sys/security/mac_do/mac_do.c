/*-
 * Copyright(c) 2024 Baptiste Daroussin <bapt@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ctype.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/limits.h>
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

static unsigned		osd_jail_slot;

#define IT_INVALID	0 /* Must stay 0. */
#define IT_UID		1
#define IT_GID		2
#define IT_ANY		3
#define IT_LAST		IT_ANY

static const char *id_type_to_str[] = {
	[IT_INVALID]	= "invalid",
	[IT_UID]	= "uid",
	[IT_GID]	= "gid",
	/* See also parse_id_type(). */
	[IT_ANY]	= "*",
};

/*
 * We assume that 'uid_t' and 'gid_t' are aliases to 'u_int' in conversions
 * required for parsing rules specification strings.
 */
_Static_assert(sizeof(uid_t) == sizeof(u_int) && (uid_t)-1 >= 0 &&
    sizeof(gid_t) == sizeof(u_int) && (gid_t)-1 >= 0,
    "mac_do(4) assumes that 'uid_t' and 'gid_t' are aliases to 'u_int'");

/*
 * Internal flags.
 *
 * They either apply as per-type (t) or per-ID (i) but are conflated because all
 * per-ID flags are also valid as per-type ones to qualify the "current" (".")
 * per-type flag.  Also, some of them are in fact exclusive, but we use one-hot
 * encoding for simplicity.
 *
 * There is currently room for "only" 16 bits.  As these flags are purely
 * internal, they can be renumbered and/or their type changed as needed.
 *
 * See also the check_*() functions below.
 */
typedef uint16_t	flags_t;

/* (i,gid) Specification concerns primary groups. */
#define MDF_PRIMARY	(1u << 0)
/* (i,gid) Specification concerns supplementary groups. */
#define MDF_SUPP_ALLOW	(1u << 1)
/* (i,gid) Group must appear as a supplementary group. */
#define MDF_SUPP_MUST	(1u << 2)
/* (i,gid) Group must not appear as a supplementary group. */
#define MDF_SUPP_DONT	(1u << 3)
#define MDF_SUPP_MASK	(MDF_SUPP_ALLOW | MDF_SUPP_MUST | MDF_SUPP_DONT)
#define MDF_ID_MASK	(MDF_PRIMARY | MDF_SUPP_MASK)

/*
 * (t) All IDs allowed.
 *
 * For GIDs, MDF_ANY only concerns primary groups.  The MDF_PRIMARY and
 * MDF_SUPP_* flags never apply to MDF_ANY, but can be present if MDF_CURRENT is
 * present also, as usual.
 */
#define MDF_ANY			(1u << 8)
/* (t) Current IDs allowed. */
#define MDF_CURRENT		(1u << 9)
#define MDF_TYPE_COMMON_MASK	(MDF_ANY | MDF_CURRENT)
/* (t,gid) All IDs allowed as supplementary groups. */
#define MDF_ANY_SUPP		(1u << 10)
/* (t,gid) Some ID or MDF_CURRENT has MDF_SUPP_MUST or MDF_SUPP_DONT. */
#define MDF_MAY_REJ_SUPP	(1u << 11)
/* (t,gid) Some explicit ID (not MDF_CURRENT) has MDF_SUPP_MUST. */
#define MDF_EXPLICIT_SUPP_MUST	(1u << 12)
/* (t,gid) Whether any target clause is about primary groups.  Used during
 * parsing only. */
#define MDF_HAS_PRIMARY_CLAUSE	(1u << 13)
/* (t,gid) Whether any target clause is about supplementary groups.  Used during
 * parsing only. */
#define MDF_HAS_SUPP_CLAUSE	(1u << 14)
#define MDF_TYPE_GID_MASK	(MDF_ANY_SUPP | MDF_MAY_REJ_SUPP |	\
    MDF_EXPLICIT_SUPP_MUST | MDF_HAS_PRIMARY_CLAUSE | MDF_HAS_SUPP_CLAUSE)
#define MDF_TYPE_MASK		(MDF_TYPE_COMMON_MASK | MDF_TYPE_GID_MASK)

/*
 * Persistent structures.
 */

struct id_spec {
	u_int		 id;
	flags_t		 flags; /* See MDF_* above. */
};

/*
 * This limits the number of target clauses per type to 65535.  With the current
 * value of MAC_RULE_STRING_LEN (1024), this is way more than enough anyway.
 */
typedef uint16_t	 id_nb_t;
/* We only have a few IT_* types. */
typedef uint16_t	 id_type_t;

struct rule {
	TAILQ_ENTRY(rule) r_entries;
	id_type_t	 from_type;
	u_int		 from_id;
	flags_t		 uid_flags; /* See MDF_* above. */
	id_nb_t		 uids_nb;
	flags_t		 gid_flags; /* See MDF_* above. */
	id_nb_t		 gids_nb;
	struct id_spec	*uids;
	struct id_spec	*gids;
};

TAILQ_HEAD(rulehead, rule);

struct rules {
	char string[MAC_RULE_STRING_LEN];
	struct rulehead head;
};

/*
 * Temporary structures used to build a 'struct rule' above.
 */

struct id_elem {
	TAILQ_ENTRY(id_elem) ie_entries;
	struct id_spec spec;
};

TAILQ_HEAD(id_list, id_elem);

#ifdef INVARIANTS
static void
check_type(const id_type_t type)
{
	if (type > IT_LAST)
		panic("Invalid type number %u", type);
}

static void
panic_for_unexpected_flags(const id_type_t type, const flags_t flags,
    const char *const str)
{
	panic("ID type %s: Unexpected flags %u (%s), ", id_type_to_str[type],
	    flags, str);
}

static void
check_type_and_id_flags(const id_type_t type, const flags_t flags)
{
	const char *str;

	check_type(type);
	switch (type) {
	case IT_UID:
		if (flags != 0) {
			str = "only 0 allowed";
			goto unexpected_flags;
		}
		break;
	case IT_GID:
		if ((flags & ~MDF_ID_MASK) != 0) {
			str = "only bits in MDF_ID_MASK allowed";
			goto unexpected_flags;
		}
		if (!powerof2(flags & MDF_SUPP_MASK)) {
			str = "only a single flag in MDF_SUPP_MASK allowed";
			goto unexpected_flags;
		}
		break;
	default:
	    __assert_unreachable();
	}
	return;

unexpected_flags:
	panic_for_unexpected_flags(type, flags, str);
}

static void
check_type_and_id_spec(const id_type_t type, const struct id_spec *const is)
{
	check_type_and_id_flags(type, is->flags);
}

static void
check_type_and_type_flags(const id_type_t type, const flags_t flags)
{
	const char *str;

	check_type_and_id_flags(type, flags & MDF_ID_MASK);
	if ((flags & ~MDF_ID_MASK & ~MDF_TYPE_MASK) != 0) {
		str = "only MDF_ID_MASK | MDF_TYPE_MASK bits allowed";
		goto unexpected_flags;
	}
	if ((flags & MDF_ANY) != 0 && (flags & MDF_CURRENT) != 0 &&
	    (type == IT_UID || (flags & MDF_PRIMARY) != 0)) {
		str = "MDF_ANY and MDF_CURRENT are exclusive for UIDs "
		    "or primary group GIDs";
		goto unexpected_flags;
	}
	if ((flags & MDF_ANY_SUPP) != 0 && (flags & MDF_CURRENT) != 0 &&
	    (flags & MDF_SUPP_MASK) != 0) {
		str = "MDF_SUPP_ANY and MDF_CURRENT with supplementary "
		    "groups specification are exclusive";
		goto unexpected_flags;
	}
	if (((flags & MDF_PRIMARY) != 0 || (flags & MDF_ANY) != 0) &&
	    (flags & MDF_HAS_PRIMARY_CLAUSE) == 0) {
		str = "Presence of folded primary clause not reflected "
		    "by presence of MDF_HAS_PRIMARY_CLAUSE";
		goto unexpected_flags;
	}
	if (((flags & MDF_SUPP_MASK) != 0 || (flags & MDF_ANY_SUPP) != 0) &&
	    (flags & MDF_HAS_SUPP_CLAUSE) == 0) {
		str = "Presence of folded supplementary clause not reflected "
		    "by presence of MDF_HAS_SUPP_CLAUSE";
		goto unexpected_flags;
	}
	return;

unexpected_flags:
	panic_for_unexpected_flags(type, flags, str);
}
#else /* !INVARIANTS */
#define check_type_and_id_flags(...)
#define check_type_and_id_spec(...)
#define check_type_and_type_flags(...)
#endif /* INVARIANTS */

/*
 * Returns EALREADY if both flags have some overlap, or EINVAL if flags are
 * incompatible, else 0 with flags successfully merged into 'dest'.
 */
static int
coalesce_id_flags(const flags_t src, flags_t *const dest)
{
	flags_t res;

	if ((src & *dest) != 0)
		return (EALREADY);

	res = src | *dest;

	/* Check for compatibility of supplementary flags, and coalesce. */
	if ((res & MDF_SUPP_MASK) != 0) {
		/* MDF_SUPP_DONT incompatible with the rest. */
		if ((res & MDF_SUPP_DONT) != 0 && (res & MDF_SUPP_MASK &
		    ~MDF_SUPP_DONT) != 0)
			return (EINVAL);
		/*
		 * Coalesce MDF_SUPP_ALLOW and MDF_SUPP_MUST into MDF_SUPP_MUST.
		 */
		if ((res & MDF_SUPP_ALLOW) != 0 && (res & MDF_SUPP_MUST) != 0)
			res &= ~MDF_SUPP_ALLOW;
	}

	*dest = res;
	return (0);
}

static void
toast_rules(struct rules *const rules)
{
	struct rulehead *const head = &rules->head;
	struct rule *rule;

	while ((rule = TAILQ_FIRST(head)) != NULL) {
		TAILQ_REMOVE(head, rule, r_entries);
		free(rule->uids, M_DO);
		free(rule->gids, M_DO);
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

static bool
is_null_or_empty(const char *s)
{
	return (s == NULL || s[0] == '\0');
}

/*
 * String to unsigned int.
 *
 * Contrary to the "standard" strtou*() family of functions, do not tolerate
 * spaces at start nor an empty string, and returns a status code, the 'u_int'
 * result being returned through a passed pointer (if no error).
 *
 * We detour through 'quad_t' because in-kernel strto*() functions cannot set
 * 'errno' and thus can't distinguish a true maximum value from one returned
 * because of overflow.  We use 'quad_t' instead of 'u_quad_t' to support
 * negative specifications (e.g., such as "-1" for UINT_MAX).
 */
static int
strtoui_strict(const char *const restrict s, const char **const restrict endptr,
    int base, u_int *result)
{
	char *ep;
	quad_t q;

	/* Rule out spaces and empty specifications. */
	if (s[0] == '\0' || isspace(s[0])) {
		if (endptr != NULL)
			*endptr = s;
		return (EINVAL);
	}

	q = strtoq(s, &ep, base);
	if (endptr != NULL)
		*endptr = ep;
	if (q < 0) {
		/* We allow specifying a negative number. */
		if (q < -(quad_t)UINT_MAX - 1 || q == QUAD_MIN)
			return (EOVERFLOW);
	} else {
		if (q > UINT_MAX || q == UQUAD_MAX)
			return (EOVERFLOW);
	}

	*result = (u_int)q;
	return (0);
}

static int
parse_id_type(const char *const string, id_type_t *const type)
{
	/*
	 * Special case for "any", as the canonical form for IT_ANY in
	 * id_type_to_str[] is "*".
	 */
	if (strcmp(string, "any") == 0) {
		*type = IT_ANY;
		return (0);
	}

	/* Start at 1 to avoid parsing "invalid". */
	for (size_t i = 1; i <= IT_LAST; ++i) {
		if (strcmp(string, id_type_to_str[i]) == 0) {
			*type = i;
			return (0);
		}
	}

	*type = IT_INVALID;
	return (EINVAL);
}

static size_t
parse_gid_flags(const char *const string, flags_t *const flags,
    flags_t *const gid_flags)
{
	switch (string[0]) {
	case '+':
		*flags |= MDF_SUPP_ALLOW;
		goto has_supp_clause;
	case '!':
		*flags |= MDF_SUPP_MUST;
		*gid_flags |= MDF_MAY_REJ_SUPP;
		goto has_supp_clause;
	case '-':
		*flags |= MDF_SUPP_DONT;
		*gid_flags |= MDF_MAY_REJ_SUPP;
		goto has_supp_clause;
	has_supp_clause:
		*gid_flags |= MDF_HAS_SUPP_CLAUSE;
		return (1);
	}

	return (0);
}

static bool
parse_any(const char *const string)
{
	return (strcmp(string, "*") == 0 || strcmp(string, "any") == 0);
}

static bool
has_clauses(const id_nb_t nb, const flags_t type_flags)
{
	return ((type_flags & MDF_TYPE_MASK) != 0 || nb != 0);
}

static int
parse_target_clause(char *to, struct rule *const rule,
    struct id_list *const uid_list, struct id_list *const gid_list)
{
	char *to_type, *to_id;
	const char *p;
	struct id_list *list;
	id_nb_t *nb;
	flags_t *tflags;
	struct id_elem *ie;
	struct id_spec is = {.flags = 0};
	flags_t gid_flags = 0;
	id_type_t type;
	int error;

	MPASS(to != NULL);
	to_type = strsep(&to, "=");
	MPASS(to_type != NULL);
	to_type += parse_gid_flags(to_type, &is.flags, &gid_flags);
	error = parse_id_type(to_type, &type);
	if (error != 0)
		goto einval;
	if (type != IT_GID && is.flags != 0)
		goto einval;

	to_id = strsep(&to, "");
	switch (type) {
	case IT_GID:
		if (to_id == NULL)
			goto einval;

		if (is.flags == 0) {
			/* No flags: Dealing with a primary group. */
			is.flags |= MDF_PRIMARY;
			gid_flags |= MDF_HAS_PRIMARY_CLAUSE;
		}

		list = gid_list;
		nb = &rule->gids_nb;
		tflags = &rule->gid_flags;

		/* "*" or "any"? */
		if (parse_any(to_id)) {
			/*
			 * We check that we have not seen any other clause of
			 * the same category (i.e., concerning primary or
			 * supplementary groups).
			 */
			if ((is.flags & MDF_PRIMARY) != 0) {
				if ((*tflags & MDF_HAS_PRIMARY_CLAUSE) != 0)
					goto einval;
				*tflags |= gid_flags | MDF_ANY;
			} else {
				/*
				 * If a supplementary group flag was present, it
				 * must be MDF_SUPP_ALLOW ("+").
				 */
				if ((is.flags & MDF_SUPP_MASK) != MDF_SUPP_ALLOW ||
				    (*tflags & MDF_HAS_SUPP_CLAUSE) != 0)
					goto einval;
				*tflags |= gid_flags | MDF_ANY_SUPP;
			}
			goto check_type_and_finish;
		} else {
			/*
			 * Check that we haven't already seen "any" for the same
			 * category.
			 */
			if ((is.flags & MDF_PRIMARY) != 0) {
				if ((*tflags & MDF_ANY) != 0)
					goto einval;
			} else if ((*tflags & MDF_ANY_SUPP) != 0 &&
			    (is.flags & MDF_SUPP_ALLOW) != 0)
				goto einval;
			*tflags |= gid_flags;
		}
		break;

	case IT_UID:
		if (to_id == NULL)
			goto einval;

		list = uid_list;
		nb = &rule->uids_nb;
		tflags = &rule->uid_flags;

		/* "*" or "any"? */
		if (parse_any(to_id)) {
			/* There must not be any other clause. */
			if (has_clauses(*nb, *tflags))
				goto einval;
			*tflags |= MDF_ANY;
			goto check_type_and_finish;
		} else {
			/*
			 * Check that we haven't already seen "any" for the same
			 * category.
			 */
			if ((*tflags & MDF_ANY) != 0)
				goto einval;
		}
		break;

	case IT_ANY:
		/* No ID allowed. */
		if (to_id != NULL)
			goto einval;
		/*
		 * We can't have IT_ANY after any other IT_*, it must be the
		 * only one.
		 */
		if (has_clauses(rule->uids_nb, rule->uid_flags) ||
		    has_clauses(rule->gids_nb, rule->gid_flags))
			goto einval;
		rule->uid_flags |= MDF_ANY;
		rule->gid_flags |= MDF_ANY | MDF_ANY_SUPP |
		    MDF_HAS_PRIMARY_CLAUSE | MDF_HAS_SUPP_CLAUSE;
		goto finish;

	default:
		/* parse_id_type() returns no other types currently. */
		__assert_unreachable();
	}

	/* Rule out cases that have been treated above. */
	MPASS((type == IT_UID || type == IT_GID) && !parse_any(to_id));

	/* "."? */
	if (strcmp(to_id, ".") == 0) {
		if ((*tflags & MDF_CURRENT) != 0) {
			/* Duplicate "." <id>.  Try to coalesce. */
			error = coalesce_id_flags(is.flags, tflags);
			if (error != 0)
				goto einval;
		} else
			*tflags |= MDF_CURRENT | is.flags;
		goto check_type_and_finish;
	}

	/* Parse an ID. */
	error = strtoui_strict(to_id, &p, 10, &is.id);
	if (error != 0 || *p != '\0')
		goto einval;

	/* Explicit ID flags. */
	if (type == IT_GID && (is.flags & MDF_SUPP_MUST) != 0)
		*tflags |= MDF_EXPLICIT_SUPP_MUST;

	/*
	 * We check for duplicate IDs and coalesce their 'struct id_spec' only
	 * at end of parse_single_rule() because it is much more performant then
	 * (using sorted arrays).
	 */
	++*nb;
	if (*nb == 0)
		return (EOVERFLOW);
	ie = malloc(sizeof(*ie), M_DO, M_WAITOK);
	ie->spec = is;
	TAILQ_INSERT_TAIL(list, ie, ie_entries);
	check_type_and_id_spec(type, &is);
finish:
	return (0);
check_type_and_finish:
	check_type_and_type_flags(type, *tflags);
	return (0);
einval:
	return (EINVAL);
}

static int
u_int_cmp(const u_int i1, const u_int i2)
{
	return ((i1 > i2) - (i1 < i2));
}

static int
id_spec_cmp(const void *const p1, const void *const p2)
{
	const struct id_spec *const is1 = p1;
	const struct id_spec *const is2 = p2;

	return (u_int_cmp(is1->id, is2->id));
}

/*
 * Transfer content of 'list' into 'array', freeing and emptying list.
 *
 * 'nb' must be 'list''s length and not be greater than 'array''s size.  The
 * destination array is sorted by ID.  Structures 'struct id_spec' with same IDs
 * are coalesced if that makes sense (not including duplicate clauses), else
 * EINVAL is returned.  On success, 'nb' is updated (lowered) to account for
 * coalesced specifications.  The parameter 'type' is only for testing purposes
 * (INVARIANTS).
 */
static int
pour_list_into_rule(const id_type_t type, struct id_list *const list,
    struct id_spec *const array, id_nb_t *const nb)
{
	struct id_elem *ie, *ie_next;
	size_t idx = 0;

	/* Fill the array. */
	TAILQ_FOREACH_SAFE(ie, list, ie_entries, ie_next) {
		MPASS(idx < *nb);
		array[idx] = ie->spec;
		free(ie, M_DO);
		++idx;
	}
	MPASS(idx == *nb);
	TAILQ_INIT(list);

	/* Sort it (by ID). */
	qsort(array, *nb, sizeof(*array), id_spec_cmp);

	/* Coalesce same IDs. */
	if (*nb != 0) {
		size_t ref_idx = 0;

		for (idx = 1; idx < *nb; ++idx) {
			const u_int id = array[idx].id;

			if (id != array[ref_idx].id) {
				++ref_idx;
				if (ref_idx != idx)
					array[ref_idx] = array[idx];
				continue;
			}

			switch (type) {
				int error;

			case IT_GID:
				error = coalesce_id_flags(array[idx].flags,
				    &array[ref_idx].flags);
				if (error != 0)
					return (EINVAL);
				check_type_and_id_flags(type,
				    array[ref_idx].flags);
				break;

			case IT_UID:
				/*
				 * No flags in this case.  Multiple appearances
				 * of the same UID is an exact redundancy, so
				 * error out.
				 */
				return (EINVAL);

			default:
				__assert_unreachable();
			}
		}
		*nb = ref_idx + 1;
	}

	return (0);
}

/*
 * See also first comments for parse_rule() below.
 *
 * The second part of a rule, called <target> (or <to>), is a comma-separated
 * (',') list of '<flags><type>=<id>' clauses similar to that of the <from>
 * part, with the extensions that <id> may also be "*" or "any" or ".", and that
 * <flags> may contain at most one of the '+', '-' and '!' characters when
 * <type> is "gid" (no flags are allowed for "uid").  No two clauses in a single
 * <to> list may list the same <id>.  "*" and "any" both designate any ID for
 * the <type>, and are aliases to each other.  In front of "any" (or "*"), only
 * the '+' flag is allowed (in the "gid" case).  "." designates the process'
 * current IDs for the <type>.  The precise meaning of flags and "." is
 * explained in functions checking privileges below.
 */
static int
parse_single_rule(char *rule, struct rules *const rules)
{
	const char *from_type, *from_id, *p;
	char *to_list;
	struct id_list uid_list, gid_list;
	struct id_elem *ie, *ie_next;
	struct rule *new;
	int error;

	MPASS(rule != NULL);
	TAILQ_INIT(&uid_list);
	TAILQ_INIT(&gid_list);

	/* Freed when the 'struct rules' container is freed. */
	new = malloc(sizeof(*new), M_DO, M_WAITOK | M_ZERO);

	from_type = strsep(&rule, "=");
	MPASS(from_type != NULL); /* Because 'rule' was not NULL. */
	error = parse_id_type(from_type, &new->from_type);
	if (error != 0)
		goto einval;
	switch (new->from_type) {
	case IT_UID:
	case IT_GID:
		break;
	default:
		goto einval;
	}

	from_id = strsep(&rule, ":");
	if (is_null_or_empty(from_id))
		goto einval;

	error = strtoui_strict(from_id, &p, 10, &new->from_id);
	if (error != 0 || *p != '\0')
		goto einval;

	/*
	 * We will now parse the "to" list.
	 *
	 * In order to ease parsing, we will begin by building lists of target
	 * UIDs and GIDs in local variables 'uid_list' and 'gid_list'.  The
	 * number of each type of IDs will be filled directly in 'new'.  At end
	 * of parse, we will allocate both arrays of IDs to be placed into the
	 * 'uids' and 'gids' members, sort them, and discard the tail queues
	 * used to build them.  This conversion to sorted arrays at end of parse
	 * allows to minimize memory allocations and enables searching IDs in
	 * O(log(n)) instead of linearly.
	 */
	to_list = strsep(&rule, ",");
	if (to_list == NULL)
		goto einval;
	do {
		error = parse_target_clause(to_list, new, &uid_list, &gid_list);
		if (error != 0)
			goto einval;

		to_list = strsep(&rule, ",");
	} while (to_list != NULL);

	if (new->uids_nb != 0) {
		new->uids = malloc(sizeof(*new->uids) * new->uids_nb, M_DO,
		    M_WAITOK);
		error = pour_list_into_rule(IT_UID, &uid_list, new->uids,
		    &new->uids_nb);
		if (error != 0)
			goto einval;
	}
	MPASS(TAILQ_EMPTY(&uid_list));
	if (!has_clauses(new->uids_nb, new->uid_flags)) {
		/* No UID specified, default is "uid=.". */
		MPASS(new->uid_flags == 0);
		new->uid_flags = MDF_CURRENT;
		check_type_and_type_flags(IT_UID, new->uid_flags);
	}

	if (new->gids_nb != 0) {
		new->gids = malloc(sizeof(*new->gids) * new->gids_nb, M_DO,
		    M_WAITOK);
		error = pour_list_into_rule(IT_GID, &gid_list, new->gids,
		    &new->gids_nb);
		if (error != 0)
			goto einval;
	}
	MPASS(TAILQ_EMPTY(&gid_list));
	if (!has_clauses(new->gids_nb, new->gid_flags)) {
		/* No GID specified, default is "gid=.,!gid=.". */
		MPASS(new->gid_flags == 0);
		new->gid_flags = MDF_CURRENT | MDF_PRIMARY | MDF_SUPP_MUST |
		    MDF_HAS_PRIMARY_CLAUSE | MDF_HAS_SUPP_CLAUSE;
		check_type_and_type_flags(IT_GID, new->gid_flags);
	}

	TAILQ_INSERT_TAIL(&rules->head, new, r_entries);
	return (0);

einval:
	free(new->gids, M_DO);
	free(new->uids, M_DO);
	free(new, M_DO);
	TAILQ_FOREACH_SAFE(ie, &gid_list, ie_entries, ie_next)
	    free(ie, M_DO);
	TAILQ_FOREACH_SAFE(ie, &uid_list, ie_entries, ie_next)
	    free(ie, M_DO);
	return (EINVAL);
}

/*
 * Parse rules specification and produce rule structures out of it.
 *
 * Returns 0 on success, with '*rulesp' made to point to a 'struct rule'
 * representing the rules.  On error, the returned value is non-zero and
 * '*rulesp' is unchanged.  If 'string' has length greater or equal to
 * MAC_RULE_STRING_LEN, ENAMETOOLONG is returned.  If it is not in the expected
 * format, EINVAL is returned.
 *
 * Expected format: A semi-colon-separated list of rules of the form
 * "<from>:<target>".  The <from> part is of the form "<type>=<id>" where <type>
 * is "uid" or "gid", <id> an UID or GID (depending on <type>) and <target> is
 * "*", "any" or a comma-separated list of '<flags><type>=<id>' clauses (see the
 * comment for parse_single_rule() for more details).  For convenience, empty
 * rules are allowed (and do nothing).
 *
 * Examples:
 * - "uid=1001:uid=1010,gid=1010;uid=1002:any"
 * - "gid=1010:gid=1011,gid=1012,gid=1013"
 */
static int
parse_rules(const char *const string, struct rules **const rulesp)
{
	const size_t len = strlen(string);
	char *copy, *p, *rule;
	struct rules *rules;
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
	while ((rule = strsep(&p, ";")) != NULL) {
		if (rule[0] == '\0')
			continue;
		error = parse_single_rule(rule, rules);
		if (error != 0) {
			toast_rules(rules);
			goto out;
		}
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
		rules = osd_jail_get(cpr, osd_jail_slot);
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
 * OSD destructor for slot 'osd_jail_slot'.
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
 * Destroys the 'osd_jail_slot' slot of the passed jail.
 */
static void
remove_rules(struct prison *const pr)
{
	prison_lock(pr);
	/* This calls destructor dealloc_osd(). */
	osd_jail_del(pr, osd_jail_slot);
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

	rsv = osd_reserve(osd_jail_slot);

	prison_lock(pr);
	old_rules = osd_jail_get(pr, osd_jail_slot);
	osd_jail_set_reserved(pr, osd_jail_slot, rsv, rules);
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

	osd_jail_slot = osd_jail_register(dealloc_osd, osd_methods);
	set_empty_rules(&prison0);
	sx_slock(&allprison_lock);
	TAILQ_FOREACH(pr, &allprison, pr_list)
	    set_empty_rules(pr);
	sx_sunlock(&allprison_lock);
}

static void
mac_do_destroy(struct mac_policy_conf *mpc)
{
	osd_jail_deregister(osd_jail_slot);
}

static bool
rule_applies(struct ucred *cred, struct rule *r)
{
	if (r->from_type == IT_UID && r->from_id == cred->cr_uid)
		return (true);
	if (r->from_type == IT_GID && groupmember(r->from_id, cred))
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
	char *fullpath = NULL;
	char *freebuf = NULL;
	struct prison *pr;
	struct rules *rule;
	struct id_spec uid_is = {.id = uid};
	int error;

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
		if (!((r->from_type == IT_UID && cred->cr_uid == r->from_id) ||
		    (r->from_type == IT_GID && groupmember(r->from_id, cred))))
			continue;

		if (r->uid_flags & MDF_ANY ||
		    ((r->uid_flags & MDF_CURRENT) && (uid == cred->cr_uid ||
		    uid == cred->cr_ruid || uid == cred->cr_svuid)) ||
		    bsearch(&uid_is, r->uids, r->uids_nb, sizeof(*r->uids),
		    id_spec_cmp) != NULL) {
			error = 0;
			break;
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
