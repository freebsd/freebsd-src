/*-
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.org> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 *
 * The AMD Elan sc520 is a system-on-chip gadget which is used in embedded
 * kind of things, see www.soekris.com for instance, and it has a few quirks
 * we need to deal with.
 * Unfortunately we cannot identify the gadget by CPUID output because it
 * depends on strapping options and only the stepping field may be useful
 * and those are undocumented from AMDs side.
 *
 * So instead we recognize the on-chip host-PCI bridge and call back from
 * sys/i386/pci/pci_bus.c to here if we find it.
 *
 * #ifdef ELAN_PPS
 *   The Elan has three general purpose counters, which when used just right
 *   can hardware timestamp external events with approx 250 nanoseconds
 *   resolution _and_ precision.  Connect the signal to TMR1IN and PIO7.
 *   (You can use any PIO pin, look for PIO7 to change this).  Use the
 *   PPS-API on the /dev/elan-mmcr device.
 * #endif ELAN_PPS
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_cpu.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/sysctl.h>
#include <sys/timetc.h>
#include <sys/proc.h>
#include <sys/uio.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/malloc.h>
#include <sys/sysctl.h>
#include <sys/timepps.h>
#include <sys/watchdog.h>

#include <dev/led/led.h>
#include <machine/md_var.h>

#include <vm/vm.h>
#include <vm/pmap.h>

uint16_t *elan_mmcr;

#ifdef ELAN_PPS
/* Relating to the PPS-api */
static struct pps_state elan_pps;

static void
elan_poll_pps(struct timecounter *tc)
{
	static int state;
	int i;

	/* XXX: This is PIO7, change to your preference */
	i = elan_mmcr[0xc30 / 2] & 0x80;
	if (i == state)
		return;
	state = i;
	if (!state)
		return;
	pps_capture(&elan_pps);
	elan_pps.capcount =
	    (elan_mmcr[0xc84 / 2] - elan_mmcr[0xc7c / 2]) & 0xffff;
	pps_event(&elan_pps, PPS_CAPTUREASSERT);
}
#endif /* ELAN_PPS */

static unsigned
elan_get_timecount(struct timecounter *tc)
{
	return (elan_mmcr[0xc84 / 2]);
}

/*
 * The Elan CPU can be run from a number of clock frequencies, this
 * allows you to override the default 33.3 MHZ.
 */
#ifndef ELAN_XTAL
#define ELAN_XTAL 33333333
#endif

static struct timecounter elan_timecounter = {
	elan_get_timecount,
	NULL,
	0xffff,
	ELAN_XTAL / 4,
	"ELAN",
	1000
};

static int
sysctl_machdep_elan_freq(SYSCTL_HANDLER_ARGS)
{
	u_int f;
	int error;

	f = elan_timecounter.tc_frequency * 4;
	error = sysctl_handle_int(oidp, &f, sizeof(f), req);
	if (error == 0 && req->newptr != NULL) 
		elan_timecounter.tc_frequency = (f + 3) / 4;
	return (error);
}

SYSCTL_PROC(_machdep, OID_AUTO, elan_freq, CTLTYPE_UINT | CTLFLAG_RW,
    0, sizeof (u_int), sysctl_machdep_elan_freq, "IU", "");

void
init_AMD_Elan_sc520(void)
{
	u_int new;
	int i;

	if (bootverbose)
		printf("Doing h0h0magic for AMD Elan sc520\n");
	elan_mmcr = pmap_mapdev(0xfffef000, 0x1000);

	/*-
	 * The i8254 is driven with a nonstandard frequency which is
	 * derived thusly:
	 *   f = 32768 * 45 * 25 / 31 = 1189161.29...
	 * We use the sysctl to get the timecounter etc into whack.
	 */
	
	new = 1189161;
	i = kernel_sysctlbyname(&thread0, "machdep.i8254_freq", 
	    NULL, 0, 
	    &new, sizeof new, 
	    NULL);
	if (bootverbose)
		printf("sysctl machdep.i8254_freq=%d returns %d\n", new, i);

	/* Start GP timer #2 and use it as timecounter, hz permitting */
	elan_mmcr[0xc82 / 2] = 0xc001;

#ifdef ELAN_PPS
	/* Set up GP timer #1 as pps counter */
	elan_mmcr[0xc24 / 2] &= ~0x10;
	elan_mmcr[0xc7a / 2] = 0x8000 | 0x4000 | 0x10 | 0x1;
	elan_pps.ppscap |= PPS_CAPTUREASSERT;
	pps_init(&elan_pps);
#endif

	tc_init(&elan_timecounter);
}

static d_ioctl_t elan_ioctl;
static d_mmap_t elan_mmap;

#ifdef CPU_SOEKRIS
/* Support for /dev/led/error */
static u_int soekris_errled_cookie = 0x200;
static dev_t soekris_errled_dev;

static void
gpio_led(void *cookie, int state)
{
	u_int u;

	u = *(u_int *)cookie;

	if (state)
		elan_mmcr[0xc34 / 2] = u;
	else
		elan_mmcr[0xc38 / 2] = u;
}
#endif /* CPU_SOEKRIS */

#define ELAN_MMCR	0

static struct cdevsw elan_cdevsw = {
	.d_ioctl =	elan_ioctl,
	.d_mmap =	elan_mmap,
	.d_name =	"elan",
};

static void
elan_drvinit(void)
{

	if (elan_mmcr == NULL)
		return;
	printf("Elan-mmcr driver: MMCR at %p\n", elan_mmcr);
	make_dev(&elan_cdevsw, ELAN_MMCR,
	    UID_ROOT, GID_WHEEL, 0600, "elan-mmcr");

#ifdef CPU_SOEKRIS
	soekris_errled_dev =
	    led_create(gpio_led, &soekris_errled_cookie, "error");
#endif /* CPU_SOEKRIS */
	return;
}

SYSINIT(elan, SI_SUB_PSEUDO, SI_ORDER_MIDDLE, elan_drvinit, NULL);

static int
elan_mmap(dev_t dev, vm_offset_t offset, vm_paddr_t *paddr, int nprot)
{

	if (minor(dev) != ELAN_MMCR)
		return (EOPNOTSUPP);
	if (offset >= 0x1000) 
		return (-1);
	*paddr = 0xfffef000;
	return (0);
}

static int
elan_watchdog(u_int spec)
{
	u_int u, v;
	static u_int cur;

	if (spec & ~__WD_LEGAL)
		return (EINVAL);
	switch (spec & (WD_ACTIVE|WD_PASSIVE)) {
	case WD_ACTIVE:
		u = spec & WD_INTERVAL;
		if (u > 35)
			return (EINVAL);
		u = imax(u - 5, 24);
		v = 2 << (u - 24);
		v |= 0xc000;

		/*
		 * There is a bug in some silicon which prevents us from
		 * writing to the WDTMRCTL register if the GP echo mode is
		 * enabled.  GP echo mode on the other hand is desirable
		 * for other reasons.  Save and restore the GP echo mode
		 * around our hardware tom-foolery.
		 */
		u = elan_mmcr[0xc00 / 2];
		elan_mmcr[0xc00 / 2] = 0;
		if (v != cur) {
			/* Clear the ENB bit */
			elan_mmcr[0xcb0 / 2] = 0x3333;
			elan_mmcr[0xcb0 / 2] = 0xcccc;
			elan_mmcr[0xcb0 / 2] = 0;

			/* Set new value */
			elan_mmcr[0xcb0 / 2] = 0x3333;
			elan_mmcr[0xcb0 / 2] = 0xcccc;
			elan_mmcr[0xcb0 / 2] = v;
			cur = v;
		} else {
			/* Just reset timer */
			elan_mmcr[0xcb0 / 2] = 0xaaaa;
			elan_mmcr[0xcb0 / 2] = 0x5555;
		}
		elan_mmcr[0xc00 / 2] = u;
		return (0);
	case WD_PASSIVE:
		return (EOPNOTSUPP);
	case 0:
		u = elan_mmcr[0xc00 / 2];
		elan_mmcr[0xc00 / 2] = 0;
		elan_mmcr[0xcb0 / 2] = 0x3333;
		elan_mmcr[0xcb0 / 2] = 0xcccc;
		elan_mmcr[0xcb0 / 2] = 0x4080;
		elan_mmcr[0xc00 / 2] = u;
		cur = 0;
		return (0);
	default:
		return (EINVAL);
	}

}

static int
elan_ioctl(dev_t dev, u_long cmd, caddr_t arg, int flag, struct  thread *tdr)
{
	int error;

	error = ENOTTY;
#ifdef ELAN_PPS
	error = pps_ioctl(cmd, arg, &elan_pps);
	/*
	 * We only want to incur the overhead of the PPS polling if we
	 * are actually asked to timestamp.
	 */
	if (elan_pps.ppsparam.mode & PPS_CAPTUREASSERT)
		elan_timecounter.tc_poll_pps = elan_poll_pps;
	else
		elan_timecounter.tc_poll_pps = NULL;
	if (error != ENOTTY)
		return (error);
#endif /* ELAN_PPS */

	if (cmd == WDIOCPATPAT)
		return elan_watchdog(*((u_int*)arg));

	/* Other future ioctl handling here */
	return(error);
}

