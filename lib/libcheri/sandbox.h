/*-
 * Copyright (c) 2012-2014 Robert N. M. Watson
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
	register_t	sbm_heapbase;		/* Offset: 0 */
	register_t	sbm_heaplen;		/* Offset: 8 */
};

/*
 * This section defines interfaces for setting up, invoking, resetting, and
 * destroying sandboxes.
 */

extern int sb_verbose;

struct sandbox;
int	sandbox_setup(const char *path, register_t sandboxlen,
	    struct sandbox **sbp);
void	sandbox_destroy(struct sandbox *sb);

#if __has_feature(capabilities)
register_t
sandbox_cinvoke(struct sandbox *sb, register_t a0, register_t a1,
    register_t a2, register_t a3, register_t a4, register_t a5, register_t a6,
    register_t a7, __capability void *c3, __capability void *c4,
    __capability void *c5, __capability void *c6, __capability void *c7,
    __capability void *c8, __capability void *c9, __capability void *c10);
#endif
register_t	sandbox_invoke(struct sandbox *sb, register_t a0,
	    register_t a1, register_t a2, register_t a3, struct chericap *c3,
	    struct chericap *c4, struct chericap *c5, struct chericap *c6,
	    struct chericap *c7, struct chericap *c8, struct chericap *c9,
	    struct chericap *c10);

/*
 * Second-generation sandbox API with a more object-oriented spin.
 */
struct sandbox_class;
int	sandbox_class_new(const char *path, size_t sandboxlen,
	    struct sandbox_class **sbcpp);
int	sandbox_class_method_declare(struct sandbox_class *sbcp,
	    u_int methodnum, const char *methodname);
void	sandbox_class_destroy(struct sandbox_class *sbcp);

struct sandbox_object;
int	sandbox_object_new(struct sandbox_class *sbcp,
	    struct sandbox_object **sbopp);
#if __has_feature(capabilities)
register_t	sandbox_object_cinvoke(struct sandbox_object *sbop,
		    u_int methodnum, register_t a1, register_t a2,
		    register_t a3, register_t a4, register_t a5,
		    register_t a6, register_t a7, __capability void *c3,
		    __capability void *c4, __capability void *c5,
		    __capability void *c6,   __capability void *c7,
		    __capability void *c8, __capability void *c9,
		    __capability void *c10);
#endif
register_t	sandbox_object_invoke(struct sandbox_object *sbop,
		    register_t methodnum, register_t a1, register_t a2,
		    register_t a3, register_t a4, register_t a5,
		    register_t a6, register_t a7, struct chericap *c3p,
		    struct chericap *c4p, struct chericap *c5p,
		    struct chericap *c6p, struct chericap *c7p,
		    struct chericap *c8p, struct chericap *c9p,
		    struct chericap *c10p);
void	sandbox_object_destroy(struct sandbox_object *sbop);

/*
 * API to query system capabilities for use by sandboxes.
 */
void	cheri_systemcap_get(struct cheri_object *cop);

#endif /* !_SANDBOX_H_ */
