/*
 * Copyright (c) 1993 Paul Kranenburg
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Paul Kranenburg.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	$Id: xbits.c,v 1.1 1993/10/16 21:52:37 pk Exp $
 */

/*
 * "Generic" byte-swap routines.
 */

#include <sys/param.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <ar.h>
#include <ranlib.h>
#include <a.out.h>
#include <stab.h>
#include <string.h>

#include "ld.h"

void
swap_longs(lp, n)
int	n;
long	*lp;
{
	for (; n > 0; n--, lp++)
		*lp = md_swap_long(*lp);
}

void
swap_symbols(s, n)
struct nlist *s;
int n;
{
	for (; n; n--, s++) {
		s->n_un.n_strx = md_swap_long(s->n_un.n_strx);
		s->n_desc = md_swap_short(s->n_desc);
		s->n_value = md_swap_long(s->n_value);
	}
}

void
swap_zsymbols(s, n)
struct nzlist *s;
int n;
{
	for (; n; n--, s++) {
		s->nz_strx = md_swap_long(s->nz_strx);
		s->nz_desc = md_swap_short(s->nz_desc);
		s->nz_value = md_swap_long(s->nz_value);
		s->nz_size = md_swap_long(s->nz_size);
	}
}


void
swap_ranlib_hdr(rlp, n)
struct ranlib *rlp;
int n;
{
	for (; n; n--, rlp++) {
		rlp->ran_un.ran_strx = md_swap_long(rlp->ran_un.ran_strx);
		rlp->ran_off = md_swap_long(rlp->ran_off);
	}
}

void
swap_link_dynamic(dp)
struct link_dynamic *dp;
{
	dp->ld_version = md_swap_long(dp->ld_version);
	dp->ldd = (struct ld_debug *)md_swap_long((long)dp->ldd);
	dp->ld_un.ld_2 = (struct link_dynamic_2 *)md_swap_long((long)dp->ld_un.ld_2);
	dp->ld_entry = (struct ld_entry *)md_swap_long((long)dp->ld_entry);
}

void
swap_link_dynamic_2(ldp)
struct link_dynamic_2 *ldp;
{
	swap_longs((long *)ldp, sizeof(*ldp)/sizeof(long));
}

void
swap_ld_debug(lddp)
struct ld_debug	*lddp;
{
	swap_longs((long *)lddp, sizeof(*lddp)/sizeof(long));
}

void
swapin_link_object(lop, n)
struct link_object *lop;
int n;
{
	unsigned long	bits;

	for (; n; n--, lop++) {
		lop->lo_name = md_swap_long(lop->lo_name);
		lop->lo_major = md_swap_short(lop->lo_major);
		lop->lo_minor = md_swap_short(lop->lo_minor);
		lop->lo_next = md_swap_long(lop->lo_next);
		bits = ((unsigned long *)lop)[1];
		lop->lo_library = ((bits >> 24) & 1);
	}
}

void
swapout_link_object(lop, n)
struct link_object *lop;
int n;
{
	unsigned long	bits;

	for (; n; n--, lop++) {
		lop->lo_name = md_swap_long(lop->lo_name);
		lop->lo_major = md_swap_short(lop->lo_major);
		lop->lo_minor = md_swap_short(lop->lo_minor);
		lop->lo_next = md_swap_long(lop->lo_next);
		bits = (unsigned long)(lop->lo_library) << 24;
		((unsigned long *)lop)[1] = bits;
	}
}

void
swap_rrs_hash(fsp, n)
struct rrs_hash	*fsp;
int n;
{
	for (; n; n--, fsp++) {
		fsp->rh_symbolnum = md_swap_long(fsp->rh_symbolnum);
		fsp->rh_next = md_swap_long(fsp->rh_next);
	}
}

