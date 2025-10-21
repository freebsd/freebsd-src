/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2003-2009 Tim Kientzle
 * All rights reserved.
 */
#include "test.h"

#include "../cpio.h"
#include "lafe_err.h"

#if !defined(_WIN32)
#define ROOT "root"
static const int root_uids[] = { 0 };
static const int root_gids[] = { 0, 1 };
#elif defined(__CYGWIN__)
/* On cygwin, the Administrator user most likely exists (unless
 * it has been renamed or is in a non-English localization), but
 * its primary group membership depends on how the user set up
 * their /etc/passwd. Likely values are 513 (None), 545 (Users),
 * or 544 (Administrators). Just check for one of those...
 * TODO: Handle non-English localizations... e.g. French 'Administrateur'
 *       Use CreateWellKnownSID() and LookupAccountName()?
 */
#define ROOT "Administrator"
static const int root_uids[] = { 500 };
static const int root_gids[] = { 513, 545, 544 };
#endif

#if defined(ROOT)
static int
int_in_list(int i, const int *l, size_t n)
{
	while (n-- > 0)
		if (*l++ == i)
			return (1);
	failure("%d", i);
	return (0);
}

static void
free_cpio_owner(struct cpio_owner *owner) {
	owner->uid = -1;
	owner->gid = -1;
	free(owner->uname);
	free(owner->gname);
}
#endif

DEFINE_TEST(test_owner_parse)
{
#if !defined(ROOT)
	skipping("No uid/gid configuration for this OS");
#else
	struct cpio_owner owner;
	const char *errstr;

	assert(0 == owner_parse(ROOT, &owner, &errstr));
	assert(int_in_list(owner.uid, root_uids,
		sizeof(root_uids)/sizeof(root_uids[0])));
	assertEqualInt(-1, owner.gid);
	free_cpio_owner(&owner);

	assert(0 == owner_parse(ROOT ":", &owner, &errstr));
	assert(int_in_list(owner.uid, root_uids,
		sizeof(root_uids)/sizeof(root_uids[0])));
	assert(int_in_list(owner.gid, root_gids,
		sizeof(root_gids)/sizeof(root_gids[0])));
	free_cpio_owner(&owner);

	assert(0 == owner_parse(ROOT ".", &owner, &errstr));
	assert(int_in_list(owner.uid, root_uids,
		sizeof(root_uids)/sizeof(root_uids[0])));
	assert(int_in_list(owner.gid, root_gids,
		sizeof(root_gids)/sizeof(root_gids[0])));
	free_cpio_owner(&owner);

	assert(0 == owner_parse("111", &owner, &errstr));
	assertEqualInt(111, owner.uid);
	assertEqualInt(-1, owner.gid);
	free_cpio_owner(&owner);

	assert(0 == owner_parse("112:", &owner, &errstr));
	assertEqualInt(112, owner.uid);
	/* Can't assert gid, since we don't know gid for user #112. */
	free_cpio_owner(&owner);

	assert(0 == owner_parse("113.", &owner, &errstr));
	assertEqualInt(113, owner.uid);
	/* Can't assert gid, since we don't know gid for user #113. */
	free_cpio_owner(&owner);

	assert(0 == owner_parse(":114", &owner, &errstr));
	assertEqualInt(-1, owner.uid);
	assertEqualInt(114, owner.gid);
	free_cpio_owner(&owner);

	assert(0 == owner_parse(".115", &owner, &errstr));
	assertEqualInt(-1, owner.uid);
	assertEqualInt(115, owner.gid);
	free_cpio_owner(&owner);

	assert(0 == owner_parse("116:117", &owner, &errstr));
	assertEqualInt(116, owner.uid);
	assertEqualInt(117, owner.gid);
	free_cpio_owner(&owner);

	/*
	 * TODO: Lookup current user/group name, build strings and
	 * use those to verify username/groupname lookups for ordinary
	 * users.
	 */

	errstr = NULL;
	assert(0 != owner_parse(":nonexistentgroup", &owner, &errstr));
	assertEqualString(errstr, "Couldn't lookup group ``nonexistentgroup''");
	free_cpio_owner(&owner);

	errstr = NULL;
	assert(0 != owner_parse(ROOT ":nonexistentgroup", &owner, &errstr));
	assertEqualString(errstr, "Couldn't lookup group ``nonexistentgroup''");
	free_cpio_owner(&owner);

	errstr = NULL;
	assert(0 != owner_parse("nonexistentuser:nonexistentgroup", &owner,
	    &errstr));
	assertEqualString(errstr, "Couldn't lookup user ``nonexistentuser''");
	free_cpio_owner(&owner);
#endif
}
