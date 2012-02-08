/*-
 * Copyright (c) 2003-2010 Tim Kientzle
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
 * Exercise the system-independent portion of the ACL support.
 * Check that archive_entry objects can save and restore NFS4 ACL data.
 *
 * This should work on all systems, regardless of whether local
 * filesystems support ACLs or not.
 */

struct acl_t {
	int type;  /* Type of entry: "allow" or "deny" */
	int permset; /* Permissions for this class of users. */
	int tag; /* Owner, User, Owning group, group, everyone, etc. */
	int qual; /* GID or UID of user/group, depending on tag. */
	const char *name; /* Name of user/group, depending on tag. */
};

static struct acl_t acls1[] = {
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_EXECUTE,
	  ARCHIVE_ENTRY_ACL_USER_OBJ, -1, "" },
	{ ARCHIVE_ENTRY_ACL_TYPE_DENY, ARCHIVE_ENTRY_ACL_READ_DATA,
	  ARCHIVE_ENTRY_ACL_USER, 77, "user77" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_READ_DATA,
	  ARCHIVE_ENTRY_ACL_GROUP_OBJ, -1, "" },
	{ ARCHIVE_ENTRY_ACL_TYPE_DENY, ARCHIVE_ENTRY_ACL_WRITE_DATA,
	  ARCHIVE_ENTRY_ACL_EVERYONE, -1, "" },
};

static struct acl_t acls2[] = {
	/* An entry for each type. */
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, 0,
	  ARCHIVE_ENTRY_ACL_USER, 108, "user108" },
	{ ARCHIVE_ENTRY_ACL_TYPE_DENY, 0,
	  ARCHIVE_ENTRY_ACL_USER, 109, "user109" },
	{ ARCHIVE_ENTRY_ACL_TYPE_AUDIT, 0,
	  ARCHIVE_ENTRY_ACL_USER, 110, "user110" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALARM, 0,
	  ARCHIVE_ENTRY_ACL_USER, 111, "user111" },

	/* An entry for each permission. */
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_EXECUTE,
	  ARCHIVE_ENTRY_ACL_USER, 112, "user112" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_READ_DATA,
	  ARCHIVE_ENTRY_ACL_USER, 113, "user113" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_LIST_DIRECTORY,
	  ARCHIVE_ENTRY_ACL_USER, 114, "user114" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_WRITE_DATA,
	  ARCHIVE_ENTRY_ACL_USER, 115, "user115" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_ADD_FILE,
	  ARCHIVE_ENTRY_ACL_USER, 116, "user116" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_APPEND_DATA,
	  ARCHIVE_ENTRY_ACL_USER, 117, "user117" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_ADD_SUBDIRECTORY,
	  ARCHIVE_ENTRY_ACL_USER, 118, "user118" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_READ_NAMED_ATTRS,
	  ARCHIVE_ENTRY_ACL_USER, 119, "user119" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_WRITE_NAMED_ATTRS,
	  ARCHIVE_ENTRY_ACL_USER, 120, "user120" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_DELETE_CHILD,
	  ARCHIVE_ENTRY_ACL_USER, 121, "user121" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_READ_ATTRIBUTES,
	  ARCHIVE_ENTRY_ACL_USER, 122, "user122" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_WRITE_ATTRIBUTES,
	  ARCHIVE_ENTRY_ACL_USER, 123, "user123" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_DELETE,
	  ARCHIVE_ENTRY_ACL_USER, 124, "user124" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_READ_ACL,
	  ARCHIVE_ENTRY_ACL_USER, 125, "user125" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_WRITE_ACL,
	  ARCHIVE_ENTRY_ACL_USER, 126, "user126" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_WRITE_OWNER,
	  ARCHIVE_ENTRY_ACL_USER, 127, "user127" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_SYNCHRONIZE,
	  ARCHIVE_ENTRY_ACL_USER, 128, "user128" },

	/* One entry with each inheritance value. */
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW,
	  ARCHIVE_ENTRY_ACL_READ_DATA | ARCHIVE_ENTRY_ACL_ENTRY_FILE_INHERIT,
	  ARCHIVE_ENTRY_ACL_USER, 129, "user129" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW,
	  ARCHIVE_ENTRY_ACL_READ_DATA | ARCHIVE_ENTRY_ACL_ENTRY_DIRECTORY_INHERIT,
	  ARCHIVE_ENTRY_ACL_USER, 130, "user130" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW,
	  ARCHIVE_ENTRY_ACL_READ_DATA | ARCHIVE_ENTRY_ACL_ENTRY_NO_PROPAGATE_INHERIT,
	  ARCHIVE_ENTRY_ACL_USER, 131, "user131" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW,
	  ARCHIVE_ENTRY_ACL_READ_DATA | ARCHIVE_ENTRY_ACL_ENTRY_INHERIT_ONLY,
	  ARCHIVE_ENTRY_ACL_USER, 132, "user132" },
	{ ARCHIVE_ENTRY_ACL_TYPE_AUDIT,
	  ARCHIVE_ENTRY_ACL_READ_DATA | ARCHIVE_ENTRY_ACL_ENTRY_SUCCESSFUL_ACCESS,
	  ARCHIVE_ENTRY_ACL_USER, 133, "user133" },
	{ ARCHIVE_ENTRY_ACL_TYPE_AUDIT,
	  ARCHIVE_ENTRY_ACL_READ_DATA | ARCHIVE_ENTRY_ACL_ENTRY_FAILED_ACCESS,
	  ARCHIVE_ENTRY_ACL_USER, 134, "user134" },

	/* One entry for each qualifier. */
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_EXECUTE,
	  ARCHIVE_ENTRY_ACL_USER, 135, "user135" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_EXECUTE,
	  ARCHIVE_ENTRY_ACL_USER_OBJ, -1, "" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_EXECUTE,
	  ARCHIVE_ENTRY_ACL_GROUP, 136, "group136" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_EXECUTE,
	  ARCHIVE_ENTRY_ACL_GROUP_OBJ, -1, "" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_EXECUTE,
	  ARCHIVE_ENTRY_ACL_EVERYONE, -1, "" },
};

/*
 * Entries that should be rejected when we attempt to set them
 * on an ACL that already has NFS4 entries.
 */
static struct acl_t acls_bad[] = {
	/* POSIX.1e ACL types */
	{ ARCHIVE_ENTRY_ACL_TYPE_ACCESS, ARCHIVE_ENTRY_ACL_EXECUTE,
	  ARCHIVE_ENTRY_ACL_USER, 78, "" },
	{ ARCHIVE_ENTRY_ACL_TYPE_DEFAULT, ARCHIVE_ENTRY_ACL_EXECUTE,
	  ARCHIVE_ENTRY_ACL_USER, 78, "" },

	/* POSIX.1e tags */
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_EXECUTE,
	  ARCHIVE_ENTRY_ACL_OTHER, -1, "" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_EXECUTE,
	  ARCHIVE_ENTRY_ACL_MASK, -1, "" },

	/* POSIX.1e permissions */
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_READ,
	  ARCHIVE_ENTRY_ACL_EVERYONE, -1, "" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_WRITE,
	  ARCHIVE_ENTRY_ACL_EVERYONE, -1, "" },
};

static void
set_acls(struct archive_entry *ae, struct acl_t *acls, int n)
{
	int i;

	archive_entry_acl_clear(ae);
	for (i = 0; i < n; i++) {
		failure("type=%d, permset=%d, tag=%d, qual=%d name=%s",
		    acls[i].type, acls[i].permset, acls[i].tag,
		    acls[i].qual, acls[i].name);
		assertEqualInt(ARCHIVE_OK,
		    archive_entry_acl_add_entry(ae,
			acls[i].type, acls[i].permset, acls[i].tag,
			acls[i].qual, acls[i].name));
	}
}

static int
acl_match(struct acl_t *acl, int type, int permset, int tag, int qual, const char *name)
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
	if (tag == ARCHIVE_ENTRY_ACL_EVERYONE)
		return (1);
	if (qual != acl->qual)
		return (0);
	if (name == NULL) {
		if (acl->name == NULL || acl->name[0] == '\0')
			return (1);
	}
	if (acl->name == NULL) {
		if (name[0] == '\0')
			return (1);
	}
	return (0 == strcmp(name, acl->name));
}

static void
compare_acls(struct archive_entry *ae, struct acl_t *acls, int n)
{
	int *marker = malloc(sizeof(marker[0]) * n);
	int i;
	int r;
	int type, permset, tag, qual;
	int matched;
	const char *name;

	for (i = 0; i < n; i++)
		marker[i] = i;

	while (0 == (r = archive_entry_acl_next(ae,
			 ARCHIVE_ENTRY_ACL_TYPE_NFS4,
			 &type, &permset, &tag, &qual, &name))) {
		for (i = 0, matched = 0; i < n && !matched; i++) {
			if (acl_match(&acls[marker[i]], type, permset,
				tag, qual, name)) {
				/* We found a match; remove it. */
				marker[i] = marker[n - 1];
				n--;
				matched = 1;
			}
		}
		failure("Could not find match for ACL "
		    "(type=%d,permset=%d,tag=%d,qual=%d,name=``%s'')",
		    type, permset, tag, qual, name);
		assertEqualInt(1, matched);
	}
	assertEqualInt(ARCHIVE_EOF, r);
	failure("Could not find match for ACL "
	    "(type=%d,permset=%d,tag=%d,qual=%d,name=``%s'')",
	    acls[marker[0]].type, acls[marker[0]].permset,
	    acls[marker[0]].tag, acls[marker[0]].qual, acls[marker[0]].name);
	assertEqualInt(0, n); /* Number of ACLs not matched should == 0 */
	free(marker);
}

DEFINE_TEST(test_acl_nfs4)
{
	struct archive_entry *ae;
	int i;

	/* Create a simple archive_entry. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_pathname(ae, "file");
        archive_entry_set_mode(ae, S_IFREG | 0777);

	/* Store and read back some basic ACL entries. */
	set_acls(ae, acls1, sizeof(acls1)/sizeof(acls1[0]));
	assertEqualInt(4,
	    archive_entry_acl_reset(ae, ARCHIVE_ENTRY_ACL_TYPE_NFS4));
	compare_acls(ae, acls1, sizeof(acls1)/sizeof(acls1[0]));

	/* A more extensive set of ACLs. */
	set_acls(ae, acls2, sizeof(acls2)/sizeof(acls2[0]));
	assertEqualInt(32,
	    archive_entry_acl_reset(ae, ARCHIVE_ENTRY_ACL_TYPE_NFS4));
	compare_acls(ae, acls2, sizeof(acls2)/sizeof(acls2[0]));

	/*
	 * Check that clearing ACLs gets rid of them all by repeating
	 * the first test.
	 */
	set_acls(ae, acls1, sizeof(acls1)/sizeof(acls1[0]));
	failure("Basic ACLs shouldn't be stored as extended ACLs");
	assertEqualInt(4,
	    archive_entry_acl_reset(ae, ARCHIVE_ENTRY_ACL_TYPE_NFS4));

	/*
	 * Different types of malformed ACL entries that should
	 * fail when added to existing NFS4 ACLs.
	 */
	set_acls(ae, acls2, sizeof(acls2)/sizeof(acls2[0]));
	for (i = 0; i < sizeof(acls_bad)/sizeof(acls_bad[0]); ++i) {
		struct acl_t *p = &acls_bad[i];
		failure("Malformed ACL test #%d", i);
		assertEqualInt(ARCHIVE_FAILED,
		    archive_entry_acl_add_entry(ae,
			p->type, p->permset, p->tag, p->qual, p->name));
		failure("Malformed ACL test #%d", i);
		assertEqualInt(32,
		    archive_entry_acl_reset(ae,
			ARCHIVE_ENTRY_ACL_TYPE_NFS4));
	}
	archive_entry_free(ae);
}
