/*-
 *  dgb.c $Id: dgb.c,v 1.19 1996/06/18 01:21:40 bde Exp $
 *
 *  Digiboard driver.
 *
 *  Stage 1. "Better than nothing".
 *
 *  Based on sio driver by Bruce Evans and on Linux driver by Troy 
 *  De Jongh <troyd@digibd.com> or <troyd@skypoint.com> 
 *  which is under GNU General Public License version 2 so this driver 
 *  is forced to be under GPL 2 too.
 *
 *  Written by Serge Babkin,
 *      Joint Stock Commercial Bank "Chelindbank"
 *      (Chelyabinsk, Russia)
 *      babkin@hq.icb.chel.su
 */

#include "dgb.h"

#if NDGB > 0 

/* the overall number of ports controlled by this driver */

#ifndef NDGBPORTS
#	define NDGBPORTS (NDGB*16)
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/reboot.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/dkstat.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/malloc.h>
#include <sys/syslog.h>
#ifdef DEVFS
#include <sys/devfsext.h>
#endif /*DEVFS*/

#include <machine/clock.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <i386/isa/isa_device.h>

#include <gnu/i386/isa/dgbios.h>
#include <gnu/i386/isa/dgfep.h>
#include <gnu/i386/isa/dgreg.h>

#define	CALLOUT_MASK		0x80
#define	CONTROL_MASK		0x60
#define	CONTROL_INIT_STATE	0x20
#define	CONTROL_LOCK_STATE	0x40
#define UNIT_MASK			0x30000
#define PORT_MASK			0x1F
#define	DEV_TO_UNIT(dev)	(MINOR_TO_UNIT(minor(dev)))
#define	MINOR_MAGIC_MASK	(CALLOUT_MASK | CONTROL_MASK)
#define	MINOR_TO_UNIT(mynor)	(((mynor) & UNIT_MASK)>>16)
#define MINOR_TO_PORT(mynor)	((mynor) & PORT_MASK)

/* types.  XXX - should be elsewhere */
typedef u_char	bool_t;		/* boolean */

/* digiboard port structure */
struct dgb_p {
	bool_t	status;

	u_char unit;           /* board unit number */
	u_char pnum;           /* port number */
	u_char omodem;         /* FEP output modem status     */
	u_char imodem;         /* FEP input modem status      */
	u_char modemfake;      /* Modem values to be forced   */
	u_char modem;          /* Force values                */
	u_char hflow;
	u_char dsr;
	u_char dcd;
	u_char stopc;
	u_char startc;
	u_char stopca;
	u_char startca;
	u_char fepstopc;
	u_char fepstartc;
	u_char fepstopca;
	u_char fepstartca;
	u_char txwin;
	u_char rxwin;
	ushort fepiflag;
	ushort fepcflag;
	ushort fepoflag;
	ushort txbufhead;
	ushort txbufsize;
	ushort rxbufhead;
	ushort rxbufsize;
	int close_delay;
	int count;
	int blocked_open;
	int event;
	int asyncflags;
	u_long statusflags;
	u_char *txptr;
	u_char *rxptr;
	struct board_chan *brdchan;
	struct tty *tty;

	bool_t  active_out;	/* nonzero if the callout device is open */
	u_int	wopeners;	/* # processes waiting for DCD in open() */

	/* Initial state. */
	struct termios	it_in;	/* should be in struct tty */
	struct termios	it_out;

	/* Lock state. */
	struct termios	lt_in;	/* should be in struct tty */
	struct termios	lt_out;

	/* flags of state, are used in sleep() too */
	u_char closing;	/* port is being closed now */
	u_char draining; /* port is being drained now */
	u_char used;	/* port is being used now */
	u_char mustdrain; /* data must be waited to drain in dgbparam() */
#ifdef	DEVFS
	struct	{
		void	*tty;
		void	*init;
		void	*lock;
		void	*cua;
	} devfs_token;
#endif
};

/* Digiboard per-board structure */
struct dgb_softc {
	/* struct board_info */
	u_char status;	/* status: DISABLED/ENABLED */
	u_char unit;	/* unit number */
	u_char type;	/* type of card: PCXE, PCXI, PCXEVE */
	u_char altpin;	/* do we need alternate pin setting ? */
	ushort numports;	/* number of ports on card */
	ushort port;	/* I/O port */
	u_char *vmem; /* virtual memory address */
	long pmem; /* physical memory address */
	int mem_seg;  /* internal memory segment */
	struct dgb_p *ports;	/* pointer to array of port descriptors */
	struct tty *ttys;	/* pointer to array of TTY structures */
	volatile struct global_data *mailbox;
	};
	

static struct dgb_softc dgb_softc[NDGB];
static struct dgb_p dgb_ports[NDGBPORTS];
static struct tty dgb_tty[NDGBPORTS];

/*
 * The public functions in the com module ought to be declared in a com-driver
 * system header.
 */

/* Interrupt handling entry points. */
static void	dgbpoll		__P((void *unit_c));

/* Device switch entry points. */
#define	dgbreset	noreset
#define	dgbmmap		nommap
#define	dgbstrategy	nostrategy

static	int	dgbattach	__P((struct isa_device *dev));
static	int	dgbprobe	__P((struct isa_device *dev));

static void fepcmd(struct dgb_p *port, int cmd, int op1, int op2,
	int ncmds, int bytecmd);

static	void	dgbstart	__P((struct tty *tp));
static	int	dgbparam	__P((struct tty *tp, struct termios *t));
static void dgbhardclose	__P((struct dgb_p *port));
static	void	dgb_drain_or_flush	__P((struct dgb_p *port));
static	int	dgbdrain	__P((struct dgb_p *port));
static	void	dgb_pause	__P((void *chan));
static	void	wakeflush	__P((void *p));


struct isa_driver	dgbdriver = {
	dgbprobe, dgbattach, "dgb",0
};

static	d_open_t	dgbopen;
static	d_close_t	dgbclose;
static	d_read_t	dgbread;
static	d_write_t	dgbwrite;
static	d_ioctl_t	dgbioctl;
static	d_stop_t	dgbstop;
static	d_devtotty_t	dgbdevtotty;

#define CDEV_MAJOR 58
static struct cdevsw dgb_cdevsw = 
	{ dgbopen,	dgbclose,	dgbread,	dgbwrite,	/*58*/
	  dgbioctl,	dgbstop,	noreset,	dgbdevtotty, /* dgb */
	  ttselect,	nommap,		NULL,	"dgb",	NULL,	-1 };

static	speed_t	dgbdefaultrate = TTYDEF_SPEED;

static	struct speedtab dgbspeedtab[] = {
	0,	0, /* old (sysV-like) Bx codes */
	50,	1,
	75,	2,
	110, 3,
	134, 4,
	150, 5,
	200, 6,
	300, 7,
	600, 8,
	1200, 9,
	1800, 10,
	2400, 11,
	4800, 12,
	9600, 13,
	19200, 14,
	38400, 15,
	57600, (02000 | 1),	/* B50 & fast baud table */
	115200, (02000 | 2), /* B100 & fast baud table */
	-1,	-1
};

static int dgbdebug=0;
SYSCTL_INT(_debug, OID_AUTO, dgb_debug, CTLFLAG_RW,
	&dgbdebug, 0, "");

static int setwin __P((struct dgb_softc *sc, unsigned addr));
static int setinitwin __P((struct dgb_softc *sc, unsigned addr));
static void hidewin __P((struct dgb_softc *sc));
static void towin __P((struct dgb_softc *sc, int win));

static inline int 
setwin(sc,addr)
	struct dgb_softc *sc;
	unsigned int addr;
{
	if(sc->type==PCXEVE) {
		outb(sc->port+1, FEPWIN|(addr>>13));
		DPRINT3("dgb%d: switched to window 0x%x\n",sc->unit,addr>>13);
		return (addr & 0x1FFF);
	} else {
		outb(sc->port,FEPMEM);
		return addr;
	}
}

static inline int 
setinitwin(sc,addr)
	struct dgb_softc *sc;
	unsigned int addr;
{
	if(sc->type==PCXEVE) {
		outb(sc->port+1, FEPWIN|(addr>>13));
		DPRINT3("dgb%d: switched to window 0x%x\n",sc->unit,addr>>13);
		return (addr & 0x1FFF);
	} else {
		outb(sc->port,inb(sc->port)|FEPMEM);
		return addr;
	}
}

static inline void
hidewin(sc)
	struct dgb_softc *sc;
{
	if(sc->type==PCXEVE)
		outb(sc->port+1, 0);
	else
		outb(sc->port,0);
}

static inline void
towin(sc,win)
	struct dgb_softc *sc;
	int win;
{
	if(sc->type==PCXEVE) {
		outb(sc->port+1, win);
	} else {
		outb(sc->port,FEPMEM);
	}
}

static int
dgbprobe(dev)
	struct isa_device	*dev;
{
	struct dgb_softc *sc= &dgb_softc[dev->id_unit];
	int i, v;
	u_long win_size;  /* size of vizible memory window */
	int unit=dev->id_unit;

	sc->unit=dev->id_unit;
	sc->port=dev->id_iobase;

	if(dev->id_flags & DGBFLAG_ALTPIN)
		sc->altpin=1;
	else
		sc->altpin=0;

	/* left 24 bits only (ISA address) */
	sc->pmem=((long)dev->id_maddr & 0xFFFFFF); 
	
	DPRINT4("dgb%d: port 0x%x mem 0x%x\n",unit,sc->port,sc->pmem);

	outb(sc->port, FEPRST);
	sc->status=DISABLED;

	for(i=0; i< 1000; i++) {
		DELAY(1);
		if( (inb(sc->port) & FEPMASK) == FEPRST ) {
			sc->status=ENABLED;
			DPRINT3("dgb%d: got reset after %d us\n",unit,i);
			break;
		}
	}

	if(sc->status!=ENABLED) {
		DPRINT2("dgb%d: failed to respond\n",dev->id_unit);
		return 0;
	}

	/* check type of card and get internal memory characteristics */

	v=inb(sc->port);

	if( v & 0x1 ) {
		switch( v&0x30 ) {
		case 0:
			sc->mem_seg=0xF000;
			win_size=0x10000;
			printf("dgb%d: PC/Xi 64K\n",dev->id_unit);
			break;
		case 0x10:
			sc->mem_seg=0xE000;
			win_size=0x20000;
			printf("dgb%d: PC/Xi 128K\n",dev->id_unit);
			break;
		case 0x20:
			sc->mem_seg=0xC000;
			win_size=0x40000;
			printf("dgb%d: PC/Xi 256K\n",dev->id_unit);
			break;
		default: /* case 0x30: */
			sc->mem_seg=0x8000;
			win_size=0x80000;
			printf("dgb%d: PC/Xi 512K\n",dev->id_unit);
			break;
		}
		sc->type=PCXI;
	} else {
		outb(sc->port, 1);
		v=inb(sc->port);

		if( v & 0x1 ) {
			printf("dgb%d: PC/Xm isn't supported\n",dev->id_unit);
			sc->status=DISABLED;
			return 0;
			}

		sc->mem_seg=0xF000;

		if(dev->id_flags==DGBFLAG_NOWIN || ( v&0xC0 )==0) {
			win_size=0x10000;
			printf("dgb%d: PC/Xe 64K\n",dev->id_unit);
			sc->type=PCXE;
		} else {
			win_size=0x2000;
			printf("dgb%d: PC/Xe 64/8K (windowed)\n",dev->id_unit);
			sc->type=PCXEVE;
			if((u_long)sc->pmem & ~0xFFE000) {
				printf("dgb%d: warning: address 0x%x truncated to 0x%x\n",
					dev->id_unit, sc->pmem,
					(long)sc->pmem & 0xFFE000);

				dev->id_maddr= (u_char *)( (long)sc->pmem & 0xFFE000 );
			}
		}
	}

	/* save size of vizible memory segment */
	dev->id_msize=win_size;

	/* map memory */
	dev->id_maddr=sc->vmem=pmap_mapdev(sc->pmem,dev->id_msize);

	outb(sc->port, FEPCLR); /* drop RESET */

	return 4; /* we need I/O space of 4 ports */
}

static int
dgbattach(dev)
	struct isa_device	*dev;
{
	int unit=dev->id_unit;
	struct dgb_softc *sc= &dgb_softc[dev->id_unit];
	int i, t;
	u_char *mem;
	u_char *ptr;
	int addr;
	struct dgb_p *port;
	struct board_chan *bc;
	int shrinkmem;
	int nfails;
	ushort *pstat;
	int lowwater;
	int nports=0;

	if(sc->status!=ENABLED) {
		DPRINT2("dbg%d: try to attach a disabled card\n",unit);
		return 0;
		}

	mem=sc->vmem;

	DPRINT3("dgb%d: internal memory segment 0x%x\n",unit,sc->mem_seg);

	outb(sc->port, FEPRST); DELAY(1);

	for(i=0; (inb(sc->port) & FEPMASK) != FEPRST ; i++) {
		if(i>10000) {
			printf("dgb%d: 1st reset failed\n",dev->id_unit);
			sc->status=DISABLED;
			hidewin(sc);
			return 0;
		}
		DELAY(1);
	}

	DPRINT3("dgb%d: got reset after %d us\n",unit,i);

	/* for PCXEVE set up interrupt and base address */

	if(sc->type==PCXEVE) {
		t=(((u_long)sc->pmem>>8) & 0xFFE0) | 0x10 /* enable windowing */;

		/* IRQ isn't used */
#if 0
		switch(dev->id_irq) {
		case IRQ3:
			t|=0x1;
			break;
		case IRQ5:
			t|=2;
			break;
		case IRQ7:
			t|=3;
			break;
		case IRQ10:
			t|=4;
			break;
		case IRQ11:
			t|=5;
			break;
		case IRQ12:
			t|=6;
			break;
		case IRQ15:
			t|=7;
			break;
		default:
			printf("dgb%d: wrong IRQ mask 0x%x\n",dev->id_unit,dev->id_irq);
			sc->status=DISABLED;
			return 0;
		}
#endif

		outb(sc->port+2,t & 0xFF);
		outb(sc->port+3,t>>8);
	} else if(sc->type==PCXE) {
		t=(((u_long)sc->pmem>>8) & 0xFFE0) /* disable windowing */;
		outb(sc->port+2,t & 0xFF);
		outb(sc->port+3,t>>8);
	}


	if(sc->type==PCXI || sc->type==PCXE) {
		outb(sc->port, FEPRST|FEPMEM); DELAY(1);

		for(i=0; (inb(sc->port) & FEPMASK) != (FEPRST|FEPMEM) ; i++) {
			if(i>10000) {
				printf("dgb%d: 2nd reset failed\n",dev->id_unit);
				sc->status=DISABLED;
				hidewin(sc);
				return 0;
			}
			DELAY(1);
		}

		DPRINT3("dgb%d: got memory after %d us\n",unit,i);
	}

	mem=sc->vmem;

	/* very short memory test */

	addr=setinitwin(sc,BOTWIN);
	*(u_long *)(mem+addr) = 0xA55A3CC3;
	if(*(u_long *)(mem+addr)!=0xA55A3CC3) {
		printf("dgb%d: 1st memory test failed\n",dev->id_unit);
		sc->status=DISABLED;
		hidewin(sc);
		return 0;
	}
		
	addr=setinitwin(sc,TOPWIN);
	*(u_long *)(mem+addr) = 0x5AA5C33C;
	if(*(u_long *)(mem+addr)!=0x5AA5C33C) {
		printf("dgb%d: 2nd memory test failed\n",dev->id_unit);
		sc->status=DISABLED;
		hidewin(sc);
		return 0;
	}
		
	addr=setinitwin(sc,BIOSCODE+((0xF000-sc->mem_seg)<<4));
	*(u_long *)(mem+addr) = 0x5AA5C33C;
	if(*(u_long *)(mem+addr)!=0x5AA5C33C) {
		printf("dgb%d: 3rd (BIOS) memory test failed\n",dev->id_unit);
	}
		
	addr=setinitwin(sc,MISCGLOBAL);
	for(i=0; i<16; i++) {
		mem[addr+i]=0;
	}

	if(sc->type==PCXI || sc->type==PCXE) {

		addr=BIOSCODE+((0xF000-sc->mem_seg)<<4);

		DPRINT3("dgb%d: BIOS local address=0x%x\n",unit,addr);

		ptr= mem+addr;

		for(i=0; i<pcxx_nbios; i++, ptr++)
			*ptr = pcxx_bios[i];

		ptr= mem+addr;

		nfails=0;
		for(i=0; i<pcxx_nbios; i++, ptr++)
			if( *ptr != pcxx_bios[i] ) {
				DPRINT5("dgb%d: wrong code in BIOS at addr 0x%x : \
0x%x instead of 0x%x\n", unit, ptr-(mem+addr), *ptr, pcxx_bios[i] );

				if(++nfails>=5) {
					printf("dgb%d: 4th memory test (BIOS load) fails\n",unit);
					break;
					}
				}

		outb(sc->port,FEPMEM);

		for(i=0; (inb(sc->port) & FEPMASK) != FEPMEM ; i++) {
			if(i>10000) {
				printf("dgb%d: BIOS start failed\n",dev->id_unit);
				sc->status=DISABLED;
				hidewin(sc);
				return 0;
			}
			DELAY(1);
		}

		DPRINT3("dgb%d: reset dropped after %d us\n",unit,i);

		for(i=0; i<200000; i++) {
			if( *((ushort *)(mem+MISCGLOBAL)) == *((ushort *)"GD") )
				goto load_fep;
			DELAY(1);
		}
		printf("dgb%d: BIOS download failed\n",dev->id_unit);
		DPRINT4("dgb%d: code=0x%x must be 0x%x\n",
			dev->id_unit,
			*((ushort *)(mem+MISCGLOBAL)),
			*((ushort *)"GD"));

		sc->status=DISABLED;
		hidewin(sc);
		return 0;
	}

	if(sc->type==PCXEVE) {
		/* set window 7 */
		outb(sc->port+1,0xFF);

		ptr= mem+(BIOSCODE & 0x1FFF);

		for(i=0; i<pcxx_nbios; i++)
			*ptr++ = pcxx_bios[i];

		ptr= mem+(BIOSCODE & 0x1FFF);

		nfails=0;
		for(i=0; i<pcxx_nbios; i++, ptr++)
			if( *ptr != pcxx_bios[i] ) {
				DPRINT5("dgb%d: wrong code in BIOS at addr 0x%x : \
0x%x instead of 0x%x\n", unit, ptr-(mem+addr), *ptr, pcxx_bios[i] );

				if(++nfails>=5) {
					printf("dgb%d: 4th memory test (BIOS load) fails\n",unit);
					break;
					}
				}

		outb(sc->port,FEPCLR);

		setwin(sc,0);

		for(i=0; (inb(sc->port) & FEPMASK) != FEPCLR ; i++) {
			if(i>10000) {
				printf("dgb%d: BIOS start failed\n",dev->id_unit);
				sc->status=DISABLED;
				hidewin(sc);
				return 0;
			}
			DELAY(1);
		}

		DPRINT3("dgb%d: reset dropped after %d us\n",unit,i);

		addr=setwin(sc,MISCGLOBAL);

		for(i=0; i<200000; i++) {
			if(*(ushort *)(mem+addr)== *(ushort *)"GD")
				goto load_fep;
			DELAY(1);
		}
		printf("dgb%d: BIOS download failed\n",dev->id_unit);
		DPRINT5("dgb%d: Error#(0x%x,0x%x) code=0x%x\n",
			dev->id_unit,
			*(ushort *)(mem+0xC12),
			*(ushort *)(mem+0xC14),
			*(ushort *)(mem+MISCGLOBAL));

		sc->status=DISABLED;
		hidewin(sc);
		return 0;
	}

load_fep:
	DPRINT2("dgb%d: BIOS loaded\n",dev->id_unit);

	addr=setwin(sc,FEPCODE);

	ptr= mem+addr;

	for(i=0; i<pcxx_ncook; i++)
		*ptr++ = pcxx_cook[i];

	addr=setwin(sc,MBOX);
	*(ushort *)(mem+addr+ 0)=2;
	*(ushort *)(mem+addr+ 2)=sc->mem_seg+FEPCODESEG;
	*(ushort *)(mem+addr+ 4)=0;
	*(ushort *)(mem+addr+ 6)=FEPCODESEG;
	*(ushort *)(mem+addr+ 8)=0;
	*(ushort *)(mem+addr+10)=pcxx_ncook;
	
	outb(sc->port,FEPMEM|FEPINT); /* send interrupt to BIOS */
	outb(sc->port,FEPMEM);

	for(i=0; *(ushort *)(mem+addr)!=0; i++) {
		if(i>200000) {
			printf("dgb%d: FEP code download failed\n",unit);
			DPRINT3("dgb%d: code=0x%x must be 0\n", unit,
				*(ushort *)(mem+addr));
			sc->status=DISABLED;
			hidewin(sc);
			return 0;
		}
	}

	DPRINT2("dgb%d: FEP code loaded\n",unit);

	*(ushort *)(mem+setwin(sc,FEPSTAT))=0;
	addr=setwin(sc,MBOX);
	*(ushort *)(mem+addr+0)=1;
	*(ushort *)(mem+addr+2)=FEPCODESEG;
	*(ushort *)(mem+addr+4)=0x4;

	outb(sc->port,FEPINT); /* send interrupt to BIOS */
	outb(sc->port,FEPCLR);

	addr=setwin(sc,FEPSTAT);
	for(i=0; *(ushort *)(mem+addr)!= *(ushort *)"OS"; i++) {
		if(i>200000) {
			printf("dgb%d: FEP/OS start failed\n",dev->id_unit);
			sc->status=DISABLED;
			hidewin(sc);
			return 0;
		}
	}

	DPRINT2("dgb%d: FEP/OS started\n",dev->id_unit);

	sc->numports= *(ushort *)(mem+setwin(sc,NPORT));

	printf("dgb%d: %d ports\n",unit,sc->numports);

	if(sc->numports > MAX_DGB_PORTS) {
		printf("dgb%d: too many ports\n",unit);
		sc->status=DISABLED;
		hidewin(sc);
		return 0;
	}

	if(nports+sc->numports>NDGBPORTS) {
		printf("dgb%d: only %d ports are usable\n", unit, NDGBPORTS-nports);
		sc->numports=NDGBPORTS-nports;
	}

	/* allocate port and tty structures */
	sc->ports=&dgb_ports[nports];
	sc->ttys=&dgb_tty[nports];
	nports+=sc->numports;

	addr=setwin(sc,PORTBASE);
	pstat=(ushort *)(mem+addr);

	for(i=0; i<sc->numports && pstat[i]; i++)
		if(pstat[i])
			sc->ports[i].status=ENABLED;
		else {
			sc->ports[i].status=DISABLED;
			printf("dgb%d: port %d is broken\n", unit, i);
		}

	/* We should now init per-port structures */
	bc=(struct board_chan *)(mem + CHANSTRUCT);
	sc->mailbox=(struct global_data *)(mem + FEP_GLOBAL);

	if(sc->numports<3)
		shrinkmem=1;
	else
		shrinkmem=0;

	for(i=0; i<sc->numports; i++, bc++) {
		port= &sc->ports[i];

		port->tty=&sc->ttys[i];
		port->unit=unit;

		port->brdchan=bc;

		if(sc->altpin) {
			port->dsr=CD;
			port->dcd=DSR;
		} else {
			port->dcd=CD;
			port->dsr=DSR;
		}

		port->pnum=i;

		if(shrinkmem) {
			DPRINT2("dgb%d: shrinking memory\n",unit);
			fepcmd(port, SETBUFFER, 32, 0, 0, 0);
			shrinkmem=0;
			}

		if(sc->type!=PCXEVE) {
			port->txptr=mem+((bc->tseg-sc->mem_seg)<<4);
			port->rxptr=mem+((bc->rseg-sc->mem_seg)<<4);
			port->txwin=port->rxwin=0;
		} else {
			port->txptr=mem+( ((bc->tseg-sc->mem_seg)<<4) & 0x1FFF );
			port->rxptr=mem+( ((bc->rseg-sc->mem_seg)<<4) & 0x1FFF );
			port->txwin=FEPWIN | ((bc->tseg-sc->mem_seg)>>9);
			port->rxwin=FEPWIN | ((bc->rseg-sc->mem_seg)>>9);
		}

		port->txbufhead=0;
		port->rxbufhead=0;
		port->txbufsize=bc->tmax+1;
		port->rxbufsize=bc->rmax+1;

		lowwater= (port->txbufsize>=2000) ? 1024 : (port->txbufsize/2);
		fepcmd(port, STXLWATER, lowwater, 0, 10, 0);
		fepcmd(port, SRXLWATER, port->rxbufsize/4, 0, 10, 0);
		fepcmd(port, SRXHWATER, 3*port->rxbufsize/4, 0, 10, 0);

		bc->edelay=100;
		bc->idata=1;

		port->startc=bc->startc;
		port->startca=bc->startca;
		port->stopc=bc->stopc;
		port->stopca=bc->stopca;
			
		port->close_delay=50;

		/*
		 * We don't use all the flags from <sys/ttydefaults.h> since they
		 * are only relevant for logins.  It's important to have echo off
		 * initially so that the line doesn't start blathering before the
		 * echo flag can be turned off.
		 */
		port->it_in.c_iflag = TTYDEF_IFLAG;
		port->it_in.c_oflag = TTYDEF_OFLAG;
		port->it_in.c_cflag = TTYDEF_CFLAG;
		port->it_in.c_lflag = TTYDEF_LFLAG;
		termioschars(&port->it_in);
		port->it_in.c_ispeed = port->it_in.c_ospeed = dgbdefaultrate;
		port->it_out = port->it_in;
#ifdef	DEVFS
/*XXX*/ /* fix the minor numbers */
		port->devfs_token.tty = 
			devfs_add_devswf(&dgb_cdevsw,
					 (unit*32)+i,/*mytical number*/
					 DV_CHR, 0, 0, 0600, "dgb%d.%d", unit, 
					 i);

		port->devfs_token.tty = 
			devfs_add_devswf(&dgb_cdevsw,
					 (unit*32)+i+64,/*mytical number*/
					 DV_CHR, 0, 0, 0600, "idgb%d.%d", unit, 
					 i);

		port->devfs_token.tty = 
			devfs_add_devswf(&dgb_cdevsw,
					 (unit*32)+i+128,/*mytical number*/
					 DV_CHR, 0, 0, 0600, "ldgb%d.%d", unit,
					 i);

		port->devfs_token.tty = 
			devfs_add_devswf(&dgb_cdevsw,
					 (unit*32)+i+192,/*mytical number*/
					 DV_CHR, 0, 0, 0600, "dgbcua%d.%d",
					 unit, i);
#endif
	}

	hidewin(sc);

	/* register the polling function */
	timeout(dgbpoll, (void *)unit, hz/25);

	return 1;
}

/* ARGSUSED */
static	int
dgbopen(dev, flag, mode, p)
	dev_t		dev;
	int		flag;
	int		mode;
	struct proc	*p;
{
	struct dgb_softc *sc;
	struct tty *tp;
	int unit;
	int mynor;
	int pnum;
	struct dgb_p *port;
	int s;
	int error;
	struct board_chan *bc;

	error=0;

	mynor=minor(dev);
	unit=MINOR_TO_UNIT(mynor);
	pnum=MINOR_TO_PORT(mynor);

	if(unit >= NDGB) {
		DPRINT2("dgb%d: try to open a nonexisting card\n",unit);
		return ENXIO;
	}

	sc=&dgb_softc[unit];

	if(sc->status!=ENABLED) {
		DPRINT2("dgb%d: try to open a disabled card\n",unit);
		return ENXIO;
	}

	if(pnum>=sc->numports) {
		DPRINT3("dgb%d: try to open non-existing port %d\n",unit,pnum);
		return ENXIO;
	}

	if(mynor & CONTROL_MASK)
		return 0;

	tp=&sc->ttys[pnum];
	port=&sc->ports[pnum];
	bc=port->brdchan;

open_top:
	
	s=spltty();

	while(port->closing) {
		error=tsleep(&port->closing, TTOPRI|PCATCH, "dgocl", 0);

		if(error) {
			DPRINT4("dgb%d: port %d: tsleep(dgocl) error=%d\n",unit,pnum,error);
			goto out;
		}
	}

	if (tp->t_state & TS_ISOPEN) {
		/*
		 * The device is open, so everything has been initialized.
		 * Handle conflicts.
		 */
		if (mynor & CALLOUT_MASK) {
			if (!port->active_out) {
				error = EBUSY;
				DPRINT4("dgb%d: port %d: BUSY error=%d\n",unit,pnum,error);
				goto out;
			}
		} else {
			if (port->active_out) {
				if (flag & O_NONBLOCK) {
					error = EBUSY;
					DPRINT4("dgb%d: port %d: BUSY error=%d\n",unit,pnum,error);
					goto out;
				}
				error =	tsleep(&port->active_out,
					       TTIPRI | PCATCH, "dgbi", 0);
				if (error != 0) {
					DPRINT4("dgb%d: port %d: tsleep(dgbi) error=%d\n",
						unit,pnum,error);
					goto out;
				}
				splx(s);
				goto open_top;
			}
		}
		if (tp->t_state & TS_XCLUDE && p->p_ucred->cr_uid != 0) {
			error = EBUSY;
			goto out;
		}
	} else {
		/*
		 * The device isn't open, so there are no conflicts.
		 * Initialize it.  Initialization is done twice in many
		 * cases: to preempt sleeping callin opens if we are
		 * callout, and to complete a callin open after DCD rises.
		 */
		tp->t_oproc=dgbstart;
		tp->t_param=dgbparam;
		tp->t_dev=dev;
		tp->t_termios= (mynor & CALLOUT_MASK) ?
							port->it_out :
							port->it_in;

		setwin(sc,0);
		port->imodem=bc->mstat;
		bc->rout=bc->rin; /* clear input queue */
		bc->idata=1;

		hidewin(sc);

		port->wopeners++;
		error=dgbparam(tp, &tp->t_termios);
		port->wopeners--;

		if(error!=0) {
			DPRINT4("dgb%d: port %d: dgbparam error=%d\n",unit,pnum,error);
			goto out;
		}

		ttsetwater(tp);

		/* handle fake DCD for callout devices */
		/* and initial DCD */

		if( (port->imodem & port->dcd) || mynor & CALLOUT_MASK )
			linesw[tp->t_line].l_modem(tp,1);

	}

	/*
	 * Wait for DCD if necessary.
	 */
	if (!(tp->t_state & TS_CARR_ON) && !(mynor & CALLOUT_MASK)
	    && !(tp->t_cflag & CLOCAL) && !(flag & O_NONBLOCK)) {
		++port->wopeners;
		error = tsleep(TSA_CARR_ON(tp), TTIPRI | PCATCH, "dgdcd", 0);
		--port->wopeners;
		if (error != 0) {
			DPRINT4("dgb%d: port %d: tsleep(dgdcd) error=%d\n",unit,pnum,error);
			goto out;
		}
		splx(s);
		goto open_top;
	}
	error =	linesw[tp->t_line].l_open(dev, tp);
	DPRINT4("dgb%d: port %d: l_open error=%d\n",unit,pnum,error);

	if (tp->t_state & TS_ISOPEN && mynor & CALLOUT_MASK)
		port->active_out = TRUE;

	port->used=1;

	/* If any port is open (i.e. the open() call is completed for it) 
	 * the device is busy
	 */

out:
	splx(s);

	if( !(tp->t_state & TS_ISOPEN) && port->wopeners==0 )
		dgbhardclose(port);

	DPRINT4("dgb%d: port %d: open() returns %d\n",unit,pnum,error);

	return error;
}

/*ARGSUSED*/
static	int
dgbclose(dev, flag, mode, p)
	dev_t		dev;
	int		flag;
	int		mode;
	struct proc	*p;
{
	int		mynor;
	struct tty	*tp;
	int unit, pnum;
	struct dgb_softc *sc;
	struct dgb_p *port;
	int s;
	int i;

	mynor=minor(dev);
	unit=MINOR_TO_UNIT(mynor);
	pnum=MINOR_TO_PORT(mynor);

	sc=&dgb_softc[unit];
	tp=&sc->ttys[pnum];
	port=sc->ports+pnum;

	if(mynor & CONTROL_MASK)
		return 0;

	DPRINT3("dgb%d: port %d: closing\n",unit,pnum);

	s=spltty();

	port->closing=1;
	linesw[tp->t_line].l_close(tp,flag);
	dgb_drain_or_flush(port);
	dgbhardclose(port);
	ttyclose(tp);
	port->closing=0; wakeup(&port->closing);
	port->used=0;

	/* mark the card idle when all ports are closed */

	for(i=0; i<sc->numports; i++)
		if(sc->ports[i].used)
			break;

	splx(s);

	wakeup(TSA_CARR_ON(tp));
	wakeup(&port->active_out);
	port->active_out=0;

	return 0;
}

static void
dgbhardclose(port)
	struct dgb_p *port;
{
	struct dgb_softc *sc=&dgb_softc[port->unit];
	struct board_chan *bc=port->brdchan;

	setwin(sc,0);

	bc->idata=0; bc->iempty=0; bc->ilow=0;
	if(port->tty->t_cflag & HUPCL) {
		port->omodem &= ~(RTS|DTR);
		fepcmd(port, SETMODEM, 0, DTR|RTS, 0, 1);
	}

	hidewin(sc);

	timeout(dgb_pause, &port->brdchan, hz/2);
	tsleep(&port->brdchan, TTIPRI | PCATCH, "dgclo", 0);
}

static void 
dgb_pause(chan)
	void *chan;
{
wakeup((caddr_t)chan);
}


static	int
dgbread(dev, uio, flag)
	dev_t		dev;
	struct uio	*uio;
	int		flag;
{
	int		mynor;
	struct tty	*tp;
	int error, unit, pnum;

	mynor=minor(dev);
	if (mynor & CONTROL_MASK)
		return (ENODEV);
	unit=MINOR_TO_UNIT(mynor);
	pnum=MINOR_TO_PORT(mynor);

	tp=&dgb_softc[unit].ttys[pnum];

	error=linesw[tp->t_line].l_read(tp, uio, flag);
	DPRINT4("dgb%d: port %d: read() returns %d\n",unit,pnum,error);

	return error;
}

static	int
dgbwrite(dev, uio, flag)
	dev_t		dev;
	struct uio	*uio;
	int		flag;
{
	int		mynor;
	struct tty	*tp;
	int error, unit, pnum;

	mynor=minor(dev);
	if (mynor & CONTROL_MASK)
		return (ENODEV);

	unit=MINOR_TO_UNIT(mynor);
	pnum=MINOR_TO_PORT(mynor);

	tp=&dgb_softc[unit].ttys[pnum];

	error=linesw[tp->t_line].l_write(tp, uio, flag);
	DPRINT4("dgb%d: port %d: write() returns %d\n",unit,pnum,error);

	return error;
}

static void
dgbpoll(unit_c)
	void *unit_c;
{
	int unit=(int)unit_c;
	int pnum;
	struct dgb_p *port;
	struct dgb_softc *sc=&dgb_softc[unit];
	int head, tail;
	u_char *eventbuf;
	int event, mstat, lstat;
	struct board_chan *bc;
	struct tty *tp;
	int rhead, rtail;
	int whead, wtail;
	int wrapmask;
	int size;
	int c=0;
	u_char *ptr;
	int ocount;

	if(sc->status==DISABLED) {
		printf("dgb%d: polling of disabled board stopped\n",unit);
		return;
	}
	
	setwin(sc,0);

	head=sc->mailbox->ein;
	tail=sc->mailbox->eout;

	while(head!=tail) {
		if(head >= FEP_IMAX-FEP_ISTART 
		|| tail >= FEP_IMAX-FEP_ISTART 
		|| (head|tail) & 03 ) {
			printf("dgb%d: event queue's head or tail is wrong!\n", unit);
			break;
		}

		eventbuf=sc->vmem+tail+FEP_ISTART;
		pnum=eventbuf[0];
		event=eventbuf[1];
		mstat=eventbuf[2];
		lstat=eventbuf[3];

		port=&sc->ports[pnum];
		tp=&sc->ttys[pnum];

		if(pnum>=sc->numports || port->status==DISABLED) {
			printf("dgb%d: port %d: got event on nonexisting port\n",unit,pnum);
		} else if(port->used || port->wopeners>0 ) {

			bc=port->brdchan;

			if( !(event & ALL_IND) ) 
				printf("dgb%d: port%d: ? event 0x%x mstat 0x%x lstat 0x%x\n",
					unit, pnum, event, mstat, lstat);

			if(event & DATA_IND) {
				DPRINT3("dgb%d: port %d: DATA_IND\n",unit,pnum);

				wrapmask=port->rxbufsize-1;

				rhead=bc->rin & wrapmask; 
				rtail=bc->rout & wrapmask;

				if( !(tp->t_cflag & CREAD) || !port->used ) {
					bc->rout=rhead;
					goto end_of_data;
				}

				if(bc->orun) {
					printf("dgb%d: port%d: overrun\n", unit, pnum);
					bc->orun=0;
				}

				while(rhead!=rtail) {
					DPRINT5("dgb%d: port %d: p rx head=%d tail=%d\n",
						unit,pnum,rhead,rtail);

					if(rhead>rtail)
						size=rhead-rtail;
					else
						size=port->rxbufsize-rtail;

					ptr=port->rxptr+rtail;

					for(c=0; c<size; c++) {
						int chr;

						towin(sc,port->rxwin);
						chr= *ptr++;

#if 0
						if(chr>=' ' && chr<127)
							DPRINT4("dgb%d: port %d: got char '%c'\n",
								unit,pnum,chr);
						else
							DPRINT4("dgb%d: port %d: got char 0x%x\n",
								unit,pnum,chr);
#endif

						hidewin(sc);
						linesw[tp->t_line].l_rint(chr, tp);
					}

					setwin(sc,0);
					rtail= (rtail + size) & wrapmask;
					bc->rout=rtail;
					rhead=bc->rin & wrapmask; 
				}
					
			end_of_data:
			}

			if(event & MODEMCHG_IND) {
				DPRINT3("dgb%d: port %d: MODEMCHG_IND\n",unit,pnum);
				port->imodem=mstat;
				if(mstat & port->dcd) {
					hidewin(sc);
					linesw[tp->t_line].l_modem(tp,1);
					setwin(sc,0);
					wakeup(TSA_CARR_ON(tp));
				} else {
					hidewin(sc);
					linesw[tp->t_line].l_modem(tp,0);
					setwin(sc,0);
					if( port->draining) {
						port->draining=0;
						wakeup(&port->draining);
					}
				}
			}

			if(event & BREAK_IND) {
				DPRINT3("dgb%d: port %d: BREAK_IND\n",unit,pnum);
				hidewin(sc);
				linesw[tp->t_line].l_rint(TTY_BI, tp);
				setwin(sc,0);
			}

			if(event & (LOWTX_IND | EMPTYTX_IND) ) {
				DPRINT3("dgb%d: port %d: LOWTX_IND or EMPTYTX_IND\n",unit,pnum);

				if( (event & EMPTYTX_IND ) && tp->t_outq.c_cc==0
				&& port->draining) {
					port->draining=0;
					wakeup(&port->draining);
					bc->ilow=0; bc->iempty=0;
				}

				wrapmask=port->txbufsize;

				while( tp->t_outq.c_cc!=0 ) {
#ifndef TS_ASLEEP	/* post 2.0.5 FreeBSD */
					ttwwakeup(tp);
#else
					if(tp->t_outq.c_cc <= tp->t_lowat) {
						if(tp->t_state & TS_ASLEEP) {
							tp->t_state &= ~TS_ASLEEP;
							wakeup(TSA_OLOWAT(tp));
						}
						selwakeup(&tp->t_wsel);
					}
#endif
					setwin(sc,0);

					whead=bc->tin & wrapmask;
					wtail=bc->tout & wrapmask;

					DPRINT5("dgb%d: port%d: p tx head=%d tail=%d\n",
						unit,pnum,whead,wtail);

					if(whead<wtail)
						size=wtail-whead-1;
					else {
						size=port->txbufsize-whead;
						if(wtail==0)
							size--;
					}

					if(size==0) {
						bc->iempty=1; bc->ilow=1;
						goto end_of_buffer;
					}

					towin(sc,port->txwin);

					ocount=q_to_b(&tp->t_outq, port->txptr+whead, size);
					whead+=ocount;

					setwin(sc,0);
					bc->tin=whead;
				}
#ifndef TS_ASLEEP	/* post 2.0.5 FreeBSD */
				ttwwakeup(tp);
#else
				if(tp->t_state & TS_ASLEEP) {
					tp->t_state &= ~TS_ASLEEP;
					wakeup(TSA_OLOWAT(tp));
				}
				tp->t_state &= ~TS_BUSY;
#endif
			end_of_buffer:
			}
			bc->idata=1; 

		} else {
			bc=port->brdchan;
			DPRINT4("dgb%d: port %d: got event 0x%x on closed port\n",
				unit,pnum,event);
			bc->rout=bc->rin;
			bc->idata=bc->iempty=bc->ilow=0;
		}

		tail= (tail+4) & (FEP_IMAX-FEP_ISTART-4);
	}

	sc->mailbox->eout=tail;
	hidewin(sc);

	timeout(dgbpoll, unit_c, hz/25);
}

static	int
dgbioctl(dev, cmd, data, flag, p)
	dev_t		dev;
	int		cmd;
	caddr_t		data;
	int		flag;
	struct proc	*p;
{
	struct dgb_softc *sc;
	int unit, pnum;
	struct dgb_p *port;
	int mynor;
	struct tty *tp;
	struct board_chan *bc;
	int error;
	int s;
	int tiocm_xxx;

	mynor=minor(dev);
	unit=MINOR_TO_UNIT(mynor);
	pnum=MINOR_TO_PORT(mynor);

	sc=&dgb_softc[unit];
	port=&sc->ports[pnum];
	tp=&sc->ttys[pnum];
	bc=port->brdchan;

	if (mynor & CONTROL_MASK) {
		struct termios *ct;

		switch (mynor & CONTROL_MASK) {
		case CONTROL_INIT_STATE:
			ct = mynor & CALLOUT_MASK ? &port->it_out : &port->it_in;
			break;
		case CONTROL_LOCK_STATE:
			ct = mynor & CALLOUT_MASK ? &port->lt_out : &port->lt_in;
			break;
		default:
			return (ENODEV);	/* /dev/nodev */
		}
		switch (cmd) {
		case TIOCSETA:
			error = suser(p->p_ucred, &p->p_acflag);
			if (error != 0)
				return (error);
			*ct = *(struct termios *)data;
			return (0);
		case TIOCGETA:
			*(struct termios *)data = *ct;
			return (0);
		case TIOCGETD:
			*(int *)data = TTYDISC;
			return (0);
		case TIOCGWINSZ:
			bzero(data, sizeof(struct winsize));
			return (0);
		default:
			return (ENOTTY);
		}
	}

	if (cmd == TIOCSETA || cmd == TIOCSETAW || cmd == TIOCSETAF) {
		int	cc;
		struct termios *dt = (struct termios *)data;
		struct termios *lt = mynor & CALLOUT_MASK
				     ? &port->lt_out : &port->lt_in;

		dt->c_iflag = (tp->t_iflag & lt->c_iflag)
			      | (dt->c_iflag & ~lt->c_iflag);
		dt->c_oflag = (tp->t_oflag & lt->c_oflag)
			      | (dt->c_oflag & ~lt->c_oflag);
		dt->c_cflag = (tp->t_cflag & lt->c_cflag)
			      | (dt->c_cflag & ~lt->c_cflag);
		dt->c_lflag = (tp->t_lflag & lt->c_lflag)
			      | (dt->c_lflag & ~lt->c_lflag);
		for (cc = 0; cc < NCCS; ++cc)
			if (lt->c_cc[cc] != 0)
				dt->c_cc[cc] = tp->t_cc[cc];
		if (lt->c_ispeed != 0)
			dt->c_ispeed = tp->t_ispeed;
		if (lt->c_ospeed != 0)
			dt->c_ospeed = tp->t_ospeed;
	}

	if(cmd==TIOCSTOP) {
		setwin(sc,0);
		fepcmd(port, PAUSETX, 0, 0, 0, 0);
		hidewin(sc);
		return 0;
	} else if(cmd==TIOCSTART) {
		setwin(sc,0);
		fepcmd(port, RESUMETX, 0, 0, 0, 0);
		hidewin(sc);
		return 0;
	}

	if(cmd==TIOCSETAW || cmd==TIOCSETAF)
		port->mustdrain=1;

	error = linesw[tp->t_line].l_ioctl(tp, cmd, data, flag, p);

	if (error >= 0)
		return error;
	error = ttioctl(tp, cmd, data, flag);

	port->mustdrain=0;

	if (error >= 0)
		return error;
	s = spltty();
	switch (cmd) {
	case TIOCSBRK:
		error=dgbdrain(port);

		if(error!=0) {
			splx(s);
			return error;
		}

		setwin(sc,0);
	
		/* now it sends 250 millisecond break because I don't know */
		/* how to send an infinite break */

		fepcmd(port, SENDBREAK, 250, 0, 10, 0);
		hidewin(sc);
		break;
	case TIOCCBRK:
		/* now it's empty */
		break;
	case TIOCSDTR:
		DPRINT3("dgb%d: port %d: set DTR\n",unit,pnum);
		port->omodem |= DTR;
		setwin(sc,0);
		fepcmd(port, SETMODEM, port->omodem, RTS, 0, 1);

		if( !(bc->mstat & DTR) ) {
			DPRINT3("dgb%d: port %d: DTR is off\n",unit,pnum);
		}

		hidewin(sc);
		break;
	case TIOCCDTR:
		DPRINT3("dgb%d: port %d: reset DTR\n",unit,pnum);
		port->omodem &= ~DTR;
		setwin(sc,0);
		fepcmd(port, SETMODEM, port->omodem, RTS|DTR, 0, 1);

		if( bc->mstat & DTR ) {
			DPRINT3("dgb%d: port %d: DTR is on\n",unit,pnum);
		}

		hidewin(sc);
		break;
	case TIOCMSET:
		if(*(int *)data & TIOCM_DTR)
			port->omodem |=DTR;
		else
			port->omodem &=~DTR;

		if(*(int *)data & TIOCM_RTS)
			port->omodem |=RTS;
		else
			port->omodem &=~RTS;

		setwin(sc,0);
		fepcmd(port, SETMODEM, port->omodem, RTS|DTR, 0, 1);
		hidewin(sc);
		break;
	case TIOCMBIS:
		if(*(int *)data & TIOCM_DTR)
			port->omodem |=DTR;

		if(*(int *)data & TIOCM_RTS)
			port->omodem |=RTS;

		setwin(sc,0);
		fepcmd(port, SETMODEM, port->omodem, RTS|DTR, 0, 1);
		hidewin(sc);
		break;
	case TIOCMBIC:
		if(*(int *)data & TIOCM_DTR)
			port->omodem &=~DTR;

		if(*(int *)data & TIOCM_RTS)
			port->omodem &=~RTS;

		setwin(sc,0);
		fepcmd(port, SETMODEM, port->omodem, RTS|DTR, 0, 1);
		hidewin(sc);
		break;
	case TIOCMGET:
		setwin(sc,0);
		port->imodem=bc->mstat;
		hidewin(sc);

		tiocm_xxx = TIOCM_LE;	/* XXX - always enabled while open */

		DPRINT3("dgb%d: port %d: modem stat -- ",unit,pnum);

		if (port->imodem & DTR) {
			DPRINT1("DTR ");
			tiocm_xxx |= TIOCM_DTR;
		}
		if (port->imodem & RTS) {
			DPRINT1("RTS ");
			tiocm_xxx |= TIOCM_RTS;
		}
		if (port->imodem & CTS) {
			DPRINT1("CTS ");
			tiocm_xxx |= TIOCM_CTS;
		}
		if (port->imodem & port->dcd) {
			DPRINT1("DCD ");
			tiocm_xxx |= TIOCM_CD;
		}
		if (port->imodem & port->dsr) {
			DPRINT1("DSR ");
			tiocm_xxx |= TIOCM_DSR;
		}
		if (port->imodem & RI) {
			DPRINT1("RI ");
			tiocm_xxx |= TIOCM_RI;
		}
		*(int *)data = tiocm_xxx;
		DPRINT1("--\n");
		break;
	default:
		splx(s);
		return ENOTTY;
	}
	splx(s);
	return 0;
}

static void 
wakeflush(p)
	void *p;
{
	struct dgb_p *port=p;

	wakeup(&port->draining);
}

/* wait for the output to drain */

static int
dgbdrain(port)
	struct dgb_p	*port;
{
	struct dgb_softc *sc=&dgb_softc[port->unit];
	struct board_chan *bc=port->brdchan;
	int error;
	int head, tail;

	setwin(sc,0);

	bc->iempty=1;
	tail=bc->tout;
	head=bc->tin;

	while(tail!=head) {
		DPRINT5("dgb%d: port %d: drain: head=%d tail=%d\n",
			port->unit, port->pnum, head, tail);

		hidewin(sc);
		port->draining=1;
		timeout(wakeflush,port, hz);
		error=tsleep(&port->draining, TTIPRI | PCATCH, "dgdrn", 0);
		port->draining=0;
		setwin(sc,0);

		if (error != 0) {
			DPRINT4("dgb%d: port %d: tsleep(dgdrn) error=%d\n",
				port->unit,port->pnum,error);

			bc->iempty=0;
			hidewin(sc);
			return error;
		}

		tail=bc->tout;
		head=bc->tin;
	}
	DPRINT5("dgb%d: port %d: drain: head=%d tail=%d\n",
		port->unit, port->pnum, head, tail);

	return 0;
}

/* wait for the output to drain */
/* or simply clear the buffer it it's stopped */

static void
dgb_drain_or_flush(port)
	struct dgb_p	*port;
{
	struct tty *tp=port->tty;
	struct dgb_softc *sc=&dgb_softc[port->unit];
	struct board_chan *bc=port->brdchan;
	int error;
	int lasttail;
	int head, tail;

	setwin(sc,0);

	lasttail=-1;
	bc->iempty=1;
	tail=bc->tout;
	head=bc->tin;

	while(tail!=head /* && tail!=lasttail */ ) {
		DPRINT5("dgb%d: port %d: flush: head=%d tail=%d\n",
			port->unit, port->pnum, head, tail);

		/* if there is no carrier simply clean the buffer */
		if( !(tp->t_state & TS_CARR_ON) ) {
			bc->tout=bc->tin=0;
			bc->iempty=0;
			hidewin(sc);
			return;
		}

		hidewin(sc);
		port->draining=1;
		timeout(wakeflush,port, hz);
		error=tsleep(&port->draining, TTIPRI | PCATCH, "dgfls", 0);
		port->draining=0;
		setwin(sc,0);

		if (error != 0) {
			DPRINT4("dgb%d: port %d: tsleep(dgfls) error=%d\n",
				port->unit,port->pnum,error);

			/* silently clean the buffer */

			bc->tout=bc->tin=0;
			bc->iempty=0;
			hidewin(sc);
			return;
		}

		lasttail=tail;
		tail=bc->tout;
		head=bc->tin;
	}
	DPRINT5("dgb%d: port %d: flush: head=%d tail=%d\n",
			port->unit, port->pnum, head, tail);
}

static int
dgbparam(tp, t)
	struct tty	*tp;
	struct termios	*t;
{
	int dev=tp->t_dev;
	int unit=MINOR_TO_UNIT(dev);
	int pnum=MINOR_TO_PORT(dev);
	struct dgb_softc *sc=&dgb_softc[unit];
	struct dgb_p *port=&sc->ports[pnum];
	struct board_chan *bc=port->brdchan;
	int cflag;
	int head;
	int mval;
	int iflag;
	int hflow;
	int s;

	DPRINT3("dgb%d: port%d: setting parameters\n",unit,pnum);

	if(port->mustdrain) {
		DPRINT3("dgb%d: port%d: must call dgbdrain()\n",unit,pnum);
		dgbdrain(port);
	}

	cflag=ttspeedtab(t->c_ospeed, dgbspeedtab);

	if (t->c_ispeed == 0)
		t->c_ispeed = t->c_ospeed;

	if (cflag < 0 || cflag > 0 && t->c_ispeed != t->c_ospeed)
		return (EINVAL);

	s=spltty();

	setwin(sc,0);

	if(cflag==0) { /* hangup */
		DPRINT3("dgb%d: port%d: hangup\n",unit,pnum);
		head=bc->rin;
		bc->rout=head;
		head=bc->tin;
		fepcmd(port, STOUT, head, 0, 0, 0);
		mval= port->omodem & ~(DTR|RTS);
	} else {
		DPRINT4("dgb%d: port%d: CBAUD=%d\n",unit,pnum,cflag);

		/* convert flags to sysV-style values */
		if(t->c_cflag & PARODD)
			cflag|=01000;
		if(t->c_cflag & PARENB)
			cflag|=00400;
		if(t->c_cflag & CSTOPB)
			cflag|=00100;

		cflag|= (t->c_cflag & CSIZE) >> 4;
		DPRINT4("dgb%d: port%d: CFLAG=0x%x\n",unit,pnum,cflag);

		if(cflag!=port->fepcflag) {
			DPRINT3("dgb%d: port%d: set cflag\n",unit,pnum);
			port->fepcflag=cflag;
			fepcmd(port, SETCTRLFLAGS, (unsigned)cflag, 0, 0, 0);
		}
		mval= port->omodem | (DTR|RTS) ;
	}

	iflag=t->c_iflag & (IGNBRK|BRKINT|IGNPAR|PARMRK|INPCK|ISTRIP);
	if(t->c_cflag & IXON)
		cflag|=002000;
	if(t->c_cflag & IXANY)
		cflag|=004000;
	if(t->c_cflag & IXOFF)
		cflag|=010000;

	if(iflag!=port->fepiflag) {
		DPRINT3("dgb%d: port%d: set iflag\n",unit,pnum);
		port->fepiflag=iflag;
		fepcmd(port, SETIFLAGS, (unsigned)iflag, 0, 0, 0);
	}

	bc->mint=port->dcd;

	if(t->c_cflag & CRTSCTS)
		hflow=(CTS|RTS);
	else
		hflow=0;

	if(hflow!=port->hflow) {
		DPRINT3("dgb%d: port%d: set hflow\n",unit,pnum);
		port->hflow=hflow;
		fepcmd(port, SETHFLOW, (unsigned)hflow, 0xff, 0, 1);
	}
	
	if(port->omodem != mval) {
		DPRINT4("dgb%d: port %d: setting modem parameters 0x%x\n",
			unit,pnum,mval);
		port->omodem=mval;
		fepcmd(port, SETMODEM, (unsigned)mval, RTS|DTR, 0, 1);
	}

	if(port->fepstartc!=t->c_cc[VSTART] || port->fepstopc!=t->c_cc[VSTOP]) {
		DPRINT3("dgb%d: port%d: set startc, stopc\n",unit,pnum);
		port->fepstartc=t->c_cc[VSTART];
		port->fepstopc=t->c_cc[VSTOP];
		fepcmd(port, SONOFFC, port->fepstartc, port->fepstopc, 0, 1);
	}

	hidewin(sc);
	splx(s);

	return 0;

}

static void
dgbstart(tp)
	struct tty	*tp;
{
	int unit;
	int pnum;
	struct dgb_p *port;
	struct dgb_softc *sc;
	struct board_chan *bc;
	int head, tail;
	int size, ocount;
	int s;
	int wmask;

	unit=MINOR_TO_UNIT(minor(tp->t_dev));
	pnum=MINOR_TO_PORT(minor(tp->t_dev));
	sc=&dgb_softc[unit];
	port=&sc->ports[pnum];
	bc=port->brdchan;

	wmask=port->txbufsize-1;

	s=spltty();

	while( tp->t_outq.c_cc!=0 ) {
#ifndef TS_ASLEEP	/* post 2.0.5 FreeBSD */
		ttwwakeup(tp);
#else
		if(tp->t_outq.c_cc <= tp->t_lowat) {
			if(tp->t_state & TS_ASLEEP) {
				tp->t_state &= ~TS_ASLEEP;
				wakeup(TSA_OLOWAT(tp));
			}
			selwakeup(&tp->t_wsel);
		}
#endif
		setwin(sc,0);

		head=bc->tin & wmask;
		tail=bc->tout & wmask;

		DPRINT5("dgb%d: port%d: s tx head=%d tail=%d\n",unit,pnum,head,tail);

		if(tail>head)
			size=tail-head-1;
		else {
			size=port->txbufsize-head;
			if(tail==0)
				size--;
		}

		if(size==0) {
			bc->iempty=1; bc->ilow=1;
			hidewin(sc);
			tp->t_state|=TS_BUSY;
			splx(s);
			return;
		}

		towin(sc,port->txwin);

		ocount=q_to_b(&tp->t_outq, port->txptr+head, size);
		head+=ocount;
		if(head>=port->txbufsize)
			head-=port->txbufsize;

		setwin(sc,0);
		bc->tin=head;
	}

#ifndef TS_ASLEEP	/* post 2.0.5 FreeBSD */
	ttwwakeup(tp);
#else
	if(tp->t_state & TS_ASLEEP) {
		tp->t_state &= ~TS_ASLEEP;
		wakeup(TSA_OLOWAT(tp));
	}
	tp->t_state&=~TS_BUSY;
#endif
	hidewin(sc);
	splx(s);
}

void
dgbstop(tp, rw)
	struct tty	*tp;
	int		rw;
{
}

struct tty *
dgbdevtotty(dev)
	dev_t	dev;
{
	int mynor, pnum, unit;
	struct dgb_softc *sc;

	mynor = minor(dev);
	if (mynor & CONTROL_MASK)
		return (NULL);
	unit = MINOR_TO_UNIT(mynor);
	if ((u_int) unit >= NDGB)
		return (NULL);
	pnum = MINOR_TO_PORT(mynor);
	sc = &dgb_softc[unit];
	if (pnum >= sc->numports)
		return (NULL);
	return (&sc->ttys[pnum]);
}

static void 
fepcmd(port, cmd, op1, op2, ncmds, bytecmd)
	struct dgb_p *port;
	int cmd, op1, op2, ncmds, bytecmd;
{
	struct dgb_softc *sc=&dgb_softc[port->unit];
	u_char *mem=sc->vmem;
	unsigned tail, head;
	int count, n;

	if(port->status==DISABLED) {
		printf("dgb%d(%d): FEP command on disabled port\n", 
			port->unit, port->pnum);
		return;
	}

	setwin(sc,0);
	head=sc->mailbox->cin;

	if(head>=(FEP_CMAX-FEP_CSTART) || (head & 3)) {
		printf("dgb%d(%d): wrong pointer head of command queue : 0x%x\n",
			port->unit, port->pnum, head);
		return;
	}

	if(bytecmd) {
		mem[head+FEP_CSTART+0]=cmd;
		mem[head+FEP_CSTART+1]=port->pnum;
		mem[head+FEP_CSTART+2]=op1;
		mem[head+FEP_CSTART+3]=op2;
	} else {
		mem[head+FEP_CSTART+0]=cmd;
		mem[head+FEP_CSTART+1]=port->pnum;
		*(ushort *)(mem+head+FEP_CSTART+2)=op1;
	}

	head=(head+4) & (FEP_CMAX-FEP_CSTART-4);
	sc->mailbox->cin=head;

	for(count=FEPTIMEOUT; count>0; count--) {
		head=sc->mailbox->cin;
		tail=sc->mailbox->cout;
		n=(head-tail) & (FEP_CMAX-FEP_CSTART-4);

		if(n <= ncmds * 4)
			return;
	}

	printf("dgb%d(%d): timeout on FEP command\n",
			port->unit, port->pnum);
}


static dgb_devsw_installed = 0;

static void 
dgb_drvinit(void *unused)
{
	dev_t dev;

	if( ! dgb_devsw_installed ) {
		dev = makedev(CDEV_MAJOR, 0);
		cdevsw_add(&dev,&dgb_cdevsw, NULL);
		dgb_devsw_installed = 1;
    	}
}

SYSINIT(dgbdev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,dgb_drvinit,NULL)

#endif /* NDGB > 0 */
