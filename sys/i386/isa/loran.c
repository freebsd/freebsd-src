/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.org> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id: loran.c,v 1.16 1999/04/28 10:52:39 dt Exp $
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
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/uio.h>
#include <sys/malloc.h>

#include <i386/isa/isa_device.h>
#endif /* KERNEL */

typedef TAILQ_HEAD(, datapoint) dphead_t;

struct datapoint {
	/* Fields used by kernel */
	u_int64_t		scheduled;
	u_int			code;
	u_int			fri;
	u_int			agc;
	u_int			phase;
	u_int			width;
	u_int			par;
	u_int			isig;
	u_int			qsig;
	u_int			ssig;
	u_int64_t		epoch;
	TAILQ_ENTRY(datapoint)	list;
	u_char			status;
	int			vco;
	int			bounce;
	pid_t			pid;
	struct timespec		when;

	int			priority;
	dphead_t		*home;

	/* Fields used only in userland */
	void			*ident;
	int			index;
	double			ival;
	double			qval;
	double			sval;
	double			mval;

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

#define VCO_SHIFT 8		/* bits of fraction on VCO value */
#define VCO (2048 << VCO_SHIFT) /* initial vco dac (0 V)*/


#define PGUARD 990             /* program guard time (cycle) (990!) */


#ifdef KERNEL

#define NLORAN	10		/* Allow ten minor devices */

#define NDUMMY 4		/* How many idlers we want */

#define PORT 0x0300             /* controller port address */


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

static dphead_t minors[NLORAN], working, holding;

static struct datapoint dummy[NDUMMY];

static u_int64_t ticker;

static u_char par;

static MALLOC_DEFINE(M_LORAN, "Loran", "Loran datapoints");

static int loranerror;
static char lorantext[80];

static u_int vco_is;
static u_int vco_should;
static u_int vco_want;
static u_int64_t vco_when;
static int64_t vco_error;

/**********************************************************************/

static	int		loranprobe (struct isa_device *dvp);
static	void		init_tgc (void);
static	int		loranattach (struct isa_device *isdp);
static	void		loranenqueue (struct datapoint *);
static	d_open_t	loranopen;
static	d_close_t	loranclose;
static	d_read_t	loranread;
static	d_write_t	loranwrite;
static	ointhand2_t	loranintr;
extern	struct timecounter loran_timecounter;

/**********************************************************************/

int
loranprobe(struct isa_device *dvp)
{
	/* We need to be a "fast-intr" */
	dvp->id_ri_flags |= RI_FAST;

	dvp->id_iobase = PORT;
	return (8);
}

static u_short tg_init[] = {		/* stc initialization vector	*/
	0x0562,      12,         13,	/* counter 1 (p0)  Mode J	*/
	0x0262,  PGUARD,        GRI,	/* counter 2 (gri) Mode J	*/
	0x8562,     PCX, 5000 - PCX,	/* counter 3 (pcx)		*/
	0xc562,       0,     STROBE,	/* counter 4 (stb) Mode L	*/
	0x052a,	      0,          0	/* counter 5 (out)		*/
};

static void
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

	isdp->id_ointr = loranintr;

	/* We need to be a "fast-intr" */
	isdp->id_ri_flags |= RI_FAST;

	printf("loran0: LORAN-C Receiver\n");

	vco_should = VCO;
	vco_is = vco_should >> VCO_SHIFT;
	LOAD_DAC(DACA, vco_is);
	 
	init_tgc();

	init_timecounter(&loran_timecounter);

	TAILQ_INIT(&working);
	TAILQ_INIT(&holding);
	for (i = 0; i < NLORAN; i++) {
		TAILQ_INIT(&minors[i]);
		
	}

	for (i = 0; i < NDUMMY; i++) {
		dummy[i].agc = 4095;
		dummy[i].code = 0xac;
		dummy[i].fri = PGUARD;
		dummy[i].phase = 50;
		dummy[i].width = 50;
		dummy[i].priority = 9999;
		TAILQ_INSERT_TAIL(&working, &dummy[i], list);
	}

	inb(ADC);		/* Flush any old result */
	outb(ADC, ADC_S);

	par = ENG|IEN;
	outb(PAR, par);

	return (1);
}

static	int
loranopen (dev_t dev, int flags, int fmt, struct proc *p)
{
	int idx;

	idx = minor(dev);
	if (idx >= NLORAN) 
		return (ENODEV);

	return(0);
}

static	int
loranclose(dev_t dev, int flags, int fmt, struct proc *p)
{
	return(0);
}

static	int
loranread(dev_t dev, struct uio * uio, int ioflag)
{
	u_long ef;
	struct datapoint *this;
	int err, c;
	int idx;

	idx = minor(dev);
	
	if (loranerror) {
		printf("Loran0: %s", lorantext);
		return(EIO);
	}
	if (TAILQ_EMPTY(&minors[idx])) 
		tsleep ((caddr_t)&minors[idx], (PZERO + 8) |PCATCH, "loranrd", hz*2);
	if (TAILQ_EMPTY(&minors[idx])) 
		return(0);
	this = TAILQ_FIRST(&minors[idx]);
	ef = read_eflags();
	disable_intr();
	TAILQ_REMOVE(&minors[idx], this, list);
	write_eflags(ef);

	c = imin(uio->uio_resid, (int)sizeof *this);
	err = uiomove((caddr_t)this, c, uio);        
	FREE(this, M_LORAN);
        return(err);
}

static void
loranenqueue(struct datapoint *dp)
{
	struct datapoint *dpp, *dpn;

	while(dp) {
		/*
		 * The first two elements on "working" are sacred,
		 * they're already partly setup in hardware, so the
		 * earliest slot we can use is #3
		 */
		dpp = TAILQ_FIRST(&working);
		dpp = TAILQ_NEXT(dpp, list);
		dpn = TAILQ_NEXT(dpp, list);
		while (1) {
			/* 
			 * We cannot bump "dpp", so if "dp" overlaps it
			 * skip a beat.
			 * XXX: should use better algorithm ?
			 */
			if (dpp->scheduled + PGUARD > dp->scheduled) {
				dp->scheduled += dp->fri;
				continue;
			}		

			/*
			 * If "dpn" will be done before "dp" wants to go,
			 * we must look further down the list.
			 */
			if (dpn && dpn->scheduled + PGUARD < dp->scheduled) {
				dpp = dpn;
				dpn = TAILQ_NEXT(dpp, list);
				continue;
			}

			/* 
			 * If at end of list, put "dp" there
			 */
			if (!dpn) {
				TAILQ_INSERT_AFTER(&working, dpp, dp, list);
				break;
			}

			/*
			 * If "dp" fits before "dpn", insert it there
			 */
			if (dpn->scheduled > dp->scheduled + PGUARD) {
				TAILQ_INSERT_AFTER(&working, dpp, dp, list);
				break;
			}

			/*
			 * If "dpn" is less important, bump it.
			 */
			if (dp->priority < dpn->priority) {
				TAILQ_REMOVE(&working, dpn, list);
				TAILQ_INSERT_TAIL(&holding, dpn, list);
				dpn = TAILQ_NEXT(dpp, list);
				continue;
			}

			/*
			 * "dpn" was more or equally important, "dp" must
			 * take yet turn.
			 */
			dp->scheduled += dp->fri;
		}

		do {
			/*
			 * If anything was bumped, put it back as best we can
			 */
			if (TAILQ_EMPTY(&holding)) {
				dp = 0;
				break;
			}
			dp = TAILQ_FIRST(&holding);
			TAILQ_REMOVE(&holding, dp, list);
			if (dp->home) {
				if (!--dp->bounce) {
					TAILQ_INSERT_TAIL(dp->home, dp, list);
					wakeup((caddr_t)dp->home);
					dp = 0;
				}
			}
		} while (!dp);
	}
}

static	int
loranwrite(dev_t dev, struct uio * uio, int ioflag)
{
	u_long ef;
        int err = 0, c;
	struct datapoint *this;
	int idx;
	u_int64_t dt;

	idx = minor(dev);

	MALLOC(this, struct datapoint *, sizeof *this, M_LORAN, M_WAITOK);
	c = imin(uio->uio_resid, (int)sizeof *this);
	err = uiomove((caddr_t)this, c, uio);        
	if (!err && this->fri == 0)
		err = EINVAL;
	/* XXX more checks */
	this->home = &minors[idx];
	this->priority = idx;
	if (ticker > this->scheduled) {
		dt = ticker - this->scheduled;
		dt -= dt % this->fri;
		this->scheduled += dt;
	}
	if (!err) {
		ef = read_eflags();
		disable_intr();
		loranenqueue(this);
		write_eflags(ef);
		if (this->vco >= 0)
			vco_want = this->vco;
	} else {
		FREE(this, M_LORAN);
	}
	return(err);
}

static void
loranintr(int unit)
{
	u_long ef;
	int status = 0, count = 0, i;
	struct datapoint *this, *next;
	int delay;

	ef = read_eflags();
	disable_intr();

	this = TAILQ_FIRST(&working);
	TAILQ_REMOVE(&working, this, list);

	nanotime(&this->when);
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
	this->vco = vco_is;

	if (this->home) {
		TAILQ_INSERT_TAIL(this->home, this, list);
		wakeup((caddr_t)this->home);
	} else {
		loranenqueue(this);
	}

	this = TAILQ_FIRST(&working);
	next = TAILQ_NEXT(this, list);

	delay = next->scheduled - this->scheduled;
	delay -= GRI;

	/* load this->params */
	par &= ~(INTEG|GATE);
	par |= this->par & (INTEG|GATE);

	LOAD_DAC(DACB, this->agc);

	outb(CODE, this->code);

	LOAD_9513(0x0a, delay);

	/*
	 * We need to load this from the opposite register * due to some 
	 * weirdness which you can read about in in the 9513 manual on 
	 * page 1-26 under "LOAD"
	 */
	LOAD_9513(0x0c, this->phase);
	LOAD_9513(0x14, this->phase);
	outb(TGC, TG_LOADARM + 0x08);
	LOAD_9513(0x14, this->width);

	vco_error += ((vco_is << VCO_SHIFT) - vco_should) * (ticker - vco_when);
	vco_should = vco_want;
	i = vco_should >> VCO_SHIFT;
	if (vco_error < 0)
		i++;
	
	if (vco_is != i) {
		LOAD_DAC(DACA, i);
		vco_is = i;
	}
	vco_when = ticker;

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
		snprintf(lorantext, sizeof(lorantext),
		    "Missed: %02x %d %d this:%p next:%p (dummy=%p)\n", 
		    status, count, delay, this, next, &dummy);
		loranerror = 1;
	}

	ticker = this->scheduled;

	write_eflags(ef);
}

/**********************************************************************/

static unsigned
loran_get_timecount(struct timecounter *tc)
{
	unsigned count;
	u_long ef;

	ef = read_eflags();
	disable_intr();

	outb(TGC, TG_SAVE + 0x10);	/* save counter #5 */
	outb(TGC, TG_LOADDP +0x15);	/* hold counter #5 */
	count = inb(TGD);
	count |= inb(TGD) << 8;

	write_eflags(ef);
	return (count);
}

static struct timecounter loran_timecounter = {
	loran_get_timecount,	/* get_timecount */
	0,			/* no pps_poll */
	0xffff,			/* counter_mask */
	5000000,		/* frequency */
	"loran"			/* name */
};

SYSCTL_OPAQUE(_debug, OID_AUTO, loran_timecounter, CTLFLAG_RD, 
	&loran_timecounter, sizeof(loran_timecounter), "S,timecounter", "");


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


static int loran_devsw_installed;

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
