/*
 * Copyright (c) 1993, 1994, 1995, 1996, 1997, 1998
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the Computer Systems
 *	Engineering Group at Lawrence Berkeley Laboratory.
 * 4. Neither the name of the University nor of the Laboratory may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static const char rcsid[] =
    "@(#) $Header: /tcpdump/master/libpcap/pcap.c,v 1.38 2001/12/29 21:55:32 guy Exp $ (LBL)";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#ifdef HAVE_OS_PROTO_H
#include "os-proto.h"
#endif

#include "pcap-int.h"

int
pcap_dispatch(pcap_t *p, int cnt, pcap_handler callback, u_char *user)
{

	if (p->sf.rfile != NULL)
		return (pcap_offline_read(p, cnt, callback, user));
	return (pcap_read(p, cnt, callback, user));
}

int
pcap_loop(pcap_t *p, int cnt, pcap_handler callback, u_char *user)
{
	register int n;

	for (;;) {
		if (p->sf.rfile != NULL)
			n = pcap_offline_read(p, cnt, callback, user);
		else {
			/*
			 * XXX keep reading until we get something
			 * (or an error occurs)
			 */
			do {
				n = pcap_read(p, cnt, callback, user);
			} while (n == 0);
		}
		if (n <= 0)
			return (n);
		if (cnt > 0) {
			cnt -= n;
			if (cnt <= 0)
				return (0);
		}
	}
}

struct singleton {
	struct pcap_pkthdr *hdr;
	const u_char *pkt;
};


static void
pcap_oneshot(u_char *userData, const struct pcap_pkthdr *h, const u_char *pkt)
{
	struct singleton *sp = (struct singleton *)userData;
	*sp->hdr = *h;
	sp->pkt = pkt;
}

const u_char *
pcap_next(pcap_t *p, struct pcap_pkthdr *h)
{
	struct singleton s;

	s.hdr = h;
	if (pcap_dispatch(p, 1, pcap_oneshot, (u_char*)&s) <= 0)
		return (0);
	return (s.pkt);
}

int
pcap_datalink(pcap_t *p)
{
	return (p->linktype);
}

int
pcap_list_datalinks(pcap_t *p, int **dlt_buffer)
{
	if (p->dlt_count <= 0) {
		*dlt_buffer = NULL;
		return -1;
	}
	*dlt_buffer = (int*)malloc(sizeof(**dlt_buffer) * p->dlt_count);
	if (*dlt_buffer == NULL) {
		(void)snprintf(p->errbuf, sizeof(p->errbuf), "malloc: %s",
		    pcap_strerror(errno));
		return -1;
	}
	(void)memcpy(*dlt_buffer, p->dlt_list,
	    sizeof(**dlt_buffer) * p->dlt_count);
	return (p->dlt_count);
}

int
pcap_snapshot(pcap_t *p)
{
	return (p->snapshot);
}

int
pcap_is_swapped(pcap_t *p)
{
	return (p->sf.swapped);
}

int
pcap_major_version(pcap_t *p)
{
	return (p->sf.version_major);
}

int
pcap_minor_version(pcap_t *p)
{
	return (p->sf.version_minor);
}

FILE *
pcap_file(pcap_t *p)
{
	return (p->sf.rfile);
}

int
pcap_fileno(pcap_t *p)
{
	return (p->fd);
}

void
pcap_perror(pcap_t *p, char *prefix)
{
	fprintf(stderr, "%s: %s\n", prefix, p->errbuf);
}

char *
pcap_geterr(pcap_t *p)
{
	return (p->errbuf);
}

/*
 * NOTE: in the future, these may need to call platform-dependent routines,
 * e.g. on platforms with memory-mapped packet-capture mechanisms where
 * "pcap_read()" uses "select()" or "poll()" to wait for packets to arrive.
 */
int
pcap_getnonblock(pcap_t *p, char *errbuf)
{
	int fdflags;

	if (p->sf.rfile != NULL) {
		/*
		 * This is a savefile, not a live capture file, so
		 * never say it's in non-blocking mode.
		 */
		return (0);
	}
	fdflags = fcntl(p->fd, F_GETFL, 0);
	if (fdflags == -1) {
		snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "F_GETFL: %s",
		    pcap_strerror(errno));
		return (-1);
	}
	if (fdflags & O_NONBLOCK)
		return (1);
	else
		return (0);
}

int
pcap_setnonblock(pcap_t *p, int nonblock, char *errbuf)
{
	int fdflags;

	if (p->sf.rfile != NULL) {
		/*
		 * This is a savefile, not a live capture file, so
		 * ignore requests to put it in non-blocking mode.
		 */
		return (0);
	}
	fdflags = fcntl(p->fd, F_GETFL, 0);
	if (fdflags == -1) {
		snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "F_GETFL: %s",
		    pcap_strerror(errno));
		return (-1);
	}
	if (nonblock)
		fdflags |= O_NONBLOCK;
	else
		fdflags &= ~O_NONBLOCK;
	if (fcntl(p->fd, F_SETFL, fdflags) == -1) {
		snprintf(p->errbuf, PCAP_ERRBUF_SIZE, "F_SETFL: %s",
		    pcap_strerror(errno));
		return (-1);
	}
	return (0);
}

/*
 * Not all systems have strerror().
 */
char *
pcap_strerror(int errnum)
{
#ifdef HAVE_STRERROR
	return (strerror(errnum));
#else
	extern int sys_nerr;
	extern const char *const sys_errlist[];
	static char ebuf[20];

	if ((unsigned int)errnum < sys_nerr)
		return ((char *)sys_errlist[errnum]);
	(void)snprintf(ebuf, sizeof ebuf, "Unknown error: %d", errnum);
	return(ebuf);
#endif
}

pcap_t *
pcap_open_dead(int linktype, int snaplen)
{
	pcap_t *p;

	p = malloc(sizeof(*p));
	if (p == NULL)
		return NULL;
	memset (p, 0, sizeof(*p));
	p->fd = -1;
	p->snapshot = snaplen;
	p->linktype = linktype;
	return p;
}

void
pcap_close(pcap_t *p)
{
	/*XXX*/
	if (p->fd >= 0) {
#ifdef linux
		pcap_close_linux(p);
#endif
		close(p->fd);
	}
	if (p->sf.rfile != NULL) {
		if (p->sf.rfile != stdin)
			(void)fclose(p->sf.rfile);
		if (p->sf.base != NULL)
			free(p->sf.base);
	} else if (p->buffer != NULL)
		free(p->buffer);
	if (p->dlt_list != NULL)
		free(p->dlt_list);
	
	pcap_freecode(&p->fcode);
	free(p);
}
