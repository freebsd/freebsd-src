/*-
 * Copyright (c) 1995 Jean-Marc Zucconi
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
#include "joy.h"

#if NJOY > 0

#include <errno.h>

#include <sys/param.h>
#include <sys/systm.h>

#include <machine/joystick.h>

#include <i386/isa/isa.h>
#include <i386/isa/isa_device.h>
#include <i386/isa/timerreg.h>

#ifdef JREMOD
#include <sys/conf.h>
#include <sys/kernel.h>
#ifdef DEVFS
#include <sys/devfsext.h>
#endif /*DEVFS*/
#define CDEV_MAJOR 51
#endif /*JREMOD*/

/* The game port can manage 4 buttons and 4 variable resistors (usually 2
 * joysticks, each with 2 buttons and 2 pots.) via the port at address 0x201.
 * Getting the state of the buttons is done by reading the game port:
 * buttons 1-4 correspond to bits 4-7 and resistors 1-4 (X1, Y1, X2, Y2)
 * to bits 0-3.
 * if button 1 (resp 2, 3, 4) is pressed, the bit 4 (resp 5, 6, 7) is set to 0
 * to get the value of a resistor, write the value 0xff at port and
 * wait until the corresponding bit returns to 0.
 */


/* the formulae below only work if u is  ``not too large''. See also
 * the discussion in microtime.s */
#define usec2ticks(u) 	(((u) * 19549)>>14)
#define ticks2usec(u) 	(((u) * 3433)>>12)


#define joypart(d) minor(d)&1
#define UNIT(d) minor(d)>>1&3
#ifndef JOY_TIMEOUT
#define JOY_TIMEOUT   2000 /* 2 milliseconds */
#endif

static struct {
    int port;
    int x_off[2], y_off[2];
    int timeout[2];
} joy[NJOY];


extern int timer0_max_count;

int joyprobe (struct isa_device *), joyattach (struct isa_device *);

struct isa_driver joydriver = {joyprobe, joyattach, "joy"};

static int get_tick ();


int
joyprobe (struct isa_device *dev)
{
#ifdef WANT_JOYSTICK_CONNECTED
    outb (dev->id_iobase, 0xff);
    DELAY (10000); /*  10 ms delay */
    return (inb (dev->id_iobase) & 0x0f) != 0x0f;
#else
    return 1;
#endif
}

int
joyattach (struct isa_device *dev)
{
    joy[dev->id_unit].port = dev->id_iobase;
    joy[dev->id_unit].timeout[0] = joy[dev->id_unit].timeout[1] = 0;
    printf("joy%d: joystick\n", dev->id_unit);

    return 1;
}

int
joyopen (dev_t dev, int flags, int fmt, struct proc *p)
{
    int unit = UNIT (dev);
    int i = joypart (dev);

    if (joy[unit].timeout[i])
	return EBUSY;
    joy[unit].x_off[i] = joy[unit].y_off[i] = 0;
    joy[unit].timeout[i] = JOY_TIMEOUT;
    return 0;
}
int
joyclose (dev_t dev, int flags, int fmt, struct proc *p)
{
    int unit = UNIT (dev);
    int i = joypart (dev);

    joy[unit].timeout[i] = 0;
    return 0;
}

int
joyread (dev_t dev, struct uio *uio, int flag)
{
    int unit = UNIT(dev);
    int port = joy[unit].port;
    int i, t0, t1;
    int state = 0, x = 0, y = 0;
    struct joystick c;

    disable_intr ();
    outb (port, 0xff);
    t0 = get_tick ();
    t1 = t0;
    i = usec2ticks(joy[unit].timeout[joypart(dev)]);
    while (t0-t1 < i) {
	state = inb (port);
	if (joypart(dev) == 1)
	    state >>= 2;
	t1 = get_tick ();
	if (t1 > t0)
	    t1 -= timer0_max_count;
	if (!x && !(state & 0x01))
	    x = t1;
	if (!y && !(state & 0x02))
	    y =  t1;
	if (x && y)
	    break;
    }
    enable_intr ();
    c.x = x ? joy[unit].x_off[joypart(dev)] + ticks2usec(t0-x) : 0x80000000;
    c.y = y ? joy[unit].y_off[joypart(dev)] + ticks2usec(t0-y) : 0x80000000;
    state >>= 4;
    c.b1 = ~state & 1;
    c.b2 = ~(state >> 1) & 1;
    return uiomove ((caddr_t)&c, sizeof(struct joystick), uio);
}
int joyioctl (dev_t dev, int cmd, caddr_t data, int flag, struct proc *p)
{
    int unit = UNIT (dev);
    int i = joypart (dev);
    int x;

    switch (cmd) {
    case JOY_SETTIMEOUT:
	x = *(int *) data;
	if (x < 1 || x > 10000) /* 10ms maximum! */
	    return EINVAL;
	joy[unit].timeout[i] = x;
	break;
    case JOY_GETTIMEOUT:
	*(int *) data = joy[unit].timeout[i];
	break;
    case JOY_SET_X_OFFSET:
	joy[unit].x_off[i] = *(int *) data;
	break;
    case JOY_SET_Y_OFFSET:
	joy[unit].y_off[i] = *(int *) data;
	break;
    case JOY_GET_X_OFFSET:
	*(int *) data = joy[unit].x_off[i];
	break;
    case JOY_GET_Y_OFFSET:
	*(int *) data = joy[unit].y_off[i];
	break;
    default:
	return ENXIO;
    }
    return 0;
}
static int
get_tick ()
{
    int low, high;

    outb (TIMER_MODE, TIMER_SEL0);
    low = inb (TIMER_CNTR0);
    high = inb (TIMER_CNTR0);

    return (high << 8) | low;
}


#ifdef JREMOD
struct cdevsw joy_cdevsw = 
	{ joyopen,	joyclose,	joyread,	nowrite,	/*51*/
	  joyioctl,	nostop,		nullreset,	nodevtotty,/*joystick */
	  seltrue,	nommap,		NULL};

static joy_devsw_installed = 0;

static void 	joy_drvinit(void *unused)
{
	dev_t dev;

	if( ! joy_devsw_installed ) {
		dev = makedev(CDEV_MAJOR,0);
		cdevsw_add(&dev,&joy_cdevsw,NULL);
		joy_devsw_installed = 1;
#ifdef DEVFS
		{
			int x;
/* default for a simple device with no probe routine (usually delete this) */
			x=devfs_add_devsw(
/*	path	name	devsw		minor	type   uid gid perm*/
	"/",	"joy",	major(dev),	0,	DV_CHR,	0,  0, 0600);
		}
#endif
    	}
}

SYSINIT(joydev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,joy_drvinit,NULL)

#endif /* JREMOD */

#endif /* NJOY > 0 */
