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

#if defined(__FreeBSD__) && __FreeBSD__ >= 8
#define _ACL_PRIVATE
#include <sys/acl.h>

struct myacl_t {
	int type;
	int permset;
	int tag;
	int qual; /* GID or UID of user/group, depending on tag. */
	const char *name; /* Name of user/group, depending on tag. */
};

static struct myacl_t acls_reg[] = {
	/* For this test, we need the file owner to be able to read and write the ACL. */
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW,
	  ARCHIVE_ENTRY_ACL_READ_DATA | ARCHIVE_ENTRY_ACL_READ_ACL | ARCHIVE_ENTRY_ACL_WRITE_ACL | ARCHIVE_ENTRY_ACL_READ_NAMED_ATTRS | ARCHIVE_ENTRY_ACL_READ_ATTRIBUTES,
	  ARCHIVE_ENTRY_ACL_USER_OBJ, -1, ""},

	/* An entry for each type. */
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_EXECUTE,
	  ARCHIVE_ENTRY_ACL_USER, 108, "user108" },
	{ ARCHIVE_ENTRY_ACL_TYPE_DENY, ARCHIVE_ENTRY_ACL_EXECUTE,
	  ARCHIVE_ENTRY_ACL_USER, 109, "user109" },

	/* An entry for each permission. */
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_EXECUTE,
	  ARCHIVE_ENTRY_ACL_USER, 112, "user112" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_READ_DATA,
	  ARCHIVE_ENTRY_ACL_USER, 113, "user113" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_WRITE_DATA,
	  ARCHIVE_ENTRY_ACL_USER, 115, "user115" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_APPEND_DATA,
	  ARCHIVE_ENTRY_ACL_USER, 117, "user117" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_READ_NAMED_ATTRS,
	  ARCHIVE_ENTRY_ACL_USER, 119, "user119" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_WRITE_NAMED_ATTRS,
	  ARCHIVE_ENTRY_ACL_USER, 120, "user120" },
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

	/* One entry for each qualifier. */
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_EXECUTE,
	  ARCHIVE_ENTRY_ACL_USER, 135, "user135" },
//	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_EXECUTE,
//	  ARCHIVE_ENTRY_ACL_USER_OBJ, -1, "" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_EXECUTE,
	  ARCHIVE_ENTRY_ACL_GROUP, 136, "group136" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_EXECUTE,
	  ARCHIVE_ENTRY_ACL_GROUP_OBJ, -1, "" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_EXECUTE,
	  ARCHIVE_ENTRY_ACL_EVERYONE, -1, "" }
};


static struct myacl_t acls_dir[] = {
	/* For this test, we need to be able to read and write the ACL. */
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_READ_ACL,
	  ARCHIVE_ENTRY_ACL_USER_OBJ, -1, ""},

	/* An entry for each type. */
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_LIST_DIRECTORY,
	  ARCHIVE_ENTRY_ACL_USER, 101, "user101" },
	{ ARCHIVE_ENTRY_ACL_TYPE_DENY, ARCHIVE_ENTRY_ACL_LIST_DIRECTORY,
	  ARCHIVE_ENTRY_ACL_USER, 102, "user102" },

	/* An entry for each permission. */
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_LIST_DIRECTORY,
	  ARCHIVE_ENTRY_ACL_USER, 201, "user201" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_ADD_FILE,
	  ARCHIVE_ENTRY_ACL_USER, 202, "user202" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_ADD_SUBDIRECTORY,
	  ARCHIVE_ENTRY_ACL_USER, 203, "user203" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_READ_NAMED_ATTRS,
	  ARCHIVE_ENTRY_ACL_USER, 204, "user204" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_WRITE_NAMED_ATTRS,
	  ARCHIVE_ENTRY_ACL_USER, 205, "user205" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_DELETE_CHILD,
	  ARCHIVE_ENTRY_ACL_USER, 206, "user206" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_READ_ATTRIBUTES,
	  ARCHIVE_ENTRY_ACL_USER, 207, "user207" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_WRITE_ATTRIBUTES,
	  ARCHIVE_ENTRY_ACL_USER, 208, "user208" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_DELETE,
	  ARCHIVE_ENTRY_ACL_USER, 209, "user209" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_READ_ACL,
	  ARCHIVE_ENTRY_ACL_USER, 210, "user210" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_WRITE_ACL,
	  ARCHIVE_ENTRY_ACL_USER, 211, "user211" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_WRITE_OWNER,
	  ARCHIVE_ENTRY_ACL_USER, 212, "user212" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_SYNCHRONIZE,
	  ARCHIVE_ENTRY_ACL_USER, 213, "user213" },

	/* One entry with each inheritance value. */
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW,
	  ARCHIVE_ENTRY_ACL_READ_DATA | ARCHIVE_ENTRY_ACL_ENTRY_FILE_INHERIT,
	  ARCHIVE_ENTRY_ACL_USER, 301, "user301" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW,
	  ARCHIVE_ENTRY_ACL_READ_DATA | ARCHIVE_ENTRY_ACL_ENTRY_DIRECTORY_INHERIT,
	  ARCHIVE_ENTRY_ACL_USER, 302, "user302" },
#if 0
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW,
	  ARCHIVE_ENTRY_ACL_READ_DATA | ARCHIVE_ENTRY_ACL_ENTRY_NO_PROPAGATE_INHERIT,
	  ARCHIVE_ENTRY_ACL_USER, 303, "user303" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW,
	  ARCHIVE_ENTRY_ACL_READ_DATA | ARCHIVE_ENTRY_ACL_ENTRY_INHERIT_ONLY,
	  ARCHIVE_ENTRY_ACL_USER, 304, "user304" },
#endif

#if 0
	/* FreeBSD does not support audit entries. */
	{ ARCHIVE_ENTRY_ACL_TYPE_AUDIT,
	  ARCHIVE_ENTRY_ACL_READ_DATA | ARCHIVE_ENTRY_ACL_ENTRY_SUCCESSFUL_ACCESS,
	  ARCHIVE_ENTRY_ACL_USER, 401, "user401" },
	{ ARCHIVE_ENTRY_ACL_TYPE_AUDIT,
	  ARCHIVE_ENTRY_ACL_READ_DATA | ARCHIVE_ENTRY_ACL_ENTRY_FAILED_ACCESS,
	  ARCHIVE_ENTRY_ACL_USER, 402, "user402" },
#endif

	/* One entry for each qualifier. */
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_LIST_DIRECTORY,
	  ARCHIVE_ENTRY_ACL_USER, 501, "user501" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_LIST_DIRECTORY,
	  ARCHIVE_ENTRY_ACL_GROUP, 502, "group502" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_LIST_DIRECTORY,
	  ARCHIVE_ENTRY_ACL_GROUP_OBJ, -1, "" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_LIST_DIRECTORY,
	  ARCHIVE_ENTRY_ACL_EVERYONE, -1, "" }
};

static void
set_acls(struct archive_entry *ae, struct myacl_t *acls, int start, int end)
{
	int i;

	archive_entry_acl_clear(ae);
	if (start > 0) {
		assertEqualInt(ARCHIVE_OK,
			archive_entry_acl_add_entry(ae,
			    acls[0].type, acls[0].permset, acls[0].tag,
			    acls[0].qual, acls[0].name));
	}
	for (i = start; i < end; i++) {
		assertEqualInt(ARCHIVE_OK,
		    archive_entry_acl_add_entry(ae,
			acls[i].type, acls[i].permset, acls[i].tag,
			acls[i].qual, acls[i].name));
	}
}

static int
acl_permset_to_bitmap(acl_permset_t opaque_ps)
{
	static struct { int machine; int portable; } perms[] = {
		{ACL_EXECUTE, ARCHIVE_ENTRY_ACL_EXECUTE},
		{ACL_WRITE, ARCHIVE_ENTRY_ACL_WRITE},
		{ACL_READ, ARCHIVE_ENTRY_ACL_READ},
		{ACL_READ_DATA, ARCHIVE_ENTRY_ACL_READ_DATA},
		{ACL_LIST_DIRECTORY, ARCHIVE_ENTRY_ACL_LIST_DIRECTORY},
		{ACL_WRITE_DATA, ARCHIVE_ENTRY_ACL_WRITE_DATA},
		{ACL_ADD_FILE, ARCHIVE_ENTRY_ACL_ADD_FILE},
		{ACL_APPEND_DATA, ARCHIVE_ENTRY_ACL_APPEND_DATA},
		{ACL_ADD_SUBDIRECTORY, ARCHIVE_ENTRY_ACL_ADD_SUBDIRECTORY},
		{ACL_READ_NAMED_ATTRS, ARCHIVE_ENTRY_ACL_READ_NAMED_ATTRS},
		{ACL_WRITE_NAMED_ATTRS, ARCHIVE_ENTRY_ACL_WRITE_NAMED_ATTRS},
		{ACL_DELETE_CHILD, ARCHIVE_ENTRY_ACL_DELETE_CHILD},
		{ACL_READ_ATTRIBUTES, ARCHIVE_ENTRY_ACL_READ_ATTRIBUTES},
		{ACL_WRITE_ATTRIBUTES, ARCHIVE_ENTRY_ACL_WRITE_ATTRIBUTES},
		{ACL_DELETE, ARCHIVE_ENTRY_ACL_DELETE},
		{ACL_READ_ACL, ARCHIVE_ENTRY_ACL_READ_ACL},
		{ACL_WRITE_ACL, ARCHIVE_ENTRY_ACL_WRITE_ACL},
		{ACL_WRITE_OWNER, ARCHIVE_ENTRY_ACL_WRITE_OWNER},
		{ACL_SYNCHRONIZE, ARCHIVE_ENTRY_ACL_SYNCHRONIZE}
	};
	int i, permset = 0;

	for (i = 0; i < (int)(sizeof(perms)/sizeof(perms[0])); ++i)
		if (acl_get_perm_np(opaque_ps, perms[i].machine))
			permset |= perms[i].portable;
	return permset;
}

static int
acl_flagset_to_bitmap(acl_flagset_t opaque_fs)
{
	static struct { int machine; int portable; } flags[] = {
		{ACL_ENTRY_FILE_INHERIT, ARCHIVE_ENTRY_ACL_ENTRY_FILE_INHERIT},
		{ACL_ENTRY_DIRECTORY_INHERIT, ARCHIVE_ENTRY_ACL_ENTRY_DIRECTORY_INHERIT},
		{ACL_ENTRY_NO_PROPAGATE_INHERIT, ARCHIVE_ENTRY_ACL_ENTRY_NO_PROPAGATE_INHERIT},
		{ACL_ENTRY_INHERIT_ONLY, ARCHIVE_ENTRY_ACL_ENTRY_INHERIT_ONLY},
	};
	int i, flagset = 0;

	for (i = 0; i < (int)(sizeof(flags)/sizeof(flags[0])); ++i)
		if (acl_get_flag_np(opaque_fs, flags[i].machine))
			flagset |= flags[i].portable;
	return flagset;
}

static int
acl_match(acl_entry_t aclent, struct myacl_t *myacl)
{
	gid_t g, *gp;
	uid_t u, *up;
	acl_tag_t tag_type;
	acl_permset_t opaque_ps;
	acl_flagset_t opaque_fs;
	int perms;

	acl_get_tag_type(aclent, &tag_type);

	/* translate the silly opaque permset to a bitmap */
	acl_get_permset(aclent, &opaque_ps);
	acl_get_flagset_np(aclent, &opaque_fs);
	perms = acl_permset_to_bitmap(opaque_ps) | acl_flagset_to_bitmap(opaque_fs);
	if (perms != myacl->permset)
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
	case ACL_EVERYONE:
		if (myacl->tag != ARCHIVE_ENTRY_ACL_EVERYONE) return (0);
		break;
	}
	return (1);
}

static void
compare_acls(acl_t acl, struct myacl_t *myacls, const char *filename, int start, int end)
{
	int *marker;
	int entry_id = ACL_FIRST_ENTRY;
	int matched;
	int i, n;
	acl_entry_t acl_entry;

	n = end - start;
	marker = malloc(sizeof(marker[0]) * (n + 1));
	for (i = 0; i < n; i++)
		marker[i] = i + start;
	/* Always include the first ACE. */
	if (start > 0) {
	  marker[n] = 0;
	  ++n;
	}

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

		failure("ACL entry on file %s that shouldn't be there", filename);
		assert(matched == 1);
	}

	/* Dump entries in the myacls array that weren't in the system acl. */
	for (i = 0; i < n; ++i) {
		failure(" ACL entry %d missing from %s: "
		    "type=%d,permset=%x,tag=%d,qual=%d,name=``%s''\n",
		    marker[i], filename,
		    myacls[marker[i]].type, myacls[marker[i]].permset,
		    myacls[marker[i]].tag, myacls[marker[i]].qual,
		    myacls[marker[i]].name);
		assert(0); /* Record this as a failure. */
	}
	free(marker);
}

static void
compare_entry_acls(struct archive_entry *ae, struct myacl_t *myacls, const char *filename, int start, int end)
{
	int *marker;
	int matched;
	int i, n;
	int type, permset, tag, qual;
	const char *name;

	/* Count ACL entries in myacls array and allocate an indirect array. */
	n = end - start;
	marker = malloc(sizeof(marker[0]) * (n + 1));
	for (i = 0; i < n; i++)
		marker[i] = i + start;
	/* Always include the first ACE. */
	if (start > 0) {
	  marker[n] = 0;
	  ++n;
	}

	/*
	 * Iterate over acls in entry, try to match each
	 * one with an item in the myacls array.
	 */
	assertEqualInt(n, archive_entry_acl_reset(ae, ARCHIVE_ENTRY_ACL_TYPE_NFS4));
	while (ARCHIVE_OK == archive_entry_acl_next(ae,
	    ARCHIVE_ENTRY_ACL_TYPE_NFS4, &type, &permset, &tag, &qual, &name)) {

		/* Search for a matching entry (tag and qualifier) */
		for (i = 0, matched = 0; i < n && !matched; i++) {
			if (tag == myacls[marker[i]].tag
			    && qual == myacls[marker[i]].qual
			    && permset == myacls[marker[i]].permset
			    && type == myacls[marker[i]].type) {
				/* We found a match; remove it. */
				marker[i] = marker[n - 1];
				n--;
				matched = 1;
			}
		}

		failure("ACL entry on file that shouldn't be there: "
			"type=%d,permset=%x,tag=%d,qual=%d",
			type,permset,tag,qual);
		assert(matched == 1);
	}

	/* Dump entries in the myacls array that weren't in the system acl. */
	for (i = 0; i < n; ++i) {
		failure(" ACL entry %d missing from %s: "
		    "type=%d,permset=%x,tag=%d,qual=%d,name=``%s''\n",
		    marker[i], filename,
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

DEFINE_TEST(test_acl_freebsd_nfs4)
{
#if !defined(__FreeBSD__)
	skipping("FreeBSD-specific NFS4 ACL restore test");
#elif __FreeBSD__ < 8
	skipping("NFS4 ACLs supported only on FreeBSD 8.0 and later");
#else
	char buff[64];
	struct stat st;
	struct archive *a;
	struct archive_entry *ae;
	int i, n;
	acl_t acl;

	/*
	 * First, do a quick manual set/read of ACL data to
	 * verify that the local filesystem does support ACLs.
	 * If it doesn't, we'll simply skip the remaining tests.
	 */
	acl = acl_from_text("owner@:rwxp::allow,group@:rwp:f:allow");
	assert((void *)acl != NULL);
	/* Create a test dir and try to set an ACL on it. */
	if (!assertMakeDir("pretest", 0755)) {
		acl_free(acl);
		return;
	}

	n = acl_set_file("pretest", ACL_TYPE_NFS4, acl);
	acl_free(acl);
	if (n != 0 && errno == EOPNOTSUPP) {
		skipping("NFS4 ACL tests require that NFS4 ACLs"
		    " be enabled on the filesystem");
		return;
	}
	if (n != 0 && errno == EINVAL) {
		skipping("This filesystem does not support NFS4 ACLs");
		return;
	}
	failure("acl_set_file(): errno = %d (%s)",
	    errno, strerror(errno));
	assertEqualInt(0, n);

	/* Create a write-to-disk object. */
	assert(NULL != (a = archive_write_disk_new()));
	archive_write_disk_set_options(a,
	    ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM | ARCHIVE_EXTRACT_ACL);

	/* Populate an archive entry with some metadata, including ACL info */
	ae = archive_entry_new();
	assert(ae != NULL);
	archive_entry_set_pathname(ae, "testall");
	archive_entry_set_filetype(ae, AE_IFREG);
	archive_entry_set_perm(ae, 0654);
	archive_entry_set_mtime(ae, 123456, 7890);
	archive_entry_set_size(ae, 0);
	set_acls(ae, acls_reg, 0, (int)(sizeof(acls_reg)/sizeof(acls_reg[0])));

	/* Write the entry to disk, including ACLs. */
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));

	/* Likewise for a dir. */
	archive_entry_set_pathname(ae, "dirall");
	archive_entry_set_filetype(ae, AE_IFDIR);
	archive_entry_set_perm(ae, 0654);
	archive_entry_set_mtime(ae, 123456, 7890);
	set_acls(ae, acls_dir, 0, (int)(sizeof(acls_dir)/sizeof(acls_dir[0])));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));

	for (i = 0; i < (int)(sizeof(acls_dir)/sizeof(acls_dir[0])); ++i) {
	  sprintf(buff, "dir%d", i);
	  archive_entry_set_pathname(ae, buff);
	  archive_entry_set_filetype(ae, AE_IFDIR);
	  archive_entry_set_perm(ae, 0654);
	  archive_entry_set_mtime(ae, 123456 + i, 7891 + i);
	  set_acls(ae, acls_dir, i, i + 1);
	  assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	}

	archive_entry_free(ae);

	/* Close the archive. */
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	/* Verify the data on disk. */
	assertEqualInt(0, stat("testall", &st));
	assertEqualInt(st.st_mtime, 123456);
	acl = acl_get_file("testall", ACL_TYPE_NFS4);
	assert(acl != (acl_t)NULL);
	compare_acls(acl, acls_reg, "testall", 0, (int)(sizeof(acls_reg)/sizeof(acls_reg[0])));
	acl_free(acl);

	/* Verify single-permission dirs on disk. */
	for (i = 0; i < (int)(sizeof(acls_dir)/sizeof(acls_dir[0])); ++i) {
	  sprintf(buff, "dir%d", i);
	  assertEqualInt(0, stat(buff, &st));
	  assertEqualInt(st.st_mtime, 123456 + i);
	  acl = acl_get_file(buff, ACL_TYPE_NFS4);
	  assert(acl != (acl_t)NULL);
	  compare_acls(acl, acls_dir, buff, i, i + 1);
	  acl_free(acl);
	}

	/* Verify "dirall" on disk. */
	assertEqualInt(0, stat("dirall", &st));
	assertEqualInt(st.st_mtime, 123456);
	acl = acl_get_file("dirall", ACL_TYPE_NFS4);
	assert(acl != (acl_t)NULL);
	compare_acls(acl, acls_dir, "dirall", 0,  (int)(sizeof(acls_dir)/sizeof(acls_dir[0])));
	acl_free(acl);

	/* Read and compare ACL via archive_read_disk */
	a = archive_read_disk_new();
	assert(a != NULL);
	ae = archive_entry_new();
	assert(ae != NULL);
	archive_entry_set_pathname(ae, "testall");
	assertEqualInt(ARCHIVE_OK,
		       archive_read_disk_entry_from_file(a, ae, -1, NULL));
	compare_entry_acls(ae, acls_reg, "testall", 0, (int)(sizeof(acls_reg)/sizeof(acls_reg[0])));
	archive_entry_free(ae);
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	/* Read and compare ACL via archive_read_disk */
	a = archive_read_disk_new();
	assert(a != NULL);
	ae = archive_entry_new();
	assert(ae != NULL);
	archive_entry_set_pathname(ae, "dirall");
	assertEqualInt(ARCHIVE_OK,
		       archive_read_disk_entry_from_file(a, ae, -1, NULL));
	compare_entry_acls(ae, acls_dir, "dirall", 0,  (int)(sizeof(acls_dir)/sizeof(acls_dir[0])));
	archive_entry_free(ae);
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
#endif
}
