/*
 * pppd.h - PPP daemon global declarations.
 *
 * Copyright (c) 1989 Carnegie Mellon University.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by Carnegie Mellon University.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

/*
 * TODO:
 */

#ifndef __PPPD_H__
#define __PPPD_H__
#include "args.h"
#define NPPP	1		/* One PPP interface supported (per process) */

extern int debug;		/* Debug flag */
extern char ifname[];		/* Interface name */
extern int fd;			/* Device file descriptor */
extern int s;			/* socket descriptor */
extern char hostname[];		/* hostname */
extern u_char hostname_len;	/* and its length */
extern u_char outpacket_buf[];	/* buffer for outgoing packets */

#define MAX_HOSTNAME_LEN 128	/* should be 255 - MAX_CHALLENGE_LEN + 1 */

void quit __ARGS((void));			/* Cleanup and exit */
void timeout __ARGS((void (*)(), caddr_t, int));
                            /* Look-alike of kernel's timeout() */
void untimeout __ARGS((void (*)(), caddr_t));
                            /* Look-alike of kernel's untimeout() */
void output __ARGS((int, u_char *, int));	/* Output a PPP packet */
void demuxprotrej __ARGS((int, int));	/* Demultiplex a Protocol-Reject */
u_char login __ARGS((char *, int, char *, int, char **, int *)); /* Login user */
void logout __ARGS((void));			/* Logout user */
void get_secret __ARGS((u_char *, u_char *, int *)); /* get "secret" for chap */
u_long GetMask __ARGS((u_long)); /* get netmask for address */
extern int errno;


/*
 * Inline versions of get/put char/short/long.
 * Pointer is advanced; we assume that both arguments
 * are lvalues and will already be in registers.
 * cp MUST be u_char *.
 */
#define GETCHAR(c, cp) { \
	(c) = *(cp)++; \
}
#define PUTCHAR(c, cp) { \
	*(cp)++ = (c); \
}


#define GETSHORT(s, cp) { \
	(s) = *(cp)++ << 8; \
	(s) |= *(cp)++; \
}
#define PUTSHORT(s, cp) { \
	*(cp)++ = (s) >> 8; \
	*(cp)++ = (s); \
}

#define GETLONG(l, cp) { \
	(l) = *(cp)++ << 8; \
	(l) |= *(cp)++; (l) <<= 8; \
	(l) |= *(cp)++; (l) <<= 8; \
	(l) |= *(cp)++; \
}
#define PUTLONG(l, cp) { \
	*(cp)++ = (l) >> 24; \
	*(cp)++ = (l) >> 16; \
	*(cp)++ = (l) >> 8; \
	*(cp)++ = (l); \
}

#define INCPTR(n, cp)	((cp) += (n))
#define DECPTR(n, cp)	((cp) -= (n))

/*
 * System dependent definitions for user-level 4.3BSD UNIX implementation.
 */

#define DEMUXPROTREJ(u, p) demuxprotrej(u, p)

#define TIMEOUT(r, f, t) timeout((r), (f), (t))
#define UNTIMEOUT(r, f)	untimeout((r), (f))

#define BCOPY(s, d, l)	bcopy(s, d, l)
#define EXIT(u)		quit()

#define GETUSERPASSWD(u)
#define LOGIN(n, u, ul, p, pl, m, ml) login(u, ul, p, pl, m, ml);
#define LOGOUT(n)	logout()
#define GETSECRET(n, s, sl) get_secret(n, s, sl)
#define PRINTMSG(m, l)	{ m[l] = '\0'; syslog(LOG_INFO, "Remote message: %s", m); }

/*
 * return a pointer to the beginning of the data part of a packet.
 */

#define PACKET_DATA(p) (p + DLLHEADERLEN)

/*
 * MAKEHEADER - Add Header fields to a packet.  (Should we do
 * AC compression here?)
 */
#define MAKEHEADER(p, t) { \
    PUTCHAR(ALLSTATIONS, p); \
    PUTCHAR(UI, p); \
    PUTSHORT(t, p); }

/*
 * SIFASYNCMAP - Config the interface async map.
 */
#ifdef STREAMS
#define	SIFASYNCMAP(u, a)	{ \
     u_long x = a; \
     if(ioctl(fd, SIOCSIFASYNCMAP, (caddr_t) &x) < 0) { \
	syslog(LOG_ERR, "ioctl(SIOCSIFASYNCMAP): %m"); \
    } }
#else
#define SIFASYNCMAP(u, a)	{ \
    u_long x = a; \
    if (ioctl(fd, PPPIOCSASYNCMAP, (caddr_t) &x) < 0) { \
	syslog(LOG_ERR, "ioctl(PPPIOCSASYNCMAP): %m"); \
	quit(); \
    } }
#endif

/*
 * SIFPCOMPRESSION - Config the interface for protocol compression.
 */
#ifdef	STREAMS
#define SIFPCOMPRESSION(u)	{ \
    char c = 1; \
    if(ioctl(fd, SIOCSIFCOMPPROT, &c) < 0) { \
	syslog(LOG_ERR, "ioctl(SIOCSIFCOMPPROT): %m"); \
    }}
#else
#define SIFPCOMPRESSION(u)	{ \
    u_int x; \
    if (ioctl(fd, PPPIOCGFLAGS, (caddr_t) &x) < 0) { \
	syslog(LOG_ERR, "ioctl (PPPIOCGFLAGS): %m"); \
	quit(); \
    } \
    x |= SC_COMP_PROT; \
    if (ioctl(fd, PPPIOCSFLAGS, (caddr_t) &x) < 0) { \
	syslog(LOG_ERR, "ioctl(PPPIOCSFLAGS): %m"); \
	quit(); \
    } }
#endif

/*
 * CIFPCOMPRESSION - Config the interface for no protocol compression.
 */
#ifdef	STREAMS
#define CIFPCOMPRESSION(u)	{ \
    char c = 0; \
    if(ioctl(fd, SIOCSIFCOMPPROT, &c) < 0) { \
	syslog(LOG_ERR, "ioctl(SIOCSIFCOMPPROT): %m"); \
	quit(); \
    }}
#else
#define CIFPCOMPRESSION(u)	{ \
    u_int x; \
    if (ioctl(fd, PPPIOCGFLAGS, (caddr_t) &x) < 0) { \
	syslog(LOG_ERR, "ioctl(PPPIOCGFLAGS): %m"); \
	quit(); \
    } \
    x &= ~SC_COMP_PROT; \
    if (ioctl(fd, PPPIOCSFLAGS, (caddr_t) &x) < 0) { \
	syslog(LOG_ERR, "ioctl(PPPIOCSFLAGS): %m"); \
	quit(); \
    } }
#endif

/*
 * SIFACCOMPRESSION - Config the interface for address/control compression.
 */
#ifdef	STREAMS
#define SIFACCOMPRESSION(u)	{ \
   char c = 1; \
    if(ioctl(fd, SIOCSIFCOMPAC, &c) < 0) { \
	syslog(LOG_ERR, "ioctl(SIOCSIFCOMPAC): %m"); \
	quit(); \
    }}
#else
#define SIFACCOMPRESSION(u)	{ \
    u_int x; \
    if (ioctl(fd, PPPIOCGFLAGS, (caddr_t) &x) < 0) { \
	syslog(LOG_ERR, "ioctl (PPPIOCGFLAGS): %m"); \
	quit(); \
    } \
    x |= SC_COMP_AC; \
    if (ioctl(fd, PPPIOCSFLAGS, (caddr_t) &x) < 0) { \
	syslog(LOG_ERR, "ioctl(PPPIOCSFLAGS): %m"); \
	quit(); \
    } }
#endif

/*
 * CIFACCOMPRESSION - Config the interface for no address/control compression.
 */
#ifdef	STREAMS
#define CIFACCOMPRESSION(u)	{ \
    char c = 0; \
    if(ioctl(fd, SIOCSIFCOMPAC, &c) < 0) { \
	syslog(LOG_ERR, "ioctl(SIOCSIFCOMPAC): %m"); \
	quit(); \
    }}
#else
#define CIFACCOMPRESSION(u)	{ \
    u_int x; \
    if (ioctl(fd, PPPIOCGFLAGS, (caddr_t) &x) < 0) { \
	syslog(LOG_ERR, "ioctl (PPPIOCGFLAGS): %m"); \
	quit(); \
    } \
    x &= ~SC_COMP_AC; \
    if (ioctl(fd, PPPIOCSFLAGS, (caddr_t) &x) < 0) { \
	syslog(LOG_ERR, "ioctl(PPPIOCSFLAGS): %m"); \
	quit(); \
    } }
#endif

/*
 * SIFVJCOMP - config tcp header compression
 */
#ifdef STREAMS
#define	SIFVJCOMP(u, a) 	{ \
    char x = a;			\
	if (debug) syslog(LOG_DEBUG, "SIFVJCOMP unit %d to value %d\n",u,x); \
	if(ioctl(fd, SIOCSIFVJCOMP, (caddr_t) &x) < 0) { \
		syslog(LOG_ERR, "ioctl(SIOCSIFVJCOMP): %m"); \
		quit(); \
	} \
}
#else
#define	SIFVJCOMP(u, a) 	{ \
    u_int x; \
    if (debug) \
	syslog(LOG_DEBUG, "PPPIOCSFLAGS unit %d set %s\n",u,a?"on":"off"); \
    if (ioctl(fd, PPPIOCGFLAGS, (caddr_t) &x) < 0) { \
	syslog(LOG_ERR, "ioctl (PPPIOCGFLAGS): %m"); \
	quit(); \
    } \
    x = (x & ~SC_COMP_TCP) | ((a) ? SC_COMP_TCP : 0); \
    if(ioctl(fd, PPPIOCSFLAGS, (caddr_t) &x) < 0) { \
	syslog(LOG_ERR, "ioctl(PPPIOCSFLAGS): %m"); \
	quit(); \
    } }
#endif

/*
 * SIFUP - Config the interface up.
 */
#define SIFUP(u)	{ \
    struct ifreq ifr; \
    strncpy(ifr.ifr_name, ifname, sizeof (ifr.ifr_name)); \
    if (ioctl(s, SIOCGIFFLAGS, (caddr_t) &ifr) < 0) { \
	syslog(LOG_ERR, "ioctl (SIOCGIFFLAGS): %m"); \
	quit(); \
    } \
    ifr.ifr_flags |= IFF_UP; \
    if (ioctl(s, SIOCSIFFLAGS, (caddr_t) &ifr) < 0) { \
	syslog(LOG_ERR, "ioctl(SIOCSIFFLAGS): %m"); \
	quit(); \
    } }

/*
 * SIFDOWN - Config the interface down.
 */
#define SIFDOWN(u)	{ \
    struct ifreq ifr; \
    strncpy(ifr.ifr_name, ifname, sizeof (ifr.ifr_name)); \
    if (ioctl(s, SIOCGIFFLAGS, (caddr_t) &ifr) < 0) { \
	syslog(LOG_ERR, "ioctl (SIOCGIFFLAGS): %m"); \
	quit(); \
    } \
    ifr.ifr_flags &= ~IFF_UP; \
    if (ioctl(s, SIOCSIFFLAGS, (caddr_t) &ifr) < 0) { \
	syslog(LOG_ERR, "ioctl(SIOCSIFFLAGS): %m"); \
	quit(); \
    } }

/*
 * SIFMTU - Config the interface MTU.
 */
#define SIFMTU(u, m)	{ \
    struct ifreq ifr; \
    strncpy(ifr.ifr_name, ifname, sizeof (ifr.ifr_name)); \
    ifr.ifr_mtu = m; \
    if (ioctl(s, SIOCSIFMTU, (caddr_t) &ifr) < 0) { \
	syslog(LOG_ERR, "ioctl(SIOCSIFMTU): %m"); \
	quit(); \
    } }


#ifdef __386BSD__ /* BSD >= 44 ? */
#define SET_SA_FAMILY(addr, family)		\
    bzero((char *) &(addr), sizeof(addr));	\
    addr.sa_family = (family); 			\
    addr.sa_len = sizeof(addr);
#else
#define SET_SA_FAMILY(addr, family)		\
    bzero((char *) &(addr), sizeof(addr));	\
    addr.sa_family = (family);
#endif

/*
 * SIFADDR - Config the interface IP addresses.
 */
#define SIFADDR(u, o, h)	{ \
    struct ifreq ifr; \
    strncpy(ifr.ifr_name, ifname, sizeof (ifr.ifr_name)); \
    SET_SA_FAMILY(ifr.ifr_addr, AF_INET); \
    ((struct sockaddr_in *) &ifr.ifr_addr)->sin_addr.s_addr = o; \
    if (ioctl(s, SIOCSIFADDR, (caddr_t) &ifr) < 0) { \
	syslog(LOG_ERR, "ioctl(SIOCSIFADDR): %m"); \
    } \
    ((struct sockaddr_in *) &ifr.ifr_dstaddr)->sin_addr.s_addr = h; \
    if (ioctl(s, SIOCSIFDSTADDR, (caddr_t) &ifr) < 0) { \
	syslog(LOG_ERR, "ioctl(SIOCSIFDSTADDR): %m"); \
	quit(); \
    } }

/*
 * CIFADDR - Clear the interface IP addresses.
 */
#if BSD > 43
#define CIFADDR(u, o, h)	{ \
    struct ortentry rt; \
    SET_SA_FAMILY(rt.rt_dst, AF_INET); \
    ((struct sockaddr_in *) &rt.rt_dst)->sin_addr.s_addr = h; \
    SET_SA_FAMILY(rt.rt_gateway, AF_INET); \
    ((struct sockaddr_in *) &rt.rt_gateway)->sin_addr.s_addr = o; \
    rt.rt_flags |= RTF_HOST; \
    syslog(LOG_INFO, "Deleting host route from %s to %s\n", \
	   ip_ntoa(h), ip_ntoa(o)); \
    if (ioctl(s, SIOCDELRT, (caddr_t) &rt) < 0) { \
	syslog(LOG_ERR, "ioctl(SIOCDELRT): %m"); \
    } }
#else
#define CIFADDR(u, o, h)	{ \
    struct rtentry rt; \
    SET_SA_FAMILY(rt.rt_dst, AF_INET); \
    ((struct sockaddr_in *) &rt.rt_dst)->sin_addr.s_addr = h; \
    SET_SA_FAMILY(rt.rt_gateway, AF_INET); \
    ((struct sockaddr_in *) &rt.rt_gateway)->sin_addr.s_addr = o; \
    rt.rt_flags |= RTF_HOST; \
    syslog(LOG_INFO, "Deleting host route from %s to %s\n", \
	   ip_ntoa(h), ip_ntoa(o)); \
    if (ioctl(s, SIOCDELRT, (caddr_t) &rt) < 0) { \
	syslog(LOG_ERR, "ioctl(SIOCDELRT): %m"); \
    } }
#endif

/*
 * SIFMASK - Config the interface net mask
 */
#define SIFMASK(u, m)	{ \
    struct ifreq ifr; \
    strncpy(ifr.ifr_name, ifname, sizeof (ifr.ifr_name)); \
    SET_SA_FAMILY(ifr.ifr_addr, AF_INET); \
    ((struct sockaddr_in *) &ifr.ifr_addr)->sin_addr.s_addr = m; \
    syslog(LOG_INFO, "Setting interface mask to %s\n", ip_ntoa(m)); \
    if (ioctl(s, SIOCSIFNETMASK, (caddr_t) &ifr) < 0) { \
	syslog(LOG_ERR, "ioctl(SIOCSIFADDR): %m"); \
    } }

#ifndef LOG_PPP			/* we use LOG_LOCAL2 for syslog by default */
#if defined(DEBUGMAIN) || defined(DEBUGFSM) || defined(DEBUG) \
  || defined(DEBUGLCP) || defined(DEBUGIPCP) || defined(DEBUGUPAP) \
  || defined(DEBUGCHAP) 
#define LOG_PPP LOG_LOCAL2
#else
#define LOG_PPP LOG_DAEMON
#endif
#endif /* LOG_PPP */

#ifdef DEBUGMAIN
#define MAINDEBUG(x)	if (debug) syslog x;
#else
#define MAINDEBUG(x)
#endif

#ifdef DEBUGFSM
#define FSMDEBUG(x)	if (debug) syslog x;
#else
#define FSMDEBUG(x)
#endif

#ifdef DEBUGLCP
#define LCPDEBUG(x)	if (debug) syslog x;
#else
#define LCPDEBUG(x)
#endif

#ifdef DEBUGIPCP
#define IPCPDEBUG(x)	if (debug) syslog x;
#else
#define IPCPDEBUG(x)
#endif

#ifdef DEBUGUPAP
#define UPAPDEBUG(x)	if (debug) syslog x;
#else
#define UPAPDEBUG(x)
#endif

#ifdef DEBUGCHAP
#define CHAPDEBUG(x)	if (debug) syslog x;
#else
#define CHAPDEBUG(x)
#endif

#ifndef SIGTYPE
#if defined(sun) || defined(SYSV) || defined(POSIX_SOURCE)
#define SIGTYPE void
#else
#define SIGTYPE int
#endif /* defined(sun) || defined(SYSV) || defined(POSIX_SOURCE) */
#endif /* SIGTYPE */
#endif /* __PPP_H__ */
