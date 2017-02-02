/*-
 * Copyright (c) 2003-2008 Tim Kientzle
 * Copyright (c) 2017 Martin Matuska
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
__FBSDID("$FreeBSD: head/lib/libarchive/test/test_acl_freebsd.c 189427 2009-03-06 04:21:23Z kientzle $");

#if HAVE_POSIX_ACL || HAVE_SUN_ACL
#include <sys/acl.h>
#if HAVE_ACL_GET_PERM
#include <acl/libacl.h>
#define ACL_GET_PERM acl_get_perm
#elif HAVE_ACL_GET_PERM_NP
#define ACL_GET_PERM acl_get_perm_np
#endif

static struct archive_test_acl_t acls2[] = {
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
};

static int
#if HAVE_SUN_ACL
acl_entry_get_perm(aclent_t *aclent)
#else
acl_entry_get_perm(acl_entry_t aclent)
#endif
{
	int permset = 0;
#if HAVE_POSIX_ACL
	acl_permset_t opaque_ps;
#endif

#if HAVE_SUN_ACL
	if (aclent->a_perm & 1)
		permset |= ARCHIVE_ENTRY_ACL_EXECUTE;
	if (aclent->a_perm & 2)
		permset |= ARCHIVE_ENTRY_ACL_WRITE;
	if (aclent->a_perm & 4)
		permset |= ARCHIVE_ENTRY_ACL_READ;
#else
	/* translate the silly opaque permset to a bitmap */
	acl_get_permset(aclent, &opaque_ps);
	if (ACL_GET_PERM(opaque_ps, ACL_EXECUTE))
		permset |= ARCHIVE_ENTRY_ACL_EXECUTE;
	if (ACL_GET_PERM(opaque_ps, ACL_WRITE))
		permset |= ARCHIVE_ENTRY_ACL_WRITE;
	if (ACL_GET_PERM(opaque_ps, ACL_READ))
		permset |= ARCHIVE_ENTRY_ACL_READ;
#endif
	return permset;
}

#if 0
static int
acl_get_specific_entry(acl_t acl, acl_tag_t requested_tag_type, int requested_tag) {
	int entry_id = ACL_FIRST_ENTRY;
	acl_entry_t acl_entry;
	acl_tag_t acl_tag_type;
	
	while (1 == acl_get_entry(acl, entry_id, &acl_entry)) {
		/* After the first time... */
		entry_id = ACL_NEXT_ENTRY;

		/* If this matches, return perm mask */
		acl_get_tag_type(acl_entry, &acl_tag_type);
		if (acl_tag_type == requested_tag_type) {
			switch (acl_tag_type) {
			case ACL_USER_OBJ:
				if ((uid_t)requested_tag == *(uid_t *)(acl_get_qualifier(acl_entry))) {
					return acl_entry_get_perm(acl_entry);
				}
				break;
			case ACL_GROUP_OBJ:
				if ((gid_t)requested_tag == *(gid_t *)(acl_get_qualifier(acl_entry))) {
					return acl_entry_get_perm(acl_entry);
				}
				break;
			case ACL_USER:
			case ACL_GROUP:
			case ACL_OTHER:
				return acl_entry_get_perm(acl_entry);
			default:
				failure("Unexpected ACL tag type");
				assert(0);
			}
		}


	}
	return -1;
}
#endif

static int
#if HAVE_SUN_ACL
acl_match(aclent_t *aclent, struct archive_test_acl_t *myacl)
#else
acl_match(acl_entry_t aclent, struct archive_test_acl_t *myacl)
#endif
{
#if HAVE_POSIX_ACL
	gid_t g, *gp;
	uid_t u, *up;
	acl_tag_t tag_type;
#endif

	if (myacl->permset != acl_entry_get_perm(aclent))
		return (0);

#if HAVE_SUN_ACL
	switch (aclent->a_type)
#else
	acl_get_tag_type(aclent, &tag_type);
	switch (tag_type)
#endif
	{
#if HAVE_SUN_ACL
	case DEF_USER_OBJ:
	case USER_OBJ:
#else
	case ACL_USER_OBJ:
#endif
		if (myacl->tag != ARCHIVE_ENTRY_ACL_USER_OBJ) return (0);
		break;
#if HAVE_SUN_ACL
	case DEF_USER:
	case USER:
#else
	case ACL_USER:
#endif
		if (myacl->tag != ARCHIVE_ENTRY_ACL_USER)
			return (0);
#if HAVE_SUN_ACL
		if ((uid_t)myacl->qual != aclent->a_id)
			return (0);
#else
		up = acl_get_qualifier(aclent);
		u = *up;
		acl_free(up);
		if ((uid_t)myacl->qual != u)
			return (0);
#endif
		break;
#if HAVE_SUN_ACL
	case DEF_GROUP_OBJ:
	case GROUP_OBJ:
#else
	case ACL_GROUP_OBJ:
#endif
		if (myacl->tag != ARCHIVE_ENTRY_ACL_GROUP_OBJ) return (0);
		break;
#if HAVE_SUN_ACL
	case DEF_GROUP:
	case GROUP:
#else
	case ACL_GROUP:
#endif
		if (myacl->tag != ARCHIVE_ENTRY_ACL_GROUP)
			return (0);
#if HAVE_SUN_ACL
		if ((gid_t)myacl->qual != aclent->a_id)
			return (0);
#else
		gp = acl_get_qualifier(aclent);
		g = *gp;
		acl_free(gp);
		if ((gid_t)myacl->qual != g)
			return (0);
#endif
		break;
#if HAVE_SUN_ACL
	case DEF_CLASS_OBJ:
	case CLASS_OBJ:
#else
	case ACL_MASK:
#endif
		if (myacl->tag != ARCHIVE_ENTRY_ACL_MASK) return (0);
		break;
#if HAVE_SUN_ACL
	case DEF_OTHER_OBJ:
	case OTHER_OBJ:
#else
	case ACL_OTHER:
#endif
		if (myacl->tag != ARCHIVE_ENTRY_ACL_OTHER) return (0);
		break;
	}
	return (1);
}

static void
#if HAVE_SUN_ACL
compare_acls(acl_t *acl, struct archive_test_acl_t *myacls, int n)
#else
compare_acls(acl_t acl, struct archive_test_acl_t *myacls, int n)
#endif
{
	int *marker;
	int matched;
	int i;
#if HAVE_SUN_ACL
	int e;
	aclent_t *acl_entry;
#else
	int entry_id = ACL_FIRST_ENTRY;
	acl_entry_t acl_entry;
#endif

	/* Count ACL entries in myacls array and allocate an indirect array. */
	marker = malloc(sizeof(marker[0]) * n);
	if (marker == NULL)
		return;
	for (i = 0; i < n; i++)
		marker[i] = i;

	/*
	 * Iterate over acls in system acl object, try to match each
	 * one with an item in the myacls array.
	 */
#if HAVE_SUN_ACL
	for(e = 0; e < acl->acl_cnt; e++) {
		acl_entry = &((aclent_t *)acl->acl_aclp)[e];
#else
	while (1 == acl_get_entry(acl, entry_id, &acl_entry)) {
		/* After the first time... */
		entry_id = ACL_NEXT_ENTRY;
#endif

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
		    "type=%#010x,permset=%#010x,tag=%d,qual=%d,name=``%s''\n",
		    myacls[marker[i]].type, myacls[marker[i]].permset,
		    myacls[marker[i]].tag, myacls[marker[i]].qual,
		    myacls[marker[i]].name);
		assert(0); /* Record this as a failure. */
	}
	free(marker);
}

#endif


/*
 * Verify ACL restore-to-disk.  This test is Platform-specific.
 */

DEFINE_TEST(test_acl_platform_posix1e_restore)
{
#if !HAVE_SUN_ACL && !HAVE_POSIX_ACL
	skipping("POSIX.1e ACLs are not supported on this platform");
#else	/* HAVE_SUN_ACL || HAVE_POSIX_ACL */
	struct stat st;
	struct archive *a;
	struct archive_entry *ae;
	int n, fd;
	char *func;
#if HAVE_SUN_ACL
	acl_t *acl, *acl2;
#else
	acl_t acl;
#endif

	/*
	 * First, do a quick manual set/read of ACL data to
	 * verify that the local filesystem does support ACLs.
	 * If it doesn't, we'll simply skip the remaining tests.
	 */
#if HAVE_SUN_ACL
	n = acl_fromtext("user::rwx,user:1:rw-,group::rwx,group:15:r-x,other:rwx,mask:rwx", &acl);
	failure("acl_fromtext(): errno = %d (%s)", errno, strerror(errno));
	assertEqualInt(0, n);
#else
	acl = acl_from_text("u::rwx,u:1:rw,g::rwx,g:15:rx,o::rwx,m::rwx");
	failure("acl_from_text(): errno = %d (%s)", errno, strerror(errno));
	assert((void *)acl != NULL);
#endif

	/* Create a test file and try ACL on it. */
	fd = open("pretest", O_WRONLY | O_CREAT | O_EXCL, 0777);
	failure("Could not create test file?!");
	if (!assert(fd >= 0)) {
		acl_free(acl);
		return;
	}

#if HAVE_SUN_ACL
	n = facl_get(fd, 0, &acl2);
	if (n != 0) {
		close(fd);
		acl_free(acl);
	}
	if (errno == ENOSYS) {
		skipping("POSIX.1e ACLs are not supported on this filesystem");
		return;
	}
	failure("facl_get(): errno = %d (%s)", errno, strerror(errno));
	assertEqualInt(0, n);

	if (acl2->acl_type != ACLENT_T) {
		acl_free(acl2);
		skipping("POSIX.1e ACLs are not supported on this filesystem");
		return;
	}
	acl_free(acl2);

	func = "facl_set()";
	n = facl_set(fd, acl);
#else
	func = "acl_set_fd()";
	n = acl_set_fd(fd, acl);
#endif
	acl_free(acl);
	if (n != 0) {
#if HAVE_SUN_ACL
		if (errno == ENOSYS)
#else
		if (errno == EOPNOTSUPP || errno == EINVAL)
#endif
		{
			close(fd);
			skipping("POSIX.1e ACLs are not supported on this filesystem");
			return;
		}
	}
	failure("%s: errno = %d (%s)", func, errno, strerror(errno));
	assertEqualInt(0, n);

#if HAVE_SUN_ACL

#endif
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
	archive_test_set_acls(ae, acls2, sizeof(acls2)/sizeof(acls2[0]));
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	archive_entry_free(ae);

	/* Close the archive. */
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	/* Verify the data on disk. */
	assertEqualInt(0, stat("test0", &st));
	assertEqualInt(st.st_mtime, 123456);
#if HAVE_SUN_ACL
	n = acl_get("test0", 0, &acl);
	failure("acl_get(): errno = %d (%s)", errno, strerror(errno));
	assertEqualInt(0, n);
#else
	acl = acl_get_file("test0", ACL_TYPE_ACCESS);
	failure("acl_get_file(): errno = %d (%s)", errno, strerror(errno));
	assert(acl != (acl_t)NULL);
#endif
	compare_acls(acl, acls2, sizeof(acls2)/sizeof(acls2[0]));
	acl_free(acl);
#endif	/* HAVE_SUN_ACL || HAVE_POSIX_ACL */
}

/*
 * Verify ACL read-from-disk.  This test is Platform-specific.
 */
DEFINE_TEST(test_acl_platform_posix1e_read)
{
#if !HAVE_SUN_ACL && !HAVE_POSIX_ACL
	skipping("POSIX.1e ACLs are not supported on this platform");
#else
	struct archive *a;
	struct archive_entry *ae;
	int n, fd, flags, dflags;
	char *func, *acl_text;
	const char *acl1_text, *acl2_text, *acl3_text;
#if HAVE_SUN_ACL
	acl_t *acl, *acl1, *acl2, *acl3;
#else
	acl_t acl1, acl2, acl3;
#endif

	/*
	 * Manually construct a directory and two files with
	 * different ACLs.  This also serves to verify that ACLs
	 * are supported on the local filesystem.
	 */

	/* Create a test file f1 with acl1 */
#if HAVE_SUN_ACL
	acl1_text = "user::rwx,"
	    "group::rwx,"
	    "other:rwx,"
	    "user:1:rw-,"
	    "group:15:r-x,"
	    "mask:rwx";
	n = acl_fromtext(acl1_text, &acl1);
	failure("acl_fromtext(): errno = %d (%s)", errno, strerror(errno));
	assertEqualInt(0, n);
#else
	acl1_text = "user::rwx\n"
	    "group::rwx\n"
	    "other::rwx\n"
	    "user:1:rw-\n"
	    "group:15:r-x\n"
	    "mask::rwx";
	acl1 = acl_from_text(acl1_text);
	failure("acl_from_text(): errno = %d (%s)", errno, strerror(errno));
	assert((void *)acl1 != NULL);
#endif
	fd = open("f1", O_WRONLY | O_CREAT | O_EXCL, 0777);
	failure("Could not create test file?!");
	if (!assert(fd >= 0)) {
		acl_free(acl1);
		return;
	}
#if HAVE_SUN_ACL
	/* Check if Solars filesystem supports POSIX.1e ACLs */
	n = facl_get(fd, 0, &acl);
	if (n != 0)
		close(fd);
	if (n != 0 && errno == ENOSYS) {
		acl_free(acl1);
		skipping("POSIX.1e ACLs are not supported on this filesystem");
		return;
	}
	failure("facl_get(): errno = %d (%s)", errno, strerror(errno));
	assertEqualInt(0, n);

	if (acl->acl_type != ACLENT_T) {
		acl_free(acl);
		acl_free(acl1);
		close(fd);
		skipping("POSIX.1e ACLs are not supported on this filesystem");
		return;
	}

	func = "facl_set()";
	n = facl_set(fd, acl1);
#else
	func = "acl_set_fd()";
	n = acl_set_fd(fd, acl1);
#endif
	acl_free(acl1);

	if (n != 0) {
#if HAVE_SUN_ACL
		if (errno == ENOSYS)
#else
		if (errno == EOPNOTSUPP || errno == EINVAL)
#endif
		{
			close(fd);
			skipping("POSIX.1e ACLs are not supported on this filesystem");
			return;
		}
	}
	failure("%s: errno = %d (%s)", func, errno, strerror(errno));
	assertEqualInt(0, n);

	close(fd);

	assertMakeDir("d", 0700);

	/*
	 * Create file d/f1 with acl2
	 *
	 * This differs from acl1 in the u:1: and g:15: permissions.
	 *
	 * This file deliberately has the same name but a different ACL.
	 * Github Issue #777 explains how libarchive's directory traversal
	 * did not always correctly enter directories before attempting
	 * to read ACLs, resulting in reading the ACL from a like-named
	 * file in the wrong directory.
	 */
#if HAVE_SUN_ACL
	acl2_text = "user::rwx,"
	    "group::rwx,"
	    "other:---,"
	    "user:1:r--,"
	    "group:15:r--,"
	    "mask:rwx";
	n = acl_fromtext(acl2_text, &acl2);
	failure("acl_fromtext(): errno = %d (%s)", errno, strerror(errno));
	assertEqualInt(0, n);
#else
	acl2_text = "user::rwx\n"
	    "group::rwx\n"
	    "other::---\n"
	    "user:1:r--\n"
	    "group:15:r--\n"
	    "mask::rwx";
	acl2 = acl_from_text(acl2_text);
	failure("acl_from_text(): errno = %d (%s)", errno, strerror(errno));
	assert((void *)acl2 != NULL);
#endif
	fd = open("d/f1", O_WRONLY | O_CREAT | O_EXCL, 0777);
	failure("Could not create test file?!");
	if (!assert(fd >= 0)) {
		acl_free(acl2);
		return;
	}
#if HAVE_SUN_ACL
	func = "facl_set()";
	n = facl_set(fd, acl2);
#else
	func = "acl_set_fd()";
	n = acl_set_fd(fd, acl2);
#endif
	acl_free(acl2);
	if (n != 0)
		close(fd);
	failure("%s: errno = %d (%s)", func, errno, strerror(errno));
	assertEqualInt(0, n);
	close(fd);

	/* Create directory d2 with default ACLs */
	assertMakeDir("d2", 0755);

#if HAVE_SUN_ACL
	acl3_text = "user::rwx,"
	    "group::r-x,"
	    "other:r-x,"
	    "user:2:r--,"
	    "group:16:-w-,"
	    "mask:rwx,"
	    "default:user::rwx,"
	    "default:user:1:r--,"
	    "default:group::r-x,"
	    "default:group:15:r--,"
	    "default:mask:rwx,"
	    "default:other:r-x";
	n = acl_fromtext(acl3_text, &acl3);
	failure("acl_fromtext(): errno = %d (%s)", errno, strerror(errno));
	assertEqualInt(0, n);
#else
	acl3_text = "user::rwx\n"
	    "user:1:r--\n"
	    "group::r-x\n"
	    "group:15:r--\n"
	    "mask::rwx\n"
	    "other::r-x";
	acl3 = acl_from_text(acl3_text);
	failure("acl_from_text(): errno = %d (%s)", errno, strerror(errno));
	assert((void *)acl3 != NULL);
#endif

#if HAVE_SUN_ACL
	func = "acl_set()";
	n = acl_set("d2", acl3);
#else
	func = "acl_set_file()";
	n = acl_set_file("d2", ACL_TYPE_DEFAULT, acl3);
#endif
	acl_free(acl3);

	failure("%s: errno = %d (%s)", func, errno, strerror(errno));
	assertEqualInt(0, n);

	/* Create a read-from-disk object. */
	assert(NULL != (a = archive_read_disk_new()));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_disk_open(a, "."));
	assert(NULL != (ae = archive_entry_new()));

#if HAVE_SUN_ACL
	flags = ARCHIVE_ENTRY_ACL_TYPE_POSIX1E
	    | ARCHIVE_ENTRY_ACL_STYLE_SEPARATOR_COMMA
	    | ARCHIVE_ENTRY_ACL_STYLE_SOLARIS;
	dflags = flags;
#else
	flags = ARCHIVE_ENTRY_ACL_TYPE_ACCESS;
	dflags = ARCHIVE_ENTRY_ACL_TYPE_DEFAULT;
#endif

	/* Walk the dir until we see both of the files */
	while (ARCHIVE_OK == archive_read_next_header2(a, ae)) {
		archive_read_disk_descend(a);
		if (strcmp(archive_entry_pathname(ae), "./f1") == 0) {
			acl_text = archive_entry_acl_to_text(ae, NULL, flags);
			assertEqualString(acl_text, acl1_text);
			free(acl_text);
		} else if (strcmp(archive_entry_pathname(ae), "./d/f1") == 0) {
			acl_text = archive_entry_acl_to_text(ae, NULL, flags);
			assertEqualString(acl_text, acl2_text);
			free(acl_text);
		} else if (strcmp(archive_entry_pathname(ae), "./d2") == 0) {
			acl_text = archive_entry_acl_to_text(ae, NULL, dflags);
			assertEqualString(acl_text, acl3_text);
			free(acl_text);
		}
	}

	archive_entry_free(ae);
	assertEqualInt(ARCHIVE_OK, archive_free(a));
#endif
}
