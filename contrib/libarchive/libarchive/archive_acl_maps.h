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

#ifndef __LIBARCHIVE_BUILD
#error This header is only to be used internally to libarchive.
#endif

#ifndef ARCHIVE_ACL_MAPS_H_INCLUDED
#define ARCHIVE_ACL_MAPS_H_INCLUDED

#include "archive_platform_acl.h"

typedef struct {
	const int a_perm;	/* Libarchive permission or flag */
	const int p_perm;	/* Platform permission or flag */
} acl_perm_map_t;

#if ARCHIVE_ACL_POSIX1E
extern const acl_perm_map_t acl_posix_perm_map[];
extern const int acl_posix_perm_map_size;
#endif
#if ARCHIVE_ACL_NFS4
extern const acl_perm_map_t acl_nfs4_perm_map[];
extern const int acl_nfs4_perm_map_size;
extern const acl_perm_map_t acl_nfs4_flag_map[];
extern const int acl_nfs4_flag_map_size;
#endif
#endif /* ARCHIVE_ACL_MAPS_H_INCLUDED */
