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
 * $Id: irs.h,v 8.1 1997/12/04 04:55:19 halley Exp $
 */

#ifndef _IRS_H_INCLUDED
#define _IRS_H_INCLUDED

#include <sys/types.h>

#include <netdb.h>
#include <grp.h>
#include <pwd.h>

/*
 * This is the group map class.
 */
struct irs_gr {
	void *		private;
	void		(*close) __P((struct irs_gr *));
	struct group *	(*next) __P((struct irs_gr *));
	struct group *	(*byname) __P((struct irs_gr *, const char *));
	struct group *	(*bygid) __P((struct irs_gr *, gid_t));
	int		(*list) __P((struct irs_gr *, const char *,
				     gid_t, gid_t *, int *));
	void		(*rewind) __P((struct irs_gr *));
	void		(*minimize) __P((struct irs_gr *));
};

/*
 * This is the password map class.
 */
struct irs_pw {
	void *		private;
	void		(*close) __P((struct irs_pw *));
	struct passwd *	(*next) __P((struct irs_pw *));
	struct passwd *	(*byname) __P((struct irs_pw *, const char *));
	struct passwd *	(*byuid) __P((struct irs_pw *, uid_t));
	void		(*rewind) __P((struct irs_pw *));
	void		(*minimize) __P((struct irs_pw *));
};

/*
 * This is the service map class.
 */
struct irs_sv {
	void *		private;
	void		(*close) __P((struct irs_sv *));
	struct servent *(*byname) __P((struct irs_sv *,
				       const char *, const char *));
	struct servent *(*byport) __P((struct irs_sv *, int, const char *));
	struct servent *(*next) __P((struct irs_sv *));
	void		(*rewind) __P((struct irs_sv *));
	void		(*minimize) __P((struct irs_sv *));
};

/*
 * This is the protocols map class.
 */
struct irs_pr {
	void *		private;
	void		(*close) __P((struct irs_pr *));
	struct protoent	*(*byname) __P((struct irs_pr *, const char *));
	struct protoent	*(*bynumber) __P((struct irs_pr *, int));
	struct protoent	*(*next) __P((struct irs_pr *));
	void		(*rewind) __P((struct irs_pr *));
	void		(*minimize) __P((struct irs_pr *));
};

/*
 * This is the hosts map class.
 */
struct irs_ho {
	void *		private;
	void		(*close) __P((struct irs_ho *));
	struct hostent *(*byname) __P((struct irs_ho *, const char *));
	struct hostent *(*byname2) __P((struct irs_ho *, const char *, int));
	struct hostent *(*byaddr) __P((struct irs_ho *,
				       const void *, int, int));
	struct hostent *(*next) __P((struct irs_ho *));
	void		(*rewind) __P((struct irs_ho *));
	void		(*minimize) __P((struct irs_ho *));
};

/*
 * This is the networks map class.
 */
struct irs_nw {
	void *		private;
	void		(*close) __P((struct irs_nw *));
	struct nwent *	(*byname) __P((struct irs_nw *, const char *, int));
	struct nwent *	(*byaddr) __P((struct irs_nw *, void *, int, int));
	struct nwent *	(*next) __P((struct irs_nw *));
	void		(*rewind) __P((struct irs_nw *));
	void		(*minimize) __P((struct irs_nw *));
};

/*
 * This is the netgroups map class.
 */
struct irs_ng {
	void *		private;
	void		(*close) __P((struct irs_ng *));
	int		(*next) __P((struct irs_ng *, char **, char **,
				     char **));
	int		(*test) __P((struct irs_ng *, const char *,
				     const char *, const char *,
				     const char *));
	void		(*rewind) __P((struct irs_ng *, const char *));
	void		(*minimize) __P((struct irs_ng *));
};

/*
 * This is the generic map class, which copies the front of all others.
 */
struct irs_map {
	void *		private;
	void		(*close) __P((void *));
};

/*
 * This is the accessor class.  It contains pointers to all of the
 * initializers for the map classes for a particular accessor.
 */
struct irs_acc {
	void *		private;
	void		(*close) __P((struct irs_acc *));
	struct irs_gr *	(*gr_map) __P((struct irs_acc *));
	struct irs_pw *	(*pw_map) __P((struct irs_acc *));
	struct irs_sv *	(*sv_map) __P((struct irs_acc *));
	struct irs_pr *	(*pr_map) __P((struct irs_acc *));
	struct irs_ho *	(*ho_map) __P((struct irs_acc *));
	struct irs_nw *	(*nw_map) __P((struct irs_acc *));
	struct irs_ng *	(*ng_map) __P((struct irs_acc *));
};

/*
 * This is because the official definition of "struct netent" has no
 * concept of CIDR even though it allows variant address families (on
 * output but not input).  The compatibility stubs convert the structs
 * below into "struct netent"'s.
 */
struct nwent {
	char		*n_name;	/* official name of net */
	char		**n_aliases;	/* alias list */
	int		n_addrtype;	/* net address type */
	void		*n_addr;	/* network address */
	int		n_length;	/* address length, in bits */
};

/*
 * Hide external function names from POSIX.
 */
#define	irs_gen_acc	__irs_gen_acc
#define	irs_lcl_acc	__irs_lcl_acc
#define	irs_dns_acc	__irs_dns_acc
#define	irs_nis_acc	__irs_nis_acc

/*
 * Externs.
 */
extern struct irs_acc *	irs_gen_acc __P((const char *options));
extern struct irs_acc *	irs_lcl_acc __P((const char *options));
extern struct irs_acc *	irs_dns_acc __P((const char *options));
extern struct irs_acc *	irs_nis_acc __P((const char *options));

#endif /*_IRS_H_INCLUDED*/
