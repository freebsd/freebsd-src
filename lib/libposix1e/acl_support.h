/*-
 * Copyright (c) 1999 Robert N. M. Watson
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$FreeBSD: src/lib/libposix1e/acl_support.h,v 1.2 2000/01/26 04:19:37 rwatson Exp $
 */
/*
 * Support functionality for the POSIX.1e ACL interface
 * These calls are intended only to be called within the library.
 */
#ifndef _ACL_SUPPORT_H
#define _ACL_SUPPORT_H

#define ACL_STRING_PERM_MAXSIZE 3       /* read, write, exec */

int	acl_check(struct acl *acl);
int	acl_sort(acl_t acl);
int	acl_posix1e(acl_t acl, acl_type_t type);
int	acl_id_to_name(acl_tag_t tag, uid_t id, ssize_t buf_len, char *buf);
int	acl_name_to_id(acl_tag_t tag, char *name, uid_t *id);
int	acl_perm_to_string(acl_perm_t perm, ssize_t buf_len, char *buf);
int	acl_string_to_perm(char *string, acl_perm_t *perm);
int	acl_add_entry(acl_t acl, acl_tag_t tag, uid_t id, acl_perm_t perm);

#endif
