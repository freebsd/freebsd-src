/*
 * Copyright 1996 Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that both the above copyright notice and this
 * permission notice appear in all copies, that both the above
 * copyright notice and this permission notice appear in all
 * supporting documentation, and that the name of M.I.T. not be used
 * in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  M.I.T. makes
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 * 
 * THIS SOFTWARE IS PROVIDED BY M.I.T. ``AS IS''.  M.I.T. DISCLAIMS
 * ALL EXPRESS OR IMPLIED WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT
 * SHALL M.I.T. BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$ANA: pciconf.c,v 1.1.1.1 1996/09/25 21:12:57 wollman Exp $
 */

#include <sys/types.h>
#include <sys/fcntl.h>

#include <err.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <pci/pcivar.h>
#include <pci/pci_ioctl.h>

#include "pathnames.h"

static void list_devs(void);
static void readit(const char *, const char *, int);
static void writeit(const char *, const char *, const char *, int);
static void chkattached(const char *, int);

static exitstatus = 0;

static void
usage(const char *argv0) {
	fprintf(stderr, "usage:\n\t%s -l\n"
		"\t%s -a sel\n"
		"\t%s [-r|-w] [-bh] sel addr [value]\n",
		argv0, argv0, argv0);
	exit (1);
}

int
main(int argc, char **argv)
{
	int c;
	int listmode, readmode, writemode, attachedmode;
	int byte, isshort;

	listmode = readmode = writemode = attachedmode = byte = isshort = 0;

	while ((c = getopt(argc, argv, "alrwbh")) != EOF) {
		switch(c) {
		case 'a':
			attachedmode = 1;
			break;

		case 'l':
			listmode = 1;
			break;

		case 'r':
			readmode = 1;
			break;
			
		case 'w':
			writemode = 1;
			break;

		case 'b':
			byte = 1;
			break;

		case 'h':
			isshort = 1;
			break;

		default:
			usage(argv[0]);
		}
	}

	if ((listmode && optind != argc)
	    || (writemode && optind + 3 != argc)
	    || (readmode && optind + 2 != argc)
	    || (attachedmode && optind + 1 != argc))
		usage(argv[0]);

	if (listmode) {
		list_devs();
	} else if(attachedmode) {
		chkattached(argv[optind], 
		       byte ? 1 : isshort ? 2 : 4);
	} else if(readmode) {
		readit(argv[optind], argv[optind + 1], 
		       byte ? 1 : isshort ? 2 : 4);
	} else if(writemode) {
		writeit(argv[optind], argv[optind + 1], argv[optind + 2],
		       byte ? 1 : isshort ? 2 : 4);
	} else {
 		usage(argv[0]);
	}

	return exitstatus;
}

static void
list_devs(void)
{
	int fd;
	struct pci_conf_io pc;
	struct pci_conf conf[255], *p;

	fd = open(_PATH_DEVPCI, O_RDONLY, 0);
	if (fd < 0)
		err(1, "%s", _PATH_DEVPCI);

	pc.pci_len = sizeof(conf);
	pc.pci_buf = conf;

	if (ioctl(fd, PCIOCGETCONF, &pc) < 0)
		err(1, "ioctl(PCIOCGETCONF)");

	close(fd);

	for (p = conf; p < &conf[pc.pci_len / sizeof conf[0]]; p++) {
	    printf("pci%d:%d:%d:\tclass=0x%06x card=0x%08lx chip=0x%08lx rev=0x%02x hdr=0x%02x\n",
		   p->pc_sel.pc_bus, p->pc_sel.pc_dev, p->pc_sel.pc_func, 
		   p->pc_class >> 8, p->pc_subid,
		   p->pc_devid, p->pc_class & 0xff, p->pc_hdr);
	}
}

static struct pcisel
getsel(const char *str)
{
	char *ep = (char*) str;
	struct pcisel sel;
	
	if (strncmp(ep, "pci", 3) == 0) {
		ep += 3;
		sel.pc_bus = strtoul(ep, &ep, 0);
		if (!ep || *ep++ != ':')
			errx(1, "cannot parse selector %s", str);
		sel.pc_dev = strtoul(ep, &ep, 0);
		if (!ep || *ep != ':') {
			sel.pc_func = 0;
		} else {
			ep++;
			sel.pc_func = strtoul(ep, &ep, 0);
		}
	}
	if (*ep == ':')
		ep++;
	if (*ep || ep == str)
		errx(1, "cannot parse selector %s", str);
	return sel;
}

static void
readit(const char *name, const char *reg, int width)
{
	int fd;
	struct pci_io pi;

	pi.pi_sel = getsel(name);
	pi.pi_reg = strtoul(reg, (char **)0, 0); /* XXX error check */
	pi.pi_width = width;

	fd = open(_PATH_DEVPCI, O_RDWR, 0);
	if (fd < 0)
		err(1, "%s", _PATH_DEVPCI);

	if (ioctl(fd, PCIOCREAD, &pi) < 0)
		err(1, "ioctl(PCIOCREAD)");

	printf("0x%08x\n", pi.pi_data);
}

static void
writeit(const char *name, const char *reg, const char *data, int width)
{
	int fd;
	struct pci_io pi;

	pi.pi_sel = getsel(name);
	pi.pi_reg = strtoul(reg, (char **)0, 0); /* XXX error check */
	pi.pi_width = width;
	pi.pi_data = strtoul(data, (char **)0, 0); /* XXX error check */

	fd = open(_PATH_DEVPCI, O_RDWR, 0);
	if (fd < 0)
		err(1, "%s", _PATH_DEVPCI);

	if (ioctl(fd, PCIOCWRITE, &pi) < 0)
		err(1, "ioctl(PCIOCWRITE)");
}

static void
chkattached (const char *name, int width)
{
	int fd;
	struct pci_io pi;

	pi.pi_sel = getsel(name);
	pi.pi_reg = 0;
	pi.pi_width = width;
	pi.pi_data = 0;

	fd = open(_PATH_DEVPCI, O_RDWR, 0);
	if (fd < 0)
		err(1, "%s", _PATH_DEVPCI);

	if (ioctl(fd, PCIOCATTACHED, &pi) < 0)
		err(1, "ioctl(PCIOCATTACHED)");

	exitstatus = pi.pi_data ? 0 : 2; /* exit(2), if NOT attached */
	printf("%s: %s%s\n", name, pi.pi_data == 0 ? "not " : "", "attached");
}
