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
 * $Id: irs_data.h,v 1.12 1999/01/18 07:46:55 vixie Exp $
 */

#ifndef __BIND_NOSTATIC

#define	net_data_init		__net_data_init

struct net_data {
	struct irs_acc *	irs;

	struct irs_gr *		gr;
	struct irs_pw *		pw;
	struct irs_sv *		sv;
	struct irs_pr *		pr;
	struct irs_ho *		ho;
	struct irs_nw *		nw;
	struct irs_ng *		ng;

	struct group *		gr_last;
	struct passwd *		pw_last;
	struct servent *	sv_last;
	struct protoent *	pr_last;
	struct netent *		nw_last; /* should have been ne_last */
	struct nwent *		nww_last;
	struct hostent *	ho_last;

	unsigned int		gr_stayopen :1;
	unsigned int		pw_stayopen :1;
	unsigned int		sv_stayopen :1;
	unsigned int		pr_stayopen :1;
	unsigned int		ho_stayopen :1;
	unsigned int		nw_stayopen :1;

	void *			nw_data;
	void *			ho_data;

	struct __res_state *	res;	/* for gethostent.c */

};

extern struct net_data *	net_data_init(const char *conf_file);
extern void			net_data_minimize(struct net_data *);

#endif /*__BIND_NOSTATIC*/
