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
 * #ifdef CPU_ELAN_PPS
 *   The Elan has three general purpose counters, and when two of these
 *   are used just right they can hardware timestamp external events with
 *   approx 125 nsec resolution and +/- 125 nsec precision.
 *
 *   Connect the signal to TMR1IN and a GPIO pin, and configure the GPIO pin
 *   with a 'P' in sysctl machdep.elan_gpio_config.
 *
 *   The rising edge of the signal will start timer 1 counting up from
 *   zero, and when the timecounter polls for PPS, both counter 1 & 2 is
 *   read, as well as the GPIO bit.  If a rising edge has happened, the
 *   contents of timer 1 which is how long time ago the edge happened,
 *   is subtracted from timer 2 to give us a "true time stamp".
 *
 *   Echoing the PPS signal on any GPIO pin is supported (set it to 'e'
 *   or 'E' (inverted) in the sysctl)  The echo signal should only be
 *   used as a visual indication, not for calibration since it suffers
 *   from 1/hz (or more) jitter which the timestamps are compensated for.
 * #endif CPU_ELAN_PPS
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_cpu.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
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

static char gpio_config[33];

uint16_t *elan_mmcr;

#ifdef CPU_ELAN_PPS
static struct pps_state elan_pps;
u_int	pps_a, pps_d;
u_int	echo_a, echo_d;
#endif /* CPU_ELAN_PPS */
u_int	led_cookie[32];
dev_t	led_dev[32];

static void
gpio_led(void *cookie, int state)
{
	u_int u, v;

	u = *(int *)cookie;
	v = u & 0xffff;
	u >>= 16;
	if (!state)
		v ^= 0xc;
	elan_mmcr[v / 2] = u;
}

static int
sysctl_machdep_elan_gpio_config(SYSCTL_HANDLER_ARGS)
{
	u_int u, v;
	int i, np, ne;
	int error;
	char buf[32], tmp[10];

	error = SYSCTL_OUT(req, gpio_config, 33);
	if (error != 0 || req->newptr == NULL)
		return (error);
	if (req->newlen != 32)
		return (EINVAL);
	error = SYSCTL_IN(req, buf, 32);
	if (error != 0)
		return (error);
	/* Disallow any disabled pins and count pps and echo */
	np = ne = 0;
	for (i = 0; i < 32; i++) {
		if (gpio_config[i] == '-' && (buf[i] != '-' && buf[i] != '.'))
			return (EPERM);
		if (buf[i] == 'P') {
			np++;
			if (np > 1)
				return (EINVAL);
		}
		if (buf[i] == 'e' || buf[i] == 'E') {
			ne++;
			if (ne > 1)
				return (EINVAL);
		}
		if (buf[i] != 'L' && buf[i] != 'l'
#ifdef CPU_ELAN_PPS
		    && buf[i] != 'P' && buf[i] != 'E' && buf[i] != 'e'
#endif /* CPU_ELAN_PPS */
		    && buf[i] != '.' && buf[i] != '-')
			return (EINVAL);
	}
#ifdef CPU_ELAN_PPS
	if (np == 0)
		pps_a = pps_d = 0;
	if (ne == 0)
		echo_a = echo_d = 0;
#endif
	for (i = 0; i < 32; i++) {
		u = 1 << (i & 0xf);
		if (i >= 16)
			v = 2;
		else
			v = 0;
		if (buf[i] != 'l' && buf[i] != 'L' && led_dev[i] != NULL) {
			led_destroy(led_dev[i]);	
			led_dev[i] = NULL;
			elan_mmcr[(0xc2a + v) / 2] &= ~u;
		}
		switch (buf[i]) {
#ifdef CPU_ELAN_PPS
		case 'P':
			pps_d = u;
			pps_a = 0xc30 + v;
			elan_mmcr[(0xc2a + v) / 2] &= ~u;
			gpio_config[i] = buf[i];
			break;
		case 'e':
		case 'E':
			echo_d = u;
			if (buf[i] == 'E')
				echo_a = 0xc34 + v;
			else
				echo_a = 0xc38 + v;
			elan_mmcr[(0xc2a + v) / 2] |= u;
			gpio_config[i] = buf[i];
			break;
#endif /* CPU_ELAN_PPS */
		case 'l':
		case 'L':
			if (buf[i] == 'L')
				led_cookie[i] = (0xc34 + v) | (u << 16);
			else
				led_cookie[i] = (0xc38 + v) | (u << 16);
			if (led_dev[i])
				break;
			sprintf(tmp, "gpio%d", i);
			led_dev[i] =
			    led_create(gpio_led, &led_cookie[i], tmp);
			elan_mmcr[(0xc2a + v) / 2] |= u;
			gpio_config[i] = buf[i];
			break;
		case '.':
			gpio_config[i] = buf[i];
			break;
		case '-':
		default:
			break;
		}
	}
	return (0);
}

SYSCTL_OID(_machdep, OID_AUTO, elan_gpio_config, CTLTYPE_STRING | CTLFLAG_RW,
    NULL, 0, sysctl_machdep_elan_gpio_config, "A", "Elan CPU GPIO pin config");

#ifdef CPU_ELAN_PPS
static void
elan_poll_pps(struct timecounter *tc)
{
	static int state;
	int i;
	u_int u;

	/*
	 * Order is important here.  We need to check the state of the GPIO
	 * pin first, in order to avoid reading timer 1 right before the
	 * state change.  Technically pps_a may be zero in which case we
	 * harmlessly read the REVID register and the contents of pps_d is
	 * of no concern.
	 */
	i = elan_mmcr[pps_a / 2] & pps_d;

	/*
	 * Subtract timer1 from timer2 to compensate for time from the
	 * edge until now.
	 */
	u = elan_mmcr[0xc84 / 2] - elan_mmcr[0xc7c / 2];

	/* If state did not change or we don't have a GPIO pin, return */
	if (i == state || pps_a == 0)
		return;

	state = i;

	/* If the state is "low", flip the echo GPIO and return.  */
	if (!i) {
		if (echo_a)
			elan_mmcr[(echo_a ^ 0xc) / 2] = echo_d;
		return;
	}

	/* State is "high", record the pps data */
	pps_capture(&elan_pps);
	elan_pps.capcount = u & 0xffff;
	pps_event(&elan_pps, PPS_CAPTUREASSERT);

	/* Twiddle echo bit */
	if (echo_a)
		elan_mmcr[echo_a / 2] = echo_d;
}
#endif /* CPU_ELAN_PPS */

static unsigned
elan_get_timecount(struct timecounter *tc)
{

	/* Read timer2, end of story */
	return (elan_mmcr[0xc84 / 2]);
}

/*
 * The Elan CPU can be run from a number of clock frequencies, this
 * allows you to override the default 33.3 MHZ.
 */
#ifndef CPU_ELAN_XTAL
#define CPU_ELAN_XTAL 33333333
#endif

static struct timecounter elan_timecounter = {
	elan_get_timecount,
	NULL,
	0xffff,
	CPU_ELAN_XTAL / 4,
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

/*
 * Positively identifying the Elan can only be done through the PCI id of
 * the host-bridge, this function is called from i386/pci/pci_bus.c.
 */
void
init_AMD_Elan_sc520(void)
{
	u_int new;
	int i;

	elan_mmcr = pmap_mapdev(0xfffef000, 0x1000);

	/*-
	 * The i8254 is driven with a nonstandard frequency which is
	 * derived thusly:
	 *   f = 32768 * 45 * 25 / 31 = 1189161.29...
	 * We use the sysctl to get the i8254 (timecounter etc) into whack.
	 */
	
	new = 1189161;
	i = kernel_sysctlbyname(&thread0, "machdep.i8254_freq", 
	    NULL, 0, &new, sizeof new, NULL);
	if (bootverbose || 1)
		printf("sysctl machdep.i8254_freq=%d returns %d\n", new, i);

	/* Start GP timer #2 and use it as timecounter, hz permitting */
	elan_mmcr[0xc8e / 2] = 0x0;
	elan_mmcr[0xc82 / 2] = 0xc001;

#ifdef CPU_ELAN_PPS
	/* Set up GP timer #1 as pps counter */
	elan_mmcr[0xc24 / 2] &= ~0x10;
	elan_mmcr[0xc7a / 2] = 0x8000 | 0x4000 | 0x10 | 0x1;
	elan_mmcr[0xc7e / 2] = 0x0;
	elan_mmcr[0xc80 / 2] = 0x0;
	elan_pps.ppscap |= PPS_CAPTUREASSERT;
	pps_init(&elan_pps);
#endif
	tc_init(&elan_timecounter);
}

static d_ioctl_t elan_ioctl;
static d_mmap_t elan_mmap;

static struct cdevsw elan_cdevsw = {
	.d_ioctl =	elan_ioctl,
	.d_mmap =	elan_mmap,
	.d_name =	"elan",
};

static void
elan_drvinit(void)
{

	/* If no elan found, just return */
	if (elan_mmcr == NULL)
		return;

	printf("Elan-mmcr driver: MMCR at %p.%s\n", 
	    elan_mmcr,
#ifdef CPU_ELAN_PPS
	    " PPS support."
#else
	    ""
#endif
	    );

	make_dev(&elan_cdevsw, 0,
	    UID_ROOT, GID_WHEEL, 0600, "elan-mmcr");

#ifdef CPU_SOEKRIS
	/* Create the error LED on GPIO9 */
	led_cookie[9] = 0x02000c34;
	led_dev[9] = led_create(gpio_led, &led_cookie[9], "error");
	
	/* Disable the unavailable GPIO pins */
	strcpy(gpio_config, "-----....--..--------..---------");
#else /* !CPU_SOEKRIS */
	/* We don't know which pins are available so enable them all */
	strcpy(gpio_config, "................................");
#endif /* CPU_SOEKRIS */
}

SYSINIT(elan, SI_SUB_PSEUDO, SI_ORDER_MIDDLE, elan_drvinit, NULL);

static int
elan_mmap(dev_t dev, vm_offset_t offset, vm_paddr_t *paddr, int nprot)
{

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

#ifdef CPU_ELAN_PPS
	if (pps_a != 0)
		error = pps_ioctl(cmd, arg, &elan_pps);
	/*
	 * We only want to incur the overhead of the PPS polling if we
	 * are actually asked to timestamp.
	 */
	if (elan_pps.ppsparam.mode & PPS_CAPTUREASSERT) {
		elan_timecounter.tc_poll_pps = elan_poll_pps;
	} else {
		elan_timecounter.tc_poll_pps = NULL;
	}
	if (error != ENOTTY)
		return (error);
#endif

	if (cmd == WDIOCPATPAT)
		return elan_watchdog(*((u_int*)arg));

	/* Other future ioctl handling here */
	return(error);
}

