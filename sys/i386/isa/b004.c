/*
 * FreeBSD device driver for B004-compatible Transputer boards.
 *
 * based on Linux version Copyright (C) 1993 by Christoph Niemann
 *
 * Rewritten for FreeBSD by
 * Luigi Rizzo (luigi@iet.unipi.it) and
 * Lorenzo Vicisano (l.vicisano@iet.unipi.it)
 *    Dipartimento di Ingegneria dell'Informazione
 *    Universita` di Pisa
 *    via Diotisalvi 2, 56126 Pisa, ITALY
 * 14 september 1994
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Christoph Niemann,
 *      Luigi Rizzo and Lorenzo Vicisano - Dipartimento di Ingegneria
 *		dell'Informazione
 * 4. The names of these contributors may not be used to endorse or promote
 *  products derived from this software without specific prior written
 *  permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * NOTE NOTE NOTE
 * The assembler version is still under development.
 */

/* #define USE_ASM */

#include "bqu.h"
#if NBQU > 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/uio.h>
#include <sys/devconf.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#ifdef DEVFS
#include <sys/devfsext.h>
#endif /*DEVFS*/

#include <machine/clock.h>

#include <i386/isa/b004.h>
#include <i386/isa/isa.h>
#include <i386/isa/isa_device.h>


static u_char d_inb(u_int port);
static void d_outb(u_int port, u_char data);

static struct kern_devconf kdc_bqu[NBQU] = { {
	0, 0, 0,		/* filled in by dev_attach */
	"bqu", 0, { MDDT_ISA, 0 },
	isa_generic_externalize, 0, 0, ISA_EXTERNALLEN,
	&kdc_isa0,		/* parent */
	0,			/* parentdata */
	DC_UNCONFIGURED,	/* always start here */
	"B004-compatible Transputer board",
	DC_CLS_MISC
} };

#define IOCTL_OUT(arg, ret)             *(int*)arg = ret

#define	B004PRI	(PZERO+8)

#define B004_CHANCE	8

/*
 * Define these symbols if you want to debug the code.
 */
#undef B004_DEBUG
#undef B004_DEBUG_2

#ifdef B004_DEBUG
#define out(port,data)	d_outb(port, data)
#define in(a)		d_inb(((u_int)a))
#else
#define out(port, data)		outb(port,data)
#define in(port)		inb(((u_int)port))
#endif B004_DEBUG

#ifdef B004_DEBUG
#define DEB(x) x
#define NO_DEB(x) /* */
#else
#define DEB(x) /* */
#define NO_DEB(x) x
#endif

#ifdef B004_DEBUG_2
#define DEB2(x) x
#else
#define DEB2(x)
#endif

static int bquprobe(struct isa_device *idp);
static int bquattach(struct isa_device *idp);


struct isa_driver bqudriver = {
   bquprobe, bquattach, "bqu"
};

static	d_open_t	bquopen;
static	d_close_t	bquclose;
static	d_read_t	bquread;
static	d_write_t	bquwrite;
static	d_ioctl_t	bquioctl;
static	d_select_t	bquselect;

#define CDEV_MAJOR 8
struct cdevsw bqu_cdevsw = 
	{ bquopen,      bquclose,       bquread,        bquwrite,       /*8*/
	  bquioctl,     nostop,         nullreset,      nodevtotty,/* tputer */
	  bquselect,    nommap,         NULL,	"bqu",	NULL,	-1 };

static int b004_sleep; /* wait address */

static struct b004_struct b004_table[NBQU];

static int first_time=1;

/*
 * At these addresses the driver will search for B004-compatible boards
 */
static int
b004_base_addresses[B004_CHANCE] = {
    /* 0x150, 0x170, 0x190, 0x200, 0x300, 0x320, 0x340, 0x360 */
    0x150, 0x190, 0, 0, 0, 0, 0, 0
};


static void
d_outb(u_int port, u_char data)
{

	printf("OUT 0x%x TO 0x%x\n",data,port);
	outb(port,data);
}

static u_char d_inb(u_int port)
{
u_char ap;
	ap=inb(port);
	printf("INPUT 0x%x FROM 0x%x\n",ap,port);
	return(ap);
}

static int
detected(int base)
{
    int i;
    for(i=0;i<NBQU;i++)
	if ((B004_F(i) & B004_EXIST) && (B004_BASE(i)==base)) return 1;
    return (0);
}

#define b004_delay(a)	DELAY(10000)

/*
 * static void bqureset(): reset transputer network.
 *
 */

static void
bqureset( const int dev_min )
{
    DEB(printf("B004 resetting transputer at link %d.\n", dev_min);)
    out(B004_BASE(dev_min)+B004_ANALYSE_OFFSET, B004_DEASSERT_ANALYSE);
    b004_delay(dev_min);

    out(B004_BASE(dev_min) + B004_RESET_OFFSET, B004_DEASSERT_RESET);
    b004_delay(dev_min);

    out(B004_BASE(dev_min) + B004_RESET_OFFSET, B004_ASSERT_RESET);
    b004_delay(dev_min);

    out(B004_BASE(dev_min) + B004_RESET_OFFSET, B004_DEASSERT_RESET);
    b004_delay(dev_min);

    DEB(printf("B004 reset done.\n");)
}

/*
 * static void bquanalyse(): switch transputer network to analyse mode.
 *
 */

static void
bquanalyse( const int dev_min )
{
    DEB(printf("B004 analysing transputer at link %d.\n", dev_min);)

    out(B004_BASE(dev_min) + B004_ANALYSE_OFFSET, B004_DEASSERT_ANALYSE);
    b004_delay(dev_min);

    out(B004_BASE(dev_min) + B004_ANALYSE_OFFSET, B004_ASSERT_ANALYSE);
    b004_delay(dev_min);

    out(B004_BASE(dev_min) + B004_RESET_OFFSET, B004_ASSERT_RESET);
    b004_delay(dev_min);

    out(B004_BASE(dev_min) + B004_RESET_OFFSET, B004_DEASSERT_RESET);
    b004_delay(dev_min);

    out(B004_BASE(dev_min) + B004_ANALYSE_OFFSET, B004_DEASSERT_ANALYSE);
    b004_delay(dev_min);

    DEB(printf("B004 switching to analyse-mode done.\n");)
}


/****************************************************************************
 *
 * int bquread() - read bytes from the link interface.
 *
 * At first, the driver checks if the link-interface is ready to send a byte
 * to the PC. If not, this check is repeated up to B004_MAXTRY times.
 * If the link-interface is not ready after this loop, the driver sleeps for
 * an NO=1 ticks and then checks the link-interface again.
 * If the interface is still not ready, repeats as above incrementing NO.
 * Once almost one byte is read N0 is set to 1.
 * If B004_TIMEOUT != 0 and the link-interface is not ready for more than
 * B004_TIMEOUT ticks read aborts returnig with the number of bytes read
 * or with an error if no byte was read.
 *
 * By default, B004_TIMEOUT is = 0 (read is blocking)
 *
 *****************************************************************************/

static int
bquread(dev_t dev, struct uio *uio, int flag)
{
    unsigned int dev_min = minor(dev) & 7;

    int	timeout=B004_TIMEOUT(dev_min);
    int	Timeout=timeout;
    int idr=B004_IDR(dev_min);
    int isr=B004_ISR(dev_min);
    char	buffer[B004_MAX_BYTES];

    if ( uio->uio_resid < 0) {
	DEB(printf("B004: invalid count for reading = %d.\n", uio->uio_resid);)
	return EINVAL;
    }

    while ( uio->uio_resid ) {
	int sleep_ticks=0;
	char *p, *last, *lim;
	int i, end = min(B004_MAX_BYTES,uio->uio_resid);
	lim= &buffer[end];
	for (p= buffer; p<lim;) {
	    last=p;
	    /*** try to read as much as possible ***/
#ifdef USE_ASM
	/* assembly code uses a very tight loop, with
	 * BX= data port, DX= address port, CX=count, ES:DI=p, AL=data, AH=1
	 * SI=retry counter
	 */
	__asm__ (
	    "movl %1, %%edx\n\t" /* isr */
	    "movl %2, %%ebx\n\t" /* idr */
	    "movl %3, %%edi\n" /* p */
	    "movl %4, %%ecx\n\t" /* lim */
	    "subl %%edi, %%ecx\n\t"

	    "push %%es\n\t"
	    "movw %%ss, %%ax\n\t" /** prepare ES, DF for transfer */
	    "movw %%ax, %%es\n\t"
	    "cld\n\t"
	    "movb $1, %%ah\n\t"

	    "1:\tinb %%dx, %%al\n\t"
	    "testb %%ah, %%al\n\t"
	    "jz 2f\n\t"
	    "xchgl %%edx, %%ebx\n\t"
	    "insb\n\t"
	    "xchgl %%edx, %%ebx\n"
	    "2:\tloop 1b\n\t"

	    "pop %%es\n\t"
	    "movl %%edi, %0\n\t" /* store p */
		: /* out */ "=g" (p)
		: /* in */ "g" (isr), "g" (idr), "g" (p), "g" (lim)
		: /* regs */ "eax", "ebx", "edx", "ecx", "edi");
#else
	    for (i=lim - p; i-- ;)
		if (inb(isr)&B004_READBYTE) *p++ =(char) inb(idr);
#endif
	    if (last!=p) {
		sleep_ticks = 0;
	    } else {
		/*** no new data read, must sleep ***/
		sleep_ticks= (sleep_ticks<20 ? sleep_ticks+1 : sleep_ticks);
		if (Timeout) {
		    if (timeout <=0) {
			DEB2(printf("Read : TIMEOUT OCCURRED XXXXXXXXXXX\n");)
			break;
		    }
		    if (timeout < sleep_ticks) sleep_ticks=timeout;
		    timeout -= sleep_ticks;
		}
		DEB2(printf("Read: SLEEPING FOR %d TICKS XXXXX\n",sleep_ticks);)
		if (tsleep((caddr_t)&b004_sleep, B004PRI | PCATCH,
		    "b004_rd", sleep_ticks)!=EWOULDBLOCK) return 1;
	    }
	}
	if (p != buffer) {
	    uiomove((caddr_t)buffer, p - buffer, uio);
	}
	if( (Timeout) && (timeout <= 0) )
	    break;
    }
    return 0;
} /* bquread() */


/*
 * int bquwrite() - write to the link interface.
 */

static int
bquwrite(dev_t dev, struct uio *uio, int flag)
{
    unsigned int dev_min = minor(dev) & 7;

    int	i, end;
    int	timeout=B004_TIMEOUT(dev_min);
    int	Timeout=timeout;
    int odr=B004_ODR(dev_min);
    int osr=B004_OSR(dev_min);
    char	buffer[B004_MAX_BYTES];

    if ( uio->uio_resid < 0) {
	DEB(printf("B004 invalid argument for writing: count = %d.\n", uio->uio_resid);)
	return EINVAL;
    }

    while ( uio->uio_resid ) {
	int sleep_ticks=0;
	char *p, *last, *lim;
	end = min(B004_MAX_BYTES,uio->uio_resid);
	uiomove((caddr_t)buffer, end, uio);

	lim= &buffer[end];
	for (p= &buffer[0]; p<lim;) {
	    last=p;
#ifdef USE_ASM
	/* assembly code uses a very tight loop, with
	 * BX= data port, DX= address port, CX=count, DS:SI=p, AL=data, AH=1
	 * DI= retry counter
	 * Unfortunately, C is almost as fast as this!
	 */
	__asm__ (
	    "movl %1, %%edx\n\t" /* osr */
	    "movl %2, %%ebx\n\t" /* odr */
	    "movl %3, %%esi\n" /* p */
	    "movl %4, %%ecx\n\t" /* lim */
	    "subl %%esi, %%ecx\n\t"

	    "push %%ds\n\t"
	    "movw %%ss, %%ax\n\t" /** prepare DS, DF for transfer */
	    "movw %%ax, %%ds\n\t"
	    "cld\n\t"
	    "movb $1, %%ah\n\t"
	    "movw $100, %%di\n\t"

	    "1:\tinb %%dx, %%al\n\t"
	    "testb %%ah, %%al\n\t"
	    "jz 2f\n\t"
	    "xchgl %%edx, %%ebx\n\t"
	    "outsb\n\t"
	    "xchgl %%edx, %%ebx\n\t"
	    "loop 1b\n\t"
	    "jmp 3f\n"

	"2:\tdec %%di\n\t"
	"jnc 1b\n\t"

	    "3:\tpop %%ds\n"
	    "movl %%esi, %0\n\t" /* store p */
		: /* out */ "=g" (p)
		: /* in */ "g" (osr), "g" (odr), "g" (p), "g" (lim)
		: /* regs */ "eax", "ebx", "edx", "ecx", "esi", "edi");
#else
	    for (i=lim - p; i-- ; ) {
		if (inb(osr)&B004_WRITEBYTE) outb(odr, *p++);
	    }
#endif
	    if (p != last ) {
		sleep_ticks=0;
	    } else {
		sleep_ticks= (sleep_ticks<20 ? sleep_ticks+1 : sleep_ticks);
		if (Timeout) {
		    if (timeout <=0) {
			DEB2(printf("Write : TIMEOUT OCCURRED XXXXXXXXXXX\n");)
			uio->uio_resid += (lim - p);
			break;
		    }
		    if (timeout < sleep_ticks) sleep_ticks=timeout;
		    timeout -= sleep_ticks;
		}
		DEB2(printf("Write: SLEEPING FOR %d TICKS XXXXXXX\n",sleep_ticks);)
		if (tsleep((caddr_t)&b004_sleep, B004PRI | PCATCH,
		    "b004_rd", sleep_ticks)!=EWOULDBLOCK) return 1;
	    }
	}
	if( (Timeout) && (timeout <= 0) )
	    break;
    }
    return 0;
} /* bquwrite() */

/*
 * int bquopen() -- open the link-device.
 *
 */

static int
bquopen(dev_t dev, int flags, int fmt, struct proc *p)
{
    unsigned int dev_min = minor(dev) & 7;

    if (dev_min >= NBQU) {
	DEB(printf("B004 not opened, minor number >= %d.\n", NBQU);)
	return ENXIO;
    }
    if ((B004_F(dev_min) & B004_EXIST) == 0) {
	DEB(printf("B004 not opened, board %d does not exist.\n", dev_min);)
	return ENXIO;
    }
    if (B004_F(dev_min) & B004_BUSY) {
	DEB(printf("B004 not opened, board busy (minor = %d).\n", dev_min);)
	return EBUSY;
    }
    B004_F(dev_min) |= B004_BUSY;
    kdc_bqu[dev_min].kdc_state = DC_BUSY;
    B004_TIMEOUT(dev_min) = 0;
    DEB(printf( "B004 opened, minor = %d.\n", dev_min );)
    return 0;
} /* bquopen() */


/*
 * int b004close() -- close the link device.
 */

static int
bquclose(dev_t dev, int flags, int fmt, struct proc *p)
{
    unsigned int dev_min = minor(dev) & 7;

    if (dev_min >= NBQU) {
	DEB(printf("B004 not released, minor number >= %d.\n", NBQU);)
	return ENXIO;
    }
    B004_F(dev_min) &= ~B004_BUSY;
    kdc_bqu[dev_min].kdc_state = DC_IDLE;
    DEB(printf("B004(%d) released.\n", dev_min );)
    return 0;
}

static int
bquselect(dev_t dev, int rw, struct proc *p)
{
    /* still unimplemented */
    return(1);
}

/*
 * int bquioctl()
 *
 * Supported functions:
 * - reset
 * - analyse
 * - test error flag
 * - set timeout
 */

static int
bquioctl(dev_t dev, int cmd, caddr_t addr, int flag, struct proc *p)
{
    unsigned int dev_min = minor(dev) & 7;
    int result = 0;

    if (dev_min >= NBQU) {
	DEB(printf("B004 ioctl exit, minor >= %d.\n", NBQU );)
	return ENODEV;
    }

    if ((B004_F(dev_min) & B004_EXIST) == 0) {
	DEB(printf("B004 ioctl exit, (B004_F & B004_EXIST) == 0.\n" );)
	return ENODEV;
    }

    switch ( cmd ) {
	case B004RESET:		/* reset transputer */
	    bqureset(dev_min);
	DEB(printf("B004 ioctl B004RESET, done\n" );)
	    break;
	case B004WRITEABLE:	/* can we write a byte to the C012 ? */
	    IOCTL_OUT (addr, ((in(B004_OSR(dev_min))&B004_WRITEBYTE) != 0 ));
	    break;
	case B004READABLE:	/* can we read a byte from C012 ? */
	    IOCTL_OUT (addr, ((in(B004_ISR(dev_min)) & B004_READBYTE) != 0 ));
	    break;
	case B004ANALYSE:	/* switch transputer to analyse mode */
	    bquanalyse(dev_min);
	    break;
	case B004ERROR:		/* test error-flag */
	    IOCTL_OUT (addr,
		((inb(B004_BASE(dev_min)+B004_ERROR_OFFSET) &
		B004_TEST_ERROR) ? 0 : 1));
	    break;
	case B004TIMEOUT:	/* set, retrieve timeout for writing & reading*/
	    B004_TIMEOUT(dev_min) = *((int *)addr);
	    break;
	default: result = EINVAL;
    }
    return result;
} /* bquioctl() */


static inline void
bqu_registerdev(struct isa_device *id)
{
	int unit = id->id_unit;

	kdc_bqu[unit] = kdc_bqu[0]; /* XXX */ /* ??Eh?? */
	kdc_bqu[unit].kdc_unit = unit;
	kdc_bqu[unit].kdc_parentdata = id;
	dev_attach(&kdc_bqu[unit]);
}

static int
bquattach(struct isa_device *idp)
{
	int unit = idp->id_unit;
	struct b004_struct *bp;
	char	name[32];
	int	i;

	kdc_bqu[unit].kdc_state = DC_IDLE;

#ifdef DEVFS
#define BQU_UID 66
#define BQU_GID 66
#define BQU_PERM 0600
	bp = &b004_table[unit];
	for ( i = 0; i < 8; i++) {
#ifdef NOTYET
	/*	if (we've done all the ports found) break; */
#endif
		sprintf(name,"ttyba%d" ,i);
		bp->devfs_token[i][0]=devfs_add_devsw(
			"/", name, &bqu_cdevsw, i, DV_CHR,
			BQU_UID, BQU_GID, BQU_PERM);
		sprintf(name,"ttybd%d" ,i);
		bp->devfs_token[i][0]=devfs_add_devsw(
			"/", name, &bqu_cdevsw, i+64, DV_CHR,
			BQU_UID, BQU_GID, BQU_PERM);
		sprintf(name,"ttybc%d" ,i);
		bp->devfs_token[i][0]=devfs_add_devsw(
			"/", name, &bqu_cdevsw, i+128, DV_CHR,
			BQU_UID, BQU_GID, BQU_PERM);
		sprintf(name,"ttybd%d" ,i);
		bp->devfs_token[i][0]=devfs_add_devsw(
			"/", name, &bqu_cdevsw, i+192, DV_CHR,
			BQU_UID, BQU_GID, BQU_PERM);
	}
#endif
	return 1;
}

/*
 * int bquprobe
 *
 * Initializes the driver. It tries to detect the hardware
 * and sets up all relevant data-structures.
 */

static int
bquprobe(struct isa_device *idp)
{
    unsigned int test;
    unsigned int dev_min = idp->id_unit;
    int i,found = 0;
    /* After a reset it should be possible to write a byte to
       the B004. So let'S do a reset and then test the output status
       register
    */
#ifdef undef
	printf(
	  "bquprobe::\nIOBASE 0x%x\nIRQ %d\nDRQ %d\nMSIZE %d\nUNIT %d\nFLAGS"
	  "x0%x\nALIVE %d\n",idp->id_iobase,idp->id_irq,
	  idp->id_drq,idp->id_msize,idp->id_unit,idp->id_flags,idp->id_alive);
#endif
    if(first_time){
	for(i=0;i<NBQU;i++) B004_F(i) &= ~B004_EXIST;
	first_time=0;
    }

    if(dev_min >= NBQU) return (0);	/* No more descriptors */
    if ((idp->id_iobase < 0x100) || (idp->id_iobase >= 0x1000))
	idp->id_iobase=0;		/* Dangerous isa addres ) */
#ifndef DEV_LKM
    bqu_registerdev(idp);
#endif /* not DEV_LKM */


    for (test = 0; (test < B004_CHANCE); test++) {
	if((idp->id_iobase==0)&&((!b004_base_addresses[test])||
	   detected(b004_base_addresses[test])))
	    continue;
	idp->id_iobase=b004_base_addresses[test];

        DEB(printf("Probing device %d at address 0x%x\n",dev_min,
		idp->id_iobase);
	)
	b004_delay(test);
	B004_F(dev_min) = 0;
	B004_TIMEOUT(dev_min) = B004_INIT_TIMEOUT;
	B004_BASE(dev_min) = idp->id_iobase;
	B004_ODR(dev_min) = B004_BASE(dev_min) + B004_ODR_OFFSET;
	B004_ISR(dev_min) = B004_BASE(dev_min) + B004_ISR_OFFSET;
	B004_OSR(dev_min) = B004_BASE(dev_min) + B004_OSR_OFFSET;
	bqureset(dev_min);

	for (i = 0; i < B004_MAXTRY; i++)
	    if ( in(B004_OSR(dev_min))  == B004_WRITEBYTE) {
		B004_F(dev_min) |= B004_EXIST;
		out(B004_BASE(dev_min) + B008_INT_OFFSET, 0);
		b004_delay(test);
		if (in(B004_BASE(dev_min) + B008_INT_OFFSET) & 0x0f == 0)
		    B004_BOARDTYPE(dev_min) = B008;
		else
		    B004_BOARDTYPE(dev_min) = B004;
		printf("bqu%d at 0x0%x (polling) is a B00%s\n",
		    dev_min,B004_IDR(dev_min),
		    (B004_BOARDTYPE(dev_min) == B004) ? "4" : "8");
		found = 1;
		break;
	    }
	if(!found) {
	    idp->id_iobase=0;
	}
	else break;

    }

    if (!found){
	printf("b004probe(): no B004-board found.\n");
	return (0);
    }

    idp->id_maddr=NULL;
    idp->id_irq=0;
    if(B004_BOARDTYPE(dev_min) == B004)
	return(18);
    else
	return(20);
} /* bquprobe() */


static bqu_devsw_installed = 0;

static void
bqu_drvinit(void *unused)
{
	dev_t dev;

	if( ! bqu_devsw_installed ) {
		dev = makedev(CDEV_MAJOR, 0);
		cdevsw_add(&dev,&bqu_cdevsw, NULL);
		bqu_devsw_installed = 1;
    	}
}

SYSINIT(bqudev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,bqu_drvinit,NULL)


#endif /* NBQU */
