/*
 * Copyright (c) 1996,1999 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

/*
 * $Id: irs_p.h,v 1.8 1999/01/08 19:24:42 vixie Exp $
 */

#ifndef _IRS_P_H_INCLUDED
#define _IRS_P_H_INCLUDED

#include <stdio.h>

#include "pathnames.h"

#define IRS_SV_MAXALIASES	35

struct lcl_sv {
	FILE *		fp;
	char		line[BUFSIZ+1];
	struct servent	serv;
	char *		serv_aliases[IRS_SV_MAXALIASES];
};

#define	irs_nul_ng	__irs_nul_ng
#define	map_v4v6_address __map_v4v6_address
#define	make_group_list	__make_group_list
#define	irs_lclsv_fnxt	__irs_lclsv_fnxt

extern void		map_v4v6_address(const char *src, char *dst);
extern int		make_group_list(struct irs_gr *, const char *,
					gid_t, gid_t *, int *);
extern struct irs_ng *	irs_nul_ng(struct irs_acc *);
extern struct servent * irs_lclsv_fnxt(struct lcl_sv *);

#endif
