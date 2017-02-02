/*-
 * Copyright (c) 2003-2010 Tim Kientzle
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
__FBSDID("$FreeBSD$");

#if HAVE_POSIX_ACL || HAVE_NFS4_ACL
#define _ACL_PRIVATE
#include <sys/acl.h>
#if HAVE_DARWIN_ACL
#include <membership.h>
#endif
#endif

#if HAVE_NFS4_ACL
struct myacl_t {
	int type;
	int permset;
	int tag;
	int qual; /* GID or UID of user/group, depending on tag. */
	const char *name; /* Name of user/group, depending on tag. */
};

static struct myacl_t acls_reg[] = {
#if !HAVE_DARWIN_ACL
	/* For this test, we need the file owner to be able to read and write the ACL. */
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW,
	  ARCHIVE_ENTRY_ACL_READ_DATA | ARCHIVE_ENTRY_ACL_READ_ACL | ARCHIVE_ENTRY_ACL_WRITE_ACL | ARCHIVE_ENTRY_ACL_READ_NAMED_ATTRS | ARCHIVE_ENTRY_ACL_READ_ATTRIBUTES,
	  ARCHIVE_ENTRY_ACL_USER_OBJ, -1, ""},
#endif
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
#if !HAVE_DARWIN_ACL
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_EXECUTE,
	  ARCHIVE_ENTRY_ACL_GROUP_OBJ, -1, "" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_EXECUTE,
	  ARCHIVE_ENTRY_ACL_EVERYONE, -1, "" }
#else	/* MacOS - mode 0654 */
	{ ARCHIVE_ENTRY_ACL_TYPE_DENY, ARCHIVE_ENTRY_ACL_EXECUTE,
	  ARCHIVE_ENTRY_ACL_USER_OBJ, -1, "" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW,
	    ARCHIVE_ENTRY_ACL_READ_DATA |
	    ARCHIVE_ENTRY_ACL_WRITE_DATA |
	    ARCHIVE_ENTRY_ACL_APPEND_DATA |
	    ARCHIVE_ENTRY_ACL_READ_ATTRIBUTES |
	    ARCHIVE_ENTRY_ACL_WRITE_ATTRIBUTES |
	    ARCHIVE_ENTRY_ACL_READ_NAMED_ATTRS |
	    ARCHIVE_ENTRY_ACL_WRITE_NAMED_ATTRS |
	    ARCHIVE_ENTRY_ACL_READ_ACL |
	    ARCHIVE_ENTRY_ACL_WRITE_ACL |
	    ARCHIVE_ENTRY_ACL_WRITE_OWNER |
	    ARCHIVE_ENTRY_ACL_SYNCHRONIZE,
	  ARCHIVE_ENTRY_ACL_USER_OBJ, -1, "" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW,
	    ARCHIVE_ENTRY_ACL_READ_DATA |
	    ARCHIVE_ENTRY_ACL_EXECUTE |
	    ARCHIVE_ENTRY_ACL_READ_ATTRIBUTES |
	    ARCHIVE_ENTRY_ACL_READ_NAMED_ATTRS |
	    ARCHIVE_ENTRY_ACL_READ_ACL |
	    ARCHIVE_ENTRY_ACL_SYNCHRONIZE,
	  ARCHIVE_ENTRY_ACL_GROUP_OBJ, -1, "" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW,
	    ARCHIVE_ENTRY_ACL_READ_DATA |
	    ARCHIVE_ENTRY_ACL_READ_ATTRIBUTES |
	    ARCHIVE_ENTRY_ACL_READ_NAMED_ATTRS |
	    ARCHIVE_ENTRY_ACL_READ_ACL |
	    ARCHIVE_ENTRY_ACL_SYNCHRONIZE,
	  ARCHIVE_ENTRY_ACL_EVERYONE, -1, "" }
#endif
};

static const int acls_reg_cnt = (int)(sizeof(acls_reg)/sizeof(acls_reg[0]));

static struct myacl_t acls_dir[] = {
	/* For this test, we need to be able to read and write the ACL. */
#if !HAVE_DARWIN_ACL
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_READ_DATA | ARCHIVE_ENTRY_ACL_READ_ACL,
	  ARCHIVE_ENTRY_ACL_USER_OBJ, -1, ""},
#endif

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
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW,
	  ARCHIVE_ENTRY_ACL_READ_DATA | ARCHIVE_ENTRY_ACL_ENTRY_INHERITED,
	  ARCHIVE_ENTRY_ACL_USER, 305, "user305" },
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
#if !HAVE_DARWIN_ACL
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_LIST_DIRECTORY,
	  ARCHIVE_ENTRY_ACL_GROUP_OBJ, -1, "" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_LIST_DIRECTORY,
	  ARCHIVE_ENTRY_ACL_EVERYONE, -1, "" }
#else	/* MacOS - mode 0654 */
	{ ARCHIVE_ENTRY_ACL_TYPE_DENY, ARCHIVE_ENTRY_ACL_EXECUTE,
	  ARCHIVE_ENTRY_ACL_USER_OBJ, -1, "" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW,
	    ARCHIVE_ENTRY_ACL_READ_DATA |
	    ARCHIVE_ENTRY_ACL_WRITE_DATA |
	    ARCHIVE_ENTRY_ACL_APPEND_DATA |
	    ARCHIVE_ENTRY_ACL_READ_ATTRIBUTES |
	    ARCHIVE_ENTRY_ACL_WRITE_ATTRIBUTES |
	    ARCHIVE_ENTRY_ACL_READ_NAMED_ATTRS |
	    ARCHIVE_ENTRY_ACL_WRITE_NAMED_ATTRS |
	    ARCHIVE_ENTRY_ACL_READ_ACL |
	    ARCHIVE_ENTRY_ACL_WRITE_ACL |
	    ARCHIVE_ENTRY_ACL_WRITE_OWNER |
	    ARCHIVE_ENTRY_ACL_SYNCHRONIZE,
	  ARCHIVE_ENTRY_ACL_USER_OBJ, -1, "" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW,
	    ARCHIVE_ENTRY_ACL_READ_DATA |
	    ARCHIVE_ENTRY_ACL_EXECUTE |
	    ARCHIVE_ENTRY_ACL_READ_ATTRIBUTES |
	    ARCHIVE_ENTRY_ACL_READ_NAMED_ATTRS |
	    ARCHIVE_ENTRY_ACL_READ_ACL |
	    ARCHIVE_ENTRY_ACL_SYNCHRONIZE,
	  ARCHIVE_ENTRY_ACL_GROUP_OBJ, -1, "" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW,
	    ARCHIVE_ENTRY_ACL_READ_DATA |
	    ARCHIVE_ENTRY_ACL_READ_ATTRIBUTES |
	    ARCHIVE_ENTRY_ACL_READ_NAMED_ATTRS |
	    ARCHIVE_ENTRY_ACL_READ_ACL |
	    ARCHIVE_ENTRY_ACL_SYNCHRONIZE,
	  ARCHIVE_ENTRY_ACL_EVERYONE, -1, "" }
#endif
};

static const int acls_dir_cnt = (int)(sizeof(acls_dir)/sizeof(acls_dir[0]));

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
#ifdef HAVE_SUN_ACL
acl_permset_to_bitmap(uint32_t a_access_mask)
#else
acl_permset_to_bitmap(acl_permset_t opaque_ps)
#endif
{
	static struct { int machine; int portable; } perms[] = {
#ifdef HAVE_SUN_ACL	/* Solaris NFSv4 ACL permissions */
		{ACE_EXECUTE, ARCHIVE_ENTRY_ACL_EXECUTE},
		{ACE_READ_DATA, ARCHIVE_ENTRY_ACL_READ_DATA},
		{ACE_LIST_DIRECTORY, ARCHIVE_ENTRY_ACL_LIST_DIRECTORY},
		{ACE_WRITE_DATA, ARCHIVE_ENTRY_ACL_WRITE_DATA},
		{ACE_ADD_FILE, ARCHIVE_ENTRY_ACL_ADD_FILE},
		{ACE_APPEND_DATA, ARCHIVE_ENTRY_ACL_APPEND_DATA},
		{ACE_ADD_SUBDIRECTORY, ARCHIVE_ENTRY_ACL_ADD_SUBDIRECTORY},
		{ACE_READ_NAMED_ATTRS, ARCHIVE_ENTRY_ACL_READ_NAMED_ATTRS},
		{ACE_WRITE_NAMED_ATTRS, ARCHIVE_ENTRY_ACL_WRITE_NAMED_ATTRS},
		{ACE_DELETE_CHILD, ARCHIVE_ENTRY_ACL_DELETE_CHILD},
		{ACE_READ_ATTRIBUTES, ARCHIVE_ENTRY_ACL_READ_ATTRIBUTES},
		{ACE_WRITE_ATTRIBUTES, ARCHIVE_ENTRY_ACL_WRITE_ATTRIBUTES},
		{ACE_DELETE, ARCHIVE_ENTRY_ACL_DELETE},
		{ACE_READ_ACL, ARCHIVE_ENTRY_ACL_READ_ACL},
		{ACE_WRITE_ACL, ARCHIVE_ENTRY_ACL_WRITE_ACL},
		{ACE_WRITE_OWNER, ARCHIVE_ENTRY_ACL_WRITE_OWNER},
		{ACE_SYNCHRONIZE, ARCHIVE_ENTRY_ACL_SYNCHRONIZE}
#elif HAVE_DARWIN_ACL	/* MacOS NFSv4 ACL permissions */
		{ACL_READ_DATA, ARCHIVE_ENTRY_ACL_READ_DATA},
		{ACL_LIST_DIRECTORY, ARCHIVE_ENTRY_ACL_LIST_DIRECTORY},
		{ACL_WRITE_DATA, ARCHIVE_ENTRY_ACL_WRITE_DATA},
		{ACL_ADD_FILE, ARCHIVE_ENTRY_ACL_ADD_FILE},
		{ACL_EXECUTE, ARCHIVE_ENTRY_ACL_EXECUTE},
		{ACL_DELETE, ARCHIVE_ENTRY_ACL_DELETE},
		{ACL_APPEND_DATA, ARCHIVE_ENTRY_ACL_APPEND_DATA},
		{ACL_ADD_SUBDIRECTORY, ARCHIVE_ENTRY_ACL_ADD_SUBDIRECTORY},
		{ACL_DELETE_CHILD, ARCHIVE_ENTRY_ACL_DELETE_CHILD},
		{ACL_READ_ATTRIBUTES, ARCHIVE_ENTRY_ACL_READ_ATTRIBUTES},
		{ACL_WRITE_ATTRIBUTES, ARCHIVE_ENTRY_ACL_WRITE_ATTRIBUTES},
		{ACL_READ_EXTATTRIBUTES, ARCHIVE_ENTRY_ACL_READ_NAMED_ATTRS},
		{ACL_WRITE_EXTATTRIBUTES, ARCHIVE_ENTRY_ACL_WRITE_NAMED_ATTRS},
		{ACL_READ_SECURITY, ARCHIVE_ENTRY_ACL_READ_ACL},
		{ACL_WRITE_SECURITY, ARCHIVE_ENTRY_ACL_WRITE_ACL},
		{ACL_CHANGE_OWNER, ARCHIVE_ENTRY_ACL_WRITE_OWNER},
		{ACL_SYNCHRONIZE, ARCHIVE_ENTRY_ACL_SYNCHRONIZE},
#else	/* FreeBSD NFSv4 ACL permissions */
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
#endif
	};
	int i, permset = 0;

	for (i = 0; i < (int)(sizeof(perms)/sizeof(perms[0])); ++i)
#if HAVE_SUN_ACL
		if (a_access_mask & perms[i].machine)
#else
		if (acl_get_perm_np(opaque_ps, perms[i].machine))
#endif
			permset |= perms[i].portable;
	return permset;
}

static int
#if HAVE_SUN_ACL
acl_flagset_to_bitmap(uint16_t a_flags)
#else
acl_flagset_to_bitmap(acl_flagset_t opaque_fs)
#endif
{
	static struct { int machine; int portable; } flags[] = {
#if HAVE_SUN_ACL	/* Solaris NFSv4 ACL inheritance flags */
		{ACE_FILE_INHERIT_ACE, ARCHIVE_ENTRY_ACL_ENTRY_FILE_INHERIT},
		{ACE_DIRECTORY_INHERIT_ACE, ARCHIVE_ENTRY_ACL_ENTRY_DIRECTORY_INHERIT},
		{ACE_NO_PROPAGATE_INHERIT_ACE, ARCHIVE_ENTRY_ACL_ENTRY_NO_PROPAGATE_INHERIT},
		{ACE_INHERIT_ONLY_ACE, ARCHIVE_ENTRY_ACL_ENTRY_INHERIT_ONLY},
		{ACE_SUCCESSFUL_ACCESS_ACE_FLAG, ARCHIVE_ENTRY_ACL_ENTRY_SUCCESSFUL_ACCESS},
		{ACE_FAILED_ACCESS_ACE_FLAG, ARCHIVE_ENTRY_ACL_ENTRY_FAILED_ACCESS},
		{ACE_INHERITED_ACE, ARCHIVE_ENTRY_ACL_ENTRY_INHERITED}
#elif HAVE_DARWIN_ACL	/* MacOS NFSv4 ACL inheritance flags */
		{ACL_ENTRY_INHERITED, ARCHIVE_ENTRY_ACL_ENTRY_INHERITED},
		{ACL_ENTRY_FILE_INHERIT, ARCHIVE_ENTRY_ACL_ENTRY_FILE_INHERIT},
		{ACL_ENTRY_DIRECTORY_INHERIT, ARCHIVE_ENTRY_ACL_ENTRY_DIRECTORY_INHERIT},
		{ACL_ENTRY_LIMIT_INHERIT, ARCHIVE_ENTRY_ACL_ENTRY_NO_PROPAGATE_INHERIT},
		{ACL_ENTRY_ONLY_INHERIT, ARCHIVE_ENTRY_ACL_ENTRY_INHERIT_ONLY}
#else	/* FreeBSD NFSv4 ACL inheritance flags */
		{ACL_ENTRY_FILE_INHERIT, ARCHIVE_ENTRY_ACL_ENTRY_FILE_INHERIT},
		{ACL_ENTRY_DIRECTORY_INHERIT, ARCHIVE_ENTRY_ACL_ENTRY_DIRECTORY_INHERIT},
		{ACL_ENTRY_NO_PROPAGATE_INHERIT, ARCHIVE_ENTRY_ACL_ENTRY_NO_PROPAGATE_INHERIT},
		{ACL_ENTRY_SUCCESSFUL_ACCESS, ARCHIVE_ENTRY_ACL_ENTRY_SUCCESSFUL_ACCESS},
		{ACL_ENTRY_NO_PROPAGATE_INHERIT, ARCHIVE_ENTRY_ACL_ENTRY_FAILED_ACCESS},
		{ACL_ENTRY_INHERIT_ONLY, ARCHIVE_ENTRY_ACL_ENTRY_INHERIT_ONLY},
#endif
	};
	int i, flagset = 0;

	for (i = 0; i < (int)(sizeof(flags)/sizeof(flags[0])); ++i)
#if HAVE_SUN_ACL
		if (a_flags & flags[i].machine)
#else
		if (acl_get_flag_np(opaque_fs, flags[i].machine))
#endif
			flagset |= flags[i].portable;
	return flagset;
}

static int
#if HAVE_SUN_ACL
acl_match(ace_t *ace, struct myacl_t *myacl)
#else
acl_match(acl_entry_t aclent, struct myacl_t *myacl)
#endif
{
#if !HAVE_SUN_ACL
#if HAVE_DARWIN_ACL
	void *q;
	uid_t ugid;
	int r, idtype;
#else
	gid_t g, *gp;
	uid_t u, *up;
	acl_entry_type_t entry_type;
#endif	/* !HAVE_DARWIN_ACL */
	acl_tag_t tag_type;
	acl_permset_t opaque_ps;
	acl_flagset_t opaque_fs;
#endif	/* !HAVE_SUN_ACL */
	int perms;

#if HAVE_SUN_ACL
	perms = acl_permset_to_bitmap(ace->a_access_mask) | acl_flagset_to_bitmap(ace->a_flags);
#else
	acl_get_tag_type(aclent, &tag_type);
#if !HAVE_DARWIN_ACL
	acl_get_entry_type_np(aclent, &entry_type);
#endif

	/* translate the silly opaque permset to a bitmap */
	acl_get_permset(aclent, &opaque_ps);
	acl_get_flagset_np(aclent, &opaque_fs);
	perms = acl_permset_to_bitmap(opaque_ps) | acl_flagset_to_bitmap(opaque_fs);
#endif
	if (perms != myacl->permset)
		return (0);

#if HAVE_SUN_ACL
	switch (ace->a_type) {
	case ACE_ACCESS_ALLOWED_ACE_TYPE:
		if (myacl->type != ARCHIVE_ENTRY_ACL_TYPE_ALLOW)
			return (0);
		break;
	case ACE_ACCESS_DENIED_ACE_TYPE:
		if (myacl->type != ARCHIVE_ENTRY_ACL_TYPE_DENY)
			return (0);
		break;
	case ACE_SYSTEM_AUDIT_ACE_TYPE:
		if (myacl->type != ARCHIVE_ENTRY_ACL_TYPE_AUDIT)
			return (0);
		break;
	case ACE_SYSTEM_ALARM_ACE_TYPE:
		if (myacl->type != ARCHIVE_ENTRY_ACL_TYPE_ALARM)
			return (0);
		break;
	default:
		return (0);
	}

	if (ace->a_flags & ACE_OWNER) {
		if (myacl->tag != ARCHIVE_ENTRY_ACL_USER_OBJ)
			return (0);
	} else if (ace->a_flags & ACE_GROUP) {
		if (myacl->tag != ARCHIVE_ENTRY_ACL_GROUP_OBJ)
			return (0);
	} else if (ace->a_flags & ACE_EVERYONE) {
		if (myacl->tag != ARCHIVE_ENTRY_ACL_EVERYONE)
			return (0);
	} else if (ace->a_flags & ACE_IDENTIFIER_GROUP) {
		if (myacl->tag != ARCHIVE_ENTRY_ACL_GROUP)
			return (0);
		if ((gid_t)myacl->qual != ace->a_who)
			return (0);
	} else {
		if (myacl->tag != ARCHIVE_ENTRY_ACL_USER)
			return (0);
		if ((uid_t)myacl->qual != ace->a_who)
			return (0);
	}
#elif HAVE_DARWIN_ACL
	r = 0;
	switch (tag_type) {
	case ACL_EXTENDED_ALLOW:
		if (myacl->type != ARCHIVE_ENTRY_ACL_TYPE_ALLOW)
			return (0);
		break;
	case ACL_EXTENDED_DENY:
		if (myacl->type != ARCHIVE_ENTRY_ACL_TYPE_DENY)
			return (0);
		break;
	default:
		return (0);
	}
	q = acl_get_qualifier(aclent);
	if (q == NULL)
		return (0);
	r = mbr_uuid_to_id((const unsigned char *)q, &ugid, &idtype);
	acl_free(q);
	if (r != 0)
		return (0);
	switch (idtype) {
		case ID_TYPE_UID:
			if (myacl->tag != ARCHIVE_ENTRY_ACL_USER)
				return (0);
			if ((uid_t)myacl->qual != ugid)
				return (0);
			break;
		case ID_TYPE_GID:
			if (myacl->tag != ARCHIVE_ENTRY_ACL_GROUP)
				return (0);
			if ((gid_t)myacl->qual != ugid)
				return (0);
			break;
		default:
			return (0);
	}
#else	/* !HAVE_SUN_ACL && !HAVE_DARWIN_ACL */
	switch (entry_type) {
	case ACL_ENTRY_TYPE_ALLOW:
		if (myacl->type != ARCHIVE_ENTRY_ACL_TYPE_ALLOW)
			return (0);
		break;
	case ACL_ENTRY_TYPE_DENY:
		if (myacl->type != ARCHIVE_ENTRY_ACL_TYPE_DENY)
			return (0);
		break;
	case ACL_ENTRY_TYPE_AUDIT:
		if (myacl->type != ARCHIVE_ENTRY_ACL_TYPE_AUDIT)
			return (0);
	case ACL_ENTRY_TYPE_ALARM:
		if (myacl->type != ARCHIVE_ENTRY_ACL_TYPE_ALARM)
			return (0);
	default:
		return (0);
	}

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
#endif	/* !HAVE_SUN_ACL && !HAVE_DARWIN_ACL */
	return (1);
}

static void
compare_acls(
#if HAVE_SUN_ACL
    acl_t *acl,
#else
    acl_t acl,
#endif
    struct myacl_t *myacls, const char *filename, int start, int end)
{
	int *marker;
	int matched;
	int i, n;
#if HAVE_SUN_ACL
	int e;
	ace_t *acl_entry;
#else
	int entry_id = ACL_FIRST_ENTRY;
	acl_entry_t acl_entry;
#endif

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
#if HAVE_SUN_ACL
	for (e = 0; e < acl->acl_cnt; e++)
#elif HAVE_DARWIN_ACL
	while (0 == acl_get_entry(acl, entry_id, &acl_entry))
#else
	while (1 == acl_get_entry(acl, entry_id, &acl_entry))
#endif
	{
#if HAVE_SUN_ACL
		acl_entry = &((ace_t *)acl->acl_aclp)[e];
#else
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

		failure("ACL entry on file %s that shouldn't be there",
		    filename);
		assert(matched == 1);
	}

	/* Dump entries in the myacls array that weren't in the system acl. */
	for (i = 0; i < n; ++i) {
		failure(" ACL entry %d missing from %s: "
		    "type=%#010x,permset=%#010x,tag=%d,qual=%d,name=``%s''\n",
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
	assertEqualInt(n, archive_entry_acl_reset(ae,
	    ARCHIVE_ENTRY_ACL_TYPE_NFS4));
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
			"type=%#010x,permset=%#010x,tag=%d,qual=%d",
			type,permset,tag,qual);
		assert(matched == 1);
	}

	/* Dump entries in the myacls array that weren't in the system acl. */
	for (i = 0; i < n; ++i) {
		failure(" ACL entry %d missing from %s: "
		    "type=%#010x,permset=%#010x,tag=%d,qual=%d,name=``%s''\n",
		    marker[i], filename,
		    myacls[marker[i]].type, myacls[marker[i]].permset,
		    myacls[marker[i]].tag, myacls[marker[i]].qual,
		    myacls[marker[i]].name);
		assert(0); /* Record this as a failure. */
	}
	free(marker);
}
#endif	/* HAVE_NFS4_ACL */

/*
 * Verify ACL restore-to-disk.  This test is Platform-specific.
 */

DEFINE_TEST(test_acl_platform_nfs4)
{
#if !HAVE_NFS4_ACL
	skipping("NFS4 ACLs are not supported on this platform");
#else
	char buff[64];
	struct stat st;
	struct archive *a;
	struct archive_entry *ae;
	int i, n;
	char *func;
#if HAVE_DARWIN_ACL /* On MacOS we skip trivial ACLs in some tests */
	const int regcnt = acls_reg_cnt - 4;
	const int dircnt = acls_dir_cnt - 4;
#else
	const int regcnt = acls_reg_cnt;
	const int dircnt = acls_dir_cnt;
#endif
#if HAVE_SUN_ACL
	acl_t *acl;
#else	/* !HAVE_SUN_ACL */
#if HAVE_DARWIN_ACL
	acl_entry_t aclent;
	acl_permset_t permset;
	const uid_t uid = 1000;
	uuid_t uuid;
#endif	/* HAVE_DARWIN_ACL */
	acl_t acl;
#endif	/* !HAVE_SUN_ACL */

	/*
	 * First, do a quick manual set/read of ACL data to
	 * verify that the local filesystem does support ACLs.
	 * If it doesn't, we'll simply skip the remaining tests.
	 */
#if HAVE_POSIX_ACL && HAVE_ACL_TYPE_NFS4
	acl = acl_from_text("owner@:rwxp::allow,group@:rwp:f:allow");
	failure("acl_from_text(): errno = %d (%s)", errno, strerror(errno));
	assert((void *)acl != NULL);
#elif HAVE_DARWIN_ACL
	acl = acl_init(1);
	assert((void *)acl != NULL);
	assertEqualInt(0, acl_create_entry(&acl, &aclent));
	assertEqualInt(0, acl_set_tag_type(aclent, ACL_EXTENDED_ALLOW));
	assertEqualInt(0, acl_get_permset(aclent, &permset));
	assertEqualInt(0, acl_add_perm(permset, ACL_READ_DATA));
	assertEqualInt(0, acl_add_perm(permset, ACL_WRITE_DATA));
	assertEqualInt(0, acl_add_perm(permset, ACL_APPEND_DATA));
	assertEqualInt(0, acl_add_perm(permset, ACL_EXECUTE));
	assertEqualInt(0, acl_set_permset(aclent, permset));
	assertEqualInt(0, mbr_identifier_to_uuid(ID_TYPE_UID, &uid,
	    sizeof(uid_t), uuid));
	assertEqualInt(0, acl_set_qualifier(aclent, uuid));
#endif

	/* Create a test dir and try to set an ACL on it. */
	if (!assertMakeDir("pretest", 0755)) {
#if !HAVE_SUN_ACL
		acl_free(acl);
#endif
		return;
	}

#if HAVE_SUN_ACL
	func = "acl_get()";
	n = acl_get("pretest", 0, &acl);
#else
	func = "acl_set_file()";
#if HAVE_DARWIN_ACL
	n = acl_set_file("pretest", ACL_TYPE_EXTENDED, acl);
#else
	n = acl_set_file("pretest", ACL_TYPE_NFS4, acl);
#endif
	acl_free(acl);
#endif
	if (n != 0) {
#if HAVE_SUN_ACL
		if (errno == ENOSYS)
#else
		if (errno == EOPNOTSUPP || errno == EINVAL)
#endif
		{
			skipping("NFS4 ACL is not supported on this filesystem");
			return;
		}
	}
	failure("%s: errno = %d (%s)", func, errno, strerror(errno));
	assertEqualInt(0, n);

#if HAVE_SUN_ACL
	if (acl->acl_type != ACE_T) {
		acl_free(acl);
		skipping("NFS4 ACL is not supported on this filesystem");
		return;
	}
	acl_free(acl);
#endif

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
	set_acls(ae, acls_reg, 0, acls_reg_cnt);

	/* Write the entry to disk, including ACLs. */
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));

	/* Likewise for a dir. */
	archive_entry_set_pathname(ae, "dirall");
	archive_entry_set_filetype(ae, AE_IFDIR);
	archive_entry_set_perm(ae, 0654);
	archive_entry_set_mtime(ae, 123456, 7890);
	set_acls(ae, acls_dir, 0, acls_dir_cnt);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));

	for (i = 0; i < acls_dir_cnt; ++i) {
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
#if HAVE_SUN_ACL
	n = acl_get("testall", 0, &acl);
	failure("acl_get(): errno = %d (%s)", errno, strerror(errno));
	assertEqualInt(0, n);
#else
#if HAVE_DARWIN_ACL
	acl = acl_get_file("testall", ACL_TYPE_EXTENDED);
#else
	acl = acl_get_file("testall", ACL_TYPE_NFS4);
#endif
	failure("acl_get_file(): errno = %d (%s)", errno, strerror(errno));
	assert(acl != (acl_t)NULL);
#endif
	compare_acls(acl, acls_reg, "testall", 0, regcnt);
	acl_free(acl);

	/* Verify single-permission dirs on disk. */
	for (i = 0; i < dircnt; ++i) {
		sprintf(buff, "dir%d", i);
		assertEqualInt(0, stat(buff, &st));
		assertEqualInt(st.st_mtime, 123456 + i);
#if HAVE_SUN_ACL
		n = acl_get(buff, 0, &acl);
		failure("acl_get(): errno = %d (%s)", errno, strerror(errno));
		assertEqualInt(0, n);
#else
#if HAVE_DARWIN_ACL
		acl = acl_get_file(buff, ACL_TYPE_EXTENDED);
#else
		acl = acl_get_file(buff, ACL_TYPE_NFS4);
#endif
		failure("acl_get_file(): errno = %d (%s)", errno,
		    strerror(errno));
		assert(acl != (acl_t)NULL);
#endif
		compare_acls(acl, acls_dir, buff, i, i + 1);
		acl_free(acl);
	}

	/* Verify "dirall" on disk. */
	assertEqualInt(0, stat("dirall", &st));
	assertEqualInt(st.st_mtime, 123456);
#if HAVE_SUN_ACL
	n = acl_get("dirall", 0, &acl);
	failure("acl_get(): errno = %d (%s)", errno, strerror(errno));
	assertEqualInt(0, n);
#else
#if HAVE_DARWIN_ACL
	acl = acl_get_file("dirall", ACL_TYPE_EXTENDED);
#else
	acl = acl_get_file("dirall", ACL_TYPE_NFS4);
#endif
	failure("acl_get_file(): errno = %d (%s)", errno, strerror(errno));
	assert(acl != (acl_t)NULL);
#endif
	compare_acls(acl, acls_dir, "dirall", 0, dircnt);
	acl_free(acl);

	/* Read and compare ACL via archive_read_disk */
	a = archive_read_disk_new();
	assert(a != NULL);
	ae = archive_entry_new();
	assert(ae != NULL);
	archive_entry_set_pathname(ae, "testall");
	assertEqualInt(ARCHIVE_OK,
		       archive_read_disk_entry_from_file(a, ae, -1, NULL));
	compare_entry_acls(ae, acls_reg, "testall", 0, acls_reg_cnt);
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
	compare_entry_acls(ae, acls_dir, "dirall", 0, acls_dir_cnt);
	archive_entry_free(ae);
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
#endif /* HAVE_NFS4_ACL */
}
