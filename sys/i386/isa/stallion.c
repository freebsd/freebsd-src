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
 * $FreeBSD$
 */

/*****************************************************************************/

#define	TTYDEFCHARS	1

#include "opt_compat.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/tty.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <i386/isa/isa_device.h>
#include <i386/isa/ic/scd1400.h>
#include <i386/isa/ic/sc26198.h>
#include <machine/comstats.h>

#include "pci.h"
#if NPCI > 0
#include <pci/pcivar.h>
#include <pci/pcireg.h>
#endif

#undef STLDEBUG

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
#define	BRD_ECH64PCI	27
#define	BRD_EASYIOPCI	28

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
#define	STL_MAXBANKS		8
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

#define	STL_DEFSPEED	TTYDEF_SPEED
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
static const char	stl_drvversion[] = "2.0.0";
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
typedef struct stlport {
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
	void		*uartp;
	unsigned int	state;
	unsigned int	hwid;
	unsigned int	sigs;
	unsigned int	rxignoremsk;
	unsigned int	rxmarkmsk;
	unsigned int	crenable;
	unsigned int	imr;
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

typedef struct stlpanel {
	int		panelnr;
	int		brdnr;
	int		pagenr;
	int		nrports;
	int		iobase;
	unsigned int	hwid;
	unsigned int	ackmask;
	void		(*isr)(struct stlpanel *panelp, unsigned int iobase);
	void		*uartp;
	stlport_t	*ports[STL_PORTSPERPANEL];
} stlpanel_t;

typedef struct stlbrd {
	int		brdnr;
	int		brdtype;
	int		unitid;
	int		state;
	int		nrpanels;
	int		nrports;
	int		nrbnks;
	int		irq;
	int		irqtype;
	unsigned int	ioaddr1;
	unsigned int	ioaddr2;
	unsigned int	iostatus;
	unsigned int	ioctrl;
	unsigned int	ioctrlval;
	unsigned int	hwid;
	unsigned long	clk;
	void		(*isr)(struct stlbrd *brdp);
	unsigned int	bnkpageaddr[STL_MAXBANKS];
	unsigned int	bnkstataddr[STL_MAXBANKS];
	stlpanel_t	*bnk2panel[STL_MAXBANKS];
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
#define	ASY_TXFLOWED	0x80
#define	ASY_TXBUSY	0x100
#define	ASY_TXEMPTY	0x200

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
	"EC8/64-PCI",
	"EasyIO-PCI",
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
#define	EIO_MK3		0x03
#define	EIO_IDBITMASK	0x07

#define	EIO_BRDMASK	0xf0
#define	ID_BRD4		0x10
#define	ID_BRD8		0x20
#define	ID_BRD16	0x30

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
#define	ECH_PNLXPID     0x40
#define	ECH_PNLINTRPEND	0x80
#define	ECH_ADDR2MASK	0x1e0

#define	EIO_CLK		25000000
#define	EIO_CLK8M	20000000
#define	ECH_CLK		EIO_CLK

/*
 *      Define the PCI vendor and device ID for Stallion PCI boards.
 */
#define	STL_PCINSVENDID	0x100b
#define	STL_PCINSDEVID	0xd001

#define	STL_PCIVENDID	0x124d
#define	STL_PCI32DEVID	0x0000
#define	STL_PCI64DEVID	0x0002
#define	STL_PCIEIODEVID	0x0003

#define	STL_PCIBADCLASS	0x0101

typedef struct stlpcibrd {
        unsigned short          vendid;
        unsigned short          devid;
        int                     brdtype;
} stlpcibrd_t;

static	stlpcibrd_t	stl_pcibrds[] = {
	{ STL_PCIVENDID, STL_PCI64DEVID, BRD_ECH64PCI },
	{ STL_PCIVENDID, STL_PCIEIODEVID, BRD_EASYIOPCI },
	{ STL_PCIVENDID, STL_PCI32DEVID, BRD_ECHPCI },
	{ STL_PCINSVENDID, STL_PCINSDEVID, BRD_ECHPCI },
};

static int      stl_nrpcibrds = sizeof(stl_pcibrds) / sizeof(stlpcibrd_t);

/*****************************************************************************/

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
 *      Define some spare buffer space for un-wanted received characters.
 */
static char     stl_unwanted[SC26198_RXFIFOSIZE];

/*****************************************************************************/

/*
 *	Define macros to extract a brd and port number from a minor number.
 *	This uses the extended minor number range in the upper 2 bytes of
 *	the device number. This gives us plenty of minor numbers to play
 *	with...
 */
#define	MKDEV2BRD(m)	((minor(m) & 0x00700000) >> 20)
#define	MKDEV2PORT(m)	((minor(m) & 0x1f) | ((minor(m) & 0x00010000) >> 11))

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

static int	stlprobe(struct isa_device *idp);
static int	stlattach(struct isa_device *idp);

STATIC	d_open_t	stlopen;
STATIC	d_close_t	stlclose;
STATIC	d_ioctl_t	stlioctl;

/*
 *	Internal function prototypes.
 */
static stlport_t *stl_dev2port(dev_t dev);
static int	stl_findfreeunit(void);
static int	stl_rawopen(stlport_t *portp);
static int	stl_rawclose(stlport_t *portp);
static void	stl_flush(stlport_t *portp, int flag);
static int	stl_param(struct tty *tp, struct termios *tiosp);
static void	stl_start(struct tty *tp);
static void	stl_stop(struct tty *tp, int);
static void	stl_ttyoptim(stlport_t *portp, struct termios *tiosp);
static void	stl_dotimeout(void);
static void	stl_poll(void *arg);
static void	stl_rxprocess(stlport_t *portp);
static void	stl_flowcontrol(stlport_t *portp, int hw, int sw);
static void	stl_dtrwakeup(void *arg);
static int	stl_brdinit(stlbrd_t *brdp);
static int	stl_initeio(stlbrd_t *brdp);
static int	stl_initech(stlbrd_t *brdp);
static int	stl_initports(stlbrd_t *brdp, stlpanel_t *panelp);
static void	stl_eiointr(stlbrd_t *brdp);
static void	stl_echatintr(stlbrd_t *brdp);
static void	stl_echmcaintr(stlbrd_t *brdp);
static void	stl_echpciintr(stlbrd_t *brdp);
static void	stl_echpci64intr(stlbrd_t *brdp);
static int	stl_memioctl(dev_t dev, unsigned long cmd, caddr_t data,
			int flag, struct proc *p);
static int	stl_getbrdstats(caddr_t data);
static int	stl_getportstats(stlport_t *portp, caddr_t data);
static int	stl_clrportstats(stlport_t *portp, caddr_t data);
static stlport_t *stl_getport(int brdnr, int panelnr, int portnr);
static ointhand2_t	stlintr;

#if NPCI > 0
static const char *stlpciprobe(pcici_t tag, pcidi_t type);
static void	stlpciattach(pcici_t tag, int unit);
static void	stlpciintr(void * arg);
#endif

/*
 *	CD1400 uart specific handling functions.
 */
static void	stl_cd1400setreg(stlport_t *portp, int regnr, int value);
static int	stl_cd1400getreg(stlport_t *portp, int regnr);
static int	stl_cd1400updatereg(stlport_t *portp, int regnr, int value);
static int	stl_cd1400panelinit(stlbrd_t *brdp, stlpanel_t *panelp);
static void	stl_cd1400portinit(stlbrd_t *brdp, stlpanel_t *panelp, stlport_t *portp);
static int	stl_cd1400setport(stlport_t *portp, struct termios *tiosp);
static int	stl_cd1400getsignals(stlport_t *portp);
static void	stl_cd1400setsignals(stlport_t *portp, int dtr, int rts);
static void	stl_cd1400ccrwait(stlport_t *portp);
static void	stl_cd1400enablerxtx(stlport_t *portp, int rx, int tx);
static void	stl_cd1400startrxtx(stlport_t *portp, int rx, int tx);
static void	stl_cd1400disableintrs(stlport_t *portp);
static void	stl_cd1400sendbreak(stlport_t *portp, long len);
static void	stl_cd1400sendflow(stlport_t *portp, int hw, int sw);
static int	stl_cd1400datastate(stlport_t *portp);
static void	stl_cd1400flush(stlport_t *portp, int flag);
static __inline void	stl_cd1400txisr(stlpanel_t *panelp, int ioaddr);
static void	stl_cd1400rxisr(stlpanel_t *panelp, int ioaddr);
static void	stl_cd1400mdmisr(stlpanel_t *panelp, int ioaddr);
static void	stl_cd1400eiointr(stlpanel_t *panelp, unsigned int iobase);
static void	stl_cd1400echintr(stlpanel_t *panelp, unsigned int iobase);

/*
 *	SC26198 uart specific handling functions.
 */
static void	stl_sc26198setreg(stlport_t *portp, int regnr, int value);
static int	stl_sc26198getreg(stlport_t *portp, int regnr);
static int	stl_sc26198updatereg(stlport_t *portp, int regnr, int value);
static int	stl_sc26198getglobreg(stlport_t *portp, int regnr);
static int	stl_sc26198panelinit(stlbrd_t *brdp, stlpanel_t *panelp);
static void	stl_sc26198portinit(stlbrd_t *brdp, stlpanel_t *panelp, stlport_t *portp);
static int	stl_sc26198setport(stlport_t *portp, struct termios *tiosp);
static int	stl_sc26198getsignals(stlport_t *portp);
static void	stl_sc26198setsignals(stlport_t *portp, int dtr, int rts);
static void	stl_sc26198enablerxtx(stlport_t *portp, int rx, int tx);
static void	stl_sc26198startrxtx(stlport_t *portp, int rx, int tx);
static void	stl_sc26198disableintrs(stlport_t *portp);
static void	stl_sc26198sendbreak(stlport_t *portp, long len);
static void	stl_sc26198sendflow(stlport_t *portp, int hw, int sw);
static int	stl_sc26198datastate(stlport_t *portp);
static void	stl_sc26198flush(stlport_t *portp, int flag);
static void	stl_sc26198txunflow(stlport_t *portp);
static void	stl_sc26198wait(stlport_t *portp);
static void	stl_sc26198intr(stlpanel_t *panelp, unsigned int iobase);
static void	stl_sc26198txisr(stlport_t *port);
static void	stl_sc26198rxisr(stlport_t *port, unsigned int iack);
static void	stl_sc26198rxgoodchars(stlport_t *portp);
static void	stl_sc26198rxbadchars(stlport_t *portp);
static void	stl_sc26198otherisr(stlport_t *port, unsigned int iack);

/*****************************************************************************/

/*
 *      Generic UART support structure.
 */
typedef struct uart {
	int	(*panelinit)(stlbrd_t *brdp, stlpanel_t *panelp);
	void	(*portinit)(stlbrd_t *brdp, stlpanel_t *panelp, stlport_t *portp);
	int	(*setport)(stlport_t *portp, struct termios *tiosp);
	int	(*getsignals)(stlport_t *portp);
	void	(*setsignals)(stlport_t *portp, int dtr, int rts);
	void	(*enablerxtx)(stlport_t *portp, int rx, int tx);
	void	(*startrxtx)(stlport_t *portp, int rx, int tx);
	void	(*disableintrs)(stlport_t *portp);
	void	(*sendbreak)(stlport_t *portp, long len);
	void	(*sendflow)(stlport_t *portp, int hw, int sw);
	void	(*flush)(stlport_t *portp, int flag);
	int	(*datastate)(stlport_t *portp);
	void	(*intr)(stlpanel_t *panelp, unsigned int iobase);
} uart_t;

/*
 *	Define some macros to make calling these functions nice and clean.
 */
#define stl_panelinit		(* ((uart_t *) panelp->uartp)->panelinit)
#define stl_portinit		(* ((uart_t *) portp->uartp)->portinit)
#define stl_setport		(* ((uart_t *) portp->uartp)->setport)
#define stl_getsignals		(* ((uart_t *) portp->uartp)->getsignals)
#define stl_setsignals		(* ((uart_t *) portp->uartp)->setsignals)
#define stl_enablerxtx		(* ((uart_t *) portp->uartp)->enablerxtx)
#define stl_startrxtx		(* ((uart_t *) portp->uartp)->startrxtx)
#define stl_disableintrs	(* ((uart_t *) portp->uartp)->disableintrs)
#define stl_sendbreak		(* ((uart_t *) portp->uartp)->sendbreak)
#define stl_sendflow		(* ((uart_t *) portp->uartp)->sendflow)
#define stl_uartflush		(* ((uart_t *) portp->uartp)->flush)
#define stl_datastate		(* ((uart_t *) portp->uartp)->datastate)

/*****************************************************************************/

/*
 *      CD1400 UART specific data initialization.
 */
static uart_t stl_cd1400uart = {
	stl_cd1400panelinit,
	stl_cd1400portinit,
	stl_cd1400setport,
	stl_cd1400getsignals,
	stl_cd1400setsignals,
	stl_cd1400enablerxtx,
	stl_cd1400startrxtx,
	stl_cd1400disableintrs,
	stl_cd1400sendbreak,
	stl_cd1400sendflow,
	stl_cd1400flush,
	stl_cd1400datastate,
	stl_cd1400eiointr
};

/*
 *      Define the offsets within the register bank of a cd1400 based panel.
 *      These io address offsets are common to the EasyIO board as well.
 */
#define	EREG_ADDR	0
#define	EREG_DATA	4
#define	EREG_RXACK	5
#define	EREG_TXACK	6
#define	EREG_MDACK	7

#define	EREG_BANKSIZE	8

#define	CD1400_CLK	25000000
#define	CD1400_CLK8M	20000000

/*
 *      Define the cd1400 baud rate clocks. These are used when calculating
 *      what clock and divisor to use for the required baud rate. Also
 *      define the maximum baud rate allowed, and the default base baud.
 */
static int	stl_cd1400clkdivs[] = {
	CD1400_CLK0, CD1400_CLK1, CD1400_CLK2, CD1400_CLK3, CD1400_CLK4
};

/*
 *      Define the maximum baud rate of the cd1400 devices.
 */
#define	CD1400_MAXBAUD	230400

/*****************************************************************************/

/*
 *      SC26198 UART specific data initization.
 */
static uart_t stl_sc26198uart = {
	stl_sc26198panelinit,
	stl_sc26198portinit,
	stl_sc26198setport,
	stl_sc26198getsignals,
	stl_sc26198setsignals,
	stl_sc26198enablerxtx,
	stl_sc26198startrxtx,
	stl_sc26198disableintrs,
	stl_sc26198sendbreak,
	stl_sc26198sendflow,
	stl_sc26198flush,
	stl_sc26198datastate,
	stl_sc26198intr
};

/*
 *      Define the offsets within the register bank of a sc26198 based panel.
 */
#define	XP_DATA		0
#define	XP_ADDR		1
#define	XP_MODID	2
#define	XP_STATUS	2
#define	XP_IACK		3

#define	XP_BANKSIZE	4

/*
 *      Define the sc26198 baud rate table. Offsets within the table
 *      represent the actual baud rate selector of sc26198 registers.
 */
static unsigned int	sc26198_baudtable[] = {
	50, 75, 150, 200, 300, 450, 600, 900, 1200, 1800, 2400, 3600,
	4800, 7200, 9600, 14400, 19200, 28800, 38400, 57600, 115200,
	230400, 460800
};

#define	SC26198_NRBAUDS	(sizeof(sc26198_baudtable) / sizeof(unsigned int))

/*
 *      Define the maximum baud rate of the sc26198 devices.
 */
#define	SC26198_MAXBAUD	460800

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

COMPAT_PCI_DRIVER (stlpci, stlpcidriver);

#endif

/*****************************************************************************/

#if VFREEBSD >= 220

/*
 *	FreeBSD-2.2+ kernel linkage.
 */

#define	CDEV_MAJOR	72
static struct cdevsw stl_cdevsw = {
	/* open */	stlopen,
	/* close */	stlclose,
	/* read */	ttyread,
	/* write */	ttywrite,
	/* ioctl */	stlioctl,
	/* poll */	ttypoll,
	/* mmap */	nommap,
	/* strategy */	nostrategy,
	/* name */	"stl",
	/* maj */	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	D_TTY | D_KQFILTER,
	/* bmaj */	-1,
	/* kqfilter */	ttykqfilter,
};

static void stl_drvinit(void *unused)
{

	cdevsw_add(&stl_cdevsw);
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

static int stlprobe(struct isa_device *idp)
{
	unsigned int	status;

#if STLDEBUG
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
	case EIO_MK3:
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

static int stlattach(struct isa_device *idp)
{
	stlbrd_t	*brdp;
	int		boardnr, portnr, minor_dev;

#if STLDEBUG
	printf("stlattach(idp=%p): unit=%d iobase=%x\n", (void *) idp,
		idp->id_unit, idp->id_iobase);
#endif

/*	idp->id_ointr = stlintr; */

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

	/* register devices for DEVFS */
	boardnr = brdp->brdnr;
	make_dev(&stl_cdevsw, boardnr + 0x1000000, UID_ROOT, GID_WHEEL,
		 0600, "staliomem%d", boardnr);

	for (portnr = 0, minor_dev = boardnr * 0x100000;
	      portnr < 32; portnr++, minor_dev++) {
		/* hw ports */
		make_dev(&stl_cdevsw, minor_dev,
			 UID_ROOT, GID_WHEEL, 0600,
			 "ttyE%d", portnr + (boardnr * 64));
		make_dev(&stl_cdevsw, minor_dev + 32,
			 UID_ROOT, GID_WHEEL, 0600,
			 "ttyiE%d", portnr + (boardnr * 64));
		make_dev(&stl_cdevsw, minor_dev + 64,
			 UID_ROOT, GID_WHEEL, 0600,
			 "ttylE%d", portnr + (boardnr * 64));
		make_dev(&stl_cdevsw, minor_dev + 128,
			 UID_ROOT, GID_WHEEL, 0600,
			 "cue%d", portnr + (boardnr * 64));
		make_dev(&stl_cdevsw, minor_dev + 160,
			 UID_ROOT, GID_WHEEL, 0600,
			 "cuie%d", portnr + (boardnr * 64));
		make_dev(&stl_cdevsw, minor_dev + 192,
			 UID_ROOT, GID_WHEEL, 0600,
			 "cule%d", portnr + (boardnr * 64));

		/* sw ports */
		make_dev(&stl_cdevsw, minor_dev + 0x10000,
			 UID_ROOT, GID_WHEEL, 0600,
			 "ttyE%d", portnr + (boardnr * 64) + 32);
		make_dev(&stl_cdevsw, minor_dev + 32 + 0x10000,
			 UID_ROOT, GID_WHEEL, 0600,
			 "ttyiE%d", portnr + (boardnr * 64) + 32);
		make_dev(&stl_cdevsw, minor_dev + 64 + 0x10000,
			 UID_ROOT, GID_WHEEL, 0600,
			 "ttylE%d", portnr + (boardnr * 64) + 32);
		make_dev(&stl_cdevsw, minor_dev + 128 + 0x10000,
			 UID_ROOT, GID_WHEEL, 0600,
			 "cue%d", portnr + (boardnr * 64) + 32);
		make_dev(&stl_cdevsw, minor_dev + 160 + 0x10000,
			 UID_ROOT, GID_WHEEL, 0600,
			 "cuie%d", portnr + (boardnr * 64) + 32);
		make_dev(&stl_cdevsw, minor_dev + 192 + 0x10000,
			 UID_ROOT, GID_WHEEL, 0600,
			 "cule%d", portnr + (boardnr * 64) + 32);
	}
	boardnr = brdp->brdnr;
	make_dev(&stl_cdevsw, boardnr + 0x1000000, UID_ROOT, GID_WHEEL,
		 0600, "staliomem%d", boardnr);

	for (portnr = 0, minor_dev = boardnr * 0x100000;
	      portnr < 32; portnr++, minor_dev++) {
		/* hw ports */
		make_dev(&stl_cdevsw, minor_dev,
			 UID_ROOT, GID_WHEEL, 0600,
			 "ttyE%d", portnr + (boardnr * 64));
		make_dev(&stl_cdevsw, minor_dev + 32,
			 UID_ROOT, GID_WHEEL, 0600,
			 "ttyiE%d", portnr + (boardnr * 64));
		make_dev(&stl_cdevsw, minor_dev + 64,
			 UID_ROOT, GID_WHEEL, 0600,
			 "ttylE%d", portnr + (boardnr * 64));
		make_dev(&stl_cdevsw, minor_dev + 128,
			 UID_ROOT, GID_WHEEL, 0600,
			 "cue%d", portnr + (boardnr * 64));
		make_dev(&stl_cdevsw, minor_dev + 160,
			 UID_ROOT, GID_WHEEL, 0600,
			 "cuie%d", portnr + (boardnr * 64));
		make_dev(&stl_cdevsw, minor_dev + 192,
			 UID_ROOT, GID_WHEEL, 0600,
			 "cule%d", portnr + (boardnr * 64));

		/* sw ports */
		make_dev(&stl_cdevsw, minor_dev + 0x10000,
			 UID_ROOT, GID_WHEEL, 0600,
			 "ttyE%d", portnr + (boardnr * 64) + 32);
		make_dev(&stl_cdevsw, minor_dev + 32 + 0x10000,
			 UID_ROOT, GID_WHEEL, 0600,
			 "ttyiE%d", portnr + (boardnr * 64) + 32);
		make_dev(&stl_cdevsw, minor_dev + 64 + 0x10000,
			 UID_ROOT, GID_WHEEL, 0600,
			 "ttylE%d", portnr + (boardnr * 64) + 32);
		make_dev(&stl_cdevsw, minor_dev + 128 + 0x10000,
			 UID_ROOT, GID_WHEEL, 0600,
			 "cue%d", portnr + (boardnr * 64) + 32);
		make_dev(&stl_cdevsw, minor_dev + 160 + 0x10000,
			 UID_ROOT, GID_WHEEL, 0600,
			 "cuie%d", portnr + (boardnr * 64) + 32);
		make_dev(&stl_cdevsw, minor_dev + 192 + 0x10000,
			 UID_ROOT, GID_WHEEL, 0600,
			 "cule%d", portnr + (boardnr * 64) + 32);
	}

	return(1);
}

/*****************************************************************************/

#if NPCI > 0

/*
 *	Probe specifically for the PCI boards. We need to be a little
 *	carefull here, since it looks sort like a Nat Semi IDE chip...
 */

static const char *stlpciprobe(pcici_t tag, pcidi_t type)
{
	unsigned long	class;
	int		i, brdtype;

#if STLDEBUG
	printf("stlpciprobe(tag=%x,type=%x)\n", (int) &tag, (int) type);
#endif

	brdtype = 0;
	for (i = 0; (i < stl_nrpcibrds); i++) {
		if (((type & 0xffff) == stl_pcibrds[i].vendid) &&
                    (((type >> 16) & 0xffff) == stl_pcibrds[i].devid)) {
                        brdtype = stl_pcibrds[i].brdtype;
                        break;
                }
        }

        if (brdtype == 0)
                return((char *) NULL);

        class = pci_conf_read(tag, PCI_CLASS_REG);
        if ((class & PCI_CLASS_MASK) == PCI_CLASS_MASS_STORAGE)
                return((char *) NULL);

        return(stl_brdnames[brdtype]);
}

/*****************************************************************************/

/*
 *	Allocate resources for and initialize the specified PCI board.
 */

void stlpciattach(pcici_t tag, int unit)
{
	stlbrd_t	*brdp;
        unsigned int    bar[4];
        unsigned int    id;
        int             i;
	int		boardnr, portnr, minor_dev;

#if STLDEBUG
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

/*
 *      Determine what type of PCI board this is...
 */
        id = (unsigned int) pci_conf_read(tag, 0x0);
        for (i = 0; (i < stl_nrpcibrds); i++) {
                if (((id & 0xffff) == stl_pcibrds[i].vendid) &&
                    (((id >> 16) & 0xffff) == stl_pcibrds[i].devid)) {
                        brdp->brdtype = stl_pcibrds[i].brdtype;
                        break;
                }
        }

        if (i >= stl_nrpcibrds) {
                printf("STALLION: probed PCI board unknown type=%x\n", id);
                return;
        }

        for (i = 0; (i < 4); i++)
                bar[i] = (unsigned int) pci_conf_read(tag, 0x10 + (i * 4)) &
                        0xfffc;

        switch (brdp->brdtype) {
        case BRD_ECH64PCI:
                brdp->ioaddr1 = bar[1];
                brdp->ioaddr2 = bar[2];
                break;
        case BRD_EASYIOPCI:
                brdp->ioaddr1 = bar[2];
                brdp->ioaddr2 = bar[1];
                break;
        case BRD_ECHPCI:
                brdp->ioaddr1 = bar[1];
                brdp->ioaddr2 = bar[0];
                break;
        default:
                printf("STALLION: unknown PCI board type=%d\n", brdp->brdtype);
                return;
                break;
        }

        brdp->unitid = brdp->brdnr; /* PCI units auto-assigned */
        brdp->irq = ((int) pci_conf_read(tag, 0x3c)) & 0xff;
        brdp->irqtype = 0;
        if (pci_map_int(tag, stlpciintr, (void *) NULL, &tty_imask) == 0) {
                printf("STALLION: failed to map interrupt irq=%d for unit=%d\n",
                        brdp->irq, brdp->brdnr);
                return;
        }

        stl_brdinit(brdp);

	/* register devices for DEVFS */
	boardnr = brdp->brdnr;
	make_dev(&stl_cdevsw, boardnr + 0x1000000, UID_ROOT, GID_WHEEL,
		 0600, "staliomem%d", boardnr);

	for (portnr = 0, minor_dev = boardnr * 0x100000;
	      portnr < 32; portnr++, minor_dev++) {
		/* hw ports */
		make_dev(&stl_cdevsw, minor_dev,
			 UID_ROOT, GID_WHEEL, 0600,
			 "ttyE%d", portnr + (boardnr * 64));
		make_dev(&stl_cdevsw, minor_dev + 32,
			 UID_ROOT, GID_WHEEL, 0600,
			 "ttyiE%d", portnr + (boardnr * 64));
		make_dev(&stl_cdevsw, minor_dev + 64,
			 UID_ROOT, GID_WHEEL, 0600,
			 "ttylE%d", portnr + (boardnr * 64));
		make_dev(&stl_cdevsw, minor_dev + 128,
			 UID_ROOT, GID_WHEEL, 0600,
			 "cue%d", portnr + (boardnr * 64));
		make_dev(&stl_cdevsw, minor_dev + 160,
			 UID_ROOT, GID_WHEEL, 0600,
			 "cuie%d", portnr + (boardnr * 64));
		make_dev(&stl_cdevsw, minor_dev + 192,
			 UID_ROOT, GID_WHEEL, 0600,
			 "cule%d", portnr + (boardnr * 64));

		/* sw ports */
		make_dev(&stl_cdevsw, minor_dev + 0x10000,
			 UID_ROOT, GID_WHEEL, 0600,
			 "ttyE%d", portnr + (boardnr * 64) + 32);
		make_dev(&stl_cdevsw, minor_dev + 32 + 0x10000,
			 UID_ROOT, GID_WHEEL, 0600,
			 "ttyiE%d", portnr + (boardnr * 64) + 32);
		make_dev(&stl_cdevsw, minor_dev + 64 + 0x10000,
			 UID_ROOT, GID_WHEEL, 0600,
			 "ttylE%d", portnr + (boardnr * 64) + 32);
		make_dev(&stl_cdevsw, minor_dev + 128 + 0x10000,
			 UID_ROOT, GID_WHEEL, 0600,
			 "cue%d", portnr + (boardnr * 64) + 32);
		make_dev(&stl_cdevsw, minor_dev + 160 + 0x10000,
			 UID_ROOT, GID_WHEEL, 0600,
			 "cuie%d", portnr + (boardnr * 64) + 32);
		make_dev(&stl_cdevsw, minor_dev + 192 + 0x10000,
			 UID_ROOT, GID_WHEEL, 0600,
			 "cule%d", portnr + (boardnr * 64) + 32);
	}
}

#endif

/*****************************************************************************/

STATIC int stlopen(dev_t dev, int flag, int mode, struct proc *p)
{
	struct tty	*tp;
	stlport_t	*portp;
	int		error, callout, x;

#if STLDEBUG
	printf("stlopen(dev=%x,flag=%x,mode=%x,p=%x)\n", (int) dev, flag,
		mode, (int) p);
#endif

/*
 *	Firstly check if the supplied device number is a valid device.
 */
	if (minor(dev) & STL_MEMDEV)
		return(0);

	portp = stl_dev2port(dev);
	if (portp == (stlport_t *) NULL)
		return(ENXIO);
        if (minor(dev) & STL_CTRLDEV)
                return(0);
	tp = &portp->tty;
	dev->si_tty = tp;
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
		tp->t_stop = stl_stop;
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
		if ((tp->t_state & TS_XCLUDE) && suser(p)) {
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

#if STLDEBUG
	printf("stlclose(dev=%s,flag=%x,mode=%x,p=%p)\n", devtoname(dev),
		flag, mode, (void *) p);
#endif

	if (minor(dev) & STL_MEMDEV)
		return(0);
        if (minor(dev) & STL_CTRLDEV)
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

#if VFREEBSD >= 220

STATIC void stl_stop(struct tty *tp, int rw)
{
#if STLDEBUG
	printf("stl_stop(tp=%x,rw=%x)\n", (int) tp, rw);
#endif

	stl_flush((stlport_t *) tp, rw);
}

#else

STATIC int stlstop(struct tty *tp, int rw)
{
#if STLDEBUG
	printf("stlstop(tp=%x,rw=%x)\n", (int) tp, rw);
#endif

	stl_flush((stlport_t *) tp, rw);
	return(0);
}

#endif

/*****************************************************************************/

STATIC int stlioctl(dev_t dev, unsigned long cmd, caddr_t data, int flag,
		    struct proc *p)
{
	struct termios	*newtios, *localtios;
	struct tty	*tp;
	stlport_t	*portp;
	int		error, i, x;

#if STLDEBUG
	printf("stlioctl(dev=%s,cmd=%lx,data=%p,flag=%x,p=%p)\n",
		devtoname(dev), cmd, (void *) data, flag, (void *) p);
#endif

	if (minor(dev) & STL_MEMDEV)
		return(stl_memioctl(dev, cmd, data, flag, p));

	portp = stl_dev2port(dev);
	if (portp == (stlport_t *) NULL)
		return(ENODEV);
	tp = &portp->tty;
	error = 0;
	
/*
 *	First up handle ioctls on the control devices.
 */
	if (minor(dev) & STL_CTRLDEV) {
		if ((minor(dev) & STL_CTRLDEV) == STL_CTRLINIT)
			localtios = (minor(dev) & STL_CALLOUTDEV) ?
				&portp->initouttios : &portp->initintios;
		else if ((minor(dev) & STL_CTRLDEV) == STL_CTRLLOCK)
			localtios = (minor(dev) & STL_CALLOUTDEV) ?
				&portp->lockouttios : &portp->lockintios;
		else
			return(ENODEV);

		switch (cmd) {
		case TIOCSETA:
			if ((error = suser(p)) == 0)
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
		unsigned long	oldcmd;

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
		localtios = (minor(dev) & STL_CALLOUTDEV) ? 
			&portp->lockouttios : &portp->lockintios;

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
	if (error != ENOIOCTL)
		return(error);

	x = spltty();
	error = ttioctl(tp, cmd, data, flag);
	stl_ttyoptim(portp, &tp->t_termios);
	if (error != ENOIOCTL) {
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
		if ((error = suser(p)) == 0)
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
#if STLDEBUG
	printf("stl_rawopen(portp=%p): brdnr=%d panelnr=%d portnr=%d\n",
		(void *) portp, portp->brdnr, portp->panelnr, portp->portnr);
#endif

        stl_setport(portp, &portp->tty.t_termios);
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

#if STLDEBUG
	printf("stl_rawclose(portp=%p): brdnr=%d panelnr=%d portnr=%d\n",
		(void *) portp, portp->brdnr, portp->panelnr, portp->portnr);
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

#if STLDEBUG
	printf("stl_start(tp=%x): brdnr=%d portnr=%d\n", (int) tp, 
		portp->brdnr, portp->portnr);
#endif

	x = spltty();

/*
 *	Check if the ports input has been blocked, and take appropriate action.
 *	Not very often do we really need to do anything, so make it quick.
 */
	if (tp->t_state & TS_TBLOCK) {
                if ((portp->state & ASY_RTSFLOWMODE) &&
                    ((portp->state & ASY_RTSFLOW) == 0))
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

#if STLDEBUG
	printf("stl_flush(portp=%x,flag=%x)\n", (int) portp, flag);
#endif

	if (portp == (stlport_t *) NULL)
		return;

	x = spltty();

	if (flag & FWRITE) {
                stl_uartflush(portp, FWRITE);
                portp->tx.tail = portp->tx.head;
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
 *      Interrupt handler for host based boards. Interrupts for all boards
 *      are vectored through here.
 */

void stlintr(int unit)
{
        stlbrd_t        *brdp;
        int             i;

#if STLDEBUG
        printf("stlintr(unit=%d)\n", unit);
#endif

        for (i = 0; (i < stl_nrbrds); i++) {
                if ((brdp = stl_brds[i]) == (stlbrd_t *) NULL)
                        continue;
                if (brdp->state == 0)
                        continue;
                (* brdp->isr)(brdp);
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
 *      Interrupt service routine for EasyIO boards.
 */

static void stl_eiointr(stlbrd_t *brdp)
{
        stlpanel_t      *panelp;
        int             iobase;

#if STLDEBUG
        printf("stl_eiointr(brdp=%p)\n", brdp);
#endif

        panelp = (stlpanel_t *) brdp->panels[0];
        iobase = panelp->iobase;
        while (inb(brdp->iostatus) & EIO_INTRPEND)
                (* panelp->isr)(panelp, iobase);
}

/*
 *      Interrupt service routine for ECH-AT board types.
 */

static void stl_echatintr(stlbrd_t *brdp)
{
        stlpanel_t      *panelp;
        unsigned int    ioaddr;
        int             bnknr;

        outb(brdp->ioctrl, (brdp->ioctrlval | ECH_BRDENABLE));

        while (inb(brdp->iostatus) & ECH_INTRPEND) {
                for (bnknr = 0; (bnknr < brdp->nrbnks); bnknr++) {
                        ioaddr = brdp->bnkstataddr[bnknr];
                        if (inb(ioaddr) & ECH_PNLINTRPEND) {
                                panelp = brdp->bnk2panel[bnknr];
                                (* panelp->isr)(panelp, (ioaddr & 0xfffc));
                        }
                }
        }

        outb(brdp->ioctrl, (brdp->ioctrlval | ECH_BRDDISABLE));
}

/*****************************************************************************/

/*
 *      Interrupt service routine for ECH-MCA board types.
 */

static void stl_echmcaintr(stlbrd_t *brdp)
{
        stlpanel_t      *panelp;
        unsigned int    ioaddr;
        int             bnknr;

        while (inb(brdp->iostatus) & ECH_INTRPEND) {
                for (bnknr = 0; (bnknr < brdp->nrbnks); bnknr++) {
                        ioaddr = brdp->bnkstataddr[bnknr];
                        if (inb(ioaddr) & ECH_PNLINTRPEND) {
                                panelp = brdp->bnk2panel[bnknr];
                                (* panelp->isr)(panelp, (ioaddr & 0xfffc));
                        }
                }
        }
}

/*****************************************************************************/

/*
 *      Interrupt service routine for ECH-PCI board types.
 */

static void stl_echpciintr(stlbrd_t *brdp)
{
        stlpanel_t      *panelp;
        unsigned int    ioaddr;
        int             bnknr, recheck;

#if STLDEBUG
        printf("stl_echpciintr(brdp=%x)\n", (int) brdp);
#endif

        for (;;) {
                recheck = 0;
                for (bnknr = 0; (bnknr < brdp->nrbnks); bnknr++) {
                        outb(brdp->ioctrl, brdp->bnkpageaddr[bnknr]);
                        ioaddr = brdp->bnkstataddr[bnknr];
                        if (inb(ioaddr) & ECH_PNLINTRPEND) {
                                panelp = brdp->bnk2panel[bnknr];
                                (* panelp->isr)(panelp, (ioaddr & 0xfffc));
                                recheck++;
                        }
                }
                if (! recheck)
                        break;
        }
}

/*****************************************************************************/

/*
 *      Interrupt service routine for EC8/64-PCI board types.
 */

static void stl_echpci64intr(stlbrd_t *brdp)
{
        stlpanel_t      *panelp;
        unsigned int    ioaddr;
        int             bnknr;

#if STLDEBUG
        printf("stl_echpci64intr(brdp=%p)\n", brdp);
#endif

        while (inb(brdp->ioctrl) & 0x1) {
                for (bnknr = 0; (bnknr < brdp->nrbnks); bnknr++) {
                        ioaddr = brdp->bnkstataddr[bnknr];
#if STLDEBUG
	printf("    --> ioaddr=%x status=%x(%x)\n", ioaddr, inb(ioaddr) & ECH_PNLINTRPEND, inb(ioaddr));
#endif
                        if (inb(ioaddr) & ECH_PNLINTRPEND) {
                                panelp = brdp->bnk2panel[bnknr];
                                (* panelp->isr)(panelp, (ioaddr & 0xfffc));
                        }
                }
        }
}

/*****************************************************************************/

/*
 *	If we haven't scheduled a timeout then do it, some port needs high
 *	level processing.
 */

static void stl_dotimeout()
{
#if STLDEBUG
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

#if STLDEBUG
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
                        if (portp->state & ASY_TXEMPTY) {
                                if (stl_datastate(portp) == 0) {
                                        portp->state &= ~ASY_TXEMPTY;
                                        tp->t_state &= ~TS_BUSY;
                                        (*linesw[tp->t_line].l_start)(tp);
                                }
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

#if STLDEBUG
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
			status = *(portp->rx.tail + STL_RXBUFSIZE);
			if (status) {
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
 *	If we were flow controled then maybe the buffer is low enough that
 *	we can re-activate it.
 */
	if ((portp->state & ASY_RTSFLOW) && ((tp->t_state & TS_TBLOCK) == 0))
		stl_flowcontrol(portp, 1, -1);
}

/*****************************************************************************/

static int stl_param(struct tty *tp, struct termios *tiosp)
{
        stlport_t       *portp;

        portp = (stlport_t *) tp;
        if (portp == (stlport_t *) NULL)
                return(ENODEV);

        return(stl_setport(portp, tiosp));
}

/*****************************************************************************/

/*
 *	Action the flow control as required. The hw and sw args inform the
 *	routine what flow control methods it should try.
 */

static void stl_flowcontrol(stlport_t *portp, int hw, int sw)
{
	unsigned char	*head, *tail;
	int		len, hwflow;

#if STLDEBUG
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
        stl_sendflow(portp, hwflow, sw);
}

/*****************************************************************************/

/*
 *	Enable l_rint processing bypass mode if tty modes allow it.
 */

static void stl_ttyoptim(stlport_t *portp, struct termios *tiosp)
{
	struct tty	*tp;

	tp = &portp->tty;
	if (((tiosp->c_iflag &
	      (ICRNL | IGNCR | IMAXBEL | INLCR | ISTRIP)) == 0) &&
	    (((tiosp->c_iflag & BRKINT) == 0) || (tiosp->c_iflag & IGNBRK)) &&
	    (((tiosp->c_iflag & PARMRK) == 0) ||
		((tiosp->c_iflag & (IGNPAR | IGNBRK)) == (IGNPAR | IGNBRK))) &&
	    ((tiosp->c_lflag & (ECHO | ICANON | IEXTEN | ISIG | PENDIN)) ==0) &&
	    (linesw[tp->t_line].l_rint == ttyinput))
		tp->t_state |= TS_CAN_BYPASS_L_RINT;
	else
		tp->t_state &= ~TS_CAN_BYPASS_L_RINT;
	portp->hotchar = linesw[tp->t_line].l_hotchar;
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
	int		i, j;

#if STLDEBUG
	printf("stl_initports(panelp=%x)\n", (int) panelp);
#endif

        chipmask = stl_panelinit(brdp, panelp);

/*
 *      All UART's are initialized if found. Now go through and setup
 *      each ports data structures. Also initialize each individual
 *      UART port.
 */
        for (i = 0; (i < panelp->nrports); i++) {
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
                portp->uartp = panelp->uartp;
                portp->clk = brdp->clk;
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

                stl_portinit(brdp, panelp, portp);
        }

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

#if STLDEBUG
	printf("stl_initeio(brdp=%x)\n", (int) brdp);
#endif

	brdp->ioctrl = brdp->ioaddr1 + 1;
	brdp->iostatus = brdp->ioaddr1 + 2;
	brdp->clk = EIO_CLK;
        brdp->isr = stl_eiointr;

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
        case EIO_MK3:
                switch (status & EIO_BRDMASK) {
                case ID_BRD4:
                        brdp->nrports = 4;
                        break;
                case ID_BRD8:
                        brdp->nrports = 8;
                        break;
                case ID_BRD16:
                        brdp->nrports = 16;
                        break;
                default:
                        return(ENODEV);
                }
                brdp->ioctrl++;
                break;
	default:
		return(ENODEV);
	}

        if (brdp->brdtype == BRD_EASYIOPCI) {
                outb((brdp->ioaddr2 + 0x4c), 0x41);
        } else {
/*
 *	Check that the supplied IRQ is good and then use it to setup the
 *	programmable interrupt bits on EIO board. Also set the edge/level
 *	triggered interrupt bit.
 */
		if ((brdp->irq < 0) || (brdp->irq > 15) ||
			(stl_vecmap[brdp->irq] == (unsigned char) 0xff)) {
			printf("STALLION: invalid irq=%d for brd=%d\n",
			       brdp->irq, brdp->brdnr);
			return(EINVAL);
		}
		outb(brdp->ioctrl, (stl_vecmap[brdp->irq] |
			((brdp->irqtype) ? EIO_INTLEVEL : EIO_INTEDGE)));
	}

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
        if ((status & EIO_IDBITMASK) == EIO_MK3) {
                panelp->uartp = (void *) &stl_sc26198uart;
                panelp->isr = stl_sc26198intr;
        } else {
                panelp->uartp = (void *) &stl_cd1400uart;
                panelp->isr = stl_cd1400eiointr;
        }
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
	int		panelnr, ioaddr, banknr, i;

#if STLDEBUG
	printf("stl_initech(brdp=%x)\n", (int) brdp);
#endif

/*
 *	Set up the initial board register contents for boards. This varys a
 *	bit between the different board types. So we need to handle each
 *	separately. Also do a check that the supplied IRQ is good.
 */
        switch (brdp->brdtype) {

        case BRD_ECH:
                brdp->isr = stl_echatintr;
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
                break;

        case BRD_ECHMC:
                brdp->isr = stl_echmcaintr;
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
                break;

        case BRD_ECHPCI:
                brdp->isr = stl_echpciintr;
                brdp->ioctrl = brdp->ioaddr1 + 2;
                break;

        case BRD_ECH64PCI:
                brdp->isr = stl_echpci64intr;
                brdp->ioctrl = brdp->ioaddr2 + 0x40;
                outb((brdp->ioaddr1 + 0x4c), 0x43);
                break;

        default:
                printf("STALLION: unknown board type=%d\n", brdp->brdtype);
                break;
        }

	brdp->clk = ECH_CLK;

/*
 *	Scan through the secondary io address space looking for panels.
 *	As we find'em allocate and initialize panel structures for each.
 */
	ioaddr = brdp->ioaddr2;
	panelnr = 0;
	nxtid = 0;
	banknr = 0;

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
                brdp->bnk2panel[banknr] = panelp;
                brdp->bnkpageaddr[banknr] = nxtid;
                brdp->bnkstataddr[banknr++] = ioaddr + ECH_PNLSTATUS;

                if (status & ECH_PNLXPID) {
                        panelp->uartp = (void *) &stl_sc26198uart;
                        panelp->isr = stl_sc26198intr;
                        if (status & ECH_PNL16PORT) {
                                panelp->nrports = 16;
                                brdp->bnk2panel[banknr] = panelp;
                                brdp->bnkpageaddr[banknr] = nxtid;
                                brdp->bnkstataddr[banknr++] = ioaddr + 4 +
                                        ECH_PNLSTATUS;
                        } else {
                                panelp->nrports = 8;
                        }
                } else {
                        panelp->uartp = (void *) &stl_cd1400uart;
                        panelp->isr = stl_cd1400echintr;
                        if (status & ECH_PNL16PORT) {
                                panelp->nrports = 16;
                                panelp->ackmask = 0x80;
                                if (brdp->brdtype != BRD_ECHPCI)
                                        ioaddr += EREG_BANKSIZE;
                                brdp->bnk2panel[banknr] = panelp;
                                brdp->bnkpageaddr[banknr] = ++nxtid;
                                brdp->bnkstataddr[banknr++] = ioaddr +
                                        ECH_PNLSTATUS;
                        } else {
                                panelp->nrports = 8;
                                panelp->ackmask = 0xc0;
                        }
		}

                nxtid++;
                ioaddr += EREG_BANKSIZE;
                brdp->nrports += panelp->nrports;
                brdp->panels[panelnr++] = panelp;
                if ((brdp->brdtype == BRD_ECH) || (brdp->brdtype == BRD_ECHMC)){
                        if (ioaddr >= (brdp->ioaddr2 + 0x20)) {
                                printf("STALLION: too many ports attached "
                                        "to board %d, remove last module\n",
                                        brdp->brdnr);
                                break;
                        }
                }
	}

        brdp->nrpanels = panelnr;
        brdp->nrbnks = banknr;
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

#if STLDEBUG
	printf("stl_brdinit(brdp=%x): unit=%d type=%d io1=%x io2=%x irq=%d\n",
		(int) brdp, brdp->brdnr, brdp->brdtype, brdp->ioaddr1,
		brdp->ioaddr2, brdp->irq);
#endif

	switch (brdp->brdtype) {
	case BRD_EASYIO:
        case BRD_EASYIOPCI:
		stl_initeio(brdp);
		break;
	case BRD_ECH:
	case BRD_ECHMC:
	case BRD_ECHPCI:
        case BRD_ECH64PCI:
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
			return(ENODEV);
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

static int stl_memioctl(dev_t dev, unsigned long cmd, caddr_t data, int flag,
			struct proc *p)
{
	int		rc;

#if STLDEBUG
	printf("stl_memioctl(dev=%s,cmd=%lx,data=%p,flag=%x)\n",
		devtoname(dev), cmd, (void *) data, flag);
#endif

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

/*****************************************************************************/
/*                         CD1400 UART CODE                                  */
/*****************************************************************************/

/*
 *      These functions get/set/update the registers of the cd1400 UARTs.
 *      Access to the cd1400 registers is via an address/data io port pair.
 */

static int stl_cd1400getreg(stlport_t *portp, int regnr)
{
        outb(portp->ioaddr, (regnr + portp->uartaddr));
        return(inb(portp->ioaddr + EREG_DATA));
}

/*****************************************************************************/

static void stl_cd1400setreg(stlport_t *portp, int regnr, int value)
{
        outb(portp->ioaddr, (regnr + portp->uartaddr));
        outb((portp->ioaddr + EREG_DATA), value);
}

/*****************************************************************************/

static int stl_cd1400updatereg(stlport_t *portp, int regnr, int value)
{
        outb(portp->ioaddr, (regnr + portp->uartaddr));
        if (inb(portp->ioaddr + EREG_DATA) != value) {
                outb((portp->ioaddr + EREG_DATA), value);
                return(1);
        }
        return(0);
}

/*****************************************************************************/

static void stl_cd1400flush(stlport_t *portp, int flag)
{
        int     x;

#if STLDEBUG
        printf("stl_cd1400flush(portp=%x,flag=%x)\n", (int) portp, flag);
#endif

        if (portp == (stlport_t *) NULL)
                return;

        x = spltty();

        if (flag & FWRITE) {
                BRDENABLE(portp->brdnr, portp->pagenr);
                stl_cd1400setreg(portp, CAR, (portp->portnr & 0x03));
                stl_cd1400ccrwait(portp);
                stl_cd1400setreg(portp, CCR, CCR_TXFLUSHFIFO);
                stl_cd1400ccrwait(portp);
                BRDDISABLE(portp->brdnr);
        }

        if (flag & FREAD) {
                /* Hmmm */
        }

        splx(x);
}

/*****************************************************************************/

static void stl_cd1400ccrwait(stlport_t *portp)
{
        int     i;

        for (i = 0; (i < CCR_MAXWAIT); i++) {
                if (stl_cd1400getreg(portp, CCR) == 0)
                        return;
        }

        printf("stl%d: cd1400 device not responding, panel=%d port=%d\n",
            portp->brdnr, portp->panelnr, portp->portnr);
}

/*****************************************************************************/

/*
 *      Transmit interrupt handler. This has gotta be fast!  Handling TX
 *      chars is pretty simple, stuff as many as possible from the TX buffer
 *      into the cd1400 FIFO. Must also handle TX breaks here, since they
 *      are embedded as commands in the data stream. Oh no, had to use a goto!
 */

static __inline void stl_cd1400txisr(stlpanel_t *panelp, int ioaddr)
{
        struct tty      *tp;
        stlport_t       *portp;
        unsigned char   ioack, srer;
        char            *head, *tail;
        int             len, stlen;

#if STLDEBUG
        printf("stl_cd1400txisr(panelp=%x,ioaddr=%x)\n", (int) panelp, ioaddr);
#endif

        ioack = inb(ioaddr + EREG_TXACK);
        if (((ioack & panelp->ackmask) != 0) ||
            ((ioack & ACK_TYPMASK) != ACK_TYPTX)) {
                printf("STALLION: bad TX interrupt ack value=%x\n",
                        ioack);
                return;
        }
        portp = panelp->ports[(ioack >> 3)];
        tp = &portp->tty;

/*
 *      Unfortunately we need to handle breaks in the data stream, since
 *      this is the only way to generate them on the cd1400. Do it now if
 *      a break is to be sent. Some special cases here: brklen is -1 then
 *      start sending an un-timed break, if brklen is -2 then stop sending
 *      an un-timed break, if brklen is -3 then we have just sent an
 *      un-timed break and do not want any data to go out, if brklen is -4
 *      then a break has just completed so clean up the port settings.
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
                        portp->state |= ASY_TXEMPTY;
                        portp->state &= ~ASY_TXBUSY;
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
 *      Receive character interrupt handler. Determine if we have good chars
 *      or bad chars and then process appropriately.
 */

static __inline void stl_cd1400rxisr(stlpanel_t *panelp, int ioaddr)
{
        stlport_t       *portp;
        struct tty      *tp;
        unsigned int    ioack, len, buflen, stlen;
        unsigned char   status;
        char            ch;
        char            *head, *tail;

#if STLDEBUG
        printf("stl_cd1400rxisr(panelp=%x,ioaddr=%x)\n", (int) panelp, ioaddr);
#endif

        ioack = inb(ioaddr + EREG_RXACK);
        if ((ioack & panelp->ackmask) != 0) {
                printf("STALLION: bad RX interrupt ack value=%x\n", ioack);
                return;
        }
        portp = panelp->ports[(ioack >> 3)];
        tp = &portp->tty;

/*
 *      First up, calculate how much room there is in the RX ring queue.
 *      We also want to keep track of the longest possible copy length,
 *      this has to allow for the wrapping of the ring queue.
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
 *      Check if the input buffer is near full. If so then we should take
 *      some flow control action... It is very easy to do hardware and
 *      software flow control from here since we have the port selected on
 *      the UART.
 */
        if (buflen <= (STL_RXBUFSIZE - STL_RXBUFHIGH)) {
                if (((portp->state & ASY_RTSFLOW) == 0) &&
                    (portp->state & ASY_RTSFLOWMODE)) {
                        portp->state |= ASY_RTSFLOW;
                        stl_cd1400setreg(portp, MCOR1,
                                (stl_cd1400getreg(portp, MCOR1) & 0xf0));
                        stl_cd1400setreg(portp, MSVR2, 0);
                        portp->stats.rxrtsoff++;
                }
        }

/*
 *      OK we are set, process good data... If the RX ring queue is full
 *      just chuck the chars - don't leave them in the UART.
 */
        if ((ioack & ACK_TYPMASK) == ACK_TYPRXGOOD) {
                outb(ioaddr, (RDCR + portp->uartaddr));
                len = inb(ioaddr + EREG_DATA);
                if (buflen == 0) {
                        outb(ioaddr, (RDSR + portp->uartaddr));
                        insb((ioaddr + EREG_DATA), &stl_unwanted[0], len);
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
 *      Modem interrupt handler. The is called when the modem signal line
 *      (DCD) has changed state.
 */

static __inline void stl_cd1400mdmisr(stlpanel_t *panelp, int ioaddr)
{
        stlport_t       *portp;
        unsigned int    ioack;
        unsigned char   misr;

#if STLDEBUG
        printf("stl_cd1400mdmisr(panelp=%x,ioaddr=%x)\n", (int) panelp, ioaddr);
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
 *      Interrupt service routine for cd1400 EasyIO boards.
 */

static void stl_cd1400eiointr(stlpanel_t *panelp, unsigned int iobase)
{
        unsigned char   svrtype;

#if STLDEBUG
        printf("stl_cd1400eiointr(panelp=%x,iobase=%x)\n", (int) panelp,
                iobase);
#endif

        outb(iobase, SVRR);
        svrtype = inb(iobase + EREG_DATA);
        if (panelp->nrports > 4) {
                outb(iobase, (SVRR + 0x80));
                svrtype |= inb(iobase + EREG_DATA);
        }
#if STLDEBUG
printf("stl_cd1400eiointr(panelp=%x,iobase=%x): svrr=%x\n", (int) panelp, iobase, svrtype);
#endif

        if (svrtype & SVRR_RX)
                stl_cd1400rxisr(panelp, iobase);
        else if (svrtype & SVRR_TX)
                stl_cd1400txisr(panelp, iobase);
        else if (svrtype & SVRR_MDM)
                stl_cd1400mdmisr(panelp, iobase);
}

/*****************************************************************************/

/*
 *      Interrupt service routine for cd1400 panels.
 */

static void stl_cd1400echintr(stlpanel_t *panelp, unsigned int iobase)
{
        unsigned char   svrtype;

#if STLDEBUG
        printf("stl_cd1400echintr(panelp=%x,iobase=%x)\n", (int) panelp,
                iobase);
#endif

        outb(iobase, SVRR);
        svrtype = inb(iobase + EREG_DATA);
        outb(iobase, (SVRR + 0x80));
        svrtype |= inb(iobase + EREG_DATA);
        if (svrtype & SVRR_RX)
                stl_cd1400rxisr(panelp, iobase);
        else if (svrtype & SVRR_TX)
                stl_cd1400txisr(panelp, iobase);
        else if (svrtype & SVRR_MDM)
                stl_cd1400mdmisr(panelp, iobase);
}

/*****************************************************************************/

/*
 *      Set up the cd1400 registers for a port based on the termios port
 *      settings.
 */

static int stl_cd1400setport(stlport_t *portp, struct termios *tiosp)
{
        unsigned int    clkdiv;
        unsigned char   cor1, cor2, cor3;
        unsigned char   cor4, cor5, ccr;
        unsigned char   srer, sreron, sreroff;
        unsigned char   mcor1, mcor2, rtpr;
        unsigned char   clk, div;
        int             x;

#if STLDEBUG
        printf("stl_cd1400setport(portp=%x,tiosp=%x): brdnr=%d portnr=%d\n",
                (int) portp, (int) tiosp, portp->brdnr, portp->portnr);
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
 *      Set up the RX char ignore mask with those RX error types we
 *      can ignore. We could have used some special modes of the cd1400
 *      UART to help, but it is better this way because we can keep stats
 *      on the number of each type of RX exception event.
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
 *      Go through the char size, parity and stop bits and set all the
 *      option registers appropriately.
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

/*
 *      Set the RX FIFO threshold at 6 chars. This gives a bit of breathing
 *      space for hardware flow control and the like. This should be set to
 *      VMIN. Also here we will set the RX data timeout to 10ms - this should
 *      really be based on VTIME...
 */
        cor3 |= FIFO_RXTHRESHOLD;
        rtpr = 2;

/*
 *      Calculate the baud rate timers. For now we will just assume that
 *      the input and output baud are the same. Could have used a baud
 *      table here, but this way we can generate virtually any baud rate
 *      we like!
 */
        if (tiosp->c_ispeed == 0)
                tiosp->c_ispeed = tiosp->c_ospeed;
        if ((tiosp->c_ospeed < 0) || (tiosp->c_ospeed > CD1400_MAXBAUD))
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
 *      Check what form of modem signaling is required and set it up.
 */
        if ((tiosp->c_cflag & CLOCAL) == 0) {
                mcor1 |= MCOR1_DCD;
                mcor2 |= MCOR2_DCD;
                sreron |= SRER_MODEM;
        }

/*
 *      Setup cd1400 enhanced modes if we can. In particular we want to
 *      handle as much of the flow control as possbile automatically. As
 *      well as saving a few CPU cycles it will also greatly improve flow
 *      control reliablilty.
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
 *      All cd1400 register values calculated so go through and set them
 *      all up.
 */
#if STLDEBUG
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
        stl_cd1400setreg(portp, CAR, (portp->portnr & 0x3));
        srer = stl_cd1400getreg(portp, SRER);
        stl_cd1400setreg(portp, SRER, 0);
        ccr += stl_cd1400updatereg(portp, COR1, cor1);
        ccr += stl_cd1400updatereg(portp, COR2, cor2);
        ccr += stl_cd1400updatereg(portp, COR3, cor3);
        if (ccr) {
                stl_cd1400ccrwait(portp);
                stl_cd1400setreg(portp, CCR, CCR_CORCHANGE);
        }
        stl_cd1400setreg(portp, COR4, cor4);
        stl_cd1400setreg(portp, COR5, cor5);
        stl_cd1400setreg(portp, MCOR1, mcor1);
        stl_cd1400setreg(portp, MCOR2, mcor2);
        if (tiosp->c_ospeed == 0) {
                stl_cd1400setreg(portp, MSVR1, 0);
        } else {
                stl_cd1400setreg(portp, MSVR1, MSVR1_DTR);
                stl_cd1400setreg(portp, TCOR, clk);
                stl_cd1400setreg(portp, TBPR, div);
                stl_cd1400setreg(portp, RCOR, clk);
                stl_cd1400setreg(portp, RBPR, div);
        }
        stl_cd1400setreg(portp, SCHR1, tiosp->c_cc[VSTART]);
        stl_cd1400setreg(portp, SCHR2, tiosp->c_cc[VSTOP]);
        stl_cd1400setreg(portp, SCHR3, tiosp->c_cc[VSTART]);
        stl_cd1400setreg(portp, SCHR4, tiosp->c_cc[VSTOP]);
        stl_cd1400setreg(portp, RTPR, rtpr);
        mcor1 = stl_cd1400getreg(portp, MSVR1);
        if (mcor1 & MSVR1_DCD)
                portp->sigs |= TIOCM_CD;
        else
                portp->sigs &= ~TIOCM_CD;
        stl_cd1400setreg(portp, SRER, ((srer & ~sreroff) | sreron));
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
 *      Action the flow control as required. The hw and sw args inform the
 *      routine what flow control methods it should try.
 */

static void stl_cd1400sendflow(stlport_t *portp, int hw, int sw)
{
        int     x;

#if STLDEBUG
        printf("stl_cd1400sendflow(portp=%x,hw=%d,sw=%d)\n",
                (int) portp, hw, sw);
#endif

        x = spltty();
        BRDENABLE(portp->brdnr, portp->pagenr);
        stl_cd1400setreg(portp, CAR, (portp->portnr & 0x03));

        if (sw >= 0) {
                stl_cd1400ccrwait(portp);
                if (sw) {
                        stl_cd1400setreg(portp, CCR, CCR_SENDSCHR2);
                        portp->stats.rxxoff++;
                } else {
                        stl_cd1400setreg(portp, CCR, CCR_SENDSCHR1);
                        portp->stats.rxxon++;
                }
                stl_cd1400ccrwait(portp);
        }

        if (hw == 0) {
                portp->state |= ASY_RTSFLOW;
                stl_cd1400setreg(portp, MCOR1,
                        (stl_cd1400getreg(portp, MCOR1) & 0xf0));
                stl_cd1400setreg(portp, MSVR2, 0);
                portp->stats.rxrtsoff++;
        } else if (hw > 0) {
                portp->state &= ~ASY_RTSFLOW;
                stl_cd1400setreg(portp, MSVR2, MSVR2_RTS);
                stl_cd1400setreg(portp, MCOR1,
                        (stl_cd1400getreg(portp, MCOR1) | FIFO_RTSTHRESHOLD));
                portp->stats.rxrtson++;
        }

        BRDDISABLE(portp->brdnr);
        splx(x);
}

/*****************************************************************************/

/*
 *      Return the current state of data flow on this port. This is only
 *      really interresting when determining if data has fully completed
 *      transmission or not... This is easy for the cd1400, it accurately
 *      maintains the busy port flag.
 */

static int stl_cd1400datastate(stlport_t *portp)
{
#if STLDEBUG
        printf("stl_cd1400datastate(portp=%x)\n", (int) portp);
#endif

        if (portp == (stlport_t *) NULL)
                return(0);

        return((portp->state & ASY_TXBUSY) ? 1 : 0);
}

/*****************************************************************************/

/*
 *      Set the state of the DTR and RTS signals. Got to do some extra
 *      work here to deal hardware flow control.
 */

static void stl_cd1400setsignals(stlport_t *portp, int dtr, int rts)
{
        unsigned char   msvr1, msvr2;
        int             x;

#if STLDEBUG
        printf("stl_cd1400setsignals(portp=%x,dtr=%d,rts=%d)\n", (int) portp,
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
        stl_cd1400setreg(portp, CAR, (portp->portnr & 0x03));
        if (rts >= 0) {
                if (portp->tty.t_cflag & CRTS_IFLOW) {
                        if (rts == 0) {
                                stl_cd1400setreg(portp, MCOR1,
                                      (stl_cd1400getreg(portp, MCOR1) & 0xf0));
                                portp->stats.rxrtsoff++;
                        } else {
                                stl_cd1400setreg(portp, MCOR1,
                                        (stl_cd1400getreg(portp, MCOR1) |
                                        FIFO_RTSTHRESHOLD));
                                portp->stats.rxrtson++;
                        }
                }
                stl_cd1400setreg(portp, MSVR2, msvr2);
        }
        if (dtr >= 0)
                stl_cd1400setreg(portp, MSVR1, msvr1);
        BRDDISABLE(portp->brdnr);
        splx(x);
}

/*****************************************************************************/

/*
 *      Get the state of the signals.
 */

static int stl_cd1400getsignals(stlport_t *portp)
{
        unsigned char   msvr1, msvr2;
        int             sigs, x;

#if STLDEBUG
        printf("stl_cd1400getsignals(portp=%x)\n", (int) portp);
#endif

        x = spltty();
        BRDENABLE(portp->brdnr, portp->pagenr);
        stl_cd1400setreg(portp, CAR, (portp->portnr & 0x3));
        msvr1 = stl_cd1400getreg(portp, MSVR1);
        msvr2 = stl_cd1400getreg(portp, MSVR2);
        BRDDISABLE(portp->brdnr);
        splx(x);

        sigs = 0;
        sigs |= (msvr1 & MSVR1_DCD) ? TIOCM_CD : 0;
        sigs |= (msvr1 & MSVR1_CTS) ? TIOCM_CTS : 0;
        sigs |= (msvr1 & MSVR1_DTR) ? TIOCM_DTR : 0;
        sigs |= (msvr2 & MSVR2_RTS) ? TIOCM_RTS : 0;
#if 0
        sigs |= (msvr1 & MSVR1_RI) ? TIOCM_RI : 0;
        sigs |= (msvr1 & MSVR1_DSR) ? TIOCM_DSR : 0;
#else
        sigs |= TIOCM_DSR;
#endif
        return(sigs);
}

/*****************************************************************************/

/*
 *      Enable or disable the Transmitter and/or Receiver.
 */

static void stl_cd1400enablerxtx(stlport_t *portp, int rx, int tx)
{
        unsigned char   ccr;
        int             x;

#if STLDEBUG
        printf("stl_cd1400enablerxtx(portp=%x,rx=%d,tx=%d)\n",
                (int) portp, rx, tx);
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
        stl_cd1400setreg(portp, CAR, (portp->portnr & 0x03));
        stl_cd1400ccrwait(portp);
        stl_cd1400setreg(portp, CCR, ccr);
        stl_cd1400ccrwait(portp);
        BRDDISABLE(portp->brdnr);
        splx(x);
}

/*****************************************************************************/

/*
 *      Start or stop the Transmitter and/or Receiver.
 */

static void stl_cd1400startrxtx(stlport_t *portp, int rx, int tx)
{
        unsigned char   sreron, sreroff;
        int             x;

#if STLDEBUG
        printf("stl_cd1400startrxtx(portp=%x,rx=%d,tx=%d)\n",
                (int) portp, rx, tx);
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
        stl_cd1400setreg(portp, CAR, (portp->portnr & 0x3));
        stl_cd1400setreg(portp, SRER,
                ((stl_cd1400getreg(portp, SRER) & ~sreroff) | sreron));
        BRDDISABLE(portp->brdnr);
        if (tx > 0) {
                portp->state |= ASY_TXBUSY;
                portp->tty.t_state |= TS_BUSY;
        }
        splx(x);
}

/*****************************************************************************/

/*
 *      Disable all interrupts from this port.
 */

static void stl_cd1400disableintrs(stlport_t *portp)
{
        int     x;

#if STLDEBUG
        printf("stl_cd1400disableintrs(portp=%x)\n", (int) portp);
#endif

        x = spltty();
        BRDENABLE(portp->brdnr, portp->pagenr);
        stl_cd1400setreg(portp, CAR, (portp->portnr & 0x3));
        stl_cd1400setreg(portp, SRER, 0);
        BRDDISABLE(portp->brdnr);
        splx(x);
}

/*****************************************************************************/

static void stl_cd1400sendbreak(stlport_t *portp, long len)
{
        int     x;

#if STLDEBUG
        printf("stl_cd1400sendbreak(portp=%x,len=%d)\n", (int) portp,
                (int) len);
#endif

        x = spltty();
        BRDENABLE(portp->brdnr, portp->pagenr);
        stl_cd1400setreg(portp, CAR, (portp->portnr & 0x3));
        stl_cd1400setreg(portp, COR2,
                (stl_cd1400getreg(portp, COR2) | COR2_ETC));
        stl_cd1400setreg(portp, SRER,
                ((stl_cd1400getreg(portp, SRER) & ~SRER_TXDATA) |
                SRER_TXEMPTY));
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
 *      Try and find and initialize all the ports on a panel. We don't care
 *      what sort of board these ports are on - since the port io registers
 *      are almost identical when dealing with ports.
 */

static void stl_cd1400portinit(stlbrd_t *brdp, stlpanel_t *panelp, stlport_t *portp)
{
#if STLDEBUG
        printf("stl_cd1400portinit(brdp=%x,panelp=%x,portp=%x)\n",
                (int) brdp, (int) panelp, (int) portp);
#endif

        if ((brdp == (stlbrd_t *) NULL) || (panelp == (stlpanel_t *) NULL) ||
            (portp == (stlport_t *) NULL))
                return;

        portp->ioaddr = panelp->iobase + (((brdp->brdtype == BRD_ECHPCI) ||
                (portp->portnr < 8)) ? 0 : EREG_BANKSIZE);
        portp->uartaddr = (portp->portnr & 0x04) << 5;
        portp->pagenr = panelp->pagenr + (portp->portnr >> 3);

        BRDENABLE(portp->brdnr, portp->pagenr);
        stl_cd1400setreg(portp, CAR, (portp->portnr & 0x3));
        stl_cd1400setreg(portp, LIVR, (portp->portnr << 3));
        portp->hwid = stl_cd1400getreg(portp, GFRCR);
        BRDDISABLE(portp->brdnr);
}

/*****************************************************************************/

/*
 *      Inbitialize the UARTs in a panel. We don't care what sort of board
 *      these ports are on - since the port io registers are almost
 *      identical when dealing with ports.
 */

static int stl_cd1400panelinit(stlbrd_t *brdp, stlpanel_t *panelp)
{
        unsigned int    gfrcr;
        int             chipmask, i, j;
        int             nrchips, uartaddr, ioaddr;

#if STLDEBUG
        printf("stl_cd1400panelinit(brdp=%x,panelp=%x)\n", (int) brdp,
                (int) panelp);
#endif

        BRDENABLE(panelp->brdnr, panelp->pagenr);

/*
 *      Check that each chip is present and started up OK.
 */
        chipmask = 0;
        nrchips = panelp->nrports / CD1400_PORTS;
        for (i = 0; (i < nrchips); i++) {
                if (brdp->brdtype == BRD_ECHPCI) {
                        outb((panelp->pagenr + (i >> 1)), brdp->ioctrl);
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
                        if ((gfrcr = inb(ioaddr + EREG_DATA)) != 0)
                                break;
                }
                if ((j >= CCR_MAXWAIT) || (gfrcr < 0x40) || (gfrcr > 0x60)) {
                        printf("STALLION: cd1400 not responding, "
                                "board=%d panel=%d chip=%d\n", panelp->brdnr,
                                panelp->panelnr, i);
                        continue;
                }
                chipmask |= (0x1 << i);
                outb(ioaddr, (PPR + uartaddr));
                outb((ioaddr + EREG_DATA), PPR_SCALAR);
        }


        BRDDISABLE(panelp->brdnr);
        return(chipmask);
}

/*****************************************************************************/
/*                      SC26198 HARDWARE FUNCTIONS                           */
/*****************************************************************************/

/*
 *      These functions get/set/update the registers of the sc26198 UARTs.
 *      Access to the sc26198 registers is via an address/data io port pair.
 *      (Maybe should make this inline...)
 */

static int stl_sc26198getreg(stlport_t *portp, int regnr)
{
        outb((portp->ioaddr + XP_ADDR), (regnr | portp->uartaddr));
        return(inb(portp->ioaddr + XP_DATA));
}

static void stl_sc26198setreg(stlport_t *portp, int regnr, int value)
{
        outb((portp->ioaddr + XP_ADDR), (regnr | portp->uartaddr));
        outb((portp->ioaddr + XP_DATA), value);
}

static int stl_sc26198updatereg(stlport_t *portp, int regnr, int value)
{
        outb((portp->ioaddr + XP_ADDR), (regnr | portp->uartaddr));
        if (inb(portp->ioaddr + XP_DATA) != value) {
                outb((portp->ioaddr + XP_DATA), value);
                return(1);
        }
        return(0);
}

/*****************************************************************************/

/*
 *      Functions to get and set the sc26198 global registers.
 */

static int stl_sc26198getglobreg(stlport_t *portp, int regnr)
{
        outb((portp->ioaddr + XP_ADDR), regnr);
        return(inb(portp->ioaddr + XP_DATA));
}

#if 0
static void stl_sc26198setglobreg(stlport_t *portp, int regnr, int value)
{
        outb((portp->ioaddr + XP_ADDR), regnr);
        outb((portp->ioaddr + XP_DATA), value);
}
#endif

/*****************************************************************************/

/*
 *      Inbitialize the UARTs in a panel. We don't care what sort of board
 *      these ports are on - since the port io registers are almost
 *      identical when dealing with ports.
 */

static int stl_sc26198panelinit(stlbrd_t *brdp, stlpanel_t *panelp)
{
        int     chipmask, i;
        int     nrchips, ioaddr;

#if STLDEBUG
        printf("stl_sc26198panelinit(brdp=%x,panelp=%x)\n", (int) brdp,
                (int) panelp);
#endif

        BRDENABLE(panelp->brdnr, panelp->pagenr);

/*
 *      Check that each chip is present and started up OK.
 */
        chipmask = 0;
        nrchips = (panelp->nrports + 4) / SC26198_PORTS;
        if (brdp->brdtype == BRD_ECHPCI)
                outb(brdp->ioctrl, panelp->pagenr);

        for (i = 0; (i < nrchips); i++) {
                ioaddr = panelp->iobase + (i * 4); 
                outb((ioaddr + XP_ADDR), SCCR);
                outb((ioaddr + XP_DATA), CR_RESETALL);
                outb((ioaddr + XP_ADDR), TSTR);
                if (inb(ioaddr + XP_DATA) != 0) {
                        printf("STALLION: sc26198 not responding, "
                                "board=%d panel=%d chip=%d\n", panelp->brdnr,
                                panelp->panelnr, i);
                        continue;
                }
                chipmask |= (0x1 << i);
                outb((ioaddr + XP_ADDR), GCCR);
                outb((ioaddr + XP_DATA), GCCR_IVRTYPCHANACK);
                outb((ioaddr + XP_ADDR), WDTRCR);
                outb((ioaddr + XP_DATA), 0xff);
        }

        BRDDISABLE(panelp->brdnr);
        return(chipmask);
}

/*****************************************************************************/

/*
 *      Initialize hardware specific port registers.
 */

static void stl_sc26198portinit(stlbrd_t *brdp, stlpanel_t *panelp, stlport_t *portp)
{
#if STLDEBUG
        printf("stl_sc26198portinit(brdp=%x,panelp=%x,portp=%x)\n",
                (int) brdp, (int) panelp, (int) portp);
#endif

        if ((brdp == (stlbrd_t *) NULL) || (panelp == (stlpanel_t *) NULL) ||
            (portp == (stlport_t *) NULL))
                return;

        portp->ioaddr = panelp->iobase + ((portp->portnr < 8) ? 0 : 4);
        portp->uartaddr = (portp->portnr & 0x07) << 4;
        portp->pagenr = panelp->pagenr;
        portp->hwid = 0x1;

        BRDENABLE(portp->brdnr, portp->pagenr);
        stl_sc26198setreg(portp, IOPCR, IOPCR_SETSIGS);
        BRDDISABLE(portp->brdnr);
}

/*****************************************************************************/

/*
 *      Set up the sc26198 registers for a port based on the termios port
 *      settings.
 */

static int stl_sc26198setport(stlport_t *portp, struct termios *tiosp)
{
        unsigned char   mr0, mr1, mr2, clk;
        unsigned char   imron, imroff, iopr, ipr;
        int             x;

#if STLDEBUG
        printf("stl_sc26198setport(portp=%x,tiosp=%x): brdnr=%d portnr=%d\n",
                (int) portp, (int) tiosp, portp->brdnr, portp->portnr);
#endif

        mr0 = 0;
        mr1 = 0;
        mr2 = 0;
        clk = 0;
        iopr = 0;
        imron = 0;
        imroff = 0;

/*
 *      Set up the RX char ignore mask with those RX error types we
 *      can ignore.
 */
        portp->rxignoremsk = 0;
        if (tiosp->c_iflag & IGNPAR)
                portp->rxignoremsk |= (SR_RXPARITY | SR_RXFRAMING |
                        SR_RXOVERRUN);
        if (tiosp->c_iflag & IGNBRK)
                portp->rxignoremsk |= SR_RXBREAK;

        portp->rxmarkmsk = SR_RXOVERRUN;
        if (tiosp->c_iflag & (INPCK | PARMRK))
                portp->rxmarkmsk |= (SR_RXPARITY | SR_RXFRAMING);
        if (tiosp->c_iflag & BRKINT)
                portp->rxmarkmsk |= SR_RXBREAK;

/*
 *      Go through the char size, parity and stop bits and set all the
 *      option registers appropriately.
 */
        switch (tiosp->c_cflag & CSIZE) {
        case CS5:
                mr1 |= MR1_CS5;
                break;
        case CS6:
                mr1 |= MR1_CS6;
                break;
        case CS7:
                mr1 |= MR1_CS7;
                break;
        default:
                mr1 |= MR1_CS8;
                break;
        }

        if (tiosp->c_cflag & CSTOPB)
                mr2 |= MR2_STOP2;
        else
                mr2 |= MR2_STOP1;

        if (tiosp->c_cflag & PARENB) {
                if (tiosp->c_cflag & PARODD)
                        mr1 |= (MR1_PARENB | MR1_PARODD);
                else
                        mr1 |= (MR1_PARENB | MR1_PAREVEN);
        } else {
                mr1 |= MR1_PARNONE;
        }

        mr1 |= MR1_ERRBLOCK;

/*
 *      Set the RX FIFO threshold at 8 chars. This gives a bit of breathing
 *      space for hardware flow control and the like. This should be set to
 *      VMIN.
 */
        mr2 |= MR2_RXFIFOHALF;

/*
 *      Calculate the baud rate timers. For now we will just assume that
 *      the input and output baud are the same. The sc26198 has a fixed
 *      baud rate table, so only discrete baud rates possible.
 */
        if (tiosp->c_ispeed == 0)
                tiosp->c_ispeed = tiosp->c_ospeed;
        if ((tiosp->c_ospeed < 0) || (tiosp->c_ospeed > SC26198_MAXBAUD))
                return(EINVAL);

        if (tiosp->c_ospeed > 0) {
                for (clk = 0; (clk < SC26198_NRBAUDS); clk++) {
                        if (tiosp->c_ospeed <= sc26198_baudtable[clk])
                                break;
                }
        }

/*
 *      Check what form of modem signaling is required and set it up.
 */
        if ((tiosp->c_cflag & CLOCAL) == 0) {
                iopr |= IOPR_DCDCOS;
                imron |= IR_IOPORT;
        }

/*
 *      Setup sc26198 enhanced modes if we can. In particular we want to
 *      handle as much of the flow control as possible automatically. As
 *      well as saving a few CPU cycles it will also greatly improve flow
 *      control reliability.
 */
        if (tiosp->c_iflag & IXON) {
                mr0 |= MR0_SWFTX | MR0_SWFT;
                imron |= IR_XONXOFF;
        } else {
                imroff |= IR_XONXOFF;
        }
#if 0
        if (tiosp->c_iflag & IXOFF)
                mr0 |= MR0_SWFRX;
#endif

        if (tiosp->c_cflag & CCTS_OFLOW)
                mr2 |= MR2_AUTOCTS;
        if (tiosp->c_cflag & CRTS_IFLOW)
                mr1 |= MR1_AUTORTS;

/*
 *      All sc26198 register values calculated so go through and set
 *      them all up.
 */

#if STLDEBUG
        printf("SETPORT: portnr=%d panelnr=%d brdnr=%d\n", portp->portnr,
                portp->panelnr, portp->brdnr);
        printf("    mr0=%x mr1=%x mr2=%x clk=%x\n", mr0, mr1, mr2, clk);
        printf("    iopr=%x imron=%x imroff=%x\n", iopr, imron, imroff);
        printf("    schr1=%x schr2=%x schr3=%x schr4=%x\n",
                tiosp->c_cc[VSTART], tiosp->c_cc[VSTOP],
                tiosp->c_cc[VSTART], tiosp->c_cc[VSTOP]);
#endif

        x = spltty();
        BRDENABLE(portp->brdnr, portp->pagenr);
        stl_sc26198setreg(portp, IMR, 0);
        stl_sc26198updatereg(portp, MR0, mr0);
        stl_sc26198updatereg(portp, MR1, mr1);
        stl_sc26198setreg(portp, SCCR, CR_RXERRBLOCK);
        stl_sc26198updatereg(portp, MR2, mr2);
        iopr = (stl_sc26198getreg(portp, IOPIOR) & ~IPR_CHANGEMASK) | iopr;
        if (tiosp->c_ospeed == 0) {
                iopr &= ~IPR_DTR;
        } else {
                iopr |= IPR_DTR;
                stl_sc26198setreg(portp, TXCSR, clk);
                stl_sc26198setreg(portp, RXCSR, clk);
        }
        stl_sc26198updatereg(portp, IOPIOR, iopr);
        stl_sc26198setreg(portp, XONCR, tiosp->c_cc[VSTART]);
        stl_sc26198setreg(portp, XOFFCR, tiosp->c_cc[VSTOP]);
        ipr = stl_sc26198getreg(portp, IPR);
        if (ipr & IPR_DCD)
                portp->sigs &= ~TIOCM_CD;
        else
                portp->sigs |= TIOCM_CD;
        portp->imr = (portp->imr & ~imroff) | imron;
        stl_sc26198setreg(portp, IMR, portp->imr);
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
 *      Set the state of the DTR and RTS signals.
 */

static void stl_sc26198setsignals(stlport_t *portp, int dtr, int rts)
{
        unsigned char   iopioron, iopioroff;
        int             x;

#if STLDEBUG
        printf("stl_sc26198setsignals(portp=%x,dtr=%d,rts=%d)\n",
                (int) portp, dtr, rts);
#endif

        iopioron = 0;
        iopioroff = 0;
        if (dtr == 0)
                iopioroff |= IPR_DTR;
        else if (dtr > 0)
                iopioron |= IPR_DTR;
        if (rts == 0)
                iopioroff |= IPR_RTS;
        else if (rts > 0)
                iopioron |= IPR_RTS;

        x = spltty();
        BRDENABLE(portp->brdnr, portp->pagenr);
        if ((rts >= 0) && (portp->tty.t_cflag & CRTS_IFLOW)) {
                if (rts == 0) {
                        stl_sc26198setreg(portp, MR1,
                                (stl_sc26198getreg(portp, MR1) & ~MR1_AUTORTS));
                        portp->stats.rxrtsoff++;
                } else {
                        stl_sc26198setreg(portp, MR1,
                                (stl_sc26198getreg(portp, MR1) | MR1_AUTORTS));
                        portp->stats.rxrtson++;
                }
        }
        stl_sc26198setreg(portp, IOPIOR,
                ((stl_sc26198getreg(portp, IOPIOR) & ~iopioroff) | iopioron));
        BRDDISABLE(portp->brdnr);
        splx(x);
}

/*****************************************************************************/

/*
 *      Return the state of the signals.
 */

static int stl_sc26198getsignals(stlport_t *portp)
{
        unsigned char   ipr;
        int             x, sigs;

#if STLDEBUG
        printf("stl_sc26198getsignals(portp=%x)\n", (int) portp);
#endif

        x = spltty();
        BRDENABLE(portp->brdnr, portp->pagenr);
        ipr = stl_sc26198getreg(portp, IPR);
        BRDDISABLE(portp->brdnr);
        splx(x);

        sigs = TIOCM_DSR;
        sigs |= (ipr & IPR_DCD) ? 0 : TIOCM_CD;
        sigs |= (ipr & IPR_CTS) ? 0 : TIOCM_CTS;
        sigs |= (ipr & IPR_DTR) ? 0: TIOCM_DTR;
        sigs |= (ipr & IPR_RTS) ? 0: TIOCM_RTS;
        return(sigs);
}

/*****************************************************************************/

/*
 *      Enable/Disable the Transmitter and/or Receiver.
 */

static void stl_sc26198enablerxtx(stlport_t *portp, int rx, int tx)
{
        unsigned char   ccr;
        int             x;

#if STLDEBUG
        printf("stl_sc26198enablerxtx(portp=%x,rx=%d,tx=%d)\n",
                (int) portp, rx, tx);
#endif

        ccr = portp->crenable;
        if (tx == 0)
                ccr &= ~CR_TXENABLE;
        else if (tx > 0)
                ccr |= CR_TXENABLE;
        if (rx == 0)
                ccr &= ~CR_RXENABLE;
        else if (rx > 0)
                ccr |= CR_RXENABLE;

        x = spltty();
        BRDENABLE(portp->brdnr, portp->pagenr);
        stl_sc26198setreg(portp, SCCR, ccr);
        BRDDISABLE(portp->brdnr);
        portp->crenable = ccr;
        splx(x);
}

/*****************************************************************************/

/*
 *      Start/stop the Transmitter and/or Receiver.
 */

static void stl_sc26198startrxtx(stlport_t *portp, int rx, int tx)
{
        unsigned char   imr;
        int             x;

#if STLDEBUG
        printf("stl_sc26198startrxtx(portp=%x,rx=%d,tx=%d)\n",
                (int) portp, rx, tx);
#endif

        imr = portp->imr;
        if (tx == 0)
                imr &= ~IR_TXRDY;
        else if (tx == 1)
                imr |= IR_TXRDY;
        if (rx == 0)
                imr &= ~(IR_RXRDY | IR_RXBREAK | IR_RXWATCHDOG);
        else if (rx > 0)
                imr |= IR_RXRDY | IR_RXBREAK | IR_RXWATCHDOG;

        x = spltty();
        BRDENABLE(portp->brdnr, portp->pagenr);
        stl_sc26198setreg(portp, IMR, imr);
        BRDDISABLE(portp->brdnr);
        portp->imr = imr;
        if (tx > 0) {
                portp->state |= ASY_TXBUSY;
                portp->tty.t_state |= TS_BUSY;
        }
        splx(x);
}

/*****************************************************************************/

/*
 *      Disable all interrupts from this port.
 */

static void stl_sc26198disableintrs(stlport_t *portp)
{
        int     x;

#if STLDEBUG
        printf("stl_sc26198disableintrs(portp=%x)\n", (int) portp);
#endif

        x = spltty();
        BRDENABLE(portp->brdnr, portp->pagenr);
        portp->imr = 0;
        stl_sc26198setreg(portp, IMR, 0);
        BRDDISABLE(portp->brdnr);
        splx(x);
}

/*****************************************************************************/

static void stl_sc26198sendbreak(stlport_t *portp, long len)
{
        int     x;

#if STLDEBUG
        printf("stl_sc26198sendbreak(portp=%x,len=%d)\n",
                (int) portp, (int) len);
#endif

        x = spltty();
        BRDENABLE(portp->brdnr, portp->pagenr);
        if (len == -1) {
                stl_sc26198setreg(portp, SCCR, CR_TXSTARTBREAK);
                portp->stats.txbreaks++;
        } else {
                stl_sc26198setreg(portp, SCCR, CR_TXSTOPBREAK);
        }
        BRDDISABLE(portp->brdnr);
        splx(x);
}

/*****************************************************************************/

/*
 *      Take flow control actions...
 */

static void stl_sc26198sendflow(stlport_t *portp, int hw, int sw)
{
        unsigned char   mr0;
        int             x;

#if STLDEBUG
        printf("stl_sc26198sendflow(portp=%x,hw=%d,sw=%d)\n",
                (int) portp, hw, sw);
#endif

        if (portp == (stlport_t *) NULL)
                return;

        x = spltty();
        BRDENABLE(portp->brdnr, portp->pagenr);

        if (sw >= 0) {
                mr0 = stl_sc26198getreg(portp, MR0);
                stl_sc26198setreg(portp, MR0, (mr0 & ~MR0_SWFRXTX));
                if (sw > 0) {
                        stl_sc26198setreg(portp, SCCR, CR_TXSENDXOFF);
                        mr0 &= ~MR0_SWFRX;
                        portp->stats.rxxoff++;
                } else {
                        stl_sc26198setreg(portp, SCCR, CR_TXSENDXON);
                        mr0 |= MR0_SWFRX;
                        portp->stats.rxxon++;
                }
                stl_sc26198wait(portp);
                stl_sc26198setreg(portp, MR0, mr0);
        }

        if (hw == 0) {
                portp->state |= ASY_RTSFLOW;
                stl_sc26198setreg(portp, MR1,
                        (stl_sc26198getreg(portp, MR1) & ~MR1_AUTORTS));
                stl_sc26198setreg(portp, IOPIOR,
                        (stl_sc26198getreg(portp, IOPIOR) & ~IOPR_RTS));
                portp->stats.rxrtsoff++;
        } else if (hw > 0) {
                portp->state &= ~ASY_RTSFLOW;
                stl_sc26198setreg(portp, MR1,
                        (stl_sc26198getreg(portp, MR1) | MR1_AUTORTS));
                stl_sc26198setreg(portp, IOPIOR,
                        (stl_sc26198getreg(portp, IOPIOR) | IOPR_RTS));
                portp->stats.rxrtson++;
        }

        BRDDISABLE(portp->brdnr);
        splx(x);
}

/*****************************************************************************/

/*
 *      Return the current state of data flow on this port. This is only
 *      really interresting when determining if data has fully completed
 *      transmission or not... The sc26198 interrupt scheme cannot
 *      determine when all data has actually drained, so we need to
 *      check the port statusy register to be sure.
 */

static int stl_sc26198datastate(stlport_t *portp)
{
        unsigned char   sr;
        int             x;

#if STLDEBUG
        printf("stl_sc26198datastate(portp=%x)\n", (int) portp);
#endif

        if (portp == (stlport_t *) NULL)
                return(0);
        if (portp->state & ASY_TXBUSY) 
                return(1);

        x = spltty();
        BRDENABLE(portp->brdnr, portp->pagenr);
        sr = stl_sc26198getreg(portp, SR);
        BRDDISABLE(portp->brdnr);
        splx(x);

        return((sr & SR_TXEMPTY) ? 0 : 1);
}

/*****************************************************************************/

static void stl_sc26198flush(stlport_t *portp, int flag)
{
        int     x;

#if STLDEBUG
        printf("stl_sc26198flush(portp=%x,flag=%x)\n", (int) portp, flag);
#endif

        if (portp == (stlport_t *) NULL)
                return;

        x = spltty();
        BRDENABLE(portp->brdnr, portp->pagenr);
        if (flag & FWRITE) {
                stl_sc26198setreg(portp, SCCR, CR_TXRESET);
                stl_sc26198setreg(portp, SCCR, portp->crenable);
        }
        if (flag & FREAD) {
                while (stl_sc26198getreg(portp, SR) & SR_RXRDY)
                        stl_sc26198getreg(portp, RXFIFO);
        }
        BRDDISABLE(portp->brdnr);
        splx(x);
}

/*****************************************************************************/

/*
 *      If we are TX flow controlled and in IXANY mode then we may
 *      need to unflow control here. We gotta do this because of the
 *      automatic flow control modes of the sc26198 - which downs't
 *      support any concept of an IXANY mode.
 */

static void stl_sc26198txunflow(stlport_t *portp)
{
        unsigned char   mr0;

        mr0 = stl_sc26198getreg(portp, MR0);
        stl_sc26198setreg(portp, MR0, (mr0 & ~MR0_SWFRXTX));
        stl_sc26198setreg(portp, SCCR, CR_HOSTXON);
        stl_sc26198setreg(portp, MR0, mr0);
        portp->state &= ~ASY_TXFLOWED;
}

/*****************************************************************************/

/*
 *      Delay for a small amount of time, to give the sc26198 a chance
 *      to process a command...
 */

static void stl_sc26198wait(stlport_t *portp)
{
        int     i;

#if STLDEBUG
        printf("stl_sc26198wait(portp=%x)\n", (int) portp);
#endif

        if (portp == (stlport_t *) NULL)
                return;

        for (i = 0; (i < 20); i++)
                stl_sc26198getglobreg(portp, TSTR);
}

/*****************************************************************************/

/*
 *      Transmit interrupt handler. This has gotta be fast!  Handling TX
 *      chars is pretty simple, stuff as many as possible from the TX buffer
 *      into the sc26198 FIFO.
 */

static __inline void stl_sc26198txisr(stlport_t *portp)
{
        unsigned int    ioaddr;
        unsigned char   mr0;
        char            *head, *tail;
        int             len, stlen;

#if STLDEBUG
        printf("stl_sc26198txisr(portp=%x)\n", (int) portp);
#endif

        ioaddr = portp->ioaddr;

        head = portp->tx.head;
        tail = portp->tx.tail;
        len = (head >= tail) ? (head - tail) : (STL_TXBUFSIZE - (tail - head));
        if ((len == 0) || ((len < STL_TXBUFLOW) &&
            ((portp->state & ASY_TXLOW) == 0))) {
                portp->state |= ASY_TXLOW;
                stl_dotimeout();
        }

        if (len == 0) {
                outb((ioaddr + XP_ADDR), (MR0 | portp->uartaddr));
                mr0 = inb(ioaddr + XP_DATA);
                if ((mr0 & MR0_TXMASK) == MR0_TXEMPTY) {
                        portp->imr &= ~IR_TXRDY;
                        outb((ioaddr + XP_ADDR), (IMR | portp->uartaddr));
                        outb((ioaddr + XP_DATA), portp->imr);
                        portp->state |= ASY_TXEMPTY;
                        portp->state &= ~ASY_TXBUSY;
                } else {
                        mr0 |= ((mr0 & ~MR0_TXMASK) | MR0_TXEMPTY);
                        outb((ioaddr + XP_DATA), mr0);
                }
        } else {
                len = MIN(len, SC26198_TXFIFOSIZE);
                portp->stats.txtotal += len;
                stlen = MIN(len, (portp->tx.endbuf - tail));
                outb((ioaddr + XP_ADDR), GTXFIFO);
                outsb((ioaddr + XP_DATA), tail, stlen);
                len -= stlen;
                tail += stlen;
                if (tail >= portp->tx.endbuf)
                        tail = portp->tx.buf;
                if (len > 0) {
                        outsb((ioaddr + XP_DATA), tail, len);
                        tail += len;
                }
                portp->tx.tail = tail;
        }
}

/*****************************************************************************/

/*
 *      Receive character interrupt handler. Determine if we have good chars
 *      or bad chars and then process appropriately. Good chars are easy
 *      just shove the lot into the RX buffer and set all status byte to 0.
 *      If a bad RX char then process as required. This routine needs to be
 *      fast!
 */

static __inline void stl_sc26198rxisr(stlport_t *portp, unsigned int iack)
{
#if STLDEBUG
        printf("stl_sc26198rxisr(portp=%x,iack=%x)\n", (int) portp, iack);
#endif

        if ((iack & IVR_TYPEMASK) == IVR_RXDATA)
                stl_sc26198rxgoodchars(portp);
        else
                stl_sc26198rxbadchars(portp);

/*
 *      If we are TX flow controlled and in IXANY mode then we may need
 *      to unflow control here. We gotta do this because of the automatic
 *      flow control modes of the sc26198.
 */
        if ((portp->state & ASY_TXFLOWED) && (portp->tty.t_iflag & IXANY))
                stl_sc26198txunflow(portp);
}

/*****************************************************************************/

/*
 *      Process the good received characters from RX FIFO.
 */

static void stl_sc26198rxgoodchars(stlport_t *portp)
{
        unsigned int    ioaddr, len, buflen, stlen;
        char            *head, *tail;

#if STLDEBUG
        printf("stl_sc26198rxgoodchars(port=%x)\n", (int) portp);
#endif

        ioaddr = portp->ioaddr;

/*
 *      First up, calculate how much room there is in the RX ring queue.
 *      We also want to keep track of the longest possible copy length,
 *      this has to allow for the wrapping of the ring queue.
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
 *      Check if the input buffer is near full. If so then we should take
 *      some flow control action... It is very easy to do hardware and
 *      software flow control from here since we have the port selected on
 *      the UART.
 */
        if (buflen <= (STL_RXBUFSIZE - STL_RXBUFHIGH)) {
                if (((portp->state & ASY_RTSFLOW) == 0) &&
                    (portp->state & ASY_RTSFLOWMODE)) {
                        portp->state |= ASY_RTSFLOW;
                        stl_sc26198setreg(portp, MR1,
                                (stl_sc26198getreg(portp, MR1) & ~MR1_AUTORTS));
                        stl_sc26198setreg(portp, IOPIOR,
                                (stl_sc26198getreg(portp, IOPIOR) & ~IOPR_RTS));
                        portp->stats.rxrtsoff++;
                }
        }

/*
 *      OK we are set, process good data... If the RX ring queue is full
 *      just chuck the chars - don't leave them in the UART.
 */
        outb((ioaddr + XP_ADDR), GIBCR);
        len = inb(ioaddr + XP_DATA) + 1;
        if (buflen == 0) {
                outb((ioaddr + XP_ADDR), GRXFIFO);
                insb((ioaddr + XP_DATA), &stl_unwanted[0], len);
                portp->stats.rxlost += len;
                portp->stats.rxtotal += len;
        } else {
                len = MIN(len, buflen);
                portp->stats.rxtotal += len;
                stlen = MIN(len, stlen);
                if (len > 0) {
                        outb((ioaddr + XP_ADDR), GRXFIFO);
                        insb((ioaddr + XP_DATA), head, stlen);
                        head += stlen;
                        if (head >= portp->rx.endbuf) {
                                head = portp->rx.buf;
                                len -= stlen;
                                insb((ioaddr + XP_DATA), head, len);
                                head += len;
                        }
                }
        }

        portp->rx.head = head;
        portp->state |= ASY_RXDATA;
        stl_dotimeout();
}

/*****************************************************************************/

/*
 *      Process all characters in the RX FIFO of the UART. Check all char
 *      status bytes as well, and process as required. We need to check
 *      all bytes in the FIFO, in case some more enter the FIFO while we
 *      are here. To get the exact character error type we need to switch
 *      into CHAR error mode (that is why we need to make sure we empty
 *      the FIFO).
 */

static void stl_sc26198rxbadchars(stlport_t *portp)
{
        unsigned char   mr1;
        unsigned int    status;
        char            *head, *tail;
        char            ch;
        int             len;

/*
 *      First up, calculate how much room there is in the RX ring queue.
 *      We also want to keep track of the longest possible copy length,
 *      this has to allow for the wrapping of the ring queue.
 */
        head = portp->rx.head;
        tail = portp->rx.tail;
        len = (head >= tail) ? (STL_RXBUFSIZE - (head - tail) - 1) :
                (tail - head - 1);

/*
 *      To get the precise error type for each character we must switch
 *      back into CHAR error mode.
 */
        mr1 = stl_sc26198getreg(portp, MR1);
        stl_sc26198setreg(portp, MR1, (mr1 & ~MR1_ERRBLOCK));

        while ((status = stl_sc26198getreg(portp, SR)) & SR_RXRDY) {
                stl_sc26198setreg(portp, SCCR, CR_CLEARRXERR);
                ch = stl_sc26198getreg(portp, RXFIFO);

                if (status & SR_RXBREAK)
                        portp->stats.rxbreaks++;
                if (status & SR_RXFRAMING)
                        portp->stats.rxframing++;
                if (status & SR_RXPARITY)
                        portp->stats.rxparity++;
                if (status & SR_RXOVERRUN)
                        portp->stats.rxoverrun++;
                if ((portp->rxignoremsk & status) == 0) {
                        if ((portp->tty.t_state & TS_CAN_BYPASS_L_RINT) &&
                            ((status & SR_RXFRAMING) ||
                            ((status & SR_RXPARITY) &&
                            (portp->tty.t_iflag & INPCK))))
                                ch = 0;
                        if ((portp->rxmarkmsk & status) == 0)
                                status = 0;
                        if (len > 0) {
                                *(head + STL_RXBUFSIZE) = status;
                                *head++ = ch;
                                if (head >= portp->rx.endbuf)
                                        head = portp->rx.buf;
                                len--;
                        }
                }
        }

/*
 *      To get correct interrupt class we must switch back into BLOCK
 *      error mode.
 */
        stl_sc26198setreg(portp, MR1, mr1);

        portp->rx.head = head;
        portp->state |= ASY_RXDATA;
        stl_dotimeout();
}

/*****************************************************************************/

/*
 *      Other interrupt handler. This includes modem signals, flow
 *      control actions, etc.
 */

static void stl_sc26198otherisr(stlport_t *portp, unsigned int iack)
{
        unsigned char   cir, ipr, xisr;

#if STLDEBUG
        printf("stl_sc26198otherisr(portp=%x,iack=%x)\n", (int) portp, iack);
#endif

        cir = stl_sc26198getglobreg(portp, CIR);

        switch (cir & CIR_SUBTYPEMASK) {
        case CIR_SUBCOS:
                ipr = stl_sc26198getreg(portp, IPR);
                if (ipr & IPR_DCDCHANGE) {
                        portp->state |= ASY_DCDCHANGE;
                        portp->stats.modem++;
                        stl_dotimeout();
                }
                break;
        case CIR_SUBXONXOFF:
                xisr = stl_sc26198getreg(portp, XISR);
                if (xisr & XISR_RXXONGOT) {
                        portp->state |= ASY_TXFLOWED;
                        portp->stats.txxoff++;
                }
                if (xisr & XISR_RXXOFFGOT) {
                        portp->state &= ~ASY_TXFLOWED;
                        portp->stats.txxon++;
                }
                break;
        case CIR_SUBBREAK:
                stl_sc26198setreg(portp, SCCR, CR_BREAKRESET);
                stl_sc26198rxbadchars(portp);
                break;
        default:
                break;
        }
}

/*****************************************************************************/

/*
 *      Interrupt service routine for sc26198 panels.
 */

static void stl_sc26198intr(stlpanel_t *panelp, unsigned int iobase)
{
        stlport_t       *portp;
        unsigned int    iack;

/* 
 *      Work around bug in sc26198 chip... Cannot have A6 address
 *      line of UART high, else iack will be returned as 0.
 */
        outb((iobase + 1), 0);

        iack = inb(iobase + XP_IACK);
#if STLDEBUG
	printf("stl_sc26198intr(panelp=%p,iobase=%x): iack=%x\n", panelp, iobase, iack);
#endif
        portp = panelp->ports[(iack & IVR_CHANMASK) + ((iobase & 0x4) << 1)];

        if (iack & IVR_RXDATA)
                stl_sc26198rxisr(portp, iack);
        else if (iack & IVR_TXDATA)
                stl_sc26198txisr(portp);
        else
                stl_sc26198otherisr(portp, iack);
}

/*****************************************************************************/
