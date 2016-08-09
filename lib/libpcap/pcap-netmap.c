/*
 * Copyright (C) 2014 Luigi Rizzo. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS''AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <poll.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define NETMAP_WITH_LIBS
#include <net/netmap_user.h>

#include "pcap-int.h"

/*
 * $FreeBSD$
 *
 * This code is meant to build also on other versions of libpcap.
 *
 * older libpcap miss p->priv, use p->md.device instead (and allocate).
 * Also opt.timeout was in md.timeout before.
 * Use #define PCAP_IF_UP to discriminate
 */
#ifdef PCAP_IF_UP
#define NM_PRIV(p)	((struct pcap_netmap *)(p->priv))
#define the_timeout	opt.timeout
#else
#define HAVE_NO_PRIV
#define	NM_PRIV(p)	((struct pcap_netmap *)(p->md.device))
#define SET_PRIV(p, x)	p->md.device = (void *)x
#define the_timeout	md.timeout
#endif

#if defined (linux)
/* On FreeBSD we use IFF_PPROMISC which is in ifr_flagshigh.
 * remap to IFF_PROMISC on linux
 */
#define IFF_PPROMISC	IFF_PROMISC
#endif /* linux */

struct pcap_netmap {
	struct nm_desc *d;	/* pointer returned by nm_open() */
	pcap_handler cb;	/* callback and argument */
	u_char *cb_arg;
	int must_clear_promisc;	/* flag */
	uint64_t rx_pkts;	/* # of pkts received before the filter */
};


static int
pcap_netmap_stats(pcap_t *p, struct pcap_stat *ps)
{
	struct pcap_netmap *pn = NM_PRIV(p);

	ps->ps_recv = pn->rx_pkts;
	ps->ps_drop = 0;
	ps->ps_ifdrop = 0;
	return 0;
}


static void
pcap_netmap_filter(u_char *arg, struct pcap_pkthdr *h, const u_char *buf)
{
	pcap_t *p = (pcap_t *)arg;
	struct pcap_netmap *pn = NM_PRIV(p);
	const struct bpf_insn *pc = p->fcode.bf_insns;

	++pn->rx_pkts;
	if (pc == NULL || bpf_filter(pc, buf, h->len, h->caplen))
		pn->cb(pn->cb_arg, h, buf);
}


static int
pcap_netmap_dispatch(pcap_t *p, int cnt, pcap_handler cb, u_char *user)
{
	int ret;
	struct pcap_netmap *pn = NM_PRIV(p);
	struct nm_desc *d = pn->d;
	struct pollfd pfd = { .fd = p->fd, .events = POLLIN, .revents = 0 };

	pn->cb = cb;
	pn->cb_arg = user;

	for (;;) {
		if (p->break_loop) {
			p->break_loop = 0;
			return PCAP_ERROR_BREAK;
		}
		/* nm_dispatch won't run forever */

		ret = nm_dispatch((void *)d, cnt, (void *)pcap_netmap_filter, (void *)p);
		if (ret != 0)
			break;
		errno = 0;
		ret = poll(&pfd, 1, p->the_timeout);
	}
	return ret;
}


/* XXX need to check the NIOCTXSYNC/poll */
static int
pcap_netmap_inject(pcap_t *p, const void *buf, size_t size)
{
	struct nm_desc *d = NM_PRIV(p)->d;

	return nm_inject(d, buf, size);
}


static int
pcap_netmap_ioctl(pcap_t *p, u_long what, uint32_t *if_flags)
{
	struct pcap_netmap *pn = NM_PRIV(p);
	struct nm_desc *d = pn->d;
	struct ifreq ifr;
	int error, fd = d->fd;

#ifdef linux
	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) {
		fprintf(stderr, "Error: cannot get device control socket.\n");
		return -1;
	}
#endif /* linux */
	bzero(&ifr, sizeof(ifr));
	strncpy(ifr.ifr_name, d->req.nr_name, sizeof(ifr.ifr_name));
	switch (what) {
	case SIOCSIFFLAGS:
		ifr.ifr_flags = *if_flags;
#ifdef __FreeBSD__
		ifr.ifr_flagshigh = *if_flags >> 16;
#endif /* __FreeBSD__ */
		break;
	}
	error = ioctl(fd, what, &ifr);
	if (!error) {
		switch (what) {
		case SIOCGIFFLAGS:
			*if_flags = ifr.ifr_flags;
#ifdef __FreeBSD__
			*if_flags |= (ifr.ifr_flagshigh << 16);
#endif /* __FreeBSD__ */
		}
	}
#ifdef linux
	close(fd);
#endif /* linux */
	return error ? -1 : 0;
}


static void
pcap_netmap_close(pcap_t *p)
{
	struct pcap_netmap *pn = NM_PRIV(p);
	struct nm_desc *d = pn->d;
	uint32_t if_flags = 0;

	if (pn->must_clear_promisc) {
		pcap_netmap_ioctl(p, SIOCGIFFLAGS, &if_flags); /* fetch flags */
		if (if_flags & IFF_PPROMISC) {
			if_flags &= ~IFF_PPROMISC;
			pcap_netmap_ioctl(p, SIOCSIFFLAGS, &if_flags);
		}
	}
	nm_close(d);
#ifdef HAVE_NO_PRIV
	free(pn);
	SET_PRIV(p, NULL); // unnecessary
#endif
	pcap_cleanup_live_common(p);
}


static int
pcap_netmap_activate(pcap_t *p)
{
	struct pcap_netmap *pn = NM_PRIV(p);
	struct nm_desc *d = nm_open(p->opt.source, NULL, 0, NULL);
	uint32_t if_flags = 0;

	if (d == NULL) {
		snprintf(p->errbuf, PCAP_ERRBUF_SIZE,
			"netmap open: cannot access %s: %s\n",
			p->opt.source, pcap_strerror(errno));
#ifdef HAVE_NO_PRIV
		free(pn);
		SET_PRIV(p, NULL); // unnecessary
#endif
		pcap_cleanup_live_common(p);
		return (PCAP_ERROR);
	}
	if (0)
	    fprintf(stderr, "%s device %s priv %p fd %d ports %d..%d\n",
		__FUNCTION__, p->opt.source, d, d->fd,
		d->first_rx_ring, d->last_rx_ring);
	pn->d = d;
	p->fd = d->fd;
	if (p->opt.promisc && !(d->req.nr_ringid & NETMAP_SW_RING)) {
		pcap_netmap_ioctl(p, SIOCGIFFLAGS, &if_flags); /* fetch flags */
		if (!(if_flags & IFF_PPROMISC)) {
			pn->must_clear_promisc = 1;
			if_flags |= IFF_PPROMISC;
			pcap_netmap_ioctl(p, SIOCSIFFLAGS, &if_flags);
		}
	}
	p->linktype = DLT_EN10MB;
	p->selectable_fd = p->fd;
	p->read_op = pcap_netmap_dispatch;
	p->inject_op = pcap_netmap_inject;
	p->setfilter_op = install_bpf_program;
	p->setdirection_op = NULL;
	p->set_datalink_op = NULL;
	p->getnonblock_op = pcap_getnonblock_fd;
	p->setnonblock_op = pcap_setnonblock_fd;
	p->stats_op = pcap_netmap_stats;
	p->cleanup_op = pcap_netmap_close;

	return (0);
}


pcap_t *
pcap_netmap_create(const char *device, char *ebuf, int *is_ours)
{
	pcap_t *p;

	*is_ours = (!strncmp(device, "netmap:", 7) || !strncmp(device, "vale", 4));
	if (! *is_ours)
		return NULL;
#ifdef HAVE_NO_PRIV
	{
		void *pn = calloc(1, sizeof(struct pcap_netmap));
		if (pn == NULL)
			return NULL;
		p = pcap_create_common(device, ebuf);
		if (p == NULL) {
			free(pn);
			return NULL;
		}
		SET_PRIV(p, pn);
	}
#else
	p = pcap_create_common(device, ebuf, sizeof (struct pcap_netmap));
	if (p == NULL)
		return (NULL);
#endif
	p->activate_op = pcap_netmap_activate;
	return (p);
}
