/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.org> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $FreeBSD$
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

#ifdef _KERNEL
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/uio.h>
#include <sys/bus.h>
#include <sys/malloc.h>
#include <sys/timetc.h>

#include <i386/isa/isa_device.h>
#endif /* _KERNEL */

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
	int			vco;
	int			bounce;
	pid_t			pid;
	struct timespec		when;

	int			priority;
	dphead_t		*home;

	/* Fields used only in userland */
	void			(*proc)(struct datapoint *);
	void			*ident;
	int			index;
	char			*name;


	/* Fields used only in userland */
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

#ifdef _KERNEL

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

extern struct cdevsw loran_cdevsw;

static dphead_t minors[NLORAN + 1], working;

static struct datapoint dummy[NDUMMY], *first, *second;

static u_int64_t ticker;

static u_char par;

static MALLOC_DEFINE(M_LORAN, "Loran", "Loran datapoints");

static int loranerror;
static char lorantext[160];

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

static int
loranprobe(struct isa_device *dvp)
{
	static int once;

	if (!once++)
		cdevsw_add(&loran_cdevsw);
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

static int
loranattach(struct isa_device *isdp)
{
	int i;

	isdp->id_ointr = loranintr;

	/* We need to be a "fast-intr" */
	/* isdp->id_ri_flags |= RI_FAST; XXX unimplemented - use newbus! */

	printf("loran0: LORAN-C Receiver\n");

	vco_want = vco_should = VCO;
	vco_is = vco_should >> VCO_SHIFT;
	LOAD_DAC(DACA, vco_is);
	 
	init_tgc();

	tc_init(&loran_timecounter);

	TAILQ_INIT(&working);
	for (i = 0; i < NLORAN + 1; i++) {
		TAILQ_INIT(&minors[i]);
		
	}

	for (i = 0; i < NDUMMY; i++) {
		dummy[i].agc = 4095;
		dummy[i].code = 0xac;
		dummy[i].fri = PGUARD;
		dummy[i].scheduled = PGUARD * 2 * i;
		dummy[i].phase = 50;
		dummy[i].width = 50;
		dummy[i].priority = NLORAN * 256;
		dummy[i].home = &minors[NLORAN];
		if (i == 0) 
			first = &dummy[i];
		else if (i == 1) 
			second = &dummy[i];
		else
			TAILQ_INSERT_TAIL(&working, &dummy[i], list);
	}

	inb(ADC);		/* Flush any old result */
	outb(ADC, ADC_S);

	par = ENG|IEN;
	outb(PAR, par);

	return (1);
}

static	int
loranopen (dev_t dev, int flags, int fmt, struct thread *td)
{
	int idx;

	idx = minor(dev);
	if (idx >= NLORAN) 
		return (ENODEV);

	return(0);
}

static	int
loranclose(dev_t dev, int flags, int fmt, struct thread *td)
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
		loranerror = 0;
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
	struct datapoint *dpp;

	TAILQ_FOREACH(dpp, &working, list) {
		if (dpp->priority <= dp->priority)
			continue;
		TAILQ_INSERT_BEFORE(dpp, dp, list);
		return;
	}
	TAILQ_INSERT_TAIL(&working, dp, list);
}

static	int
loranwrite(dev_t dev, struct uio * uio, int ioflag)
{
	u_long ef;
        int err = 0, c;
	struct datapoint *this;
	int idx;
	u_int64_t dt;
	u_int64_t when;

	idx = minor(dev);

	MALLOC(this, struct datapoint *, sizeof *this, M_LORAN, M_WAITOK);
	c = imin(uio->uio_resid, (int)sizeof *this);
	err = uiomove((caddr_t)this, c, uio);        
	if (err) {
		FREE(this, M_LORAN);
		return (err);
	}
	if (this->fri == 0) {
		FREE(this, M_LORAN);
		return (EINVAL);
	}
	this->par &= INTEG|GATE;
	/* XXX more checks needed! */
	this->home = &minors[idx];
	this->priority &= 0xff;
	this->priority += idx * 256;
	this->bounce = 0;
	when = second->scheduled + PGUARD;
	if (when > this->scheduled) {
		dt = when - this->scheduled;
		dt -= dt % this->fri;
		this->scheduled += dt;
	}
	ef = read_eflags();
	disable_intr();
	loranenqueue(this);
	write_eflags(ef);
	if (this->vco >= 0)
		vco_want = this->vco;
	return(err);
}

static void
loranintr(int unit)
{
	u_long ef;
	int status = 0, i;
#if 0
	int count = 0;
#endif
	int delay;
	u_int64_t when;
	struct timespec there, then;
	struct datapoint *dp, *done;

	ef = read_eflags();
	disable_intr();

	/*
	 * Pick up the measurement which just completed, and setup
	 * the next measurement.  We have 1100 microseconds for this
	 * of which some eaten by the A/D of the S channel and the 
	 * interrupt to get us here.
	 */

	done = first;

	nanotime(&there);
	done->ssig = inb(ADC);

	par &= ~(ENG | IEN);
	outb(PAR, par);

	outb(ADC, ADC_I);
	outb(ADCGO, ADC_START);

	/* Interlude: while we wait: setup the next measurement */
		LOAD_DAC(DACB, second->agc);
		outb(CODE, second->code);
		par &= ~(INTEG|GATE);
		par |= second->par;
		par |= ENG | IEN;

	while (!(inb(ADCGO) & ADC_DONE))
		continue;
	done->isig = inb(ADC);

	outb(ADC, ADC_Q);
	outb(ADCGO, ADC_START);
	/* Interlude: while we wait: setup the next measurement */
		/*
		 * We need to load this from the opposite register due to some 
		 * weirdness which you can read about in in the 9513 manual on 
		 * page 1-26 under "LOAD"
		 */
		LOAD_9513(0x0c, second->phase);
		LOAD_9513(0x14, second->phase);
		outb(TGC, TG_LOADARM + 0x08);
		LOAD_9513(0x14, second->width);
	while (!(inb(ADCGO) & ADC_DONE))
		continue;
	done->qsig = inb(ADC);

	outb(ADC, ADC_S);

	outb(PAR, par);

	/*
	 * End of VERY time critical stuff, we have 8 msec to find
	 * the next measurement and program the delay.
	 */
	status = inb(TGC);
	nanotime(&then);

	first = second;
	second = 0;
	when = first->scheduled + PGUARD;
	TAILQ_FOREACH(dp, &working, list) {
		while (dp->scheduled < when)
			dp->scheduled += dp->fri;
		if (second && dp->scheduled + PGUARD >= second->scheduled)
			continue;
		second = dp;
	}

	delay = (second->scheduled - first->scheduled) - GRI;

	LOAD_9513(0x0a, delay);

	/* Done, the rest is leisure work */

	vco_error += ((vco_is << VCO_SHIFT) - vco_should) * 
	    (ticker - vco_when);
	vco_should = vco_want;
	i = vco_should >> VCO_SHIFT;
	if (vco_error < 0)
		i++;
	
	if (vco_is != i) {
		LOAD_DAC(DACA, i);
		vco_is = i;
	}
	vco_when = ticker;

	/* Check if we overran */
	status &= 0x0c;
#if 0

	if (status) {
		outb(TGC, TG_SAVE + 2);		/* save counter #2 */
		outb(TGC, TG_LOADDP + 0x12);	/* hold counter #2 */
		count = inb(TGD);
		count |= inb(TGD) << 8;
		LOAD_9513(0x12, GRI)
	}
#endif

	if (status) {
		printf( "Missed: %02x %d first:%p second:%p %.09ld\n",
		    status, delay, first, second,
		    then.tv_nsec - there.tv_nsec);
		first->bounce++;
	}

	TAILQ_REMOVE(&working, second, list);

	if (done->bounce) {
		done->bounce = 0;
		loranenqueue(done);
	} else {
		done->epoch = ticker;
		done->vco = vco_is;
		done->when = there;
		TAILQ_INSERT_TAIL(done->home, done, list);
		wakeup((caddr_t)done->home);
	}

	ticker = first->scheduled;

	while ((dp = TAILQ_FIRST(&minors[NLORAN])) != NULL) {
		TAILQ_REMOVE(&minors[NLORAN], dp, list);
		TAILQ_INSERT_TAIL(&working, dp, list);
	}

	when = second->scheduled + PGUARD;

	TAILQ_FOREACH(dp, &working, list) {
		while (dp->scheduled < when)
			dp->scheduled += dp->fri;
	}
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


/**********************************************************************/

struct	isa_driver lorandriver = {
	INTR_TYPE_TTY | INTR_FAST,
	loranprobe,
	loranattach,
	"loran"
};
COMPAT_ISA_DRIVER(loran, lorandriver);

#define CDEV_MAJOR 94
static struct cdevsw loran_cdevsw = {
	/* open */	loranopen,
	/* close */	loranclose,
	/* read */	loranread,
	/* write */	loranwrite,
	/* ioctl */	noioctl,
	/* poll */	nopoll,
	/* mmap */	nommap,
	/* strategy */	nostrategy,
	/* name */	"loran",
	/* maj */	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	0,
};

#endif /* _KERNEL */
