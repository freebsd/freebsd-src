/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright 2016 by the Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 * require a specific license from the United States Government.
 * It is the responsibility of any person or organization contemplating
 * export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

#include "udppktinfo.h"

#include <netinet/in.h>
#include <sys/socket.h>

#if defined(IP_PKTINFO) && defined(HAVE_STRUCT_IN_PKTINFO)
#define HAVE_IP_PKTINFO
#endif

#if defined(IPV6_PKTINFO) && defined(HAVE_STRUCT_IN6_PKTINFO)
#define HAVE_IPV6_PKTINFO
#endif

#if defined(HAVE_IP_PKTINFO) || defined(IP_SENDSRCADDR) ||      \
    defined(HAVE_IPV6_PKTINFO)
#define HAVE_PKTINFO_SUPPORT
#endif

/* Use RFC 3542 API below, but fall back from IPV6_RECVPKTINFO to IPV6_PKTINFO
 * for RFC 2292 implementations. */
#if !defined(IPV6_RECVPKTINFO) && defined(IPV6_PKTINFO)
#define IPV6_RECVPKTINFO IPV6_PKTINFO
#endif

/* Parallel, though not standardized. */
#if !defined(IP_RECVPKTINFO) && defined(IP_PKTINFO)
#define IP_RECVPKTINFO IP_PKTINFO
#endif /* IP_RECVPKTINFO */

#if defined(CMSG_SPACE) && defined(HAVE_STRUCT_CMSGHDR) &&      \
    defined(HAVE_PKTINFO_SUPPORT)
union pktinfo {
#ifdef HAVE_STRUCT_IN6_PKTINFO
    struct in6_pktinfo pi6;
#endif
#ifdef HAVE_STRUCT_IN_PKTINFO
    struct in_pktinfo pi4;
#endif
#ifdef IP_RECVDSTADDR
    struct in_addr iaddr;
#endif
    char c;
};
#endif /* HAVE_IPV6_PKTINFO && HAVE_STRUCT_CMSGHDR && HAVE_PKTINFO_SUPPORT */

#ifdef HAVE_IP_PKTINFO

#define set_ipv4_pktinfo set_ipv4_recvpktinfo
static inline krb5_error_code
set_ipv4_recvpktinfo(int sock)
{
    int sockopt = 1;
    return setsockopt(sock, IPPROTO_IP, IP_RECVPKTINFO, &sockopt,
                      sizeof(sockopt));
}

#elif defined(IP_RECVDSTADDR) /* HAVE_IP_PKTINFO */

#define set_ipv4_pktinfo set_ipv4_recvdstaddr
static inline krb5_error_code
set_ipv4_recvdstaddr(int sock)
{
    int sockopt = 1;
    return setsockopt(sock, IPPROTO_IP, IP_RECVDSTADDR, &sockopt,
                      sizeof(sockopt));
}

#else /* HAVE_IP_PKTINFO || IP_RECVDSTADDR */
#define set_ipv4_pktinfo(s) EINVAL
#endif /* HAVE_IP_PKTINFO || IP_RECVDSTADDR */

#ifdef HAVE_IPV6_PKTINFO

#define set_ipv6_pktinfo set_ipv6_recvpktinfo
static inline krb5_error_code
set_ipv6_recvpktinfo(int sock)
{
    int sockopt = 1;
    return setsockopt(sock, IPPROTO_IPV6, IPV6_RECVPKTINFO, &sockopt,
                      sizeof(sockopt));
}

#else /* HAVE_IPV6_PKTINFO */
#define set_ipv6_pktinfo(s) EINVAL
#endif /* HAVE_IPV6_PKTINFO */

/*
 * Set pktinfo option on a socket. Takes a socket and the socket address family
 * as arguments.
 *
 * Returns 0 on success, EINVAL if pktinfo is not supported for the address
 * family.
 */
krb5_error_code
set_pktinfo(int sock, int family)
{
    switch (family) {
    case AF_INET:
        return set_ipv4_pktinfo(sock);
    case AF_INET6:
        return set_ipv6_pktinfo(sock);
    default:
        return EINVAL;
    }
}

#if defined(HAVE_PKTINFO_SUPPORT) && defined(CMSG_SPACE)

/*
 * Check if a socket is bound to a wildcard address.
 * Returns 1 if it is, 0 if it's bound to a specific address, or -1 on error
 * with errno set to the error.
 */
static int
is_socket_bound_to_wildcard(int sock)
{
    struct sockaddr_storage bound_addr;
    socklen_t bound_addr_len = sizeof(bound_addr);
    struct sockaddr *sa = ss2sa(&bound_addr);

    if (getsockname(sock, sa, &bound_addr_len) < 0)
        return -1;

    if (!sa_is_inet(sa)) {
        errno = EINVAL;
        return -1;
    }

    return sa_is_wildcard(sa);
}

#ifdef HAVE_IP_PKTINFO

static inline struct in_pktinfo *
cmsg2pktinfo(struct cmsghdr *cmsgptr)
{
    return (struct in_pktinfo *)(void *)CMSG_DATA(cmsgptr);
}

#define check_cmsg_v4_pktinfo check_cmsg_ip_pktinfo
static int
check_cmsg_ip_pktinfo(struct cmsghdr *cmsgptr, struct sockaddr *to,
                      socklen_t *tolen, aux_addressing_info *auxaddr)
{
    struct in_pktinfo *pktinfo;

    if (cmsgptr->cmsg_level == IPPROTO_IP &&
        cmsgptr->cmsg_type == IP_PKTINFO &&
        *tolen >= sizeof(struct sockaddr_in)) {

        memset(to, 0, sizeof(struct sockaddr_in));
        pktinfo = cmsg2pktinfo(cmsgptr);
        sa2sin(to)->sin_addr = pktinfo->ipi_addr;
        sa2sin(to)->sin_family = AF_INET;
        *tolen = sizeof(struct sockaddr_in);
        return 1;
    }
    return 0;
}

#elif defined(IP_RECVDSTADDR) /* HAVE_IP_PKTINFO */

static inline struct in_addr *
cmsg2sin(struct cmsghdr *cmsgptr)
{
    return (struct in_addr *)(void *)CMSG_DATA(cmsgptr);
}

#define check_cmsg_v4_pktinfo check_cmsg_ip_recvdstaddr
static int
check_cmsg_ip_recvdstaddr(struct cmsghdr *cmsgptr, struct sockaddr *to,
                          socklen_t *tolen, aux_addressing_info * auxaddr)
{
    if (cmsgptr->cmsg_level == IPPROTO_IP &&
        cmsgptr->cmsg_type == IP_RECVDSTADDR &&
        *tolen >= sizeof(struct sockaddr_in)) {
        struct in_addr *sin_addr;

        memset(to, 0, sizeof(struct sockaddr_in));
        sin_addr = cmsg2sin(cmsgptr);
        sa2sin(to)->sin_addr = *sin_addr;
        sa2sin(to)->sin_family = AF_INET;
        *tolen = sizeof(struct sockaddr_in);
        return 1;
    }
    return 0;
}

#else /* HAVE_IP_PKTINFO || IP_RECVDSTADDR */
#define check_cmsg_v4_pktinfo(c, t, l, a) 0
#endif /* HAVE_IP_PKTINFO || IP_RECVDSTADDR */

#ifdef HAVE_IPV6_PKTINFO

static inline struct in6_pktinfo *
cmsg2pktinfo6(struct cmsghdr *cmsgptr)
{
    return (struct in6_pktinfo *)(void *)CMSG_DATA(cmsgptr);
}

#define check_cmsg_v6_pktinfo check_cmsg_ipv6_pktinfo
static int
check_cmsg_ipv6_pktinfo(struct cmsghdr *cmsgptr, struct sockaddr *to,
                        socklen_t *tolen, aux_addressing_info *auxaddr)
{
    struct in6_pktinfo *pktinfo;

    if (cmsgptr->cmsg_level == IPPROTO_IPV6 &&
        cmsgptr->cmsg_type == IPV6_PKTINFO &&
        *tolen >= sizeof(struct sockaddr_in6)) {

        memset(to, 0, sizeof(struct sockaddr_in6));
        pktinfo = cmsg2pktinfo6(cmsgptr);
        sa2sin6(to)->sin6_addr = pktinfo->ipi6_addr;
        sa2sin6(to)->sin6_family = AF_INET6;
        *tolen = sizeof(struct sockaddr_in6);
        auxaddr->ipv6_ifindex = pktinfo->ipi6_ifindex;
        return 1;
    }
    return 0;
}
#else /* HAVE_IPV6_PKTINFO */
#define check_cmsg_v6_pktinfo(c, t, l, a) 0
#endif /* HAVE_IPV6_PKTINFO */

static int
check_cmsg_pktinfo(struct cmsghdr *cmsgptr, struct sockaddr *to,
                   socklen_t *tolen, aux_addressing_info *auxaddr)
{
    return check_cmsg_v4_pktinfo(cmsgptr, to, tolen, auxaddr) ||
           check_cmsg_v6_pktinfo(cmsgptr, to, tolen, auxaddr);
}

/*
 * Receive a message from a socket.
 *
 * Arguments:
 *  sock
 *  buf     - The buffer to store the message in.
 *  len     - buf length
 *  flags
 *  from    - Set to the address that sent the message
 *  fromlen
 *  to      - Set to the address that the message was sent to if possible.
 *            May not be set in certain cases such as if pktinfo support is
 *            missing. May be NULL.
 *  tolen
 *  auxaddr - Miscellaneous address information.
 *
 * Returns 0 on success, otherwise an error code.
 */
krb5_error_code
recv_from_to(int sock, void *buf, size_t len, int flags,
             struct sockaddr *from, socklen_t * fromlen,
             struct sockaddr *to, socklen_t * tolen,
             aux_addressing_info *auxaddr)

{
    int r;
    struct iovec iov;
    char cmsg[CMSG_SPACE(sizeof(union pktinfo))];
    struct cmsghdr *cmsgptr;
    struct msghdr msg;

    /* Don't use pktinfo if the socket isn't bound to a wildcard address. */
    r = is_socket_bound_to_wildcard(sock);
    if (r < 0)
        return errno;

    if (!to || !tolen || !r)
        return recvfrom(sock, buf, len, flags, from, fromlen);

    /* Clobber with something recognizeable in case we can't extract the
     * address but try to use it anyways. */
    memset(to, 0x40, *tolen);

    iov.iov_base = buf;
    iov.iov_len = len;
    memset(&msg, 0, sizeof(msg));
    msg.msg_name = from;
    msg.msg_namelen = *fromlen;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsg;
    msg.msg_controllen = sizeof(cmsg);

    r = recvmsg(sock, &msg, flags);
    if (r < 0)
        return r;
    *fromlen = msg.msg_namelen;

    /*
     * On Darwin (and presumably all *BSD with KAME stacks), CMSG_FIRSTHDR
     * doesn't check for a non-zero controllen.  RFC 3542 recommends making
     * this check, even though the (new) spec for CMSG_FIRSTHDR says it's
     * supposed to do the check.
     */
    if (msg.msg_controllen) {
        cmsgptr = CMSG_FIRSTHDR(&msg);
        while (cmsgptr) {
            if (check_cmsg_pktinfo(cmsgptr, to, tolen, auxaddr))
                return r;
            cmsgptr = CMSG_NXTHDR(&msg, cmsgptr);
        }
    }
    /* No info about destination addr was available.  */
    *tolen = 0;
    return r;
}

#ifdef HAVE_IP_PKTINFO

#define set_msg_from_ipv4 set_msg_from_ip_pktinfo
static krb5_error_code
set_msg_from_ip_pktinfo(struct msghdr *msg, struct cmsghdr *cmsgptr,
                        struct sockaddr *from, socklen_t fromlen,
                        aux_addressing_info *auxaddr)
{
    struct in_pktinfo *p = cmsg2pktinfo(cmsgptr);
    const struct sockaddr_in *from4 = sa2sin(from);

    if (fromlen != sizeof(struct sockaddr_in))
        return EINVAL;
    cmsgptr->cmsg_level = IPPROTO_IP;
    cmsgptr->cmsg_type = IP_PKTINFO;
    cmsgptr->cmsg_len = CMSG_LEN(sizeof(struct in_pktinfo));
    p->ipi_spec_dst = from4->sin_addr;

    msg->msg_controllen = CMSG_SPACE(sizeof(struct in_pktinfo));
    return 0;
}

#elif defined(IP_SENDSRCADDR) /* HAVE_IP_PKTINFO */

#define set_msg_from_ipv4 set_msg_from_ip_sendsrcaddr
static krb5_error_code
set_msg_from_ip_sendsrcaddr(struct msghdr *msg, struct cmsghdr *cmsgptr,
                            struct sockaddr *from, socklen_t fromlen,
                            aux_addressing_info *auxaddr)
{
    struct in_addr *sin_addr = cmsg2sin(cmsgptr);
    const struct sockaddr_in *from4 = sa2sin(from);
    if (fromlen != sizeof(struct sockaddr_in))
        return EINVAL;
    cmsgptr->cmsg_level = IPPROTO_IP;
    cmsgptr->cmsg_type = IP_SENDSRCADDR;
    cmsgptr->cmsg_len = CMSG_LEN(sizeof(struct in_addr));
    msg->msg_controllen = CMSG_SPACE(sizeof(struct in_addr));
    *sin_addr = from4->sin_addr;
    return 0;
}

#else /* HAVE_IP_PKTINFO || IP_SENDSRCADDR */
#define set_msg_from_ipv4(m, c, f, l, a) EINVAL
#endif /* HAVE_IP_PKTINFO || IP_SENDSRCADDR */

#ifdef HAVE_IPV6_PKTINFO

#define set_msg_from_ipv6 set_msg_from_ipv6_pktinfo
static krb5_error_code
set_msg_from_ipv6_pktinfo(struct msghdr *msg, struct cmsghdr *cmsgptr,
                          struct sockaddr *from, socklen_t fromlen,
                          aux_addressing_info *auxaddr)
{
    struct in6_pktinfo *p = cmsg2pktinfo6(cmsgptr);
    const struct sockaddr_in6 *from6 = sa2sin6(from);

    if (fromlen != sizeof(struct sockaddr_in6))
        return EINVAL;
    cmsgptr->cmsg_level = IPPROTO_IPV6;
    cmsgptr->cmsg_type = IPV6_PKTINFO;
    cmsgptr->cmsg_len = CMSG_LEN(sizeof(struct in6_pktinfo));

    p->ipi6_addr = from6->sin6_addr;
    /*
     * Because of the possibility of asymmetric routing, we
     * normally don't want to specify an interface.  However,
     * macOS doesn't like sending from a link-local address
     * (which can come up in testing at least, if you wind up
     * with a "foo.local" name) unless we do specify the
     * interface.
     */
    if (IN6_IS_ADDR_LINKLOCAL(&from6->sin6_addr))
        p->ipi6_ifindex = auxaddr->ipv6_ifindex;
    /* otherwise, already zero */

    msg->msg_controllen = CMSG_SPACE(sizeof(struct in6_pktinfo));
    return 0;
}

#else /* HAVE_IPV6_PKTINFO */
#define set_msg_from_ipv6(m, c, f, l, a) EINVAL
#endif /* HAVE_IPV6_PKTINFO */

static krb5_error_code
set_msg_from(int family, struct msghdr *msg, struct cmsghdr *cmsgptr,
             struct sockaddr *from, socklen_t fromlen,
             aux_addressing_info *auxaddr)
{
    switch (family) {
    case AF_INET:
        return set_msg_from_ipv4(msg, cmsgptr, from, fromlen, auxaddr);
    case AF_INET6:
        return set_msg_from_ipv6(msg, cmsgptr, from, fromlen, auxaddr);
    }

    return EINVAL;
}

/*
 * Send a message to an address.
 *
 * Arguments:
 *  sock
 *  buf     - The message to send.
 *  len     - buf length
 *  flags
 *  to      - The address to send the message to.
 *  tolen
 *  from    - The address to attempt to send the message from. May be NULL.
 *  fromlen
 *  auxaddr - Miscellaneous address information.
 *
 * Returns 0 on success, otherwise an error code.
 */
krb5_error_code
send_to_from(int sock, void *buf, size_t len, int flags,
             const struct sockaddr *to, socklen_t tolen, struct sockaddr *from,
             socklen_t fromlen, aux_addressing_info *auxaddr)
{
    int r;
    struct iovec iov;
    struct msghdr msg;
    struct cmsghdr *cmsgptr;
    char cbuf[CMSG_SPACE(sizeof(union pktinfo))];

    /* Don't use pktinfo if the socket isn't bound to a wildcard address. */
    r = is_socket_bound_to_wildcard(sock);
    if (r < 0)
        return errno;

    if (from == NULL || fromlen == 0 || from->sa_family != to->sa_family || !r)
        goto use_sendto;

    iov.iov_base = buf;
    iov.iov_len = len;
    /* Truncation?  */
    if (iov.iov_len != len)
        return EINVAL;
    memset(cbuf, 0, sizeof(cbuf));
    memset(&msg, 0, sizeof(msg));
    msg.msg_name = (void *)to;
    msg.msg_namelen = tolen;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cbuf;
    /* CMSG_FIRSTHDR needs a non-zero controllen, or it'll return NULL on
     * Linux. */
    msg.msg_controllen = sizeof(cbuf);
    cmsgptr = CMSG_FIRSTHDR(&msg);
    msg.msg_controllen = 0;

    if (set_msg_from(from->sa_family, &msg, cmsgptr, from, fromlen, auxaddr))
        goto use_sendto;
    return sendmsg(sock, &msg, flags);

use_sendto:
    return sendto(sock, buf, len, flags, to, tolen);
}

#else /* HAVE_PKTINFO_SUPPORT && CMSG_SPACE */

krb5_error_code
recv_from_to(int sock, void *buf, size_t len, int flags,
             struct sockaddr *from, socklen_t *fromlen,
             struct sockaddr *to, socklen_t *tolen,
             aux_addressing_info *auxaddr)
{
    if (to && tolen) {
        /* Clobber with something recognizeable in case we try to use the
         * address. */
        memset(to, 0x40, *tolen);
        *tolen = 0;
    }

    return recvfrom(sock, buf, len, flags, from, fromlen);
}

krb5_error_code
send_to_from(int sock, void *buf, size_t len, int flags,
             const struct sockaddr *to, socklen_t tolen,
             struct sockaddr *from, socklen_t fromlen,
             aux_addressing_info *auxaddr)
{
    return sendto(sock, buf, len, flags, to, tolen);
}

#endif /* HAVE_PKTINFO_SUPPORT && CMSG_SPACE */
