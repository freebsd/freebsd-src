/*
 * Copyright (c) 1996-1999 by Internet Software Consortium.
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

#if !defined(LINT) && !defined(CODECENTER)
static const char rcsid[] = "$Id: gethostent.c,v 1.29 2001/05/29 05:48:44 marka Exp $";
#endif

/* Imports */

#include "port_before.h"

#if !defined(__BIND_NOSTATIC)

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <netdb.h>
#include <resolv.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <irs.h>
#include <isc/memcluster.h>

#include "port_after.h"

#include "irs_p.h"
#include "irs_data.h"

/* Definitions */

struct pvt {
	char *		aliases[1];
	char *		addrs[2];
	char		addr[NS_IN6ADDRSZ];
	char		name[NS_MAXDNAME + 1];
	struct hostent	host;
};

/* Forward */

static struct net_data *init(void);
static void		freepvt(struct net_data *);
static struct hostent  *fakeaddr(const char *, int, struct net_data *);


/* Public */

struct hostent *
gethostbyname(const char *name) {
	struct net_data *net_data = init();

	return (gethostbyname_p(name, net_data));
}

struct hostent *
gethostbyname2(const char *name, int af) {
	struct net_data *net_data = init();

	return (gethostbyname2_p(name, af, net_data));
}

struct hostent *
gethostbyaddr(const char *addr, int len, int af) {
	struct net_data *net_data = init();

	return (gethostbyaddr_p(addr, len, af, net_data));
}

struct hostent *
gethostent() {
	struct net_data *net_data = init();

	return (gethostent_p(net_data));
}

void
sethostent(int stayopen) {
	struct net_data *net_data = init();
	sethostent_p(stayopen, net_data);
}


void
endhostent() {
	struct net_data *net_data = init();
	endhostent_p(net_data);
}

/* Shared private. */

struct hostent *
gethostbyname_p(const char *name, struct net_data *net_data) {
	struct hostent *hp;

	if (!net_data)
		return (NULL);

	if (net_data->res->options & RES_USE_INET6) {
		hp = gethostbyname2_p(name, AF_INET6, net_data);
		if (hp)
			return (hp);
	}
	return (gethostbyname2_p(name, AF_INET, net_data));
}

struct hostent *
gethostbyname2_p(const char *name, int af, struct net_data *net_data) {
	struct irs_ho *ho;
	char tmp[NS_MAXDNAME];
	struct hostent *hp;
	const char *cp;
	char **hap;

	if (!net_data || !(ho = net_data->ho))
		return (NULL);
	if (net_data->ho_stayopen && net_data->ho_last &&
	    net_data->ho_last->h_addrtype == af) {
		if (ns_samename(name, net_data->ho_last->h_name) == 1)
			return (net_data->ho_last);
		for (hap = net_data->ho_last->h_aliases; hap && *hap; hap++)
			if (ns_samename(name, *hap) == 1)
				return (net_data->ho_last);
	}
	if (!strchr(name, '.') && (cp = res_hostalias(net_data->res, name,
						      tmp, sizeof tmp)))
		name = cp;
	if ((hp = fakeaddr(name, af, net_data)) != NULL)
		return (hp);
	net_data->ho_last = (*ho->byname2)(ho, name, af);
	if (!net_data->ho_stayopen)
		endhostent();
	return (net_data->ho_last);
}

struct hostent *
gethostbyaddr_p(const char *addr, int len, int af, struct net_data *net_data) {
	struct irs_ho *ho;
	char **hap;

	if (!net_data || !(ho = net_data->ho))
		return (NULL);
	if (net_data->ho_stayopen && net_data->ho_last &&
	    net_data->ho_last->h_length == len)
		for (hap = net_data->ho_last->h_addr_list;
		     hap && *hap;
		     hap++)
			if (!memcmp(addr, *hap, len))
				return (net_data->ho_last);
	net_data->ho_last = (*ho->byaddr)(ho, addr, len, af);
	if (!net_data->ho_stayopen)
		endhostent();
	return (net_data->ho_last);
}


struct hostent *
gethostent_p(struct net_data *net_data) {
	struct irs_ho *ho;
	struct hostent *hp;

	if (!net_data || !(ho = net_data->ho))
		return (NULL);
	while ((hp = (*ho->next)(ho)) != NULL &&
	       hp->h_addrtype == AF_INET6 &&
	       (net_data->res->options & RES_USE_INET6) == 0)
		continue;
	net_data->ho_last = hp;
	return (net_data->ho_last);
}


void
sethostent_p(int stayopen, struct net_data *net_data) {
	struct irs_ho *ho;

	if (!net_data || !(ho = net_data->ho))
		return;
	freepvt(net_data);
	(*ho->rewind)(ho);
	net_data->ho_stayopen = (stayopen != 0);
	if (stayopen == 0)
		net_data_minimize(net_data);
}

void
endhostent_p(struct net_data *net_data) {
	struct irs_ho *ho;

	if ((net_data != NULL) && ((ho = net_data->ho) != NULL))
		(*ho->minimize)(ho);
}

#ifndef IN6_IS_ADDR_V4COMPAT
static const unsigned char in6addr_compat[12] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
#define IN6_IS_ADDR_V4COMPAT(x) (!memcmp((x)->s6_addr, in6addr_compat, 12) && \
				 ((x)->s6_addr[12] != 0 || \
				  (x)->s6_addr[13] != 0 || \
				  (x)->s6_addr[14] != 0 || \
				   ((x)->s6_addr[15] != 0 && \
				    (x)->s6_addr[15] != 1)))
#endif
#ifndef IN6_IS_ADDR_V4MAPPED
#define IN6_IS_ADDR_V4MAPPED(x) (!memcmp((x)->s6_addr, in6addr_mapped, 12))
#endif

static const unsigned char in6addr_mapped[12] = {
	0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0xff, 0xff };

static int scan_interfaces(int *, int *);
static struct hostent *copyandmerge(struct hostent *, struct hostent *, int, int *);

/*
 *	Public functions
 */

/*
 *	AI_V4MAPPED + AF_INET6
 *	If no IPv6 address then a query for IPv4 and map returned values.
 *
 *	AI_ALL + AI_V4MAPPED + AF_INET6
 *	Return IPv6 and IPv4 mapped.
 *
 *	AI_ADDRCONFIG
 *	Only return IPv6 / IPv4 address if there is an interface of that
 *	type active.
 */

struct hostent *
getipnodebyname(const char *name, int af, int flags, int *error_num) {
	int have_v4 = 1, have_v6 = 1;
	struct in_addr in4;
	struct in6_addr in6;
	struct hostent he, *he1 = NULL, *he2 = NULL, *he3;
	int v4 = 0, v6 = 0;
	struct net_data *net_data = init();
	u_long options;
	int tmp_err;

	if (net_data == NULL) {
		*error_num = NO_RECOVERY;
		return (NULL);
	}

	/* If we care about active interfaces then check. */
	if ((flags & AI_ADDRCONFIG) != 0)
		if (scan_interfaces(&have_v4, &have_v6) == -1) {
			*error_num = NO_RECOVERY;
			return (NULL);
		}

	/* Check for literal address. */
	if ((v4 = inet_pton(AF_INET, name, &in4)) != 1)
		v6 = inet_pton(AF_INET6, name, &in6);

	/* Impossible combination? */
	 
	if ((af == AF_INET6 && (flags & AI_V4MAPPED) == 0 && v4 == 1) ||
	    (af == AF_INET && v6 == 1) ||
	    (have_v4 == 0 && v4 == 1) ||
	    (have_v6 == 0 && v6 == 1) ||
	    (have_v4 == 0 && af == AF_INET) ||
	    (have_v6 == 0 && af == AF_INET6)) {
		*error_num = HOST_NOT_FOUND;
		return (NULL);
	}

	/* Literal address? */
	if (v4 == 1 || v6 == 1) {
		char *addr_list[2];
		char *aliases[1];

		DE_CONST(name, he.h_name);
		he.h_addr_list = addr_list;
		he.h_addr_list[0] = (v4 == 1) ? (char *)&in4 : (char *)&in6;
		he.h_addr_list[1] = NULL;
		he.h_aliases = aliases;
		he.h_aliases[0] = NULL;
		he.h_length = (v4 == 1) ? INADDRSZ : IN6ADDRSZ;
		he.h_addrtype = (v4 == 1) ? AF_INET : AF_INET6;
		return (copyandmerge(&he, NULL, af, error_num));
	}

	options = net_data->res->options;
	net_data->res->options &= ~RES_USE_INET6;

	tmp_err = NO_RECOVERY;
	if (have_v6 && af == AF_INET6) {
		he2 = gethostbyname2_p(name, AF_INET6, net_data);
		if (he2 != NULL) {
			he1 = copyandmerge(he2, NULL, af, error_num);
			if (he1 == NULL)
				return (NULL);
			he2 = NULL;
		} else {
			tmp_err = net_data->res->res_h_errno;
		}
	}

	if (have_v4 &&
	    ((af == AF_INET) ||
	     (af == AF_INET6 && (flags & AI_V4MAPPED) != 0 &&
	      (he1 == NULL || (flags & AI_ALL) != 0)))) {
		he2 = gethostbyname2_p(name, AF_INET, net_data);
		if (he1 == NULL && he2 == NULL) {
			*error_num = net_data->res->res_h_errno;
			return (NULL);
		} 
	} else
		*error_num = tmp_err;

	net_data->res->options = options;

	he3 = copyandmerge(he1, he2, af, error_num);

	if (he1 != NULL)
		freehostent(he1);
	return (he3);
}

struct hostent *
getipnodebyaddr(const void *src, size_t len, int af, int *error_num) {
	struct hostent *he1, *he2;
	struct net_data *net_data = init();

	/* Sanity Checks. */
	if (src == NULL) {
		*error_num = NO_RECOVERY;
		return (NULL);
	}
		
	switch (af) {
	case AF_INET:
		if (len != INADDRSZ) {
			*error_num = NO_RECOVERY;
			return (NULL);
		}
		break;
	case AF_INET6:
		if (len != IN6ADDRSZ) {
			*error_num = NO_RECOVERY;
			return (NULL);
		}
		break;
	default:
		*error_num = NO_RECOVERY;
		return (NULL);
	}

	/*
	 * Lookup IPv4 and IPv4 mapped/compatible addresses
	 */
	if ((af == AF_INET6 &&
	     IN6_IS_ADDR_V4COMPAT((const struct in6_addr *)src)) ||
	    (af == AF_INET6 &&
	     IN6_IS_ADDR_V4MAPPED((const struct in6_addr *)src)) ||
	    (af == AF_INET)) {
		const char *cp = src;

		if (af == AF_INET6)
			cp += 12;
		he1 = gethostbyaddr_p(cp, 4, AF_INET, net_data);
		if (he1 == NULL) {
			*error_num = net_data->res->res_h_errno;
			return (NULL);
		}
		he2 = copyandmerge(he1, NULL, af, error_num);
		if (he2 == NULL)
			return (NULL);
		/*
		 * Restore original address if mapped/compatible.
		 */
		if (af == AF_INET6)
			memcpy(he1->h_addr, src, len);
		return (he2);
	}

	/*
	 * Lookup IPv6 address.
	 */
	if (memcmp((const struct in6_addr *)src, &in6addr_any, 16) == 0) {
		*error_num = HOST_NOT_FOUND;
		return (NULL);
	}

	he1 = gethostbyaddr_p(src, 16, AF_INET6, net_data);
	if (he1 == NULL) {
		*error_num = net_data->res->res_h_errno;
		return (NULL);
	}
	return (copyandmerge(he1, NULL, af, error_num));
}

void
freehostent(struct hostent *he) {
	char **cpp;
	int names = 1;
	int addresses = 1;

	memput(he->h_name, strlen(he->h_name) + 1);

	cpp = he->h_addr_list;
	while (*cpp != NULL) {
		memput(*cpp, (he->h_addrtype == AF_INET) ?
			     INADDRSZ : IN6ADDRSZ);
		*cpp = NULL;
		cpp++;
		addresses++;
	}

	cpp = he->h_aliases;
	while (*cpp != NULL) {
		memput(*cpp, strlen(*cpp) + 1);
		cpp++;
		names++;
	}

	memput(he->h_aliases, sizeof(char *) * (names));
	memput(he->h_addr_list, sizeof(char *) * (addresses));
	memput(he, sizeof *he);
}

/*
 * Private
 */

/*
 * Scan the interface table and set have_v4 and have_v6 depending
 * upon whether there are IPv4 and IPv6 interface addresses.
 *
 * Returns:
 *	0 on success
 *	-1 on failure.
 */

static int
scan_interfaces(int *have_v4, int *have_v6) {
#ifndef SIOCGLIFCONF
/* map new to old */
#define SIOCGLIFCONF SIOCGIFCONF
#define lifc_len ifc_len
#define lifc_buf ifc_buf
	struct ifconf lifc;
#else
#define SETFAMILYFLAGS
	struct lifconf lifc;
#endif

#ifndef SIOCGLIFADDR
/* map new to old */
#define SIOCGLIFADDR SIOCGIFADDR
#endif

#ifndef SIOCGLIFFLAGS
#define SIOCGLIFFLAGS SIOCGIFFLAGS
#define lifr_addr ifr_addr
#define lifr_name ifr_name
#define lifr_flags ifr_flags
#define ss_family sa_family
	struct ifreq lifreq;
#else
	struct lifreq lifreq;
#endif
	struct in_addr in4;
	struct in6_addr in6;
	char *buf = NULL, *cp, *cplim;
	static unsigned int bufsiz = 4095;
	int s, cpsize, n;

	/* Set to zero.  Used as loop terminators below. */
	*have_v4 = *have_v6 = 0;

	/* Get interface list from system. */
	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		goto err_ret;

	/*
	 * Grow buffer until large enough to contain all interface
	 * descriptions.
	 */
	for (;;) {
		buf = memget(bufsiz);
		if (buf == NULL)
			goto err_ret;
#ifdef SETFAMILYFLAGS
		lifc.lifc_family = AF_UNSPEC;
		lifc.lifc_flags = 0;
#endif
		lifc.lifc_len = bufsiz;
		lifc.lifc_buf = buf;
#ifdef IRIX_EMUL_IOCTL_SIOCGIFCONF
		/*
		 * This is a fix for IRIX OS in which the call to ioctl with
		 * the flag SIOCGIFCONF may not return an entry for all the
		 * interfaces like most flavors of Unix.
		 */
		if (emul_ioctl(&lifc) >= 0)
			break;
#else
		if ((n = ioctl(s, SIOCGLIFCONF, (char *)&lifc)) != -1) {
			/*
			 * Some OS's just return what will fit rather
			 * than set EINVAL if the buffer is too small
			 * to fit all the interfaces in.  If 
			 * lifc.lifc_len is too near to the end of the
			 * buffer we will grow it just in case and
			 * retry.
			 */
			if (lifc.lifc_len + 2 * sizeof(lifreq) < bufsiz)
				break;
		}
#endif
		if ((n == -1) && errno != EINVAL)
			goto err_ret;

		if (bufsiz > 1000000)
			goto err_ret;

		memput(buf, bufsiz);
		bufsiz += 4096;
	}

	/* Parse system's interface list. */
	cplim = buf + lifc.lifc_len;    /* skip over if's with big ifr_addr's */
	for (cp = buf;
	     (*have_v4 == 0 || *have_v6 == 0) && cp < cplim;
	     cp += cpsize) {
		memcpy(&lifreq, cp, sizeof lifreq);
#ifdef HAVE_SA_LEN
#ifdef FIX_ZERO_SA_LEN
		if (lifreq.lifr_addr.sa_len == 0)
			lifreq.lifr_addr.sa_len = 16;
#endif
#ifdef HAVE_MINIMUM_IFREQ
		cpsize = sizeof lifreq;
		if (lifreq.lifr_addr.sa_len > sizeof (struct sockaddr))
			cpsize += (int)lifreq.lifr_addr.sa_len -
				(int)(sizeof (struct sockaddr));
#else
		cpsize = sizeof lifreq.lifr_name + lifreq.lifr_addr.sa_len;
#endif /* HAVE_MINIMUM_IFREQ */
#elif defined SIOCGIFCONF_ADDR
		cpsize = sizeof lifreq;
#else
		cpsize = sizeof lifreq.lifr_name;
		/* XXX maybe this should be a hard error? */
		if (ioctl(s, SIOCGLIFADDR, (char *)&lifreq) < 0)
			continue;
#endif
		switch (lifreq.lifr_addr.ss_family) {
		case AF_INET:
			if (*have_v4 == 0) {
				memcpy(&in4,
				       &((struct sockaddr_in *)
				       &lifreq.lifr_addr)->sin_addr,
				       sizeof in4);
				if (in4.s_addr == INADDR_ANY)
					break;
				n = ioctl(s, SIOCGLIFFLAGS, (char *)&lifreq);
				if (n < 0)
					break;
				if ((lifreq.lifr_flags & IFF_UP) == 0)
					break;
				*have_v4 = 1;
			} 
			break;
		case AF_INET6:
			if (*have_v6 == 0) {
				memcpy(&in6,
				       &((struct sockaddr_in6 *)
				       &lifreq.lifr_addr)->sin6_addr, sizeof in6);
				if (memcmp(&in6, &in6addr_any, sizeof in6) == 0)
					break;
				n = ioctl(s, SIOCGLIFFLAGS, (char *)&lifreq);
				if (n < 0)
					break;
				if ((lifreq.lifr_flags & IFF_UP) == 0)
					break;
				*have_v6 = 1;
			}
			break;
		}
	}
	if (buf != NULL)
		memput(buf, bufsiz);
	close(s);
	/* printf("scan interface -> 4=%d 6=%d\n", *have_v4, *have_v6); */
	return (0);
 err_ret:
	if (buf != NULL)
		memput(buf, bufsiz);
	if (s != -1)
		close(s);
	/* printf("scan interface -> 4=%d 6=%d\n", *have_v4, *have_v6); */
	return (-1);
}

static struct hostent *
copyandmerge(struct hostent *he1, struct hostent *he2, int af, int *error_num) {
	struct hostent *he = NULL;
	int addresses = 1;	/* NULL terminator */
	int names = 1;		/* NULL terminator */
	int len = 0;
	char **cpp, **npp;

	/*
	 * Work out array sizes;
	 */
	if (he1 != NULL) {
		cpp = he1->h_addr_list;
		while (*cpp != NULL) {
			addresses++;
			cpp++;
		}
		cpp = he1->h_aliases;
		while (*cpp != NULL) {
			names++;
			cpp++;
		}
	}

	if (he2 != NULL) {
		cpp = he2->h_addr_list;
		while (*cpp != NULL) {
			addresses++;
			cpp++;
		}
		if (he1 == NULL) {
			cpp = he2->h_aliases;
			while (*cpp != NULL) {
				names++;
				cpp++;
			}
		}
	}

	if (addresses == 1) {
		*error_num = NO_ADDRESS;
		return (NULL);
	}

	he = memget(sizeof *he);
	if (he == NULL)
		goto no_recovery;

	he->h_addr_list = memget(sizeof(char *) * (addresses));
	if (he->h_addr_list == NULL)
		goto cleanup0;
	memset(he->h_addr_list, 0, sizeof(char *) * (addresses));

	/* copy addresses */
	npp = he->h_addr_list;
	if (he1 != NULL) {
		cpp = he1->h_addr_list;
		while (*cpp != NULL) {
			*npp = memget((af == AF_INET) ? INADDRSZ : IN6ADDRSZ);
			if (*npp == NULL)
				goto cleanup1;
			/* convert to mapped if required */
			if (af == AF_INET6 && he1->h_addrtype == AF_INET) {
				memcpy(*npp, in6addr_mapped,
				       sizeof in6addr_mapped);
				memcpy(*npp + sizeof in6addr_mapped, *cpp,
				       INADDRSZ);
			} else {
				memcpy(*npp, *cpp,
				       (af == AF_INET) ? INADDRSZ : IN6ADDRSZ);
			}
			cpp++;
			npp++;
		}
	}

	if (he2 != NULL) {
		cpp = he2->h_addr_list;
		while (*cpp != NULL) {
			*npp = memget((af == AF_INET) ? INADDRSZ : IN6ADDRSZ);
			if (*npp == NULL)
				goto cleanup1;
			/* convert to mapped if required */
			if (af == AF_INET6 && he2->h_addrtype == AF_INET) {
				memcpy(*npp, in6addr_mapped,
				       sizeof in6addr_mapped);
				memcpy(*npp + sizeof in6addr_mapped, *cpp,
				       INADDRSZ);
			} else {
				memcpy(*npp, *cpp,
				       (af == AF_INET) ? INADDRSZ : IN6ADDRSZ);
			}
			cpp++;
			npp++;
		}
	}

	he->h_aliases = memget(sizeof(char *) * (names));
	if (he->h_aliases == NULL)
		goto cleanup1;
	memset(he->h_aliases, 0, sizeof(char *) * (names));

	/* copy aliases */
	npp = he->h_aliases;
	cpp = (he1 != NULL) ? he1->h_aliases : he2->h_aliases;
	while (*cpp != NULL) {
		len = strlen (*cpp) + 1;
		*npp = memget(len);
		if (*npp == NULL)
			goto cleanup2;
		strcpy(*npp, *cpp);
		npp++;
		cpp++;
	}

	/* copy hostname */
	he->h_name = memget(strlen((he1 != NULL) ?
			    he1->h_name : he2->h_name) + 1);
	if (he->h_name == NULL)
		goto cleanup2;
	strcpy(he->h_name, (he1 != NULL) ? he1->h_name : he2->h_name);

	/* set address type and length */
	he->h_addrtype = af;
	he->h_length = (af == AF_INET) ? INADDRSZ : IN6ADDRSZ;
	return(he);

 cleanup2:
	cpp = he->h_aliases;
	while (*cpp != NULL) {
		memput(*cpp, strlen(*cpp) + 1);
		cpp++;
	}
	memput(he->h_aliases, sizeof(char *) * (names));

 cleanup1:
	cpp = he->h_addr_list;
	while (*cpp != NULL) {
		memput(*cpp, (af == AF_INET) ? INADDRSZ : IN6ADDRSZ);
		*cpp = NULL;
		cpp++;
	}
	memput(he->h_addr_list, sizeof(char *) * (addresses));

 cleanup0:
	memput(he, sizeof *he);

 no_recovery:
	*error_num = NO_RECOVERY;
	return (NULL);
}

static struct net_data *
init() {
	struct net_data *net_data;

	if (!(net_data = net_data_init(NULL)))
		goto error;
	if (!net_data->ho) {
		net_data->ho = (*net_data->irs->ho_map)(net_data->irs);
		if (!net_data->ho || !net_data->res) {
  error:
			errno = EIO;
			if (net_data && net_data->res)
				RES_SET_H_ERRNO(net_data->res, NETDB_INTERNAL);
			return (NULL);
		}
	
		(*net_data->ho->res_set)(net_data->ho, net_data->res, NULL);
	}
	
	return (net_data);
}

static void
freepvt(struct net_data *net_data) {
	if (net_data->ho_data) {
		free(net_data->ho_data);
		net_data->ho_data = NULL;
	}
}

static struct hostent *
fakeaddr(const char *name, int af, struct net_data *net_data) {
	struct pvt *pvt;

	freepvt(net_data);
	net_data->ho_data = malloc(sizeof (struct pvt));
	if (!net_data->ho_data) {
		errno = ENOMEM;
		RES_SET_H_ERRNO(net_data->res, NETDB_INTERNAL);
		return (NULL);
	}
	pvt = net_data->ho_data;
#ifndef __bsdi__
	/*
	 * Unlike its forebear(inet_aton), our friendly inet_pton() is strict
	 * in its interpretation of its input, and it will only return "1" if
	 * the input string is a formally valid(and thus unambiguous with
	 * respect to host names) internet address specification for this AF.
	 *
	 * This means "telnet 0xdeadbeef" and "telnet 127.1" are dead now.
	 */
	if (inet_pton(af, name, pvt->addr) != 1) {
#else
	/* BSDI XXX
	 * We put this back to inet_aton -- we really want the old behavior
	 * Long live 127.1...
	 */
	if ((af != AF_INET ||
	    inet_aton(name, (struct in_addr *)pvt->addr) != 1) &&
	    inet_pton(af, name, pvt->addr) != 1) {
#endif
		RES_SET_H_ERRNO(net_data->res, HOST_NOT_FOUND);
		return (NULL);
	}
	strncpy(pvt->name, name, NS_MAXDNAME);
	pvt->name[NS_MAXDNAME] = '\0';
	if (af == AF_INET && (net_data->res->options & RES_USE_INET6) != 0) {
		map_v4v6_address(pvt->addr, pvt->addr);
		af = AF_INET6;
	}
	pvt->host.h_addrtype = af;
	switch(af) {
	case AF_INET:
		pvt->host.h_length = NS_INADDRSZ;
		break;
	case AF_INET6:
		pvt->host.h_length = NS_IN6ADDRSZ;
		break;
	default:
		errno = EAFNOSUPPORT;
		RES_SET_H_ERRNO(net_data->res, NETDB_INTERNAL);
		return (NULL);
	}
	pvt->host.h_name = pvt->name;
	pvt->host.h_aliases = pvt->aliases;
	pvt->aliases[0] = NULL;
	pvt->addrs[0] = (char *)pvt->addr;
	pvt->addrs[1] = NULL;
	pvt->host.h_addr_list = pvt->addrs;
	RES_SET_H_ERRNO(net_data->res, NETDB_SUCCESS);
	return (&pvt->host);
}

#ifdef grot	/* for future use in gethostbyaddr(), for "SUNSECURITY" */
	struct hostent *rhp;
	char **haddr;
	u_long old_options;
	char hname2[MAXDNAME+1];

	if (af == AF_INET) {
	    /*
	     * turn off search as the name should be absolute,
	     * 'localhost' should be matched by defnames
	     */
	    strncpy(hname2, hp->h_name, MAXDNAME);
	    hname2[MAXDNAME] = '\0';
	    old_options = net_data->res->options;
	    net_data->res->options &= ~RES_DNSRCH;
	    net_data->res->options |= RES_DEFNAMES;
	    if (!(rhp = gethostbyname(hname2))) {
		net_data->res->options = old_options;
		RES_SET_H_ERRNO(net_data->res, HOST_NOT_FOUND);
		return (NULL);
	    }
	    net_data->res->options = old_options;
	    for (haddr = rhp->h_addr_list; *haddr; haddr++)
		if (!memcmp(*haddr, addr, INADDRSZ))
			break;
	    if (!*haddr) {
		RES_SET_H_ERRNO(net_data->res, HOST_NOT_FOUND);
		return (NULL);
	    }
	}
#endif /* grot */

#endif /*__BIND_NOSTATIC*/
