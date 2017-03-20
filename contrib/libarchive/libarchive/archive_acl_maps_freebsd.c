/*-
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

#include "archive_platform.h"

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_ACL_H
#define _ACL_PRIVATE /* For debugging */
#include <sys/acl.h>
#endif

#include "archive_entry.h"
#include "archive_private.h"
#include "archive_read_disk_private.h"
#include "archive_acl_maps.h"

const acl_perm_map_t acl_posix_perm_map[] = {
	{ARCHIVE_ENTRY_ACL_EXECUTE, ACL_EXECUTE},
	{ARCHIVE_ENTRY_ACL_WRITE, ACL_WRITE},
	{ARCHIVE_ENTRY_ACL_READ, ACL_READ},
};

const int acl_posix_perm_map_size =
    (int)(sizeof(acl_posix_perm_map)/sizeof(acl_posix_perm_map[0]));

#if ARCHIVE_ACL_FREEBSD_NFS4
const acl_perm_map_t acl_nfs4_perm_map[] = {
	{ARCHIVE_ENTRY_ACL_EXECUTE, ACL_EXECUTE},
	{ARCHIVE_ENTRY_ACL_READ_DATA, ACL_READ_DATA},
	{ARCHIVE_ENTRY_ACL_LIST_DIRECTORY, ACL_LIST_DIRECTORY},
	{ARCHIVE_ENTRY_ACL_WRITE_DATA, ACL_WRITE_DATA},
	{ARCHIVE_ENTRY_ACL_ADD_FILE, ACL_ADD_FILE},
	{ARCHIVE_ENTRY_ACL_APPEND_DATA, ACL_APPEND_DATA},
	{ARCHIVE_ENTRY_ACL_ADD_SUBDIRECTORY, ACL_ADD_SUBDIRECTORY},
	{ARCHIVE_ENTRY_ACL_READ_NAMED_ATTRS, ACL_READ_NAMED_ATTRS},
	{ARCHIVE_ENTRY_ACL_WRITE_NAMED_ATTRS, ACL_WRITE_NAMED_ATTRS},
	{ARCHIVE_ENTRY_ACL_DELETE_CHILD, ACL_DELETE_CHILD},
	{ARCHIVE_ENTRY_ACL_READ_ATTRIBUTES, ACL_READ_ATTRIBUTES},
	{ARCHIVE_ENTRY_ACL_WRITE_ATTRIBUTES, ACL_WRITE_ATTRIBUTES},
	{ARCHIVE_ENTRY_ACL_DELETE, ACL_DELETE},
	{ARCHIVE_ENTRY_ACL_READ_ACL, ACL_READ_ACL},
	{ARCHIVE_ENTRY_ACL_WRITE_ACL, ACL_WRITE_ACL},
	{ARCHIVE_ENTRY_ACL_WRITE_OWNER, ACL_WRITE_OWNER},
	{ARCHIVE_ENTRY_ACL_SYNCHRONIZE, ACL_SYNCHRONIZE}
};

const int acl_nfs4_perm_map_size =
    (int)(sizeof(acl_nfs4_perm_map)/sizeof(acl_nfs4_perm_map[0]));

const acl_perm_map_t acl_nfs4_flag_map[] = {
	{ARCHIVE_ENTRY_ACL_ENTRY_FILE_INHERIT, ACL_ENTRY_FILE_INHERIT},
	{ARCHIVE_ENTRY_ACL_ENTRY_DIRECTORY_INHERIT, ACL_ENTRY_DIRECTORY_INHERIT},
	{ARCHIVE_ENTRY_ACL_ENTRY_NO_PROPAGATE_INHERIT, ACL_ENTRY_NO_PROPAGATE_INHERIT},
	{ARCHIVE_ENTRY_ACL_ENTRY_INHERIT_ONLY, ACL_ENTRY_INHERIT_ONLY},
	{ARCHIVE_ENTRY_ACL_ENTRY_SUCCESSFUL_ACCESS, ACL_ENTRY_SUCCESSFUL_ACCESS},
	{ARCHIVE_ENTRY_ACL_ENTRY_FAILED_ACCESS, ACL_ENTRY_FAILED_ACCESS},
	{ARCHIVE_ENTRY_ACL_ENTRY_INHERITED, ACL_ENTRY_INHERITED}
};

const int acl_nfs4_flag_map_size =
    (int)(sizeof(acl_nfs4_flag_map)/sizeof(acl_nfs4_flag_map[0]));
#endif /* ARCHIVE_ACL_FREEBSD_NFS4 */
