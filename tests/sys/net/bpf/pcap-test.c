/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Gleb Smirnoff <glebius@FreeBSD.org>
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
 */

#include <sys/param.h>
#include <sys/queue.h>
#include <netinet/ip.h>
#include <pcap/pcap.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <err.h>

static int
strtolerr(const char *s)
{
	int rv;

	if ((rv = (int)strtol(s, NULL, 10)) < 1)
		errx(1, "bad count %s", s);
	return (rv);
}

static pcap_direction_t
strtodir(const char *s)
{
	static const struct dirstr {
		const char *str;
		pcap_direction_t dir;
	} dirs[] = {
		{ "in", PCAP_D_IN },
		{ "out", PCAP_D_OUT },
		{ "both", PCAP_D_INOUT },
		{ "inout", PCAP_D_INOUT },
	};

	for (u_int i = 0; i < nitems(dirs); i++)
		if (strcasecmp(s, dirs[i].str) == 0)
			return (dirs[i].dir);
	errx(1, "bad directions %s", s);
}

static char errbuf[PCAP_ERRBUF_SIZE];

static pcap_t *
pcap_open(const char *name, pcap_direction_t dir)
{
	pcap_t *p;

	if ((p = pcap_create(name, errbuf)) == NULL)
		errx(1, "pcap_create: %s", errbuf);
	if (pcap_set_timeout(p, 10) != 0)
		errx(1, "pcap_set_timeout: %s", pcap_geterr(p));
	if (pcap_activate(p) != 0)
		errx(1, "pcap_activate: %s", errbuf);
	if (pcap_setdirection(p, dir) != 0)
		errx(1, "pcap_setdirection: %s", pcap_geterr(p));
	return (p);
}

#if 0
/*
 * Deal with the FreeBSD writer only optimization hack in bpf(4).
 * Needed only when net.bpf.optimize_writers=1.
 */
static pcap_t *
pcap_rwopen(const char *name, pcap_direction_t dir)
{
	pcap_t *p;
	struct bpf_program fp;

	p = pcap_open(name, dir);
	if (pcap_compile(p, &fp, "", 0, PCAP_NETMASK_UNKNOWN) != 0)
		errx(1, "pcap_compile: %s", pcap_geterr(p));
	if (pcap_setfilter(p, &fp) != 0)
		errx(1, "pcap_setfilter: %s", pcap_geterr(p));
	pcap_freecode(&fp);
	return (p);
}
#endif

static void
list(int argc __unused, char *argv[] __unused)
{
	pcap_if_t *all, *p;

	if (pcap_findalldevs(&all, errbuf) != 0)
		errx(1, "pcap_findalldevs: %s", errbuf);
	for (p = all; p != NULL; p = p->next)
		printf("%s ", p->name);
	printf("\n");
	pcap_freealldevs(all);
}

/* args: tap file count direction */
static void
capture(int argc __unused, char *argv[])
{
	pcap_t *p;
	pcap_dumper_t *d;
	pcap_direction_t dir;
	int cnt;

	cnt = strtolerr(argv[2]);
	dir = strtodir(argv[3]);
	p = pcap_open(argv[0], dir);

	if ((d = pcap_dump_open(p, argv[1])) == NULL)
		errx(1, "pcap_dump_open: %s", pcap_geterr(p));

	if (pcap_loop(p, cnt, pcap_dump, (u_char *)d) != 0)
		errx(1, "pcap_loop: %s", pcap_geterr(p));
	pcap_dump_close(d);
}

static void
inject_packet(u_char *user, const struct pcap_pkthdr *h, const u_char *bytes)
{
	pcap_t *p = (pcap_t *)user;

	if (h->caplen != h->len)
		errx(1, "incomplete packet %u of %u", h->caplen, h->len);

	if (pcap_inject(p, bytes, h->caplen) != (int)h->caplen)
		errx(1, "pcap_inject: %s", errbuf);
}

/* args: tap file count */
static void
inject(int argc __unused, char *argv[])
{
	pcap_t *p, *d;
	int cnt;

	cnt = strtolerr(argv[2]);
	p = pcap_open(argv[0], PCAP_D_INOUT);

	if ((d = pcap_open_offline(argv[1], errbuf)) == NULL)
		errx(1, "pcap_open_offline: %s", errbuf);
	if (pcap_loop(d, cnt, inject_packet, (u_char *)p) != 0)
		errx(1, "pcap_loop: %s", pcap_geterr(p));
	pcap_close(p);
	pcap_close(d);
}

struct packet {
	STAILQ_ENTRY(packet)	next;
	const void		*data;
	u_int			caplen;
	u_int			len;
};
STAILQ_HEAD(plist, packet);

static void
store_packet(u_char *user, const struct pcap_pkthdr *h, const u_char *bytes)
{
	struct plist *list = (struct plist *)user;
	struct packet *p;

	p = malloc(sizeof(*p));
	p->data = bytes;
	p->caplen = h->caplen;
	p->len = h->len;
	STAILQ_INSERT_TAIL(list, p, next);
}

/* args: file1 file2 */
static void
compare(int argc __unused, char *argv[])
{
	pcap_t *f1, *f2;
	struct plist
	    list1 = STAILQ_HEAD_INITIALIZER(list1),
	    list2 = STAILQ_HEAD_INITIALIZER(list2);
	struct packet *p1, *p2;
	u_int cnt;

        if ((f1 = pcap_open_offline(argv[0], errbuf)) == NULL)
                errx(1, "pcap_open_offline: %s", errbuf);
	if (pcap_loop(f1, 0, store_packet, (u_char *)&list1) != 0)
		errx(1, "pcap_loop: %s", pcap_geterr(f1));
        if ((f2 = pcap_open_offline(argv[1], errbuf)) == NULL)
                errx(1, "pcap_open_offline: %s", errbuf);
	if (pcap_loop(f2, 0, store_packet, (u_char *)&list2) != 0)
		errx(1, "pcap_loop: %s", pcap_geterr(f2));

	for (p1 = STAILQ_FIRST(&list1), p2 = STAILQ_FIRST(&list2), cnt = 1;
	    p1 != NULL && p2 != NULL;
	    p1 = STAILQ_NEXT(p1, next), p2 = STAILQ_NEXT(p2, next), cnt++) {
		if (p1->len != p2->len)
			errx(1, "packet #%u length %u != %u",
			    cnt, p1->len, p2->len);
		if (p1->caplen != p2->caplen)
			errx(1, "packet #%u capture length %u != %u",
			    cnt, p1->caplen, p2->caplen);
		if (memcmp(p1->data, p2->data, p1->caplen) != 0)
			errx(1, "packet #%u payload different", cnt);
	}
	if (p1 != NULL || p2 != NULL)
		errx(1, "packet count different");

	pcap_close(f1);
	pcap_close(f2);
}

static const struct cmd {
	const char *cmd;
	void (*func)(int, char **);
	u_int argc;
} cmds[] = {
	{ .cmd = "list",	.func = list,	.argc = 0 },
	{ .cmd = "inject",	.func = inject,	.argc = 3 },
	{ .cmd = "capture",	.func = capture,.argc = 4 },
	{ .cmd = "compare",	.func = compare,.argc = 2 },
};

int
main(int argc, char *argv[])
{

	if (argc < 2) {
		fprintf(stderr, "Usage: %s ", argv[0]);
		for (u_int i = 0; i < nitems(cmds); i++)
			fprintf(stderr, "%s%s", cmds[i].cmd,
			    i != nitems(cmds) - 1 ? "|" : "\n");
		exit(1);
	}

	for (u_int i = 0; i < nitems(cmds); i++)
		if (strcasecmp(argv[1], cmds[i].cmd) == 0) {
			argc -= 2;
			argv += 2;
			if (argc < (int)cmds[i].argc)
				errx(1, "%s takes %u args",
				    cmds[i].cmd, cmds[i].argc);
			cmds[i].func(argc, argv);
			return (0);
		}

	warnx("Unknown command %s\n", argv[1]);
	return (1);
}
