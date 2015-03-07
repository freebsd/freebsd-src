/*-
 * Copyright (c) 2014-2015 SRI International
 * Copyright (c) 2015 Robert N. M. Watson
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

#ifndef __SANDBOX_METHODS_H__
#define __SANDBOX_METHODS_H__

/*
 * Description of a method provided by a sandbox to be called via ccall.
 * This data is used to create the 'sandbox class' vtable, resolve symbols
 * to fill in caller .CHERI_CALLER variables, and store the corresponding
 * method numbers in the .CHERI_CALLEE variables of each 'sandbox object'.
 *
 * XXXBD: Storing the method offsets past the vtable creation is arguably
 * wasteful, but would allow a model where method entries are only populated
 * in the vtable if the linker finds a consumer thus limited the attack
 * surface.
 * XXXBD: Updating callee variables via indirection is inefficient.  If
 * we're sure that symtab entries are the same order as .CHERI_CALLEE then
 * we could avoid that and simply fill them in order.
 */
struct sandbox_provided_method {
	char		*spm_method;		/* Method name */
	intptr_t	 spm_offset;		/* Offset ($pcc relative) */
	intptr_t	 spm_index_offset;	/* Offset of callee variable */
};

/*
 * List of methods provided by a sandbox.  Sandbox method numbers (and
 * by extension vtable indexs) are defined by method position in the
 * spms_methods array.
 */
struct sandbox_provided_methods {
	char				*spms_class;	/* Class name */
	size_t				 spms_nmethods;	/* Number of methods */
	struct sandbox_provided_method	*spms_methods;	/* Array of methods */
};

/*
 * Description of a method required by a sandbox to be called by ccall.
 */
struct sandbox_required_method {
	char		*srm_class;		/* Class name */
	char		*srm_method;		/* Method name */
	intptr_t	 srm_index_offset;	/* Offset of caller variable */
	intptr_t	 srm_method_number;	/* Method number */
	bool		 srm_resolved;		/* Resolved? */
};

/*
 * List of methods required by a sandbox (or program).  Sandbox objects
 * must not be created until all symbols are resolved.
 */
struct sandbox_required_methods {
	size_t				 srms_nmethods;	/* Number of methods */
	size_t				 srms_unresolved_methods; /* Number */
	struct sandbox_required_method	*srms_methods;	/* Array of methods */
};

int	sandbox_parse_ccall_methods(int fd,
	    struct sandbox_provided_methods **provided_methodsp,
	    struct sandbox_required_methods **required_methodsp);
int	sandbox_resolve_methods(
	    struct sandbox_provided_methods *provided_methods,
	    struct sandbox_required_methods *required_methods);
void	sandbox_free_required_methods(
	    struct sandbox_required_methods *required_methods);
void	sandbox_free_provided_methods(
	    struct sandbox_provided_methods *provided_methods);
void	sandbox_warn_unresolved_methods(
	    struct sandbox_required_methods *required_methods);

int	sandbox_create_method_vtable(__capability void * codecap,
	    struct sandbox_provided_methods *provided_methods,
	    void __capability * __capability * __capability *vtablep);
int	sandbox_set_provided_method_variables(__capability void *datacap,
	    struct sandbox_provided_methods *provided_methods);
int	sandbox_set_required_method_variables(__capability void *datacap,
	    struct sandbox_required_methods *required_methods);

#endif /* __SANDBOX_METHODS_H__ */
