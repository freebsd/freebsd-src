/*-
 * Copyright (c) 2000, 2001 Michael Smith
 * Copyright (c) 2000 BSDi
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$FreeBSD$
 */
#include "opt_acpi.h"
#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/timetc.h>

#include <machine/bus_pio.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include "acpi.h"

#include <acpica/acpivar.h>
#include <pci/pcivar.h>

/*
 * A timecounter based on the free-running ACPI timer.
 *
 * Based on the i386-only mp_clock.c by <phk@FreeBSD.ORG>.
 */

/*
 * Hooks for the ACPI CA debugging infrastructure
 */
#define _COMPONENT	ACPI_SYSTEM
ACPI_MODULE_NAME("TIMER")

static device_t	acpi_timer_dev;
struct resource	*acpi_timer_reg;
#define TIMER_READ	bus_space_read_4(rman_get_bustag(acpi_timer_reg),	\
					 rman_get_bushandle(acpi_timer_reg),	\
					 0)

static u_int	acpi_timer_frequency = 14318182/4;

static void	acpi_timer_identify(driver_t *driver, device_t parent);
static int	acpi_timer_probe(device_t dev);
static int	acpi_timer_attach(device_t dev);
static unsigned	acpi_timer_get_timecount(struct timecounter *tc);
static unsigned	acpi_timer_get_timecount_safe(struct timecounter *tc);
static int	acpi_timer_sysctl_freq(SYSCTL_HANDLER_ARGS);
static void	acpi_timer_test(void);

/*
 * Driver hung off ACPI.
 */
static device_method_t acpi_timer_methods[] = {
    DEVMETHOD(device_identify,	acpi_timer_identify),
    DEVMETHOD(device_probe,	acpi_timer_probe),
    DEVMETHOD(device_attach,	acpi_timer_attach),

    {0, 0}
};

static driver_t acpi_timer_driver = {
    "acpi_timer",
    acpi_timer_methods,
    0,
};

static devclass_t acpi_timer_devclass;
DRIVER_MODULE(acpi_timer, acpi, acpi_timer_driver, acpi_timer_devclass, 0, 0);

/*
 * Timecounter.
 */
static struct timecounter acpi_timer_timecounter = {
    acpi_timer_get_timecount_safe,
    0,
    0xffffff,
    0,
    "ACPI"
};

SYSCTL_OPAQUE(_debug, OID_AUTO, acpi_timecounter, CTLFLAG_RD,
	      &acpi_timer_timecounter, sizeof(acpi_timer_timecounter), "S,timecounter", "");

/*
 * Locate the ACPI timer using the FADT, set up and allocate the I/O resources
 * we will be using.
 */
static void
acpi_timer_identify(driver_t *driver, device_t parent)
{
    device_t	dev;
    char	desc[40];
    int		rid;

    ACPI_FUNCTION_TRACE(__func__);

    if (acpi_disabled("timer"))
	return_VOID;

    if (AcpiGbl_FADT == NULL)
	return_VOID;
    
    if ((dev = BUS_ADD_CHILD(parent, 0, "acpi_timer", 0)) == NULL) {
	device_printf(parent, "could not add acpi_timer0\n");
	return_VOID;
    }
    acpi_timer_dev = dev;
    rid = 0;
    bus_set_resource(dev, SYS_RES_IOPORT, rid, AcpiGbl_FADT->V1_PmTmrBlk, sizeof(u_int32_t));
    if ((acpi_timer_reg = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid, 0, ~0, 1, RF_ACTIVE)) == NULL) {
	device_printf(dev, "couldn't allocate I/O resource (port 0x%x)\n", AcpiGbl_FADT->V1_PmTmrBlk);
	return_VOID;
    }
    if (getenv("debug.acpi.timer_test") != NULL)
	acpi_timer_test();

    acpi_timer_timecounter.tc_get_timecount = acpi_timer_get_timecount;
    acpi_timer_timecounter.tc_frequency = acpi_timer_frequency;
    tc_init(&acpi_timer_timecounter);

    sprintf(desc, "%d-bit timer at 3.579545MHz", AcpiGbl_FADT->TmrValExt ? 32 : 24);
    device_set_desc_copy(dev, desc);

#if 0    
    {
	u_int64_t	first;
    
	first = rdtsc();
	acpi_timer_get_timecount(NULL);	
	printf("acpi_timer_get_timecount %lld cycles\n", rdtsc() - first);

	first = rdtsc();
	acpi_timer_get_timecount_safe(NULL);
	printf("acpi_timer_get_timecount_safe %lld cycles\n", rdtsc() - first);
    }
#endif

    return_VOID;
}

static int
acpi_timer_probe(device_t dev)
{
    if (dev == acpi_timer_dev)
	return(0);
    return(ENXIO);
}

static int
acpi_timer_attach(device_t dev)
{
    return(0);
}

/*
 * Fetch current time value from reliable hardware.
 */
static unsigned
acpi_timer_get_timecount(struct timecounter *tc)
{
    return(TIMER_READ);
}

/*
 * Fetch current time value from hardware that may not correctly
 * latch the counter.
 */
static unsigned
acpi_timer_get_timecount_safe(struct timecounter *tc)
{
    unsigned u1, u2, u3;

    u2 = TIMER_READ;
    u3 = TIMER_READ;
    do {
	u1 = u2;
	u2 = u3;
	u3 = TIMER_READ;
    } while (u1 > u2 || u2 > u3);
    return (u2);
}

/*
 * Timecounter freqency adjustment interface.
 */ 
static int
acpi_timer_sysctl_freq(SYSCTL_HANDLER_ARGS)
{
    int error;
    u_int freq;
 
    if (acpi_timer_timecounter.tc_frequency == 0)
	return (EOPNOTSUPP);
    freq = acpi_timer_frequency;
    error = sysctl_handle_int(oidp, &freq, sizeof(freq), req);
    if (error == 0 && req->newptr != NULL) {
	acpi_timer_frequency = freq;
	acpi_timer_timecounter.tc_frequency = acpi_timer_frequency;
	tc_update(&acpi_timer_timecounter);
    }
    return (error);
}
 
SYSCTL_PROC(_machdep, OID_AUTO, acpi_timer_freq, CTLTYPE_INT | CTLFLAG_RW,
	    0, sizeof(u_int), acpi_timer_sysctl_freq, "I", "");

/*
 * Test harness for verifying ACPI timer behaviour.
 * Boot with debug.acpi.timer_test set to invoke this.
 */
static void
acpi_timer_test(void)
{
    u_int32_t	u1, u2, u3;
    
    u1 = TIMER_READ;
    u2 = TIMER_READ;
    u3 = TIMER_READ;
    
    device_printf(acpi_timer_dev, "timer test in progress, reboot to quit.\n");
    for (;;) {
	/*
	 * The failure case is where u3 > u1, but u2 does not fall between the two,
	 * ie. it contains garbage.
	 */
	if (u3 > u1) {
	    if ((u2 < u1) || (u2 > u3))
		device_printf(acpi_timer_dev, "timer is not monotonic: 0x%08x,0x%08x,0x%08x\n",
			      u1, u2, u3);
	}
	u1 = u2;
	u2 = u3;
	u3 = TIMER_READ;
    }
}

/*
 * Chipset workaround driver hung off PCI.
 *
 * Some ACPI timers are known or believed to suffer from implementation
 * problems which can lead to erroneous values being read from the timer.
 *
 * Since we can't trust unknown chipsets, we default to a timer-read
 * routine which compensates for the most common problem (as detailed
 * in the excerpt from the Intel PIIX4 datasheet below).
 *
 * When we detect a known-functional chipset, we disable the workaround
 * to improve speed.
 *
 * ] 20. ACPI Timer Errata
 * ]
 * ]   Problem: The power management timer may return improper result when
 * ]   read. Although the timer value settles properly after incrementing,
 * ]   while incrementing there is a 3nS window every 69.8nS where the
 * ]   timer value is indeterminate (a 4.2% chance that the data will be
 * ]   incorrect when read). As a result, the ACPI free running count up
 * ]   timer specification is violated due to erroneous reads.  Implication:
 * ]   System hangs due to the "inaccuracy" of the timer when used by
 * ]   software for time critical events and delays.
 * ]
 * ] Workaround: Read the register twice and compare.
 * ] Status: This will not be fixed in the PIIX4 or PIIX4E, it is fixed
 * ] in the PIIX4M.
 *
 * The counter is in other words not latched to the PCI bus clock when
 * read.  Notice the workaround isn't:  We need to read until we have
 * three monotonic samples and then use the middle one, otherwise we are
 * not protected against the fact that the bits can be wrong in two
 * directions.  If we only cared about monosity two reads would be enough.
 */

static int	acpi_timer_pci_probe(device_t dev);

static device_method_t acpi_timer_pci_methods[] = {
    DEVMETHOD(device_probe,	acpi_timer_pci_probe),
    {0, 0}
};

static driver_t acpi_timer_pci_driver = {
    "acpi_timer_pci",
    acpi_timer_pci_methods,
    0,
};

devclass_t acpi_timer_pci_devclass;
DRIVER_MODULE(acpi_timer_pci, pci, acpi_timer_pci_driver, acpi_timer_pci_devclass, 0, 0);

/*
 * Look at PCI devices going past; if we detect one we know contains
 * a functional ACPI timer device, enable the faster timecounter read
 * routine.
 */
static int
acpi_timer_pci_probe(device_t dev)
{
    int vendor, device, revid;
    
    vendor = pci_get_vendor(dev);
    device = pci_get_device(dev);
    revid  = pci_get_revid(dev);
    
    if (((vendor == 0x8086) && (device == 0x7113) && (revid >= 0x03))	|| /* PIIX4M */
	((vendor == 0x8086) && (device == 0x719b)) 			|| /* i440MX */
	0) {

	acpi_timer_timecounter.tc_get_timecount = acpi_timer_get_timecount;
	acpi_timer_timecounter.tc_name = "ACPI-fast";
	if (bootverbose)
	    device_printf(acpi_timer_dev, "functional ACPI timer detected, enabling fast timecount interface\n");
    }

    return(ENXIO);		/* we never match anything */
}
