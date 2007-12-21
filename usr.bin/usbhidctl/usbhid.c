/*	$NetBSD: usbhid.c,v 1.14 2000/07/03 02:51:37 matt Exp $	*/
/*	$FreeBSD$ */

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (augustss@netbsd.org).
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <err.h>
#include <ctype.h>
#include <errno.h>
#include <usbhid.h>
#include <dev/usb/usb.h>
#include <dev/usb/usbhid.h>

int verbose = 0;
int all = 0;
int noname = 0;
int hexdump = 0;
static int reportid;

char **names;
int nnames;

void prbits(int bits, char **strs, int n);
void usage(void);
void dumpitem(const char *label, struct hid_item *h);
void dumpitems(report_desc_t r);
void rev(struct hid_item **p);
void prdata(u_char *buf, struct hid_item *h);
void dumpdata(int f, report_desc_t r, int loop);
int gotname(char *n);

int
gotname(char *n)
{
	int i;

	for (i = 0; i < nnames; i++)
		if (strcmp(names[i], n) == 0)
			return 1;
	return 0;
}

void
prbits(int bits, char **strs, int n)
{
	int i;

	for(i = 0; i < n; i++, bits >>= 1)
		if (strs[i*2])
			printf("%s%s", i == 0 ? "" : ", ", strs[i*2 + (bits&1)]);
}

void
usage(void)
{
	extern char *__progname;

	fprintf(stderr,
                "usage: %s -f device "
                "[-l] [-n] [-r] [-t tablefile] [-v] [-x] name ...\n",
                __progname);
	fprintf(stderr,
                "       %s -f device "
                "[-l] [-n] [-r] [-t tablefile] [-v] [-x] -a\n",
                __progname);
	exit(1);
}

void
dumpitem(const char *label, struct hid_item *h)
{
	if ((h->flags & HIO_CONST) && !verbose)
		return;
	printf("%s size=%d count=%d page=%s usage=%s%s", label,
	       h->report_size, h->report_count,
	       hid_usage_page(HID_PAGE(h->usage)),
	       hid_usage_in_page(h->usage),
	       h->flags & HIO_CONST ? " Const" : "");
	printf(", logical range %d..%d",
	       h->logical_minimum, h->logical_maximum);
	if (h->physical_minimum != h->physical_maximum)
		printf(", physical range %d..%d",
		       h->physical_minimum, h->physical_maximum);
	if (h->unit)
		printf(", unit=0x%02x exp=%d", h->unit, h->unit_exponent);
	printf("\n");
}

void
dumpitems(report_desc_t r)
{
	struct hid_data *d;
	struct hid_item h;
	int size;

	for (d = hid_start_parse(r, ~0, reportid); hid_get_item(d, &h); ) {
		switch (h.kind) {
		case hid_collection:
			printf("Collection page=%s usage=%s\n",
			       hid_usage_page(HID_PAGE(h.usage)),
			       hid_usage_in_page(h.usage));
			break;
		case hid_endcollection:
			printf("End collection\n");
			break;
		case hid_input:
			dumpitem("Input  ", &h);
			break;
		case hid_output:
			dumpitem("Output ", &h);
			break;
		case hid_feature:
			dumpitem("Feature", &h);
			break;
		}
	}
	hid_end_parse(d);
	size = hid_report_size(r, hid_input, 0);
	printf("Total   input size %d bytes\n", size);

	size = hid_report_size(r, hid_output, 0);
	printf("Total  output size %d bytes\n", size);

	size = hid_report_size(r, hid_feature, 0);
	printf("Total feature size %d bytes\n", size);
}

void
rev(struct hid_item **p)
{
	struct hid_item *cur, *prev, *next;

	prev = 0;
	cur = *p;
	while(cur != 0) {
		next = cur->next;
		cur->next = prev;
		prev = cur;
		cur = next;
	}
	*p = prev;
}

void
prdata(u_char *buf, struct hid_item *h)
{
	u_int data;
	int i, pos;

	pos = h->pos;
	for (i = 0; i < h->report_count; i++) {
		data = hid_get_data(buf, h);
		if (h->logical_minimum < 0)
			printf("%d", (int)data);
		else
			printf("%u", data);
                if (hexdump)
			printf(" [0x%x]", data);
		pos += h->report_size;
	}
}

void
dumpdata(int f, report_desc_t rd, int loop)
{
	struct hid_data *d;
	struct hid_item h, *hids, *n;
	int r, dlen;
	u_char *dbuf;
	static int one = 1;
	u_int32_t colls[100];
	int sp = 0;
	char namebuf[10000], *namep;

	hids = 0;
	for (d = hid_start_parse(rd, 1<<hid_input, reportid);
	     hid_get_item(d, &h); ) {
		if (h.kind == hid_collection)
			colls[++sp] = h.usage;
		else if (h.kind == hid_endcollection)
			--sp;
		if (h.kind != hid_input || (h.flags & HIO_CONST))
			continue;
		h.next = hids;
		h.collection = colls[sp];
		hids = malloc(sizeof *hids);
		*hids = h;
	}
	hid_end_parse(d);
	rev(&hids);
	dlen = hid_report_size(rd, hid_input, 0);
	dbuf = malloc(dlen);
	if (!loop)
		if (ioctl(f, USB_SET_IMMED, &one) < 0) {
			if (errno == EOPNOTSUPP)
				warnx("device does not support immediate mode, only changes reported.");
			else
				err(1, "USB_SET_IMMED");
		}
	do {
		r = read(f, dbuf, dlen);
		if (r != dlen) {
			err(1, "bad read %d != %d", r, dlen);
		}
		for (n = hids; n; n = n->next) {
			namep = namebuf;
			namep += sprintf(namep, "%s:%s.",
					 hid_usage_page(HID_PAGE(n->collection)),
					 hid_usage_in_page(n->collection));
			namep += sprintf(namep, "%s:%s",
					 hid_usage_page(HID_PAGE(n->usage)),
					 hid_usage_in_page(n->usage));
			if (all || gotname(namebuf)) {
				if (!noname)
					printf("%s=", namebuf);
				prdata(dbuf + (reportid != 0), n);
				printf("\n");
			}
		}
		if (loop)
			printf("\n");
	} while (loop);
	free(dbuf);
}

int
main(int argc, char **argv)
{
	int f;
	report_desc_t r;
	char devnam[100], *dev = 0;
	int ch;
	int repdump = 0;
	int loop = 0;
	char *table = 0;

	while ((ch = getopt(argc, argv, "af:lnrt:vx")) != -1) {
		switch(ch) {
		case 'a':
			all++;
			break;
		case 'f':
			dev = optarg;
			break;
		case 'l':
			loop ^= 1;
			break;
		case 'n':
			noname++;
			break;
		case 'r':
			repdump++;
			break;
		case 't':
			table = optarg;
			break;
		case 'v':
			verbose++;
			break;
		case 'x':
			hexdump = 1;
			break;
		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	if (dev == 0)
		usage();
	names = argv;
	nnames = argc;

	if (nnames == 0 && !all && !repdump)
		usage();

	if (dev[0] != '/') {
		if (isdigit(dev[0]))
			snprintf(devnam, sizeof(devnam), "/dev/uhid%s", dev);
		else
			snprintf(devnam, sizeof(devnam), "/dev/%s", dev);
		dev = devnam;
	}

	hid_init(table);

	f = open(dev, O_RDWR);
	if (f < 0)
		err(1, "%s", dev);

	r = hid_get_report_desc(f);
	if (r == 0)
		errx(1, "USB_GET_REPORT_DESC");

	if (repdump) {
		printf("Report descriptor:\n");
		dumpitems(r);
	}
	if (nnames != 0 || all)
		dumpdata(f, r, loop);

	hid_dispose_report_desc(r);
	exit(0);
}
