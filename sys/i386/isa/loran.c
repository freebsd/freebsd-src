/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.org> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id: loran.c,v 1.5 1998/05/29 08:04:44 phk Exp $
 *
 * This device-driver helps the userland controlprogram for a LORAN-C
 * receiver avoid monopolizing the CPU.
 *
 * This is clearly a candidate for the "most weird hardware support in
 * FreeBSD" prize.  At this time only two copies of the receiver are
 * known to exist in the entire world.
 *
 * Details can be found at:
 *     ftp://ftp.eecis.udel.edu/pub/ntp/loran.tar.Z
 *
 */

#ifdef KERNEL
#include "loran.h"
#include "opt_devfs.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/uio.h>
#include <sys/syslog.h>
#include <sys/time.h>
#include <sys/malloc.h>
#ifdef DEVFS
#include <sys/devfsext.h>
#endif /*DEVFS*/

#include <i386/isa/isa_device.h>
#endif /* KERNEL */

struct datapoint {
	void			*ident;
	int			index;
	u_int64_t		scheduled;
	u_int			delay;
	u_int			code;
	u_int			gri;
	u_int			agc;
	u_int			phase;
	u_int			width;
	u_int			par;
	u_int			isig;
	u_int			qsig;
	u_int			ssig;
	u_int64_t		epoch;
	struct timespec 	actual;
	TAILQ_ENTRY(datapoint)	list;
	double			ival;
	double			qval;
	double			sval;
	double			mval;
	u_char			status;
	u_int			vco;
	int			count;
	int			remain;
};

/*
 * Mode register (PAR) hardware definitions
 */
    #define INTEG 0x03          /* integrator mask */
        #define INTEG_1000us    0
        #define INTEG_264us     1
        #define INTEG_36us      2
        #define INTEG_SHORT     3
    #define GATE 0x0C           /* gate source mask */
        #define GATE_OPEN       0x0
        #define GATE_GRI        0x4
        #define GATE_PCI        0x8
        #define GATE_STB        0xc
    #define MSB 0x10            /* load dac high-order bits */
    #define IEN 0x20            /* enable interrupt bit */
    #define EN5 0x40            /* enable counter 5 bit */
    #define ENG 0x80            /* enable gri bit */

#define VCO 2048                /* initial vco dac (0 V)*/

#ifdef KERNEL


#define PORT 0x0300             /* controller port address */

#define PGUARD 990             /* program guard time (cycle) (990!) */

#define GRI 800                 /* pulse-group gate (cycle) */

/*
 * Analog/digital converter (ADC) hardware definitions
 */
#define ADC PORT+2              /* adc buffer (r)/address (w) */
#define ADCGO PORT+3            /* adc status (r)/adc start (w) */
    #define ADC_START 0x01      /* converter start bit (w) */
    #define ADC_BUSY 0x01       /* converter busy bit (r) */
    #define ADC_DONE 0x80       /* converter done bit (r) */
    #define ADC_I 0             /* i channel (phase) */
    #define ADC_Q 1             /* q channel (amplitude) */
    #define ADC_S 2             /* s channel (agc) */

/*
 * Digital/analog converter (DAC) hardware definitions
 * Note: output voltage increases with value programmed; the buffer
 * is loaded in two 8-bit bytes, the lsb 8 bits with the MSB bit off in
 * the PAR register, the msb 4 bits with the MSB on.
 */
#define DACA PORT+4             /* vco (dac a) buffer (w) */
#define DACB PORT+5             /* agc (dac b) buffer (w) */

#define LOAD_DAC(dac, val) if (0) { } else {				\
	par &= ~MSB; outb(PAR, par); outb((dac), (val) & 0xff);		\
	par |=  MSB; outb(PAR, par); outb((dac), ((val) >> 8) & 0xff);	\
	}

/*
 * Pulse-code generator (CODE) hardware definitions
 * Note: bits are shifted out from the lsb first
 */
#define CODE PORT+6             /* pulse-code buffer (w) */
    #define MPCA 0xCA           /* LORAN-C master pulse code group a */
    #define MPCB 0x9F           /* LORAN-C master pulse code group b */
    #define SPCA 0xF9           /* LORAN-C slave pulse code group a */
    #define SPCB 0xAC           /* LORAN-C slave pulse code group b */

/*
 * Mode register (PAR) hardware definitions
 */
#define PAR PORT+7              /* parameter buffer (w) */

#define TGC PORT+0              /* stc control port (r/w) */
#define TGD PORT+1              /* stc data port (r/w) */

/*
 * Timing generator (STC) hardware commands
 */
/* argument sssss = counter numbers 5-1 */
#define TG_LOADDP 0x00             /* load data pointer */
    /* argument ee = element (all groups except ggg = 000 or 111) */
    #define MODEREG 0x00        /* mode register */
    #define LOADREG 0x08        /* load register */
    #define HOLDREG 0x10        /* hold register */
    #define HOLDINC 0x18        /* hold register (hold cycle increm) */
    /* argument ee = element (group ggg = 111) */
    #define ALARM1 0x07         /* alarm register 1 */
    #define ALARM2 0x0F         /* alarm register 2 */
    #define MASTER 0x17         /* master mode register */
    #define STATUS 0x1F         /* status register */
#define ARM 0x20                /* arm counters */
#define LOAD 0x40               /* load counters */
#define TG_LOADARM 0x60            /* load and arm counters */
#define DISSAVE 0x80            /* disarm and save counters */
#define TG_SAVE 0xA0               /* save counters */
#define DISARM 0xC0             /* disarm counters */
/* argument nnn = counter number */
#define SETTOG 0xE8             /* set toggle output HIGH for counter */
#define CLRTOG 0xE0             /* set toggle output LOW for counter */
#define STEP 0xF0               /* step counter */
/* argument eeggg, where ee = element, ggg - counter group */
/* no arguments */
#define ENABDPS 0xE0            /* enable data pointer sequencing */
#define ENABFOUT 0xE6           /* enable fout */
#define ENAB8 0xE7              /* enable 8-bit data bus */
#define DSABDPS 0xE8            /* disable data pointer sequencing */
#define ENAB16 0xEF             /* enable 16-bit data bus */
#define DSABFOUT 0xEE           /* disable fout */
#define ENABPFW 0xF8            /* enable prefetch for write */
#define DSABPFW 0xF9            /* disable prefetch for write */
#define TG_RESET 0xFF              /* master reset */

#define LOAD_9513(index, val) if (0) {} else {		\
	outb(TGC, TG_LOADDP + (index));			\
	outb(TGD, (val) & 0xff);					\
	outb(TGD, ((val) >> 8) & 0xff);				\
	}

#define NENV 40                 /* size of envelope filter */
#define CLOCK 50                /* clock period (clock) */
#define CYCLE 10                /* carrier period (us) */
#define PCX (NENV * CLOCK)      /* envelope gate (clock) */
#define STROBE 50               /* strobe gate (clock) */

/**********************************************************************/

static TAILQ_HEAD(qhead, datapoint)	qdone, qready;

static struct datapoint dummy;

static u_int64_t ticker;

static u_char par;

static struct datapoint *this, *next;

static MALLOC_DEFINE(M_LORAN, "Loran", "Loran datapoints");

static int loranerror;
static char lorantext[80];

static u_int	vco_is;
static u_int	vco_should;

static int	lorantc_magic;

/**********************************************************************/

static	int		loranprobe (struct isa_device *dvp);
static	int		loranattach (struct isa_device *isdp);
static	void		loranenqueue (struct datapoint *);
static	d_open_t	loranopen;
static	d_close_t	loranclose;
static	d_read_t	loranread;
static	d_write_t	loranwrite;
extern	struct timecounter loran_timecounter[];

/**********************************************************************/

int
loranprobe(struct isa_device *dvp)
{
	/* We need to be a "fast-intr" */
	dvp->id_ri_flags |= RI_FAST;

	dvp->id_iobase = PORT;
	return (8);
}

u_short tg_init[] = {			/* stc initialization vector	*/
	0x0562,      12,         13,	/* counter 1 (p0)		*/
	0x0262,  PGUARD,        GRI,	/* counter 2 (gri)		*/
	0x8562,     PCX, 5000 - PCX,	/* counter 3 (pcx)		*/
	0xc562,       0,     STROBE,	/* counter 4 (stb)		*/
	0x052a,	      0,          0	/* counter 5 (out)		*/
};

void
init_tgc(void)
{
	int i;

	/* Initialize the 9513A */
	outb(TGC, TG_RESET);         outb(TGC, LOAD+0x1f); /* reset STC chip */
	LOAD_9513(MASTER, 0x8af0);
	outb(TGC, TG_LOADDP+1);
	tg_init[4] = 7499 - GRI;
	for (i = 0; i < 5*3; i++) {
		outb(TGD, tg_init[i]);
		outb(TGD, tg_init[i] >> 8);
	}
	outb(TGC, TG_LOADARM+0x1f);    /* let the good times roll */
}

int
loranattach(struct isa_device *isdp)
{
	int i;

	/* We need to be a "fast-intr" */
	isdp->id_ri_flags |= RI_FAST;

	printf("loran0: LORAN-C Receiver\n");

	vco_is = VCO;
	LOAD_DAC(DACA, VCO);
	 
	init_tgc();

	init_timecounter(loran_timecounter);

	TAILQ_INIT(&qdone);
	TAILQ_INIT(&qready);

	dummy.agc = 4095;
	dummy.code = 0xac;
	dummy.delay = PGUARD - GRI;
	dummy.gri = PGUARD;
	dummy.phase = 50;
	dummy.width = 50;

	TAILQ_INSERT_HEAD(&qready, &dummy, list);
	this = &dummy;
	next = &dummy;

	inb(ADC);		/* Flush any old result */
	outb(ADC, ADC_S);

	par = ENG|IEN;
	outb(PAR, par);

	return (1);
}

static	int
loranopen (dev_t dev, int flags, int fmt, struct proc *p)
{
	u_long ef;
	struct datapoint *this;

	while (!TAILQ_EMPTY(&qdone)) {
		ef = read_eflags();
		disable_intr();
		this = TAILQ_FIRST(&qdone);
		TAILQ_REMOVE(&qdone, this, list);
		write_eflags(ef);
		FREE(this, M_LORAN);
	}
	init_tgc();
	loranerror = 0;
	return(0);
}

static	int
loranclose(dev_t dev, int flags, int fmt, struct proc *p)
{
	/*
	 * Lower ENG
	 */	
	return(0);
}

static	int
loranread(dev_t dev, struct uio * uio, int ioflag)
{
	u_long ef;
	struct datapoint *this;
	int err, c;
	
	if (loranerror) {
		printf("Loran0: %s", lorantext);
		return(EIO);
	}
	if (TAILQ_EMPTY(&qdone)) 
		tsleep ((caddr_t)&qdone, PZERO + 8 |PCATCH, "loranrd", hz*2);
	if (TAILQ_EMPTY(&qdone)) 
		return(0);
	this = TAILQ_FIRST(&qdone);
	ef = read_eflags();
	disable_intr();
	TAILQ_REMOVE(&qdone, this, list);
	write_eflags(ef);

	c = imin(uio->uio_resid, (int)sizeof *this);
	err = uiomove((caddr_t)this, c, uio);        
	FREE(this, M_LORAN);
        return(err);
}

static void
loranenqueue(struct datapoint *this)
{
	struct datapoint *p, *q;
	u_long ef;
	u_int64_t x;

	if (this->scheduled < ticker) {
		x = (ticker - this->scheduled)  / (2 * this->gri);
		this->scheduled += x * 2 * this->gri;
	}

	ef = read_eflags();
	disable_intr();

	p = TAILQ_FIRST(&qready);
	while (1) {
		while (this->scheduled < p->scheduled + PGUARD) 
			this->scheduled += 2 * this->gri;
		q = TAILQ_NEXT(p, list);
		if (!q) {
			this->delay = this->scheduled - p->scheduled - GRI;
			TAILQ_INSERT_TAIL(&qready, this, list);
			break;
		}
		if (this->scheduled + PGUARD < q->scheduled) {
			this->delay = this->scheduled - p->scheduled - GRI;
			TAILQ_INSERT_BEFORE(q, this, list);
			q->delay = q->scheduled - this->scheduled - GRI;
			break;
		}
		p = q;
	}
	write_eflags(ef);
}

static	int
loranwrite(dev_t dev, struct uio * uio, int ioflag)
{
        int err = 0, c;
	struct datapoint *this;

	MALLOC(this, struct datapoint *, sizeof *this, M_LORAN, M_WAITOK);
	c = imin(uio->uio_resid, (int)sizeof *this);
	err = uiomove((caddr_t)this, c, uio);        
	if (!err && this->gri == 0)
		err = EINVAL;
	if (!err) {
		loranenqueue(this);
		vco_should = this->vco;
	} else {
		FREE(this, M_LORAN);
	}
	return(err);
}

void
loranintr(int unit)
{
	u_long ef;
	int status = 0, count = 0, i;

	ef = read_eflags();
	disable_intr();

	if (this != &dummy) {
		outb(TGC, DSABDPS);
		outb(TGC, TG_LOADDP + 0x12);	/* hold counter #2 */
		this->remain = -1;
		i = 2;
		for (i = 0; i < 2; i++) {
			count = this->remain;
			do {
				outb(TGC, TG_SAVE + 0x12);
				this->remain = inb(TGD) & 0xff;
				this->remain |= inb(TGD) << 8;
			} while (count == this->remain);
		}
		lorantc_magic = 1;
		nanotime(&this->actual);
		lorantc_magic = 0;
		outb(TGC, TG_LOADDP + 0x0a);	
		this->count = inb(TGD);
		this->count |= inb(TGD) << 8;
		LOAD_9513(0x12, GRI)
	}

	this->ssig = inb(ADC);

	par &= ~(ENG | IEN);
	outb(PAR, par);

	outb(ADC, ADC_I);
	outb(ADCGO, ADC_START);
	while (!(inb(ADCGO) & ADC_DONE))
		continue;
	this->isig = inb(ADC);

	outb(ADC, ADC_Q);
	outb(ADCGO, ADC_START);
	while (!(inb(ADCGO) & ADC_DONE))
		continue;
	this->qsig = inb(ADC);

	outb(ADC, ADC_S);

	this->epoch = ticker;

	if (this != &dummy) {
		TAILQ_INSERT_TAIL(&qdone, this, list);
		wakeup((caddr_t)&qdone);
	}

	if (next != &dummy || TAILQ_NEXT(next, list))
		TAILQ_REMOVE(&qready, next, list);

	this = next;
	ticker += GRI;
	ticker += this->delay;

	next = TAILQ_FIRST(&qready);
	if (!next) {
		next = &dummy;
		TAILQ_INSERT_HEAD(&qready, next, list);
	} else if (next->delay + GRI > PGUARD * 2) {
		next->delay -= PGUARD;
		next = &dummy;
		TAILQ_INSERT_HEAD(&qready, next, list);
	}
	if (next == &dummy)
		next->scheduled = ticker + GRI + next->delay;

	/* load this->params */
	par &= ~(INTEG|GATE);
	par |= this->par & (INTEG|GATE);

	LOAD_DAC(DACB, this->agc);

	outb(CODE, this->code);

	LOAD_9513(0x0a, next->delay);

	/*
	 * We need to load this from the opposite register * due to some 
	 * weirdness which you can read about in in the 9513 manual on 
	 * page 1-26 under "LOAD"
	 */
	LOAD_9513(0x0c, this->phase);
	LOAD_9513(0x14, this->phase);
	outb(TGC, TG_LOADARM + 0x08);
	LOAD_9513(0x14, this->width);

	if (vco_is != vco_should) {
		LOAD_DAC(DACA, vco_should);
		vco_is = vco_should;
	}

	this->status = inb(TGC);
#if 1
	/* Check if we overran */
	status = this->status & 0x1c;

	if (status) {
		outb(TGC, TG_SAVE + 2);		/* save counter #2 */
		outb(TGC, TG_LOADDP + 0x12);	/* hold counter #2 */
		count = inb(TGD);
		count |= inb(TGD) << 8;
		LOAD_9513(0x12, GRI)
	}
#endif

	par |= ENG | IEN;
	outb(PAR, par);

	if (status) {
		sprintf(lorantext, "Missed: %02x %d %d this:%p next:%p (dummy=%p)\n", 
		    status, count, next->delay, this, next, &dummy);
		loranerror = 1;
	}
	if (next->delay < PGUARD - GRI) {
		sprintf(lorantext, "Bogus: %02x %d %d\n",
		    status, count, next->delay);
		loranerror = 1;
	}

	write_eflags(ef);
}

/**********************************************************************/

static unsigned
loran_get_timecount(void)
{
	unsigned count;
	u_long ef;
	u_int high, low;

	ef = read_eflags();
	disable_intr();

	if (!lorantc_magic)
		outb(TGC, TG_SAVE + 0x10);	/* save counter #5 */
	outb(TGC, TG_LOADDP +0x15);	/* hold counter #5 */
	count = inb(TGD);
	count |= inb(TGD) << 8;

	write_eflags(ef);
	return (count);
}

static struct timecounter loran_timecounter[3] = {
	loran_get_timecount,	/* get_timecount */
	0,			/* no pps_poll */
	0xffff,			/* counter_mask */
	5000000,		/* frequency */
	"loran"			/* name */
};

SYSCTL_OPAQUE(_debug, OID_AUTO, loran_timecounter, CTLFLAG_RD, 
	loran_timecounter, sizeof(loran_timecounter), "S,timecounter", "");


/**********************************************************************/

struct	isa_driver lorandriver = {
	loranprobe, loranattach, "loran"
};

#define CDEV_MAJOR 94
static struct cdevsw loran_cdevsw = 
	{ loranopen,	loranclose,	loranread,	loranwrite,
	  noioctl,	nullstop,	nullreset,	nodevtotty,
	  seltrue,	nommap,		nostrat,	"loran",
	  NULL,		-1 };


static loran_devsw_installed = 0;

static void 	loran_drvinit(void *unused)
{
	dev_t dev;

	if(!loran_devsw_installed) {
		dev = makedev(CDEV_MAJOR, 0);
		cdevsw_add(&dev,&loran_cdevsw, NULL);
		loran_devsw_installed = 1;
    	}
}

SYSINIT(lorandev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,loran_drvinit,NULL)

#endif /* KERNEL */
