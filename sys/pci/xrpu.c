/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.org> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id: xrpu.c,v 1.2 1998/08/18 00:32:48 bde Exp $
 *
 * A very simple device driver for PCI cards based on Xilinx 6200 series
 * FPGA/RPU devices.  Current Functionality is to allow you to open and
 * mmap the entire thing into your program.
 *
 * Hardware currently supported:
 *	www.vcc.com HotWorks 1 6216 based card.
 *
 */

#include "xrpu.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/timepps.h>
#include <sys/devfsext.h>
#include <sys/xrpuio.h>
#include <pci/pcireg.h>
#include <pci/pcivar.h>

static	char*	xrpu_probe  (pcici_t tag, pcidi_t type);
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
static struct cdevsw xrpudevsw = {
	xrpu_open,	xrpu_close,	noread,		nowrite,
	xrpu_ioctl,	nullstop,	noreset,	nodevtotty,
	seltrue,	xrpu_mmap,	nostrategy,	"xrpu",
	NULL,		-1
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
	struct {
		pps_params_t	params;
		pps_info_t	info;
		int		cap;
		u_int		*assert, last_assert;
		u_int		*clear, last_clear;
	} pps[XRPU_MAX_PPS];
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
		if (sc->pps[i].assert) {
			ppscount = *(sc->pps[i].assert) & tc->tc_counter_mask;
			do {
				count1 = ppscount;
				ppscount =  *(sc->pps[i].assert) & tc->tc_counter_mask;
			} while (ppscount != count1);
			if (ppscount != sc->pps[i].last_assert) {
				timecounter_timespec(ppscount, &sc->pps[i].info.assert_timestamp);
				if (sc->pps[i].params.mode & PPS_OFFSETASSERT) {
					timespecadd(&sc->pps[i].info.assert_timestamp,
						&sc->pps[i].params.assert_offset);
					if (sc->pps[i].info.assert_timestamp.tv_nsec < 0) {
						sc->pps[i].info.assert_timestamp.tv_nsec += 1000000000;
						sc->pps[i].info.assert_timestamp.tv_sec -= 1;
					}
				}
				sc->pps[i].info.assert_sequence++;
				sc->pps[i].last_assert = ppscount;
			}
		}
		if (sc->pps[i].clear) {
			ppscount = *(sc->pps[i].clear) & tc->tc_counter_mask;
			do {
				count1 = ppscount;
				ppscount =  *(sc->pps[i].clear) & tc->tc_counter_mask;
			} while (ppscount != count1);
			if (ppscount != sc->pps[i].last_clear) {
				timecounter_timespec(ppscount, &sc->pps[i].info.clear_timestamp);
				if (sc->pps[i].params.mode & PPS_OFFSETASSERT) {
					timespecadd(&sc->pps[i].info.clear_timestamp,
						&sc->pps[i].params.clear_offset);
					if (sc->pps[i].info.clear_timestamp.tv_nsec < 0) {
						sc->pps[i].info.clear_timestamp.tv_nsec += 1000000000;
						sc->pps[i].info.clear_timestamp.tv_sec -= 1;
					}
				}
				sc->pps[i].info.clear_sequence++;
				sc->pps[i].last_clear = ppscount;
			}
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
xrpu_mmap(dev_t dev, int offset, int nprot)
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
		if (!sc->pps[i].cap)
			return ENODEV;
		error =  std_pps_ioctl(cmd, arg, &sc->pps[i].params,
                    &sc->pps[i].info, sc->pps[i].cap);
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
			devfs_add_devswf(&xrpudevsw, (i+1)<<16, DV_CHR, UID_ROOT, GID_WHEEL, 0600,
				"xpps%d", i);
			/* DEVFS */
			if (xt->xt_pps[i].xt_addr_assert) {
				sc->pps[i].assert = sc->virbase62 + xt->xt_pps[i].xt_addr_assert;
				sc->pps[i].cap |= PPS_CAPTUREASSERT | PPS_OFFSETASSERT;
			}
			if (xt->xt_pps[i].xt_addr_clear) {
				sc->pps[i].clear = sc->virbase62 + xt->xt_pps[i].xt_addr_clear;
				sc->pps[i].cap |= PPS_CAPTURECLEAR | PPS_OFFSETCLEAR;
			}
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

DATA_SET (pcidevice_set, xrpu_device);

static char* 
xrpu_probe (pcici_t tag, pcidi_t typea)
{
	int data = pci_conf_read(tag, PCI_CLASS_REG);
	u_int id = pci_conf_read(tag, PCI_ID_REG);
	const char *vendor, *chip, *type;

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
		cdevsw_add(&cdev, &xrpudevsw, NULL);

	devfs_add_devswf(&xrpudevsw, 0, DV_CHR, UID_ROOT, GID_WHEEL, 0600,
		"xrpu%d", unit);
}
