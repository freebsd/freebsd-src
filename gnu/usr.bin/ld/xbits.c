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
 *    derived from this software without specific prior written permission
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
 *	$Id: xbits.c,v 1.2 1993/11/09 04:19:08 paul Exp $
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
swap__dynamic(dp)
struct _dynamic *dp;
{
	dp->d_version = md_swap_long(dp->d_version);
	dp->d_debug = (struct so_debug *)md_swap_long((long)dp->d_debug);
	dp->d_un.d_sdt = (struct section_dispatch_table *)
				md_swap_long((long)dp->d_un.d_sdt);
	dp->d_entry = (struct ld_entry *)md_swap_long((long)dp->d_entry);
}

void
swap_section_dispatch_table(sdp)
struct section_dispatch_table *sdp;
{
	swap_longs((long *)sdp, sizeof(*sdp)/sizeof(long));
}

void
swap_so_debug(ddp)
struct so_debug	*ddp;
{
	swap_longs((long *)ddp, sizeof(*ddp)/sizeof(long));
}

void
swapin_sod(sodp, n)
struct sod *sodp;
int n;
{
	unsigned long	bits;

	for (; n; n--, sodp++) {
		sodp->sod_name = md_swap_long(sodp->sod_name);
		sodp->sod_major = md_swap_short(sodp->sod_major);
		sodp->sod_minor = md_swap_short(sodp->sod_minor);
		sodp->sod_next = md_swap_long(sodp->sod_next);
		bits = ((unsigned long *)sodp)[1];
		sodp->sod_library = ((bits >> 24) & 1);
	}
}

void
swapout_sod(sodp, n)
struct sod *sodp;
int n;
{
	unsigned long	bits;

	for (; n; n--, sodp++) {
		sodp->sod_name = md_swap_long(sodp->sod_name);
		sodp->sod_major = md_swap_short(sodp->sod_major);
		sodp->sod_minor = md_swap_short(sodp->sod_minor);
		sodp->sod_next = md_swap_long(sodp->sod_next);
		bits = (unsigned long)(sodp->sod_library) << 24;
		((unsigned long *)sodp)[1] = bits;
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

