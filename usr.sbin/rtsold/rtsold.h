/*	$KAME: rtsold.h,v 1.11 2000/10/10 06:18:04 itojun Exp $	*/

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

struct ifinfo {
	struct ifinfo *next;	/* pointer to the next interface */

	struct sockaddr_dl *sdl; /* link-layer address */
	char ifname[IF_NAMESIZE]; /* interface name */
	int active;		/* interface status */
	int probeinterval;	/* interval of probe timer(if necessary) */
	int probetimer;		/* rest of probe timer */
	int mediareqok;		/* wheter the IF supports SIOCGIFMEDIA */
	int otherconfig;	/* need a separate protocol for the "other"
				 * configuration */
	int state;
	int probes;
	int dadcount;
	struct timeval timer;
	struct timeval expire;
	int errors;		/* # of errors we've got - detect wedge */

	int racnt;		/* total # of valid RAs it have got */

	size_t rs_datalen;
	u_char *rs_data;
};

/* per interface status */
#define IFS_IDLE	0
#define IFS_DELAY	1
#define IFS_PROBE	2
#define IFS_DOWN	3
#define IFS_TENTATIVE	4

/* rtsold.c */
extern struct timeval tm_max;
extern int dflag;
extern char *otherconf_script;
struct ifinfo *find_ifinfo __P((int ifindex));
void rtsol_timer_update __P((struct ifinfo *ifinfo));
extern void warnmsg __P((int, const char *, const char *, ...))
     __attribute__((__format__(__printf__, 3, 4)));

/* if.c */
extern int ifinit __P((void));
extern int interface_up __P((char *name));
extern int interface_status __P((struct ifinfo*));
extern int lladdropt_length __P((struct sockaddr_dl *sdl));
extern void lladdropt_fill __P((struct sockaddr_dl *sdl,
				struct nd_opt_hdr *ndopt));
extern struct sockaddr_dl *if_nametosdl __P((char *name));
extern int getinet6sysctl __P((int code));

/* rtsol.c */
extern int sockopen __P((void));
extern void sendpacket __P((struct ifinfo *ifinfo));
extern void rtsol_input __P((int s));

/* probe.c */
extern int probe_init __P((void));
extern void defrouter_probe __P((int ifindex));

/* dump.c */
extern void rtsold_dump_file __P((char *));

/* rtsock.c */
extern int rtsock_open __P((void));
extern int rtsock_input __P((int));
