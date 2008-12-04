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

#include <ctype.h>
#include <err.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/pciio.h>
#include <sys/queue.h>

#include <dev/pci/pcireg.h>

#include "pathnames.h"
#include "pciconf.h"

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

static void list_devs(int verbose, int caps);
static void list_verbose(struct pci_conf *p);
static const char *guess_class(struct pci_conf *p);
static const char *guess_subclass(struct pci_conf *p);
static int load_vendors(void);
static void readit(const char *, const char *, int);
static void writeit(const char *, const char *, const char *, int);
static void chkattached(const char *, int);

static int exitstatus = 0;

static void
usage(void)
{
	fprintf(stderr, "%s\n%s\n%s\n%s\n",
		"usage: pciconf -l [-cv]",
		"       pciconf -a selector",
		"       pciconf -r [-b | -h] selector addr[:addr2]",
		"       pciconf -w [-b | -h] selector addr value");
	exit (1);
}

int
main(int argc, char **argv)
{
	int c;
	int listmode, readmode, writemode, attachedmode, caps, verbose;
	int byte, isshort;

	listmode = readmode = writemode = attachedmode = caps = verbose = byte = isshort = 0;

	while ((c = getopt(argc, argv, "abchlrwv")) != -1) {
		switch(c) {
		case 'a':
			attachedmode = 1;
			break;

		case 'b':
			byte = 1;
			break;

		case 'c':
			caps = 1;
			break;

		case 'h':
			isshort = 1;
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
		list_devs(verbose, caps);
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
list_devs(int verbose, int caps)
{
	int fd;
	struct pci_conf_io pc;
	struct pci_conf conf[255], *p;
	int none_count = 0;

	if (verbose)
		load_vendors();

	fd = open(_PATH_DEVPCI, caps ? O_RDWR : O_RDONLY, 0);
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
			printf("%s%d@pci%d:%d:%d:%d:\tclass=0x%06x card=0x%08x "
			    "chip=0x%08x rev=0x%02x hdr=0x%02x\n",
			    (p->pd_name && *p->pd_name) ? p->pd_name :
			    "none",
			    (p->pd_name && *p->pd_name) ? (int)p->pd_unit :
			    none_count++, p->pc_sel.pc_domain,
			    p->pc_sel.pc_bus, p->pc_sel.pc_dev,
			    p->pc_sel.pc_func, (p->pc_class << 16) |
			    (p->pc_subclass << 8) | p->pc_progif,
			    (p->pc_subdevice << 16) | p->pc_subvendor,
			    (p->pc_device << 16) | p->pc_vendor,
			    p->pc_revid, p->pc_hdr);
			if (verbose)
				list_verbose(p);
			if (caps)
				list_caps(fd, p);
		}
	} while (pc.status == PCI_GETCONF_MORE_DEVS);

	close(fd);
}

static void
list_verbose(struct pci_conf *p)
{
	struct pci_vendor_info	*vi;
	struct pci_device_info	*di;
	const char *dp;

	TAILQ_FOREACH(vi, &pci_vendors, link) {
		if (vi->id == p->pc_vendor) {
			printf("    vendor     = '%s'\n", vi->desc);
			break;
		}
	}
	if (vi == NULL) {
		di = NULL;
	} else {
		TAILQ_FOREACH(di, &vi->devs, link) {
			if (di->id == p->pc_device) {
				printf("    device     = '%s'\n", di->desc);
				break;
			}
		}
	}
	if ((dp = guess_class(p)) != NULL)
		printf("    class      = %s\n", dp);
	if ((dp = guess_subclass(p)) != NULL)
		printf("    subclass   = %s\n", dp);
}

/*
 * This is a direct cut-and-paste from the table in sys/dev/pci/pci.c.
 */
static struct
{
	int	class;
	int	subclass;
	const char *desc;
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
	{PCIC_STORAGE,		PCIS_STORAGE_ATA_ADMA,	"ATA (ADMA)"},
	{PCIC_STORAGE,		PCIS_STORAGE_SATA,	"SATA"},
	{PCIC_STORAGE,		PCIS_STORAGE_SAS,	"SAS"},
	{PCIC_NETWORK,		-1,			"network"},
	{PCIC_NETWORK,		PCIS_NETWORK_ETHERNET,	"ethernet"},
	{PCIC_NETWORK,		PCIS_NETWORK_TOKENRING,	"token ring"},
	{PCIC_NETWORK,		PCIS_NETWORK_FDDI,	"fddi"},
	{PCIC_NETWORK,		PCIS_NETWORK_ATM,	"ATM"},
	{PCIC_NETWORK,		PCIS_NETWORK_ISDN,	"ISDN"},
	{PCIC_DISPLAY,		-1,			"display"},
	{PCIC_DISPLAY,		PCIS_DISPLAY_VGA,	"VGA"},
	{PCIC_DISPLAY,		PCIS_DISPLAY_XGA,	"XGA"},
	{PCIC_DISPLAY,		PCIS_DISPLAY_3D,	"3D"},
	{PCIC_MULTIMEDIA,	-1,			"multimedia"},
	{PCIC_MULTIMEDIA,	PCIS_MULTIMEDIA_VIDEO,	"video"},
	{PCIC_MULTIMEDIA,	PCIS_MULTIMEDIA_AUDIO,	"audio"},
	{PCIC_MULTIMEDIA,	PCIS_MULTIMEDIA_TELE,	"telephony"},
	{PCIC_MULTIMEDIA,	PCIS_MULTIMEDIA_HDA,	"HDA"},
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
	{PCIC_BRIDGE,		PCIS_BRIDGE_RACEWAY,	"PCI-RACEway"},
	{PCIC_SIMPLECOMM,	-1,			"simple comms"},
	{PCIC_SIMPLECOMM,	PCIS_SIMPLECOMM_UART,	"UART"},	/* could detect 16550 */
	{PCIC_SIMPLECOMM,	PCIS_SIMPLECOMM_PAR,	"parallel port"},
	{PCIC_SIMPLECOMM,	PCIS_SIMPLECOMM_MULSER,	"multiport serial"},
	{PCIC_SIMPLECOMM,	PCIS_SIMPLECOMM_MODEM,	"generic modem"},
	{PCIC_BASEPERIPH,	-1,			"base peripheral"},
	{PCIC_BASEPERIPH,	PCIS_BASEPERIPH_PIC,	"interrupt controller"},
	{PCIC_BASEPERIPH,	PCIS_BASEPERIPH_DMA,	"DMA controller"},
	{PCIC_BASEPERIPH,	PCIS_BASEPERIPH_TIMER,	"timer"},
	{PCIC_BASEPERIPH,	PCIS_BASEPERIPH_RTC,	"realtime clock"},
	{PCIC_BASEPERIPH,	PCIS_BASEPERIPH_PCIHOT,	"PCI hot-plug controller"},
	{PCIC_BASEPERIPH,	PCIS_BASEPERIPH_SDHC,	"SD host controller"},
	{PCIC_INPUTDEV,		-1,			"input device"},
	{PCIC_INPUTDEV,		PCIS_INPUTDEV_KEYBOARD,	"keyboard"},
	{PCIC_INPUTDEV,		PCIS_INPUTDEV_DIGITIZER,"digitizer"},
	{PCIC_INPUTDEV,		PCIS_INPUTDEV_MOUSE,	"mouse"},
	{PCIC_INPUTDEV,		PCIS_INPUTDEV_SCANNER,	"scanner"},
	{PCIC_INPUTDEV,		PCIS_INPUTDEV_GAMEPORT,	"gameport"},
	{PCIC_DOCKING,		-1,			"docking station"},
	{PCIC_PROCESSOR,	-1,			"processor"},
	{PCIC_SERIALBUS,	-1,			"serial bus"},
	{PCIC_SERIALBUS,	PCIS_SERIALBUS_FW,	"FireWire"},
	{PCIC_SERIALBUS,	PCIS_SERIALBUS_ACCESS,	"AccessBus"},
	{PCIC_SERIALBUS,	PCIS_SERIALBUS_SSA,	"SSA"},
	{PCIC_SERIALBUS,	PCIS_SERIALBUS_USB,	"USB"},
	{PCIC_SERIALBUS,	PCIS_SERIALBUS_FC,	"Fibre Channel"},
	{PCIC_SERIALBUS,	PCIS_SERIALBUS_SMBUS,	"SMBus"},
	{PCIC_WIRELESS,		-1,			"wireless controller"},
	{PCIC_WIRELESS,		PCIS_WIRELESS_IRDA,	"iRDA"},
	{PCIC_WIRELESS,		PCIS_WIRELESS_IR,	"IR"},
	{PCIC_WIRELESS,		PCIS_WIRELESS_RF,	"RF"},
	{PCIC_INTELLIIO,	-1,			"intelligent I/O controller"},
	{PCIC_INTELLIIO,	PCIS_INTELLIIO_I2O,	"I2O"},
	{PCIC_SATCOM,		-1,			"satellite communication"},
	{PCIC_SATCOM,		PCIS_SATCOM_TV,		"sat TV"},
	{PCIC_SATCOM,		PCIS_SATCOM_AUDIO,	"sat audio"},
	{PCIC_SATCOM,		PCIS_SATCOM_VOICE,	"sat voice"},
	{PCIC_SATCOM,		PCIS_SATCOM_DATA,	"sat data"},
	{PCIC_CRYPTO,		-1,			"encrypt/decrypt"},
	{PCIC_CRYPTO,		PCIS_CRYPTO_NETCOMP,	"network/computer crypto"},
	{PCIC_CRYPTO,		PCIS_CRYPTO_NETCOMP,	"entertainment crypto"},
	{PCIC_DASP,		-1,			"dasp"},
	{PCIC_DASP,		PCIS_DASP_DPIO,		"DPIO module"},
	{0, 0,		NULL}
};

static const char *
guess_class(struct pci_conf *p)
{
	int	i;

	for (i = 0; pci_nomatch_tab[i].desc != NULL; i++) {
		if (pci_nomatch_tab[i].class == p->pc_class)
			return(pci_nomatch_tab[i].desc);
	}
	return(NULL);
}

static const char *
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
	const char *dbf;
	FILE *db;
	struct pci_vendor_info *cv;
	struct pci_device_info *cd;
	char buf[1024], str[1024];
	char *ch;
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

		if ((ch = strchr(buf, '#')) != NULL)
			*ch = '\0';
		ch = strchr(buf, '\0') - 1;
		while (ch > buf && isspace(*ch))
			*ch-- = '\0';
		if (ch <= buf)
			continue;

		/* Can't handle subvendor / subdevice entries yet */
		if (buf[0] == '\t' && buf[1] == '\t')
			continue;

		/* Check for vendor entry */
		if (buf[0] != '\t' && sscanf(buf, "%04x %[^\n]", &id, str) == 2) {
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
		if (buf[0] == '\t' && sscanf(buf + 1, "%04x %[^\n]", &id, str) == 2) {
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

uint32_t
read_config(int fd, struct pcisel *sel, long reg, int width)
{
	struct pci_io pi;

	pi.pi_sel = *sel;
	pi.pi_reg = reg;
	pi.pi_width = width;

	if (ioctl(fd, PCIOCREAD, &pi) < 0)
		err(1, "ioctl(PCIOCREAD)");

	return (pi.pi_data);
}

static struct pcisel
getsel(const char *str)
{
	char *ep = strchr(str, '@');
	char *epbase;
	struct pcisel sel;
	unsigned long selarr[4];
	int i;

	if (ep == NULL)
		ep = (char *)str;
	else
		ep++;

	epbase = ep;

	if (strncmp(ep, "pci", 3) == 0) {
		ep += 3;
		i = 0;
		do {
			selarr[i++] = strtoul(ep, &ep, 10);
		} while ((*ep == ':' || *ep == '.') && *++ep != '\0' && i < 4);

		if (i > 2)
			sel.pc_func = selarr[--i];
		else
			sel.pc_func = 0;
		sel.pc_dev = selarr[--i];
		sel.pc_bus = selarr[--i];
		if (i > 0)
			sel.pc_domain = selarr[--i];
		else
			sel.pc_domain = 0;
	}
	if (*ep != '\x0' || ep == epbase)
		errx(1, "cannot parse selector %s", str);
	return sel;
}

static void
readone(int fd, struct pcisel *sel, long reg, int width)
{

	printf("%0*x", width*2, read_config(fd, sel, reg, width));
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
		if (i && !(i % 8))
			putchar(' ');
		putchar(i % (16/width) ? ' ' : '\n');
	}
	if (i % (16/width) != 1)
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
chkattached(const char *name, int width)
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
