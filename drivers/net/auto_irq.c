/* auto_irq.c: Auto-configure IRQ lines for linux. */
/*
    Written 1994 by Donald Becker.

    The author may be reached as becker@scyld.com

    This code is a general-purpose IRQ line detector for devices with
    jumpered IRQ lines.  If you can make the device raise an IRQ (and
    that IRQ line isn't already being used), these routines will tell
    you what IRQ line it's using -- perfect for those oh-so-cool boot-time
    device probes!

    To use this, first call autoirq_setup(timeout). TIMEOUT is how many
    'jiffies' (1/100 sec.) to detect other devices that have active IRQ lines,
    and can usually be zero at boot.  'autoirq_setup()' returns the bit
    vector of nominally-available IRQ lines (lines may be physically in-use,
    but not yet registered to a device).
    Next, set up your device to trigger an interrupt.
    Finally call autoirq_report(TIMEOUT) to find out which IRQ line was
    most recently active.  The TIMEOUT should usually be zero, but may
    be set to the number of jiffies to wait for a slow device to raise an IRQ.

    The idea of using the setup timeout to filter out bogus IRQs came from
    the serial driver.
*/


#ifdef version
static const char *version=
"auto_irq.c:v1.11 Donald Becker (becker@scyld.com)";
#endif

#include <linux/module.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <asm/bitops.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <linux/netdevice.h>

static unsigned long irqs;

void autoirq_setup(int waittime)
{
	irqs = probe_irq_on();
}

#define BUSY_LOOP_UNTIL(j) while ((long)(jiffies-(j)) < 0) ;
int autoirq_report(int waittime)
{
	unsigned long delay = jiffies + waittime;
	BUSY_LOOP_UNTIL(delay)
	return probe_irq_off(irqs);
}

EXPORT_SYMBOL(autoirq_setup);
EXPORT_SYMBOL(autoirq_report);


/*
 * Local variables:
 *  compile-command: "gcc -DKERNEL -Wall -O6 -fomit-frame-pointer -I/usr/src/linux/net/tcp -c auto_irq.c"
 *  version-control: t
 *  kept-new-versions: 5
 *  c-indent-level: 4
 *  tab-width: 4
 * End:
 */
