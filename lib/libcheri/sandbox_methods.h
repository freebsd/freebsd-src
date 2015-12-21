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

struct sandbox_provided_classes;
struct sandbox_required_methods;

int	sandbox_parse_ccall_methods(int fd,
	    struct sandbox_provided_classes **provided_classesp,
	    struct sandbox_required_methods **required_methodsp);
int	sandbox_resolve_methods(
	    struct sandbox_provided_classes *provided_classes,
	    struct sandbox_required_methods *required_methods);
void	sandbox_free_required_methods(
	    struct sandbox_required_methods *required_methods);
void	sandbox_free_provided_classes(
	    struct sandbox_provided_classes *provided_classes);
size_t	sandbox_get_unresolved_methods(
	    struct sandbox_required_methods *required_methods);
void	sandbox_warn_unresolved_methods(
	    struct sandbox_required_methods *required_methods);
__capability vm_offset_t *sandbox_make_vtable(void *datacap, const char *class,
	    struct sandbox_provided_classes *provided_classes);

int	sandbox_set_required_method_variables(__capability void *datacap,
	    struct sandbox_required_methods *required_methods);

#endif /* __SANDBOX_METHODS_H__ */
