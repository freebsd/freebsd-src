/*	$NetBSD: usbdevs.c,v 1.4 1998/07/23 13:57:51 augustss Exp $	*/
/*	$FreeBSD$	*/

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
#include <unistd.h>
#include <err.h>
#include <errno.h>
#include <dev/usb/usb.h>
#if defined(__FreeBSD__)
#include <sys/ioctl.h>
#endif

#define USBDEV "/dev/usb"

int verbose;

void usage __P((void));
void usbdev __P((int f, int a, int rec));
void usbdump __P((int f));
void dumpone __P((char *name, int f, int addr));
int main __P((int, char **));

extern char *__progname;

void
usage()
{
	fprintf(stderr, "Usage: %s [-a addr] [-f dev] [-v]\n", __progname);
	exit(1);
}

char done[USB_MAX_DEVICES];
int indent;

void
usbdev(f, a, rec)
	int f;
	int a;
	int rec;
{
	struct usb_device_info di;
	int e, p;

	di.udi_addr = a;
	e = ioctl(f, USB_DEVICEINFO, &di);
	if (e)
		return;
	done[a] = 1;
	printf("addr %d: ", di.udi_addr);
	if (verbose) {
		if (di.udi_lowspeed)
			printf("low speed, ");
		if (di.udi_power)
			printf("power %d mA, ", di.udi_power);
		else
			printf("self powered, ");
		if (di.udi_config)
			printf("config %d, ", di.udi_config);
		else
			printf("unconfigured, ");
	}
	if (verbose) {
		printf("%s(0x%04x), %s(0x%04x), rev 0x%04x",
			di.udi_product, di.udi_productNo,
			di.udi_vendor, di.udi_vendorNo, di.udi_releaseNo);
	} else
		printf("%s, %s", di.udi_product, di.udi_vendor);
	printf("\n");
	if (!rec)
		return;
	for (p = 0; p < di.udi_nports; p++) {
		int s = di.udi_ports[p];
		if (s >= USB_MAX_DEVICES) {
			if (verbose) {
				printf("%*sport %d %s\n", indent+1, "", p+1,
				       s == USB_PORT_ENABLED ? "enabled" :
				       s == USB_PORT_SUSPENDED ? "suspended" : 
				       s == USB_PORT_POWERED ? "powered" :
				       s == USB_PORT_DISABLED ? "disabled" :
				       "???");
				
			}
			continue;
		}
		indent++;
		printf("%*s", indent, "");
		if (verbose)
			printf("port %d ", p+1);
		usbdev(f, di.udi_ports[p], 1);
		indent--;
	}
}

void
usbdump(f)
	int f;
{
	int a;

	for (a = 1; a < USB_MAX_DEVICES; a++) {
		if (!done[a])
			usbdev(f, a, 1);
	}
}

void
dumpone(name, f, addr)
	char *name;
	int f;
	int addr;
{
	if (verbose)
		printf("Controller %s:\n", name);
	indent = 0;
	memset(done, 0, sizeof done);
	if (addr)
		usbdev(f, addr, 0);
	else
		usbdump(f);
}

int
main(argc, argv)
	int argc;
	char **argv;
{
	int ch, i, f;
	char buf[50];
	extern int optind;
	extern char *optarg;
	char *dev = 0;
	int addr = 0;
	int ncont;

	while ((ch = getopt(argc, argv, "a:f:v")) != -1) {
		switch(ch) {
		case 'a':
			addr = atoi(optarg);
			break;
		case 'f':
			dev = optarg;
			break;
		case 'v':
			verbose = 1;
			break;
		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (dev == 0) {
		for (ncont = 0, i = 0; i < 10; i++) {
			sprintf(buf, "%s%d", USBDEV, i);
			f = open(buf, O_RDONLY);
			if (f >= 0) {
				ncont++;
				dumpone(buf, f, addr);
				close(f);
			} else {
				if (errno == EACCES)
					warn("%s", buf);
			}
		}
		if (verbose && ncont == 0)
			printf("%s: no USB controllers found\n", __progname);
	} else {
		f = open(dev, O_RDONLY);
		if (f >= 0)
			dumpone(dev, f, addr);
		else
			err(1, "%s", dev);
	}
	exit(0);
}
