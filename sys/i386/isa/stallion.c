/*****************************************************************************/

/*
 * stallion.c  -- stallion multiport serial driver.
 *
 * Copyright (c) 1995-1996 Greg Ungerer (gerg@stallion.oz.au).
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Greg Ungerer.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 * $Id: stallion.c,v 1.11 1997/03/24 21:38:51 davidn Exp $
 */

/*****************************************************************************/

#define	TTYDEFCHARS	1

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/tty.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <machine/clock.h>
#include <i386/isa/isa_device.h>
#include <i386/isa/ic/scd1400.h>
#include <machine/comstats.h>

#include "pci.h"
#if NPCI > 0
#include <pci/pcivar.h>
#include <pci/pcireg.h>
#endif

/*****************************************************************************/

/*
 *	Define the version level of the kernel - so we can compile in the
 *	appropriate bits of code. By default this will compile for a 2.1
 *	level kernel.
 */
#define	VFREEBSD	220

#if VFREEBSD >= 220
#define	STATIC		static
#else
#define	STATIC
#endif

/*****************************************************************************/

/*
 *	Define different board types. At the moment I have only declared
 *	those boards that this driver supports. But I will use the standard
 *	"assigned" board numbers. In the future this driver will support
 *	some of the other Stallion boards. Currently supported boards are
 *	abbreviated as EIO = EasyIO and ECH = EasyConnection 8/32.
 */
#define	BRD_EASYIO	20
#define	BRD_ECH		21
#define	BRD_ECHMC	22
#define	BRD_ECHPCI	26

/*
 *	When using the BSD "config" stuff there is no easy way to specifiy
 *	a secondary IO address region. So it is hard wired here. Also the
 *	shared interrupt information is hard wired here...
 */
static unsigned int	stl_ioshared = 0x280;
static unsigned int	stl_irqshared = 0;

/*****************************************************************************/

/*
 *	Define important driver limitations.
 */
#define	STL_MAXBRDS		8
#define	STL_MAXPANELS		4
#define	STL_PORTSPERPANEL	16
#define	STL_PORTSPERBRD		64

/*
 *	Define the important minor number break down bits. These have been
 *	chosen to be "compatable" with the standard sio driver minor numbers.
 *	Extra high bits are used to distinguish between boards.
 */
#define	STL_CALLOUTDEV		0x80
#define	STL_CTRLLOCK		0x40
#define	STL_CTRLINIT		0x20
#define	STL_CTRLDEV		(STL_CTRLLOCK | STL_CTRLINIT)

#define	STL_MEMDEV	0x07000000

#define	STL_DEFSPEED	9600
#define	STL_DEFCFLAG	(CS8 | CREAD | HUPCL)

/*
 *	I haven't really decided (or measured) what buffer sizes give
 *	a good balance between performance and memory usage. These seem
 *	to work pretty well...
 */
#define	STL_RXBUFSIZE		2048
#define	STL_TXBUFSIZE		2048

#define	STL_TXBUFLOW		(STL_TXBUFSIZE / 4)
#define	STL_RXBUFHIGH		(3 * STL_RXBUFSIZE / 4)

/*****************************************************************************/

/*
 *	Define our local driver identity first. Set up stuff to deal with
 *	all the local structures required by a serial tty driver.
 */
static const char	stl_drvname[] = "stl";
static const char	stl_longdrvname[] = "Stallion Multiport Serial Driver";
static const char	stl_drvversion[] = "1.0.0";
static int		stl_brdprobed[STL_MAXBRDS];

static int		stl_nrbrds = 0;
static int		stl_doingtimeout = 0;

static const char 	__file__[] = /*__FILE__*/ "stallion.c";

/*
 *	Define global stats structures. Not used often, and can be
 *	re-used for each stats call.
 */
static combrd_t		stl_brdstats;
static comstats_t	stl_comstats;

/*****************************************************************************/

/*
 *	Define a set of structures to hold all the board/panel/port info
 *	for our ports. These will be dynamically allocated as required.
 */

/*
 *	Define a ring queue structure for each port. This will hold the
 *	TX data waiting to be output. Characters are fed into this buffer
 *	from the line discipline (or even direct from user space!) and
 *	then fed into the UARTs during interrupts. Will use a clasic ring
 *	queue here for this. The good thing about this type of ring queue
 *	is that the head and tail pointers can be updated without interrupt
 *	protection - since "write" code only needs to change the head, and
 *	interrupt code only needs to change the tail.
 */
typedef struct {
	char	*buf;
	char	*endbuf;
	char	*head;
	char	*tail;
} stlrq_t;

/*
 *	Port, panel and board structures to hold status info about each.
 *	The board structure contains pointers to structures for each panel
 *	connected to it, and in turn each panel structure contains pointers
 *	for each port structure for each port on that panel. Note that
 *	the port structure also contains the board and panel number that it
 *	is associated with, this makes it (fairly) easy to get back to the
 *	board/panel info for a port. Also note that the tty struct is at
 *	the top of the structure, this is important, since the code uses
 *	this fact to get the port struct pointer from the tty struct
 *	pointer!
 */
typedef struct {
	struct tty	tty;
	int		portnr;
	int		panelnr;
	int		brdnr;
	int		ioaddr;
	int		uartaddr;
	int		pagenr;
	int		callout;
	int		brklen;
	int		dtrwait;
	int		dotimestamp;
	int		waitopens;
	int		hotchar;
	unsigned int	state;
	unsigned int	hwid;
	unsigned int	sigs;
	unsigned int	rxignoremsk;
	unsigned int	rxmarkmsk;
	unsigned long	clk;
	struct termios	initintios;
	struct termios	initouttios;
	struct termios	lockintios;
	struct termios	lockouttios;
	struct timeval	timestamp;
	comstats_t	stats;
	stlrq_t		tx;
	stlrq_t		rx;
	stlrq_t		rxstatus;
} stlport_t;

typedef struct {
	int		panelnr;
	int		brdnr;
	int		pagenr;
	int		nrports;
	int		iobase;
	unsigned int	hwid;
	unsigned int	ackmask;
	stlport_t	*ports[STL_PORTSPERPANEL];
} stlpanel_t;

typedef struct {
	int		brdnr;
	int		brdtype;
	int		unitid;
	int		state;
	int		nrpanels;
	int		nrports;
	int		irq;
	int		irqtype;
	unsigned int	ioaddr1;
	unsigned int	ioaddr2;
	unsigned int	iostatus;
	unsigned int	ioctrl;
	unsigned int	ioctrlval;
	unsigned int	hwid;
	unsigned long	clk;
	stlpanel_t	*panels[STL_MAXPANELS];
	stlport_t	*ports[STL_PORTSPERBRD];
} stlbrd_t;

static stlbrd_t		*stl_brds[STL_MAXBRDS];

/*
 *	Per board state flags. Used with the state field of the board struct.
 *	Not really much here yet!
 */
#define	BRD_FOUND	0x1

/*
 *	Define the port structure state flags. These set of flags are
 *	modified at interrupt time - so setting and reseting them needs
 *	to be atomic.
 */
#define	ASY_TXLOW	0x1
#define	ASY_RXDATA	0x2
#define	ASY_DCDCHANGE	0x4
#define	ASY_DTRWAIT	0x8
#define	ASY_RTSFLOW	0x10
#define	ASY_RTSFLOWMODE	0x20
#define	ASY_CTSFLOWMODE	0x40

#define	ASY_ACTIVE	(ASY_TXLOW | ASY_RXDATA | ASY_DCDCHANGE)

/*
 *	Define an array of board names as printable strings. Handy for
 *	referencing boards when printing trace and stuff.
 */
static char	*stl_brdnames[] = {
	(char *) NULL,
	(char *) NULL,
	(char *) NULL,
	(char *) NULL,
	(char *) NULL,
	(char *) NULL,
	(char *) NULL,
	(char *) NULL,
	(char *) NULL,
	(char *) NULL,
	(char *) NULL,
	(char *) NULL,
	(char *) NULL,
	(char *) NULL,
	(char *) NULL,
	(char *) NULL,
	(char *) NULL,
	(char *) NULL,
	(char *) NULL,
	(char *) NULL,
	"EasyIO",
	"EC8/32-AT",
	"EC8/32-MC",
	(char *) NULL,
	(char *) NULL,
	(char *) NULL,
	"EC8/32-PCI",
};

/*****************************************************************************/

/*
 *	Hardware ID bits for the EasyIO and ECH boards. These defines apply
 *	to the directly accessable io ports of these boards (not the cd1400
 *	uarts - they are in scd1400.h).
 */
#define	EIO_8PORTRS	0x04
#define	EIO_4PORTRS	0x05
#define	EIO_8PORTDI	0x00
#define	EIO_8PORTM	0x06
#define	EIO_IDBITMASK	0x07
#define	EIO_INTRPEND	0x08
#define	EIO_INTEDGE	0x00
#define	EIO_INTLEVEL	0x08

#define	ECH_ID		0xa0
#define	ECH_IDBITMASK	0xe0
#define	ECH_BRDENABLE	0x08
#define	ECH_BRDDISABLE	0x00
#define	ECH_INTENABLE	0x01
#define	ECH_INTDISABLE	0x00
#define	ECH_INTLEVEL	0x02
#define	ECH_INTEDGE	0x00
#define	ECH_INTRPEND	0x01
#define	ECH_BRDRESET	0x01

#define	ECHMC_INTENABLE	0x01
#define	ECHMC_BRDRESET	0x02

#define	ECH_PNLSTATUS	2
#define	ECH_PNL16PORT	0x20
#define	ECH_PNLIDMASK	0x07
#define	ECH_PNLINTRPEND	0x80
#define	ECH_ADDR2MASK	0x1e0

#define	EIO_CLK		25000000
#define	EIO_CLK8M	20000000
#define	ECH_CLK		EIO_CLK

/*
 *	Define the offsets within the register bank for all io registers.
 *	These io address offsets are common to both the EIO and ECH.
 */
#define	EREG_ADDR	0
#define	EREG_DATA	4
#define	EREG_RXACK	5
#define	EREG_TXACK	6
#define	EREG_MDACK	7

#define	EREG_BANKSIZE	8

/*
 *	Define the PCI vendor and device id for ECH8/32-PCI.
 */
#define	STL_PCIDEVID	0xd001100b

/*
 *	Define the vector mapping bits for the programmable interrupt board
 *	hardware. These bits encode the interrupt for the board to use - it
 *	is software selectable (except the EIO-8M).
 */
static unsigned char	stl_vecmap[] = {
	0xff, 0xff, 0xff, 0x04, 0x06, 0x05, 0xff, 0x07,
	0xff, 0xff, 0x00, 0x02, 0x01, 0xff, 0xff, 0x03
};

/*
 *	Set up enable and disable macros for the ECH boards. They require
 *	the secondary io address space to be activated and deactivated.
 *	This way all ECH boards can share their secondary io region.
 *	If this is an ECH-PCI board then also need to set the page pointer
 *	to point to the correct page.
 */
#define	BRDENABLE(brdnr,pagenr)						\
	if (stl_brds[(brdnr)]->brdtype == BRD_ECH)			\
		outb(stl_brds[(brdnr)]->ioctrl,				\
			(stl_brds[(brdnr)]->ioctrlval | ECH_BRDENABLE));\
	else if (stl_brds[(brdnr)]->brdtype == BRD_ECHPCI)		\
		outb(stl_brds[(brdnr)]->ioctrl, (pagenr));

#define	BRDDISABLE(brdnr)						\
	if (stl_brds[(brdnr)]->brdtype == BRD_ECH)			\
		outb(stl_brds[(brdnr)]->ioctrl,				\
			(stl_brds[(brdnr)]->ioctrlval | ECH_BRDDISABLE));

/*
 *	Define the cd1400 baud rate clocks. These are used when calculating
 *	what clock and divisor to use for the required baud rate. Also
 *	define the maximum baud rate allowed, and the default base baud.
 */
static int	stl_cd1400clkdivs[] = {
	CD1400_CLK0, CD1400_CLK1, CD1400_CLK2, CD1400_CLK3, CD1400_CLK4
};

#define	STL_MAXBAUD	230400

/*****************************************************************************/

/*
 *	Define macros to extract a brd and port number from a minor number.
 *	This uses the extended minor number range in the upper 2 bytes of
 *	the device number. This gives us plenty of minor numbers to play
 *	with...
 */
#define	MKDEV2BRD(m)	(((m) & 0x00700000) >> 20)
#define	MKDEV2PORT(m)	(((m) & 0x1f) | (((m) & 0x00010000) >> 11))

/*
 *	Define some handy local macros...
 */
#ifndef	MIN
#define	MIN(a,b)	(((a) <= (b)) ? (a) : (b))
#endif

/*****************************************************************************/

/*
 *	Declare all those functions in this driver!  First up is the set of
 *	externally visible functions.
 */

int	stlprobe(struct isa_device *idp);
int	stlattach(struct isa_device *idp);

STATIC	d_open_t	stlopen;
STATIC	d_close_t	stlclose;
STATIC	d_read_t	stlread;
STATIC	d_write_t	stlwrite;
STATIC	d_ioctl_t	stlioctl;
STATIC	d_stop_t	stlstop;

#if VFREEBSD >= 220
STATIC	d_devtotty_t	stldevtotty;
#else
struct tty		*stldevtotty(dev_t dev);
#endif

/*
 *	Internal function prototypes.
 */
static stlport_t *stl_dev2port(dev_t dev);
static int	stl_findfreeunit(void);
static int	stl_rawopen(stlport_t *portp);
static int	stl_rawclose(stlport_t *portp);
static int	stl_param(struct tty *tp, struct termios *tiosp);
static void	stl_start(struct tty *tp);
static void	stl_ttyoptim(stlport_t *portp, struct termios *tiosp);
static void	stl_dotimeout(void);
static void	stl_poll(void *arg);
static void	stl_rxprocess(stlport_t *portp);
static void	stl_dtrwakeup(void *arg);
static int	stl_brdinit(stlbrd_t *brdp);
static int	stl_initeio(stlbrd_t *brdp);
static int	stl_initech(stlbrd_t *brdp);
static int	stl_initports(stlbrd_t *brdp, stlpanel_t *panelp);
static void	stl_txisr(stlpanel_t *panelp, int ioaddr);
static void	stl_rxisr(stlpanel_t *panelp, int ioaddr);
static void	stl_mdmisr(stlpanel_t *panelp, int ioaddr);
static void	stl_setreg(stlport_t *portp, int regnr, int value);
static int	stl_getreg(stlport_t *portp, int regnr);
static int	stl_updatereg(stlport_t *portp, int regnr, int value);
static int	stl_getsignals(stlport_t *portp);
static void	stl_setsignals(stlport_t *portp, int dtr, int rts);
static void	stl_flowcontrol(stlport_t *portp, int hw, int sw);
static void	stl_ccrwait(stlport_t *portp);
static void	stl_enablerxtx(stlport_t *portp, int rx, int tx);
static void	stl_startrxtx(stlport_t *portp, int rx, int tx);
static void	stl_disableintrs(stlport_t *portp);
static void	stl_sendbreak(stlport_t *portp, long len);
static void	stl_flush(stlport_t *portp, int flag);
static int	stl_memioctl(dev_t dev, int cmd, caddr_t data, int flag,
			struct proc *p);
static int	stl_getbrdstats(caddr_t data);
static int	stl_getportstats(stlport_t *portp, caddr_t data);
static int	stl_clrportstats(stlport_t *portp, caddr_t data);
static stlport_t *stl_getport(int brdnr, int panelnr, int portnr);

#if NPCI > 0
static char	*stlpciprobe(pcici_t tag, pcidi_t type);
static void	stlpciattach(pcici_t tag, int unit);
static void	stlpciintr(void * arg);
#endif

/*****************************************************************************/

/*
 *	Declare the driver isa structure.
 */
struct isa_driver	stldriver = {
	stlprobe, stlattach, "stl"
};

/*****************************************************************************/

#if NPCI > 0

/*
 *	Declare the driver pci structure.
 */
static unsigned long	stl_count;

static struct pci_device	stlpcidriver = {
	"stl",
	stlpciprobe,
	stlpciattach,
	&stl_count,
	NULL,
};

DATA_SET (pcidevice_set, stlpcidriver);

#endif

/*****************************************************************************/

#if VFREEBSD >= 220

/*
 *	FreeBSD-2.2+ kernel linkage.
 */

#define	CDEV_MAJOR	72

static struct cdevsw stl_cdevsw = 
	{ stlopen,	stlclose,	stlread,	stlwrite,
	  stlioctl,	stlstop,	noreset,	stldevtotty,
	  ttselect,	nommap,		NULL,	"stl",	NULL,	-1 };

static stl_devsw_installed = 0;

static void stl_drvinit(void *unused)
{
	dev_t	dev;

	if (! stl_devsw_installed ) {
		dev = makedev(CDEV_MAJOR, 0);
		cdevsw_add(&dev, &stl_cdevsw, NULL);
		stl_devsw_installed = 1;
    	}
}

SYSINIT(sidev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,stl_drvinit,NULL)

#endif

/*****************************************************************************/

/*
 *	Probe for some type of EasyIO or EasyConnection 8/32 board at
 *	the supplied address. All we do is check if we can find the
 *	board ID for the board... (Note, PCI boards not checked here,
 *	they are done in the stlpciprobe() routine).
 */

int stlprobe(struct isa_device *idp)
{
	unsigned int	status;

#if DEBUG
	printf("stlprobe(idp=%x): unit=%d iobase=%x\n", (int) idp,
		idp->id_unit, idp->id_iobase);
#endif

	if (idp->id_unit > STL_MAXBRDS)
		return(0);

	status = inb(idp->id_iobase + 1);
	if ((status & ECH_IDBITMASK) == ECH_ID) {
		stl_brdprobed[idp->id_unit] = BRD_ECH;
		return(1);
	}

	status = inb(idp->id_iobase + 2);
	switch (status & EIO_IDBITMASK) {
	case EIO_8PORTRS:
	case EIO_8PORTM:
	case EIO_8PORTDI:
	case EIO_4PORTRS:
		stl_brdprobed[idp->id_unit] = BRD_EASYIO;
		return(1);
	default:
		break;
	}
	
	return(0);
}

/*****************************************************************************/

/*
 *	Find an available internal board number (unit number). The problem
 *	is that the same unit numbers can be assigned to different boards
 *	detected during the ISA and PCI initialization phases.
 */

static int stl_findfreeunit()
{
	int	i;

	for (i = 0; (i < STL_MAXBRDS); i++)
		if (stl_brds[i] == (stlbrd_t *) NULL)
			break;
	return((i >= STL_MAXBRDS) ? -1 : i);
}

/*****************************************************************************/

/*
 *	Allocate resources for and initialize the specified board.
 */

int stlattach(struct isa_device *idp)
{
	stlbrd_t	*brdp;

#if DEBUG
	printf("stlattach(idp=%x): unit=%d iobase=%x\n", idp,
		idp->id_unit, idp->id_iobase);
#endif

	brdp = (stlbrd_t *) malloc(sizeof(stlbrd_t), M_TTYS, M_NOWAIT);
	if (brdp == (stlbrd_t *) NULL) {
		printf("STALLION: failed to allocate memory (size=%d)\n",
			sizeof(stlbrd_t));
		return(0);
	}
	bzero(brdp, sizeof(stlbrd_t));

	if ((brdp->brdnr = stl_findfreeunit()) < 0) {
		printf("STALLION: too many boards found, max=%d\n",
			STL_MAXBRDS);
		return(0);
	}
	if (brdp->brdnr >= stl_nrbrds)
		stl_nrbrds = brdp->brdnr + 1;

	brdp->unitid = idp->id_unit;
	brdp->brdtype = stl_brdprobed[idp->id_unit];
	brdp->ioaddr1 = idp->id_iobase;
	brdp->ioaddr2 = stl_ioshared;
	brdp->irq = ffs(idp->id_irq) - 1;
	brdp->irqtype = stl_irqshared;
	stl_brdinit(brdp);

	return(1);
}

/*****************************************************************************/

#if NPCI > 0

/*
 *	Probe specifically for the PCI boards. We need to be a little
 *	carefull here, since it looks sort like a Nat Semi IDE chip...
 */

char *stlpciprobe(pcici_t tag, pcidi_t type)
{
	unsigned long	class;

#if DEBUG
	printf("stlpciprobe(tag=%x,type=%x)\n", (int) &tag, (int) type);
#endif

	switch (type) {
	case STL_PCIDEVID:
		break;
	default:
		return((char *) NULL);
	}

	class = pci_conf_read(tag, PCI_CLASS_REG);
	if ((class & PCI_CLASS_MASK) == PCI_CLASS_MASS_STORAGE)
		return((char *) NULL);

	return("Stallion EasyConnection 8/32-PCI");
}

/*****************************************************************************/

/*
 *	Allocate resources for and initialize the specified PCI board.
 */

void stlpciattach(pcici_t tag, int unit)
{
	stlbrd_t	*brdp;

#if DEBUG
	printf("stlpciattach(tag=%x,unit=%x)\n", (int) &tag, unit);
#endif

	brdp = (stlbrd_t *) malloc(sizeof(stlbrd_t), M_TTYS, M_NOWAIT);
	if (brdp == (stlbrd_t *) NULL) {
		printf("STALLION: failed to allocate memory (size=%d)\n",
			sizeof(stlbrd_t));
		return;
	}
	bzero(brdp, sizeof(stlbrd_t));

	if ((unit < 0) || (unit > STL_MAXBRDS)) {
		printf("STALLION: bad PCI board unit number=%d\n", unit);
		return;
	}

/*
 *	Allocate us a new driver unique unit number.
 */
	if ((brdp->brdnr = stl_findfreeunit()) < 0) {
		printf("STALLION: too many boards found, max=%d\n",
			STL_MAXBRDS);
		return;
	}
	if (brdp->brdnr >= stl_nrbrds)
		stl_nrbrds = brdp->brdnr + 1;

	brdp->unitid = 0;
	brdp->brdtype = BRD_ECHPCI;
	brdp->ioaddr1 = ((unsigned int) pci_conf_read(tag, 0x14)) & 0xfffc;
	brdp->ioaddr2 = ((unsigned int) pci_conf_read(tag, 0x10)) & 0xfffc;
	brdp->irq = ((int) pci_conf_read(tag, 0x3c)) & 0xff;
	brdp->irqtype = 0;
	if (pci_map_int(tag, stlpciintr, (void *) NULL, &tty_imask) == 0) {
		printf("STALLION: failed to map interrupt irq=%d for unit=%d\n",
			brdp->irq, brdp->brdnr);
		return;
	}

#if 0
	printf("%s(%d): ECH-PCI iobase=%x iopage=%x irq=%d\n", __file__,			 __LINE__, brdp->ioaddr2, brdp->ioaddr1, brdp->irq);
#endif
	stl_brdinit(brdp);
}

#endif

/*****************************************************************************/

STATIC int stlopen(dev_t dev, int flag, int mode, struct proc *p)
{
	struct tty	*tp;
	stlport_t	*portp;
	int		error, callout, x;

#if DEBUG
	printf("stlopen(dev=%x,flag=%x,mode=%x,p=%x)\n", (int) dev, flag,
		mode, (int) p);
#endif

/*
 *	Firstly check if the supplied device number is a valid device.
 */
	if (dev & STL_MEMDEV)
		return(0);

	portp = stl_dev2port(dev);
	if (portp == (stlport_t *) NULL)
		return(ENXIO);
	tp = &portp->tty;
	callout = minor(dev) & STL_CALLOUTDEV;
	error = 0;

	x = spltty();

stlopen_restart:
/*
 *	Wait here for the DTR drop timeout period to expire.
 */
	while (portp->state & ASY_DTRWAIT) {
		error = tsleep(&portp->dtrwait, (TTIPRI | PCATCH),
			"stldtr", 0);
		if (error)
			goto stlopen_end;
	}
	
/*
 *	We have a valid device, so now we check if it is already open.
 *	If not then initialize the port hardware and set up the tty
 *	struct as required.
 */
	if ((tp->t_state & TS_ISOPEN) == 0) {
		tp->t_oproc = stl_start;
		tp->t_param = stl_param;
		tp->t_dev = dev;
		tp->t_termios = callout ? portp->initouttios :
			portp->initintios;
		stl_rawopen(portp);
		ttsetwater(tp);
		if ((portp->sigs & TIOCM_CD) || callout)
			(*linesw[tp->t_line].l_modem)(tp, 1);
	} else {
		if (callout) {
			if (portp->callout == 0) {
				error = EBUSY;
				goto stlopen_end;
			}
		} else {
			if (portp->callout != 0) {
				if (flag & O_NONBLOCK) {
					error = EBUSY;
					goto stlopen_end;
				}
				error = tsleep(&portp->callout,
					(TTIPRI | PCATCH), "stlcall", 0);
				if (error)
					goto stlopen_end;
				goto stlopen_restart;
			}
		}
		if ((tp->t_state & TS_XCLUDE) && (p->p_ucred->cr_uid != 0)) {
			error = EBUSY;
			goto stlopen_end;
		}
	}

/*
 *	If this port is not the callout device and we do not have carrier
 *	then we need to sleep, waiting for it to be asserted.
 */
	if (((tp->t_state & TS_CARR_ON) == 0) && !callout &&
			((tp->t_cflag & CLOCAL) == 0) &&
			((flag & O_NONBLOCK) == 0)) {
		portp->waitopens++;
		error = tsleep(TSA_CARR_ON(tp), (TTIPRI | PCATCH), "stldcd", 0);
		portp->waitopens--;
		if (error)
			goto stlopen_end;
		goto stlopen_restart;
	}

/*
 *	Open the line discipline.
 */
	error = (*linesw[tp->t_line].l_open)(dev, tp);
	stl_ttyoptim(portp, &tp->t_termios);
	if ((tp->t_state & TS_ISOPEN) && callout)
		portp->callout = 1;

/*
 *	If for any reason we get to here and the port is not actually
 *	open then close of the physical hardware - no point leaving it
 *	active when the open failed...
 */
stlopen_end:
	splx(x);
	if (((tp->t_state & TS_ISOPEN) == 0) && (portp->waitopens == 0))
		stl_rawclose(portp);

	return(error);
}

/*****************************************************************************/

STATIC int stlclose(dev_t dev, int flag, int mode, struct proc *p)
{
	struct tty	*tp;
	stlport_t	*portp;
	int		x;

#if DEBUG
	printf("stlclose(dev=%x,flag=%x,mode=%x,p=%x)\n", dev, flag, mode, p);
#endif

	if (dev & STL_MEMDEV)
		return(0);

	portp = stl_dev2port(dev);
	if (portp == (stlport_t *) NULL)
		return(ENXIO);
	tp = &portp->tty;

	x = spltty();
	(*linesw[tp->t_line].l_close)(tp, flag);
	stl_ttyoptim(portp, &tp->t_termios);
	stl_rawclose(portp);
	ttyclose(tp);
	splx(x);
	return(0);
}

/*****************************************************************************/

STATIC int stlread(dev_t dev, struct uio *uiop, int flag)
{
	stlport_t	*portp;

#if DEBUG
	printf("stlread(dev=%x,uiop=%x,flag=%x)\n", dev, uiop, flag);
#endif

	portp = stl_dev2port(dev);
	if (portp == (stlport_t *) NULL)
		return(ENODEV);
	return((*linesw[portp->tty.t_line].l_read)(&portp->tty, uiop, flag));
}

/*****************************************************************************/

#if VFREEBSD >= 220

STATIC void stlstop(struct tty *tp, int rw)
{
#if DEBUG
	printf("stlstop(tp=%x,rw=%x)\n", (int) tp, rw);
#endif

	stl_flush((stlport_t *) tp, rw);
}

#else

STATIC int stlstop(struct tty *tp, int rw)
{
#if DEBUG
	printf("stlstop(tp=%x,rw=%x)\n", (int) tp, rw);
#endif

	stl_flush((stlport_t *) tp, rw);
	return(0);
}

#endif

/*****************************************************************************/

STATIC struct tty *stldevtotty(dev_t dev)
{
#if DEBUG
	printf("stldevtotty(dev=%x)\n", dev);
#endif
	return((struct tty *) stl_dev2port(dev));
}

/*****************************************************************************/

STATIC int stlwrite(dev_t dev, struct uio *uiop, int flag)
{
	stlport_t	*portp;

#if DEBUG
	printf("stlwrite(dev=%x,uiop=%x,flag=%x)\n", dev, uiop, flag);
#endif

	portp = stl_dev2port(dev);
	if (portp == (stlport_t *) NULL)
		return(ENODEV);
	return((*linesw[portp->tty.t_line].l_write)(&portp->tty, uiop, flag));
}

/*****************************************************************************/

STATIC int stlioctl(dev_t dev, int cmd, caddr_t data, int flag, struct proc *p)
{
	struct termios	*newtios, *localtios;
	struct tty	*tp;
	stlport_t	*portp;
	int		error, i, x;

#if DEBUG
	printf("stlioctl(dev=%x,cmd=%x,data=%x,flag=%x,p=%x)\n", dev, cmd,
		data, flag, p);
#endif

	dev = minor(dev);
	if (dev & STL_MEMDEV)
		return(stl_memioctl(dev, cmd, data, flag, p));

	portp = stl_dev2port(dev);
	if (portp == (stlport_t *) NULL)
		return(ENODEV);
	tp = &portp->tty;
	error = 0;
	
/*
 *	First up handle ioctls on the control devices.
 */
	if (dev & STL_CTRLDEV) {
		if ((dev & STL_CTRLDEV) == STL_CTRLINIT)
			localtios = (dev & STL_CALLOUTDEV) ?
				&portp->initouttios : &portp->initintios;
		else if ((dev & STL_CTRLDEV) == STL_CTRLLOCK)
			localtios = (dev & STL_CALLOUTDEV) ?
				&portp->lockouttios : &portp->lockintios;
		else
			return(ENODEV);

		switch (cmd) {
		case TIOCSETA:
			if ((error = suser(p->p_ucred, &p->p_acflag)) == 0)
				*localtios = *((struct termios *) data);
			break;
		case TIOCGETA:
			*((struct termios *) data) = *localtios;
			break;
		case TIOCGETD:
			*((int *) data) = TTYDISC;
			break;
		case TIOCGWINSZ:
			bzero(data, sizeof(struct winsize));
			break;
		default:
			error = ENOTTY;
			break;
		}
		return(error);
	}

/*
 *	Deal with 4.3 compatability issues if we have too...
 */
#if defined(COMPAT_43) || defined(COMPAT_SUNOS)
	if (1) {
		struct termios	tios;
		int		oldcmd;

		tios = tp->t_termios;
		oldcmd = cmd;
		if ((error = ttsetcompat(tp, &cmd, data, &tios)))
			return(error);
		if (cmd != oldcmd)
			data = (caddr_t) &tios;
	}
#endif

/*
 *	Carry out some pre-cmd processing work first...
 *	Hmmm, not so sure we want this, disable for now...
 */
	if ((cmd == TIOCSETA) || (cmd == TIOCSETAW) || (cmd == TIOCSETAF)) {
		newtios = (struct termios *) data;
		localtios = (dev & STL_CALLOUTDEV) ? &portp->lockouttios :
			 &portp->lockintios;

		newtios->c_iflag = (tp->t_iflag & localtios->c_iflag) |
			(newtios->c_iflag & ~localtios->c_iflag);
		newtios->c_oflag = (tp->t_oflag & localtios->c_oflag) |
			(newtios->c_oflag & ~localtios->c_oflag);
		newtios->c_cflag = (tp->t_cflag & localtios->c_cflag) |
			(newtios->c_cflag & ~localtios->c_cflag);
		newtios->c_lflag = (tp->t_lflag & localtios->c_lflag) |
			(newtios->c_lflag & ~localtios->c_lflag);
		for (i = 0; (i < NCCS); i++) {
			if (localtios->c_cc[i] != 0)
				newtios->c_cc[i] = tp->t_cc[i];
		}
		if (localtios->c_ispeed != 0)
			newtios->c_ispeed = tp->t_ispeed;
		if (localtios->c_ospeed != 0)
			newtios->c_ospeed = tp->t_ospeed;
	}

/*
 *	Call the line discipline and the common command processing to
 *	process this command (if they can).
 */
	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p);
	if (error >= 0)
		return(error);

	x = spltty();
	error = ttioctl(tp, cmd, data, flag);
	stl_ttyoptim(portp, &tp->t_termios);
	if (error >= 0) {
		splx(x);
		return(error);
	}

	error = 0;

/*
 *	Process local commands here. These are all commands that only we
 *	can take care of (they all rely on actually doing something special
 *	to the actual hardware).
 */
	switch (cmd) {
	case TIOCSBRK:
		stl_sendbreak(portp, -1);
		break;
	case TIOCCBRK:
		stl_sendbreak(portp, -2);
		break;
	case TIOCSDTR:
		stl_setsignals(portp, 1, -1);
		break;
	case TIOCCDTR:
		stl_setsignals(portp, 0, -1);
		break;
	case TIOCMSET:
		i = *((int *) data);
		stl_setsignals(portp, ((i & TIOCM_DTR) ? 1 : 0),
			((i & TIOCM_RTS) ? 1 : 0));
		break;
	case TIOCMBIS:
		i = *((int *) data);
		stl_setsignals(portp, ((i & TIOCM_DTR) ? 1 : -1),
			((i & TIOCM_RTS) ? 1 : -1));
		break;
	case TIOCMBIC:
		i = *((int *) data);
		stl_setsignals(portp, ((i & TIOCM_DTR) ? 0 : -1),
			((i & TIOCM_RTS) ? 0 : -1));
		break;
	case TIOCMGET:
		*((int *) data) = (stl_getsignals(portp) | TIOCM_LE);
		break;
	case TIOCMSDTRWAIT:
		if ((error = suser(p->p_ucred, &p->p_acflag)) == 0)
			portp->dtrwait = *((int *) data) * hz / 100;
		break;
	case TIOCMGDTRWAIT:
		*((int *) data) = portp->dtrwait * 100 / hz;
		break;
	case TIOCTIMESTAMP:
		portp->dotimestamp = 1;
		*((struct timeval *) data) = portp->timestamp;
		break;
	default:
		error = ENOTTY;
		break;
	}
	splx(x);

	return(error);
}

/*****************************************************************************/

/*
 *	Convert the specified minor device number into a port struct
 *	pointer. Return NULL if the device number is not a valid port.
 */

STATIC stlport_t *stl_dev2port(dev_t dev)
{
	stlbrd_t	*brdp;

	brdp = stl_brds[MKDEV2BRD(dev)];
	if (brdp == (stlbrd_t *) NULL)
		return((stlport_t *) NULL);
	return(brdp->ports[MKDEV2PORT(dev)]);
}

/*****************************************************************************/

/*
 *	Initialize the port hardware. This involves enabling the transmitter
 *	and receiver, setting the port configuration, and setting the initial
 *	signal state.
 */

static int stl_rawopen(stlport_t *portp)
{
#if DEBUG
	printf("stl_rawopen(portp=%x): brdnr=%d panelnr=%d portnr=%d\n",
		portp, portp->brdnr, portp->panelnr, portp->portnr);
#endif
	stl_param(&portp->tty, &portp->tty.t_termios);
	portp->sigs = stl_getsignals(portp);
	stl_setsignals(portp, 1, 1);
	stl_enablerxtx(portp, 1, 1);
	stl_startrxtx(portp, 1, 0);
	return(0);
}

/*****************************************************************************/

/*
 *	Shutdown the hardware of a port. Disable its transmitter and
 *	receiver, and maybe drop signals if appropriate.
 */

static int stl_rawclose(stlport_t *portp)
{
	struct tty	*tp;

#if DEBUG
	printf("stl_rawclose(portp=%x): brdnr=%d panelnr=%d portnr=%d\n",
		portp, portp->brdnr, portp->panelnr, portp->portnr);
#endif

	tp = &portp->tty;
	stl_disableintrs(portp);
	stl_enablerxtx(portp, 0, 0);
	stl_flush(portp, (FWRITE | FREAD));
	if (tp->t_cflag & HUPCL) {
		stl_setsignals(portp, 0, 0);
		if (portp->dtrwait != 0) {
			portp->state |= ASY_DTRWAIT;
			timeout(stl_dtrwakeup, portp, portp->dtrwait);
		}
	}
	portp->callout = 0;
	portp->brklen = 0;
	portp->state &= ~(ASY_ACTIVE | ASY_RTSFLOW);
	wakeup(&portp->callout);
	wakeup(TSA_CARR_ON(tp));
	return(0);
}

/*****************************************************************************/

/*
 *	Clear the DTR waiting flag, and wake up any sleepers waiting for
 *	DTR wait period to finish.
 */

static void stl_dtrwakeup(void *arg)
{
	stlport_t	*portp;

	portp = (stlport_t *) arg;
	portp->state &= ~ASY_DTRWAIT;
	wakeup(&portp->dtrwait);
}

/*****************************************************************************/

/*
 *	Start (or continue) the transfer of TX data on this port. If the
 *	port is not currently busy then load up the interrupt ring queue
 *	buffer and kick of the transmitter. If the port is running low on
 *	TX data then refill the ring queue. This routine is also used to
 *	activate input flow control!
 */

static void stl_start(struct tty *tp)
{
	stlport_t	*portp;
	unsigned int	len, stlen;
	char		*head, *tail;
	int		count, x;

	portp = (stlport_t *) tp;

#if DEBUG
	printf("stl_start(tp=%x): brdnr=%d portnr=%d\n", (int) tp, 
		portp->brdnr, portp->portnr);
#endif

	x = spltty();

/*
 *	Check if the ports input has been blocked, and take appropriate action.
 *	Not very often do we really need to do anything, so make it quick.
 */
	if (tp->t_state & TS_TBLOCK) {
		if ((portp->state & ASY_RTSFLOW) == 0)
			stl_flowcontrol(portp, 0, -1);
	} else {
		if (portp->state & ASY_RTSFLOW)
			stl_flowcontrol(portp, 1, -1);
	}

#if VFREEBSD == 205
/*
 *	Check if the output cooked clist buffers are near empty, wake up
 *	the line discipline to fill it up.
 */
	if (tp->t_outq.c_cc <= tp->t_lowat) {
		if (tp->t_state & TS_ASLEEP) {
			tp->t_state &= ~TS_ASLEEP;
			wakeup(&tp->t_outq);
		}
		selwakeup(&tp->t_wsel);
	}
#endif

	if (tp->t_state & (TS_TIMEOUT | TS_TTSTOP)) {
		splx(x);
		return;
	}

/*
 *	Copy data from the clists into the interrupt ring queue. This will
 *	require at most 2 copys... What we do is calculate how many chars
 *	can fit into the ring queue, and how many can fit in 1 copy. If after
 *	the first copy there is still more room then do the second copy. 
 *	The beauty of this type of ring queue is that we do not need to
 *	spl protect our-selves, since we only ever update the head pointer,
 *	and the interrupt routine only ever updates the tail pointer.
 */
	if (tp->t_outq.c_cc != 0) {
		head = portp->tx.head;
		tail = portp->tx.tail;
		if (head >= tail) {
			len = STL_TXBUFSIZE - (head - tail) - 1;
			stlen = portp->tx.endbuf - head;
		} else {
			len = tail - head - 1;
			stlen = len;
		}

		if (len > 0) {
			stlen = MIN(len, stlen);
			count = q_to_b(&tp->t_outq, head, stlen);
			len -= count;
			head += count;
			if (head >= portp->tx.endbuf) {
				head = portp->tx.buf;
				if (len > 0) {
					stlen = q_to_b(&tp->t_outq, head, len);
					head += stlen;
					count += stlen;
				}
			}
			portp->tx.head = head;
			if (count > 0)
				stl_startrxtx(portp, -1, 1);
		}

/*
 *		If we sent something, make sure we are called again.
 */
		tp->t_state |= TS_BUSY;
	}

#if VFREEBSD != 205
/*
 *	Do any writer wakeups.
 */
	ttwwakeup(tp);
#endif

	splx(x);
}

/*****************************************************************************/

static void stl_flush(stlport_t *portp, int flag)
{
	char	*head, *tail;
	int	len, x;

#if DEBUG
	printf("stl_flush(portp=%x,flag=%x)\n", (int) portp, flag);
#endif

	if (portp == (stlport_t *) NULL)
		return;

	x = spltty();

	if (flag & FWRITE) {
		BRDENABLE(portp->brdnr, portp->pagenr);
		stl_setreg(portp, CAR, (portp->portnr & 0x03));
		stl_ccrwait(portp);
		stl_setreg(portp, CCR, CCR_TXFLUSHFIFO);
		stl_ccrwait(portp);
		portp->tx.tail = portp->tx.head;
		BRDDISABLE(portp->brdnr);
	}

/*
 *	The only thing to watch out for when flushing the read side is
 *	the RX status buffer. The interrupt code relys on the status
 *	bytes as being zeroed all the time (it does not bother setting
 *	a good char status to 0, it expects that it already will be).
 *	We also need to un-flow the RX channel if flow control was
 *	active.
 */
	if (flag & FREAD) {
		head = portp->rx.head;
		tail = portp->rx.tail;
		if (head != tail) {
			if (head >= tail) {
				len = head - tail;
			} else {
				len = portp->rx.endbuf - tail;
				bzero(portp->rxstatus.buf,
					(head - portp->rx.buf));
			}
			bzero((tail + STL_RXBUFSIZE), len);
			portp->rx.tail = head;
		}

		if ((portp->state & ASY_RTSFLOW) &&
				((portp->tty.t_state & TS_TBLOCK) == 0))
			stl_flowcontrol(portp, 1, -1);
	}

	splx(x);
}

/*****************************************************************************/

/*
 *	These functions get/set/update the registers of the cd1400 UARTs.
 *	Access to the cd1400 registers is via an address/data io port pair.
 *	(Maybe should make this inline...)
 */

static int stl_getreg(stlport_t *portp, int regnr)
{
	outb(portp->ioaddr, (regnr + portp->uartaddr));
	return(inb(portp->ioaddr + EREG_DATA));
}

/*****************************************************************************/

static void stl_setreg(stlport_t *portp, int regnr, int value)
{
	outb(portp->ioaddr, (regnr + portp->uartaddr));
	outb((portp->ioaddr + EREG_DATA), value);
}

/*****************************************************************************/

static int stl_updatereg(stlport_t *portp, int regnr, int value)
{
	outb(portp->ioaddr, (regnr + portp->uartaddr));
	if (inb(portp->ioaddr + EREG_DATA) != value) {
		outb((portp->ioaddr + EREG_DATA), value);
		return(1);
	}
	return(0);
}

/*****************************************************************************/

/*
 *	Wait for the command register to be ready. We will poll this, since
 *	it won't usually take too long to be ready, and it is only really
 *	used for non-critical actions.
 */

static void stl_ccrwait(stlport_t *portp)
{
	int	i;

	for (i = 0; (i < CCR_MAXWAIT); i++) {
		if (stl_getreg(portp, CCR) == 0) {
			return;
		}
	}

	printf("STALLION: cd1400 device not responding, brd=%d panel=%d"
		"port=%d\n", portp->brdnr, portp->panelnr, portp->portnr);
}

/*****************************************************************************/

/*
 *	Transmit interrupt handler. This has gotta be fast!  Handling TX
 *	chars is pretty simple, stuff as many as possible from the TX buffer
 *	into the cd1400 FIFO. Must also handle TX breaks here, since they
 *	are embedded as commands in the data stream. Oh no, had to use a goto!
 *	This could be optimized more, will do when I get time...
 *	In practice it is possible that interrupts are enabled but that the
 *	port has been hung up. Need to handle not having any TX buffer here,
 *	this is done by using the side effect that head and tail will also
 *	be NULL if the buffer has been freed.
 */

static inline void stl_txisr(stlpanel_t *panelp, int ioaddr)
{
	stlport_t	*portp;
	int		len, stlen;
	char		*head, *tail;
	unsigned char	ioack, srer;

#if DEBUG
	printf("stl_txisr(panelp=%x,ioaddr=%x)\n", (int) panelp, ioaddr);
#endif

	ioack = inb(ioaddr + EREG_TXACK);
	if (((ioack & panelp->ackmask) != 0) ||
			((ioack & ACK_TYPMASK) != ACK_TYPTX)) {
		printf("STALLION: bad TX interrupt ack value=%x\n", ioack);
		return;
	}
	portp = panelp->ports[(ioack >> 3)];

/*
 *	Unfortunately we need to handle breaks in the data stream, since
 *	this is the only way to generate them on the cd1400. Do it now if
 *	a break is to be sent. Some special cases here: brklen is -1 then
 *	start sending an un-timed break, if brklen is -2 then stop sending
 *	an un-timed break, if brklen is -3 then we have just sent an
 *	un-timed break and do not want any data to go out, if brklen is -4
 *	then a break has just completed so clean up the port settings.
 */
	if (portp->brklen != 0) {
		if (portp->brklen >= -1) {
			outb(ioaddr, (TDR + portp->uartaddr));
			outb((ioaddr + EREG_DATA), ETC_CMD);
			outb((ioaddr + EREG_DATA), ETC_STARTBREAK);
			if (portp->brklen > 0) {
				outb((ioaddr + EREG_DATA), ETC_CMD);
				outb((ioaddr + EREG_DATA), ETC_DELAY);
				outb((ioaddr + EREG_DATA), portp->brklen);
				outb((ioaddr + EREG_DATA), ETC_CMD);
				outb((ioaddr + EREG_DATA), ETC_STOPBREAK);
				portp->brklen = -4;
			} else {
				portp->brklen = -3;
			}
		} else if (portp->brklen == -2) {
			outb(ioaddr, (TDR + portp->uartaddr));
			outb((ioaddr + EREG_DATA), ETC_CMD);
			outb((ioaddr + EREG_DATA), ETC_STOPBREAK);
			portp->brklen = -4;
		} else if (portp->brklen == -3) {
			outb(ioaddr, (SRER + portp->uartaddr));
			srer = inb(ioaddr + EREG_DATA);
			srer &= ~(SRER_TXDATA | SRER_TXEMPTY);
			outb((ioaddr + EREG_DATA), srer);
		} else {
			outb(ioaddr, (COR2 + portp->uartaddr));
			outb((ioaddr + EREG_DATA),
				(inb(ioaddr + EREG_DATA) & ~COR2_ETC));
			portp->brklen = 0;
		}
		goto stl_txalldone;
	}

	head = portp->tx.head;
	tail = portp->tx.tail;
	len = (head >= tail) ? (head - tail) : (STL_TXBUFSIZE - (tail - head));
	if ((len == 0) || ((len < STL_TXBUFLOW) &&
			((portp->state & ASY_TXLOW) == 0))) {
		portp->state |= ASY_TXLOW;
		stl_dotimeout();
	}

	if (len == 0) {
		outb(ioaddr, (SRER + portp->uartaddr));
		srer = inb(ioaddr + EREG_DATA);
		if (srer & SRER_TXDATA) {
			srer = (srer & ~SRER_TXDATA) | SRER_TXEMPTY;
		} else {
			srer &= ~(SRER_TXDATA | SRER_TXEMPTY);
			portp->tty.t_state &= ~TS_BUSY;
		}
		outb((ioaddr + EREG_DATA), srer);
	} else {
		len = MIN(len, CD1400_TXFIFOSIZE);
		portp->stats.txtotal += len;
		stlen = MIN(len, (portp->tx.endbuf - tail));
		outb(ioaddr, (TDR + portp->uartaddr));
		outsb((ioaddr + EREG_DATA), tail, stlen);
		len -= stlen;
		tail += stlen;
		if (tail >= portp->tx.endbuf)
			tail = portp->tx.buf;
		if (len > 0) {
			outsb((ioaddr + EREG_DATA), tail, len);
			tail += len;
		}
		portp->tx.tail = tail;
	}

stl_txalldone:
	outb(ioaddr, (EOSRR + portp->uartaddr));
	outb((ioaddr + EREG_DATA), 0);
}

/*****************************************************************************/

/*
 *	Receive character interrupt handler. Determine if we have good chars
 *	or bad chars and then process appropriately. Good chars are easy
 *	just shove the lot into the RX buffer and set all status bytes to 0.
 *	If a bad RX char then process as required. This routine needs to be
 *	fast!
 */

static inline void stl_rxisr(stlpanel_t *panelp, int ioaddr)
{
	stlport_t	*portp;
	struct tty	*tp;
	unsigned int	ioack, len, buflen, stlen;
	unsigned char	status;
	char		ch;
	char		*head, *tail;
	static char	unwanted[CD1400_RXFIFOSIZE];

#if DEBUG
	printf("stl_rxisr(panelp=%x,ioaddr=%x)\n", (int) panelp, ioaddr);
#endif

	ioack = inb(ioaddr + EREG_RXACK);
	if ((ioack & panelp->ackmask) != 0) {
		printf("STALLION: bad RX interrupt ack value=%x\n", ioack);
		return;
	}
	portp = panelp->ports[(ioack >> 3)];
	tp = &portp->tty;

/*
 *	First up, caluclate how much room there is in the RX ring queue.
 *	We also want to keep track of the longest possible copy length,
 *	this has to allow for the wrapping of the ring queue.
 */
	head = portp->rx.head;
	tail = portp->rx.tail;
	if (head >= tail) {
		buflen = STL_RXBUFSIZE - (head - tail) - 1;
		stlen = portp->rx.endbuf - head;
	} else {
		buflen = tail - head - 1;
		stlen = buflen;
	}

/*
 *	Check if the input buffer is near full. If so then we should take
 *	some flow control action... It is very easy to do hardware and
 *	software flow control from here since we have the port selected on
 *	the UART.
 */
	if (buflen <= (STL_RXBUFSIZE - STL_RXBUFHIGH)) {
		if (((portp->state & ASY_RTSFLOW) == 0) &&
				(portp->state & ASY_RTSFLOWMODE)) {
			portp->state |= ASY_RTSFLOW;
			stl_setreg(portp, MCOR1,
				(stl_getreg(portp, MCOR1) & 0xf0));
			stl_setreg(portp, MSVR2, 0);
			portp->stats.rxrtsoff++;
		}
	}

/*
 *	OK we are set, process good data... If the RX ring queue is full
 *	just chuck the chars - don't leave them in the UART.
 */
	if ((ioack & ACK_TYPMASK) == ACK_TYPRXGOOD) {
		outb(ioaddr, (RDCR + portp->uartaddr));
		len = inb(ioaddr + EREG_DATA);
		if (buflen == 0) {
			outb(ioaddr, (RDSR + portp->uartaddr));
			insb((ioaddr + EREG_DATA), &unwanted[0], len);
			portp->stats.rxlost += len;
			portp->stats.rxtotal += len;
		} else {
			len = MIN(len, buflen);
			portp->stats.rxtotal += len;
			stlen = MIN(len, stlen);
			if (len > 0) {
				outb(ioaddr, (RDSR + portp->uartaddr));
				insb((ioaddr + EREG_DATA), head, stlen);
				head += stlen;
				if (head >= portp->rx.endbuf) {
					head = portp->rx.buf;
					len -= stlen;
					insb((ioaddr + EREG_DATA), head, len);
					head += len;
				}
			}
		}
	} else if ((ioack & ACK_TYPMASK) == ACK_TYPRXBAD) {
		outb(ioaddr, (RDSR + portp->uartaddr));
		status = inb(ioaddr + EREG_DATA);
		ch = inb(ioaddr + EREG_DATA);
		if (status & ST_BREAK)
			portp->stats.rxbreaks++;
		if (status & ST_FRAMING)
			portp->stats.rxframing++;
		if (status & ST_PARITY)
			portp->stats.rxparity++;
		if (status & ST_OVERRUN)
			portp->stats.rxoverrun++;
		if (status & ST_SCHARMASK) {
			if ((status & ST_SCHARMASK) == ST_SCHAR1)
				portp->stats.txxon++;
			if ((status & ST_SCHARMASK) == ST_SCHAR2)
				portp->stats.txxoff++;
			goto stl_rxalldone;
		}
		if ((portp->rxignoremsk & status) == 0) {
			if ((tp->t_state & TS_CAN_BYPASS_L_RINT) &&
			    ((status & ST_FRAMING) ||
			    ((status & ST_PARITY) && (tp->t_iflag & INPCK))))
				ch = 0;
			if ((portp->rxmarkmsk & status) == 0)
				status = 0;
			*(head + STL_RXBUFSIZE) = status;
			*head++ = ch;
			if (head >= portp->rx.endbuf)
				head = portp->rx.buf;
		}
	} else {
		printf("STALLION: bad RX interrupt ack value=%x\n", ioack);
		return;
	}

	portp->rx.head = head;
	portp->state |= ASY_RXDATA;
	stl_dotimeout();

stl_rxalldone:
	outb(ioaddr, (EOSRR + portp->uartaddr));
	outb((ioaddr + EREG_DATA), 0);
}

/*****************************************************************************/

/*
 *	Modem interrupt handler. The is called when the modem signal line
 *	(DCD) has changed state. Leave most of the work to the off-level
 *	processing routine.
 */

static inline void stl_mdmisr(stlpanel_t *panelp, int ioaddr)
{
	stlport_t	*portp;
	unsigned int	ioack;
	unsigned char	misr;

#if DEBUG
	printf("stl_mdmisr(panelp=%x,ioaddr=%x)\n", (int) panelp, ioaddr);
#endif

	ioack = inb(ioaddr + EREG_MDACK);
	if (((ioack & panelp->ackmask) != 0) ||
			((ioack & ACK_TYPMASK) != ACK_TYPMDM)) {
		printf("STALLION: bad MODEM interrupt ack value=%x\n", ioack);
		return;
	}
	portp = panelp->ports[(ioack >> 3)];

	outb(ioaddr, (MISR + portp->uartaddr));
	misr = inb(ioaddr + EREG_DATA);
	if (misr & MISR_DCD) {
		portp->state |= ASY_DCDCHANGE;
		portp->stats.modem++;
		stl_dotimeout();
	}

	outb(ioaddr, (EOSRR + portp->uartaddr));
	outb((ioaddr + EREG_DATA), 0);
}

/*****************************************************************************/

/*
 *	Interrupt handler for EIO and ECH boards. This code ain't all that
 *	pretty, but the idea is to make it as fast as possible. This code is
 *	well suited to be assemblerized :-)  We don't use the general purpose
 *	register access functions here, for speed we will go strait to the
 *	io register.
 */

void stlintr(int unit)
{
	stlbrd_t	*brdp;
	stlpanel_t	*panelp;
	unsigned char	svrtype;
	int		i, panelnr, iobase;
	int		cnt;

#if DEBUG
	printf("stlintr(unit=%d)\n", unit);
#endif

	cnt = 0;
	panelp = (stlpanel_t *) NULL;
	for (i = 0; (i < stl_nrbrds); ) {
		if ((brdp = stl_brds[i]) == (stlbrd_t *) NULL) {
			i++;
			continue;
		}
		if (brdp->state == 0) {
			i++;
			continue;
		}
/*
 *		The following section of code handles the subtle differences
 *		between board types. It is sort of similar, but different
 *		enough to handle each separately.
 */
		if (brdp->brdtype == BRD_EASYIO) {
			if ((inb(brdp->iostatus) & EIO_INTRPEND) == 0) {
				i++;
				continue;
			}
			panelp = brdp->panels[0];
			iobase = panelp->iobase;
			outb(iobase, SVRR);
			svrtype = inb(iobase + EREG_DATA);
			if (brdp->nrports > 4) {
				outb(iobase, (SVRR + 0x80));
				svrtype |= inb(iobase + EREG_DATA);
			}
		} else if (brdp->brdtype == BRD_ECH) {
			if ((inb(brdp->iostatus) & ECH_INTRPEND) == 0) {
				i++;
				continue;
			}
			outb(brdp->ioctrl, (brdp->ioctrlval | ECH_BRDENABLE));
			for (panelnr = 0; (panelnr < brdp->nrpanels); panelnr++) {
				panelp = brdp->panels[panelnr];
				iobase = panelp->iobase;
				if (inb(iobase + ECH_PNLSTATUS) & ECH_PNLINTRPEND)
					break;
				if (panelp->nrports > 8) {
					iobase += 0x8;
					if (inb(iobase + ECH_PNLSTATUS) & ECH_PNLINTRPEND)
						break;
				}
			}	
			if (panelnr >= brdp->nrpanels) {
				i++;
				continue;
			}
			outb(iobase, SVRR);
			svrtype = inb(iobase + EREG_DATA);
			outb(iobase, (SVRR + 0x80));
			svrtype |= inb(iobase + EREG_DATA);
		} else if (brdp->brdtype == BRD_ECHPCI) {
			iobase = brdp->ioaddr2;
			for (panelnr = 0; (panelnr < brdp->nrpanels); panelnr++) {
				panelp = brdp->panels[panelnr];
				outb(brdp->ioctrl, panelp->pagenr);
				if (inb(iobase + ECH_PNLSTATUS) & ECH_PNLINTRPEND)
					break;
				if (panelp->nrports > 8) {
					outb(brdp->ioctrl, (panelp->pagenr + 1));
					if (inb(iobase + ECH_PNLSTATUS) & ECH_PNLINTRPEND)
						break;
				}
			}	
			if (panelnr >= brdp->nrpanels) {
				i++;
				continue;
			}
			outb(iobase, SVRR);
			svrtype = inb(iobase + EREG_DATA);
			outb(iobase, (SVRR + 0x80));
			svrtype |= inb(iobase + EREG_DATA);
		} else if (brdp->brdtype == BRD_ECHMC) {
			if ((inb(brdp->iostatus) & ECH_INTRPEND) == 0) {
				i++;
				continue;
			}
			for (panelnr = 0; (panelnr < brdp->nrpanels); panelnr++) {
				panelp = brdp->panels[panelnr];
				iobase = panelp->iobase;
				if (inb(iobase + ECH_PNLSTATUS) & ECH_PNLINTRPEND)
					break;
				if (panelp->nrports > 8) {
					iobase += 0x8;
					if (inb(iobase + ECH_PNLSTATUS) & ECH_PNLINTRPEND)
						break;
				}
			}	
			if (panelnr >= brdp->nrpanels) {
				i++;
				continue;
			}
			outb(iobase, SVRR);
			svrtype = inb(iobase + EREG_DATA);
			outb(iobase, (SVRR + 0x80));
			svrtype |= inb(iobase + EREG_DATA);
		} else {
			printf("STALLION: unknown board type=%x\n", brdp->brdtype);
			i++;
			continue;
		}

/*
 *		We have determined what type of service is required for a
 *		port. From here on in the service of a port is the same no
 *		matter what the board type...
 */
		if (svrtype & SVRR_RX)
			stl_rxisr(panelp, iobase);
		if (svrtype & SVRR_TX)
			stl_txisr(panelp, iobase);
		if (svrtype & SVRR_MDM)
			stl_mdmisr(panelp, iobase);

		if (brdp->brdtype == BRD_ECH)
			outb(brdp->ioctrl, (brdp->ioctrlval | ECH_BRDDISABLE));
	}
}

/*****************************************************************************/

#if NPCI > 0

static void stlpciintr(void *arg)
{
	stlintr(0);
}

#endif

/*****************************************************************************/

/*
 *	If we haven't scheduled a timeout then do it, some port needs high
 *	level processing.
 */

static void stl_dotimeout()
{
#if DEBUG
	printf("stl_dotimeout()\n");
#endif

	if (stl_doingtimeout == 0) {
		timeout(stl_poll, 0, 1);
		stl_doingtimeout++;
	}
}

/*****************************************************************************/

/*
 *	Service "software" level processing. Too slow or painfull to be done
 *	at real hardware interrupt time. This way we might also be able to
 *	do some service on other waiting ports as well...
 */

static void stl_poll(void *arg)
{
	stlbrd_t	*brdp;
	stlport_t	*portp;
	struct tty	*tp;
	int		brdnr, portnr, rearm, x;

#if DEBUG
	printf("stl_poll()\n");
#endif

	stl_doingtimeout = 0;
	rearm = 0;

	x = spltty();
	for (brdnr = 0; (brdnr < stl_nrbrds); brdnr++) {
		if ((brdp = stl_brds[brdnr]) == (stlbrd_t *) NULL)
			continue;
		for (portnr = 0; (portnr < brdp->nrports); portnr++) {
			if ((portp = brdp->ports[portnr]) == (stlport_t *) NULL)
				continue;
			if ((portp->state & ASY_ACTIVE) == 0)
				continue;
			tp = &portp->tty;

			if (portp->state & ASY_RXDATA)
				stl_rxprocess(portp);
			if (portp->state & ASY_DCDCHANGE) {
				portp->state &= ~ASY_DCDCHANGE;
				portp->sigs = stl_getsignals(portp);
				(*linesw[tp->t_line].l_modem)(tp,
					(portp->sigs & TIOCM_CD));
			}
			if (portp->state & ASY_TXLOW) {
				portp->state &= ~ASY_TXLOW;
				(*linesw[tp->t_line].l_start)(tp);
			}

			if (portp->state & ASY_ACTIVE)
				rearm++;
		}
	}
	splx(x);

	if (rearm)
		stl_dotimeout();
}

/*****************************************************************************/

/*
 *	Process the RX data that has been buffered up in the RX ring queue.
 */

static void stl_rxprocess(stlport_t *portp)
{
	struct tty	*tp;
	unsigned int	len, stlen, lostlen;
	char		*head, *tail;
	char		status;
	int		ch;

#if DEBUG
	printf("stl_rxprocess(portp=%x): brdnr=%d portnr=%d\n", (int) portp, 
		portp->brdnr, portp->portnr);
#endif

	tp = &portp->tty;
	portp->state &= ~ASY_RXDATA;

	if ((tp->t_state & TS_ISOPEN) == 0) {
		stl_flush(portp, FREAD);
		return;
	}

/*
 *	Calculate the amount of data in the RX ring queue. Also calculate
 *	the largest single copy size...
 */
	head = portp->rx.head;
	tail = portp->rx.tail;
	if (head >= tail) {
		len = head - tail;
		stlen = len;
	} else {
		len = STL_RXBUFSIZE - (tail - head);
		stlen = portp->rx.endbuf - tail;
	}

	if (tp->t_state & TS_CAN_BYPASS_L_RINT) {
		if (len > 0) {
			if (((tp->t_rawq.c_cc + len) >= TTYHOG) &&
					((portp->state & ASY_RTSFLOWMODE) ||
					(tp->t_iflag & IXOFF)) &&
					((tp->t_state & TS_TBLOCK) == 0)) {
				ch = TTYHOG - tp->t_rawq.c_cc - 1;
				len = (ch > 0) ? ch : 0;
				stlen = MIN(stlen, len);
				ttyblock(tp);
			}
			lostlen = b_to_q(tail, stlen, &tp->t_rawq);
			tail += stlen;
			len -= stlen;
			if (tail >= portp->rx.endbuf) {
				tail = portp->rx.buf;
				lostlen += b_to_q(tail, len, &tp->t_rawq);
				tail += len;
			}
			portp->stats.rxlost += lostlen;
			ttwakeup(tp);
			portp->rx.tail = tail;
		}
	} else {
		while (portp->rx.tail != head) {
			ch = (unsigned char) *(portp->rx.tail);
			if (status = *(portp->rx.tail + STL_RXBUFSIZE)) {
				*(portp->rx.tail + STL_RXBUFSIZE) = 0;
				if (status & ST_BREAK)
					ch |= TTY_BI;
				if (status & ST_FRAMING)
					ch |= TTY_FE;
				if (status & ST_PARITY)
					ch |= TTY_PE;
				if (status & ST_OVERRUN)
					ch |= TTY_OE;
			}
			(*linesw[tp->t_line].l_rint)(ch, tp);
			if (portp->rx.tail == head)
				break;

			if (++(portp->rx.tail) >= portp->rx.endbuf)
				portp->rx.tail = portp->rx.buf;
		}
	}

	if (head != portp->rx.tail)
		portp->state |= ASY_RXDATA;

/*
 *	If we where flow controled then maybe the buffer is low enough that
 *	we can re-activate it.
 */
	if ((portp->state & ASY_RTSFLOW) && ((tp->t_state & TS_TBLOCK) == 0))
		stl_flowcontrol(portp, 1, -1);
}

/*****************************************************************************/

/*
 *	Set up the cd1400 registers for a port based on the termios port
 *	settings.
 */

static int stl_param(struct tty *tp, struct termios *tiosp)
{
	stlport_t	*portp;
	unsigned int	clkdiv;
	unsigned char	cor1, cor2, cor3;
	unsigned char	cor4, cor5, ccr;
	unsigned char	srer, sreron, sreroff;
	unsigned char	mcor1, mcor2, rtpr;
	unsigned char	clk, div;
	int		x;

	portp = (stlport_t *) tp;

#if DEBUG
	printf("stl_param(tp=%x,tiosp=%x): brdnr=%d portnr=%d\n", (int) tp, 
		(int) tiosp, portp->brdnr, portp->portnr);
#endif

	cor1 = 0;
	cor2 = 0;
	cor3 = 0;
	cor4 = 0;
	cor5 = 0;
	ccr = 0;
	rtpr = 0;
	clk = 0;
	div = 0;
	mcor1 = 0;
	mcor2 = 0;
	sreron = 0;
	sreroff = 0;

/*
 *	Set up the RX char ignore mask with those RX error types we
 *	can ignore. We could have used some special modes of the cd1400
 *	UART to help, but it is better this way because we can keep stats
 *	on the number of each type of RX exception event.
 */
	portp->rxignoremsk = 0;
	if (tiosp->c_iflag & IGNPAR)
		portp->rxignoremsk |= (ST_PARITY | ST_FRAMING | ST_OVERRUN);
	if (tiosp->c_iflag & IGNBRK)
		portp->rxignoremsk |= ST_BREAK;

	portp->rxmarkmsk = ST_OVERRUN;
	if (tiosp->c_iflag & (INPCK | PARMRK))
		portp->rxmarkmsk |= (ST_PARITY | ST_FRAMING);
	if (tiosp->c_iflag & BRKINT)
		portp->rxmarkmsk |= ST_BREAK;

/*
 *	Go through the char size, parity and stop bits and set all the
 *	option registers appropriately.
 */
	switch (tiosp->c_cflag & CSIZE) {
	case CS5:
		cor1 |= COR1_CHL5;
		break;
	case CS6:
		cor1 |= COR1_CHL6;
		break;
	case CS7:
		cor1 |= COR1_CHL7;
		break;
	default:
		cor1 |= COR1_CHL8;
		break;
	}

	if (tiosp->c_cflag & CSTOPB)
		cor1 |= COR1_STOP2;
	else
		cor1 |= COR1_STOP1;

	if (tiosp->c_cflag & PARENB) {
		if (tiosp->c_cflag & PARODD)
			cor1 |= (COR1_PARENB | COR1_PARODD);
		else
			cor1 |= (COR1_PARENB | COR1_PAREVEN);
	} else {
		cor1 |= COR1_PARNONE;
	}

	if (tiosp->c_iflag & ISTRIP)
		cor5 |= COR5_ISTRIP;

/*
 *	Set the RX FIFO threshold at 6 chars. This gives a bit of breathing
 *	space for hardware flow control and the like. This should be set to
 *	VMIN. Also here we will set the RX data timeout to 10ms - this should
 *	really be based on VTIME...
 */
	cor3 |= FIFO_RXTHRESHOLD;
	rtpr = 2;

/*
 *	Calculate the baud rate timers. For now we will just assume that
 *	the input and output baud are the same. Could have used a baud
 *	table here, but this way we can generate virtually any baud rate
 *	we like!
 */
	if (tiosp->c_ispeed == 0)
		tiosp->c_ispeed = tiosp->c_ospeed;
	if ((tiosp->c_ospeed < 0) || (tiosp->c_ospeed > STL_MAXBAUD))
		return(EINVAL);

	if (tiosp->c_ospeed > 0) {
		for (clk = 0; (clk < CD1400_NUMCLKS); clk++) {
			clkdiv = ((portp->clk / stl_cd1400clkdivs[clk]) /
				tiosp->c_ospeed);
			if (clkdiv < 0x100)
				break;
		}
		div = (unsigned char) clkdiv;
	}

/*
 *	Check what form of modem signaling is required and set it up.
 */
	if ((tiosp->c_cflag & CLOCAL) == 0) {
		mcor1 |= MCOR1_DCD;
		mcor2 |= MCOR2_DCD;
		sreron |= SRER_MODEM;
	}

/*
 *	Setup cd1400 enhanced modes if we can. In particular we want to
 *	handle as much of the flow control as possbile automatically. As
 *	well as saving a few CPU cycles it will also greatly improve flow
 *	control reliablilty.
 */
	if (tiosp->c_iflag & IXON) {
		cor2 |= COR2_TXIBE;
		cor3 |= COR3_SCD12;
		if (tiosp->c_iflag & IXANY)
			cor2 |= COR2_IXM;
	}

	if (tiosp->c_cflag & CCTS_OFLOW)
		cor2 |= COR2_CTSAE;
	if (tiosp->c_cflag & CRTS_IFLOW)
		mcor1 |= FIFO_RTSTHRESHOLD;

/*
 *	All cd1400 register values calculated so go through and set them
 *	all up.
 */
#if DEBUG
	printf("SETPORT: portnr=%d panelnr=%d brdnr=%d\n", portp->portnr,
		portp->panelnr, portp->brdnr);
	printf("    cor1=%x cor2=%x cor3=%x cor4=%x cor5=%x\n", cor1, cor2,
		cor3, cor4, cor5);
	printf("    mcor1=%x mcor2=%x rtpr=%x sreron=%x sreroff=%x\n",
		mcor1, mcor2, rtpr, sreron, sreroff);
	printf("    tcor=%x tbpr=%x rcor=%x rbpr=%x\n", clk, div, clk, div);
	printf("    schr1=%x schr2=%x schr3=%x schr4=%x\n",
		tiosp->c_cc[VSTART], tiosp->c_cc[VSTOP], tiosp->c_cc[VSTART],
		tiosp->c_cc[VSTOP]);
#endif

	x = spltty();
	BRDENABLE(portp->brdnr, portp->pagenr);
	stl_setreg(portp, CAR, (portp->portnr & 0x3));
	srer = stl_getreg(portp, SRER);
	stl_setreg(portp, SRER, 0);
	ccr += stl_updatereg(portp, COR1, cor1);
	ccr += stl_updatereg(portp, COR2, cor2);
	ccr += stl_updatereg(portp, COR3, cor3);
	if (ccr) {
		stl_ccrwait(portp);
		stl_setreg(portp, CCR, CCR_CORCHANGE);
	}
	stl_setreg(portp, COR4, cor4);
	stl_setreg(portp, COR5, cor5);
	stl_setreg(portp, MCOR1, mcor1);
	stl_setreg(portp, MCOR2, mcor2);
	if (tiosp->c_ospeed == 0) {
		stl_setreg(portp, MSVR1, 0);
	} else {
		stl_setreg(portp, MSVR1, MSVR1_DTR);
		stl_setreg(portp, TCOR, clk);
		stl_setreg(portp, TBPR, div);
		stl_setreg(portp, RCOR, clk);
		stl_setreg(portp, RBPR, div);
	}
	stl_setreg(portp, SCHR1, tiosp->c_cc[VSTART]);
	stl_setreg(portp, SCHR2, tiosp->c_cc[VSTOP]);
	stl_setreg(portp, SCHR3, tiosp->c_cc[VSTART]);
	stl_setreg(portp, SCHR4, tiosp->c_cc[VSTOP]);
	stl_setreg(portp, RTPR, rtpr);
	mcor1 = stl_getreg(portp, MSVR1);
	if (mcor1 & MSVR1_DCD)
		portp->sigs |= TIOCM_CD;
	else
		portp->sigs &= ~TIOCM_CD;
	stl_setreg(portp, SRER, ((srer & ~sreroff) | sreron));
	BRDDISABLE(portp->brdnr);
	portp->state &= ~(ASY_RTSFLOWMODE | ASY_CTSFLOWMODE);
	portp->state |= ((tiosp->c_cflag & CRTS_IFLOW) ? ASY_RTSFLOWMODE : 0);
	portp->state |= ((tiosp->c_cflag & CCTS_OFLOW) ? ASY_CTSFLOWMODE : 0);
	stl_ttyoptim(portp, tiosp);
	splx(x);

	return(0);
}

/*****************************************************************************/

/*
 *	Action the flow control as required. The hw and sw args inform the
 *	routine what flow control methods it should try.
 */

static void stl_flowcontrol(stlport_t *portp, int hw, int sw)
{
	unsigned char	*head, *tail;
	int		len, hwflow, x;

#if DEBUG
	printf("stl_flowcontrol(portp=%x,hw=%d,sw=%d)\n", (int) portp, hw, sw);
#endif

	hwflow = -1;

	if (portp->state & ASY_RTSFLOWMODE) {
		if (hw == 0) {
			if ((portp->state & ASY_RTSFLOW) == 0)
				hwflow = 0;
		} else if (hw > 0) {
			if (portp->state & ASY_RTSFLOW) {
				head = portp->rx.head;
				tail = portp->rx.tail;
				len = (head >= tail) ? (head - tail) :
					(STL_RXBUFSIZE - (tail - head));
				if (len < STL_RXBUFHIGH)
					hwflow = 1;
			}
		}
	}

/*
 *	We have worked out what to do, if anything. So now apply it to the
 *	UART port.
 */
	if (hwflow >= 0) {
	    	x = spltty();
		BRDENABLE(portp->brdnr, portp->pagenr);
		stl_setreg(portp, CAR, (portp->portnr & 0x03));
		if (hwflow == 0) {
			portp->state |= ASY_RTSFLOW;
			stl_setreg(portp, MCOR1,
				(stl_getreg(portp, MCOR1) & 0xf0));
			stl_setreg(portp, MSVR2, 0);
			portp->stats.rxrtsoff++;
		} else if (hwflow > 0) {
			portp->state &= ~ASY_RTSFLOW;
			stl_setreg(portp, MSVR2, MSVR2_RTS);
			stl_setreg(portp, MCOR1,
				(stl_getreg(portp, MCOR1) | FIFO_RTSTHRESHOLD));
			portp->stats.rxrtson++;
		}
		BRDDISABLE(portp->brdnr);
		splx(x);
	}
}


/*****************************************************************************/

/*
 *	Set the state of the DTR and RTS signals.
 */

static void stl_setsignals(stlport_t *portp, int dtr, int rts)
{
	unsigned char	msvr1, msvr2;
	int		x;

#if DEBUG
	printf("stl_setsignals(portp=%x,dtr=%d,rts=%d)\n", (int) portp,
		dtr, rts);
#endif

	msvr1 = 0;
	msvr2 = 0;
	if (dtr > 0)
		msvr1 = MSVR1_DTR;
	if (rts > 0)
		msvr2 = MSVR2_RTS;

	x = spltty();
	BRDENABLE(portp->brdnr, portp->pagenr);
	stl_setreg(portp, CAR, (portp->portnr & 0x03));
	if (rts >= 0)
		stl_setreg(portp, MSVR2, msvr2);
	if (dtr >= 0)
		stl_setreg(portp, MSVR1, msvr1);
	BRDDISABLE(portp->brdnr);
	splx(x);
}

/*****************************************************************************/

/*
 *	Get the state of the signals.
 */

static int stl_getsignals(stlport_t *portp)
{
	unsigned char	msvr1, msvr2;
	int		sigs, x;

#if DEBUG
	printf("stl_getsignals(portp=%x)\n", (int) portp);
#endif

	x = spltty();
	BRDENABLE(portp->brdnr, portp->pagenr);
	stl_setreg(portp, CAR, (portp->portnr & 0x3));
	msvr1 = stl_getreg(portp, MSVR1);
	msvr2 = stl_getreg(portp, MSVR2);
	BRDDISABLE(portp->brdnr);
	splx(x);

	sigs = 0;
	sigs |= (msvr1 & MSVR1_DCD) ? TIOCM_CD : 0;
	sigs |= (msvr1 & MSVR1_CTS) ? TIOCM_CTS : 0;
	sigs |= (msvr1 & MSVR1_RI) ? TIOCM_RI : 0;
	sigs |= (msvr1 & MSVR1_DSR) ? TIOCM_DSR : 0;
	sigs |= (msvr1 & MSVR1_DTR) ? TIOCM_DTR : 0;
	sigs |= (msvr2 & MSVR2_RTS) ? TIOCM_RTS : 0;
	return(sigs);
}

/*****************************************************************************/

/*
 *	Enable or disable the Transmitter and/or Receiver.
 */

static void stl_enablerxtx(stlport_t *portp, int rx, int tx)
{
	unsigned char	ccr;
	int		x;

#if DEBUG
	printf("stl_enablerxtx(portp=%x,rx=%d,tx=%d)\n", (int) portp, rx, tx);
#endif

	ccr = 0;
	if (tx == 0)
		ccr |= CCR_TXDISABLE;
	else if (tx > 0)
		ccr |= CCR_TXENABLE;
	if (rx == 0)
		ccr |= CCR_RXDISABLE;
	else if (rx > 0)
		ccr |= CCR_RXENABLE;

	x = spltty();
	BRDENABLE(portp->brdnr, portp->pagenr);
	stl_setreg(portp, CAR, (portp->portnr & 0x03));
	stl_ccrwait(portp);
	stl_setreg(portp, CCR, ccr);
	stl_ccrwait(portp);
	BRDDISABLE(portp->brdnr);
	splx(x);
}

/*****************************************************************************/

/*
 *	Start or stop the Transmitter and/or Receiver.
 */

static void stl_startrxtx(stlport_t *portp, int rx, int tx)
{
	unsigned char	sreron, sreroff;
	int		x;

#if DEBUG
	printf("stl_startrxtx(portp=%x,rx=%d,tx=%d)\n", (int) portp, rx, tx);
#endif

	sreron = 0;
	sreroff = 0;
	if (tx == 0)
		sreroff |= (SRER_TXDATA | SRER_TXEMPTY);
	else if (tx == 1)
		sreron |= SRER_TXDATA;
	else if (tx >= 2)
		sreron |= SRER_TXEMPTY;
	if (rx == 0)
		sreroff |= SRER_RXDATA;
	else if (rx > 0)
		sreron |= SRER_RXDATA;

	x = spltty();
	BRDENABLE(portp->brdnr, portp->pagenr);
	stl_setreg(portp, CAR, (portp->portnr & 0x3));
	stl_setreg(portp, SRER,
		((stl_getreg(portp, SRER) & ~sreroff) | sreron));
	BRDDISABLE(portp->brdnr);
	if (tx > 0)
		portp->tty.t_state |= TS_BUSY;
	splx(x);
}

/*****************************************************************************/

/*
 *	Disable all interrupts from this port.
 */

static void stl_disableintrs(stlport_t *portp)
{
	int	x;

#if DEBUG
	printf("stl_disableintrs(portp=%x)\n", (int) portp);
#endif

	x = spltty();
	BRDENABLE(portp->brdnr, portp->pagenr);
	stl_setreg(portp, CAR, (portp->portnr & 0x3));
	stl_setreg(portp, SRER, 0);
	BRDDISABLE(portp->brdnr);
	splx(x);
}

/*****************************************************************************/

static void stl_sendbreak(stlport_t *portp, long len)
{
	int	x;

#if DEBUG
	printf("stl_sendbreak(portp=%x,len=%d)\n", (int) portp, (int) len);
#endif

	x = spltty();
	BRDENABLE(portp->brdnr, portp->pagenr);
	stl_setreg(portp, CAR, (portp->portnr & 0x3));
	stl_setreg(portp, COR2, (stl_getreg(portp, COR2) | COR2_ETC));
	stl_setreg(portp, SRER,
		((stl_getreg(portp, SRER) & ~SRER_TXDATA) | SRER_TXEMPTY));
	BRDDISABLE(portp->brdnr);
	if (len > 0) {
		len = len / 5;
		portp->brklen = (len > 255) ? 255 : len;
	} else {
		portp->brklen = len;
	}
	splx(x);
	portp->stats.txbreaks++;
}

/*****************************************************************************/

/*
 *	Enable l_rint processing bypass mode if tty modes allow it.
 */

static void stl_ttyoptim(stlport_t *portp, struct termios *tiosp)
{
	struct tty	*tp;

	tp = &portp->tty;
	if (((tiosp->c_iflag & (ICRNL | IGNCR | IMAXBEL | INLCR)) == 0) &&
	    (((tiosp->c_iflag & BRKINT) == 0) || (tiosp->c_iflag & IGNBRK)) &&
	    (((tiosp->c_iflag & PARMRK) == 0) ||
		((tiosp->c_iflag & (IGNPAR | IGNBRK)) == (IGNPAR | IGNBRK))) &&
	    ((tiosp->c_lflag & (ECHO | ICANON | IEXTEN | ISIG | PENDIN)) ==0) &&
	    (linesw[tp->t_line].l_rint == ttyinput))
		tp->t_state |= TS_CAN_BYPASS_L_RINT;
	else
		tp->t_state &= ~TS_CAN_BYPASS_L_RINT;

	if (tp->t_line == SLIPDISC)
		portp->hotchar = 0xc0;
	else if (tp->t_line == PPPDISC)
		portp->hotchar = 0x7e;
	else
		portp->hotchar = 0;
}

/*****************************************************************************/

/*
 *	Try and find and initialize all the ports on a panel. We don't care
 *	what sort of board these ports are on - since the port io registers
 *	are almost identical when dealing with ports.
 */

static int stl_initports(stlbrd_t *brdp, stlpanel_t *panelp)
{
	stlport_t	*portp;
	unsigned int	chipmask;
	unsigned int	gfrcr;
	int		nrchips, uartaddr, ioaddr;
	int		i, j;

#if DEBUG
	printf("stl_initports(panelp=%x)\n", (int) panelp);
#endif

	BRDENABLE(panelp->brdnr, panelp->pagenr);

/*
 *	Check that each chip is present and started up OK.
 */
	chipmask = 0;
	nrchips = panelp->nrports / CD1400_PORTS;
	for (i = 0; (i < nrchips); i++) {
		if (brdp->brdtype == BRD_ECHPCI) {
			outb(brdp->ioctrl, (panelp->pagenr + (i >> 1)));
			ioaddr = panelp->iobase;
		} else {
			ioaddr = panelp->iobase + (EREG_BANKSIZE * (i >> 1));
		}
		uartaddr = (i & 0x01) ? 0x080 : 0;
		outb(ioaddr, (GFRCR + uartaddr));
		outb((ioaddr + EREG_DATA), 0);
		outb(ioaddr, (CCR + uartaddr));
		outb((ioaddr + EREG_DATA), CCR_RESETFULL);
		outb((ioaddr + EREG_DATA), CCR_RESETFULL);
		outb(ioaddr, (GFRCR + uartaddr));
		for (j = 0; (j < CCR_MAXWAIT); j++) {
			gfrcr = inb(ioaddr + EREG_DATA);
			if ((gfrcr > 0x40) && (gfrcr < 0x60))
				break;
		}
		if (j >= CCR_MAXWAIT) {
			printf("STALLION: cd1400 not responding, brd=%d "
				"panel=%d chip=%d\n", panelp->brdnr,
				panelp->panelnr, i);
			continue;
		}
		chipmask |= (0x1 << i);
		outb(ioaddr, (PPR + uartaddr));
		outb((ioaddr + EREG_DATA), PPR_SCALAR);
	}

/*
 *	All cd1400's are initialized (if found!). Now go through and setup
 *	each ports data structures. Also init the LIVR register of cd1400
 *	for each port.
 */
	ioaddr = panelp->iobase;
	for (i = 0; (i < panelp->nrports); i++) {
		if (brdp->brdtype == BRD_ECHPCI) {
			outb(brdp->ioctrl, (panelp->pagenr + (i >> 3)));
			ioaddr = panelp->iobase;
		} else {
			ioaddr = panelp->iobase + (EREG_BANKSIZE * (i >> 3));
		}
		if ((chipmask & (0x1 << (i / 4))) == 0)
			continue;
		portp = (stlport_t *) malloc(sizeof(stlport_t), M_TTYS,
			M_NOWAIT);
		if (portp == (stlport_t *) NULL) {
			printf("STALLION: failed to allocate port memory "
				"(size=%d)\n", sizeof(stlport_t));
			break;
		}
		bzero(portp, sizeof(stlport_t));

		portp->portnr = i;
		portp->brdnr = panelp->brdnr;
		portp->panelnr = panelp->panelnr;
		portp->clk = brdp->clk;
		portp->ioaddr = ioaddr;
		portp->uartaddr = (i & 0x4) << 5;
		portp->pagenr = panelp->pagenr + (i >> 3);
		portp->hwid = stl_getreg(portp, GFRCR);
		stl_setreg(portp, CAR, (i & 0x3));
		stl_setreg(portp, LIVR, (i << 3));
		panelp->ports[i] = portp;

		j = STL_TXBUFSIZE + (2 * STL_RXBUFSIZE);
		portp->tx.buf = (char *) malloc(j, M_TTYS, M_NOWAIT);
		if (portp->tx.buf == (char *) NULL) {
			printf("STALLION: failed to allocate buffer memory "
				"(size=%d)\n", j);
			break;
		}
		portp->tx.endbuf = portp->tx.buf + STL_TXBUFSIZE;
		portp->tx.head = portp->tx.buf;
		portp->tx.tail = portp->tx.buf;
		portp->rx.buf = portp->tx.buf + STL_TXBUFSIZE;
		portp->rx.endbuf = portp->rx.buf + STL_RXBUFSIZE;
		portp->rx.head = portp->rx.buf;
		portp->rx.tail = portp->rx.buf;
		portp->rxstatus.buf = portp->rx.buf + STL_RXBUFSIZE;
		portp->rxstatus.endbuf = portp->rxstatus.buf + STL_RXBUFSIZE;
		portp->rxstatus.head = portp->rxstatus.buf;
		portp->rxstatus.tail = portp->rxstatus.buf;
		bzero(portp->rxstatus.head, STL_RXBUFSIZE);

		portp->initintios.c_ispeed = STL_DEFSPEED;
		portp->initintios.c_ospeed = STL_DEFSPEED;
		portp->initintios.c_cflag = STL_DEFCFLAG;
		portp->initintios.c_iflag = 0;
		portp->initintios.c_oflag = 0;
		portp->initintios.c_lflag = 0;
		bcopy(&ttydefchars[0], &portp->initintios.c_cc[0],
			sizeof(portp->initintios.c_cc));
		portp->initouttios = portp->initintios;
		portp->dtrwait = 3 * hz;
	}

	BRDDISABLE(panelp->brdnr);
	return(0);
}

/*****************************************************************************/

/*
 *	Try to find and initialize an EasyIO board.
 */

static int stl_initeio(stlbrd_t *brdp)
{
	stlpanel_t	*panelp;
	unsigned int	status;

#if DEBUG
	printf("stl_initeio(brdp=%x)\n", (int) brdp);
#endif

	brdp->ioctrl = brdp->ioaddr1 + 1;
	brdp->iostatus = brdp->ioaddr1 + 2;
	brdp->clk = EIO_CLK;

	status = inb(brdp->iostatus);
	switch (status & EIO_IDBITMASK) {
	case EIO_8PORTM:
		brdp->clk = EIO_CLK8M;
		/* fall thru */
	case EIO_8PORTRS:
	case EIO_8PORTDI:
		brdp->nrports = 8;
		break;
	case EIO_4PORTRS:
		brdp->nrports = 4;
		break;
	default:
		return(ENODEV);
	}

/*
 *	Check that the supplied IRQ is good and then use it to setup the
 *	programmable interrupt bits on EIO board. Also set the edge/level
 *	triggered interrupt bit.
 */
	if ((brdp->irq < 0) || (brdp->irq > 15) ||
			(stl_vecmap[brdp->irq] == (unsigned char) 0xff)) {
		printf("STALLION: invalid irq=%d for brd=%d\n", brdp->irq,
			brdp->brdnr);
		return(EINVAL);
	}
	outb(brdp->ioctrl, (stl_vecmap[brdp->irq] |
		((brdp->irqtype) ? EIO_INTLEVEL : EIO_INTEDGE)));

	panelp = (stlpanel_t *) malloc(sizeof(stlpanel_t), M_TTYS, M_NOWAIT);
	if (panelp == (stlpanel_t *) NULL) {
		printf("STALLION: failed to allocate memory (size=%d)\n",
			sizeof(stlpanel_t));
		return(ENOMEM);
	}
	bzero(panelp, sizeof(stlpanel_t));

	panelp->brdnr = brdp->brdnr;
	panelp->panelnr = 0;
	panelp->nrports = brdp->nrports;
	panelp->iobase = brdp->ioaddr1;
	panelp->hwid = status;
	brdp->panels[0] = panelp;
	brdp->nrpanels = 1;
	brdp->hwid = status;
	brdp->state |= BRD_FOUND;
	return(0);
}

/*****************************************************************************/

/*
 *	Try to find an ECH board and initialize it. This code is capable of
 *	dealing with all types of ECH board.
 */

static int stl_initech(stlbrd_t *brdp)
{
	stlpanel_t	*panelp;
	unsigned int	status, nxtid;
	int		panelnr, ioaddr, i;

#if DEBUG
	printf("stl_initech(brdp=%x)\n", (int) brdp);
#endif

/*
 *	Set up the initial board register contents for boards. This varys a
 *	bit between the different board types. So we need to handle each
 *	separately. Also do a check that the supplied IRQ is good.
 */
	if (brdp->brdtype == BRD_ECH) {
		brdp->ioctrl = brdp->ioaddr1 + 1;
		brdp->iostatus = brdp->ioaddr1 + 1;
		status = inb(brdp->iostatus);
		if ((status & ECH_IDBITMASK) != ECH_ID)
			return(ENODEV);
		brdp->hwid = status;

		if ((brdp->irq < 0) || (brdp->irq > 15) ||
				(stl_vecmap[brdp->irq] == (unsigned char) 0xff)) {
			printf("STALLION: invalid irq=%d for brd=%d\n",
				brdp->irq, brdp->brdnr);
			return(EINVAL);
		}
		status = ((brdp->ioaddr2 & ECH_ADDR2MASK) >> 1);
		status |= (stl_vecmap[brdp->irq] << 1);
		outb(brdp->ioaddr1, (status | ECH_BRDRESET));
		brdp->ioctrlval = ECH_INTENABLE |
			((brdp->irqtype) ? ECH_INTLEVEL : ECH_INTEDGE);
		outb(brdp->ioctrl, (brdp->ioctrlval | ECH_BRDENABLE));
		outb(brdp->ioaddr1, status);
	} else if (brdp->brdtype == BRD_ECHMC) {
		brdp->ioctrl = brdp->ioaddr1 + 0x20;
		brdp->iostatus = brdp->ioctrl;
		status = inb(brdp->iostatus);
		if ((status & ECH_IDBITMASK) != ECH_ID)
			return(ENODEV);
		brdp->hwid = status;

		if ((brdp->irq < 0) || (brdp->irq > 15) ||
				(stl_vecmap[brdp->irq] == (unsigned char) 0xff)) {
			printf("STALLION: invalid irq=%d for brd=%d\n",
				brdp->irq, brdp->brdnr);
			return(EINVAL);
		}
		outb(brdp->ioctrl, ECHMC_BRDRESET);
		outb(brdp->ioctrl, ECHMC_INTENABLE);
	} else if (brdp->brdtype == BRD_ECHPCI) {
		brdp->ioctrl = brdp->ioaddr1 + 2;
	}

	brdp->clk = ECH_CLK;

/*
 *	Scan through the secondary io address space looking for panels.
 *	As we find'em allocate and initialize panel structures for each.
 */
	ioaddr = brdp->ioaddr2;
	panelnr = 0;
	nxtid = 0;

	for (i = 0; (i < STL_MAXPANELS); i++) {
		if (brdp->brdtype == BRD_ECHPCI) {
			outb(brdp->ioctrl, nxtid);
			ioaddr = brdp->ioaddr2;
		}
		status = inb(ioaddr + ECH_PNLSTATUS);
		if ((status & ECH_PNLIDMASK) != nxtid)
			break;
		panelp = (stlpanel_t *) malloc(sizeof(stlpanel_t), M_TTYS,
			M_NOWAIT);
		if (panelp == (stlpanel_t *) NULL) {
			printf("STALLION: failed to allocate memory"
				"(size=%d)\n", sizeof(stlpanel_t));
			break;
		}
		bzero(panelp, sizeof(stlpanel_t));
		panelp->brdnr = brdp->brdnr;
		panelp->panelnr = panelnr;
		panelp->iobase = ioaddr;
		panelp->pagenr = nxtid;
		panelp->hwid = status;
		if (status & ECH_PNL16PORT) {
			if ((brdp->nrports + 16) > 32)
				break;
			panelp->nrports = 16;
			panelp->ackmask = 0x80;
			brdp->nrports += 16;
			ioaddr += (EREG_BANKSIZE * 2);
			nxtid += 2;
		} else {
			panelp->nrports = 8;
			panelp->ackmask = 0xc0;
			brdp->nrports += 8;
			ioaddr += EREG_BANKSIZE;
			nxtid++;
		}
		brdp->panels[panelnr++] = panelp;
		brdp->nrpanels++;
		if (ioaddr >= (brdp->ioaddr2 + 0x20))
			break;
	}

	if (brdp->brdtype == BRD_ECH)
		outb(brdp->ioctrl, (brdp->ioctrlval | ECH_BRDDISABLE));

	brdp->state |= BRD_FOUND;
	return(0);
}

/*****************************************************************************/

/*
 *	Initialize and configure the specified board. This firstly probes
 *	for the board, if it is found then the board is initialized and
 *	then all its ports are initialized as well.
 */

static int stl_brdinit(stlbrd_t *brdp)
{
	stlpanel_t	*panelp;
	int		i, j, k;

#if DEBUG
	printf("stl_brdinit(brdp=%x): unit=%d type=%d io1=%x io2=%x irq=%d\n",
		(int) brdp, brdp->brdnr, brdp->brdtype, brdp->ioaddr1,
		brdp->ioaddr2, brdp->irq);
#endif

	switch (brdp->brdtype) {
	case BRD_EASYIO:
		stl_initeio(brdp);
		break;
	case BRD_ECH:
	case BRD_ECHMC:
	case BRD_ECHPCI:
		stl_initech(brdp);
		break;
	default:
		printf("STALLION: unit=%d is unknown board type=%d\n",
			brdp->brdnr, brdp->brdtype);
		return(ENODEV);
	}

	stl_brds[brdp->brdnr] = brdp;
	if ((brdp->state & BRD_FOUND) == 0) {
#if 0
		printf("STALLION: %s board not found, unit=%d io=%x irq=%d\n",
			stl_brdnames[brdp->brdtype], brdp->brdnr,
			brdp->ioaddr1, brdp->irq);
#endif
		return(ENODEV);
	}

	for (i = 0, k = 0; (i < STL_MAXPANELS); i++) {
		panelp = brdp->panels[i];
		if (panelp != (stlpanel_t *) NULL) {
			stl_initports(brdp, panelp);
			for (j = 0; (j < panelp->nrports); j++)
				brdp->ports[k++] = panelp->ports[j];
		}
	}

	printf("stl%d: %s (driver version %s) unit=%d nrpanels=%d nrports=%d\n",
		brdp->unitid, stl_brdnames[brdp->brdtype], stl_drvversion,
		brdp->brdnr, brdp->nrpanels, brdp->nrports);
	return(0);
}

/*****************************************************************************/

/*
 *	Return the board stats structure to user app.
 */

static int stl_getbrdstats(caddr_t data)
{
	stlbrd_t	*brdp;
	stlpanel_t	*panelp;
	int		i;

	stl_brdstats = *((combrd_t *) data);
	if (stl_brdstats.brd >= STL_MAXBRDS)
		return(-ENODEV);
	brdp = stl_brds[stl_brdstats.brd];
	if (brdp == (stlbrd_t *) NULL)
		return(-ENODEV);

	bzero(&stl_brdstats, sizeof(combrd_t));
	stl_brdstats.brd = brdp->brdnr;
	stl_brdstats.type = brdp->brdtype;
	stl_brdstats.hwid = brdp->hwid;
	stl_brdstats.state = brdp->state;
	stl_brdstats.ioaddr = brdp->ioaddr1;
	stl_brdstats.ioaddr2 = brdp->ioaddr2;
	stl_brdstats.irq = brdp->irq;
	stl_brdstats.nrpanels = brdp->nrpanels;
	stl_brdstats.nrports = brdp->nrports;
	for (i = 0; (i < brdp->nrpanels); i++) {
		panelp = brdp->panels[i];
		stl_brdstats.panels[i].panel = i;
		stl_brdstats.panels[i].hwid = panelp->hwid;
		stl_brdstats.panels[i].nrports = panelp->nrports;
	}

	*((combrd_t *) data) = stl_brdstats;;
	return(0);
}

/*****************************************************************************/

/*
 *	Resolve the referenced port number into a port struct pointer.
 */

static stlport_t *stl_getport(int brdnr, int panelnr, int portnr)
{
	stlbrd_t	*brdp;
	stlpanel_t	*panelp;

	if ((brdnr < 0) || (brdnr >= STL_MAXBRDS))
		return((stlport_t *) NULL);
	brdp = stl_brds[brdnr];
	if (brdp == (stlbrd_t *) NULL)
		return((stlport_t *) NULL);
	if ((panelnr < 0) || (panelnr >= brdp->nrpanels))
		return((stlport_t *) NULL);
	panelp = brdp->panels[panelnr];
	if (panelp == (stlpanel_t *) NULL)
		return((stlport_t *) NULL);
	if ((portnr < 0) || (portnr >= panelp->nrports))
		return((stlport_t *) NULL);
	return(panelp->ports[portnr]);
}

/*****************************************************************************/

/*
 *	Return the port stats structure to user app. A NULL port struct
 *	pointer passed in means that we need to find out from the app
 *	what port to get stats for (used through board control device).
 */

static int stl_getportstats(stlport_t *portp, caddr_t data)
{
	unsigned char	*head, *tail;

	if (portp == (stlport_t *) NULL) {
		stl_comstats = *((comstats_t *) data);
		portp = stl_getport(stl_comstats.brd, stl_comstats.panel,
			stl_comstats.port);
		if (portp == (stlport_t *) NULL)
			return(-ENODEV);
	}

	portp->stats.state = portp->state;
	/*portp->stats.flags = portp->flags;*/
	portp->stats.hwid = portp->hwid;
	portp->stats.ttystate = portp->tty.t_state;
	portp->stats.cflags = portp->tty.t_cflag;
	portp->stats.iflags = portp->tty.t_iflag;
	portp->stats.oflags = portp->tty.t_oflag;
	portp->stats.lflags = portp->tty.t_lflag;

	head = portp->tx.head;
	tail = portp->tx.tail;
	portp->stats.txbuffered = ((head >= tail) ? (head - tail) :
		(STL_TXBUFSIZE - (tail - head)));

	head = portp->rx.head;
	tail = portp->rx.tail;
	portp->stats.rxbuffered = (head >= tail) ? (head - tail) :
		(STL_RXBUFSIZE - (tail - head));

	portp->stats.signals = (unsigned long) stl_getsignals(portp);

	*((comstats_t *) data) = portp->stats;
	return(0);
}

/*****************************************************************************/

/*
 *	Clear the port stats structure. We also return it zeroed out...
 */

static int stl_clrportstats(stlport_t *portp, caddr_t data)
{
	if (portp == (stlport_t *) NULL) {
		stl_comstats = *((comstats_t *) data);
		portp = stl_getport(stl_comstats.brd, stl_comstats.panel,
			stl_comstats.port);
		if (portp == (stlport_t *) NULL)
			return(-ENODEV);
	}

	bzero(&portp->stats, sizeof(comstats_t));
	portp->stats.brd = portp->brdnr;
	portp->stats.panel = portp->panelnr;
	portp->stats.port = portp->portnr;
	*((comstats_t *) data) = stl_comstats;
	return(0);
}

/*****************************************************************************/

/*
 *	The "staliomem" device is used for stats collection in this driver.
 */

static int stl_memioctl(dev_t dev, int cmd, caddr_t data, int flag, struct proc *p)
{
	stlbrd_t	*brdp;
	int		brdnr, rc;

#if DEBUG
	printf("stl_memioctl(dev=%x,cmd=%x,data=%x,flag=%x)\n", (int) dev,
		cmd, (int) data, flag);
#endif

	brdnr = dev & 0x7;
	brdp = stl_brds[brdnr];
	if (brdp == (stlbrd_t *) NULL)
		return(ENODEV);
	if (brdp->state == 0)
		return(ENODEV);

	rc = 0;

	switch (cmd) {
	case COM_GETPORTSTATS:
		rc = stl_getportstats((stlport_t *) NULL, data);
		break;
	case COM_CLRPORTSTATS:
		rc = stl_clrportstats((stlport_t *) NULL, data);
		break;
	case COM_GETBRDSTATS:
		rc = stl_getbrdstats(data);
		break;
	default:
		rc = ENOTTY;
		break;
	}

	return(rc);
}

/*****************************************************************************/
