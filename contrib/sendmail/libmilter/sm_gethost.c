/*
 *  Copyright (c) 1999-2000 Sendmail, Inc. and its suppliers.
 *	All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 */

#ifndef lint
static char id[] = "@(#)$Id: sm_gethost.c,v 8.7.8.2 2000/09/17 17:04:24 gshapiro Exp $";
#endif /* ! lint */

#if _FFR_MILTER
#include <sendmail.h>
#if NETINET || NETINET6
# include <arpa/inet.h>
#endif /* NETINET || NETINET6 */

/*
**  MI_GETHOSTBY{NAME,ADDR} -- compatibility routines for gethostbyXXX
**
**	Some operating systems have wierd problems with the gethostbyXXX
**	routines.  For example, Solaris versions at least through 2.3
**	don't properly deliver a canonical h_name field.  This tries to
**	work around these problems.
**
**	Support IPv6 as well as IPv4.
*/

#if NETINET6 && NEEDSGETIPNODE && __RES < 19990909

# ifndef AI_V4MAPPED
#  define AI_V4MAPPED	0	/* dummy */
# endif /* ! AI_V4MAPPED */
# ifndef AI_ALL
#  define AI_ALL	0	/* dummy */
# endif /* ! AI_ALL */

static struct hostent *
getipnodebyname(name, family, flags, err)
	char *name;
	int family;
	int flags;
	int *err;
{
	bool resv6 = TRUE;
	struct hostent *h;

	if (family == AF_INET6)
	{
		/* From RFC2133, section 6.1 */
		resv6 = bitset(RES_USE_INET6, _res.options);
		_res.options |= RES_USE_INET6;
	}
	h_errno = 0;
	h = gethostbyname(name);
	*err = h_errno;
	if (family == AF_INET6 && !resv6)
		_res.options &= ~RES_USE_INET6;
	return h;
}
#endif /* NEEDSGETIPNODE && NETINET6 && __RES < 19990909 */

struct hostent *
mi_gethostbyname(name, family)
	char *name;
	int family;
{
	struct hostent *h = NULL;
#if (SOLARIS > 10000 && SOLARIS < 20400) || (defined(SOLARIS) && SOLARIS < 204) || (defined(sony_news) && defined(__svr4))
# if SOLARIS == 20300 || SOLARIS == 203
	static struct hostent hp;
	static char buf[1000];
	extern struct hostent *_switch_gethostbyname_r();

	h = _switch_gethostbyname_r(name, &hp, buf, sizeof(buf), &h_errno);
# else /* SOLARIS == 20300 || SOLARIS == 203 */
	extern struct hostent *__switch_gethostbyname();

	h = __switch_gethostbyname(name);
# endif /* SOLARIS == 20300 || SOLARIS == 203 */
#else /* (SOLARIS > 10000 && SOLARIS < 20400) || (defined(SOLARIS) && SOLARIS < 204) || (defined(sony_news) && defined(__svr4)) */
# if NETINET6
	int err;
# endif /* NETINET6 */

# if NETINET6
	h = getipnodebyname(name, family, AI_V4MAPPED|AI_ALL, &err);
	h_errno = err;
# else /* NETINET6 */
	h = gethostbyname(name);
# endif /* NETINET6 */

#endif /* (SOLARIS > 10000 && SOLARIS < 20400) || (defined(SOLARIS) && SOLARIS < 204) || (defined(sony_news) && defined(__svr4)) */
	return h;
}
#endif /* _FFR_MILTER */
