/*****************************************************************************/

/*
 * istallion.c  -- stallion intelligent multiport serial driver.
 *
 * Copyright (c) 1994-1996 Greg Ungerer (gerg@stallion.oz.au).
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
 */

/*****************************************************************************/

#define	TTYDEFCHARS	1

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <sys/syslog.h>
#include <sys/devconf.h>
#include <machine/cpu.h>
#include <machine/clock.h>
#include <i386/isa/isa.h>
#include <i386/isa/isa_device.h>
#include <i386/isa/cdk.h>
#include <i386/isa/comstats.h>

/*****************************************************************************/

/*
 *	Define the version level of the kernel - so we can compile in the
 *	appropriate bits of code. By default this will compile for a 2.1
 *	level kernel.
 */
#define	VFREEBSD	210

#if VFREEBSD >= 220
#define	STATIC		static
#else
#define	STATIC
#endif

/*****************************************************************************/

/*
 *	Define different board types. Not all of the following board types
 *	are supported by this driver. But I will use the standard "assigned"
 *	board numbers. Currently supported boards are abbreviated as:
 *	ECP = EasyConnection 8/64, ONB = ONboard, BBY = Brumby and
 *	STAL = Stallion.
 */
#define	BRD_UNKNOWN	0
#define	BRD_STALLION	1
#define	BRD_BRUMBY4	2
#define	BRD_ONBOARD2	3
#define	BRD_ONBOARD	4
#define	BRD_BRUMBY8	5
#define	BRD_BRUMBY16	6
#define	BRD_ONBOARDE	7
#define	BRD_ONBOARD32	9
#define	BRD_ONBOARD2_32	10
#define	BRD_ONBOARDRS	11
#define	BRD_EASYIO	20
#define	BRD_ECH		21
#define	BRD_ECHMC	22
#define	BRD_ECP		23
#define BRD_ECPE	24
#define	BRD_ECPMC	25
#define	BRD_ECHPCI	26

#define	BRD_BRUMBY	BRD_BRUMBY4

/*****************************************************************************/

/*
 *	Define important driver limitations.
 */
#define	STL_MAXBRDS		8
#define	STL_MAXPANELS		4
#define	STL_PORTSPERPANEL	16
#define	STL_PORTSPERBRD		64

#define	STL_MAXCHANS		STL_PORTSPERBRD


/*
 *	Define the important minor number break down bits. These have been
 *	chosen to be "compatable" with the standard sio driver minor numbers.
 *	Extra high bits are used to distinguish between boards and also for
 *	really high port numbers (> 32).
 */
#define	STL_CALLOUTDEV	0x80
#define	STL_CTRLLOCK	0x40
#define	STL_CTRLINIT	0x20
#define	STL_CTRLDEV	(STL_CTRLLOCK | STL_CTRLINIT)

#define	STL_MEMDEV	0x07000000

#define	STL_DEFSPEED	9600
#define	STL_DEFCFLAG	(CS8 | CREAD | HUPCL)

/*****************************************************************************/

/*
 *	Define our local driver identity first. Set up stuff to deal with
 *	all the local structures required by a serial tty driver.
 */
static char	*stli_drvname = "stli";
static char	*stli_longdrvname = "Stallion Multiport Serial Driver";
static char	*stli_drvversion = "0.0.5";

static int	stli_nrbrds = 0;
static int	stli_doingtimeout = 0;

static char	*__file__ = /*__FILE__*/ "istallion.c";

/*
 *	Define some macros to use to class define boards.
 */
#define	BRD_ISA		0x1
#define	BRD_EISA	0x2
#define	BRD_MCA		0x4
#define	BRD_PCI		0x8

static unsigned char	stli_stliprobed[STL_MAXBRDS];

/*****************************************************************************/

/*
 *	Define a set of structures to hold all the board/panel/port info
 *	for our ports. These will be dynamically allocated as required at
 *	driver initialization time.
 */

/*
 *	Port and board structures to hold status info about each object.
 *	The board structure contains pointers to structures for each port
 *	connected to it. Panels are not distinguished here, since
 *	communication with the slave board will always be on a per port
 *	basis.
 */
typedef struct {
	struct tty	tty;
	int		portnr;
	int		panelnr;
	int		brdnr;
	int		ioaddr;
	int		callout;
	int		devnr;
	int		dtrwait;
	int		dotimestamp;
	int		waitopens;
	int		hotchar;
	int		rc;
	int		argsize;
	void		*argp;
	unsigned int	state;
	unsigned int	sigs;
	struct termios	initintios;
	struct termios	initouttios;
	struct termios	lockintios;
	struct termios	lockouttios;
	struct timeval	timestamp;
	asysigs_t	asig;
	unsigned long	addr;
	unsigned long	rxlost;
	unsigned long	rxoffset;
	unsigned long	txoffset;
	unsigned long	pflag;
	unsigned int	rxsize;
	unsigned int	txsize;
	unsigned char	reqidx;
	unsigned char	reqbit;
	unsigned char	portidx;
	unsigned char	portbit;
} stliport_t;

/*
 *	Use a structure of function pointers to do board level operations.
 *	These include, enable/disable, paging shared memory, interrupting, etc.
 */
typedef struct stlibrd {
	int		brdnr;
	int		brdtype;
	int		unitid;
	int		state;
	int		nrpanels;
	int		nrports;
	int		nrdevs;
	unsigned int	iobase;
	unsigned long	paddr;
	void		*vaddr;
	int		memsize;
	int		pagesize;
	int		hostoffset;
	int		slaveoffset;
	int		bitsize;
	int		confbits;
	void		(*init)(struct stlibrd *brdp);
	void		(*enable)(struct stlibrd *brdp);
	void		(*reenable)(struct stlibrd *brdp);
	void		(*disable)(struct stlibrd *brdp);
	void		(*intr)(struct stlibrd *brdp);
	void		(*reset)(struct stlibrd *brdp);
	char		*(*getmemptr)(struct stlibrd *brdp,
				unsigned long offset, int line);
	int		panels[STL_MAXPANELS];
	int		panelids[STL_MAXPANELS];
	stliport_t	*ports[STL_PORTSPERBRD];
} stlibrd_t;

static stlibrd_t	*stli_brds[STL_MAXBRDS];

static int		stli_shared = 0;

/*
 *	Keep a local char buffer for processing chars into the LD. We
 *	do this to avoid copying from the boards shared memory one char
 *	at a time.
 */
static int		stli_rxtmplen;
static stliport_t	*stli_rxtmpport;
static char		stli_rxtmpbuf[TTYHOG];

/*
 *	Define global stats structures. Not used often, and can be re-used
 *	for each stats call.
 */
static comstats_t	stli_comstats;
static combrd_t		stli_brdstats;
static asystats_t	stli_cdkstats;

/*
 *	Per board state flags. Used with the state field of the board struct.
 *	Not really much here... All we need to do is keep track of whether
 *	the board has been detected, and whether it is actully running a slave
 *	or not.
 */
#define	BST_FOUND	0x1
#define	BST_STARTED	0x2

/*
 *	Define the set of port state flags. These are marked for internal
 *	state purposes only, usually to do with the state of communications
 *	with the slave. They need to be updated atomically.
 */
#define	ST_INITIALIZING	0x1
#define	ST_INITIALIZED	0x2
#define	ST_OPENING	0x4
#define	ST_CLOSING	0x8
#define	ST_CMDING	0x10
#define	ST_RXING	0x20
#define	ST_TXBUSY	0x40
#define	ST_DOFLUSHRX	0x80
#define	ST_DOFLUSHTX	0x100
#define	ST_DOSIGS	0x200
#define	ST_GETSIGS	0x400
#define	ST_DTRWAIT	0x800

/*
 *	Define an array of board names as printable strings. Handy for
 *	referencing boards when printing trace and stuff.
 */
static char	*stli_brdnames[] = {
	"Unknown",
	"Stallion",
	"Brumby",
	"ONboard-MC",
	"ONboard",
	"Brumby",
	"Brumby",
	"ONboard-EI",
	(char *) NULL,
	"ONboard",
	"ONboard-MC",
	"ONboard-MC",
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
	"EC8/64-AT",
	"EC8/64-EI",
	"EC8/64-MC",
	"EC8/32-PCI",
};

/*****************************************************************************/

/*
 *	Hardware configuration info for ECP boards. These defines apply
 *	to the directly accessable io ports of the ECP. There is a set of
 *	defines for each ECP board type, ISA, EISA and MCA.
 */
#define	ECP_IOSIZE	4
#define	ECP_MEMSIZE	(128 * 1024)
#define	ECP_ATPAGESIZE	(4 * 1024)
#define	ECP_EIPAGESIZE	(64 * 1024)
#define	ECP_MCPAGESIZE	(4 * 1024)

#define	STL_EISAID	0x8c4e

/*
 *	Important defines for the ISA class of ECP board.
 */
#define	ECP_ATIREG	0
#define	ECP_ATCONFR	1
#define	ECP_ATMEMAR	2
#define	ECP_ATMEMPR	3
#define	ECP_ATSTOP	0x1
#define	ECP_ATINTENAB	0x10
#define	ECP_ATENABLE	0x20
#define	ECP_ATDISABLE	0x00
#define	ECP_ATADDRMASK	0x3f000
#define	ECP_ATADDRSHFT	12

/*
 *	Important defines for the EISA class of ECP board.
 */
#define	ECP_EIIREG	0
#define	ECP_EIMEMARL	1
#define	ECP_EICONFR	2
#define	ECP_EIMEMARH	3
#define	ECP_EIENABLE	0x1
#define	ECP_EIDISABLE	0x0
#define	ECP_EISTOP	0x4
#define	ECP_EIEDGE	0x00
#define	ECP_EILEVEL	0x80
#define	ECP_EIADDRMASKL	0x00ff0000
#define	ECP_EIADDRSHFTL	16
#define	ECP_EIADDRMASKH	0xff000000
#define	ECP_EIADDRSHFTH	24
#define	ECP_EIBRDENAB	0xc84

#define	ECP_EISAID	0x4

/*
 *	Important defines for the Micro-channel class of ECP board.
 *	(It has a lot in common with the ISA boards.)
 */
#define	ECP_MCIREG	0
#define	ECP_MCCONFR	1
#define	ECP_MCSTOP	0x20
#define	ECP_MCENABLE	0x80
#define	ECP_MCDISABLE	0x00

/*
 *	Hardware configuration info for ONboard and Brumby boards. These
 *	defines apply to the directly accessable io ports of these boards.
 */
#define	ONB_IOSIZE	16
#define	ONB_MEMSIZE	(64 * 1024)
#define	ONB_ATPAGESIZE	(64 * 1024)
#define	ONB_MCPAGESIZE	(64 * 1024)
#define	ONB_EIMEMSIZE	(128 * 1024)
#define	ONB_EIPAGESIZE	(64 * 1024)

/*
 *	Important defines for the ISA class of ONboard board.
 */
#define	ONB_ATIREG	0
#define	ONB_ATMEMAR	1
#define	ONB_ATCONFR	2
#define	ONB_ATSTOP	0x4
#define	ONB_ATENABLE	0x01
#define	ONB_ATDISABLE	0x00
#define	ONB_ATADDRMASK	0xff0000
#define	ONB_ATADDRSHFT	16

#define	ONB_HIMEMENAB	0x02

/*
 *	Important defines for the EISA class of ONboard board.
 */
#define	ONB_EIIREG	0
#define	ONB_EIMEMARL	1
#define	ONB_EICONFR	2
#define	ONB_EIMEMARH	3
#define	ONB_EIENABLE	0x1
#define	ONB_EIDISABLE	0x0
#define	ONB_EISTOP	0x4
#define	ONB_EIEDGE	0x00
#define	ONB_EILEVEL	0x80
#define	ONB_EIADDRMASKL	0x00ff0000
#define	ONB_EIADDRSHFTL	16
#define	ONB_EIADDRMASKH	0xff000000
#define	ONB_EIADDRSHFTH	24
#define	ONB_EIBRDENAB	0xc84

#define	ONB_EISAID	0x1

/*
 *	Important defines for the Brumby boards. They are pretty simple,
 *	there is not much that is programmably configurable.
 */
#define	BBY_IOSIZE	16
#define	BBY_MEMSIZE	(64 * 1024)
#define	BBY_PAGESIZE	(16 * 1024)

#define	BBY_ATIREG	0
#define	BBY_ATCONFR	1
#define	BBY_ATSTOP	0x4

/*
 *	Important defines for the Stallion boards. They are pretty simple,
 *	there is not much that is programmably configurable.
 */
#define	STAL_IOSIZE	16
#define	STAL_MEMSIZE	(64 * 1024)
#define	STAL_PAGESIZE	(64 * 1024)

/*
 *	Define the set of status register values for EasyConnection panels.
 *	The signature will return with the status value for each panel. From
 *	this we can determine what is attached to the board - before we have
 *	actually down loaded any code to it.
 */
#define	ECH_PNLSTATUS	2
#define	ECH_PNL16PORT	0x20
#define	ECH_PNLIDMASK	0x07
#define	ECH_PNLINTRPEND	0x80

/*
 *	Define some macros to do things to the board. Even those these boards
 *	are somewhat related there is often significantly different ways of
 *	doing some operation on it (like enable, paging, reset, etc). So each
 *	board class has a set of functions which do the commonly required
 *	operations. The macros below basically just call these functions,
 *	generally checking for a NULL function - which means that the board
 *	needs nothing done to it to achieve this operation!
 */
#define	EBRDINIT(brdp)					\
	if (brdp->init != NULL)				\
		(* brdp->init)(brdp)

#define	EBRDENABLE(brdp)				\
	if (brdp->enable != NULL)			\
		(* brdp->enable)(brdp);

#define	EBRDDISABLE(brdp)				\
	if (brdp->disable != NULL)			\
		(* brdp->disable)(brdp);

#define	EBRDINTR(brdp)					\
	if (brdp->intr != NULL)				\
		(* brdp->intr)(brdp);

#define	EBRDRESET(brdp)					\
	if (brdp->reset != NULL)			\
		(* brdp->reset)(brdp);

#define	EBRDGETMEMPTR(brdp,offset)			\
	(* brdp->getmemptr)(brdp, offset, __LINE__)

/*
 *	Define the maximal baud rate.
 */
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
int	stliprobe(struct isa_device *idp);
int	stliattach(struct isa_device *idp);

STATIC	d_open_t	stliopen;
STATIC	d_close_t	stliclose;
STATIC	d_read_t	stliread;
STATIC	d_write_t	stliwrite;
STATIC	d_ioctl_t	stliioctl;
STATIC	d_stop_t	stlistop;

#if VFREEBSD >= 220
STATIC	d_devtotty_t	stlidevtotty;
#else
struct tty		*stlidevtotty(dev_t dev);
#endif

/*
 *	Internal function prototypes.
 */
static stliport_t *stli_dev2port(dev_t dev);
static int	stli_chksharemem(void);
static int	stli_isaprobe(struct isa_device *idp);
static int	stli_eisaprobe(struct isa_device *idp);
static int	stli_mcaprobe(struct isa_device *idp);
static int	stli_brdinit(stlibrd_t *brdp);
static int	stli_brdattach(stlibrd_t *brdp);
static int	stli_initecp(stlibrd_t *brdp);
static int	stli_initonb(stlibrd_t *brdp);
static int	stli_initports(stlibrd_t *brdp);
static int	stli_startbrd(stlibrd_t *brdp);
static void	stli_poll(void *arg);
static void	stli_brdpoll(stlibrd_t *brdp, volatile cdkhdr_t *hdrp);
static int	stli_hostcmd(stlibrd_t *brdp, stliport_t *portp);
static void	stli_dodelaycmd(stliport_t *portp, volatile cdkctrl_t *cp);
static void	stli_mkasysigs(asysigs_t *sp, int dtr, int rts);
static long	stli_mktiocm(unsigned long sigvalue);
static void	stli_rxprocess(stlibrd_t *brdp, stliport_t *portp);
static void	stli_flush(stliport_t *portp, int flag);
static void	stli_start(struct tty *tp);
static int	stli_param(struct tty *tp, struct termios *tiosp);
static void	stli_ttyoptim(stliport_t *portp, struct termios *tiosp);
static void	stli_dtrwakeup(void *arg);
static int	stli_initopen(stliport_t *portp);
static int	stli_shutdownclose(stliport_t *portp);
static int	stli_rawopen(stlibrd_t *brdp, stliport_t *portp,
			unsigned long arg, int wait);
static int	stli_rawclose(stlibrd_t *brdp, stliport_t *portp,
			unsigned long arg, int wait);
static int	stli_cmdwait(stlibrd_t *brdp, stliport_t *portp,
			unsigned long cmd, void *arg, int size, int copyback);
static void	stli_sendcmd(stlibrd_t *brdp, stliport_t *portp,
			unsigned long cmd, void *arg, int size, int copyback);
static void	stli_mkasyport(stliport_t *portp, asyport_t *pp,
			struct termios *tiosp);
static int	stli_memrw(dev_t dev, struct uio *uiop, int flag);
static int	stli_memioctl(dev_t dev, int cmd, caddr_t data, int flag,
			struct proc *p);
static int	stli_getbrdstats(caddr_t data);
static int	stli_getportstats(stliport_t *portp, caddr_t data);
static int	stli_clrportstats(stliport_t *portp, caddr_t data);
static stliport_t *stli_getport(int brdnr, int panelnr, int portnr);

static void	stli_ecpinit(stlibrd_t *brdp);
static void	stli_ecpenable(stlibrd_t *brdp);
static void	stli_ecpdisable(stlibrd_t *brdp);
static void	stli_ecpreset(stlibrd_t *brdp);
static char	*stli_ecpgetmemptr(stlibrd_t *brdp, unsigned long offset,
			int line);
static void	stli_ecpintr(stlibrd_t *brdp);
static void	stli_ecpeiinit(stlibrd_t *brdp);
static void	stli_ecpeienable(stlibrd_t *brdp);
static void	stli_ecpeidisable(stlibrd_t *brdp);
static void	stli_ecpeireset(stlibrd_t *brdp);
static char	*stli_ecpeigetmemptr(stlibrd_t *brdp, unsigned long offset,
			int line);
static void	stli_ecpmcenable(stlibrd_t *brdp);
static void	stli_ecpmcdisable(stlibrd_t *brdp);
static void	stli_ecpmcreset(stlibrd_t *brdp);
static char	*stli_ecpmcgetmemptr(stlibrd_t *brdp, unsigned long offset,
			int line);

static void	stli_onbinit(stlibrd_t *brdp);
static void	stli_onbenable(stlibrd_t *brdp);
static void	stli_onbdisable(stlibrd_t *brdp);
static void	stli_onbreset(stlibrd_t *brdp);
static char	*stli_onbgetmemptr(stlibrd_t *brdp, unsigned long offset,
			int line);
static void	stli_onbeinit(stlibrd_t *brdp);
static void	stli_onbeenable(stlibrd_t *brdp);
static void	stli_onbedisable(stlibrd_t *brdp);
static void	stli_onbereset(stlibrd_t *brdp);
static char	*stli_onbegetmemptr(stlibrd_t *brdp, unsigned long offset,
			int line);
static void	stli_bbyinit(stlibrd_t *brdp);
static void	stli_bbyreset(stlibrd_t *brdp);
static char	*stli_bbygetmemptr(stlibrd_t *brdp, unsigned long offset,
			int line);
static void	stli_stalinit(stlibrd_t *brdp);
static void	stli_stalreset(stlibrd_t *brdp);
static char	*stli_stalgetmemptr(stlibrd_t *brdp, unsigned long offset,
			int line);

/*****************************************************************************/

/*
 *	Declare the driver isa structure.
 */
struct isa_driver	stlidriver = {
	stliprobe, stliattach, "stli"
};

/*****************************************************************************/

#if VFREEBSD >= 220

/*
 *	FreeBSD-2.2+ kernel linkage.
 */

#define	CDEV_MAJOR	72

static struct cdevsw stli_cdevsw = 
	{ stliopen,	stliclose,	stliread,	stliwrite,
	  stliioctl,	stlistop,	noreset,	stlidevtotty,
	  ttselect,	nommap,		NULL,		"stli",
	  NULL,		-1 };

static stli_devsw_installed = 0;

static void stli_drvinit(void *unused)
{
	dev_t	dev;

	if (! stli_devsw_installed ) {
		dev = makedev(CDEV_MAJOR, 0);
		cdevsw_add(&dev, &stli_cdevsw, NULL);
		stli_devsw_installed = 1;
    	}
}

SYSINIT(sidev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,stli_drvinit,NULL)

#endif

/*****************************************************************************/

static stlibrd_t *stli_brdalloc()
{
	stlibrd_t	*brdp;

	brdp = (stlibrd_t *) malloc(sizeof(stlibrd_t), M_TTYS, M_NOWAIT);
	if (brdp == (stlibrd_t *) NULL) {
		printf("STALLION: failed to allocate memory (size=%d)\n",
			sizeof(stlibrd_t));
		return((stlibrd_t *) NULL);
	}
	bzero(brdp, sizeof(stlibrd_t));
	return(brdp);
}

/*****************************************************************************/

/*
 *	Find an available internal board number (unit number). The problem
 *	is that the same unit numbers can be assigned to different class
 *	boards - but we only want to maintain one setup board structures.
 */

static int stli_findfreeunit()
{
	int	i;

	for (i = 0; (i < STL_MAXBRDS); i++)
		if (stli_brds[i] == (stlibrd_t *) NULL)
			break;
	return((i >= STL_MAXBRDS) ? -1 : i);
}

/*****************************************************************************/

/*
 *	Try and determine the ISA board type. Hopefully the board
 *	configuration entry will help us out, using the flags field.
 *	If not, we may ne be able to determine the board type...
 */

static int stli_isaprobe(struct isa_device *idp)
{
	int	btype;

#if DEBUG
	printf("stli_isaprobe(idp=%x): unit=%d iobase=%x flags=%x\n",
		(int) idp, idp->id_unit, idp->id_iobase, idp->id_flags);
#endif

	switch (idp->id_flags) {
	case BRD_STALLION:
	case BRD_BRUMBY4:
	case BRD_BRUMBY8:
	case BRD_BRUMBY16:
	case BRD_ONBOARD:
	case BRD_ONBOARD32:
	case BRD_ECP:
		btype = idp->id_flags;
		break;
	default:
		btype = 0;
		break;
	}
	return(btype);
}

/*****************************************************************************/

/*
 *	Probe for an EISA board type. We should be able to read the EISA ID,
 *	that will tell us if a board is present or not...
 */

static int stli_eisaprobe(struct isa_device *idp)
{
	int	btype, eid;

#if DEBUG
	printf("stli_eisaprobe(idp=%x): unit=%d iobase=%x flags=%x\n",
		(int) idp, idp->id_unit, idp->id_iobase, idp->id_flags);
#endif

/*
 *	Firstly check if this is an EISA system. Do this by probing for
 *	the system board EISA ID. If this is not an EISA system then
 *	don't bother going any further!
 */
	outb(0xc80, 0xff);
	if (inb(0xc80) == 0xff)
		return(0);

/*
 *	Try and read the EISA ID from the board at specified address.
 *	If one is present it will tell us the board type as well.
 */
	outb((idp->id_iobase + 0xc80), 0xff);
	eid = inb(idp->id_iobase + 0xc80);
	eid |= inb(idp->id_iobase + 0xc81) << 8;
	if (eid != STL_EISAID)
		return(0);

	btype = 0;
	eid = inb(idp->id_iobase + 0xc82);
	if (eid == ECP_EISAID)
		btype = BRD_ECPE;
	else if (eid == ONB_EISAID)
		btype = BRD_ONBOARDE;

	outb((idp->id_iobase + 0xc84), 0x1);
	return(btype);
}

/*****************************************************************************/

/*
 *	Probe for an MCA board type. Not really sure how to do this yet,
 *	so for now just use the supplied flag specifier as board type...
 */

static int stli_mcaprobe(struct isa_device *idp)
{
	int	btype;

#if DEBUG
	printf("stli_mcaprobe(idp=%x): unit=%d iobase=%x flags=%x\n",
		(int) idp, idp->id_unit, idp->id_iobase, idp->id_flags);
#endif

	switch (idp->id_flags) {
	case BRD_ONBOARD2:
	case BRD_ONBOARD2_32:
	case BRD_ONBOARDRS:
	case BRD_ECHMC:
	case BRD_ECPMC:
		btype = idp->id_flags;
		break;
	default:
		btype = 0;
		break;
	}
	return(0);
}

/*****************************************************************************/

/*
 *	Probe for a board. This is involved, since we need to enable the
 *	shared memory region to see if the board is really there or not...
 */

int stliprobe(struct isa_device *idp)
{
	stlibrd_t	*brdp;
	int		btype, bclass;

#if DEBUG
	printf("stliprobe(idp=%x): unit=%d iobase=%x flags=%x\n", (int) idp,
		idp->id_unit, idp->id_iobase, idp->id_flags);
#endif

	if (idp->id_unit > STL_MAXBRDS)
		return(0);

/*
 *	First up determine what bus type of board we might be dealing
 *	with. It is easy to separate out the ISA from the EISA and MCA
 *	boards, based on their IO addresses. We may not be able to tell
 *	the EISA and MCA apart on IO address alone...
 */
	bclass = 0;
	if ((idp->id_iobase > 0) && (idp->id_iobase < 0x400)) {
		bclass |= BRD_ISA;
	} else {
		/* ONboard2 range */
		if ((idp->id_iobase >= 0x700) && (idp->id_iobase < 0x900))
			bclass |= BRD_MCA;
		/* EC-MCA ranges */
		if ((idp->id_iobase >= 0x7000) && (idp->id_iobase < 0x7400))
			bclass |= BRD_MCA;
		if ((idp->id_iobase >= 0x8000) && (idp->id_iobase < 0xc000))
			bclass |= BRD_MCA;
		/* EISA board range */
		if ((idp->id_iobase & ~0xf000) == 0)
			bclass |= BRD_EISA;
	}

	if ((bclass == 0) || (idp->id_iobase == 0))
		return(0);

/*
 *	Based on the board bus type, try and figure out what it might be...
 */
	btype = 0;
	if (bclass & BRD_ISA)
		btype = stli_isaprobe(idp);
	if ((btype == 0) && (bclass & BRD_EISA))
		btype = stli_eisaprobe(idp);
	if ((btype == 0) && (bclass & BRD_MCA))
		btype = stli_mcaprobe(idp);
	if (btype == 0)
		return(0);

/*
 *	Go ahead and try probing for the shared memory region now.
 *	This way we will really know if the board is here...
 */
	if ((brdp = stli_brdalloc()) == (stlibrd_t *) NULL)
		return(0);

	brdp->brdnr = stli_findfreeunit();
	brdp->brdtype = btype;
	brdp->unitid = idp->id_unit;
	brdp->iobase = idp->id_iobase;
	brdp->vaddr = idp->id_maddr;
	brdp->paddr = vtophys(idp->id_maddr);

#if DEBUG
	printf("%s(%d): btype=%x unit=%d brd=%d io=%x mem=%x(%x)\n",
		__file__, __LINE__, btype, brdp->unitid, brdp->brdnr,
		brdp->iobase, brdp->paddr, brdp->vaddr);
#endif

	stli_stliprobed[idp->id_unit] = brdp->brdnr;
	stli_brdinit(brdp);
	if ((brdp->state & BST_FOUND) == 0) {
		stli_brds[brdp->brdnr] = (stlibrd_t *) NULL;
		return(0);
	}
	stli_nrbrds++;
	return(1);
}

/*****************************************************************************/

/*
 *	Allocate resources for and initialize a board.
 */

int stliattach(struct isa_device *idp)
{
	stlibrd_t	*brdp;
	int		brdnr;

#if DEBUG
	printf("stliattach(idp=%x): unit=%d iobase=%x\n", idp,
		idp->id_unit, idp->id_iobase);
#endif

	brdnr = stli_stliprobed[idp->id_unit];
	brdp = stli_brds[brdnr];
	if (brdp == (stlibrd_t *) NULL)
		return(0);
	if (brdp->state & BST_FOUND)
		stli_brdattach(brdp);
	return(1);
}


/*****************************************************************************/

STATIC int stliopen(dev_t dev, int flag, int mode, struct proc *p)
{
	struct tty	*tp;
	stlibrd_t	*brdp;
	stliport_t	*portp;
	int		error, callout, x;

#if DEBUG
	printf("stliopen(dev=%x,flag=%x,mode=%x,p=%x)\n", (int) dev, flag,
		mode, (int) p);
#endif

/*
 *	Firstly check if the supplied device number is a valid device.
 */
	if (dev & STL_MEMDEV)
		return(0);

	portp = stli_dev2port(dev);
	if (portp == (stliport_t *) NULL)
		return(ENXIO);
	tp = &portp->tty;
	callout = minor(dev) & STL_CALLOUTDEV;
	error = 0;

	x = spltty();

stliopen_restart:
/*
 *	Wait here for the DTR drop timeout period to expire.
 */
	while (portp->state & ST_DTRWAIT) {
		error = tsleep(&portp->dtrwait, (TTIPRI | PCATCH),
			"stlidtr", 0);
		if (error)
			goto stliopen_end;
	}

/*
 *	If the port is in its raw hardware initialization phase, then
 *	hold up here 'till it is done.
 */
	while (portp->state & (ST_INITIALIZING | ST_CLOSING)) {
		error = tsleep(&portp->state, (TTIPRI | PCATCH),
			"stliraw", 0);
		if (error)
			goto stliopen_end;
	}

/*
 *	We have a valid device, so now we check if it is already open.
 *	If not then initialize the port hardware and set up the tty
 *	struct as required.
 */
	if ((tp->t_state & TS_ISOPEN) == 0) {
		tp->t_oproc = stli_start;
		tp->t_param = stli_param;
		tp->t_dev = dev;
		tp->t_termios = callout ? portp->initouttios :
			portp->initintios;
		stli_initopen(portp);
		wakeup(&portp->state);
		ttsetwater(tp);
		if ((portp->sigs & TIOCM_CD) || callout)
			(*linesw[tp->t_line].l_modem)(tp, 1);
	} else {
		if (callout) {
			if (portp->callout == 0) {
				error = EBUSY;
				goto stliopen_end;
			}
		} else {
			if (portp->callout != 0) {
				if (flag & O_NONBLOCK) {
					error = EBUSY;
					goto stliopen_end;
				}
				error = tsleep(&portp->callout,
					(TTIPRI | PCATCH), "stlicall", 0);
				if (error)
					goto stliopen_end;
				goto stliopen_restart;
			}
		}
		if ((tp->t_state & TS_XCLUDE) && (p->p_ucred->cr_uid != 0)) {
			error = EBUSY;
			goto stliopen_end;
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
		error = tsleep(TSA_CARR_ON(tp), (TTIPRI | PCATCH), "stlidcd",0);
		portp->waitopens--;
		if (error)
			goto stliopen_end;
		goto stliopen_restart;
	}

/*
 *	Open the line discipline.
 */
	error = (*linesw[tp->t_line].l_open)(dev, tp);
	stli_ttyoptim(portp, &tp->t_termios);
	if ((tp->t_state & TS_ISOPEN) && callout)
		portp->callout = 1;

/*
 *	If for any reason we get to here and the port is not actually
 *	open then close of the physical hardware - no point leaving it
 *	active when the open failed...
 */
stliopen_end:
	splx(x);
	if (((tp->t_state & TS_ISOPEN) == 0) && (portp->waitopens == 0))
		stli_shutdownclose(portp);

	return(error);
}

/*****************************************************************************/

STATIC int stliclose(dev_t dev, int flag, int mode, struct proc *p)
{
	struct tty	*tp;
	stliport_t	*portp;
	int		x;

#if DEBUG
	printf("stliclose(dev=%x,flag=%x,mode=%x,p=%x)\n", dev, flag, mode, p);
#endif

	if (dev & STL_MEMDEV)
		return(0);

	portp = stli_dev2port(dev);
	if (portp == (stliport_t *) NULL)
		return(ENXIO);
	tp = &portp->tty;

	x = spltty();
	(*linesw[tp->t_line].l_close)(tp, flag);
	stli_ttyoptim(portp, &tp->t_termios);
	stli_shutdownclose(portp);
	ttyclose(tp);
	splx(x);
	return(0);
}

/*****************************************************************************/

STATIC int stliread(dev_t dev, struct uio *uiop, int flag)
{
	stliport_t	*portp;

#if DEBUG
	printf("stliread(dev=%x,uiop=%x,flag=%x)\n", dev, uiop, flag);
#endif

	if (dev & STL_MEMDEV)
		return(stli_memrw(dev, uiop, flag));

	portp = stli_dev2port(dev);
	if (portp == (stliport_t *) NULL)
		return(ENODEV);
	return((*linesw[portp->tty.t_line].l_read)(&portp->tty, uiop, flag));
}

/*****************************************************************************/

#if VFREEBSD >= 220

STATIC void stlistop(struct tty *tp, int rw)
{
#if DEBUG
	printf("stlistop(tp=%x,rw=%x)\n", (int) tp, rw);
#endif

	stli_flush((stliport_t *) tp, rw);
}

#else

STATIC int stlistop(struct tty *tp, int rw)
{
#if DEBUG
	printf("stlistop(tp=%x,rw=%x)\n", (int) tp, rw);
#endif

	stli_flush((stliport_t *) tp, rw);
	return(0);
}

#endif

/*****************************************************************************/

STATIC struct tty *stlidevtotty(dev_t dev)
{
#if DEBUG
	printf("stlidevtotty(dev=%x)\n", dev);
#endif
	return((struct tty *) stli_dev2port(dev));
}

/*****************************************************************************/

STATIC int stliwrite(dev_t dev, struct uio *uiop, int flag)
{
	stliport_t	*portp;

#if DEBUG
	printf("stliwrite(dev=%x,uiop=%x,flag=%x)\n", dev, uiop, flag);
#endif

	if (dev & STL_MEMDEV)
		return(stli_memrw(dev, uiop, flag));

	portp = stli_dev2port(dev);
	if (portp == (stliport_t *) NULL)
		return(ENODEV);
	return((*linesw[portp->tty.t_line].l_write)(&portp->tty, uiop, flag));
}

/*****************************************************************************/

STATIC int stliioctl(dev_t dev, int cmd, caddr_t data, int flag, struct proc *p)
{
	struct termios	*newtios, *localtios;
	struct tty	*tp;
	stlibrd_t	*brdp;
	stliport_t	*portp;
	long		arg;
	int		error, i, x;

#if DEBUG
	printf("stliioctl(dev=%x,cmd=%x,data=%x,flag=%x,p=%x)\n", dev, cmd,
		data, flag, p);
#endif

	dev = minor(dev);
	if (dev & STL_MEMDEV)
		return(stli_memioctl(dev, cmd, data, flag, p));

	portp = stli_dev2port(dev);
	if (portp == (stliport_t *) NULL)
		return(ENODEV);
	if ((brdp = stli_brds[portp->brdnr]) == (stlibrd_t *) NULL)
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
	stli_ttyoptim(portp, &tp->t_termios);
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
		arg = BREAKON;
		error = stli_cmdwait(brdp, portp, A_BREAK, &arg,
			sizeof(unsigned long), 0);
		break;
	case TIOCCBRK:
		arg = BREAKOFF;
		error = stli_cmdwait(brdp, portp, A_BREAK, &arg,
			sizeof(unsigned long), 0);
		break;
	case TIOCSDTR:
		stli_mkasysigs(&portp->asig, 1, -1);
		error = stli_cmdwait(brdp, portp, A_SETSIGNALS, &portp->asig,
			sizeof(asysigs_t), 0);
		break;
	case TIOCCDTR:
		stli_mkasysigs(&portp->asig, 0, -1);
		error = stli_cmdwait(brdp, portp, A_SETSIGNALS, &portp->asig,
			sizeof(asysigs_t), 0);
		break;
	case TIOCMSET:
		i = *((int *) data);
		stli_mkasysigs(&portp->asig, ((i & TIOCM_DTR) ? 1 : 0),
			((i & TIOCM_RTS) ? 1 : 0));
		error = stli_cmdwait(brdp, portp, A_SETSIGNALS, &portp->asig,
			sizeof(asysigs_t), 0);
		break;
	case TIOCMBIS:
		i = *((int *) data);
		stli_mkasysigs(&portp->asig, ((i & TIOCM_DTR) ? 1 : -1),
			((i & TIOCM_RTS) ? 1 : -1));
		error = stli_cmdwait(brdp, portp, A_SETSIGNALS, &portp->asig,
			sizeof(asysigs_t), 0);
		break;
	case TIOCMBIC:
		i = *((int *) data);
		stli_mkasysigs(&portp->asig, ((i & TIOCM_DTR) ? 0 : -1),
			((i & TIOCM_RTS) ? 0 : -1));
		error = stli_cmdwait(brdp, portp, A_SETSIGNALS, &portp->asig,
			sizeof(asysigs_t), 0);
		break;
	case TIOCMGET:
		if ((error = stli_cmdwait(brdp, portp, A_GETSIGNALS,
				&portp->asig, sizeof(asysigs_t), 1)) < 0)
			break;
		portp->sigs = stli_mktiocm(portp->asig.sigvalue);
		*((int *) data) = (portp->sigs | TIOCM_LE);
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

STATIC stliport_t *stli_dev2port(dev_t dev)
{
	stlibrd_t	*brdp;

	brdp = stli_brds[MKDEV2BRD(dev)];
	if (brdp == (stlibrd_t *) NULL)
		return((stliport_t *) NULL);
	if ((brdp->state & BST_STARTED) == 0)
		return((stliport_t *) NULL);
	return(brdp->ports[MKDEV2PORT(dev)]);
}

/*****************************************************************************/

/*
 *	Carry out first open operations on a port. This involves a number of
 *	commands to be sent to the slave. We need to open the port, set the
 *	notification events, set the initial port settings, get and set the
 *	initial signal values. We sleep and wait in between each one. But
 *	this still all happens pretty quickly.
 */

static int stli_initopen(stliport_t *portp)
{
	stlibrd_t	*brdp;
	asynotify_t	nt;
	asyport_t	aport;
	int		rc;

#if DEBUG
	printf("stli_initopen(portp=%x)\n", (int) portp);
#endif

	if ((brdp = stli_brds[portp->brdnr]) == (stlibrd_t *) NULL)
		return(ENXIO);
	if (portp->state & ST_INITIALIZED)
		return(0);
	portp->state |= ST_INITIALIZED;

	if ((rc = stli_rawopen(brdp, portp, 0, 1)) < 0)
		return(rc);

	bzero(&nt, sizeof(asynotify_t));
	nt.data = (DT_TXLOW | DT_TXEMPTY | DT_RXBUSY | DT_RXBREAK);
	nt.signal = SG_DCD;
	if ((rc = stli_cmdwait(brdp, portp, A_SETNOTIFY, &nt,
	    sizeof(asynotify_t), 0)) < 0)
		return(rc);

	stli_mkasyport(portp, &aport, &portp->tty.t_termios);
	if ((rc = stli_cmdwait(brdp, portp, A_SETPORT, &aport,
	    sizeof(asyport_t), 0)) < 0)
		return(rc);

	portp->state |= ST_GETSIGS;
	if ((rc = stli_cmdwait(brdp, portp, A_GETSIGNALS, &portp->asig,
	    sizeof(asysigs_t), 1)) < 0)
		return(rc);
	if (portp->state & ST_GETSIGS) {
		portp->sigs = stli_mktiocm(portp->asig.sigvalue);
		portp->state &= ~ST_GETSIGS;
	}

	stli_mkasysigs(&portp->asig, 1, 1);
	if ((rc = stli_cmdwait(brdp, portp, A_SETSIGNALS, &portp->asig,
	    sizeof(asysigs_t), 0)) < 0)
		return(rc);

	return(0);
}

/*****************************************************************************/

/*
 *	Shutdown the hardware of a port.
 */

static int stli_shutdownclose(stliport_t *portp)
{
	stlibrd_t	*brdp;
	struct tty	*tp;

#if DEBUG
	printf("stli_shutdownclose(portp=%x): brdnr=%d panelnr=%d portnr=%d\n",
		portp, portp->brdnr, portp->panelnr, portp->portnr);
#endif

	if ((brdp = stli_brds[portp->brdnr]) == (stlibrd_t *) NULL)
		return(ENXIO);

	tp = &portp->tty;
	stli_rawclose(brdp, portp, 0, 0);
	stli_flush(portp, (FWRITE | FREAD));
	if (tp->t_cflag & HUPCL) {
		disable_intr();
		stli_mkasysigs(&portp->asig, 0, 0);
		if (portp->state & ST_CMDING) {
			portp->state |= ST_DOSIGS;
		} else {
			stli_sendcmd(brdp, portp, A_SETSIGNALS,
				&portp->asig, sizeof(asysigs_t), 0);
		}
		enable_intr();
		if (portp->dtrwait != 0) {
			portp->state |= ST_DTRWAIT;
			timeout(stli_dtrwakeup, portp, portp->dtrwait);
		}
	}
	portp->callout = 0;
	portp->state &= ~ST_INITIALIZED;
	wakeup(&portp->callout);
	wakeup(TSA_CARR_ON(tp));
	return(0);
}

/*****************************************************************************/

/*
 *	Clear the DTR waiting flag, and wake up any sleepers waiting for
 *	DTR wait period to finish.
 */

static void stli_dtrwakeup(void *arg)
{
	stliport_t	*portp;

	portp = (stliport_t *) arg;
	portp->state &= ~ST_DTRWAIT;
	wakeup(&portp->dtrwait);
}

/*****************************************************************************/

/*
 *	Send an open message to the slave. This will sleep waiting for the
 *	acknowledgement, so must have user context. We need to co-ordinate
 *	with close events here, since we don't want open and close events
 *	to overlap.
 */

static int stli_rawopen(stlibrd_t *brdp, stliport_t *portp, unsigned long arg, int wait)
{
	volatile cdkhdr_t	*hdrp;
	volatile cdkctrl_t	*cp;
	volatile unsigned char	*bits;
	int			rc;

#if DEBUG
	printf("stli_rawopen(brdp=%x,portp=%x,arg=%x,wait=%d)\n", (int) brdp,
		(int) portp, (int) arg, wait);
#endif

	disable_intr();

/*
 *	Slave is already closing this port. This can happen if a hangup
 *	occurs on this port. So we must wait until it is complete. The
 *	order of opens and closes may not be preserved across shared
 *	memory, so we must wait until it is complete.
 */
	while (portp->state & ST_CLOSING) {
		rc = tsleep(&portp->state, (TTIPRI | PCATCH), "stliraw", 0);
		if (rc) {
			enable_intr();
			return(rc);
		}
	}

/*
 *	Everything is ready now, so write the open message into shared
 *	memory. Once the message is in set the service bits to say that
 *	this port wants service.
 */
	EBRDENABLE(brdp);
	cp = &((volatile cdkasy_t *) EBRDGETMEMPTR(brdp, portp->addr))->ctrl;
	cp->openarg = arg;
	cp->open = 1;
	hdrp = (volatile cdkhdr_t *) EBRDGETMEMPTR(brdp, CDK_CDKADDR);
	bits = ((volatile unsigned char *) hdrp) + brdp->slaveoffset +
		portp->portidx;
	*bits |= portp->portbit;
	EBRDDISABLE(brdp);

	if (wait == 0) {
		enable_intr();
		return(0);
	}

/*
 *	Slave is in action, so now we must wait for the open acknowledgment
 *	to come back.
 */
	rc = 0;
	portp->state |= ST_OPENING;
	while (portp->state & ST_OPENING) {
		rc = tsleep(&portp->state, (TTIPRI | PCATCH), "stliraw", 0);
		if (rc) {
			enable_intr();
			return(rc);
		}
	}
	enable_intr();

	if ((rc == 0) && (portp->rc != 0))
		rc = EIO;
	return(rc);
}

/*****************************************************************************/

/*
 *	Send a close message to the slave. Normally this will sleep waiting
 *	for the acknowledgement, but if wait parameter is 0 it will not. If
 *	wait is true then must have user context (to sleep).
 */

static int stli_rawclose(stlibrd_t *brdp, stliport_t *portp, unsigned long arg, int wait)
{
	volatile cdkhdr_t	*hdrp;
	volatile cdkctrl_t	*cp;
	volatile unsigned char	*bits;
	int			rc;

#if DEBUG
	printf("stli_rawclose(brdp=%x,portp=%x,arg=%x,wait=%d)\n", (int) brdp,
		(int) portp, (int) arg, wait);
#endif

	disable_intr();

/*
 *	Slave is already closing this port. This can happen if a hangup
 *	occurs on this port.
 */
	if (wait) {
		while (portp->state & ST_CLOSING) {
			rc = tsleep(&portp->state, (TTIPRI | PCATCH),
				"stliraw", 0);
			if (rc) {
				enable_intr();
				return(rc);
			}
		}
	}

/*
 *	Write the close command into shared memory.
 */
	EBRDENABLE(brdp);
	cp = &((volatile cdkasy_t *) EBRDGETMEMPTR(brdp, portp->addr))->ctrl;
	cp->closearg = arg;
	cp->close = 1;
	hdrp = (volatile cdkhdr_t *) EBRDGETMEMPTR(brdp, CDK_CDKADDR);
	bits = ((volatile unsigned char *) hdrp) + brdp->slaveoffset +
		portp->portidx;
	*bits |= portp->portbit;
	EBRDDISABLE(brdp);

	portp->state |= ST_CLOSING;
	if (wait == 0) {
		enable_intr();
		return(0);
	}

/*
 *	Slave is in action, so now we must wait for the open acknowledgment
 *	to come back.
 */
	rc = 0;
	while (portp->state & ST_CLOSING) {
		rc = tsleep(&portp->state, (TTIPRI | PCATCH), "stliraw", 0);
		if (rc) {
			enable_intr();
			return(rc);
		}
	}
	enable_intr();

	if ((rc == 0) && (portp->rc != 0))
		rc = EIO;
	return(rc);
}

/*****************************************************************************/

/*
 *	Send a command to the slave and wait for the response. This must
 *	have user context (it sleeps). This routine is generic in that it
 *	can send any type of command. Its purpose is to wait for that command
 *	to complete (as opposed to initiating the command then returning).
 */

static int stli_cmdwait(stlibrd_t *brdp, stliport_t *portp, unsigned long cmd, void *arg, int size, int copyback)
{
	int	rc;

#if DEBUG
	printf("stli_cmdwait(brdp=%x,portp=%x,cmd=%x,arg=%x,size=%d,"
		"copyback=%d)\n", (int) brdp, (int) portp, (int) cmd,
		(int) arg, size, copyback);
#endif

	disable_intr();
	while (portp->state & ST_CMDING) {
		rc = tsleep(&portp->state, (TTIPRI | PCATCH), "stliraw", 0);
		if (rc) {
			enable_intr();
			return(rc);
		}
	}

	stli_sendcmd(brdp, portp, cmd, arg, size, copyback);

	while (portp->state & ST_CMDING) {
		rc = tsleep(&portp->state, (TTIPRI | PCATCH), "stliraw", 0);
		if (rc) {
			enable_intr();
			return(rc);
		}
	}
	enable_intr();

	if (portp->rc != 0)
		return(EIO);
	return(0);
}

/*****************************************************************************/

/*
 *	Start (or continue) the transfer of TX data on this port. If the
 *	port is not currently busy then load up the interrupt ring queue
 *	buffer and kick of the transmitter. If the port is running low on
 *	TX data then refill the ring queue. This routine is also used to
 *	activate input flow control!
 */

static void stli_start(struct tty *tp)
{
	volatile cdkasy_t	*ap;
	volatile cdkhdr_t	*hdrp;
	volatile unsigned char	*bits;
	unsigned char		*shbuf;
	stliport_t		*portp;
	stlibrd_t		*brdp;
	unsigned int		len, stlen, head, tail, size;
	int			count, x;

	portp = (stliport_t *) tp;

#if DEBUG
	printf("stli_start(tp=%x): brdnr=%d portnr=%d\n", (int) tp, 
		portp->brdnr, portp->portnr);
#endif

	x = spltty();

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
 */
	if (tp->t_outq.c_cc != 0) {
		brdp = stli_brds[portp->brdnr];
		if (brdp == (stlibrd_t *) NULL) {
			splx(x);
			return;
		}

		disable_intr();
		EBRDENABLE(brdp);
		ap = (volatile cdkasy_t *) EBRDGETMEMPTR(brdp, portp->addr);
		head = (unsigned int) ap->txq.head;
		tail = (unsigned int) ap->txq.tail;
		if (tail != ((unsigned int) ap->txq.tail))
			tail = (unsigned int) ap->txq.tail;
		size = portp->txsize;
		if (head >= tail) {
			len = size - (head - tail) - 1;
			stlen = size - head;
		} else {
			len = tail - head - 1;
			stlen = len;
		}

		count = 0;
		shbuf = (char *) EBRDGETMEMPTR(brdp, portp->txoffset);

		if (len > 0) {
			stlen = MIN(len, stlen);
			count = q_to_b(&tp->t_outq, (shbuf + head), stlen);
			len -= count;
			head += count;
			if (head >= size) {
				head = 0;
				if (len > 0) {
					stlen = q_to_b(&tp->t_outq, shbuf, len);
					head += stlen;
					count += stlen;
				}
			}
		}

		ap = (volatile cdkasy_t *) EBRDGETMEMPTR(brdp, portp->addr);
		ap->txq.head = head;
		hdrp = (volatile cdkhdr_t *) EBRDGETMEMPTR(brdp, CDK_CDKADDR);
		bits = ((volatile unsigned char *) hdrp) + brdp->slaveoffset +
			portp->portidx;
		*bits |= portp->portbit;
		portp->state |= ST_TXBUSY;
		tp->t_state |= TS_BUSY;

		EBRDDISABLE(brdp);
		enable_intr();
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

/*
 *	Send a new port configuration to the slave.
 */

static int stli_param(struct tty *tp, struct termios *tiosp)
{
	stlibrd_t	*brdp;
	stliport_t	*portp;
	asyport_t	aport;
	int		x, rc;

	portp = (stliport_t *) tp;
	if ((brdp = stli_brds[portp->brdnr]) == (stlibrd_t *) NULL)
		return(ENXIO);

	x = spltty();
	stli_mkasyport(portp, &aport, tiosp);
	/* can we sleep here? */
	rc = stli_cmdwait(brdp, portp, A_SETPORT, &aport, sizeof(asyport_t), 0);
	stli_ttyoptim(portp, tiosp);
	splx(x);
	return(rc);
}

/*****************************************************************************/

/*
 *	Flush characters from the lower buffer. We may not have user context
 *	so we cannot sleep waiting for it to complete. Also we need to check
 *	if there is chars for this port in the TX cook buffer, and flush them
 *	as well.
 */

static void stli_flush(stliport_t *portp, int flag)
{
	stlibrd_t	*brdp;
	unsigned long	ftype;

#if DEBUG
	printf("stli_flush(portp=%x,flag=%x)\n", (int) portp, flag);
#endif

	if (portp == (stliport_t *) NULL)
		return;
	if ((portp->brdnr < 0) || (portp->brdnr >= stli_nrbrds))
		return;
	brdp = stli_brds[portp->brdnr];
	if (brdp == (stlibrd_t *) NULL)
		return;

	disable_intr();
	if (portp->state & ST_CMDING) {
		portp->state |= (flag & FWRITE) ? ST_DOFLUSHTX : 0;
		portp->state |= (flag & FREAD) ? ST_DOFLUSHRX : 0;
	} else {
		ftype = (flag & FWRITE) ? FLUSHTX : 0;
		ftype |= (flag & FREAD) ? FLUSHRX : 0;
		portp->state &= ~(ST_DOFLUSHTX | ST_DOFLUSHRX);
		stli_sendcmd(brdp, portp, A_FLUSH, &ftype,
			sizeof(unsigned long), 0);
	}
	if ((flag & FREAD) && (stli_rxtmpport == portp))
		stli_rxtmplen = 0;
	enable_intr();
}

/*****************************************************************************/

/*
 *	Generic send command routine. This will send a message to the slave,
 *	of the specified type with the specified argument. Must be very
 *	carefull of data that will be copied out from shared memory -
 *	containing command results. The command completion is all done from
 *	a poll routine that does not have user coontext. Therefore you cannot
 *	copy back directly into user space, or to the kernel stack of a
 *	process. This routine does not sleep, so can be called from anywhere,
 *	and must be called with interrupt locks set.
 */

static void stli_sendcmd(stlibrd_t *brdp, stliport_t *portp, unsigned long cmd, void *arg, int size, int copyback)
{
	volatile cdkhdr_t	*hdrp;
	volatile cdkctrl_t	*cp;
	volatile unsigned char	*bits;

#if DEBUG
	printf("stli_sendcmd(brdp=%x,portp=%x,cmd=%x,arg=%x,size=%d,"
		"copyback=%d)\n", (int) brdp, (int) portp, (int) cmd,
		(int) arg, size, copyback);
#endif

	if (portp->state & ST_CMDING) {
		printf("STALLION: command already busy, cmd=%x!\n", (int) cmd);
		return;
	}

	EBRDENABLE(brdp);
	cp = &((volatile cdkasy_t *) EBRDGETMEMPTR(brdp, portp->addr))->ctrl;
	if (size > 0) {
		bcopy(arg, (void *) &(cp->args[0]), size);
		if (copyback) {
			portp->argp = arg;
			portp->argsize = size;
		}
	}
	cp->status = 0;
	cp->cmd = cmd;
	hdrp = (volatile cdkhdr_t *) EBRDGETMEMPTR(brdp, CDK_CDKADDR);
	bits = ((volatile unsigned char *) hdrp) + brdp->slaveoffset +
		portp->portidx;
	*bits |= portp->portbit;
	portp->state |= ST_CMDING;
	EBRDDISABLE(brdp);
}

/*****************************************************************************/

/*
 *	Read data from shared memory. This assumes that the shared memory
 *	is enabled and that interrupts are off. Basically we just empty out
 *	the shared memory buffer into the tty buffer. Must be carefull to
 *	handle the case where we fill up the tty buffer, but still have
 *	more chars to unload.
 */

static void stli_rxprocess(stlibrd_t *brdp, stliport_t *portp)
{
	volatile cdkasyrq_t	*rp;
	volatile char		*shbuf;
	struct tty		*tp;
	unsigned int		head, tail, size;
	unsigned int		len, stlen, i;
	int			ch;

#if DEBUG
	printf("stli_rxprocess(brdp=%x,portp=%d)\n", (int) brdp, (int) portp);
#endif

	tp = &portp->tty;
	if ((tp->t_state & TS_ISOPEN) == 0) {
		stli_flush(portp, FREAD);
		return;
	}
	if (tp->t_state & TS_TBLOCK)
		return;

	rp = &((volatile cdkasy_t *) EBRDGETMEMPTR(brdp, portp->addr))->rxq;
	head = (unsigned int) rp->head;
	if (head != ((unsigned int) rp->head))
		head = (unsigned int) rp->head;
	tail = (unsigned int) rp->tail;
	size = portp->rxsize;
	if (head >= tail) {
		len = head - tail;
		stlen = len;
	} else {
		len = size - (tail - head);
		stlen = size - tail;
	}

	if (len == 0)
		return;

	shbuf = (volatile char *) EBRDGETMEMPTR(brdp, portp->rxoffset);

/*
 *	If we can bypass normal LD processing then just copy direct
 *	from board shared memory into the tty buffers.
 */
	if (tp->t_state & TS_CAN_BYPASS_L_RINT) {
		if (((tp->t_rawq.c_cc + len) >= TTYHOG) &&
		    ((tp->t_cflag & CRTS_IFLOW) || (tp->t_iflag & IXOFF)) &&
		    ((tp->t_state & TS_TBLOCK) == 0)) {
			ch = TTYHOG - tp->t_rawq.c_cc - 1;
			len = (ch > 0) ? ch : 0;
			stlen = MIN(stlen, len);
			tp->t_state |= TS_TBLOCK;
		}
		i = b_to_q((char *) (shbuf + tail), stlen, &tp->t_rawq);
		tail += stlen;
		len -= stlen;
		if (tail >= size) {
			tail = 0;
			i += b_to_q((char *) shbuf, len, &tp->t_rawq);
			tail += len;
		}
		portp->rxlost += i;
		ttwakeup(tp);
		rp = &((volatile cdkasy_t *)
			EBRDGETMEMPTR(brdp, portp->addr))->rxq;
		rp->tail = tail;

	} else {
/*
 *		Copy the data from board shared memory into a local
 *		memory buffer. Then feed them from here into the LD.
 *		We don't want to go into board shared memory one char
 *		at a time, it is too slow...
 */
		if (len > TTYHOG) {
			len = TTYHOG - 1;
			stlen = min(len, stlen);
		}
		stli_rxtmpport = portp;
		stli_rxtmplen = len;
		bcopy((char *) (shbuf + tail), &stli_rxtmpbuf[0], stlen);
		len -= stlen;
		if (len > 0)
			bcopy((char *) shbuf, &stli_rxtmpbuf[stlen], len);
		
		for (i = 0; (i < stli_rxtmplen); i++) {
			ch = (unsigned char) stli_rxtmpbuf[i];
			(*linesw[tp->t_line].l_rint)(ch, tp);
		}
		EBRDENABLE(brdp);
		rp = &((volatile cdkasy_t *)
			EBRDGETMEMPTR(brdp, portp->addr))->rxq;
		if (stli_rxtmplen == 0) {
			head = (unsigned int) rp->head;
			if (head != ((unsigned int) rp->head))
				head = (unsigned int) rp->head;
			tail = head;
		} else {
			tail += i;
			if (tail >= size)
				tail -= size;
		}
		rp->tail = tail;
		stli_rxtmpport = (stliport_t *) NULL;
		stli_rxtmplen = 0;
	}

	portp->state |= ST_RXING;
}

/*****************************************************************************/

/*
 *	Set up and carry out any delayed commands. There is only a small set
 *	of slave commands that can be done "off-level". So it is not too
 *	difficult to deal with them as a special case here.
 */

static inline void stli_dodelaycmd(stliport_t *portp, volatile cdkctrl_t *cp)
{
	int	cmd;

	if (portp->state & ST_DOSIGS) {
		if ((portp->state & ST_DOFLUSHTX) &&
		    (portp->state & ST_DOFLUSHRX))
			cmd = A_SETSIGNALSF;
		else if (portp->state & ST_DOFLUSHTX)
			cmd = A_SETSIGNALSFTX;
		else if (portp->state & ST_DOFLUSHRX)
			cmd = A_SETSIGNALSFRX;
		else
			cmd = A_SETSIGNALS;
		portp->state &= ~(ST_DOFLUSHTX | ST_DOFLUSHRX | ST_DOSIGS);
		bcopy((void *) &portp->asig, (void *) &(cp->args[0]),
			sizeof(asysigs_t));
		cp->status = 0;
		cp->cmd = cmd;
		portp->state |= ST_CMDING;
	} else if ((portp->state & ST_DOFLUSHTX) ||
	    (portp->state & ST_DOFLUSHRX)) {
		cmd = ((portp->state & ST_DOFLUSHTX) ? FLUSHTX : 0);
		cmd |= ((portp->state & ST_DOFLUSHRX) ? FLUSHRX : 0);
		portp->state &= ~(ST_DOFLUSHTX | ST_DOFLUSHRX);
		bcopy((void *) &cmd, (void *) &(cp->args[0]), sizeof(int));
		cp->status = 0;
		cp->cmd = A_FLUSH;
		portp->state |= ST_CMDING;
	}
}

/*****************************************************************************/

/*
 *	Host command service checking. This handles commands or messages
 *	coming from the slave to the host. Must have board shared memory
 *	enabled and interrupts off when called. Notice that by servicing the
 *	read data last we don't need to change the shared memory pointer
 *	during processing (which is a slow IO operation).
 *	Return value indicates if this port is still awaiting actions from
 *	the slave (like open, command, or even TX data being sent). If 0
 *	then port is still busy, otherwise the port request bit flag is
 *	returned.
 */

static inline int stli_hostcmd(stlibrd_t *brdp, stliport_t *portp)
{
	volatile cdkasy_t	*ap;
	volatile cdkctrl_t	*cp;
	asynotify_t		nt;
	unsigned long		oldsigs;
	unsigned int		head, tail;
	int			rc, donerx;

#if DEBUG
	printf("stli_hostcmd(brdp=%x,portp=%x)\n", (int) brdp, (int) portp);
#endif

	ap = (volatile cdkasy_t *) EBRDGETMEMPTR(brdp, portp->addr);
	cp = &ap->ctrl;

/*
 *	Check if we are waiting for an open completion message.
 */
	if (portp->state & ST_OPENING) {
		rc = (int) cp->openarg;
		if ((cp->open == 0) && (rc != 0)) {
			if (rc > 0)
				rc--;
			cp->openarg = 0;
			portp->rc = rc;
			portp->state &= ~ST_OPENING;
			wakeup(&portp->state);
		}
	}

/*
 *	Check if we are waiting for a close completion message.
 */
	if (portp->state & ST_CLOSING) {
		rc = (int) cp->closearg;
		if ((cp->close == 0) && (rc != 0)) {
			if (rc > 0)
				rc--;
			cp->closearg = 0;
			portp->rc = rc;
			portp->state &= ~ST_CLOSING;
			wakeup(&portp->state);
		}
	}

/*
 *	Check if we are waiting for a command completion message. We may
 *	need to copy out the command results associated with this command.
 */
	if (portp->state & ST_CMDING) {
		rc = cp->status;
		if ((cp->cmd == 0) && (rc != 0)) {
			if (rc > 0)
				rc--;
			if (portp->argp != (void *) NULL) {
				bcopy((void *) &(cp->args[0]), portp->argp,
					portp->argsize);
				portp->argp = (void *) NULL;
			}
			cp->status = 0;
			portp->rc = rc;
			portp->state &= ~ST_CMDING;
			stli_dodelaycmd(portp, cp);
			wakeup(&portp->state);
		}
	}

/*
 *	Check for any notification messages ready. This includes lots of
 *	different types of events - RX chars ready, RX break received,
 *	TX data low or empty in the slave, modem signals changed state.
 *	Must be extremely carefull if we call to the LD, it may call
 *	other routines of ours that will disable the memory...
 *	Something else we need to be carefull of is race conditions on
 *	marking the TX as empty...
 */
	donerx = 0;

	if (ap->notify) {
		struct tty	*tp;

		nt = ap->changed;
		ap->notify = 0;
		tp = &portp->tty;

		if (nt.signal & SG_DCD) {
			oldsigs = portp->sigs;
			portp->sigs = stli_mktiocm(nt.sigvalue);
			portp->state &= ~ST_GETSIGS;
			(*linesw[tp->t_line].l_modem)(tp,
				(portp->sigs & TIOCM_CD));
			EBRDENABLE(brdp);
		}
		if (nt.data & DT_RXBUSY) {
			donerx++;
			stli_rxprocess(brdp, portp);
		}
		if (nt.data & DT_RXBREAK) {
			(*linesw[tp->t_line].l_rint)(TTY_BI, tp);
			EBRDENABLE(brdp);
		}
		if (nt.data & DT_TXEMPTY) {
			ap = (volatile cdkasy_t *)
				EBRDGETMEMPTR(brdp, portp->addr);
			head = (unsigned int) ap->txq.head;
			tail = (unsigned int) ap->txq.tail;
			if (tail != ((unsigned int) ap->txq.tail))
				tail = (unsigned int) ap->txq.tail;
			head = (head >= tail) ? (head - tail) :
				portp->txsize - (tail - head);
			if (head == 0) {
				portp->state &= ~ST_TXBUSY;
				tp->t_state &= ~TS_BUSY;
			}
		}
		if (nt.data & (DT_TXEMPTY | DT_TXLOW)) {
			(*linesw[tp->t_line].l_start)(tp);
			EBRDENABLE(brdp);
		}
	}

/*
 *	It might seem odd that we are checking for more RX chars here.
 *	But, we need to handle the case where the tty buffer was previously
 *	filled, but we had more characters to pass up. The slave will not
 *	send any more RX notify messages until the RX buffer has been emptied.
 *	But it will leave the service bits on (since the buffer is not empty).
 *	So from here we can try to process more RX chars.
 */
	if ((!donerx) && (portp->state & ST_RXING)) {
		portp->state &= ~ST_RXING;
		stli_rxprocess(brdp, portp);
	}

	return((portp->state & (ST_OPENING | ST_CLOSING | ST_CMDING |
		ST_TXBUSY | ST_RXING)) ? 0 : 1);
}

/*****************************************************************************/

/*
 *	Service all ports on a particular board. Assumes that the boards
 *	shared memory is enabled, and that the page pointer is pointed
 *	at the cdk header structure.
 */

static inline void stli_brdpoll(stlibrd_t *brdp, volatile cdkhdr_t *hdrp)
{
	stliport_t	*portp;
	unsigned char	hostbits[(STL_MAXCHANS / 8) + 1];
	unsigned char	slavebits[(STL_MAXCHANS / 8) + 1];
	unsigned char	*slavep;
	int		bitpos, bitat, bitsize;
	int 		channr, nrdevs, slavebitchange;

	bitsize = brdp->bitsize;
	nrdevs = brdp->nrdevs;

/*
 *	Check if slave wants any service. Basically we try to do as
 *	little work as possible here. There are 2 levels of service
 *	bits. So if there is nothing to do we bail early. We check
 *	8 service bits at a time in the inner loop, so we can bypass
 *	the lot if none of them want service.
 */
	bcopy((((unsigned char *) hdrp) + brdp->hostoffset), &hostbits[0],
		bitsize);

	bzero(&slavebits[0], bitsize);
	slavebitchange = 0;

	for (bitpos = 0; (bitpos < bitsize); bitpos++) {
		if (hostbits[bitpos] == 0)
			continue;
		channr = bitpos * 8;
		bitat = 0x1;
		for (; (channr < nrdevs); channr++, bitat <<=1) {
			if (hostbits[bitpos] & bitat) {
				portp = brdp->ports[(channr - 1)];
				if (stli_hostcmd(brdp, portp)) {
					slavebitchange++;
					slavebits[bitpos] |= bitat;
				}
			}
		}
	}

/*
 *	If any of the ports are no longer busy then update them in the
 *	slave request bits. We need to do this after, since a host port
 *	service may initiate more slave requests...
 */
	if (slavebitchange) {
		hdrp = (volatile cdkhdr_t *)
			EBRDGETMEMPTR(brdp, CDK_CDKADDR);
		slavep = ((unsigned char *) hdrp) + brdp->slaveoffset;
		for (bitpos = 0; (bitpos < bitsize); bitpos++) {
			if (slavebits[bitpos])
				slavep[bitpos] &= ~slavebits[bitpos];
		}
	}
}

/*****************************************************************************/

/*
 *	Driver poll routine. This routine polls the boards in use and passes
 *	messages back up to host when neccesary. This is actually very
 *	CPU efficient, since we will always have the kernel poll clock, it
 *	adds only a few cycles when idle (since board service can be
 *	determined very easily), but when loaded generates no interrupts
 *	(with their expensive associated context change).
 */

static void stli_poll(void *arg)
{
	volatile cdkhdr_t	*hdrp;
	stlibrd_t		*brdp;
	int 			brdnr;

	disable_intr();

/*
 *	Check each board and do any servicing required.
 */
	for (brdnr = 0; (brdnr < stli_nrbrds); brdnr++) {
		brdp = stli_brds[brdnr];
		if (brdp == (stlibrd_t *) NULL)
			continue;
		if ((brdp->state & BST_STARTED) == 0)
			continue;

		EBRDENABLE(brdp);
		hdrp = (volatile cdkhdr_t *) EBRDGETMEMPTR(brdp, CDK_CDKADDR);
		if (hdrp->hostreq)
			stli_brdpoll(brdp, hdrp);
		EBRDDISABLE(brdp);
	}
	enable_intr();

	timeout(stli_poll, 0, 1);
}

/*****************************************************************************/

/*
 *	Translate the termios settings into the port setting structure of
 *	the slave.
 */

static void stli_mkasyport(stliport_t *portp, asyport_t *pp, struct termios *tiosp)
{
#if DEBUG
	printf("stli_mkasyport(portp=%x,pp=%x,tiosp=%d)\n", (int) portp,
		(int) pp, (int) tiosp);
#endif

	bzero(pp, sizeof(asyport_t));

/*
 *	Start of by setting the baud, char size, parity and stop bit info.
 */
	if (tiosp->c_ispeed == 0)
		tiosp->c_ispeed = tiosp->c_ospeed;
	if ((tiosp->c_ospeed < 0) || (tiosp->c_ospeed > STL_MAXBAUD))
		tiosp->c_ospeed = STL_MAXBAUD;
	pp->baudout = tiosp->c_ospeed;
	pp->baudin = pp->baudout;

	switch (tiosp->c_cflag & CSIZE) {
	case CS5:
		pp->csize = 5;
		break;
	case CS6:
		pp->csize = 6;
		break;
	case CS7:
		pp->csize = 7;
		break;
	default:
		pp->csize = 8;
		break;
	}

	if (tiosp->c_cflag & CSTOPB)
		pp->stopbs = PT_STOP2;
	else
		pp->stopbs = PT_STOP1;

	if (tiosp->c_cflag & PARENB) {
		if (tiosp->c_cflag & PARODD)
			pp->parity = PT_ODDPARITY;
		else
			pp->parity = PT_EVENPARITY;
	} else {
		pp->parity = PT_NOPARITY;
	}

	if (tiosp->c_iflag & ISTRIP)
		pp->iflag |= FI_ISTRIP;

/*
 *	Set up any flow control options enabled.
 */
	if (tiosp->c_iflag & IXON) {
		pp->flow |= F_IXON;
		if (tiosp->c_iflag & IXANY)
			pp->flow |= F_IXANY;
	}
	if (tiosp->c_iflag & IXOFF)
		pp->flow |= F_IXOFF;
	if (tiosp->c_cflag & CCTS_OFLOW)
		pp->flow |= F_CTSFLOW;
	if (tiosp->c_cflag & CRTS_IFLOW)
		pp->flow |= F_RTSFLOW;

	pp->startin = tiosp->c_cc[VSTART];
	pp->stopin = tiosp->c_cc[VSTOP];
	pp->startout = tiosp->c_cc[VSTART];
	pp->stopout = tiosp->c_cc[VSTOP];

/*
 *	Set up the RX char marking mask with those RX error types we must
 *	catch. We can get the slave to help us out a little here, it will
 *	ignore parity errors and breaks for us, and mark parity errors in
 *	the data stream.
 */
	if (tiosp->c_iflag & IGNPAR)
		pp->iflag |= FI_IGNRXERRS;
	if (tiosp->c_iflag & IGNBRK)
		pp->iflag |= FI_IGNBREAK;
	if (tiosp->c_iflag & (INPCK | PARMRK))
		pp->iflag |= FI_1MARKRXERRS;

/*
 *	Transfer any persistent flags into the asyport structure.
 */
	pp->pflag = portp->pflag;
}

/*****************************************************************************/

/*
 *	Construct a slave signals structure for setting the DTR and RTS
 *	signals as specified.
 */

static void stli_mkasysigs(asysigs_t *sp, int dtr, int rts)
{
#if DEBUG
	printf("stli_mkasysigs(sp=%x,dtr=%d,rts=%d)\n", (int) sp, dtr, rts);
#endif

	bzero(sp, sizeof(asysigs_t));
	if (dtr >= 0) {
		sp->signal |= SG_DTR;
		sp->sigvalue |= ((dtr > 0) ? SG_DTR : 0);
	}
	if (rts >= 0) {
		sp->signal |= SG_RTS;
		sp->sigvalue |= ((rts > 0) ? SG_RTS : 0);
	}
}

/*****************************************************************************/

/*
 *	Convert the signals returned from the slave into a local TIOCM type
 *	signals value. We keep them localy in TIOCM format.
 */

static long stli_mktiocm(unsigned long sigvalue)
{
	long	tiocm;

#if DEBUG
	printf("stli_mktiocm(sigvalue=%x)\n", (int) sigvalue);
#endif

	tiocm = 0;
	tiocm |= ((sigvalue & SG_DCD) ? TIOCM_CD : 0);
	tiocm |= ((sigvalue & SG_CTS) ? TIOCM_CTS : 0);
	tiocm |= ((sigvalue & SG_RI) ? TIOCM_RI : 0);
	tiocm |= ((sigvalue & SG_DSR) ? TIOCM_DSR : 0);
	tiocm |= ((sigvalue & SG_DTR) ? TIOCM_DTR : 0);
	tiocm |= ((sigvalue & SG_RTS) ? TIOCM_RTS : 0);
	return(tiocm);
}

/*****************************************************************************/

/*
 *	Enable l_rint processing bypass mode if tty modes allow it.
 */

static void stli_ttyoptim(stliport_t *portp, struct termios *tiosp)
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
 *	All panels and ports actually attached have been worked out. All
 *	we need to do here is set up the appropriate per port data structures.
 */

static int stli_initports(stlibrd_t *brdp)
{
	stliport_t	*portp;
	int		i, panelnr, panelport;

#if DEBUG
	printf("stli_initports(brdp=%x)\n", (int) brdp);
#endif

	for (i = 0, panelnr = 0, panelport = 0; (i < brdp->nrports); i++) {
		portp = (stliport_t *) malloc(sizeof(stliport_t), M_TTYS,
			M_NOWAIT);
		if (portp == (stliport_t *) NULL) {
			printf("STALLION: failed to allocate port structure\n");
			continue;
		}
		bzero(portp, sizeof(stliport_t));

		portp->portnr = i;
		portp->brdnr = brdp->brdnr;
		portp->panelnr = panelnr;
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

		panelport++;
		if (panelport >= brdp->panels[panelnr]) {
			panelport = 0;
			panelnr++;
		}
		brdp->ports[i] = portp;
	}

	return(0);
}

/*****************************************************************************/

/*
 *	All the following routines are board specific hardware operations.
 */

static void stli_ecpinit(stlibrd_t *brdp)
{
	unsigned long	memconf;

#if DEBUG
	printf("stli_ecpinit(brdp=%d)\n", (int) brdp);
#endif

	outb((brdp->iobase + ECP_ATCONFR), ECP_ATSTOP);
	DELAY(10);
	outb((brdp->iobase + ECP_ATCONFR), ECP_ATDISABLE);
	DELAY(100);

	memconf = (brdp->paddr & ECP_ATADDRMASK) >> ECP_ATADDRSHFT;
	outb((brdp->iobase + ECP_ATMEMAR), memconf);
}

/*****************************************************************************/

static void stli_ecpenable(stlibrd_t *brdp)
{	
#if DEBUG
	printf("stli_ecpenable(brdp=%x)\n", (int) brdp);
#endif
	outb((brdp->iobase + ECP_ATCONFR), ECP_ATENABLE);
}

/*****************************************************************************/

static void stli_ecpdisable(stlibrd_t *brdp)
{	
#if DEBUG
	printf("stli_ecpdisable(brdp=%x)\n", (int) brdp);
#endif
	outb((brdp->iobase + ECP_ATCONFR), ECP_ATDISABLE);
}

/*****************************************************************************/

static char *stli_ecpgetmemptr(stlibrd_t *brdp, unsigned long offset, int line)
{	
	void		*ptr;
	unsigned char	val;

#if DEBUG
	printf("stli_ecpgetmemptr(brdp=%x,offset=%x)\n", (int) brdp,
		(int) offset);
#endif

	if (offset > brdp->memsize) {
		printf("STALLION: shared memory pointer=%x out of range at "
			"line=%d(%d), brd=%d\n", (int) offset, line,
			__LINE__, brdp->brdnr);
		ptr = 0;
		val = 0;
	} else {
		ptr = brdp->vaddr + (offset % ECP_ATPAGESIZE);
		val = (unsigned char) (offset / ECP_ATPAGESIZE);
	}
	outb((brdp->iobase + ECP_ATMEMPR), val);
	return(ptr);
}

/*****************************************************************************/

static void stli_ecpreset(stlibrd_t *brdp)
{	
#if DEBUG
	printf("stli_ecpreset(brdp=%x)\n", (int) brdp);
#endif

	outb((brdp->iobase + ECP_ATCONFR), ECP_ATSTOP);
	DELAY(10);
	outb((brdp->iobase + ECP_ATCONFR), ECP_ATDISABLE);
	DELAY(500);
}

/*****************************************************************************/

static void stli_ecpintr(stlibrd_t *brdp)
{	
#if DEBUG
	printf("stli_ecpintr(brdp=%x)\n", (int) brdp);
#endif
	outb(brdp->iobase, 0x1);
}

/*****************************************************************************/

/*
 *	The following set of functions act on ECP EISA boards.
 */

static void stli_ecpeiinit(stlibrd_t *brdp)
{
	unsigned long	memconf;

#if DEBUG
	printf("stli_ecpeiinit(brdp=%x)\n", (int) brdp);
#endif

	outb((brdp->iobase + ECP_EIBRDENAB), 0x1);
	outb((brdp->iobase + ECP_EICONFR), ECP_EISTOP);
	DELAY(10);
	outb((brdp->iobase + ECP_EICONFR), ECP_EIDISABLE);
	DELAY(500);

	memconf = (brdp->paddr & ECP_EIADDRMASKL) >> ECP_EIADDRSHFTL;
	outb((brdp->iobase + ECP_EIMEMARL), memconf);
	memconf = (brdp->paddr & ECP_EIADDRMASKH) >> ECP_EIADDRSHFTH;
	outb((brdp->iobase + ECP_EIMEMARH), memconf);
}

/*****************************************************************************/

static void stli_ecpeienable(stlibrd_t *brdp)
{	
	outb((brdp->iobase + ECP_EICONFR), ECP_EIENABLE);
}

/*****************************************************************************/

static void stli_ecpeidisable(stlibrd_t *brdp)
{	
	outb((brdp->iobase + ECP_EICONFR), ECP_EIDISABLE);
}

/*****************************************************************************/

static char *stli_ecpeigetmemptr(stlibrd_t *brdp, unsigned long offset, int line)
{	
	void		*ptr;
	unsigned char	val;

#if DEBUG
	printf("stli_ecpeigetmemptr(brdp=%x,offset=%x,line=%d)\n",
		(int) brdp, (int) offset, line);
#endif

	if (offset > brdp->memsize) {
		printf("STALLION: shared memory pointer=%x out of range at "
			"line=%d(%d), brd=%d\n", (int) offset, line,
			__LINE__, brdp->brdnr);
		ptr = 0;
		val = 0;
	} else {
		ptr = brdp->vaddr + (offset % ECP_EIPAGESIZE);
		if (offset < ECP_EIPAGESIZE)
			val = ECP_EIENABLE;
		else
			val = ECP_EIENABLE | 0x40;
	}
	outb((brdp->iobase + ECP_EICONFR), val);
	return(ptr);
}

/*****************************************************************************/

static void stli_ecpeireset(stlibrd_t *brdp)
{	
	outb((brdp->iobase + ECP_EICONFR), ECP_EISTOP);
	DELAY(10);
	outb((brdp->iobase + ECP_EICONFR), ECP_EIDISABLE);
	DELAY(500);
}

/*****************************************************************************/

/*
 *	The following set of functions act on ECP MCA boards.
 */

static void stli_ecpmcenable(stlibrd_t *brdp)
{	
	outb((brdp->iobase + ECP_MCCONFR), ECP_MCENABLE);
}

/*****************************************************************************/

static void stli_ecpmcdisable(stlibrd_t *brdp)
{	
	outb((brdp->iobase + ECP_MCCONFR), ECP_MCDISABLE);
}

/*****************************************************************************/

static char *stli_ecpmcgetmemptr(stlibrd_t *brdp, unsigned long offset, int line)
{	
	void		*ptr;
	unsigned char	val;

	if (offset > brdp->memsize) {
		printf("STALLION: shared memory pointer=%x out of range at "
			"line=%d(%d), brd=%d\n", (int) offset, line,
			__LINE__, brdp->brdnr);
		ptr = 0;
		val = 0;
	} else {
		ptr = brdp->vaddr + (offset % ECP_MCPAGESIZE);
		val = ((unsigned char) (offset / ECP_MCPAGESIZE)) | ECP_MCENABLE;
	}
	outb((brdp->iobase + ECP_MCCONFR), val);
	return(ptr);
}

/*****************************************************************************/

static void stli_ecpmcreset(stlibrd_t *brdp)
{	
	outb((brdp->iobase + ECP_MCCONFR), ECP_MCSTOP);
	DELAY(10);
	outb((brdp->iobase + ECP_MCCONFR), ECP_MCDISABLE);
	DELAY(500);
}

/*****************************************************************************/

/*
 *	The following routines act on ONboards.
 */

static void stli_onbinit(stlibrd_t *brdp)
{
	unsigned long	memconf;
	int		i;

#if DEBUG
	printf("stli_onbinit(brdp=%d)\n", (int) brdp);
#endif

	outb((brdp->iobase + ONB_ATCONFR), ONB_ATSTOP);
	DELAY(10);
	outb((brdp->iobase + ONB_ATCONFR), ONB_ATDISABLE);
	for (i = 0; (i < 1000); i++)
		DELAY(1000);

	memconf = (brdp->paddr & ONB_ATADDRMASK) >> ONB_ATADDRSHFT;
	outb((brdp->iobase + ONB_ATMEMAR), memconf);
	outb(brdp->iobase, 0x1);
	DELAY(1000);
}

/*****************************************************************************/

static void stli_onbenable(stlibrd_t *brdp)
{	
#if DEBUG
	printf("stli_onbenable(brdp=%x)\n", (int) brdp);
#endif
	outb((brdp->iobase + ONB_ATCONFR), (ONB_ATENABLE | brdp->confbits));
}

/*****************************************************************************/

static void stli_onbdisable(stlibrd_t *brdp)
{	
#if DEBUG
	printf("stli_onbdisable(brdp=%x)\n", (int) brdp);
#endif
	outb((brdp->iobase + ONB_ATCONFR), (ONB_ATDISABLE | brdp->confbits));
}

/*****************************************************************************/

static char *stli_onbgetmemptr(stlibrd_t *brdp, unsigned long offset, int line)
{	
	void	*ptr;

#if DEBUG
	printf("stli_onbgetmemptr(brdp=%x,offset=%x)\n", (int) brdp,
		(int) offset);
#endif

	if (offset > brdp->memsize) {
		printf("STALLION: shared memory pointer=%x out of range at "
			"line=%d(%d), brd=%d\n", (int) offset, line,
			__LINE__, brdp->brdnr);
		ptr = 0;
	} else {
		ptr = brdp->vaddr + (offset % ONB_ATPAGESIZE);
	}
	return(ptr);
}

/*****************************************************************************/

static void stli_onbreset(stlibrd_t *brdp)
{	
	int	i;

#if DEBUG
	printf("stli_onbreset(brdp=%x)\n", (int) brdp);
#endif

	outb((brdp->iobase + ONB_ATCONFR), ONB_ATSTOP);
	DELAY(10);
	outb((brdp->iobase + ONB_ATCONFR), ONB_ATDISABLE);
	for (i = 0; (i < 1000); i++)
		DELAY(1000);
}

/*****************************************************************************/

/*
 *	The following routines act on ONboard EISA.
 */

static void stli_onbeinit(stlibrd_t *brdp)
{
	unsigned long	memconf;
	int		i;

#if DEBUG
	printf("stli_onbeinit(brdp=%d)\n", (int) brdp);
#endif

	outb((brdp->iobase + ONB_EIBRDENAB), 0x1);
	outb((brdp->iobase + ONB_EICONFR), ONB_EISTOP);
	DELAY(10);
	outb((brdp->iobase + ONB_EICONFR), ONB_EIDISABLE);
	for (i = 0; (i < 1000); i++)
		DELAY(1000);

	memconf = (brdp->paddr & ONB_EIADDRMASKL) >> ONB_EIADDRSHFTL;
	outb((brdp->iobase + ONB_EIMEMARL), memconf);
	memconf = (brdp->paddr & ONB_EIADDRMASKH) >> ONB_EIADDRSHFTH;
	outb((brdp->iobase + ONB_EIMEMARH), memconf);
	outb(brdp->iobase, 0x1);
	DELAY(1000);
}

/*****************************************************************************/

static void stli_onbeenable(stlibrd_t *brdp)
{	
#if DEBUG
	printf("stli_onbeenable(brdp=%x)\n", (int) brdp);
#endif
	outb((brdp->iobase + ONB_EICONFR), ONB_EIENABLE);
}

/*****************************************************************************/

static void stli_onbedisable(stlibrd_t *brdp)
{	
#if DEBUG
	printf("stli_onbedisable(brdp=%x)\n", (int) brdp);
#endif
	outb((brdp->iobase + ONB_EICONFR), ONB_EIDISABLE);
}

/*****************************************************************************/

static char *stli_onbegetmemptr(stlibrd_t *brdp, unsigned long offset, int line)
{	
	void		*ptr;
	unsigned char	val;

#if DEBUG
	printf("stli_onbegetmemptr(brdp=%x,offset=%x,line=%d)\n", (int) brdp,
		(int) offset, line);
#endif

	if (offset > brdp->memsize) {
		printf("STALLION: shared memory pointer=%x out of range at "
			"line=%d(%d), brd=%d\n", (int) offset, line,
			__LINE__, brdp->brdnr);
		ptr = 0;
		val = 0;
	} else {
		ptr = brdp->vaddr + (offset % ONB_EIPAGESIZE);
		if (offset < ONB_EIPAGESIZE)
			val = ONB_EIENABLE;
		else
			val = ONB_EIENABLE | 0x40;
	}
	outb((brdp->iobase + ONB_EICONFR), val);
	return(ptr);
}

/*****************************************************************************/

static void stli_onbereset(stlibrd_t *brdp)
{	
	int	i;

#if DEBUG
	printf("stli_onbereset(brdp=%x)\n", (int) brdp);
#endif

	outb((brdp->iobase + ONB_EICONFR), ONB_EISTOP);
	DELAY(10);
	outb((brdp->iobase + ONB_EICONFR), ONB_EIDISABLE);
	for (i = 0; (i < 1000); i++)
		DELAY(1000);
}

/*****************************************************************************/

/*
 *	The following routines act on Brumby boards.
 */

static void stli_bbyinit(stlibrd_t *brdp)
{
	int	i;

#if DEBUG
	printf("stli_bbyinit(brdp=%d)\n", (int) brdp);
#endif

	outb((brdp->iobase + BBY_ATCONFR), BBY_ATSTOP);
	DELAY(10);
	outb((brdp->iobase + BBY_ATCONFR), 0);
	for (i = 0; (i < 1000); i++)
		DELAY(1000);
	outb(brdp->iobase, 0x1);
	DELAY(1000);
}

/*****************************************************************************/

static char *stli_bbygetmemptr(stlibrd_t *brdp, unsigned long offset, int line)
{	
	void		*ptr;
	unsigned char	val;

#if DEBUG
	printf("stli_bbygetmemptr(brdp=%x,offset=%x)\n", (int) brdp,
		(int) offset);
#endif

	if (offset > brdp->memsize) {
		printf("STALLION: shared memory pointer=%x out of range at "
			"line=%d(%d), brd=%d\n", (int) offset, line,
			__LINE__, brdp->brdnr);
		ptr = 0;
		val = 0;
	} else {
		ptr = brdp->vaddr + (offset % BBY_PAGESIZE);
		val = (unsigned char) (offset / BBY_PAGESIZE);
	}
	outb((brdp->iobase + BBY_ATCONFR), val);
	return(ptr);
}

/*****************************************************************************/

static void stli_bbyreset(stlibrd_t *brdp)
{	
	int	i;

#if DEBUG
	printf("stli_bbyreset(brdp=%x)\n", (int) brdp);
#endif

	outb((brdp->iobase + BBY_ATCONFR), BBY_ATSTOP);
	DELAY(10);
	outb((brdp->iobase + BBY_ATCONFR), 0);
	for (i = 0; (i < 1000); i++)
		DELAY(1000);
}

/*****************************************************************************/

/*
 *	The following routines act on original old Stallion boards.
 */

static void stli_stalinit(stlibrd_t *brdp)
{
	int	i;

#if DEBUG
	printf("stli_stalinit(brdp=%d)\n", (int) brdp);
#endif

	outb(brdp->iobase, 0x1);
	for (i = 0; (i < 1000); i++)
		DELAY(1000);
}

/*****************************************************************************/

static char *stli_stalgetmemptr(stlibrd_t *brdp, unsigned long offset, int line)
{	
	void	*ptr;

#if DEBUG
	printf("stli_stalgetmemptr(brdp=%x,offset=%x)\n", (int) brdp,
		(int) offset);
#endif

	if (offset > brdp->memsize) {
		printf("STALLION: shared memory pointer=%x out of range at "
			"line=%d(%d), brd=%d\n", (int) offset, line,
			__LINE__, brdp->brdnr);
		ptr = 0;
	} else {
		ptr = brdp->vaddr + (offset % STAL_PAGESIZE);
	}
	return(ptr);
}

/*****************************************************************************/

static void stli_stalreset(stlibrd_t *brdp)
{	
	volatile unsigned long	*vecp;
	int			i;

#if DEBUG
	printf("stli_stalreset(brdp=%x)\n", (int) brdp);
#endif

	vecp = (volatile unsigned long *) (brdp->vaddr + 0x30);
	*vecp = 0xffff0000;
	outb(brdp->iobase, 0);
	for (i = 0; (i < 1000); i++)
		DELAY(1000);
}

/*****************************************************************************/

/*
 *	Try to find an ECP board and initialize it. This handles only ECP
 *	board types.
 */

static int stli_initecp(stlibrd_t *brdp)
{
	cdkecpsig_t	sig;
	cdkecpsig_t	*sigsp;
	unsigned int	status, nxtid;
	int		panelnr;

#if DEBUG
	printf("stli_initecp(brdp=%x)\n", (int) brdp);
#endif

/*
 *	Do a basic sanity check on the IO and memory addresses.
 */
	if ((brdp->iobase == 0) || (brdp->paddr == 0))
		return(EINVAL);

/*
 *	Based on the specific board type setup the common vars to access
 *	and enable shared memory. Set all board specific information now
 *	as well.
 */
	switch (brdp->brdtype) {
	case BRD_ECP:
		brdp->memsize = ECP_MEMSIZE;
		brdp->pagesize = ECP_ATPAGESIZE;
		brdp->init = stli_ecpinit;
		brdp->enable = stli_ecpenable;
		brdp->reenable = stli_ecpenable;
		brdp->disable = stli_ecpdisable;
		brdp->getmemptr = stli_ecpgetmemptr;
		brdp->intr = stli_ecpintr;
		brdp->reset = stli_ecpreset;
		break;

	case BRD_ECPE:
		brdp->memsize = ECP_MEMSIZE;
		brdp->pagesize = ECP_EIPAGESIZE;
		brdp->init = stli_ecpeiinit;
		brdp->enable = stli_ecpeienable;
		brdp->reenable = stli_ecpeienable;
		brdp->disable = stli_ecpeidisable;
		brdp->getmemptr = stli_ecpeigetmemptr;
		brdp->intr = stli_ecpintr;
		brdp->reset = stli_ecpeireset;
		break;

	case BRD_ECPMC:
		brdp->memsize = ECP_MEMSIZE;
		brdp->pagesize = ECP_MCPAGESIZE;
		brdp->init = NULL;
		brdp->enable = stli_ecpmcenable;
		brdp->reenable = stli_ecpmcenable;
		brdp->disable = stli_ecpmcdisable;
		brdp->getmemptr = stli_ecpmcgetmemptr;
		brdp->intr = stli_ecpintr;
		brdp->reset = stli_ecpmcreset;
		break;

	default:
		return(EINVAL);
	}

/*
 *	The per-board operations structure is all setup, so now lets go
 *	and get the board operational. Firstly initialize board configuration
 *	registers.
 */
	EBRDINIT(brdp);

/*
 *	Now that all specific code is set up, enable the shared memory and
 *	look for the a signature area that will tell us exactly what board
 *	this is, and what it is connected to it.
 */
	EBRDENABLE(brdp);
	sigsp = (cdkecpsig_t *) EBRDGETMEMPTR(brdp, CDK_SIGADDR);
	bcopy(sigsp, &sig, sizeof(cdkecpsig_t));
	EBRDDISABLE(brdp);

#if 0
	printf("%s(%d): sig-> magic=%x rom=%x panel=%x,%x,%x,%x,%x,%x,%x,%x\n",
		__file__, __LINE__, (int) sig.magic, sig.romver,
		sig.panelid[0], (int) sig.panelid[1], (int) sig.panelid[2],
		(int) sig.panelid[3], (int) sig.panelid[4],
		(int) sig.panelid[5], (int) sig.panelid[6],
		(int) sig.panelid[7]);
#endif

	if (sig.magic != ECP_MAGIC)
		return(ENXIO);

/*
 *	Scan through the signature looking at the panels connected to the
 *	board. Calculate the total number of ports as we go.
 */
	for (panelnr = 0, nxtid = 0; (panelnr < STL_MAXPANELS); panelnr++) {
		status = sig.panelid[nxtid];
		if ((status & ECH_PNLIDMASK) != nxtid)
			break;
		brdp->panelids[panelnr] = status;
		if (status & ECH_PNL16PORT) {
			brdp->panels[panelnr] = 16;
			brdp->nrports += 16;
			nxtid += 2;
		} else {
			brdp->panels[panelnr] = 8;
			brdp->nrports += 8;
			nxtid++;
		}
		brdp->nrpanels++;
	}

	brdp->state |= BST_FOUND;
	return(0);
}

/*****************************************************************************/

/*
 *	Try to find an ONboard, Brumby or Stallion board and initialize it.
 *	This handles only these board types.
 */

static int stli_initonb(stlibrd_t *brdp)
{
	cdkonbsig_t	sig;
	cdkonbsig_t	*sigsp;
	int		i;

#if DEBUG
	printf("stli_initonb(brdp=%x)\n", (int) brdp);
#endif

/*
 *	Do a basic sanity check on the IO and memory addresses.
 */
	if ((brdp->iobase == 0) || (brdp->paddr == 0))
		return(EINVAL);

/*
 *	Based on the specific board type setup the common vars to access
 *	and enable shared memory. Set all board specific information now
 *	as well.
 */
	switch (brdp->brdtype) {
	case BRD_ONBOARD:
	case BRD_ONBOARD32:
	case BRD_ONBOARD2:
	case BRD_ONBOARD2_32:
	case BRD_ONBOARDRS:
		brdp->memsize = ONB_MEMSIZE;
		brdp->pagesize = ONB_ATPAGESIZE;
		brdp->init = stli_onbinit;
		brdp->enable = stli_onbenable;
		brdp->reenable = stli_onbenable;
		brdp->disable = stli_onbdisable;
		brdp->getmemptr = stli_onbgetmemptr;
		brdp->intr = stli_ecpintr;
		brdp->reset = stli_onbreset;
		brdp->confbits = (brdp->paddr > 0x100000) ? ONB_HIMEMENAB : 0;
		break;

	case BRD_ONBOARDE:
		brdp->memsize = ONB_EIMEMSIZE;
		brdp->pagesize = ONB_EIPAGESIZE;
		brdp->init = stli_onbeinit;
		brdp->enable = stli_onbeenable;
		brdp->reenable = stli_onbeenable;
		brdp->disable = stli_onbedisable;
		brdp->getmemptr = stli_onbegetmemptr;
		brdp->intr = stli_ecpintr;
		brdp->reset = stli_onbereset;
		break;

	case BRD_BRUMBY4:
	case BRD_BRUMBY8:
	case BRD_BRUMBY16:
		brdp->memsize = BBY_MEMSIZE;
		brdp->pagesize = BBY_PAGESIZE;
		brdp->init = stli_bbyinit;
		brdp->enable = NULL;
		brdp->reenable = NULL;
		brdp->disable = NULL;
		brdp->getmemptr = stli_bbygetmemptr;
		brdp->intr = stli_ecpintr;
		brdp->reset = stli_bbyreset;
		break;

	case BRD_STALLION:
		brdp->memsize = STAL_MEMSIZE;
		brdp->pagesize = STAL_PAGESIZE;
		brdp->init = stli_stalinit;
		brdp->enable = NULL;
		brdp->reenable = NULL;
		brdp->disable = NULL;
		brdp->getmemptr = stli_stalgetmemptr;
		brdp->intr = stli_ecpintr;
		brdp->reset = stli_stalreset;
		break;

	default:
		return(EINVAL);
	}

/*
 *	The per-board operations structure is all setup, so now lets go
 *	and get the board operational. Firstly initialize board configuration
 *	registers.
 */
	EBRDINIT(brdp);

/*
 *	Now that all specific code is set up, enable the shared memory and
 *	look for the a signature area that will tell us exactly what board
 *	this is, and how many ports.
 */
	EBRDENABLE(brdp);
	sigsp = (cdkonbsig_t *) EBRDGETMEMPTR(brdp, CDK_SIGADDR);
	bcopy(sigsp, &sig, sizeof(cdkonbsig_t));
	EBRDDISABLE(brdp);

#if 0
	printf("%s(%d): sig-> magic=%x:%x:%x:%x romver=%x amask=%x:%x:%x\n",
		__file__, __LINE__, sig.magic0, sig.magic1, sig.magic2,
		sig.magic3, sig.romver, sig.amask0, sig.amask1, sig.amask2);
#endif

	if ((sig.magic0 != ONB_MAGIC0) || (sig.magic1 != ONB_MAGIC1) ||
	    (sig.magic2 != ONB_MAGIC2) || (sig.magic3 != ONB_MAGIC3))
		return(ENXIO);

/*
 *	Scan through the signature alive mask and calculate how many ports
 *	there are on this board.
 */
	brdp->nrpanels = 1;
	if (sig.amask1) {
		brdp->nrports = 32;
	} else {
		for (i = 0; (i < 16); i++) {
			if (((sig.amask0 << i) & 0x8000) == 0)
				break;
		}
		brdp->nrports = i;
	}
	brdp->panels[0] = brdp->nrports;

	brdp->state |= BST_FOUND;
	return(0);
}

/*****************************************************************************/

/*
 *	Start up a running board. This routine is only called after the
 *	code has been down loaded to the board and is operational. It will
 *	read in the memory map, and get the show on the road...
 */

static int stli_startbrd(stlibrd_t *brdp)
{
	volatile cdkhdr_t	*hdrp;
	volatile cdkmem_t	*memp;
	volatile cdkasy_t	*ap;
	stliport_t		*portp;
	int			portnr, nrdevs, i, rc;

#if DEBUG
	printf("stli_startbrd(brdp=%x)\n", (int) brdp);
#endif

	rc = 0;

	disable_intr();
	EBRDENABLE(brdp);
	hdrp = (volatile cdkhdr_t *) EBRDGETMEMPTR(brdp, CDK_CDKADDR);
	nrdevs = hdrp->nrdevs;

#if 0
	printf("%s(%d): CDK version %d.%d.%d --> nrdevs=%d memp=%x hostp=%x "
		"slavep=%x\n", __file__, __LINE__, hdrp->ver_release,
		hdrp->ver_modification, hdrp->ver_fix, nrdevs,
		(int) hdrp->memp, (int) hdrp->hostp, (int) hdrp->slavep);
#endif

	if (nrdevs < (brdp->nrports + 1)) {
		printf("STALLION: slave failed to allocate memory for all "
			"devices, devices=%d\n", nrdevs);
		brdp->nrports = nrdevs - 1;
	}
	brdp->nrdevs = nrdevs;
	brdp->hostoffset = hdrp->hostp - CDK_CDKADDR;
	brdp->slaveoffset = hdrp->slavep - CDK_CDKADDR;
	brdp->bitsize = (nrdevs + 7) / 8;
	memp = (volatile cdkmem_t *) hdrp->memp;
	if (((unsigned long) memp) > brdp->memsize) {
		printf("STALLION: corrupted shared memory region?\n");
		rc = EIO;
		goto stli_donestartup;
	}
	memp = (volatile cdkmem_t *) EBRDGETMEMPTR(brdp, (unsigned long) memp);
	if (memp->dtype != TYP_ASYNCTRL) {
		printf("STALLION: no slave control device found\n");
		rc = EIO;
		goto stli_donestartup;
	}
	memp++;

/*
 *	Cycle through memory allocation of each port. We are guaranteed to
 *	have all ports inside the first page of slave window, so no need to
 *	change pages while reading memory map.
 */
	for (i = 1, portnr = 0; (i < nrdevs); i++, portnr++, memp++) {
		if (memp->dtype != TYP_ASYNC)
			break;
		portp = brdp->ports[portnr];
		if (portp == (stliport_t *) NULL)
			break;
		portp->devnr = i;
		portp->addr = memp->offset;
		portp->reqidx = (unsigned char) (i * 8 / nrdevs);
		portp->reqbit = (unsigned char) (0x1 << portp->reqidx);
		portp->portidx = (unsigned char) (i / 8);
		portp->portbit = (unsigned char) (0x1 << (i % 8));
	}

	hdrp->slavereq = 0xff;

/*
 *	For each port setup a local copy of the RX and TX buffer offsets
 *	and sizes. We do this separate from the above, because we need to
 *	move the shared memory page...
 */
	for (i = 1, portnr = 0; (i < nrdevs); i++, portnr++) {
		portp = brdp->ports[portnr];
		if (portp == (stliport_t *) NULL)
			break;
		if (portp->addr == 0)
			break;
		ap = (volatile cdkasy_t *) EBRDGETMEMPTR(brdp, portp->addr);
		if (ap != (volatile cdkasy_t *) NULL) {
			portp->rxsize = ap->rxq.size;
			portp->txsize = ap->txq.size;
			portp->rxoffset = ap->rxq.offset;
			portp->txoffset = ap->txq.offset;
		}
	}

stli_donestartup:
	EBRDDISABLE(brdp);
	enable_intr();

	if (rc == 0)
		brdp->state |= BST_STARTED;

	if (stli_doingtimeout == 0) {
		timeout(stli_poll, 0, 1);
		stli_doingtimeout++;
	}

	return(rc);
}

/*****************************************************************************/

/*
 *	Probe and initialize the specified board.
 */

static int stli_brdinit(stlibrd_t *brdp)
{
#if DEBUG
	printf("stli_brdinit(brdp=%x)\n", (int) brdp);
#endif

	stli_brds[brdp->brdnr] = brdp;

	switch (brdp->brdtype) {
	case BRD_ECP:
	case BRD_ECPE:
	case BRD_ECPMC:
		stli_initecp(brdp);
		break;
	case BRD_ONBOARD:
	case BRD_ONBOARDE:
	case BRD_ONBOARD2:
	case BRD_ONBOARD32:
	case BRD_ONBOARD2_32:
	case BRD_ONBOARDRS:
	case BRD_BRUMBY4:
	case BRD_BRUMBY8:
	case BRD_BRUMBY16:
	case BRD_STALLION:
		stli_initonb(brdp);
		break;
	case BRD_EASYIO:
	case BRD_ECH:
	case BRD_ECHMC:
	case BRD_ECHPCI:
		printf("STALLION: %s board type not supported in this driver\n",
			stli_brdnames[brdp->brdtype]);
		return(ENODEV);
	default:
		printf("STALLION: unit=%d is unknown board type=%d\n",
			brdp->brdnr, brdp->brdtype);
		return(ENODEV);
	}
	return(0);
}

/*****************************************************************************/

/*
 *	Finish off the remaining initialization for a board.
 */

static int stli_brdattach(stlibrd_t *brdp)
{
#if DEBUG
	printf("stli_brdattach(brdp=%x)\n", (int) brdp);
#endif

#if 0
	if ((brdp->state & BST_FOUND) == 0) {
		printf("STALLION: %s board not found, unit=%d io=%x mem=%x\n",
			stli_brdnames[brdp->brdtype], brdp->brdnr,
			brdp->iobase, (int) brdp->paddr);
		return(ENXIO);
	}
#endif

	stli_initports(brdp);
	printf("stli%d: %s (driver version %s), unit=%d nrpanels=%d "
		"nrports=%d\n", brdp->unitid, stli_brdnames[brdp->brdtype],
		stli_drvversion, brdp->brdnr, brdp->nrpanels, brdp->nrports);
	return(0);
}

/*****************************************************************************/

/*
 *	Check for possible shared memory sharing between boards.
 *	FIX: need to start this optimization somewhere...
 */

static int stli_chksharemem()
{
	stlibrd_t	*brdp, *nxtbrdp;
	int		i, j;

#if DEBUG
	printf("stli_chksharemem()\n");
#endif

/*
 *	All found boards are initialized. Now for a little optimization, if
 *	no boards are sharing the "shared memory" regions then we can just
 *	leave them all enabled. This is in fact the usual case.
 */
	stli_shared = 0;
	if (stli_nrbrds > 1) {
		for (i = 0; (i < stli_nrbrds); i++) {
			brdp = stli_brds[i];
			if (brdp == (stlibrd_t *) NULL)
				continue;
			for (j = i + 1; (j < stli_nrbrds); j++) {
				nxtbrdp = stli_brds[j];
				if (nxtbrdp == (stlibrd_t *) NULL)
					continue;
				if ((brdp->paddr >= nxtbrdp->paddr) &&
				    (brdp->paddr <= (nxtbrdp->paddr +
				    nxtbrdp->memsize - 1))) {
					stli_shared++;
					break;
				}
			}
		}
	}

	if (stli_shared == 0) {
		for (i = 0; (i < stli_nrbrds); i++) {
			brdp = stli_brds[i];
			if (brdp == (stlibrd_t *) NULL)
				continue;
			if (brdp->state & BST_FOUND) {
				EBRDENABLE(brdp);
				brdp->enable = NULL;
				brdp->disable = NULL;
			}
		}
	}

	return(0);
}

/*****************************************************************************/

/*
 *	Return the board stats structure to user app.
 */

static int stli_getbrdstats(caddr_t data)
{
	stlibrd_t	*brdp;
	int		i;

#if DEBUG
	printf("stli_getbrdstats(data=%x)\n", data);
#endif

	stli_brdstats = *((combrd_t *) data);
	if (stli_brdstats.brd >= STL_MAXBRDS)
		return(-ENODEV);
	brdp = stli_brds[stli_brdstats.brd];
	if (brdp == (stlibrd_t *) NULL)
		return(-ENODEV);

	bzero(&stli_brdstats, sizeof(combrd_t));
	stli_brdstats.brd = brdp->brdnr;
	stli_brdstats.type = brdp->brdtype;
	stli_brdstats.hwid = 0;
	stli_brdstats.state = brdp->state;
	stli_brdstats.ioaddr = brdp->iobase;
	stli_brdstats.memaddr = brdp->paddr;
	stli_brdstats.nrpanels = brdp->nrpanels;
	stli_brdstats.nrports = brdp->nrports;
	for (i = 0; (i < brdp->nrpanels); i++) {
		stli_brdstats.panels[i].panel = i;
		stli_brdstats.panels[i].hwid = brdp->panelids[i];
		stli_brdstats.panels[i].nrports = brdp->panels[i];
	}

	*((combrd_t *) data) = stli_brdstats;
	return(0);
}

/*****************************************************************************/

/*
 *	Resolve the referenced port number into a port struct pointer.
 */

static stliport_t *stli_getport(int brdnr, int panelnr, int portnr)
{
	stlibrd_t	*brdp;
	int		i;

	if ((brdnr < 0) || (brdnr >= STL_MAXBRDS))
		return((stliport_t *) NULL);
	brdp = stli_brds[brdnr];
	if (brdp == (stlibrd_t *) NULL)
		return((stliport_t *) NULL);
	for (i = 0; (i < panelnr); i++)
		portnr += brdp->panels[i];
	if ((portnr < 0) || (portnr >= brdp->nrports))
		return((stliport_t *) NULL);
	return(brdp->ports[portnr]);
}

/*****************************************************************************/

/*
 *	Return the port stats structure to user app. A NULL port struct
 *	pointer passed in means that we need to find out from the app
 *	what port to get stats for (used through board control device).
 */

static int stli_getportstats(stliport_t *portp, caddr_t data)
{
	stlibrd_t	*brdp;
	int		rc;

	if (portp == (stliport_t *) NULL) {
		stli_comstats = *((comstats_t *) data);
		portp = stli_getport(stli_comstats.brd, stli_comstats.panel,
			stli_comstats.port);
		if (portp == (stliport_t *) NULL)
			return(-ENODEV);
	}

	brdp = stli_brds[portp->brdnr];
	if (brdp == (stlibrd_t *) NULL)
		return(-ENODEV);

	if ((rc = stli_cmdwait(brdp, portp, A_GETSTATS, &stli_cdkstats,
			sizeof(asystats_t), 1)) < 0)
		return(rc);

	stli_comstats.brd = portp->brdnr;
	stli_comstats.panel = portp->panelnr;
	stli_comstats.port = portp->portnr;
	stli_comstats.state = portp->state;
	/*stli_comstats.flags = portp->flags;*/
	stli_comstats.ttystate = portp->tty.t_state;
	stli_comstats.cflags = portp->tty.t_cflag;
	stli_comstats.iflags = portp->tty.t_iflag;
	stli_comstats.oflags = portp->tty.t_oflag;
	stli_comstats.lflags = portp->tty.t_lflag;

	stli_comstats.txtotal = stli_cdkstats.txchars;
	stli_comstats.rxtotal = stli_cdkstats.rxchars + stli_cdkstats.ringover;
	stli_comstats.txbuffered = stli_cdkstats.txringq;
	stli_comstats.rxbuffered = stli_cdkstats.rxringq;
	stli_comstats.rxoverrun = stli_cdkstats.overruns;
	stli_comstats.rxparity = stli_cdkstats.parity;
	stli_comstats.rxframing = stli_cdkstats.framing;
	stli_comstats.rxlost = stli_cdkstats.ringover + portp->rxlost;
	stli_comstats.rxbreaks = stli_cdkstats.rxbreaks;
	stli_comstats.txbreaks = stli_cdkstats.txbreaks;
	stli_comstats.txxon = stli_cdkstats.txstart;
	stli_comstats.txxoff = stli_cdkstats.txstop;
	stli_comstats.rxxon = stli_cdkstats.rxstart;
	stli_comstats.rxxoff = stli_cdkstats.rxstop;
	stli_comstats.rxrtsoff = stli_cdkstats.rtscnt / 2;
	stli_comstats.rxrtson = stli_cdkstats.rtscnt - stli_comstats.rxrtsoff;
	stli_comstats.modem = stli_cdkstats.dcdcnt;
	stli_comstats.hwid = stli_cdkstats.hwid;
	stli_comstats.signals = stli_mktiocm(stli_cdkstats.signals);

	*((comstats_t *) data) = stli_comstats;;
	return(0);
}

/*****************************************************************************/

/*
 *	Clear the port stats structure. We also return it zeroed out...
 */

static int stli_clrportstats(stliport_t *portp, caddr_t data)
{
	stlibrd_t	*brdp;
	int		rc;

	if (portp == (stliport_t *) NULL) {
		stli_comstats = *((comstats_t *) data);
		portp = stli_getport(stli_comstats.brd, stli_comstats.panel,
			stli_comstats.port);
		if (portp == (stliport_t *) NULL)
			return(-ENODEV);
	}

	brdp = stli_brds[portp->brdnr];
	if (brdp == (stlibrd_t *) NULL)
		return(-ENODEV);

	if ((rc = stli_cmdwait(brdp, portp, A_CLEARSTATS, 0, 0, 0)) < 0)
		return(rc);

	portp->rxlost = 0;
	bzero(&stli_comstats, sizeof(comstats_t));
	stli_comstats.brd = portp->brdnr;
	stli_comstats.panel = portp->panelnr;
	stli_comstats.port = portp->portnr;

	*((comstats_t *) data) = stli_comstats;;
	return(0);
}

/*****************************************************************************/

/*
 *	Code to handle an "staliomem" read and write operations. This device
 *	is the contents of the board shared memory. It is used for down
 *	loading the slave image (and debugging :-)
 */

STATIC int stli_memrw(dev_t dev, struct uio *uiop, int flag)
{
	stlibrd_t	*brdp;
	void		*memptr;
	int		brdnr, size, n, error;

#if DEBUG
	printf("stli_memrw(dev=%x,uiop=%x,flag=%x)\n", (int) dev,
		(int) uiop, flag);
#endif

	brdnr = dev & 0x7;
	brdp = stli_brds[brdnr];
	if (brdp == (stlibrd_t *) NULL)
		return(ENODEV);
	if (brdp->state == 0)
		return(ENODEV);

	if (uiop->uio_offset >= brdp->memsize)
		return(0);

	error = 0;
	size = brdp->memsize - uiop->uio_offset;

	disable_intr();
	EBRDENABLE(brdp);
	while (size > 0) {
		memptr = (void *) EBRDGETMEMPTR(brdp, uiop->uio_offset);
		n = MIN(size, (brdp->pagesize -
			(((unsigned long) uiop->uio_offset) % brdp->pagesize)));
		error = uiomove(memptr, n, uiop);
		if ((uiop->uio_resid == 0) || error)
			break;
	}
	EBRDDISABLE(brdp);
	enable_intr();

	return(error);
}

/*****************************************************************************/

/*
 *	The "staliomem" device is also required to do some special operations
 *	on the board. We need to be able to send an interrupt to the board,
 *	reset it, and start/stop it.
 */

static int stli_memioctl(dev_t dev, int cmd, caddr_t data, int flag, struct proc *p)
{
	stlibrd_t	*brdp;
	int		brdnr, rc;

#if DEBUG
	printf("stli_memioctl(dev=%x,cmd=%x,data=%x,flag=%x)\n", (int) dev,
		cmd, (int) data, flag);
#endif

	brdnr = dev & 0x7;
	brdp = stli_brds[brdnr];
	if (brdp == (stlibrd_t *) NULL)
		return(ENODEV);
	if (brdp->state == 0)
		return(ENODEV);

	rc = 0;

	switch (cmd) {
	case STL_BINTR:
		EBRDINTR(brdp);
		break;
	case STL_BSTART:
		rc = stli_startbrd(brdp);
		break;
	case STL_BSTOP:
		brdp->state &= ~BST_STARTED;
		break;
	case STL_BRESET:
		brdp->state &= ~BST_STARTED;
		EBRDRESET(brdp);
		if (stli_shared == 0) {
			if (brdp->reenable != NULL)
				(* brdp->reenable)(brdp);
		}
		break;
	case COM_GETPORTSTATS:
		rc = stli_getportstats((stliport_t *) NULL, data);
		break;
	case COM_CLRPORTSTATS:
		rc = stli_clrportstats((stliport_t *) NULL, data);
		break;
	case COM_GETBRDSTATS:
		rc = stli_getbrdstats(data);
		break;
	default:
		rc = ENOTTY;
		break;
	}

	return(rc);
}

/*****************************************************************************/
