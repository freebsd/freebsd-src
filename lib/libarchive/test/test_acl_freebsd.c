/*-
 * Copyright (c) 2003-2008 Tim Kientzle
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

#if defined(__FreeBSD__) && __FreeBSD__ > 4
#include <sys/acl.h>

struct myacl_t {
	int type;  /* Type of ACL: "access" or "default" */
	int permset; /* Permissions for this class of users. */
	int tag; /* Owner, User, Owning group, group, other, etc. */
	int qual; /* GID or UID of user/group, depending on tag. */
	const char *name; /* Name of user/group, depending on tag. */
};

static struct myacl_t acls2[] = {
	{ ARCHIVE_ENTRY_ACL_TYPE_ACCESS, ARCHIVE_ENTRY_ACL_EXECUTE | ARCHIVE_ENTRY_ACL_READ,
	  ARCHIVE_ENTRY_ACL_USER_OBJ, -1, "" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ACCESS, ARCHIVE_ENTRY_ACL_READ,
	  ARCHIVE_ENTRY_ACL_USER, 77, "user77" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ACCESS, 0,
	  ARCHIVE_ENTRY_ACL_USER, 78, "user78" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ACCESS, ARCHIVE_ENTRY_ACL_READ,
	  ARCHIVE_ENTRY_ACL_GROUP_OBJ, -1, "" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ACCESS, 0007,
	  ARCHIVE_ENTRY_ACL_GROUP, 78, "group78" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ACCESS,
	  ARCHIVE_ENTRY_ACL_WRITE | ARCHIVE_ENTRY_ACL_EXECUTE,
	  ARCHIVE_ENTRY_ACL_OTHER, -1, "" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ACCESS,
	  ARCHIVE_ENTRY_ACL_WRITE | ARCHIVE_ENTRY_ACL_READ | ARCHIVE_ENTRY_ACL_EXECUTE,
	  ARCHIVE_ENTRY_ACL_MASK, -1, "" },
	{ 0, 0, 0, 0, NULL }
};

static void
set_acls(struct archive_entry *ae, struct myacl_t *acls)
{
	int i;

	archive_entry_acl_clear(ae);
	for (i = 0; acls[i].name != NULL; i++) {
		archive_entry_acl_add_entry(ae,
		    acls[i].type, acls[i].permset, acls[i].tag, acls[i].qual,
		    acls[i].name);
	}
}

static int
acl_match(acl_entry_t aclent, struct myacl_t *myacl)
{
	gid_t g, *gp;
	uid_t u, *up;
	acl_tag_t tag_type;
	acl_permset_t opaque_ps;
	int permset = 0;

	acl_get_tag_type(aclent, &tag_type);

	/* translate the silly opaque permset to a bitmap */
	acl_get_permset(aclent, &opaque_ps);
	if (acl_get_perm_np(opaque_ps, ACL_EXECUTE))
		permset |= ARCHIVE_ENTRY_ACL_EXECUTE;
	if (acl_get_perm_np(opaque_ps, ACL_WRITE))
		permset |= ARCHIVE_ENTRY_ACL_WRITE;
	if (acl_get_perm_np(opaque_ps, ACL_READ))
		permset |= ARCHIVE_ENTRY_ACL_READ;

	if (permset != myacl->permset)
		return (0);

	switch (tag_type) {
	case ACL_USER_OBJ:
		if (myacl->tag != ARCHIVE_ENTRY_ACL_USER_OBJ) return (0);
		break;
	case ACL_USER:
		if (myacl->tag != ARCHIVE_ENTRY_ACL_USER)
			return (0);
		up = acl_get_qualifier(aclent);
		u = *up;
		acl_free(up);
		if ((uid_t)myacl->qual != u)
			return (0);
		break;
	case ACL_GROUP_OBJ:
		if (myacl->tag != ARCHIVE_ENTRY_ACL_GROUP_OBJ) return (0);
		break;
	case ACL_GROUP:
		if (myacl->tag != ARCHIVE_ENTRY_ACL_GROUP)
			return (0);
		gp = acl_get_qualifier(aclent);
		g = *gp;
		acl_free(gp);
		if ((gid_t)myacl->qual != g)
			return (0);
		break;
	case ACL_MASK:
		if (myacl->tag != ARCHIVE_ENTRY_ACL_MASK) return (0);
		break;
	case ACL_OTHER:
		if (myacl->tag != ARCHIVE_ENTRY_ACL_OTHER) return (0);
		break;
	}
	return (1);
}

static void
compare_acls(acl_t acl, struct myacl_t *myacls)
{
	int *marker;
	int entry_id = ACL_FIRST_ENTRY;
	int matched;
	int i, n;
	acl_entry_t acl_entry;

	/* Count ACL entries in myacls array and allocate an indirect array. */
	for (n = 0; myacls[n].name != NULL; ++n)
		continue;
	marker = malloc(sizeof(marker[0]) * n);
	for (i = 0; i < n; i++)
		marker[i] = i;

	/*
	 * Iterate over acls in system acl object, try to match each
	 * one with an item in the myacls array.
	 */
	while (1 == acl_get_entry(acl, entry_id, &acl_entry)) {
		/* After the first time... */
		entry_id = ACL_NEXT_ENTRY;

		/* Search for a matching entry (tag and qualifier) */
		for (i = 0, matched = 0; i < n && !matched; i++) {
			if (acl_match(acl_entry, &myacls[marker[i]])) {
				/* We found a match; remove it. */
				marker[i] = marker[n - 1];
				n--;
				matched = 1;
			}
		}

		/* TODO: Print out more details in this case. */
		failure("ACL entry on file that shouldn't be there");
		assert(matched == 1);
	}

	/* Dump entries in the myacls array that weren't in the system acl. */
	for (i = 0; i < n; ++i) {
		failure(" ACL entry missing from file: "
		    "type=%d,permset=%d,tag=%d,qual=%d,name=``%s''\n",
		    myacls[marker[i]].type, myacls[marker[i]].permset,
		    myacls[marker[i]].tag, myacls[marker[i]].qual,
		    myacls[marker[i]].name);
		assert(0); /* Record this as a failure. */
	}
	free(marker);
}

#endif


/*
 * Verify ACL restore-to-disk.  This test is FreeBSD-specific.
 */

DEFINE_TEST(test_acl_freebsd)
{
#if !defined(__FreeBSD__)
	skipping("FreeBSD-specific ACL restore test");
#elif __FreeBSD__ < 5
	skipping("ACL restore supported only on FreeBSD 5.0 and later");
#else
	struct stat st;
	struct archive *a;
	struct archive_entry *ae;
	int n, fd;
	acl_t acl;

	/*
	 * First, do a quick manual set/read of ACL data to
	 * verify that the local filesystem does support ACLs.
	 * If it doesn't, we'll simply skip the remaining tests.
	 */
	acl = acl_from_text("u::rwx,u:1:rw,g::rwx,g:15:rx,o::rwx,m::rwx");
	assert((void *)acl != NULL);
	/* Create a test file and try to set an ACL on it. */
	fd = open("pretest", O_WRONLY | O_CREAT | O_EXCL, 0777);
	failure("Could not create test file?!");
	if (!assert(fd >= 0)) {
		acl_free(acl);
		return;
	}

	n = acl_set_fd(fd, acl);
	acl_free(acl);
	if (n != 0 && errno == EOPNOTSUPP) {
		close(fd);
		skipping("ACL tests require that ACL support be enabled on the filesystem");
		return;
	}
	if (n != 0 && errno == EINVAL) {
		close(fd);
		skipping("POSIX.1e ACL tests require that POSIX.1e ACL support be enabled on the filesystem");
		return;
	}
	failure("acl_set_fd(): errno = %d (%s)",
	    errno, strerror(errno));
	assertEqualInt(0, n);
	close(fd);

	/* Create a write-to-disk object. */
	assert(NULL != (a = archive_write_disk_new()));
	archive_write_disk_set_options(a,
	    ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM | ARCHIVE_EXTRACT_ACL);

	/* Populate an archive entry with some metadata, including ACL info */
	ae = archive_entry_new();
	assert(ae != NULL);
	archive_entry_set_pathname(ae, "test0");
	archive_entry_set_mtime(ae, 123456, 7890);
	archive_entry_set_size(ae, 0);
	set_acls(ae, acls2);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	archive_entry_free(ae);

	/* Close the archive. */
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_finish(a));

	/* Verify the data on disk. */
	assertEqualInt(0, stat("test0", &st));
	assertEqualInt(st.st_mtime, 123456);
	acl = acl_get_file("test0", ACL_TYPE_ACCESS);
	assert(acl != (acl_t)NULL);
	compare_acls(acl, acls2);
	acl_free(acl);
#endif
}
