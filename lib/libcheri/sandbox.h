/*-
 * Copyright (c) 2012-2015 Robert N. M. Watson
 * Copyright (c) 2015 SRI International
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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
 */

#ifndef _SANDBOX_H_
#define	_SANDBOX_H_

/*
 * This section defines the interface between 'inside' and 'outside' the
 * sandbox model.
 */

/*
 * Per-sandbox meta-data structure mapped read-only within the sandbox at a
 * fixed address to allow sandboxed code to find its stack, heap, etc.
 *
 * NB: This data structure (and its base address) are part of the ABI between
 * libcheri and programs running in sandboxes.  Only ever append to this,
 * don't modify the order, lengths, or interpretations of existing fields.  If
 * this reaches a page in size, then allocation code in sandbox.c will need
 * updating.  See also sandbox.c and sandboxasm.h.
 */
struct sandbox_metadata {
	register_t	sbm_heapbase;			/* Offset: 0 */
	register_t	sbm_heaplen;			/* Offset: 8 */
	uint64_t	_sbm_reserved0;			/* Offset: 16 */
	uint64_t	_sbm_reserved1;			/* Offset: 24 */
	struct cheri_object	sbm_system_object;	/* Offset: 32 */
#if __has_feature(capabilities)
	__capability vm_offset_t	*sbm_vtable;		/* Cap-offset: 2 */
	__capability void	*sbm_stackcap;		/* Cap-offset: 3 */
#else
	struct chericap	sbm_vtable;
	struct chericap	sbm_stackcap;
#endif
};

/*
 * This section defines interfaces for setting up, invoking, resetting, and
 * destroying sandbox classes and objects.
 */

extern int sb_verbose;

struct sandbox_class;
int	sandbox_class_new(const char *path, size_t maxmapsize,
	    struct sandbox_class **sbcpp);
int	sandbox_class_method_declare(struct sandbox_class *sbcp,
	    u_int methodnum, const char *methodname);
void	sandbox_class_destroy(struct sandbox_class *sbcp);

struct sandbox_object;
int	sandbox_object_new(struct sandbox_class *sbcp, size_t heaplen,
	    struct sandbox_object **sbopp);
int	sandbox_object_new_flags(struct sandbox_class *sbcp, size_t heaplen,
	    uint flags, struct sandbox_object **sbopp);
int	sandbox_object_reset(struct sandbox_object *sbop);
#if __has_feature(capabilities)
register_t	sandbox_object_cinvoke(struct sandbox_object *sbop,
		    register_t methodnum, register_t a1,
		    register_t a2, register_t a3, register_t a4,
		    register_t a5, register_t a6, register_t a7,
		    __capability void *c3, __capability void *c4,
		    __capability void *c5, __capability void *c6,
		    __capability void *c7, __capability void *c8,
		    __capability void *c9, __capability void *c10);
#endif
register_t	sandbox_object_invoke(struct sandbox_object *sbop,
		    register_t methodnum, register_t a1,
		    register_t a2, register_t a3, register_t a4,
		    register_t a5, register_t a6, register_t a7,
		    struct chericap *c3p, struct chericap *c4p,
		    struct chericap *c5p, struct chericap *c6p,
		    struct chericap *c7p, struct chericap *c8p,
		    struct chericap *c9p, struct chericap *c10p);
void	sandbox_object_destroy(struct sandbox_object *sbop);

/*
 * Flags for sandbox_object_new_flags():
 */
#define	SANDBOX_OBJECT_FLAG_CONSOLE	0x00000001	/* printf(), etc. */
#define	SANDBOX_OBJECT_FLAG_ALLOCFREE	0x00000002	/* calloc(), free(). */
#define	SANDBOX_OBJECT_FLAG_USERFN	0x00000004	/* User callbacks. */

/*
 * API to query the object-capability pair for the sandbox itself
 */
struct cheri_object	sandbox_object_getobject(struct sandbox_object *sbop);

/*
 * API to query system capabilities for use by sandboxes.
 */
struct cheri_object	sandbox_object_getsystemobject(
			    struct sandbox_object *sbop);

#endif /* !_SANDBOX_H_ */
