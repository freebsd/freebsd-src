/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright(c) 2024 Baptiste Daroussin <bapt@FreeBSD.org>
 * Copyright (c) 2025 Kushagra Srivastava <kushagra1403@gmail.com>
 * Copyright (c) 2025 The FreeBSD Foundation
 *
 * Portions of this software were developed by Olivier Certner
 * <olce@FreeBSD.org> at Kumacom SARL under sponsorship from the FreeBSD
 * Foundation.
 */

#include <sys/errno.h>
#include <sys/limits.h>
#include <sys/types.h>
#include <sys/ucred.h>

#include <assert.h>
#include <err.h>
#include <getopt.h>
#include <grp.h>
#include <paths.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


static void
usage(void)
{
	fprintf(stderr,
	    "Usage: mdo [options] [--] [command [args...]]\n"
	    "\n"
	    "Options:\n"
	    "  -u <user>         Target user (name or UID; name sets groups)\n"
	    "  -k                Keep current user, allows selective overrides "
	    "(implies -i)\n"
	    "  -i                Keep current groups, unless explicitly overridden\n"
	    "  -g <group>        Override primary group (name or GID)\n"
	    "  -G <g1,g2,...>    Set supplementary groups (name or GID list)\n"
	    "  -s <mods>         Modify supplementary groups using:\n"
	    "                      @ (first) to reset, +group to add, -group to remove\n"
	    "\n"
	    "Advanced UID/GID overrides:\n"
	    "  --euid <uid>      Set effective UID\n"
	    "  --ruid <uid>      Set real UID\n"
	    "  --svuid <uid>     Set saved UID\n"
	    "  --egid <gid>      Set effective GID\n"
	    "  --rgid <gid>      Set real GID\n"
	    "  --svgid <gid>     Set saved GID\n"
	    "\n"
	    "  -h                Show this help message\n"
	    "\n"
	    "Examples:\n"
	    "  mdo -u alice id\n"
	    "  mdo -u 1001 -g wheel -G staff,operator sh\n"
	    "  mdo -u bob -s +wheel,+operator id\n"
	    "  mdo -k --ruid 1002 --egid 1004 id\n"
	);
	exit(1);
}

struct alloc {
	void	*start;
	size_t	 size;
};

static const struct alloc ALLOC_INITIALIZER = {
	.start = NULL,
	.size = 0,
};

/*
 * The default value should cover almost all cases.
 *
 * For getpwnam_r(), we assume:
 * - 88 bytes for 'struct passwd'
 * - Less than 16 bytes for the user name
 * - A typical shadow hash of 106 bytes
 * - Less than 16 bytes for the login class name
 * - Less than 64 bytes for GECOS info
 * - Less than 128 bytes for the home directory
 * - Less than 32 bytes for the shell path
 * Total: 256 + 88 + 106 = 450.
 *
 * For getgrnam_r(), we assume:
 * - 32 bytes for 'struct group'
 * - Less than 16 bytes for the group name
 * - Some hash of 106 bytes
 * - No more than 16 members, each of less than 16 bytes (=> 256 bytes)
 * Total: 256 + 32 + 16 + 106 = 410.
 *
 * We thus choose 512 (leeway, power of 2).
 */
static const size_t ALLOC_FIRST_SIZE = 512;

static bool
alloc_is_empty(const struct alloc *const alloc)
{
	if (alloc->size == 0) {
		assert(alloc->start == NULL);
		return (true);
	} else {
		assert(alloc->start != NULL);
		return (false);
	}
}

static void
alloc_realloc(struct alloc *const alloc)
{
	const size_t old_size = alloc->size;
	size_t new_size;

	if (old_size == 0) {
		assert(alloc->start == NULL);
		new_size = ALLOC_FIRST_SIZE;
	} else if (old_size < PAGE_SIZE)
		new_size = 2 * old_size;
	else
		/*
		 * We never allocate more than a page at a time when reaching
		 * a page (except perhaps for the first increment, up to two).
		 * Use roundup2() to be immune to previous cases' changes. */
		new_size = roundup2(old_size, PAGE_SIZE) + PAGE_SIZE;

	alloc->start = realloc(alloc->start, new_size);
	if (alloc->start == NULL)
		errx(EXIT_FAILURE,
		    "cannot realloc allocation (old size: %zu, new: %zu)",
		    old_size, new_size);
	alloc->size = new_size;
}

static void
alloc_free(struct alloc *const alloc)
{
	if (!alloc_is_empty(alloc)) {
		free(alloc->start);
		*alloc = ALLOC_INITIALIZER;
	}
}

struct alloc_wrap_data {
	int (*func)(void *data, const struct alloc *alloc);
};

/*
 * Wraps functions needing a backing allocation.
 *
 * Uses 'alloc' as the starting allocation, and may extend it as necessary.
 * 'alloc' is never freed, even on failure of the wrapped function.
 *
 * The function is expected to return ERANGE if and only if the provided
 * allocation is not big enough.  All other values are passed through.
 */
static int
alloc_wrap(struct alloc_wrap_data *const data, struct alloc *alloc)
{
	int error;

	/* Avoid a systematic ERANGE on first iteration. */
	if (alloc_is_empty(alloc))
		alloc_realloc(alloc);

	for (;;) {
		error = data->func(data, alloc);
		if (error != ERANGE)
			break;
		alloc_realloc(alloc);
	}

	return (error);
}

struct getpwnam_wrapper_data {
	struct alloc_wrap_data	  wrapped;
	const char		 *name;
	struct passwd		**pwdp;
};

static int
wrapped_getpwnam_r(void *data, const struct alloc *alloc)
{
	struct passwd *const pwd = alloc->start;
	struct passwd *result;
	struct getpwnam_wrapper_data *d = data;
	int error;

	assert(alloc->size >= sizeof(*pwd));

	error = getpwnam_r(d->name, pwd, (char *)(pwd + 1),
	    alloc->size - sizeof(*pwd), &result);

	if (error == 0) {
		if (result == NULL)
			error = ENOENT;
	} else
		assert(result == NULL);
	*d->pwdp = result;
	return (error);
}

/*
 * Wraps getpwnam_r(), automatically dealing with memory allocation.
 *
 * 'alloc' may be any allocation (even empty), and will be extended as
 * necessary.  It is not freed on error.
 *
 * On success, '*pwdp' is filled with a pointer to the returned 'struct passwd',
 * and on failure, is set to NULL.
 */
static int
alloc_getpwnam(const char *name, struct passwd **pwdp,
    struct alloc *const alloc)
{
	struct getpwnam_wrapper_data data;

	data.wrapped.func = wrapped_getpwnam_r;
	data.name = name;
	data.pwdp = pwdp;
	return (alloc_wrap((struct alloc_wrap_data *)&data, alloc));
}

struct getgrnam_wrapper_data {
	struct alloc_wrap_data	  wrapped;
	const char		 *name;
	struct group		**grpp;
};

static int
wrapped_getgrnam_r(void *data, const struct alloc *alloc)
{
	struct group *grp = alloc->start;
	struct group *result;
	struct getgrnam_wrapper_data *d = data;
	int error;

	assert(alloc->size >= sizeof(*grp));

	error = getgrnam_r(d->name, grp, (char *)(grp + 1),
	    alloc->size - sizeof(*grp), &result);

	if (error == 0) {
		if (result == NULL)
			error = ENOENT;
	} else
		assert(result == NULL);
	*d->grpp = result;
	return (error);
}

/*
 * Wraps getgrnam_r(), automatically dealing with memory allocation.
 *
 * 'alloc' may be any allocation (even empty), and will be extended as
 * necessary.  It is not freed on error.
 *
 * On success, '*grpp' is filled with a pointer to the returned 'struct group',
 * and on failure, is set to NULL.
 */
static int
alloc_getgrnam(const char *const name, struct group **const grpp,
    struct alloc *const alloc)
{
	struct getgrnam_wrapper_data data;

	data.wrapped.func = wrapped_getgrnam_r;
	data.name = name;
	data.grpp = grpp;
	return (alloc_wrap((struct alloc_wrap_data *)&data, alloc));
}

/*
 * Retrieve the UID from a user string.
 *
 * Tries first to interpret the string as a user name, then as a numeric ID
 * (this order is prescribed by POSIX for a number of utilities).
 *
 * 'pwdp' and 'allocp' must be NULL or non-NULL together.  If non-NULL, then
 * 'allocp' can be any allocation (possibly empty) and will be extended to
 * contain the result if necessary.  It will not be freed (even on failure).
 */
static uid_t
parse_user_pwd(const char *s, struct passwd **pwdp, struct alloc *allocp)
{
	struct passwd *pwd;
	struct alloc alloc = ALLOC_INITIALIZER;
	const char *errp;
	uid_t uid;
	int error;

	assert((pwdp == NULL && allocp == NULL) ||
	    (pwdp != NULL && allocp != NULL));

	if (pwdp == NULL) {
		pwdp = &pwd;
		allocp = &alloc;
	}

	error = alloc_getpwnam(s, pwdp, allocp);
	if (error == 0) {
		uid = (*pwdp)->pw_uid;
		goto finish;
	} else if (error != ENOENT)
		errc(EXIT_FAILURE, error,
		    "cannot access the password database");

	uid = strtonum(s, 0, UID_MAX, &errp);
	if (errp != NULL)
		errx(EXIT_FAILURE, "invalid UID '%s': %s", s, errp);

finish:
	if (allocp == &alloc)
		alloc_free(allocp);
	return (uid);
}

/* See parse_user_pwd() for the doc. */
static uid_t
parse_user(const char *s)
{
	return (parse_user_pwd(s, NULL, NULL));
}

/*
 * Retrieve the GID from a group string.
 *
 * Tries first to interpret the string as a group name, then as a numeric ID
 * (this order is prescribed by POSIX for a number of utilities).
 */
static gid_t
parse_group(const char *s)
{
	struct group *grp;
	struct alloc alloc = ALLOC_INITIALIZER;
	const char *errp;
	gid_t gid;
	int error;

	error = alloc_getgrnam(s, &grp, &alloc);
	if (error == 0) {
		gid = grp->gr_gid;
		goto finish;
	} else if (error != ENOENT)
		errc(EXIT_FAILURE, error, "cannot access the group database");

	gid = strtonum(s, 0, GID_MAX, &errp);
	if (errp != NULL)
		errx(EXIT_FAILURE, "invalid GID '%s': %s", s, errp);

finish:
	alloc_free(&alloc);
	return (gid);
}

struct group_array {
	u_int	 nb;
	gid_t	*groups;
};

static const struct group_array GROUP_ARRAY_INITIALIZER = {
	.nb = 0,
	.groups = NULL,
};

static bool
group_array_is_empty(const struct group_array *const ga)
{
	return (ga->nb == 0);
}

static void
realloc_groups(struct group_array *const ga, const u_int diff)
{
	const u_int new_nb = ga->nb + diff;
	const size_t new_size = new_nb * sizeof(*ga->groups);

	assert(new_nb >= diff && new_size >= new_nb);
	ga->groups = realloc(ga->groups, new_size);
	if (ga->groups == NULL)
		err(EXIT_FAILURE, "realloc of groups failed");
	ga->nb = new_nb;
}

static int
gidp_cmp(const void *p1, const void *p2)
{
	const gid_t g1 = *(const gid_t *)p1;
	const gid_t g2 = *(const gid_t *)p2;

	return ((g1 > g2) - (g1 < g2));
}

static void
sort_uniq_groups(struct group_array *const ga)
{
	size_t j = 0;

	if (ga->nb <= 1)
		return;

	qsort(ga->groups, ga->nb, sizeof(gid_t), gidp_cmp);

	for (size_t i = 1; i < ga->nb; ++i)
		if (ga->groups[i] != ga->groups[j])
			ga->groups[++j] = ga->groups[i];
}

/*
 * Remove elements in 'set' that are in 'remove'.
 *
 * Expects both arrays to have been treated with sort_uniq_groups(). Works in
 * O(n + m), modifying 'set' in place.
 */
static void
remove_groups(struct group_array *const set,
    const struct group_array *const remove)
{
	u_int from = 0, to = 0, rem = 0;
	gid_t cand, to_rem;

	if (set->nb == 0 || remove->nb == 0)
		/* Nothing to remove. */
		return;

	cand = set->groups[0];
	to_rem = remove->groups[0];

	for (;;) {
		if (cand < to_rem) {
			/* Keep. */
			if (to != from)
				set->groups[to] = cand;
			++to;
			cand = set->groups[++from];
			if (from == set->nb)
				break;
		} else if (cand == to_rem) {
			cand = set->groups[++from];
			if (from == set->nb)
				break;
			to_rem = remove->groups[++rem]; /* No duplicates. */
			if (rem == remove->nb)
				break;
		} else {
			to_rem = remove->groups[++rem];
			if (rem == remove->nb)
				break;
		}
	}

	/* All remaining groups in 'set' must be kept. */
	if (from == to)
		/* Nothing was removed.  'set' will stay the same. */
		return;
	memmove(set->groups + to, set->groups + from,
	    (set->nb - from) * sizeof(gid_t));
	set->nb = to + (set->nb - from);
}

int
main(int argc, char **argv)
{
	const char *const default_user = "root";

	const char *user_name = NULL;
	const char *primary_group = NULL;
	char *supp_groups_str = NULL;
	char *supp_mod_str = NULL;
	bool start_from_current_groups = false;
	bool start_from_current_users = false;
	const char *euid_str = NULL;
	const char *ruid_str = NULL;
	const char *svuid_str = NULL;
	const char *egid_str = NULL;
	const char *rgid_str = NULL;
	const char *svgid_str = NULL;
	bool need_user = false; /* '-u' or '-k' needed. */

	const int go_euid = 1000;
	const int go_ruid = 1001;
	const int go_svuid = 1002;
	const int go_egid = 1003;
	const int go_rgid = 1004;
	const int go_svgid = 1005;
	const struct option longopts[] = {
		{"euid", required_argument, NULL, go_euid},
		{"ruid", required_argument, NULL, go_ruid},
		{"svuid", required_argument, NULL, go_svuid},
		{"egid", required_argument, NULL, go_egid},
		{"rgid", required_argument, NULL, go_rgid},
		{"svgid", required_argument, NULL, go_svgid},
		{NULL, 0, NULL, 0}
	};
	int ch;

	struct setcred wcred = SETCRED_INITIALIZER;
	u_int setcred_flags = 0;

	struct passwd *pw = NULL;
	struct alloc pw_alloc = ALLOC_INITIALIZER;
	struct group_array supp_groups = GROUP_ARRAY_INITIALIZER;
	struct group_array supp_rem = GROUP_ARRAY_INITIALIZER;


	/*
	 * Process options.
	 */
	while (ch = getopt_long(argc, argv, "+G:g:hiks:u:", longopts, NULL),
	    ch != -1) {
		switch (ch) {
		case 'G':
			supp_groups_str = optarg;
			need_user = true;
			break;
		case 'g':
			primary_group = optarg;
			need_user = true;
			break;
		case 'h':
			usage();
		case 'i':
			start_from_current_groups = true;
			break;
		case 'k':
			start_from_current_users = true;
			break;
		case 's':
			supp_mod_str = optarg;
			need_user = true;
			break;
		case 'u':
			user_name = optarg;
			break;
		case go_euid:
			euid_str = optarg;
			need_user = true;
			break;
		case go_ruid:
			ruid_str = optarg;
			need_user = true;
			break;
		case go_svuid:
			svuid_str = optarg;
			need_user = true;
			break;
		case go_egid:
			egid_str = optarg;
			need_user = true;
			break;
		case go_rgid:
			rgid_str = optarg;
			need_user = true;
			break;
		case go_svgid:
			svgid_str = optarg;
			need_user = true;
			break;
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	/*
	 * Determine users.
	 *
	 * We do that first as in some cases we need to retrieve the
	 * corresponding password database entry to be able to set the primary
	 * groups.
	 */

	if (start_from_current_users) {
		if (user_name != NULL)
			errx(EXIT_FAILURE, "-k incompatible with -u");

		/*
		 * If starting from the current user(s) as a base, finding one
		 * of them in the password database and using its groups would
		 * be quite surprising, so we instead let '-k' imply '-i'.
		 */
		start_from_current_groups = true;
	} else {
		uid_t uid;

		/*
		 * In the case of any overrides, we impose an explicit base user
		 * via '-u' or '-k' instead of implicitly taking 'root' as the
		 * base.
		 */
		if (user_name == NULL) {
			if (need_user)
				errx(EXIT_FAILURE,
				    "Some overrides specified, "
				    "'-u' or '-k' needed.");
			user_name = default_user;
		}

		/*
		 * Even if all user overrides are present as well as primary and
		 * supplementary groups ones, in which case the final result
		 * doesn't depend on '-u', we still call parse_user_pwd() to
		 * check that the passed username is correct.
		 */
		uid = parse_user_pwd(user_name, &pw, &pw_alloc);
		wcred.sc_uid = wcred.sc_ruid = wcred.sc_svuid = uid;
		setcred_flags |= SETCREDF_UID | SETCREDF_RUID |
		    SETCREDF_SVUID;
	}

	if (euid_str != NULL) {
		wcred.sc_uid = parse_user(euid_str);
		setcred_flags |= SETCREDF_UID;
	}

	if (ruid_str != NULL) {
		wcred.sc_ruid = parse_user(ruid_str);
		setcred_flags |= SETCREDF_RUID;
	}

	if (svuid_str != NULL) {
		wcred.sc_svuid = parse_user(svuid_str);
		setcred_flags |= SETCREDF_SVUID;
	}

	/*
	 * Determine primary groups.
	 */

	/*
	 * When not starting from the current groups, we need to set all
	 * primary groups.  If '-g' was not passed, we use the primary
	 * group from the password database as the "base" to which
	 * overrides '--egid', '--rgid' and '--svgid' apply.  But if all
	 * overrides were specified, we in fact just don't need the
	 * password database at all.
	 *
	 * '-g' is treated outside of this 'if' as it can also be used
	 * as an override.
	 */
	if (!start_from_current_groups && primary_group == NULL &&
	    (egid_str == NULL || rgid_str == NULL || svgid_str == NULL)) {
		if (pw == NULL)
			errx(EXIT_FAILURE,
			    "must specify primary groups or a user name "
			    "with an entry in the password database");

		wcred.sc_gid = wcred.sc_rgid = wcred.sc_svgid =
		    pw->pw_gid;
		setcred_flags |= SETCREDF_GID | SETCREDF_RGID |
		    SETCREDF_SVGID;
	}

	if (primary_group != NULL) {
		/*
		 * We always call parse_group() even in case all overrides are
		 * present to check that the passed group is valid.
		 */
		wcred.sc_gid = wcred.sc_rgid = wcred.sc_svgid =
		    parse_group(primary_group);
		setcred_flags |= SETCREDF_GID | SETCREDF_RGID | SETCREDF_SVGID;
	}

	if (egid_str != NULL) {
		wcred.sc_gid = parse_group(egid_str);
		setcred_flags |= SETCREDF_GID;
	}

	if (rgid_str != NULL) {
		wcred.sc_rgid = parse_group(rgid_str);
		setcred_flags |= SETCREDF_RGID;
	}

	if (svgid_str != NULL) {
		wcred.sc_svgid = parse_group(svgid_str);
		setcred_flags |= SETCREDF_SVGID;
	}

	/*
	 * Determine supplementary groups.
	 */

	/*
	 * This makes sense to catch user's mistakes.  It is not a strong
	 * limitation of the code below (allowing this case is just a matter of,
	 * in the block treating '-s' with '@' below, replacing an assert() by
	 * a reset of 'supp_groups').
	 */
	if (supp_groups_str != NULL && supp_mod_str != NULL &&
	    supp_mod_str[0] == '@')
		errx(EXIT_FAILURE, "'-G' and '-s' with '@' are incompatible");

	/*
	 * Determine the supplementary groups to start with, but only if we
	 * really need to operate on them later (and set them back).
	 */
	if (!start_from_current_groups) {
		assert(!start_from_current_users);

		if (supp_groups_str == NULL && (supp_mod_str == NULL ||
		    supp_mod_str[0] != '@')) {
			/*
			 * If we are to replace supplementary groups (i.e.,
			 * neither '-i' nor '-k' was specified) and they are not
			 * completely specified (with '-g' or '-s' with '@'), we
			 * start from those in the groups database if we were
			 * passed a user name that is in the password database
			 * (this is a protection against erroneous ID/name
			 * conflation in the groups database), else we simply
			 * error.
			 */

			if (pw == NULL)
				errx(EXIT_FAILURE,
				    "must specify the full supplementary "
				    "groups set or a user name with an entry "
				    "in the password database");

			const long ngroups_alloc = sysconf(_SC_NGROUPS_MAX) + 1;
			gid_t *groups;
			int ngroups;

			groups = malloc(sizeof(*groups) * ngroups_alloc);
			if (groups == NULL)
				errx(EXIT_FAILURE,
				    "cannot allocate memory to retrieve "
				    "user groups from the groups database");

			ngroups = ngroups_alloc;
			getgrouplist(user_name, pw->pw_gid, groups, &ngroups);

			if (ngroups > ngroups_alloc)
				err(EXIT_FAILURE,
				    "too many groups for user '%s'",
				    user_name);

			realloc_groups(&supp_groups, ngroups);
			memcpy(supp_groups.groups + supp_groups.nb - ngroups,
			    groups, ngroups * sizeof(*groups));
			free(groups);

			/*
			 * Have to set SETCREDF_SUPP_GROUPS here since we may be
			 * in the case where neither '-G' nor '-s' was passed,
			 * but we still have to set the supplementary groups to
			 * those of the groups database.
			 */
			setcred_flags |= SETCREDF_SUPP_GROUPS;
		}
	} else if (supp_groups_str == NULL && (supp_mod_str == NULL ||
	    supp_mod_str[0] != '@')) {
		const int ngroups = getgroups(0, NULL);

		if (ngroups > 0) {
			realloc_groups(&supp_groups, ngroups);

			if (getgroups(ngroups, supp_groups.groups +
			    supp_groups.nb - ngroups) < 0)
				err(EXIT_FAILURE, "getgroups() failed");
		}

		/*
		 * Setting SETCREDF_SUPP_GROUPS here is not necessary, we will
		 * do it below since 'supp_mod_str' != NULL.
		 */
	}

	if (supp_groups_str != NULL) {
		char *p = supp_groups_str;
		char *tok;

		/*
		 * We will set the supplementary groups to exactly the set
		 * passed with '-G', and we took care above not to retrieve
		 * "base" groups (current ones or those from the groups
		 * database) in this case.
		 */
		assert(group_array_is_empty(&supp_groups));

		/* WARNING: 'supp_groups_str' going to be modified. */
		while ((tok = strsep(&p, ",")) != NULL) {
			gid_t g;

			if (*tok == '\0')
				continue;

			g = parse_group(tok);
			realloc_groups(&supp_groups, 1);
			supp_groups.groups[supp_groups.nb - 1] = g;
		}

		setcred_flags |= SETCREDF_SUPP_GROUPS;
	}

	if (supp_mod_str != NULL) {
		char *p = supp_mod_str;
		char *tok;
		gid_t gid;

		/* WARNING: 'supp_mod_str' going to be modified. */
		while ((tok = strsep(&p, ",")) != NULL) {
			switch (tok[0]) {
			case '\0':
			        break;

			case '@':
				if (tok != supp_mod_str)
					errx(EXIT_FAILURE, "'@' must be "
					    "the first token in '-s' option");
				/* See same assert() above. */
				assert(group_array_is_empty(&supp_groups));
				break;

			case '+':
			case '-':
				gid = parse_group(tok + 1);
				if (tok[0] == '+') {
					realloc_groups(&supp_groups, 1);
					supp_groups.groups[supp_groups.nb - 1] = gid;
				} else {
					realloc_groups(&supp_rem, 1);
					supp_rem.groups[supp_rem.nb - 1] = gid;
				}
				break;

			default:
				errx(EXIT_FAILURE,
				    "invalid '-s' token '%s' at index %zu",
				    tok, tok - supp_mod_str);
			}
		}

		setcred_flags |= SETCREDF_SUPP_GROUPS;
	}

	/*
	 * We don't need to pass the kernel a normalized representation of the
	 * new supplementary groups set (array sorted and without duplicates),
	 * so we don't do it here unless we need to remove some groups, where it
	 * enables more efficient algorithms (if the number of groups for some
	 * reason grows out of control).
	 */
	if (!group_array_is_empty(&supp_groups) &&
	    !group_array_is_empty(&supp_rem)) {
		sort_uniq_groups(&supp_groups);
		sort_uniq_groups(&supp_rem);
		remove_groups(&supp_groups, &supp_rem);
	}

	if ((setcred_flags & SETCREDF_SUPP_GROUPS) != 0) {
		wcred.sc_supp_groups = supp_groups.groups;
		wcred.sc_supp_groups_nb = supp_groups.nb;
	}

	if (setcred(setcred_flags, &wcred, sizeof(wcred)) != 0)
		err(EXIT_FAILURE, "setcred()");

	/*
	 * We don't bother freeing memory still allocated at this point as we
	 * are about to exec() or exit.
	 */

	if (*argv == NULL) {
		const char *sh = getenv("SHELL");

		if (sh == NULL)
			sh = _PATH_BSHELL;
		execlp(sh, sh, "-i", NULL);
	} else {
		execvp(argv[0], argv);
	}
	err(EXIT_FAILURE, "exec failed");
}
