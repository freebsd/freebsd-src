/*-
 * Copyright (c) 2003-2007 Tim Kientzle
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "test.h"
__FBSDID("$FreeBSD$");

/*
 * Verify reading entries with POSIX.1e ACLs from archives created by star
 *
 * This should work on all systems, regardless of whether local filesystems
 * support ACLs or not.
 */

struct acl_t {
	int type;  /* Type of ACL: "access" or "default" */
	int permset; /* Permissions for this class of users. */
	int tag; /* Owner, User, Owning group, group, other, etc. */
	const char *name; /* Name of user/group, depending on tag. */
};

static struct acl_t acls0[] = {
	{ ARCHIVE_ENTRY_ACL_TYPE_ACCESS, ARCHIVE_ENTRY_ACL_EXECUTE,
	  ARCHIVE_ENTRY_ACL_USER_OBJ, "" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ACCESS, ARCHIVE_ENTRY_ACL_READ,
	  ARCHIVE_ENTRY_ACL_USER, "user77" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ACCESS, ARCHIVE_ENTRY_ACL_READ,
	  ARCHIVE_ENTRY_ACL_GROUP_OBJ, "" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ACCESS, ARCHIVE_ENTRY_ACL_READ,
	  ARCHIVE_ENTRY_ACL_MASK, ""},
	{ ARCHIVE_ENTRY_ACL_TYPE_ACCESS, ARCHIVE_ENTRY_ACL_WRITE,
	  ARCHIVE_ENTRY_ACL_OTHER, "" },
};

static struct acl_t acls1[] = {
	{ ARCHIVE_ENTRY_ACL_TYPE_ACCESS, ARCHIVE_ENTRY_ACL_EXECUTE | ARCHIVE_ENTRY_ACL_READ,
	  ARCHIVE_ENTRY_ACL_USER_OBJ, "" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ACCESS, ARCHIVE_ENTRY_ACL_READ,
	  ARCHIVE_ENTRY_ACL_USER, "user77" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ACCESS, 0,
	  ARCHIVE_ENTRY_ACL_USER, "user78" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ACCESS, ARCHIVE_ENTRY_ACL_READ,
	  ARCHIVE_ENTRY_ACL_GROUP_OBJ, "" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ACCESS, 0007,
	  ARCHIVE_ENTRY_ACL_GROUP, "group78" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ACCESS, 0007,
	  ARCHIVE_ENTRY_ACL_MASK, ""}, 
	{ ARCHIVE_ENTRY_ACL_TYPE_ACCESS, ARCHIVE_ENTRY_ACL_WRITE | ARCHIVE_ENTRY_ACL_EXECUTE,
	  ARCHIVE_ENTRY_ACL_OTHER, "" },
};

static struct acl_t acls2[] = {
	{ ARCHIVE_ENTRY_ACL_TYPE_DEFAULT, ARCHIVE_ENTRY_ACL_EXECUTE,
	  ARCHIVE_ENTRY_ACL_USER_OBJ, "" },
	{ ARCHIVE_ENTRY_ACL_TYPE_DEFAULT, ARCHIVE_ENTRY_ACL_READ,
	  ARCHIVE_ENTRY_ACL_USER, "user77" },
	{ ARCHIVE_ENTRY_ACL_TYPE_DEFAULT, ARCHIVE_ENTRY_ACL_READ,
	  ARCHIVE_ENTRY_ACL_GROUP_OBJ, "" },
	{ ARCHIVE_ENTRY_ACL_TYPE_DEFAULT, ARCHIVE_ENTRY_ACL_EXECUTE,
	  ARCHIVE_ENTRY_ACL_GROUP, "group78" },
	{ ARCHIVE_ENTRY_ACL_TYPE_DEFAULT, ARCHIVE_ENTRY_ACL_READ | ARCHIVE_ENTRY_ACL_EXECUTE,
	  ARCHIVE_ENTRY_ACL_MASK, ""},
	{ ARCHIVE_ENTRY_ACL_TYPE_DEFAULT, ARCHIVE_ENTRY_ACL_WRITE,
	  ARCHIVE_ENTRY_ACL_OTHER, "" },
};

static int
acl_match(struct acl_t *acl, int type, int permset, int tag, const char *name)
{
	if (type != acl->type)
		return (0);
	if (permset != acl->permset)
		return (0);
	if (tag != acl->tag)
		return (0);
	if (tag == ARCHIVE_ENTRY_ACL_USER_OBJ)
		return (1);
	if (tag == ARCHIVE_ENTRY_ACL_GROUP_OBJ)
		return (1);
	if (tag == ARCHIVE_ENTRY_ACL_OTHER)
		return (1);
	if (tag == ARCHIVE_ENTRY_ACL_MASK)
		return (1);
	if (name == NULL)
		return (acl->name == NULL || acl->name[0] == '\0');
	if (acl->name == NULL)
		return (name == NULL || name[0] == '\0');
	return (0 == strcmp(name, acl->name));
}

static void
compare_acls(struct archive_entry *ae, struct acl_t *acls, int n, int mode,
    int want_type)
{
	int *marker = malloc(sizeof(marker[0]) * n);
	int i;
	int r;
	int type, permset, tag, qual;
	int matched;
	const char *name;

	for (i = 0; i < n; i++)
		marker[i] = i;

	while (0 == (r = archive_entry_acl_next(ae, want_type,
			 &type, &permset, &tag, &qual, &name))) {
		for (i = 0, matched = 0; i < n && !matched; i++) {
			if (acl_match(&acls[marker[i]], type, permset,
				tag, name)) {
				/* We found a match; remove it. */
				marker[i] = marker[n - 1];
				n--;
				matched = 1;
			}
		}
		if (tag == ARCHIVE_ENTRY_ACL_USER_OBJ) {
			if (!matched) printf("No match for user_obj perm\n");
			if (want_type == ARCHIVE_ENTRY_ACL_TYPE_ACCESS) {
				failure("USER_OBJ permset (%02o) != user mode (%02o)",
				    permset, 07 & (mode >> 6));
				assert((permset << 6) == (mode & 0700));
			}
		} else if (tag == ARCHIVE_ENTRY_ACL_GROUP_OBJ) {
			if (!matched) printf("No match for group_obj perm\n");
			if (want_type == ARCHIVE_ENTRY_ACL_TYPE_ACCESS) {
				failure("GROUP_OBJ permset %02o != group mode %02o",
				    permset, 07 & (mode >> 3));
				assert((permset << 3) == (mode & 0070));
			}
		} else if (tag == ARCHIVE_ENTRY_ACL_OTHER) {
			if (!matched) printf("No match for other perm\n");
			if (want_type == ARCHIVE_ENTRY_ACL_TYPE_ACCESS) {
				failure("OTHER permset (%02o) != other mode (%02o)",
				    permset, mode & 07);
				assert((permset << 0) == (mode & 0007));
			}
		} else if (tag != ARCHIVE_ENTRY_ACL_MASK) {
			failure("Could not find match for ACL "
			    "(type=%d,permset=%d,tag=%d,name=``%s'')",
			    type, permset, tag, name);
			assert(matched == 1);
		}
	}
	assertEqualInt(ARCHIVE_EOF, r);
	assert((mode_t)(mode & 0777) == (archive_entry_mode(ae) & 0777));
	failure("Could not find match for ACL "
	    "(type=%d,permset=%d,tag=%d,name=``%s'')",
	    acls[marker[0]].type, acls[marker[0]].permset,
	    acls[marker[0]].tag, acls[marker[0]].name);
	assert(n == 0); /* Number of ACLs not matched should == 0 */
	free(marker);
}

DEFINE_TEST(test_compat_star_acl_posix1e)
{
	char name[] = "test_compat_star_acl_posix1e.tar";
	struct archive *a;
	struct archive_entry *ae;

	/* Read archive file */
	assert(NULL != (a = archive_read_new()));
        assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
        assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
        extract_reference_file(name);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_filename(a, name, 10240));

	/* First item has a few ACLs */
	assertA(0 == archive_read_next_header(a, &ae));
	failure("One extended ACL should flag all ACLs to be returned.");
	assertEqualInt(5, archive_entry_acl_reset(ae, ARCHIVE_ENTRY_ACL_TYPE_ACCESS));
	compare_acls(ae, acls0, sizeof(acls0)/sizeof(acls0[0]), 0142, ARCHIVE_ENTRY_ACL_TYPE_ACCESS);
	failure("Basic ACLs should set mode to 0142, not %04o",
	    archive_entry_mode(ae)&0777);
	assert((archive_entry_mode(ae) & 0777) == 0142);

	/* Second item has pretty extensive ACLs */
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualInt(7, archive_entry_acl_reset(ae, ARCHIVE_ENTRY_ACL_TYPE_ACCESS));
	compare_acls(ae, acls1, sizeof(acls1)/sizeof(acls1[0]), 0543, ARCHIVE_ENTRY_ACL_TYPE_ACCESS);
	failure("Basic ACLs should set mode to 0543, not %04o",
	    archive_entry_mode(ae)&0777);
	assert((archive_entry_mode(ae) & 0777) == 0543);

	/* Third item has default ACLs */
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualInt(6, archive_entry_acl_reset(ae, ARCHIVE_ENTRY_ACL_TYPE_DEFAULT));
	compare_acls(ae, acls2, sizeof(acls2)/sizeof(acls2[0]), 0142, ARCHIVE_ENTRY_ACL_TYPE_DEFAULT);
	failure("Basic ACLs should set mode to 0142, not %04o",
	    archive_entry_mode(ae)&0777);
	assert((archive_entry_mode(ae) & 0777) == 0142);

	/* Close the archive. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}
