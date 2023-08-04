/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/os/localaddr.c */
/*
 * Copyright 1990,1991,2000,2001,2002,2004,2007,2008 by the Massachusetts
 * Institute of Technology.  All Rights Reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
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

/*
 * Return the protocol addresses supported by this host.
 * Exports from this file:
 *   krb5int_foreach_localaddr (does callbacks)
 *   krb5_os_localaddr (doesn't include krb5.conf extra_addresses)
 *
 * XNS support is untested, but "Should just work".  (Hah!)
 */

#include "k5-int.h"
#include "os-proto.h"

#if !defined(_WIN32)

/* needed for solaris, harmless elsewhere... */
#define BSD_COMP
#include <sys/ioctl.h>
#include <sys/time.h>
#include <errno.h>
#include <stddef.h>
#include <ctype.h>

#if defined(TEST) || defined(DEBUG)
# include "fake-addrinfo.h"
#endif

#include "foreachaddr.h"

/* Note: foreach_localaddr is exported from the library through
   krb5int_accessor, for the KDC to use.

   This function iterates over all the addresses it can find for the
   local system, in one or two passes.  In each pass, and between the
   two, it can invoke callback functions supplied by the caller.  The
   two passes should operate on the same information, though not
   necessarily in the same order each time.  Duplicate and local
   addresses should be eliminated.  Storage passed to callback
   functions should not be assumed to be valid after foreach_localaddr
   returns.

   The int return value is an errno value (XXX or krb5_error_code
   returned for a socket error) if something internal to
   foreach_localaddr fails.  If one of the callback functions wants to
   indicate an error, it should store something via the 'data' handle.
   If any callback function returns a non-zero value,
   foreach_localaddr will clean up and return immediately.

   Multiple definitions are provided below, dependent on various
   system facilities for extracting the necessary information.  */

/* Now, on to the implementations, and heaps of debugging code.  */

#ifdef TEST
# define Tprintf(X) printf X
# define Tperror(X) perror(X)
#else
# define Tprintf(X) (void) X
# define Tperror(X) (void)(X)
#endif

/*
 * The SIOCGIF* ioctls require a socket.
 * It doesn't matter *what* kind of socket they use, but it has to be
 * a socket.
 *
 * Of course, you can't just ask the kernel for a socket of arbitrary
 * type; you have to ask for one with a valid type.
 *
 */
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#ifndef USE_AF
#define USE_AF AF_INET
#define USE_TYPE SOCK_DGRAM
#define USE_PROTO 0
#endif
#endif

#ifdef KRB5_USE_NS
#include <netns/ns.h>
#ifndef USE_AF
#define USE_AF AF_NS
#define USE_TYPE SOCK_DGRAM
#define USE_PROTO 0             /* guess */
#endif
#endif
/*
 * Add more address families here.
 */


#if defined(__linux__) && !defined(HAVE_IFADDRS_H)
#define LINUX_IPV6_HACK
#endif

#include <errno.h>

/*
 * Return all the protocol addresses of this host.
 *
 * We could kludge up something to return all addresses, assuming that
 * they're valid kerberos protocol addresses, but we wouldn't know the
 * real size of the sockaddr or know which part of it was actually the
 * host part.
 *
 * This uses the SIOCGIFCONF, SIOCGIFFLAGS, and SIOCGIFADDR ioctl's.
 */

/*
 * BSD 4.4 defines the size of an ifreq to be
 * max(sizeof(ifreq), sizeof(ifreq.ifr_name)+ifreq.ifr_addr.sa_len
 * However, under earlier systems, sa_len isn't present, so the size is
 * just sizeof(struct ifreq).
 */
#ifdef HAVE_SA_LEN
#ifndef max
#define max(a,b) ((a) > (b) ? (a) : (b))
#endif
#define ifreq_size(i) max(sizeof(struct ifreq),                         \
                          sizeof((i).ifr_name)+(i).ifr_addr.sa_len)
#else
#define ifreq_size(i) sizeof(struct ifreq)
#endif /* HAVE_SA_LEN*/

#if defined(DEBUG) || defined(TEST)
#include <netinet/in.h>
#include <net/if.h>

#include "socket-utils.h"
#include "fake-addrinfo.h"

void printaddr (struct sockaddr *);

void
printaddr(struct sockaddr *sa)
/*@modifies fileSystem@*/
{
    char buf[NI_MAXHOST];
    int err;

    printf ("%p ", (void *) sa);
    err = getnameinfo (sa, sa_socklen (sa), buf, sizeof (buf), 0, 0,
                       NI_NUMERICHOST);
    if (err)
        printf ("<getnameinfo error %d: %s> family=%d",
                err, gai_strerror (err),
                sa->sa_family);
    else
        printf ("%s", buf);
}
#endif

static int
is_loopback_address(struct sockaddr *sa)
{
    switch (sa->sa_family) {
    case AF_INET: {
        struct sockaddr_in *s4 = sa2sin(sa);
        return s4->sin_addr.s_addr == htonl(INADDR_LOOPBACK);
    }
    case AF_INET6: {
        struct sockaddr_in6 *s6 = sa2sin6(sa);
        return IN6_IS_ADDR_LOOPBACK(&s6->sin6_addr);
    }
    default:
        return 0;
    }
}

#ifdef HAVE_IFADDRS_H
#include <ifaddrs.h>

#ifdef DEBUG
void
printifaddr(struct ifaddrs *ifp)
{
    printf ("%p={\n", ifp);
/*  printf ("\tnext=%p\n", ifp->ifa_next); */
    printf ("\tname=%s\n", ifp->ifa_name);
    printf ("\tflags=");
    {
        int ch, flags = ifp->ifa_flags;
        printf ("%x", flags);
        ch = '<';
#define X(F) if (flags & IFF_##F) { printf ("%c%s", ch, #F); flags &= ~IFF_##F; ch = ','; }
        X (UP); X (BROADCAST); X (DEBUG); X (LOOPBACK); X (POINTOPOINT);
        X (NOTRAILERS); X (RUNNING); X (NOARP); X (PROMISC); X (ALLMULTI);
#ifdef IFF_OACTIVE
        X (OACTIVE);
#endif
#ifdef IFF_SIMPLE
        X (SIMPLEX);
#endif
        X (MULTICAST);
        printf (">");
#undef X
    }
    if (ifp->ifa_addr)
        printf ("\n\taddr="), printaddr (ifp->ifa_addr);
    if (ifp->ifa_netmask)
        printf ("\n\tnetmask="), printaddr (ifp->ifa_netmask);
    if (ifp->ifa_broadaddr)
        printf ("\n\tbroadaddr="), printaddr (ifp->ifa_broadaddr);
    if (ifp->ifa_dstaddr)
        printf ("\n\tdstaddr="), printaddr (ifp->ifa_dstaddr);
    if (ifp->ifa_data)
        printf ("\n\tdata=%p", ifp->ifa_data);
    printf ("\n}\n");
}
#endif /* DEBUG */

#include <string.h>
#include <stdlib.h>

static int
addr_eq (struct sockaddr *s1, struct sockaddr *s2)
{
    if (s1->sa_family != s2->sa_family)
        return 0;
    switch (s1->sa_family) {
    case AF_INET:
        return !memcmp(&sa2sin(s1)->sin_addr, &sa2sin(s2)->sin_addr,
                       sizeof(sa2sin(s1)->sin_addr));
    case AF_INET6:
        return !memcmp(&sa2sin6(s1)->sin6_addr, &sa2sin6(s2)->sin6_addr,
                       sizeof(sa2sin6(s1)->sin6_addr));
    default:
        /* Err on side of duplicate listings.  */
        return 0;
    }
}
#endif

#ifndef HAVE_IFADDRS_H
/*@-usereleased@*/ /* lclint doesn't understand realloc */
static /*@null@*/ void *
grow_or_free (/*@only@*/ void *ptr, size_t newsize)
/*@*/
{
    void *newptr;
    newptr = realloc (ptr, newsize);
    if (newptr == NULL && newsize != 0) {
        free (ptr);             /* lclint complains but this is right */
        return NULL;
    }
    return newptr;
}
/*@=usereleased@*/

static int
get_ifconf (int s, size_t *lenp, /*@out@*/ char *buf)
/*@modifies *buf,*lenp@*/
{
    int ret;
    struct ifconf ifc;

    /*@+matchanyintegral@*/
    ifc.ifc_len = *lenp;
    /*@=matchanyintegral@*/
    ifc.ifc_buf = buf;
    memset(buf, 0, *lenp);
    /*@-moduncon@*/
    ret = ioctl (s, SIOCGIFCONF, (char *)&ifc);
    /*@=moduncon@*/
    /*@+matchanyintegral@*/
    *lenp = ifc.ifc_len;
    /*@=matchanyintegral@*/
    return ret;
}

/* Solaris uses SIOCGLIFCONF to return struct lifconf which is just
   an extended version of struct ifconf.

   HP-UX 11 also appears to have SIOCGLIFCONF, but uses struct
   if_laddrconf, and struct if_laddrreq to be used with
   SIOCGLIFADDR.  */
#if defined(SIOCGLIFCONF) && defined(HAVE_STRUCT_LIFCONF)
static int
get_lifconf (int af, int s, size_t *lenp, /*@out@*/ char *buf)
/*@modifies *buf,*lenp@*/
{
    int ret;
    struct lifconf lifc;

    lifc.lifc_family = af;
    lifc.lifc_flags = 0;
    /*@+matchanyintegral@*/
    lifc.lifc_len = *lenp;
    /*@=matchanyintegral@*/
    lifc.lifc_buf = buf;
    memset(buf, 0, *lenp);
    /*@-moduncon@*/
    ret = ioctl (s, SIOCGLIFCONF, (char *)&lifc);
    if (ret)
        Tperror ("SIOCGLIFCONF");
    /*@=moduncon@*/
    /*@+matchanyintegral@*/
    *lenp = lifc.lifc_len;
    /*@=matchanyintegral@*/
    return ret;
}
#endif
#if defined(SIOCGLIFCONF) && defined(HAVE_STRUCT_IF_LADDRCONF) && 0
/* I'm not sure if this is needed or if net/if.h will pull it in.  */
/* #include <net/if6.h> */
static int
get_if_laddrconf (int af, int s, size_t *lenp, /*@out@*/ char *buf)
/*@modifies *buf,*lenp@*/
{
    int ret;
    struct if_laddrconf iflc;

    /*@+matchanyintegral@*/
    iflc.iflc_len = *lenp;
    /*@=matchanyintegral@*/
    iflc.iflc_buf = buf;
    memset(buf, 0, *lenp);
    /*@-moduncon@*/
    ret = ioctl (s, SIOCGLIFCONF, (char *)&iflc);
    if (ret)
        Tperror ("SIOCGLIFCONF");
    /*@=moduncon@*/
    /*@+matchanyintegral@*/
    *lenp = iflc.iflc_len;
    /*@=matchanyintegral@*/
    return ret;
}
#endif
#endif /* ! HAVE_IFADDRS_H */

#ifdef LINUX_IPV6_HACK
#include <stdio.h>
/* Read IPv6 addresses out of /proc/net/if_inet6, since there isn't
   (currently) any ioctl to return them.  */
struct linux_ipv6_addr_list {
    struct sockaddr_in6 addr;
    struct linux_ipv6_addr_list *next;
};
static struct linux_ipv6_addr_list *
get_linux_ipv6_addrs ()
{
    struct linux_ipv6_addr_list *lst = 0;
    FILE *f;

    /* _PATH_PROCNET_IFINET6 */
    f = fopen("/proc/net/if_inet6", "r");
    if (f) {
        char ifname[21];
        unsigned int idx, pfxlen, scope, dadstat;
        struct in6_addr a6;
        struct linux_ipv6_addr_list *nw;
        int i;
        unsigned int addrbyte[16];

        set_cloexec_file(f);
        while (fscanf(f,
                      "%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x"
                      " %2x %2x %2x %2x %20s\n",
                      &addrbyte[0], &addrbyte[1], &addrbyte[2], &addrbyte[3],
                      &addrbyte[4], &addrbyte[5], &addrbyte[6], &addrbyte[7],
                      &addrbyte[8], &addrbyte[9], &addrbyte[10], &addrbyte[11],
                      &addrbyte[12], &addrbyte[13], &addrbyte[14],
                      &addrbyte[15],
                      &idx, &pfxlen, &scope, &dadstat, ifname) != EOF) {
            for (i = 0; i < 16; i++)
                a6.s6_addr[i] = addrbyte[i];
            if (scope != 0)
                continue;
            nw = calloc (1, sizeof (struct linux_ipv6_addr_list));
            if (nw == 0)
                continue;
            nw->addr.sin6_addr = a6;
            nw->addr.sin6_family = AF_INET6;
            /* Ignore other fields, we don't actually use them here.  */
            nw->next = lst;
            lst = nw;
        }
        fclose (f);
    }
    return lst;
}
#endif

/* Return value is errno if internal stuff failed, otherwise zero,
   even in the case where a called function terminated the iteration.

   If one of the callback functions wants to pass back an error
   indication, it should do it via some field pointed to by the DATA
   argument.  */

#ifdef HAVE_IFADDRS_H

int
foreach_localaddr (/*@null@*/ void *data,
                   int (*pass1fn) (/*@null@*/ void *, struct sockaddr *) /*@*/,
                   /*@null@*/ int (*betweenfn) (/*@null@*/ void *) /*@*/,
                   /*@null@*/ int (*pass2fn) (/*@null@*/ void *,
                                              struct sockaddr *) /*@*/)
#if defined(DEBUG) || defined(TEST)
/*@modifies fileSystem@*/
#endif
{
    struct ifaddrs *ifp_head, *ifp, *ifp2;
    int match;

    if (getifaddrs (&ifp_head) < 0)
        return errno;
    for (ifp = ifp_head; ifp; ifp = ifp->ifa_next) {
#ifdef DEBUG
        printifaddr (ifp);
#endif
        if ((ifp->ifa_flags & IFF_UP) == 0)
            continue;
        if (ifp->ifa_addr == NULL) {
            /* Can't use an interface without an address.  Linux
               apparently does this sometimes.  [RT ticket 1770 from
               Maurice Massar, also Debian bug 206851, shows the
               problem with a PPP link on a newer kernel than I'm
               running.]

               Pretend it's not up, so the second pass will skip
               it.  */
            ifp->ifa_flags &= ~IFF_UP;
            continue;
        }
        if (is_loopback_address(ifp->ifa_addr)) {
            /* Pretend it's not up, so the second pass will skip
               it.  */
            ifp->ifa_flags &= ~IFF_UP;
            continue;
        }
        /* If this address is a duplicate, punt.  */
        match = 0;
        for (ifp2 = ifp_head; ifp2 && ifp2 != ifp; ifp2 = ifp2->ifa_next) {
            if ((ifp2->ifa_flags & IFF_UP) == 0)
                continue;
            if (addr_eq (ifp->ifa_addr, ifp2->ifa_addr)) {
                match = 1;
                ifp->ifa_flags &= ~IFF_UP;
                break;
            }
        }
        if (match)
            continue;
        if ((*pass1fn) (data, ifp->ifa_addr))
            goto punt;
    }
    if (betweenfn && (*betweenfn)(data))
        goto punt;
    if (pass2fn)
        for (ifp = ifp_head; ifp; ifp = ifp->ifa_next) {
            if (ifp->ifa_flags & IFF_UP)
                if ((*pass2fn) (data, ifp->ifa_addr))
                    goto punt;
        }
punt:
    freeifaddrs (ifp_head);
    return 0;
}

#elif defined (SIOCGLIFNUM) && defined(HAVE_STRUCT_LIFCONF) /* Solaris 8 and later; Sol 7? */

int
foreach_localaddr (/*@null@*/ void *data,
                   int (*pass1fn) (/*@null@*/ void *, struct sockaddr *) /*@*/,
                   /*@null@*/ int (*betweenfn) (/*@null@*/ void *) /*@*/,
                   /*@null@*/ int (*pass2fn) (/*@null@*/ void *,
                                              struct sockaddr *) /*@*/)
#if defined(DEBUG) || defined(TEST)
/*@modifies fileSystem@*/
#endif
{
    /* Okay, this is kind of odd.  We have to use each of the address
       families we care about, because with an AF_INET socket, extra
       interfaces like hme0:1 that have only AF_INET6 addresses will
       cause errors.  Similarly, if hme0 has more AF_INET addresses
       than AF_INET6 addresses, we won't be able to retrieve all of
       the AF_INET addresses if we use an AF_INET6 socket.  Since
       neither family is guaranteed to have the greater number of
       addresses, we should use both.

       If it weren't for this little quirk, we could use one socket of
       any type, and ask for addresses of all types.  At least, it
       seems to work that way.  */

    static const int afs[] = { AF_INET, AF_NS, AF_INET6 };
#define N_AFS (sizeof (afs) / sizeof (afs[0]))
    struct {
        int af;
        int sock;
        void *buf;
        size_t buf_size;
        struct lifnum lifnum;
    } afp[N_AFS];
    int code, i, j;
    int retval = 0, afidx;
    krb5_error_code sock_err = 0;
    struct lifreq *lifr, lifreq, *lifr2;

#define FOREACH_AF() for (afidx = 0; afidx < N_AFS; afidx++)
#define P (afp[afidx])

    /* init */
    FOREACH_AF () {
        P.af = afs[afidx];
        P.sock = -1;
        P.buf = 0;
    }

    /* first pass: get raw data, discard uninteresting addresses, callback */
    FOREACH_AF () {
        Tprintf (("trying af %d...\n", P.af));
        P.sock = socket (P.af, USE_TYPE, USE_PROTO);
        if (P.sock < 0) {
            sock_err = SOCKET_ERROR;
            Tperror ("socket");
            continue;
        }
        set_cloexec_fd(P.sock);

        P.lifnum.lifn_family = P.af;
        P.lifnum.lifn_flags = 0;
        P.lifnum.lifn_count = 0;
        code = ioctl (P.sock, SIOCGLIFNUM, &P.lifnum);
        if (code) {
            Tperror ("ioctl(SIOCGLIFNUM)");
            retval = errno;
            goto punt;
        }

        P.buf_size = P.lifnum.lifn_count * sizeof (struct lifreq) * 2;
        P.buf = malloc (P.buf_size);
        if (P.buf == NULL) {
            retval = ENOMEM;
            goto punt;
        }

        code = get_lifconf (P.af, P.sock, &P.buf_size, P.buf);
        if (code < 0) {
            retval = errno;
            goto punt;
        }

        for (i = 0; i + sizeof(*lifr) <= P.buf_size; i+= sizeof (*lifr)) {
            lifr = (struct lifreq *)((caddr_t) P.buf+i);

            strncpy(lifreq.lifr_name, lifr->lifr_name,
                    sizeof (lifreq.lifr_name));
            Tprintf (("interface %s\n", lifreq.lifr_name));
            /*@-moduncon@*/ /* ioctl unknown to lclint */
            if (ioctl (P.sock, SIOCGLIFFLAGS, (char *)&lifreq) < 0) {
                Tperror ("ioctl(SIOCGLIFFLAGS)");
            skip:
                /* mark for next pass */
                lifr->lifr_name[0] = '\0';
                continue;
            }
            /*@=moduncon@*/

            /* None of the current callers want loopback addresses.  */
            if (is_loopback_address((struct sockaddr *)&lifr->lifr_addr)) {
                Tprintf (("  loopback\n"));
                goto skip;
            }
            /* Ignore interfaces that are down.  */
            if ((lifreq.lifr_flags & IFF_UP) == 0) {
                Tprintf (("  down\n"));
                goto skip;
            }

            /* Make sure we didn't process this address already.  */
            for (j = 0; j < i; j += sizeof (*lifr2)) {
                lifr2 = (struct lifreq *)((caddr_t) P.buf+j);
                if (lifr2->lifr_name[0] == '\0')
                    continue;
                if (lifr2->lifr_addr.ss_family == lifr->lifr_addr.ss_family
                    /* Compare address info.  If this isn't good enough --
                       i.e., if random padding bytes turn out to differ
                       when the addresses are the same -- then we'll have
                       to do it on a per address family basis.  */
                    && !memcmp (&lifr2->lifr_addr, &lifr->lifr_addr,
                                sizeof (*lifr))) {
                    Tprintf (("  duplicate addr\n"));
                    goto skip;
                }
            }

            /*@-moduncon@*/
            if ((*pass1fn) (data, ss2sa (&lifr->lifr_addr)))
                goto punt;
            /*@=moduncon@*/
        }
    }

    /* Did we actually get any working sockets?  */
    FOREACH_AF ()
        if (P.sock != -1)
            goto have_working_socket;
    retval = sock_err;
    goto punt;
have_working_socket:

    /*@-moduncon@*/
    if (betweenfn != NULL && (*betweenfn)(data))
        goto punt;
    /*@=moduncon@*/

    if (pass2fn)
        FOREACH_AF ()
            if (P.sock >= 0) {
                for (i = 0; i + sizeof (*lifr) <= P.buf_size; i+= sizeof (*lifr)) {
                    lifr = (struct lifreq *)((caddr_t) P.buf+i);

                    if (lifr->lifr_name[0] == '\0')
                        /* Marked in first pass to be ignored.  */
                        continue;

                    /*@-moduncon@*/
                    if ((*pass2fn) (data, ss2sa (&lifr->lifr_addr)))
                        goto punt;
                    /*@=moduncon@*/
                }
            }
punt:
    FOREACH_AF () {
        /*@-moduncon@*/
        closesocket(P.sock);
        /*@=moduncon@*/
        free (P.buf);
    }

    return retval;
}

#elif defined (SIOCGLIFNUM) && defined(HAVE_STRUCT_IF_LADDRCONF) && 0 /* HP-UX 11 support being debugged */

int
foreach_localaddr (/*@null@*/ void *data,
                   int (*pass1fn) (/*@null@*/ void *, struct sockaddr *) /*@*/,
                   /*@null@*/ int (*betweenfn) (/*@null@*/ void *) /*@*/,
                   /*@null@*/ int (*pass2fn) (/*@null@*/ void *,
                                              struct sockaddr *) /*@*/)
#if defined(DEBUG) || defined(TEST)
/*@modifies fileSystem@*/
#endif
{
    /* Okay, this is kind of odd.  We have to use each of the address
       families we care about, because with an AF_INET socket, extra
       interfaces like hme0:1 that have only AF_INET6 addresses will
       cause errors.  Similarly, if hme0 has more AF_INET addresses
       than AF_INET6 addresses, we won't be able to retrieve all of
       the AF_INET addresses if we use an AF_INET6 socket.  Since
       neither family is guaranteed to have the greater number of
       addresses, we should use both.

       If it weren't for this little quirk, we could use one socket of
       any type, and ask for addresses of all types.  At least, it
       seems to work that way.  */

    static const int afs[] = { AF_INET, AF_NS, AF_INET6 };
#define N_AFS (sizeof (afs) / sizeof (afs[0]))
    struct {
        int af;
        int sock;
        void *buf;
        size_t buf_size;
        int if_num;
    } afp[N_AFS];
    int code, i, j;
    int retval = 0, afidx;
    krb5_error_code sock_err = 0;
    struct if_laddrreq *lifr, lifreq, *lifr2;

#define FOREACH_AF() for (afidx = 0; afidx < N_AFS; afidx++)
#define P (afp[afidx])

    /* init */
    FOREACH_AF () {
        P.af = afs[afidx];
        P.sock = -1;
        P.buf = 0;
    }

    /* first pass: get raw data, discard uninteresting addresses, callback */
    FOREACH_AF () {
        Tprintf (("trying af %d...\n", P.af));
        P.sock = socket (P.af, USE_TYPE, USE_PROTO);
        if (P.sock < 0) {
            sock_err = SOCKET_ERROR;
            Tperror ("socket");
            continue;
        }
        set_cloexec_fd(P.sock);

        code = ioctl (P.sock, SIOCGLIFNUM, &P.if_num);
        if (code) {
            Tperror ("ioctl(SIOCGLIFNUM)");
            retval = errno;
            goto punt;
        }

        P.buf_size = P.if_num * sizeof (struct if_laddrreq) * 2;
        P.buf = malloc (P.buf_size);
        if (P.buf == NULL) {
            retval = ENOMEM;
            goto punt;
        }

        code = get_if_laddrconf (P.af, P.sock, &P.buf_size, P.buf);
        if (code < 0) {
            retval = errno;
            goto punt;
        }

        for (i = 0; i + sizeof(*lifr) <= P.buf_size; i+= sizeof (*lifr)) {
            lifr = (struct if_laddrreq *)((caddr_t) P.buf+i);

            strncpy(lifreq.iflr_name, lifr->iflr_name,
                    sizeof (lifreq.iflr_name));
            Tprintf (("interface %s\n", lifreq.iflr_name));
            /*@-moduncon@*/ /* ioctl unknown to lclint */
            if (ioctl (P.sock, SIOCGLIFFLAGS, (char *)&lifreq) < 0) {
                Tperror ("ioctl(SIOCGLIFFLAGS)");
            skip:
                /* mark for next pass */
                lifr->iflr_name[0] = '\0';
                continue;
            }
            /*@=moduncon@*/

            /* None of the current callers want loopback addresses.  */
            if (is_loopback_address(&lifr->iflr_addr)) {
                Tprintf (("  loopback\n"));
                goto skip;
            }
            /* Ignore interfaces that are down.  */
            if ((lifreq.iflr_flags & IFF_UP) == 0) {
                Tprintf (("  down\n"));
                goto skip;
            }

            /* Make sure we didn't process this address already.  */
            for (j = 0; j < i; j += sizeof (*lifr2)) {
                lifr2 = (struct if_laddrreq *)((caddr_t) P.buf+j);
                if (lifr2->iflr_name[0] == '\0')
                    continue;
                if (lifr2->iflr_addr.sa_family == lifr->iflr_addr.sa_family
                    /* Compare address info.  If this isn't good enough --
                       i.e., if random padding bytes turn out to differ
                       when the addresses are the same -- then we'll have
                       to do it on a per address family basis.  */
                    && !memcmp (&lifr2->iflr_addr, &lifr->iflr_addr,
                                sizeof (*lifr))) {
                    Tprintf (("  duplicate addr\n"));
                    goto skip;
                }
            }

            /*@-moduncon@*/
            if ((*pass1fn) (data, ss2sa (&lifr->iflr_addr)))
                goto punt;
            /*@=moduncon@*/
        }
    }

    /* Did we actually get any working sockets?  */
    FOREACH_AF ()
        if (P.sock != -1)
            goto have_working_socket;
    retval = sock_err;
    goto punt;
have_working_socket:

    /*@-moduncon@*/
    if (betweenfn != NULL && (*betweenfn)(data))
        goto punt;
    /*@=moduncon@*/

    if (pass2fn)
        FOREACH_AF ()
            if (P.sock >= 0) {
                for (i = 0; i + sizeof(*lifr) <= P.buf_size; i+= sizeof (*lifr)) {
                    lifr = (struct if_laddrreq *)((caddr_t) P.buf+i);

                    if (lifr->iflr_name[0] == '\0')
                        /* Marked in first pass to be ignored.  */
                        continue;

                    /*@-moduncon@*/
                    if ((*pass2fn) (data, ss2sa (&lifr->iflr_addr)))
                        goto punt;
                    /*@=moduncon@*/
                }
            }
punt:
    FOREACH_AF () {
        /*@-moduncon@*/
        closesocket(P.sock);
        /*@=moduncon@*/
        free (P.buf);
    }

    return retval;
}

#else /* not defined (SIOCGLIFNUM) */

#define SLOP (sizeof (struct ifreq) + 128)

static int
get_ifreq_array(char **bufp, size_t *np, int s)
{
    int code;
    int est_if_count = 8;
    size_t est_ifreq_size;
    char *buf = 0;
    size_t current_buf_size = 0, size, n;
#ifdef SIOCGSIZIFCONF
    int ifconfsize = -1;
#endif
#ifdef SIOCGIFNUM
    int numifs = -1;
#endif

    *bufp = NULL;
    *np = 0;

    /* At least on NetBSD, an ifreq can hold an IPv4 address, but
       isn't big enough for an IPv6 or ethernet address.  So add a
       little more space.  */
    est_ifreq_size = sizeof (struct ifreq) + 8;
#ifdef SIOCGSIZIFCONF
    code = ioctl (s, SIOCGSIZIFCONF, &ifconfsize);
    if (!code) {
        current_buf_size = ifconfsize;
        est_if_count = ifconfsize / est_ifreq_size;
    }
#elif defined (SIOCGIFNUM)
    code = ioctl (s, SIOCGIFNUM, &numifs);
    if (!code && numifs > 0)
        est_if_count = numifs;
#endif
    if (current_buf_size == 0)
        current_buf_size = est_ifreq_size * est_if_count + SLOP;
    buf = malloc (current_buf_size);
    if (buf == NULL)
        return ENOMEM;

ask_again:
    size = current_buf_size;
    code = get_ifconf (s, &size, buf);
    if (code < 0) {
        code = errno;
        free (buf);
        return code;
    }
    /* Test that the buffer was big enough that another ifreq could've
       fit easily, if the OS wanted to provide one.  That seems to be
       the only indication we get, complicated by the fact that the
       associated address may make the required storage a little
       bigger than the size of an ifreq.  */
    if (current_buf_size - size < SLOP
#ifdef SIOCGSIZIFCONF
        /* Unless we hear SIOCGSIZIFCONF is broken somewhere, let's
           trust the value it returns.  */
        && ifconfsize <= 0
#elif defined (SIOCGIFNUM)
        && numifs <= 0
#endif
        /* And we need *some* sort of bounds.  */
        && current_buf_size <= 100000
    ) {
        size_t new_size;

        est_if_count *= 2;
        new_size = est_ifreq_size * est_if_count + SLOP;
        buf = grow_or_free (buf, new_size);
        if (buf == 0)
            return ENOMEM;
        current_buf_size = new_size;
        goto ask_again;
    }

    n = size;
    if (n > current_buf_size)
        n = current_buf_size;

    *bufp = buf;
    *np = n;
    return 0;
}

int
foreach_localaddr (/*@null@*/ void *data,
                   int (*pass1fn) (/*@null@*/ void *, struct sockaddr *) /*@*/,
                   /*@null@*/ int (*betweenfn) (/*@null@*/ void *) /*@*/,
                   /*@null@*/ int (*pass2fn) (/*@null@*/ void *,
                                              struct sockaddr *) /*@*/)
#if defined(DEBUG) || defined(TEST)
/*@modifies fileSystem@*/
#endif
{
    struct ifreq *ifr, ifreq, *ifr2;
    int s;
    char *buf = 0;
    size_t n, i, j;
    int retval = 0;
#ifdef LINUX_IPV6_HACK
    struct linux_ipv6_addr_list *linux_ipv6_addrs = get_linux_ipv6_addrs ();
    struct linux_ipv6_addr_list *lx_v6;
#endif

    s = socket (USE_AF, USE_TYPE, USE_PROTO);
    if (s < 0)
        return SOCKET_ERRNO;
    set_cloexec_fd(s);

    retval = get_ifreq_array(&buf, &n, s);
    if (retval) {
        /*@-moduncon@*/ /* close() unknown to lclint */
        closesocket(s);
        /*@=moduncon@*/
        return retval;
    }

    /* Note: Apparently some systems put the size (used or wanted?)
       into the start of the buffer, just none that I'm actually
       using.  Fix this when there's such a test system available.
       The Samba mailing list archives mention that NTP looks for the
       size on these systems: *-fujitsu-uxp* *-ncr-sysv4*
       *-univel-sysv*.  */
    for (i = 0; i + sizeof(struct ifreq) <= n; i+= ifreq_size(*ifr) ) {
        ifr = (struct ifreq *)((caddr_t) buf+i);
        /* In case ifreq_size is more than sizeof().  */
        if (i + ifreq_size(*ifr) > n)
            break;

        strncpy(ifreq.ifr_name, ifr->ifr_name, sizeof (ifreq.ifr_name));
        Tprintf (("interface %s\n", ifreq.ifr_name));
        /*@-moduncon@*/ /* ioctl unknown to lclint */
        if (ioctl (s, SIOCGIFFLAGS, (char *)&ifreq) < 0) {
        skip:
            /* mark for next pass */
            ifr->ifr_name[0] = '\0';
            continue;
        }
        /*@=moduncon@*/

        /* None of the current callers want loopback addresses.  */
        if (is_loopback_address(&ifreq.ifr_addr)) {
            Tprintf (("  loopback\n"));
            goto skip;
        }
        /* Ignore interfaces that are down.  */
        if ((ifreq.ifr_flags & IFF_UP) == 0) {
            Tprintf (("  down\n"));
            goto skip;
        }

        /* Make sure we didn't process this address already.  */
        for (j = 0; j < i; j += ifreq_size(*ifr2)) {
            ifr2 = (struct ifreq *)((caddr_t) buf+j);
            if (ifr2->ifr_name[0] == '\0')
                continue;
            if (ifr2->ifr_addr.sa_family == ifr->ifr_addr.sa_family
                && ifreq_size (*ifr) == ifreq_size (*ifr2)
                /* Compare address info.  If this isn't good enough --
                   i.e., if random padding bytes turn out to differ
                   when the addresses are the same -- then we'll have
                   to do it on a per address family basis.  */
                && !memcmp (&ifr2->ifr_addr.sa_data, &ifr->ifr_addr.sa_data,
                            (ifreq_size (*ifr)
                             - offsetof (struct ifreq, ifr_addr.sa_data)))) {
                Tprintf (("  duplicate addr\n"));
                goto skip;
            }
        }

        /*@-moduncon@*/
        if ((*pass1fn) (data, &ifr->ifr_addr))
            goto punt;
        /*@=moduncon@*/
    }

#ifdef LINUX_IPV6_HACK
    for (lx_v6 = linux_ipv6_addrs; lx_v6; lx_v6 = lx_v6->next)
        if ((*pass1fn) (data, (struct sockaddr *) &lx_v6->addr))
            goto punt;
#endif

    /*@-moduncon@*/
    if (betweenfn != NULL && (*betweenfn)(data))
        goto punt;
    /*@=moduncon@*/

    if (pass2fn) {
        for (i = 0; i + sizeof(struct ifreq) <= n; i+= ifreq_size(*ifr) ) {
            ifr = (struct ifreq *)((caddr_t) buf+i);

            if (ifr->ifr_name[0] == '\0')
                /* Marked in first pass to be ignored.  */
                continue;

            /*@-moduncon@*/
            if ((*pass2fn) (data, &ifr->ifr_addr))
                goto punt;
            /*@=moduncon@*/
        }
#ifdef LINUX_IPV6_HACK
        for (lx_v6 = linux_ipv6_addrs; lx_v6; lx_v6 = lx_v6->next)
            if ((*pass2fn) (data, (struct sockaddr *) &lx_v6->addr))
                goto punt;
#endif
    }
punt:
    /*@-moduncon@*/
    closesocket(s);
    /*@=moduncon@*/
    free (buf);
#ifdef LINUX_IPV6_HACK
    while (linux_ipv6_addrs) {
        lx_v6 = linux_ipv6_addrs->next;
        free (linux_ipv6_addrs);
        linux_ipv6_addrs = lx_v6;
    }
#endif

    return retval;
}

#endif /* not HAVE_IFADDRS_H and not SIOCGLIFNUM */

static krb5_error_code
get_localaddrs (krb5_context context, krb5_address ***addr, int use_profile);

#ifdef TEST

static int print_addr (/*@unused@*/ void *dataptr, struct sockaddr *sa)
/*@modifies fileSystem@*/
{
    char hostbuf[NI_MAXHOST];
    int err;
    socklen_t len;

    printf ("  --> family %2d ", sa->sa_family);
    len = sa_socklen (sa);
    err = getnameinfo (sa, len, hostbuf, (socklen_t) sizeof (hostbuf),
                       (char *) NULL, 0, NI_NUMERICHOST);
    if (err) {
        int e = errno;
        printf ("<getnameinfo error %d: %s>\n", err, gai_strerror (err));
        if (err == EAI_SYSTEM)
            printf ("\t\t<errno is %d: %s>\n", e, strerror(e));
    } else
        printf ("addr %s\n", hostbuf);
    return 0;
}

int main ()
{
    int r;

    (void) setvbuf (stdout, (char *)NULL, _IONBF, 0);
    r = foreach_localaddr (0, print_addr, NULL, NULL);
    printf ("return value = %d\n", r);
    return 0;
}

#else /* not TESTing */

struct localaddr_data {
    int count, mem_err, cur_idx, cur_size;
    krb5_address **addr_temp;
};

static int
count_addrs (void *P_data, struct sockaddr *a)
/*@*/
{
    struct localaddr_data *data = P_data;
    switch (a->sa_family) {
    case AF_INET:
    case AF_INET6:
#ifdef KRB5_USE_NS
    case AF_XNS:
#endif
        data->count++;
        break;
    default:
        break;
    }
    return 0;
}

static int
allocate (void *P_data)
/*@*/
{
    struct localaddr_data *data = P_data;
    int i;
    void *n;

    n = realloc (data->addr_temp,
                 (1 + data->count + data->cur_idx) * sizeof (krb5_address *));
    if (n == 0) {
        data->mem_err++;
        return 1;
    }
    data->addr_temp = n;
    data->cur_size = 1 + data->count + data->cur_idx;
    for (i = data->cur_idx; i <= data->count + data->cur_idx; i++)
        data->addr_temp[i] = 0;
    return 0;
}

static /*@null@*/ krb5_address *
make_addr (int type, size_t length, const void *contents)
/*@*/
{
    krb5_address *a;
    void *data;

    data = malloc (length);
    if (data == NULL)
        return NULL;
    a = malloc (sizeof (krb5_address));
    if (a == NULL) {
        free (data);
        return NULL;
    }
    memcpy (data, contents, length);
    a->magic = KV5M_ADDRESS;
    a->addrtype = type;
    a->length = length;
    a->contents = data;
    return a;
}

static int
add_addr (void *P_data, struct sockaddr *a)
/*@modifies *P_data@*/
{
    struct localaddr_data *data = P_data;
    /*@null@*/ krb5_address *address = 0;

    switch (a->sa_family) {
#ifdef HAVE_NETINET_IN_H
    case AF_INET:
        address = make_addr (ADDRTYPE_INET, sizeof (struct in_addr),
                             &sa2sin(a)->sin_addr);
        if (address == NULL)
            data->mem_err++;
        break;

    case AF_INET6:
    {
        const struct sockaddr_in6 *in = sa2sin6(a);

        if (IN6_IS_ADDR_LINKLOCAL (&in->sin6_addr))
            break;

        address = make_addr (ADDRTYPE_INET6, sizeof (struct in6_addr),
                             &in->sin6_addr);
        if (address == NULL)
            data->mem_err++;
        break;
    }
#endif /* netinet/in.h */

#ifdef KRB5_USE_NS
    case AF_XNS:
        address = make_addr (ADDRTYPE_XNS, sizeof (struct ns_addr),
                             &((const struct sockaddr_ns *)a)->sns_addr);
        if (address == NULL)
            data->mem_err++;
        break;
#endif

#ifdef AF_LINK
        /* Some BSD-based systems (e.g. NetBSD 1.5) and AIX will
           include the ethernet address, but we don't want that, at
           least for now.  */
    case AF_LINK:
        break;
#endif
        /*
         * Add more address families here..
         */
    default:
        break;
    }
#ifdef __LCLINT__
    /* Redundant but unconditional store un-confuses lclint.  */
    data->addr_temp[data->cur_idx] = address;
#endif
    if (address) {
        data->addr_temp[data->cur_idx++] = address;
    }

    return data->mem_err;
}

static krb5_error_code
krb5_os_localaddr_profile (krb5_context context, struct localaddr_data *datap)
{
    krb5_error_code err;
    static const char *const profile_name[] = {
        KRB5_CONF_LIBDEFAULTS, KRB5_CONF_EXTRA_ADDRESSES, 0
    };
    char **values;
    char **iter;
    krb5_address **newaddrs;

#ifdef DEBUG
    fprintf (stderr, "looking up extra_addresses foo\n");
#endif

    err = profile_get_values (context->profile, profile_name, &values);
    /* Ignore all errors for now?  */
    if (err)
        return 0;

    for (iter = values; *iter; iter++) {
        char *cp = *iter, *next, *current;
        int i, count;

#ifdef DEBUG
        fprintf (stderr, "  found line: '%s'\n", cp);
#endif

        for (cp = *iter, next = 0; *cp; cp = next) {
            while (isspace ((int) *cp) || *cp == ',')
                cp++;
            if (*cp == 0)
                break;
            /* Start of an address.  */
#ifdef DEBUG
            fprintf (stderr, "    addr found in '%s'\n", cp);
#endif
            current = cp;
            while (*cp != 0 && !isspace((int) *cp) && *cp != ',')
                cp++;
            if (*cp != 0) {
                next = cp + 1;
                *cp = 0;
            } else
                next = cp;
            /* Got a single address, process it.  */
#ifdef DEBUG
            fprintf (stderr, "    processing '%s'\n", current);
#endif
            newaddrs = 0;
            err = k5_os_hostaddr (context, current, &newaddrs);
            if (err)
                continue;
            for (i = 0; newaddrs[i]; i++) {
#ifdef DEBUG
                fprintf (stderr, "    %d: family %d", i,
                         newaddrs[i]->addrtype);
                fprintf (stderr, "\n");
#endif
            }
            count = i;
#ifdef DEBUG
            fprintf (stderr, "    %d addresses\n", count);
#endif
            if (datap->cur_idx + count >= datap->cur_size) {
                krb5_address **bigger;
                bigger = realloc (datap->addr_temp,
                                  sizeof (krb5_address *) * (datap->cur_idx + count));
                if (bigger) {
                    datap->addr_temp = bigger;
                    datap->cur_size = datap->cur_idx + count;
                }
            }
            for (i = 0; i < count; i++) {
                if (datap->cur_idx < datap->cur_size)
                    datap->addr_temp[datap->cur_idx++] = newaddrs[i];
                else
                    free (newaddrs[i]->contents), free (newaddrs[i]);
            }
            free (newaddrs);
        }
    }
    return 0;
}

krb5_error_code KRB5_CALLCONV
krb5_os_localaddr(krb5_context context, krb5_address ***addr)
{
    return get_localaddrs(context, addr, 1);
}

static krb5_error_code
get_localaddrs (krb5_context context, krb5_address ***addr, int use_profile)
{
    struct localaddr_data data = { 0 };
    int r;

    /* Ignore errors for now. */
    if (use_profile)
        (void)krb5_os_localaddr_profile (context, &data);

    r = foreach_localaddr (&data, count_addrs, allocate, add_addr);
    if (r != 0) {
        int i;
        if (data.addr_temp) {
            for (i = 0; i < data.count; i++)
                free (data.addr_temp[i]);
            free (data.addr_temp);
        }
        if (data.mem_err)
            return ENOMEM;
        else
            return r;
    }

    data.cur_idx++; /* null termination */
    if (data.mem_err)
        return ENOMEM;
    else if (data.cur_idx == data.count)
        *addr = data.addr_temp;
    else {
        /* This can easily happen if we have IPv6 link-local
           addresses.  Just shorten the array.  */
        *addr = (krb5_address **) realloc (data.addr_temp,
                                           (sizeof (krb5_address *)
                                            * data.cur_idx));
        if (*addr == 0)
            /* Okay, shortening failed, but the original should still
               be intact.  */
            *addr = data.addr_temp;
    }

#ifdef DEBUG
    {
        int j;
        fprintf (stderr, "addresses:\n");
        for (j = 0; addr[0][j]; j++) {
            struct sockaddr_storage ss;
            int err2;
            char namebuf[NI_MAXHOST];
            void *addrp = 0;

            fprintf (stderr, "%2d: ", j);
            fprintf (stderr, "addrtype %2d, length %2d", addr[0][j]->addrtype,
                     addr[0][j]->length);
            memset (&ss, 0, sizeof (ss));
            switch (addr[0][j]->addrtype) {
            case ADDRTYPE_INET:
            {
                struct sockaddr_in *sinp = ss2sin (&ss);
                sinp->sin_family = AF_INET;
                addrp = &sinp->sin_addr;
                break;
            }
            case ADDRTYPE_INET6:
            {
                struct sockaddr_in6 *sin6p = ss2sin6 (&ss);
                sin6p->sin6_family = AF_INET6;
                addrp = &sin6p->sin6_addr;
                break;
            }
            default:
                ss2sa(&ss)->sa_family = 0;
                break;
            }
            if (addrp)
                memcpy (addrp, addr[0][j]->contents, addr[0][j]->length);
            err2 = getnameinfo (ss2sa(&ss), sa_socklen (ss2sa (&ss)),
                                namebuf, sizeof (namebuf), 0, 0,
                                NI_NUMERICHOST);
            if (err2 == 0)
                fprintf (stderr, ": addr %s\n", namebuf);
            else
                fprintf (stderr, ": getnameinfo error %d\n", err2);
        }
    }
#endif

    return 0;
}

#endif /* not TESTing */

#else /* Windows/Mac version */

/*
 * Hold on to your lunch!  Backup kludge method of obtaining your
 * local IP address, courtesy of Windows Socket Network Programming,
 * by Robert Quinn
 */
#if defined(_WIN32)
static struct hostent *local_addr_fallback_kludge()
{
    static struct hostent   host;
    static SOCKADDR_IN      addr;
    static char *           ip_ptrs[2];
    SOCKET                  sock;
    int                     size = sizeof(SOCKADDR);
    int                     err;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET)
        return NULL;
    set_cloexec_fd(sock);

    /* connect to arbitrary port and address (NOT loopback) */
    addr.sin_family = AF_INET;
    addr.sin_port = htons(IPPORT_ECHO);
    addr.sin_addr.s_addr = inet_addr("204.137.220.51");

    err = connect(sock, (LPSOCKADDR) &addr, sizeof(SOCKADDR));
    if (err == SOCKET_ERROR)
        return NULL;

    err = getsockname(sock, (LPSOCKADDR) &addr, (int *) size);
    if (err == SOCKET_ERROR)
        return NULL;

    closesocket(sock);

    host.h_name = 0;
    host.h_aliases = 0;
    host.h_addrtype = AF_INET;
    host.h_length = 4;
    host.h_addr_list = ip_ptrs;
    ip_ptrs[0] = (char *) &addr.sin_addr.s_addr;
    ip_ptrs[1] = NULL;

    return &host;
}
#endif

/* No ioctls in winsock so we just assume there is only one networking
 * card per machine, so gethostent is good enough.
 */
krb5_error_code KRB5_CALLCONV
krb5_os_localaddr (krb5_context context, krb5_address ***addr) {
    char host[64];                              /* Name of local machine */
    struct hostent *hostrec;
    int err, count, i;
    krb5_address ** paddr;

    *addr = 0;
    paddr = 0;
    err = 0;

    if (gethostname (host, sizeof(host))) {
        err = SOCKET_ERRNO;
    }

    if (!err) {
        hostrec = gethostbyname (host);
        if (hostrec == NULL) {
            err = SOCKET_ERRNO;
        }
    }

    if (err) {
        hostrec = local_addr_fallback_kludge();
        if (!hostrec)
            return err;
        else
            err = 0;  /* otherwise we will die at cleanup */
    }

    for (count = 0; hostrec->h_addr_list[count]; count++);


    paddr = (krb5_address **)calloc(count+1, sizeof(krb5_address *));
    if (!paddr) {
        err = ENOMEM;
        goto cleanup;
    }

    for (i = 0; i < count; i++) {
        paddr[i] = (krb5_address *)malloc(sizeof(krb5_address));
        if (paddr[i] == NULL) {
            err = ENOMEM;
            goto cleanup;
        }

        paddr[i]->magic = KV5M_ADDRESS;
        paddr[i]->addrtype = hostrec->h_addrtype;
        paddr[i]->length = hostrec->h_length;
        paddr[i]->contents = (unsigned char *)malloc(paddr[i]->length);
        if (!paddr[i]->contents) {
            err = ENOMEM;
            goto cleanup;
        }
        memcpy(paddr[i]->contents,
               hostrec->h_addr_list[i],
               paddr[i]->length);
    }

cleanup:
    if (err) {
        if (paddr) {
            for (i = 0; i < count; i++)
            {
                if (paddr[i]) {
                    if (paddr[i]->contents)
                        free(paddr[i]->contents);
                    free(paddr[i]);
                }
            }
            free(paddr);
        }
    }
    else
        *addr = paddr;

    return(err);
}
#endif
