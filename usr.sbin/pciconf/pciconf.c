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
#include <sys/queue.h>

#include <dev/pci/pcireg.h>

#include "pathnames.h"

struct pci_device_info 
{
    TAILQ_ENTRY(pci_device_info)	link;
    int					id;
    char				*desc;
};

struct pci_vendor_info
{
    TAILQ_ENTRY(pci_vendor_info)	link;
    TAILQ_HEAD(,pci_device_info)	devs;
    int					id;
    char				*desc;
};

TAILQ_HEAD(,pci_vendor_info)	pci_vendors;

static void list_devs(int vendors);
static void list_verbose(struct pci_conf *p);
static char *guess_class(struct pci_conf *p);
static char *guess_subclass(struct pci_conf *p);
static int load_vendors(void);
static void readit(const char *, const char *, int);
static void writeit(const char *, const char *, const char *, int);
static void chkattached(const char *, int);

static int exitstatus = 0;

static void
usage()
{
	fprintf(stderr, "%s\n%s\n%s\n%s\n",
		"usage: pciconf -l [-v]",
		"       pciconf -a sel",
		"       pciconf -r [-b | -h] sel addr[:addr]",
		"       pciconf -w [-b | -h] sel addr [value]");
	exit (1);
}

int
main(int argc, char **argv)
{
	int c;
	int listmode, readmode, writemode, attachedmode, verbose;
	int byte, isshort;

	listmode = readmode = writemode = attachedmode = verbose = byte = isshort = 0;

	while ((c = getopt(argc, argv, "alrwbhv")) != -1) {
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

		case 'v':
			verbose = 1;
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
		list_devs(verbose);
	} else if (attachedmode) {
		chkattached(argv[optind], 
		       byte ? 1 : isshort ? 2 : 4);
	} else if (readmode) {
		readit(argv[optind], argv[optind + 1], 
		       byte ? 1 : isshort ? 2 : 4);
	} else if (writemode) {
		writeit(argv[optind], argv[optind + 1], argv[optind + 2],
		       byte ? 1 : isshort ? 2 : 4);
	} else {
 		usage();
	}

	return exitstatus;
}

static void
list_devs(int verbose)
{
	int fd;
	struct pci_conf_io pc;
	struct pci_conf conf[255], *p;
	int none_count = 0;

	if (verbose)
		load_vendors();

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
			if (verbose)
				list_verbose(p);
		}
	} while (pc.status == PCI_GETCONF_MORE_DEVS);

	close(fd);
}

static void
list_verbose(struct pci_conf *p)
{
	struct pci_vendor_info	*vi;
	struct pci_device_info	*di;
	char *dp;
	
	TAILQ_FOREACH(vi, &pci_vendors, link) {
		if (vi->id == p->pc_vendor) {
			printf("    vendor   = '%s'\n", vi->desc);
			break;
		}
	}
	if (vi == NULL) {
		di = NULL;
	} else {
		TAILQ_FOREACH(di, &vi->devs, link) {
			if (di->id == p->pc_device) {
				printf("    device   = '%s'\n", di->desc);
				break;
			}
		}
	}
	if ((dp = guess_class(p)) != NULL)
		printf("    class    = %s\n", dp);
	if ((dp = guess_subclass(p)) != NULL)
		printf("    subclass = %s\n", dp);
}

/*
 * This is a direct cut-and-paste from the table in sys/dev/pci/pci.c.
 */
static struct
{
	int	class;
	int	subclass;
	char	*desc;
} pci_nomatch_tab[] = {
	{PCIC_OLD,		-1,			"old"},
	{PCIC_OLD,		PCIS_OLD_NONVGA,	"non-VGA display device"},
	{PCIC_OLD,		PCIS_OLD_VGA,		"VGA-compatible display device"},
	{PCIC_STORAGE,		-1,			"mass storage"},
	{PCIC_STORAGE,		PCIS_STORAGE_SCSI,	"SCSI"},
	{PCIC_STORAGE,		PCIS_STORAGE_IDE,	"ATA"},
	{PCIC_STORAGE,		PCIS_STORAGE_FLOPPY,	"floppy disk"},
	{PCIC_STORAGE,		PCIS_STORAGE_IPI,	"IPI"},
	{PCIC_STORAGE,		PCIS_STORAGE_RAID,	"RAID"},
	{PCIC_NETWORK,		-1,			"network"},
	{PCIC_NETWORK,		PCIS_NETWORK_ETHERNET,	"ethernet"},
	{PCIC_NETWORK,		PCIS_NETWORK_TOKENRING,	"token ring"},
	{PCIC_NETWORK,		PCIS_NETWORK_FDDI,	"fddi"},
	{PCIC_NETWORK,		PCIS_NETWORK_ATM,	"ATM"},
	{PCIC_DISPLAY,		-1,			"display"},
	{PCIC_DISPLAY,		PCIS_DISPLAY_VGA,	"VGA"},
	{PCIC_DISPLAY,		PCIS_DISPLAY_XGA,	"XGA"},
	{PCIC_MULTIMEDIA,	-1,			"multimedia"},
	{PCIC_MULTIMEDIA,	PCIS_MULTIMEDIA_VIDEO,	"video"},
	{PCIC_MULTIMEDIA,	PCIS_MULTIMEDIA_AUDIO,	"audio"},
	{PCIC_MEMORY,		-1,			"memory"},
	{PCIC_MEMORY,		PCIS_MEMORY_RAM,	"RAM"},
	{PCIC_MEMORY,		PCIS_MEMORY_FLASH,	"flash"},
	{PCIC_BRIDGE,		-1,			"bridge"},
	{PCIC_BRIDGE,		PCIS_BRIDGE_HOST,	"HOST-PCI"},
	{PCIC_BRIDGE,		PCIS_BRIDGE_ISA,	"PCI-ISA"},
	{PCIC_BRIDGE,		PCIS_BRIDGE_EISA,	"PCI-EISA"},
	{PCIC_BRIDGE,		PCIS_BRIDGE_MCA,	"PCI-MCA"},
	{PCIC_BRIDGE,		PCIS_BRIDGE_PCI,	"PCI-PCI"},
	{PCIC_BRIDGE,		PCIS_BRIDGE_PCMCIA,	"PCI-PCMCIA"},
	{PCIC_BRIDGE,		PCIS_BRIDGE_NUBUS,	"PCI-NuBus"},
	{PCIC_BRIDGE,		PCIS_BRIDGE_CARDBUS,	"PCI-CardBus"},
	{PCIC_BRIDGE,		PCIS_BRIDGE_OTHER,	"PCI-unknown"},
	{PCIC_SIMPLECOMM,	-1,			"simple comms"},
	{PCIC_SIMPLECOMM,	PCIS_SIMPLECOMM_UART,	"UART"},	/* could detect 16550 */
	{PCIC_SIMPLECOMM,	PCIS_SIMPLECOMM_PAR,	"parallel port"},
	{PCIC_BASEPERIPH,	-1,			"base peripheral"},
	{PCIC_BASEPERIPH,	PCIS_BASEPERIPH_PIC,	"interrupt controller"},
	{PCIC_BASEPERIPH,	PCIS_BASEPERIPH_DMA,	"DMA controller"},
	{PCIC_BASEPERIPH,	PCIS_BASEPERIPH_TIMER,	"timer"},
	{PCIC_BASEPERIPH,	PCIS_BASEPERIPH_RTC,	"realtime clock"},
	{PCIC_INPUTDEV,		-1,			"input device"},
	{PCIC_INPUTDEV,		PCIS_INPUTDEV_KEYBOARD,	"keyboard"},
	{PCIC_INPUTDEV,		PCIS_INPUTDEV_DIGITIZER,"digitizer"},
	{PCIC_INPUTDEV,		PCIS_INPUTDEV_MOUSE,	"mouse"},
	{PCIC_DOCKING,		-1,			"docking station"},
	{PCIC_PROCESSOR,	-1,			"processor"},
	{PCIC_SERIALBUS,	-1,			"serial bus"},
	{PCIC_SERIALBUS,	PCIS_SERIALBUS_FW,	"FireWire"},
	{PCIC_SERIALBUS,	PCIS_SERIALBUS_ACCESS,	"AccessBus"},	 
	{PCIC_SERIALBUS,	PCIS_SERIALBUS_SSA,	"SSA"},
	{PCIC_SERIALBUS,	PCIS_SERIALBUS_USB,	"USB"},
	{PCIC_SERIALBUS,	PCIS_SERIALBUS_FC,	"Fibre Channel"},
	{PCIC_SERIALBUS,	PCIS_SERIALBUS_SMBUS,	"SMBus"},
	{0, 0,		NULL}
};

static char *
guess_class(struct pci_conf *p)
{
	int	i;

	for (i = 0; pci_nomatch_tab[i].desc != NULL; i++) {
		if (pci_nomatch_tab[i].class == p->pc_class)
			return(pci_nomatch_tab[i].desc);
	}
	return(NULL);
}

static char *
guess_subclass(struct pci_conf *p)
{
	int	i;

	for (i = 0; pci_nomatch_tab[i].desc != NULL; i++) {
		if ((pci_nomatch_tab[i].class == p->pc_class) &&
		    (pci_nomatch_tab[i].subclass == p->pc_subclass))
			return(pci_nomatch_tab[i].desc);
	}
	return(NULL);
}

static int
load_vendors(void)
{
	char *dbf;
	FILE *db;
	struct pci_vendor_info *cv;
	struct pci_device_info *cd;
	char buf[100], str[100];
	int id, error;

	/*
	 * Locate the database and initialise.
	 */
	TAILQ_INIT(&pci_vendors);
	if ((dbf = getenv("PCICONF_VENDOR_DATABASE")) == NULL)
		dbf = _PATH_PCIVDB;
	if ((db = fopen(dbf, "r")) == NULL)
		return(1);
	cv = NULL;
	cd = NULL;
	error = 0;

	/*
	 * Scan input lines from the database
	 */
	for (;;) {
		if (fgets(buf, sizeof(buf), db) == NULL)
			break;

		/* Check for vendor entry */
		if ((buf[0] != '\t') && (sscanf(buf, "%04x\t%[^\n]", &id, str) == 2)) {
			if ((id == 0) || (strlen(str) < 1))
				continue;
			if ((cv = malloc(sizeof(struct pci_vendor_info))) == NULL) {
				warn("allocating vendor entry");
				error = 1;
				break;
			}
			if ((cv->desc = strdup(str)) == NULL) {
				free(cv);
				warn("allocating vendor description");
				error = 1;
				break;
			}
			cv->id = id;
			TAILQ_INIT(&cv->devs);
			TAILQ_INSERT_TAIL(&pci_vendors, cv, link);
			continue;
		}
		
		/* Check for device entry */
		if ((buf[0] == '\t') && (sscanf(buf + 1, "%04x\t%[^\n]", &id, str) == 2)) {
			if ((id == 0) || (strlen(str) < 1))
				continue;
			if (cv == NULL) {
				warnx("device entry with no vendor!");
				continue;
			}
			if ((cd = malloc(sizeof(struct pci_device_info))) == NULL) {
				warn("allocating device entry");
				error = 1;
				break;
			}
			if ((cd->desc = strdup(str)) == NULL) {
				free(cd);
				warn("allocating device description");
				error = 1;
				break;
			}
			cd->id = id;
			TAILQ_INSERT_TAIL(&cv->devs, cd, link);
			continue;
		}

		/* It's a comment or junk, ignore it */
	}
	if (ferror(db))
		error = 1;
	fclose(db);
	
	return(error);
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
