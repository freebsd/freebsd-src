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
 * $Id: irs.h,v 8.6 2001/05/29 05:47:00 marka Exp $
 */

#ifndef _IRS_H_INCLUDED
#define _IRS_H_INCLUDED

#include <sys/types.h>

#include <arpa/nameser.h>

#include <grp.h>
#include <netdb.h>
#include <resolv.h>
#include <pwd.h>
#include <netgroup.h>

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
	struct __res_state * (*res_get) __P((struct irs_gr *));
	void		(*res_set) __P((struct irs_gr *, res_state,
					void (*)(void *)));
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
	struct __res_state * (*res_get) __P((struct irs_pw *));
	void		(*res_set) __P((struct irs_pw *, res_state,
					void (*)(void *)));
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
	struct __res_state * (*res_get) __P((struct irs_sv *));
	void		(*res_set) __P((struct irs_sv *, res_state,
					void (*)(void *)));
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
	struct __res_state * (*res_get) __P((struct irs_pr *));
	void		(*res_set) __P((struct irs_pr *, res_state,
					void (*)(void *)));
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
	struct __res_state * (*res_get) __P((struct irs_ho *));
	void		(*res_set) __P((struct irs_ho *, res_state,
					void (*)(void *)));
	struct addrinfo *(*addrinfo) __P((struct irs_ho *, const char *,
					  const struct addrinfo *));
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
	struct __res_state * (*res_get) __P((struct irs_nw *));
	void		(*res_set) __P((struct irs_nw *, res_state,
					void (*)(void *)));
};

/*
 * This is the netgroups map class.
 */
struct irs_ng {
	void *		private;
	void		(*close) __P((struct irs_ng *));
	int		(*next) __P((struct irs_ng *, const char **,
				     const char **, const char **));
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
	struct __res_state * (*res_get) __P((struct irs_acc *));
	void		(*res_set) __P((struct irs_acc *, res_state,
					void (*)(void *)));
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
#define	irs_irp_acc	__irs_irp_acc

/*
 * Externs.
 */
extern struct irs_acc *	irs_gen_acc __P((const char *options, 
					 const char *conf_file));
extern struct irs_acc *	irs_lcl_acc __P((const char *options));
extern struct irs_acc *	irs_dns_acc __P((const char *options));
extern struct irs_acc *	irs_nis_acc __P((const char *options));
extern struct irs_acc *	irs_irp_acc __P((const char *options));

extern void		irs_destroy(void);

/*
 * These forward declarations are for the semi-private functions in
 * the get*.c files. Each of these funcs implements the real get*
 * functionality and the standard versions are just wrappers that
 * call these. Apart from the wrappers, only irpd is expected to
 * call these directly, hence these decls are put here and not in
 * the /usr/include replacements.
 */

struct net_data;			/* forward */

/*
 * net_data_create gets a singleton net_data object.  net_data_init
 * creates as many net_data objects as times it is called.  Clients using
 * the default interface will use net_data_create by default.  Servers will
 * probably want net_data_init (one call per client)
 */
struct net_data *net_data_create(const char *conf_file);
struct net_data *net_data_init(const char *conf_file);
void		net_data_destroy(void *p);
	
extern struct group    *getgrent_p __P((struct net_data *net_data));
extern struct group    *getgrnam_p __P((const char *name,
					struct net_data *net_data));
extern struct group    *getgrgid_p __P((gid_t gid,
					struct net_data *net_data));
extern int 		setgroupent_p __P((int stayopen,
					   struct net_data *net_data));
extern void 		endgrent_p __P((struct net_data *net_data));
extern int		getgrouplist_p __P((const char *name,
					    gid_t basegid,
					    gid_t *groups,
					    int *ngroups,
					    struct net_data *net_data));

#ifdef SETGRENT_VOID
extern void 		setgrent_p __P((struct net_data *net_data));
#else
extern int 		setgrent_p __P((struct net_data *net_data));
#endif

extern struct hostent 	*gethostbyname_p __P((const char *name,
					      struct net_data *net_data));
extern struct hostent 	*gethostbyname2_p __P((const char *name, int af,
					       struct net_data *net_data));
extern struct hostent 	*gethostbyaddr_p __P((const char *addr, int len,
					      int af,
					      struct net_data *net_data));
extern struct hostent 	*gethostent_p __P((struct net_data *net_data));
extern void 		sethostent_p __P((int stayopen,
					  struct net_data *net_data));
extern void 		endhostent_p __P((struct net_data *net_data));
extern struct hostent 	*getipnodebyname_p __P((const char *name, int af,
					       int flags, int *errp,
					       struct net_data *net_data));
extern struct hostent 	*getipnodebyaddr_p __P((const void *addr, size_t len,
					      int af, int *errp,
					      struct net_data *net_data));

extern struct netent 	*getnetent_p __P((struct net_data *net_data));
extern struct netent 	*getnetbyname_p __P((const char *name,
					     struct net_data *net_data));
extern struct netent 	*getnetbyaddr_p __P((unsigned long net, int type,
					     struct net_data *net_data));
extern void		setnetent_p __P((int stayopen,
					 struct net_data *net_data));
extern void		endnetent_p __P((struct net_data *net_data));

extern void		setnetgrent_p __P((const char *netgroup,
					   struct net_data *net_data));
extern void		endnetgrent_p __P((struct net_data *net_data));
extern int		innetgr_p __P((const char *netgroup,
				       const char *host,
				       const char *user,
				       const char *domain,
				       struct net_data *net_data));
extern int		getnetgrent_p __P((const char **host, const char **user,
					   const char **domain,
					   struct net_data *net_data));

extern struct protoent  *getprotoent_p __P((struct net_data *net_data));
extern struct protoent  *getprotobyname_p __P((const char *name,
					       struct net_data *net_data));
extern struct protoent	*getprotobynumber_p __P((int proto,
						 struct net_data *net_data));
extern void		setprotoent_p __P((int stayopen,
					   struct net_data *net_data));
extern void		endprotoent_p __P((struct net_data *net_data));


extern struct passwd 	*getpwent_p __P((struct net_data *net_data));
extern struct passwd 	*getpwnam_p __P((const char *name,
					 struct net_data *net_data));
extern struct passwd 	*getpwuid_p __P((uid_t uid,
					 struct net_data *net_data));
extern int		setpassent_p __P((int stayopen,
					  struct net_data *net_data));
extern void		endpwent_p __P((struct net_data *net_data));

#ifdef SETPWENT_VOID
extern void		setpwent_p __P((struct net_data *net_data));
#else
extern int		setpwent_p __P((struct net_data *net_data));
#endif

extern struct servent 	*getservent_p __P((struct net_data *net_data));
extern struct servent 	*getservbyname_p __P((const char *name,
					      const char *proto,
					      struct net_data *net_data));
extern struct servent 	*getservbyport_p __P((int port, const char *proto,
					      struct net_data *net_data));
extern void		setservent_p __P((int stayopen,
					  struct net_data *net_data));
extern void		endservent_p __P((struct net_data *net_data));

#endif /*_IRS_H_INCLUDED*/
