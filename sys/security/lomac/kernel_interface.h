/*-
 * Copyright (c) 2001 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by NAI Labs, the
 * Security Research Division of Network Associates, Inc. under
 * DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"), as part of the DARPA
 * CHATS research program.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
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
 * $Id$
 * $FreeBSD$
 */

#ifndef KERNEL_INTERFACE_H
#define KERNEL_INTERFACE_H

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/sbuf.h>
#include <sys/vnode.h>

#include <vm/vm.h>
#include <vm/vm_object.h>

#include "lomac.h"


/*
 * We do not yet implement any categories.  We use lattr_t's flags
 * field to implement the "LOMAC_ATTR_LOWWRITE" exception.  When
 * this bit is set on a file, lowest-level processes can write to
 * the file, regardless of the file's level.
 *
 * There is also the LOMAC_ATTR_LOWNOOPEN flag, which prevents the
 * opening of a given object to subjects with a lower level than its
 * own.
 *
 * LOMAC_ATTR_NONETDEMOTE is set on subjects to prevent demotion on
 * network reads; LOMAC_ATTR_NODEMOTE is set on subjects to prevent
 * all demotion.  Both of these will effectively set a high-level
 * subject as very trusted (and must be used sparingly).
 */
#define LOMAC_ATTR_LOWWRITE		0x00010000
#define LOMAC_ATTR_LOWNOOPEN		0x00020000
#define	LOMAC_ATTR_NONETDEMOTE		0x00040000
#define	LOMAC_ATTR_NODEMOTE		0x00080000

typedef struct proc lomac_subject_t;
typedef struct lomac_object {
	enum lomac_object_type {
		LO_TYPE_LVNODE,		/* LOMAC vnode */
		LO_TYPE_UVNODE,		/* underlying vnode */
		LO_TYPE_VM_OBJECT,	/* VM object, not OBJT_VNODE */
		LO_TYPE_PIPE,		/* pipe */
		LO_TYPE_SOCKETPAIR      /* local-domain socket in socketpair */
	} lo_type;
	union {
		struct vnode *vnode;
		vm_object_t vm_object;
		struct pipe *pipe;
		struct socket *socket;
	} lo_object;
} lomac_object_t;
typedef struct sbuf lomac_log_t;

void init_subject_lattr(lomac_subject_t *, lattr_t *);
void set_subject_lattr(lomac_subject_t *, lattr_t);
void get_subject_lattr(lomac_subject_t *, lattr_t *);
void set_object_lattr(lomac_object_t *, lattr_t);
void get_object_lattr(const lomac_object_t *, lattr_t *);
int subject_do_not_demote(lomac_subject_t *);
#if 0
void subject_propogate_lattr(lomac_subject_t *);
#endif
void kernel_vm_drop_perms(struct thread *, lattr_t *);

#endif /* KERNEL_INTERFACE_H */
