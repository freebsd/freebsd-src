/*-
 * Copyright (c) 2005 Christian S.J. Peron
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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
#include <sys/param.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/stat.h>
#include <sys/vnode.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/filedesc.h>
#include <sys/queue.h>
#include <net/bpfdesc.h>

#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#include "bpfstat.h"

/* Function prototypes */
static void	 bpfd_print_header(struct conf *);
static void	 bpfd_parse_flags(struct xbpf_d *, char *);
static struct xbpf_d *
		 bpfd_get_descs(struct conf *);
static void	 bpfd_print_row(struct xbpf_d *, struct conf *);
static void	 usage(char *);
static void	 setwidths(struct colwidths *);
static int	 diglen(double);

static void
setwidths(struct colwidths *c)
{
	c->rc_width = 4;
	c->dc_width = 4;
	c->pd_width = 3;
	c->hb_width = 5;
	c->sb_width = 5;
	c->fc_width = 5;
}

static int
diglen(double n)
{
	int width;

	width = (int)ceil(log10(n));
	return (width);
}

static void
calcwidths(struct conf *conf, struct xbpf_d *bd)
{
	struct colwidths *c;
	int width;

	c = &conf->cw;
	width = diglen((double)bd->bd_rcount);
	if (width > c->rc_width)
		c->rc_width = width;
	width = diglen((double)bd->bd_dcount);
	if (width > c->dc_width)
		c->dc_width = width;
	width = diglen((double)bd->bd_pid);
	if (width > c->pd_width)
		c->pd_width = width;
	width = diglen((double)bd->bd_slen);
	if (width > c->sb_width)
		c->sb_width = width;
	width = diglen((double)bd->bd_hlen);
	if (width > c->hb_width)
		c->hb_width = width;
	width = diglen((double)bd->bd_fcount);
	if (width > c->fc_width)
		c->fc_width = width;
}

static void
bpfd_print_header(struct conf *conf)
{
	struct colwidths *c;

	if (conf->qflag || conf->lps > 0)
		return;
	c = &conf->cw;
	printf("%*s %*s %*s %*s %*s %*s %*s %*s %s\n",
	    c->pd_width,	"pid",
	    7,			"netif",
	    7,			"flags",
	    c->rc_width,	"recv",
	    c->dc_width,	"drop",
	    c->fc_width,	"match",
	    c->sb_width,	"sblen",
	    c->hb_width,	"hblen",
	    "command");
}

static void
bpfd_print_row(struct xbpf_d *bd, struct conf *conf)
{
	struct colwidths *c;
	char flagbuf[8];

	c = &conf->cw;
	bpfd_parse_flags(bd, &flagbuf[0]);
	printf("%*d %*s %*s %*lu %*lu %*lu %*d %*d %s\n",
	    c->pd_width,	bd->bd_pid,
	    7,			bd->bd_ifname,
	    7,			flagbuf,
	    c->rc_width,	bd->bd_rcount,
	    c->dc_width,	bd->bd_dcount,
	    c->fc_width,	bd->bd_fcount,
	    c->sb_width,	bd->bd_slen,
	    c->hb_width,	bd->bd_hlen,
	    bd->bd_pcomm);
}

static void
bpfd_parse_flags(struct xbpf_d *bd, char *flagbuf)
{

	*flagbuf++ = bd->bd_promisc ? 'p' : '-';
	*flagbuf++ = bd->bd_immediate ? 'i' : '-';
	*flagbuf++ = bd->bd_hdrcmplt ? '-' : 'f';
	*flagbuf++ = bd->bd_seesent ? 's' : '.';
	*flagbuf++ = bd->bd_async ? 'a' : '-';
	*flagbuf++ = bd->bd_locked ? 'l' : '-';
	*flagbuf++ = '\0';
}

static struct xbpf_d *
bpfd_get_descs(struct conf *conf)
{
	size_t bpf_alloc;
	int error;

	error = sysctlbyname(BPFSOID, NULL, &bpf_alloc,
	    NULL, 0);
	if (error < 0)
		err(1, "get buffer size");
	if (bpf_alloc == 0)
		return (NULL);
	conf->bpf_vec = malloc(bpf_alloc);
	if (conf->bpf_vec == NULL)
		return (NULL);
	conf->nbdevs = bpf_alloc / sizeof(struct xbpf_d);
	bzero(conf->bpf_vec, bpf_alloc);
	error = sysctlbyname(BPFSOID, conf->bpf_vec,
	    &bpf_alloc, NULL, 0);
	if (error < 0)
		err(1, "get descriptors failed");
	if (bpf_alloc == 0)
		return (NULL);
	return (conf->bpf_vec);
}

int
main(int argc, char *argv [])
{
	struct xbpf_d *bd, *bpfds;
	int ch;
	char *rem;
	struct conf cf;

	memset(&cf, 0, sizeof(cf));
	setwidths(&cf.cw);
	while ((ch = getopt(argc, argv, "c:i:I:p:q")) != -1)
		switch (ch) {
		case 'c':
			cf.cflag = optarg;
			break;
		case 'i':
			cf.iflag = strtol(optarg, &rem, 0);
			break;
		case 'I':
			cf.Iflag = optarg;
			break;
		case 'p':
			cf.pflag = strtol(optarg, &rem, 0);
			break;
		case 'q':
			cf.qflag++;
			break;
		default:
			usage(argv[0]);
		}
	do {
		bpfds = bpfd_get_descs(&cf);
		if (bpfds == NULL)
			break;
		for (bd = &bpfds[0]; bd < &bpfds[cf.nbdevs]; bd++)
			calcwidths(&cf, bd);
		bpfd_print_header(&cf);
		for (bd = &bpfds[0]; bd < &bpfds[cf.nbdevs]; bd++) {
			if (cf.pflag && cf.pflag != bd->bd_pid)
				continue;
			if (cf.Iflag &&
			    strcmp(bd->bd_ifname, cf.Iflag) != 0)
				continue;
			if (cf.cflag &&
			    strcmp(bd->bd_pcomm, cf.cflag) != 0)
				continue;
			bpfd_print_row(bd, &cf);
		}
		cf.lps++;
		free(bpfds);
		(void)sleep(cf.iflag);
	} while (cf.iflag > 0);
	return (0);
}

static void
usage(char *prog)
{

	fprintf(stderr,
    "usage: %s [-q] [-c procname] [-i wait] [-I interface] [-p pid]\n",
	    prog);
	exit(1);
}
