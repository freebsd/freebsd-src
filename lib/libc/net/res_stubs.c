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
 * $Id: res_stubs.c,v 1.2 1996/12/30 13:25:38 peter Exp $
 */

/*
 * This file is for FreeBSD-3.0 that has a bind-4.9.5-P1 derived
 * resolver in the libc.  It provides aliases for functions that
 * have moved since 4.9.4-P1.
 *
 * I'll save everybody the trouble and say it now:  *THIS IS A HACK*!
 *
 * Yes, many of these are private functions to the resolver, but some are
 * needed as there is no other way to provide the functionality and they've
 * turned up all over the place. :-(
 */

#include <sys/types.h>
#include <sys/cdefs.h>

__weak_reference(__sym_ston, sym_ston);
__weak_reference(__sym_ntos, sym_ntos);
__weak_reference(__sym_ntop, sym_ntop);
__weak_reference(__fp_resstat, fp_resstat);
__weak_reference(__p_query, p_query);
__weak_reference(__p_fqnname, p_fqnname);
__weak_reference(__p_secstodate, p_secstodate);
__weak_reference(__dn_count_labels, dn_count_labels);
__weak_reference(__dn_comp, dn_comp);
__weak_reference(__res_send, res_send);
__weak_reference(__res_close, _res_close);
#ifdef BIND_RES_POSIX3
__weak_reference(__dn_expand, dn_expand);
__weak_reference(__res_init, res_init);
__weak_reference(__res_query, res_query);
__weak_reference(__res_search, res_search);
__weak_reference(__res_querydomain, res_querydomain);
__weak_reference(__res_mkquery, res_mkquery);
#endif
