/*
 * Copyright (c) 1996 by Internet Software Consortium.
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
 * $Id: dns_p.h,v 1.7 1996/10/25 07:22:59 vixie Exp $
 */

#ifndef _DNS_P_H_INCLUDED
#define	_DNS_P_H_INCLUDED

/*
 * Object state.
 */
struct dns_p {
	void	*hes_ctx;
};

/*
 * Methods.
 */

extern struct irs_gr *	irs_dns_gr __P((struct irs_acc *));
extern struct irs_pw *	irs_dns_pw __P((struct irs_acc *));
extern struct irs_sv *	irs_dns_sv __P((struct irs_acc *));
extern struct irs_pr *	irs_dns_pr __P((struct irs_acc *));
extern struct irs_ho *	irs_dns_ho __P((struct irs_acc *));
extern struct irs_nw *	irs_dns_nw __P((struct irs_acc *));

#endif /*_DNS_P_H_INCLUDED*/
