/*
 * Copyright (c) 1997 - 1999 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden). 
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution. 
 *
 * 3. Neither the name of the Institute nor the names of its contributors 
 *    may be used to endorse or promote products derived from this software 
 *    without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. 
 */

#include "krb5_locl.h"

RCSID("$Id: get_addrs.c,v 1.35 1999/12/02 17:05:09 joda Exp $");

#ifdef __osf__
/* hate */
struct rtentry;
struct mbuf;
#endif
#ifdef HAVE_NET_IF_H
#include <net/if.h>
#endif

#ifdef HAVE_SYS_SOCKIO_H
#include <sys/sockio.h>
#endif /* HAVE_SYS_SOCKIO_H */

#ifdef HAVE_NETINET_IN6_VAR_H
#include <netinet/in6_var.h>
#endif /* HAVE_NETINET_IN6_VAR_H */

static krb5_error_code
gethostname_fallback (krb5_addresses *res)
{
     krb5_error_code err;
     char hostname[MAXHOSTNAMELEN];
     struct hostent *hostent;

     if (gethostname (hostname, sizeof(hostname)))
	  return errno;
     hostent = roken_gethostbyname (hostname);
     if (hostent == NULL)
	  return errno;
     res->len = 1;
     res->val = malloc (sizeof(*res->val));
     if (res->val == NULL)
	 return ENOMEM;
     res->val[0].addr_type = hostent->h_addrtype;
     res->val[0].address.data = NULL;
     res->val[0].address.length = 0;
     err = krb5_data_copy (&res->val[0].address,
			   hostent->h_addr,
			   hostent->h_length);
     if (err) {
	 free (res->val);
	 return err;
     }
     return 0;
}

enum {
    LOOP            = 1,	/* do include loopback interfaces */
    LOOP_IF_NONE    = 2,	/* include loopback if no other if's */
    EXTRA_ADDRESSES = 4,	/* include extra addresses */
    SCAN_INTERFACES = 8		/* scan interfaces for addresses */
};

/*
 * Try to figure out the addresses of all configured interfaces with a
 * lot of magic ioctls.
 */

static krb5_error_code
find_all_addresses (krb5_context context,
		    krb5_addresses *res, int flags,
		    int af, int siocgifconf, int siocgifflags,
		    size_t ifreq_sz)
{
     krb5_error_code ret;
     int fd;
     size_t buf_size;
     char *buf;
     struct ifconf ifconf;
     int num, j = 0;
     char *p;
     size_t sz;
     struct sockaddr sa_zero;
     struct ifreq *ifr;
     krb5_address lo_addr;
     int got_lo = FALSE;

     buf = NULL;
     res->val = NULL;

     memset (&sa_zero, 0, sizeof(sa_zero));
     fd = socket(af, SOCK_DGRAM, 0);
     if (fd < 0)
	  return -1;

     buf_size = 8192;
     for (;;) {
	 buf = malloc(buf_size);
	 if (buf == NULL) {
	     ret = ENOMEM;
	     goto error_out;
	 }
	 ifconf.ifc_len = buf_size;
	 ifconf.ifc_buf = buf;
	 if (ioctl (fd, siocgifconf, &ifconf) < 0) {
	     ret = errno;
	     goto error_out;
	 }
	 /*
	  * Can the difference between a full and a overfull buf
	  * be determined?
	  */

	 if (ifconf.ifc_len < buf_size)
	     break;
	 free (buf);
	 buf_size *= 2;
     }

     num = ifconf.ifc_len / ifreq_sz;
     res->len = num;
     res->val = calloc(num, sizeof(*res->val));
     if (res->val == NULL) {
	 ret = ENOMEM;
	 goto error_out;
     }

     j = 0;
     for (p = ifconf.ifc_buf;
	  p < ifconf.ifc_buf + ifconf.ifc_len;
	  p += sz) {
	 struct ifreq ifreq;
	 struct sockaddr *sa;

	 ifr = (struct ifreq *)p;
	 sa  = &ifr->ifr_addr;

	 sz = ifreq_sz;
#ifdef HAVE_STRUCT_SOCKADDR_SA_LEN
	 sz = max(sz, sizeof(ifr->ifr_name) + sa->sa_len);
#endif
#ifdef SA_LEN
	 sz = max(sz, SA_LEN(sa));
#endif
	 memcpy (ifreq.ifr_name, ifr->ifr_name, sizeof(ifr->ifr_name));

	 if (ioctl(fd, siocgifflags, &ifreq) < 0) {
	     ret = errno;
	     goto error_out;
	 }

	 if (!(ifreq.ifr_flags & IFF_UP))
	     continue;
	 if (memcmp (sa, &sa_zero, sizeof(sa_zero)) == 0)
	     continue;
	 if (krb5_sockaddr_uninteresting (sa))
	     continue;

	 if (ifreq.ifr_flags & IFF_LOOPBACK) {
	     if (flags & LOOP_IF_NONE) {
		 ret = krb5_sockaddr2address (sa, &lo_addr);
		 if (ret)
		     goto error_out;
		 got_lo = TRUE;
		 continue;
	     } else if((flags & LOOP) == 0)
		 continue;
	 }

	 ret = krb5_sockaddr2address (sa, &res->val[j]);
	 if (ret)
	     goto error_out;
	 ++j;
     }
     if ((flags & LOOP_IF_NONE) && got_lo) {
	 if (j == 0)
	     res->val[j++] = lo_addr;
	 else
	     krb5_free_address (context, &lo_addr);
     }

     if (j != num) {
	 void *tmp;

	 res->len = j;
	 tmp = realloc (res->val, j * sizeof(*res->val));
	 if (j != 0 && tmp == NULL) {
	     ret = ENOMEM;
	     goto error_out;
	 }
	 res->val = tmp;
     }
     ret = 0;
     goto cleanup;

error_out:
     if (got_lo)
	     krb5_free_address (context, &lo_addr);
     while(j--) {
	 krb5_free_address (context, &res->val[j]);
     }
     free (res->val);
cleanup:
     close (fd);
     free (buf);
     return ret;
}

static krb5_error_code
get_addrs_int (krb5_context context, krb5_addresses *res, int flags)
{
    krb5_error_code ret = -1;

    if (flags & SCAN_INTERFACES) {
#if defined(AF_INET6) && defined(SIOCGIF6CONF) && defined(SIOCGIF6FLAGS)
	if (ret)
	    ret = find_all_addresses (context, res, flags,
				      AF_INET6, SIOCGIF6CONF, SIOCGIF6FLAGS,
				      sizeof(struct in6_ifreq));
#endif
#if defined(HAVE_IPV6) && defined(SIOCGIFCONF)
	if (ret)
	    ret = find_all_addresses (context, res, flags,
				      AF_INET6, SIOCGIFCONF, SIOCGIFFLAGS,
				      sizeof(struct ifreq));
#endif
#if defined(AF_INET) && defined(SIOCGIFCONF) && defined(SIOCGIFFLAGS)
	if (ret)
	    ret = find_all_addresses (context, res, flags,
				      AF_INET, SIOCGIFCONF, SIOCGIFFLAGS,
				      sizeof(struct ifreq));
	if(ret || res->len == 0)
	    ret = gethostname_fallback (res);
#endif
    } else
	ret = 0;

    if(ret == 0 && (flags & EXTRA_ADDRESSES)) {
	/* append user specified addresses */
	krb5_addresses a;
	ret = krb5_get_extra_addresses(context, &a);
	if(ret) {
	    krb5_free_addresses(context, res);
	    return ret;
	}
	ret = krb5_append_addresses(context, res, &a);
	if(ret) {
	    krb5_free_addresses(context, res);
	    return ret;
	}
	krb5_free_addresses(context, &a);
    }
    return ret;
}

/*
 * Try to get all addresses, but return the one corresponding to
 * `hostname' if we fail.
 *
 * Only include loopback address if there are no other.
 */

krb5_error_code
krb5_get_all_client_addrs (krb5_context context, krb5_addresses *res)
{
    int flags = LOOP_IF_NONE | EXTRA_ADDRESSES;

    if (context->scan_interfaces)
	flags |= SCAN_INTERFACES;

    return get_addrs_int (context, res, flags);
}

/*
 * Try to get all local addresses that a server should listen to.
 * If that fails, we return the address corresponding to `hostname'.
 */

krb5_error_code
krb5_get_all_server_addrs (krb5_context context, krb5_addresses *res)
{
    return get_addrs_int (context, res, LOOP | SCAN_INTERFACES);
}
