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
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/types.h>
#include <sys/fcntl.h>

#include <err.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/pciio.h>

#include "pathnames.h"

static void list_devs(void);
static void readit(const char *, const char *, int);
static void writeit(const char *, const char *, const char *, int);
static void chkattached(const char *, int);

static int exitstatus = 0;

static void
usage()
{
	fprintf(stderr, "%s\n%s\n%s\n%s\n",
		"usage: pciconf -l",
		"       pciconf -a sel",
		"       pciconf -r [-b | -h] sel addr[:addr]",
		"       pciconf -w [-b | -h] sel addr [value]");
	exit (1);
}

int
main(int argc, char **argv)
{
	int c;
	int listmode, readmode, writemode, attachedmode;
	int byte, isshort;

	listmode = readmode = writemode = attachedmode = byte = isshort = 0;

	while ((c = getopt(argc, argv, "alrwbh")) != -1) {
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
			usage();
		}
	}

	if ((listmode && optind != argc)
	    || (writemode && optind + 3 != argc)
	    || (readmode && optind + 2 != argc)
	    || (attachedmode && optind + 1 != argc))
		usage();

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
 		usage();
	}

	return exitstatus;
}

static void
list_devs(void)
{
	int fd;
	struct pci_conf_io pc;
	struct pci_conf conf[255], *p;
	int none_count = 0;

	fd = open(_PATH_DEVPCI, O_RDWR, 0);
	if (fd < 0)
		err(1, "%s", _PATH_DEVPCI);

	bzero(&pc, sizeof(struct pci_conf_io));
	pc.match_buf_len = sizeof(conf);
	pc.matches = conf;

	do {
		if (ioctl(fd, PCIOCGETCONF, &pc) == -1)
			err(1, "ioctl(PCIOCGETCONF)");

		/*
		 * 255 entries should be more than enough for most people,
		 * but if someone has more devices, and then changes things
		 * around between ioctls, we'll do the cheezy thing and
		 * just bail.  The alternative would be to go back to the
		 * beginning of the list, and print things twice, which may
		 * not be desireable.
		 */
		if (pc.status == PCI_GETCONF_LIST_CHANGED) {
			warnx("PCI device list changed, please try again");
			exitstatus = 1;
			close(fd);
			return;
		} else if (pc.status ==  PCI_GETCONF_ERROR) {
			warnx("error returned from PCIOCGETCONF ioctl");
			exitstatus = 1;
			close(fd);
			return;
		}
		for (p = conf; p < &conf[pc.num_matches]; p++) {

			printf("%s%d@pci%d:%d:%d:\tclass=0x%06x card=0x%08x "
			       "chip=0x%08x rev=0x%02x hdr=0x%02x\n", 
			       (p->pd_name && *p->pd_name) ? p->pd_name :
			       "none",
			       (p->pd_name && *p->pd_name) ? (int)p->pd_unit :
			       none_count++,
			       p->pc_sel.pc_bus, p->pc_sel.pc_dev, 
			       p->pc_sel.pc_func, (p->pc_class << 16) |
			       (p->pc_subclass << 8) | p->pc_progif,
			       (p->pc_subdevice << 16) | p->pc_subvendor,
			       (p->pc_device << 16) | p->pc_vendor,
			       p->pc_revid, p->pc_hdr);
		}
	} while (pc.status == PCI_GETCONF_MORE_DEVS);

	close(fd);
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
readone(int fd, struct pcisel *sel, long reg, int width)
{
	struct pci_io pi;

	pi.pi_sel = *sel;
	pi.pi_reg = reg;
	pi.pi_width = width;

	if (ioctl(fd, PCIOCREAD, &pi) < 0)
		err(1, "ioctl(PCIOCREAD)");

	printf("0x%08x", pi.pi_data);
}

static void
readit(const char *name, const char *reg, int width)
{
	long rstart;
	long rend;
	long r;
	char *end;
	int i;
	int fd;
	struct pcisel sel;

	fd = open(_PATH_DEVPCI, O_RDWR, 0);
	if (fd < 0)
		err(1, "%s", _PATH_DEVPCI);

	rend = rstart = strtol(reg, &end, 0);
	if (end && *end == ':') {
		end++;
		rend = strtol(end, (char **) 0, 0);
	}
	sel = getsel(name);
	for (i = 1, r = rstart; r <= rend; i++, r += width) {	
		readone(fd, &sel, r, width);
		putchar(i % 4 ? ' ' : '\n');
	}
	if (i % 4 != 1)
		putchar('\n');
	close(fd);
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
