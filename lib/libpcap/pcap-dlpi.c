/*
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * This code contributed by Atanu Ghosh (atanu@cs.ucl.ac.uk),
 * University College London.
 */
#ifndef lint
static  char rcsid[] =
    "@(#)$Header: /pub/FreeBSD/FreeBSD-CVS/src/lib/libpcap/pcap-dlpi.c,v 1.1.1.1 1995/01/20 04:13:03 jkh Exp $ (LBL)";
#endif

/*
 * Packet capture routine for dlpi under SunOS 5
 *
 * Notes:
 *
 *    - Apparently the DLIOCRAW ioctl() is specific to SunOS.
 *
 *    - There is a bug in bufmod(7) such that setting the snapshot
 *      length results in data being left of the front of the packet.
 *
 *    - It might be desirable to use pfmod(7) to filter packets in the
 *      kernel.
 */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/bufmod.h>
#include <sys/dlpi.h>
#include <sys/stream.h>
#include <sys/systeminfo.h>

#include <net/bpf.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <memory.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stropts.h>
#include <unistd.h>

#include "pcap-int.h"

#define	MAXDLBUF	8192

/* Forwards */
static int send_request(int, char *, int, char *, char *);
static int dlattachreq(int, u_long, char *);
static int dlinfoack(int, char *, char *);
static int dlinforeq(int, char *);
static int dlpromisconreq(int, u_long, char *);
static int dlokack(int, char *, char *);
static int strioctl(int, int, int, char *);
#ifdef SOLARIS
static char *getrelease(long *, long *, long *);
#endif

int
pcap_stats(pcap_t *p, struct pcap_stat *ps)
{

	*ps = p->md.stat;
	return (0);
}

int
pcap_read(pcap_t *p, int cnt, pcap_handler callback, u_char *user)
{
	register int cc, n;
	register u_char *bp, *ep, *pk;
	register struct bpf_insn *fcode;
	register struct sb_hdr *sbp;
	int flags;
	struct strbuf data;
	struct pcap_pkthdr pkthdr;

	flags = 0;
	cc = p->cc;
	if (cc == 0) {
		data.buf = (char *)p->buffer;
		data.maxlen = MAXDLBUF;
		data.len = 0;
		do {
			if (getmsg(p->fd, NULL, &data, &flags) < 0) {
				/* Don't choke when we get ptraced */
				if (errno == EINTR) {
					cc = 0;
					continue;
				}
				strcpy(p->errbuf, pcap_strerror(errno));
				return (-1);
			}
			cc = data.len;
		} while (cc == 0);
		bp = p->buffer;
	} else
		bp = p->bp;

	/* Loop through packets */
	fcode = p->fcode.bf_insns;
	ep = bp + cc;
	n = 0;
	while (bp < ep) {
		sbp = (struct sb_hdr *)bp;
		p->md.stat.ps_drop += sbp->sbh_drops;
		++p->md.stat.ps_recv;
		pk = bp + sizeof(*sbp);
		bp += sbp->sbh_totlen;
		if (bpf_filter(fcode, pk, sbp->sbh_origlen, sbp->sbh_msglen)) {
			pkthdr.ts = sbp->sbh_timestamp;
			pkthdr.len = sbp->sbh_origlen;
			pkthdr.caplen = sbp->sbh_msglen;
			(*callback)(user, &pkthdr, pk);
			if (++n >= cnt && cnt >= 0) {
				p->cc = ep - bp;
				p->bp = bp;
				return (n);
			}
		}
	}
	p->cc = 0;
	return (n);
}

pcap_t *
pcap_open_live(char *device, int snaplen, int promisc, int to_ms, char *ebuf)
{
	register pcap_t *p;
	long	buf[MAXDLBUF];
	int ppa;
	int cppa;
	register dl_info_ack_t *infop;
	u_long ss, flag;
#ifdef SOLARIS
	char *release;
	long osmajor, osminor, osmicro;
#endif
	char dname[100];

	p = (pcap_t *)malloc(sizeof(*p));
	if (p == NULL) {
		strcpy(ebuf, pcap_strerror(errno));
		return (NULL);
	}
	memset(p, 0, sizeof(*p));

	/*
	** 1) In order to get the ppa take the last character of the device
	** name if it is a number then fail the open.
	**
	** 2) If the name starts with a '/' then this is an absolute pathname,
	** otherwise prepend '/dev/'.
	**
	** 3) Remove the trailing digit and try and open the device
	** not staggeringly intuitive but it should work.
	**
	** If there are more than 9 devices this code will fail.
	*/

	cppa = device[strlen(device) - 1];
	if (!isdigit(cppa)) {
		sprintf(ebuf, "%c is not a digit, therefore not a valid ppa",
		    cppa);
		goto bad;
	}

	dname[0] = '\0';
	if (device[0] != '/')
		strcpy(dname, "/dev/");

	strcat(dname, device);
	dname[strlen(dname) - 1] = '\0';

	if ((p->fd = open(dname, O_RDWR)) < 0) {
		sprintf(ebuf, "%s: %s", dname, pcap_strerror(errno));
		goto bad;
	}
	p->snapshot = snaplen;

	ppa = cppa - '0';
	/*
	** Attach.
	*/
	if (dlattachreq(p->fd, ppa, ebuf) < 0 ||
	    dlokack(p->fd, (char *)buf, ebuf) < 0)
		goto bad;

	if (promisc) {
		/*
		** enable promiscuous.
		*/
		if (dlpromisconreq(p->fd, DL_PROMISC_PHYS, ebuf) < 0 ||
		    dlokack(p->fd, (char *)buf, ebuf) < 0)
			goto bad;
		if (dlpromisconreq(p->fd, DL_PROMISC_SAP, ebuf) < 0 ||
		    dlokack(p->fd, (char *)buf, ebuf) < 0)
			goto bad;

		/*
		** enable multicast, you would have thought promiscuous
		** would be sufficient.
		*/
		if (dlpromisconreq(p->fd, DL_PROMISC_MULTI, ebuf) < 0 ||
		    dlokack(p->fd, (char *)buf, ebuf) < 0)
			goto bad;
	}

	/*
	** Determine link type
	*/
	if (dlinforeq(p->fd, ebuf) < 0 ||
	    dlinfoack(p->fd, (char *)buf, ebuf) < 0)
		goto bad;

	infop = &((union DL_primitives *)buf)->info_ack;
	switch (infop->dl_mac_type) {

	case DL_ETHER:
		p->linktype = DLT_EN10MB;
		break;

	case DL_FDDI:
		p->linktype = DLT_FDDI;
		break;

	default:
		sprintf(ebuf, "unknown mac type 0x%lu", infop->dl_mac_type);
		goto bad;
	}

#ifdef	DLIOCRAW
	/*
	** This is a non standard SunOS hack to get the ethernet header.
	*/
	if (strioctl(p->fd, DLIOCRAW, 0, NULL) < 0) {
		sprintf(ebuf, "DLIOCRAW: %s", pcap_strerror(errno));
		goto bad;
	}
#endif

	/*
	** Another non standard call to get the data nicely buffered
	*/
	if (ioctl(p->fd, I_PUSH, "bufmod") != 0) {
		sprintf(ebuf, "I_PUSH bufmod: %s", pcap_strerror(errno));
		goto bad;
	}

	/*
	** Now that the bufmod is pushed lets configure it.
	**
	** There is a bug in bufmod(7). When dealing with messages of
	** less than snaplen size it strips data from the beginning not
	** the end.
	**
	** This bug is supposed to be fixed in 5.3.2. Also, there is a
	** patch available. Ask for bugid 1149065.
	*/
	ss = snaplen;
#ifdef SOLARIS
	release = getrelease(&osmajor, &osminor, &osmicro);
	if (osmajor == 5 && (osminor <= 2 || (osminor == 3 && osmicro < 2)) &&
	    getenv("BUFMOD_FIXED") == NULL) {
		fprintf(stderr,
		"WARNING: bufmod is broken in SunOS %s; ignoring snaplen.\n",
		    release);
		ss = 0;
	}
#endif
	if (ss > 0 &&
	    strioctl(p->fd, SBIOCSSNAP, sizeof(u_long), (char *)&ss) != 0) {
		sprintf(ebuf, "SBIOCSSNAP: %s", pcap_strerror(errno));
		goto bad;
	}

	/*
	** Set up the bufmod flags
	*/
	if (strioctl(p->fd, SBIOCGFLAGS, sizeof(u_long), (char *)&flag) < 0) {
		sprintf(ebuf, "SBIOCGFLAGS: %s", pcap_strerror(errno));
		goto bad;
	}
	flag |= SB_NO_DROPS;
	if (strioctl(p->fd, SBIOCSFLAGS, sizeof(u_long), (char *)&flag) != 0) {
		sprintf(ebuf, "SBIOCSFLAGS: %s", pcap_strerror(errno));
		goto bad;
	}
	/*
	** Set up the bufmod timeout
	*/
	if (to_ms != 0) {
		struct timeval to;

		to.tv_sec = to_ms / 1000;
		to.tv_usec = (to_ms * 1000) % 1000000;
		if (strioctl(p->fd, SBIOCSTIME, sizeof(to), (char *)&to) != 0) {
			sprintf(ebuf, "SBIOCSTIME: %s", pcap_strerror(errno));
			goto bad;
		}
	}

	/*
	** As the last operation flush the read side.
	*/
	if (ioctl(p->fd, I_FLUSH, FLUSHR) != 0) {
		sprintf(ebuf, "FLUSHR: %s", pcap_strerror(errno));
		goto bad;
	}
	/* Allocate data buffer */
	p->bufsize = MAXDLBUF * sizeof(long);
	p->buffer = (u_char *)malloc(p->bufsize);
	return (p);
bad:
	free(p);
	return (NULL);
}

int
pcap_setfilter(pcap_t *p, struct bpf_program *fp)
{

	p->fcode = *fp;
	return (0);
}

static int
send_request(int fd, char *ptr, int len, char *what, char *ebuf)
{
	struct	strbuf	ctl;
	int	flags;

	ctl.maxlen = 0;
	ctl.len = len;
	ctl.buf = ptr;

	flags = 0;
	if (putmsg(fd, &ctl, (struct strbuf *) NULL, flags) < 0) {
		sprintf(ebuf, "putmsg \"%s\"failed: %s",
		    what, pcap_strerror(errno));
		return (-1);
	}
	return (0);
}

static int
dlattachreq(int fd, u_long ppa, char *ebuf)
{
	dl_attach_req_t	req;

	req.dl_primitive = DL_ATTACH_REQ;
	req.dl_ppa = ppa;

	return (send_request(fd, (char *)&req, sizeof(req), "attach", ebuf));
}

static int
dlpromisconreq(int fd, u_long level, char *ebuf)
{
	dl_promiscon_req_t req;

	req.dl_primitive = DL_PROMISCON_REQ;
	req.dl_level = level;

	return (send_request(fd, (char *)&req, sizeof(req), "promiscon", ebuf));
}

static int
dlokack(int fd, char *bufp, char *ebuf)
{
	union	DL_primitives	*dlp;
	struct	strbuf	ctl;
	int	flags;

	ctl.maxlen = MAXDLBUF;
	ctl.len = 0;
	ctl.buf = bufp;

	flags = 0;
	if (getmsg(fd, &ctl, (struct strbuf*)NULL, &flags) < 0) {
		sprintf(ebuf, "getmsg: %s", pcap_strerror(errno));
		return (-1);
	}

	dlp = (union DL_primitives *) ctl.buf;

	if (dlp->dl_primitive != DL_OK_ACK) {
		sprintf(ebuf, "dlokack unexpected primitive %d",
			dlp->dl_primitive);
		return (-1);
	}

	if (ctl.len != sizeof(dl_ok_ack_t)) {
		sprintf(ebuf, "dlokack incorrect size returned");
		return (-1);
	}
	return (0);
}


static int
dlinforeq(int fd, char *ebuf)
{
	dl_info_req_t req;

	req.dl_primitive = DL_INFO_REQ;

	return (send_request(fd, (char *)&req, sizeof(req), "info", ebuf));
}

static int
dlinfoack(int fd, char *bufp, char *ebuf)
{
	union	DL_primitives	*dlp;
	struct	strbuf	ctl;
	int	flags;

	ctl.maxlen = MAXDLBUF;
	ctl.len = 0;
	ctl.buf = bufp;

	flags = 0;
	if (getmsg(fd, &ctl, (struct strbuf *)NULL, &flags) < 0) {
		sprintf(ebuf, "dlinfoack: getmsg: %s", pcap_strerror(errno));
		return (-1);
	}

	dlp = (union DL_primitives *) ctl.buf;

	if (dlp->dl_primitive != DL_INFO_ACK) {
		sprintf(ebuf, "dlinfoack: unexpected primitive %ld",
			dlp->dl_primitive);
		return (-1);
	}

	/* Extra stuff like the broadcast address can be returned */
	if (ctl.len < DL_INFO_ACK_SIZE) {
		sprintf(ebuf, "dlinfoack: incorrect size returned");
		return (-1);
	}
	return (0);
}

static int
strioctl(int fd, int cmd, int len, char *dp)
{
	struct strioctl str;
	int rc;

	str.ic_cmd = cmd;
	str.ic_timout = -1;
	str.ic_len = len;
	str.ic_dp = dp;
	rc = ioctl(fd, I_STR, &str);

	if (rc < 0)
		return (rc);
	else
		return (str.ic_len);
}

#ifdef SOLARIS
static char *
getrelease(long *majorp, long *minorp, long *microp)
{
	char *cp;
	static char buf[32];

	*majorp = 0;
	*minorp = 0;
	*microp = 0;
	if (sysinfo(SI_RELEASE, buf, sizeof(buf)) < 0)
		return ("?");
	cp = buf;
	if (!isdigit(*cp))
		return (buf);
	*majorp = strtol(cp, &cp, 10);
	if (*cp++ != '.')
		return (buf);
	*minorp =  strtol(cp, &cp, 10);
	if (*cp++ != '.')
		return (buf);
	*microp =  strtol(cp, &cp, 10);
	return (buf);
}
#endif
