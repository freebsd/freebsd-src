/*
 * Copyright (c) 2001 Chris D. Faulhaber
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR THE VOICES IN HIS HEAD BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _SETFACL_H
#define _SETFACL_H

#include <sys/types.h>
#include <sys/acl.h>
#include <sys/queue.h>

/* file operations */
#define	OP_MERGE_ACL		0x00	/* merge acl's (-mM) */
#define	OP_REMOVE_DEF		0x01	/* remove default acl's (-k) */
#define	OP_REMOVE_EXT		0x02	/* remove extended acl's (-b) */
#define	OP_REMOVE_ACL		0x03	/* remove acl's (-xX) */

/* ACL types for the acl array */
#define ACCESS_ACL	0
#define DEFAULT_ACL	1

/* TAILQ entry for acl operations */
struct sf_entry {
	uint	op;
	acl_t	acl;
	TAILQ_ENTRY(sf_entry) next;
};
TAILQ_HEAD(, sf_entry) entrylist;

/* TAILQ entry for files */
struct sf_file {
	const char *filename;
	TAILQ_ENTRY(sf_file) next;
};
TAILQ_HEAD(, sf_file) filelist;

/* files.c */
acl_t  get_acl_from_file(const char *filename);
/* merge.c */
int    merge_acl(acl_t acl, acl_t *prev_acl);
/* remove.c */
int    remove_acl(acl_t acl, acl_t *prev_acl);
int    remove_default(acl_t *prev_acl);
void   remove_ext(acl_t *prev_acl);
/* mask.c */
int    set_acl_mask(acl_t *prev_acl);
/* util.c */
void  *zmalloc(size_t size);

acl_type_t acl_type;
uint       have_mask;
uint       need_mask;
uint       have_stdin;
uint       n_flag;

#endif /* _SETFACL_H */
