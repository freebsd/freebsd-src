/*
 * Copyright (C) 1996 Peter Wemm <peter@freebsd.org>.
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
 * $Id$
 */

/*
 * This file is for FreeBSD-2.2 and early 3.0 that have a 4.9.4-P1 derived
 * resolver in the libc.  It provides aliases for functions that have moved
 * when 4.9.5-P1 is imported in 3.0.
 *
 * I'll save everybody the trouble and say it now:  *THIS IS A HACK*!
 *
 * Yes, many of these are private functions to the resolver, but some are
 * needed as there is no other way to provide the functionality and they've
 * turned up all over the place. :-(
 */

#include <sys/types.h>
#include <sys/cdefs.h>

__weak_reference(sym_ston, __sym_ston);
__weak_reference(sym_ntos, __sym_ntos);
__weak_reference(sym_ntop, __sym_ntop);
__weak_reference(b64_ntop, __b64_ntop);
__weak_reference(b64_pton, __b64_pton);
__weak_reference(p_fqnname, __p_fqnname);
__weak_reference(p_secstodate, __p_secstodate);
__weak_reference(dn_count_labels, __dn_count_labels);
__weak_reference(dn_comp, __dn_comp);
__weak_reference(res_send, __res_send);
__weak_reference(_res_close, __res_close);
#ifdef BIND_RES_POSIX3		/* Last minute change between 4.9.5 and -P1 */
__weak_reference(dn_expand, __dn_expand);
__weak_reference(res_init, __res_init);
__weak_reference(res_query, __res_query);
__weak_reference(res_search, __res_search);
__weak_reference(res_querydomain, __res_querydomain);
__weak_reference(res_mkquery, __res_mkquery);
#endif
