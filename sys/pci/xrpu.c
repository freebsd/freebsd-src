/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.org> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id: xrpu.c,v 1.10 1999/05/09 17:07:12 peter Exp $
 *
 * A very simple device driver for PCI cards based on Xilinx 6200 series
 * FPGA/RPU devices.  Current Functionality is to allow you to open and
 * mmap the entire thing into your program.
 *
 * Hardware currently supported:
 *	www.vcc.com HotWorks 1 6216 based card.
 *
 */

#include "opt_devfs.h"

#include "xrpu.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/timepps.h>
#ifdef DEVFS
#include <sys/devfsext.h>
#endif
#include <sys/xrpuio.h>
#include <pci/pcireg.h>
#include <pci/pcivar.h>

static	const char*	xrpu_probe  (pcici_t tag, pcidi_t type);
static	void	xrpu_attach (pcici_t tag, int unit);
static	u_long	xrpu_count;

static void xrpu_poll_pps(struct timecounter *tc);

/*
 * Device driver initialization stuff
 */

static d_open_t	xrpu_open;
static d_close_t xrpu_close;
static d_ioctl_t xrpu_ioctl;
static d_mmap_t xrpu_mmap;

#define CDEV_MAJOR 100
static struct cdevsw xrpu_cdevsw = {
	/* open */	xrpu_open,
	/* close */	xrpu_close,
	/* read */	noread,
	/* write */	nowrite,
	/* ioctl */	xrpu_ioctl,
	/* stop */	nostop,
	/* reset */	noreset,
	/* devtotty */	nodevtotty,
	/* poll */	nopoll,
	/* mmap */	xrpu_mmap,
	/* strategy */	nostrategy,
	/* name */	"xrpu",
	/* parms */	noparms,
	/* maj */	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	0,
	/* maxio */	0,
	/* bmaj */	-1
};

static MALLOC_DEFINE(M_XRPU, "xrpu", "XRPU related");

#define dev2unit(devt) (minor(devt) & 0xff)
#define dev2pps(devt) ((minor(devt) >> 16)-1)

static struct softc {
	pcici_t	tag;
	enum { NORMAL, TIMECOUNTER } mode;
	vm_offset_t virbase, physbase;
	u_int	*virbase62;
	struct timecounter tc;
	u_int *trigger, *latch, dummy;
	struct pps_state pps[XRPU_MAX_PPS];
	u_int *assert[XRPU_MAX_PPS], *clear[XRPU_MAX_PPS];
} *softc[NXRPU];

static unsigned         
xrpu_get_timecount(struct timecounter *tc)
{               
	struct softc *sc = tc->tc_priv;

	sc->dummy += *sc->trigger;
	return (*sc->latch & tc->tc_counter_mask);
}        

void            
xrpu_poll_pps(struct timecounter *tc)
{               
        struct softc *sc = tc->tc_priv;
	int i;
        unsigned count1, ppscount; 
                
	for (i = 0; i < XRPU_MAX_PPS; i++) {
		if (sc->assert[i]) {
			ppscount = *(sc->assert[i]) & tc->tc_counter_mask;
			do {
				count1 = ppscount;
				ppscount =  *(sc->assert[i]) & tc->tc_counter_mask;
			} while (ppscount != count1);
			pps_event(&sc->pps[i], &sc->tc, ppscount, PPS_CAPTUREASSERT);
		}
		if (sc->clear[i]) {
			ppscount = *(sc->clear[i]) & tc->tc_counter_mask;
			do {
				count1 = ppscount;
				ppscount =  *(sc->clear[i]) & tc->tc_counter_mask;
			} while (ppscount != count1);
			pps_event(&sc->pps[i], &sc->tc, ppscount, PPS_CAPTURECLEAR);
		}
	}
}

static int
xrpu_open(dev_t dev, int flag, int mode, struct proc *p)
{
	return (0);
}

static int
xrpu_close(dev_t dev, int flag, int mode, struct proc *p)
{ 
	return (0);
}

static int
xrpu_mmap(dev_t dev, vm_offset_t offset, int nprot)
{
	struct softc *sc = softc[dev2unit(dev)];
	if (offset >= 0x1000000) 
		return (-1);
	return (i386_btop(sc->physbase + offset));
}

static int
xrpu_ioctl(dev_t dev, u_long cmd, caddr_t arg, int flag, struct proc *pr)
{
	struct softc *sc = softc[dev2unit(dev)];
	int i, error;

	if (sc->mode == TIMECOUNTER) {
		i = dev2pps(dev);
		if (i < 0 || i >= XRPU_MAX_PPS)
			return ENODEV;
		error =  pps_ioctl(cmd, arg, &sc->pps[i]);
		return (error);
	}
		
	if (cmd == XRPU_IOC_TIMECOUNTING) {
		struct xrpu_timecounting *xt = (struct xrpu_timecounting *)arg;

		/* Name SHALL be zero terminated */
		xt->xt_name[sizeof xt->xt_name - 1] = '\0';
		i = strlen(xt->xt_name);
		sc->tc.tc_name = (char *)malloc(i + 1, M_XRPU, M_WAITOK);
		strcpy(sc->tc.tc_name, xt->xt_name);
		sc->tc.tc_frequency = xt->xt_frequency;
		sc->tc.tc_get_timecount = xrpu_get_timecount;
		sc->tc.tc_poll_pps = xrpu_poll_pps;
		sc->tc.tc_priv = sc;
		sc->tc.tc_counter_mask = xt->xt_mask;
		sc->trigger = sc->virbase62 + xt->xt_addr_trigger;
		sc->latch = sc->virbase62 + xt->xt_addr_latch;

		for (i = 0; i < XRPU_MAX_PPS; i++) {
			if (xt->xt_pps[i].xt_addr_assert == 0
			    && xt->xt_pps[i].xt_addr_clear == 0)
				continue;
#ifdef DEVFS
			devfs_add_devswf(&xrpu_cdevsw, (i+1)<<16, DV_CHR, UID_ROOT, GID_WHEEL, 
			    0600, "xpps%d", i);
#endif
			sc->pps[i].ppscap = 0;
			if (xt->xt_pps[i].xt_addr_assert) {
				sc->assert[i] = sc->virbase62 + xt->xt_pps[i].xt_addr_assert;
				sc->pps[i].ppscap |= PPS_CAPTUREASSERT;
			}
			if (xt->xt_pps[i].xt_addr_clear) {
				sc->clear[i] = sc->virbase62 + xt->xt_pps[i].xt_addr_clear;
				sc->pps[i].ppscap |= PPS_CAPTURECLEAR;
			}
			pps_init(&sc->pps[i]);
		}
		sc->mode = TIMECOUNTER;
		init_timecounter(&sc->tc);
		return (0);
	}
	error = ENOTTY;
	return (error);
}

/*
 * PCI initialization stuff
 */

static struct pci_device xrpu_device = {
	"xrpu",
	xrpu_probe,
	xrpu_attach,
	&xrpu_count,
	NULL
};

COMPAT_PCI_DRIVER (xrpu, xrpu_device);

static const char* 
xrpu_probe (pcici_t tag, pcidi_t typea)
{
	u_int id;
	const char *vendor, *chip, *type;

	(void)pci_conf_read(tag, PCI_CLASS_REG);
	id = pci_conf_read(tag, PCI_ID_REG);

	vendor = chip = type = 0;

	if (id == 0x6216133e) {
		return "VCC Hotworks-I xc6216";
	}
	return 0;
}

static void
xrpu_attach (pcici_t tag, int unit)
{
	struct softc *sc;
	dev_t cdev = makedev(CDEV_MAJOR, unit);

	sc = (struct softc *)malloc(sizeof *sc, M_XRPU, M_WAITOK);
	softc[unit] = sc;
	bzero(sc, sizeof *sc);

	sc->tag = tag;
	sc->mode = NORMAL;

	pci_map_mem(tag, PCI_MAP_REG_START, &sc->virbase, &sc->physbase);

	sc->virbase62 = (u_int *)(sc->virbase + 0x800000);

	if (bootverbose)
		printf("Mapped physbase %#lx to virbase %#lx\n",
		    (u_long)sc->physbase, (u_long)sc->virbase);

	if (!unit)
		cdevsw_add(&cdev, &xrpu_cdevsw, NULL);

#ifdef DEVFS
	devfs_add_devswf(&xrpu_cdevsw, 0, DV_CHR, UID_ROOT, GID_WHEEL, 0600,
		"xrpu%d", unit);
#endif
}
