/* asc.c - device driver for hand scanners
 *
 * Current version supports:
 *
 * 	- AmiScan (Mustek) Color and BW hand scanners (GI1904 chipset)
 *
 * Copyright (c) 1995 Gunther Schadow.  All rights reserved.
 * Copyright (c) 1995,1996,1997 Luigi Rizzo.  All rights reserved.
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
 *	This product includes software developed by Gunther Schadow
 *	and Luigi Rizzo.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 */
/*
 * $FreeBSD$
 */

#include "asc.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/poll.h>
#include <sys/selinfo.h>
#include <sys/uio.h>
#include <sys/bus.h>

#include <machine/asc_ioctl.h>

#include <i386/isa/isa.h>
#include <i386/isa/isa_device.h>
#include <i386/isa/ascreg.h>

#ifndef COMPAT_OLDISA
#error "The asc device requires the old isa compatibility shims"
#endif

/***
 *** CONSTANTS & DEFINES
 ***
 ***/

#define PROBE_FAIL    0
#define PROBE_SUCCESS IO_ASCSIZE
#define ATTACH_FAIL   0
#define ATTACH_SUCCESS 1
#define SUCCESS       0
#define FAIL         -1
#define INVALID       FAIL

#define DMA1_READY  0x08
#define ASCDEBUG
#ifdef ASCDEBUG
#	define lprintf if(scu->flags & FLAG_DEBUG) printf
#else
#	define lprintf (void)
#endif

#define TIMEOUT (hz*15)  /* timeout while reading a buffer - default value */
#define ASCPRI  PRIBIO   /* priority while reading a buffer */

/***
 *** LAYOUT OF THE MINOR NUMBER
 ***/

#define UNIT_MASK 0xc0    /* unit asc0 .. asc3 */
#define UNIT(x)   (x >> 6)
#define DBUG_MASK 0x20
#define FRMT_MASK 0x18    /* output format */
#define FRMT_RAW  0x00    /* output bits as read from scanner */
#define FRMT_GRAY 0x1     /* output gray mode for color scanner */
#define FRMT_PBM  0x08    /* output pbm format */
#define FRMT_PGM  0x18

/***
 *** THE GEMOMETRY TABLE
 ***/

#define GREY_LINE 826 /* 825, or 826 , or 550 ??? */
static const struct asc_geom {
  int dpi;     /* dots per inch */
  int dpl;     /* dots per line */
  int bpl;     /* bytes per line */
  int g_res;   /* get resolution value (ASC_STAT) */
} geomtab[] = {
  { 800, 3312, 414, ASC_RES_800},
  { 700, 2896, 362, ASC_RES_700},
  { 600, 2480, 310, ASC_RES_600},
  { 500, 1656, 258, ASC_RES_500},
  { 400, 1656, 207, ASC_RES_400},
  { 300, 1240, 155, ASC_RES_300},
  { 200, 832, 104, ASC_RES_200},
  { 100, 416, 52, ASC_RES_100},
  { 200, 3*GREY_LINE, 3*GREY_LINE, 0 /* returned by color scanner */},
  { 200, GREY_LINE, GREY_LINE, 0 /* color scanner, grey mode */},
  { INVALID, 416, 52, INVALID } /* terminator */
};

/***
 *** THE TABLE OF UNITS
 ***/

struct _sbuf {
  size_t  size;
  size_t  rptr;
  size_t  wptr; /* only changed in ascintr */
  size_t  count;
  char   *base;
};

struct asc_unit {
  long thedev;	/* XXX */
  int base;		/* base address */
  int dma_num;		/* dma number */
  char    dma_byte;       /* mask of byte for setting DMA value */
  char    int_byte;       /* mask of byte for setting int value */
  char    cfg_byte;       /* mirror of byte written to config reg (ASC_CFG). */
  char    cmd_byte;       /* mirror of byte written to cmd port (ASC_CMD)*/
  char   portf_byte;
  int flags;
#define ATTACHED 	0x01
#define OPEN     	0x02
#define READING  	0x04
#define DMA_ACTIVE      0x08
#define SLEEPING	0x10
#define SEL_COLL	0x20
#define PBM_MODE	0x40
#define FLAG_DEBUG    	0x80
  int     geometry;       /* resolution as geomtab index */
  int     linesize;       /* length of one scan line (from geom.table) */
  int     blen;           /* length of buffer in lines */
  int     btime;          /* timeout of buffer in seconds/hz */
  struct  _sbuf sbuf;
  long	  icnt;		/* interrupt count XXX for debugging */
  struct selinfo selp;
  int     height;         /* height, for pnm modes */
  size_t  bcount;         /* bytes to read, for pnm modes */
};

static struct asc_unit unittab[NASC];                                 

/*** I could not find a reasonable buffer size limit other than by
 *** experiments. MAXPHYS is obviously too much, while DEV_BSIZE and
 *** PAGE_SIZE are really too small. There must be something wrong
 *** with isa_dmastart/isa_dmarangecheck HELP!!!
 ***
 *** Note, must be DEFAULT_BLEN * samples_per_line <= MAX_BUFSIZE
 ***/
#define MAX_BUFSIZE 0xb000 /* XXX was 0x3000 */
#define DEFAULT_BLEN 16

/***
 *** THE PER-DRIVER RECORD FOR ISA.C
 ***/
static int ascprobe (struct isa_device *isdp);
static int ascattach(struct isa_device *isdp);

struct isa_driver ascdriver = {
	INTR_TYPE_TTY,
	ascprobe,
	ascattach,
	"asc"
};
COMPAT_ISA_DRIVER(asc, ascdriver);

static ointhand2_t	ascintr;

static d_open_t		ascopen;
static d_close_t	ascclose;
static d_read_t		ascread;
static d_ioctl_t	ascioctl;
static d_poll_t		ascpoll;

#define CDEV_MAJOR 71

static struct cdevsw asc_cdevsw = {
	/* open */	ascopen,
	/* close */	ascclose,
	/* read */	ascread,
	/* write */	nowrite,
	/* ioctl */	ascioctl,
	/* poll */	ascpoll,
	/* mmap */	nommap,
	/* strategy */	nostrategy,
	/* name */	"asc",
	/* maj */	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	0,
};

#define STATIC static

/***
 *** LOCALLY USED SUBROUTINES
 ***
 ***/

/***
 *** get_resolution
 ***	read resolution from the scanner
 ***/
static void
get_resolution(struct asc_unit *scu)
{
    int res, i, delay;

    res=0;
    scu->cmd_byte = ASC_STANDBY;
    outb(ASC_CMD, scu->cmd_byte);
    tsleep((caddr_t)scu, ASCPRI | PCATCH, "ascres", hz/10);
    for(delay= 100; (res=inb(ASC_STAT)) & ASC_RDY_FLAG; delay--)
    {
        i = tsleep((caddr_t)scu, ASCPRI | PCATCH, "ascres0", 1);
        if ( ( i == 0 ) || ( i == EWOULDBLOCK ) )
	    i = SUCCESS;
	else
	    break;
    }
    if (delay==0) {
	lprintf("asc.get_resolution: timeout completing command\n");
	return /*  -1 */;
    }
    /* ... actual read resolution... */
    res &= ASC_RES_MASK;
    for (i=0; geomtab[i].dpi != INVALID; i++) {
    	if (geomtab[i].g_res == res) break;
    }
    if (geomtab[i].dpi==INVALID) {
	scu->geometry= i; /* INVALID; */
	lprintf("asc.get_resolution: wrong resolution\n");
    } else {
	lprintf("asc.get_resolution: %d dpi\n",geomtab[i].dpi);
	scu->geometry = i;
    }
    scu->portf_byte=0; /* default */
    if (geomtab[scu->geometry].g_res==0 && !(scu->thedev&FRMT_GRAY)) {
	/* color scanner seems to require this */
	scu->portf_byte=2;
	/* scu->geometry++; */
    }
    scu->linesize = geomtab[scu->geometry].bpl;
    scu->height = geomtab[scu->geometry].dpl; /* default... */
}

/***
 *** buffer_allocate
 ***	allocate/reallocate a buffer
 ***	Now just checks that the preallocated buffer is large enough.
 ***/

static int
buffer_allocate(struct asc_unit *scu)
{
  size_t size, size1;

  size = scu->blen * scu->linesize;

  lprintf("asc.buffer_allocate: need 0x%x bytes\n", size);

  if ( size > MAX_BUFSIZE ) {
      size1=size;
      size= ( (MAX_BUFSIZE+scu->linesize-1) / scu->linesize)*scu->linesize;
      lprintf("asc.buffer_allocate: 0x%x bytes are too much, try 0x%x\n",
	  size1, size);
      return ENOMEM;
  }

  scu->sbuf.size = size;
  scu->sbuf.rptr  = 0;
  scu->sbuf.wptr  = 0;
  scu->sbuf.count  = 0; /* available data for reading */

  lprintf("asc.buffer_allocate: ok\n");

  return SUCCESS;
}

/*** dma_restart
 ***	invoked locally to start dma. Must run in a critical section
 ***/
static void
dma_restart(struct asc_unit *scu)
{
    unsigned char al=scu->cmd_byte;

    if (geomtab[scu->geometry].g_res==0) {/* color */
	isa_dmastart(ISADMA_READ, scu->sbuf.base+scu->sbuf.wptr,
	    scu->linesize + 90 /* XXX */ , scu->dma_num);
	/*
	 * looks like we have to set and then clear this
	 * bit to enable the scanner to send interrupts
	 */
	outb( ASC_CMD, al |= 4 ); /* seems to disable interrupts */
#if 0
	outb( ASC_CMD, al |= 8 ); /* ??? seems useless */
#endif
	outb( ASC_CMD, al &= 0xfb );
	scu->cmd_byte = al;
    } else {					/* normal */
    isa_dmastart(ISADMA_READ, scu->sbuf.base+scu->sbuf.wptr,
	scu->linesize, scu->dma_num);
    /*** this is done in sub_20, after dmastart ? ***/  
#if 0
    outb( ASC_CMD, al |= 4 );
    outb( ASC_CMD, al |= 8 ); /* ??? seems useless */
    outb( ASC_CMD, al &= 0xfb );
    scu->cmd_byte = al;
#else
    outb( ASC_CMD, ASC_OPERATE); 
#endif
    }
    scu->flags |= DMA_ACTIVE;
}

/***
 *** the main functions
 ***/

/*** asc_reset
 ***	resets the scanner and the config bytes...
 ***/
static void
asc_reset(struct asc_unit *scu)
{
  scu->cfg_byte = 0 ; /* clear... */
  scu->cmd_byte = 0 ; /* clear... */

  outb(ASC_CFG,scu->cfg_byte);	/* for safety, do this here */
  outb(ASC_CMD,scu->cmd_byte);	/* probably not needed */
  tsleep((caddr_t)scu, ASCPRI | PCATCH, "ascres", hz/10); /* sleep .1 sec */

  scu->blen = DEFAULT_BLEN;
  scu->btime = TIMEOUT;
  scu->height = 0 ; /* don't know better... */
}
/**************************************************************************
 ***
 *** ascprobe
 ***	read status port and check for proper configuration:
 ***	- if address group matches (status byte has reasonable value)
 ***	  cannot check interrupt/dma, only clear the config byte.
 ***/
static int
ascprobe (struct isa_device *isdp)
{
  int unit = isdp->id_unit;
  struct asc_unit *scu = unittab + unit;
  int stb;

  scu->base = isdp->id_iobase; /*** needed by the following macros ***/
  scu->flags = FLAG_DEBUG;

  if ( isdp->id_iobase < 0 ) {
      lprintf("asc%d.probe: no iobase given\n", unit);
      return PROBE_FAIL;
  }

  if ((stb=inb(ASC_PROBE)) != ASC_PROBE_VALUE) {
      lprintf("asc%d.probe: failed, got 0x%02x instead of 0x%02x\n",
	  unit, stb, ASC_PROBE_VALUE);
      return PROBE_FAIL;
  }

/*
 * NOTE NOTE NOTE
 * the new AmiScan Color board uses int 10,11,12 instead of 3,5,10
 * respectively. This means that the driver must act accordingly.
 * Unfortunately there is no easy way of telling which board one has,
 * other than trying to get an interrupt and noticing that it is
 * missing. use "option ASC_NEW_BOARD" if you have a new board.
 *
 */

#if ASC_NEW_BOARD
#define	ASC_IRQ_A	10
#define	ASC_IRQ_B	11
#define	ASC_IRQ_C	12
#else
#define	ASC_IRQ_A	3
#define	ASC_IRQ_B	5
#define	ASC_IRQ_C	10
#endif

  switch(ffs(isdp->id_irq) - 1) {
    case ASC_IRQ_A :
      scu->int_byte = ASC_CNF_IRQ3;
      break;
    case ASC_IRQ_B :
      scu->int_byte = ASC_CNF_IRQ5;
      break;
    case ASC_IRQ_C :
      scu->int_byte = ASC_CNF_IRQ10;
      break;
#if 0
    case -1:
      scu->int_byte = 0;
      lprintf("asc%d.probe: warning - going interruptless\n", unit);
      break;
#endif
    default:
      lprintf("asc%d.probe: unsupported INT %d (only 3, 5, 10)\n",
		unit, ffs(isdp->id_irq) - 1 );
      return PROBE_FAIL;
  }
  scu->dma_num = isdp->id_drq;
  switch(scu->dma_num) {
    case 1:
      scu->dma_byte = ASC_CNF_DMA1;
      break;
    case 3:
      scu->dma_byte = ASC_CNF_DMA3;
      break;
    default:
      lprintf("asc%d.probe: unsupported DMA %d (only 1 or 3)\n", 
		unit, scu->dma_num);
      return PROBE_FAIL;
  }
  asc_reset(scu);
/*  lprintf("asc%d.probe: ok\n", unit); */

  scu->flags &= ~FLAG_DEBUG;
  scu->icnt = 0;
  return PROBE_SUCCESS;
}

/**************************************************************************
 ***
 *** ascattach
 ***	finish initialization of unit structure, get geometry value (?)
 ***/

static int
ascattach(struct isa_device *isdp)
{
  int unit = isdp->id_unit;
  struct asc_unit *scu = unittab + unit;

  isdp->id_ointr = ascintr;
  scu->flags |= FLAG_DEBUG;
  printf("asc%d: [GI1904/Trust Ami-Scan Grey/Color]\n", unit);

  /*
   * Initialize buffer structure.
   * XXX this must be done early to give a good chance of getting a
   * contiguous buffer.  This wastes memory.
   */
  scu->sbuf.base = contigmalloc((unsigned long)MAX_BUFSIZE, M_DEVBUF, M_NOWAIT,
				0ul, 0xfffffful, 1ul, 0x10000ul);
  if ( scu->sbuf.base == NULL )
    {
      lprintf("asc%d.attach: buffer allocation failed\n", unit);
      return ATTACH_FAIL;	/* XXX attach must not fail */
    }
  scu->sbuf.size = INVALID;
  scu->sbuf.rptr  = INVALID;

  scu->flags |= ATTACHED;
/*  lprintf("asc%d.attach: ok\n", unit); */
  scu->flags &= ~FLAG_DEBUG;

    scu->selp.si_flags=0;
#define ASC_UID 0
#define ASC_GID 13
  make_dev(&asc_cdevsw, unit<<6, ASC_UID, ASC_GID, 0666, "asc%d", unit);
  make_dev(&asc_cdevsw, ((unit<<6) + FRMT_PBM),
    ASC_UID,  ASC_GID, 0666, "asc%dp", unit);
  make_dev(&asc_cdevsw, ((unit<<6) + DBUG_MASK),
    ASC_UID,  ASC_GID, 0666, "asc%dd", unit);
  make_dev(&asc_cdevsw, ((unit<<6) + DBUG_MASK+FRMT_PBM), 
    ASC_UID, ASC_GID, 0666, "asc%dpd", unit);
  return ATTACH_SUCCESS;
}

/**************************************************************************
 ***
 *** ascintr
 ***	the interrupt routine, at the end of DMA...
 ***/
static void
ascintr(int unit)
{
    struct asc_unit *scu = unittab + unit;
    int chan_bit = 0x01 << scu->dma_num;

    scu->icnt++;
    /* ignore stray interrupts... */
    if ((scu->flags & (OPEN |READING)) != (OPEN | READING) ) {
	/* must be after closing... */
	scu->flags &= ~(OPEN | READING | DMA_ACTIVE | SLEEPING | SEL_COLL);
	return;
    }
    if ( (scu->flags & DMA_ACTIVE) && (inb(DMA1_READY) & chan_bit) != 0) {
	outb( ASC_CMD, ASC_STANDBY);
	scu->flags &= ~DMA_ACTIVE;
		/* bounce buffers... */
        isa_dmadone(ISADMA_READ, scu->sbuf.base+scu->sbuf.wptr,
	    scu->linesize, scu->dma_num);
	scu->sbuf.wptr += scu->linesize;
	if (scu->sbuf.wptr >= scu->sbuf.size) scu->sbuf.wptr=0;
	scu->sbuf.count += scu->linesize;
	if (scu->flags & SLEEPING) {
	    scu->flags &= ~SLEEPING;
	    wakeup((caddr_t)scu);
	}
	if (scu->sbuf.size - scu->sbuf.count >= scu->linesize) {
	    dma_restart(scu);
	}
	if (SEL_WAITING(&scu->selp)) {
	    selwakeup(&scu->selp);
	}
    }
}

/**************************************************************************
 ***
 *** ascopen
 ***	set open flag, set modes according to minor number
 *** 	FOR RELEASE:
 ***	don't switch scanner on, wait until first read or ioctls go before
 ***/

STATIC int
ascopen(dev_t dev, int flags, int fmt, struct thread *td)
{
  struct asc_unit *scu;
  int unit;

  unit = UNIT(minor(dev)) & UNIT_MASK;
  if ( unit >= NASC )
    {
#ifdef ASCDEBUG
      /* XXX lprintf isn't valid here since there is no scu. */
      printf("asc%d.open: unconfigured unit number (max %d)\n", unit, NASC);
#endif
      return ENXIO;
    }
  scu = unittab + unit;
  if ( !( scu->flags & ATTACHED ) )
    {
      lprintf("asc%d.open: unit was not attached successfully 0x%04x\n",
	     unit, scu->flags);
      return ENXIO;
    }

  if ( minor(dev) & DBUG_MASK )
    scu->flags |= FLAG_DEBUG;
  else
    scu->flags &= ~FLAG_DEBUG;

  switch(minor(dev) & FRMT_MASK) {
  case FRMT_PBM:
    scu->flags |= PBM_MODE;
    lprintf("asc%d.open: pbm mode\n", unit);
    break;
  case FRMT_RAW:
    lprintf("asc%d.open: raw mode\n", unit);
    scu->flags &= ~PBM_MODE;
    break;
  default:
    lprintf("asc%d.open: gray maps are not yet supported", unit);
    return ENXIO;
  }
  
  lprintf("asc%d.open: minor %d icnt %ld\n", unit, minor(dev), scu->icnt);

  if ( scu->flags & OPEN ) {
      lprintf("asc%d.open: already open", unit);
      return EBUSY;
  }
  if (isa_dma_acquire(scu->dma_num))
      return(EBUSY);

  scu->flags = ATTACHED | OPEN;      

  asc_reset(scu);
  get_resolution(scu);
  return SUCCESS;
}

static int
asc_startread(struct asc_unit *scu)
{
    /*** from here on, things can be delayed to the first read/ioctl ***/
    /*** this was done in sub_12... ***/
  scu->cfg_byte= scu->cmd_byte=0;	/* init scanner */
  outb(ASC_CMD, scu->cmd_byte);
    /*** this was done in sub_16, set scan len... ***/
  outb(ASC_BOH, scu->portf_byte );
  if (geomtab[scu->geometry].g_res==0) {		/* color */
	scu->cmd_byte = 0x00 ;
  } else {
  scu->cmd_byte = 0x90 ;
  }
  outb(ASC_CMD, scu->cmd_byte);
  outb(ASC_LEN_L, scu->linesize & 0xff /* len_low */);
  outb(ASC_LEN_H, (scu->linesize >>8) & 0xff /* len_high */);
    /*** this was done in sub_21, config DMA ... ***/
  scu->cfg_byte |= scu->dma_byte;
  outb(ASC_CFG, scu->cfg_byte);
    /*** sub_22: enable int on the scanner ***/
  scu->cfg_byte |= scu->int_byte;
  outb(ASC_CFG, scu->cfg_byte);
    /*** sub_28: light on etc...***/
  scu->cmd_byte = ASC_STANDBY;
  outb(ASC_CMD, scu->cmd_byte);
  tsleep((caddr_t)scu, ASCPRI | PCATCH, "ascstrd", hz/10); /* sleep .1 sec */
  return SUCCESS;
}

/**************************************************************************
 ***
 *** ascclose
 ***	turn off scanner, release the buffer
 ***	should probably terminate dma ops, release int and dma. lr 12mar95
 ***/

STATIC int
ascclose(dev_t dev, int flags, int fmt, struct thread *td)
{
  int unit = UNIT(minor(dev));
  struct asc_unit *scu = unittab + unit;

  lprintf("asc%d.close: minor %d\n",
	 unit, minor(dev));

  if ( unit >= NASC || !( scu->flags & ATTACHED ) ) {
      lprintf("asc%d.close: unit was not attached successfully 0x%04x\n",
	     unit, scu->flags);
      return ENXIO;
  }
    /* all this is in sub_29... */
  /* cli(); */
  outb(ASC_CFG, 0 ); /* don't save in CFG byte!!! */
  scu->cmd_byte &= ~ASC_LIGHT_ON;
  outb(ASC_CMD, scu->cmd_byte);/* light off */
  tsleep((caddr_t)scu, ASCPRI | PCATCH, "ascclo", hz/2); /* sleep 1/2 sec */
  scu->cfg_byte &= ~ scu->dma_byte ; /* disable scanner dma */
  scu->cfg_byte &= ~ scu->int_byte ; /* disable scanner int */
  outb(ASC_CFG, scu->cfg_byte);
    /* --- disable dma controller ? --- */
  isa_dma_release(scu->dma_num);
    /* --- disable interrupts on the controller (sub_24) --- */

  scu->sbuf.size = INVALID;
  scu->sbuf.rptr  = INVALID;

  scu->flags &= ~(FLAG_DEBUG | OPEN | READING);
  
  return SUCCESS;
}

static void
pbm_init(struct asc_unit *scu)
{
    int width = geomtab[scu->geometry].dpl;
    int l= sprintf(scu->sbuf.base,"P4 %d %d\n", width, scu->height);
    char *p;

    scu->bcount = scu->height * width / 8 + l;

      /* move header to end of sbuf */
    scu->sbuf.rptr=scu->sbuf.size-l;
    bcopy(scu->sbuf.base, scu->sbuf.base+scu->sbuf.rptr,l);
    scu->sbuf.count = l;
    if (geomtab[scu->geometry].g_res!=0) { /* BW scanner */
    for(p = scu->sbuf.base + scu->sbuf.rptr; l; p++, l--)
	*p = ~*p;
}
}
/**************************************************************************
 ***
 *** ascread
 ***/

STATIC int
ascread(dev_t dev, struct uio *uio, int ioflag)
{
  int unit = UNIT(minor(dev));
  struct asc_unit *scu = unittab + unit;
  size_t nbytes;
  int sps, res;
  unsigned char *p;
  
  lprintf("asc%d.read: minor %d icnt %ld\n", unit, minor(dev), scu->icnt);

  if ( unit >= NASC || !( scu->flags & ATTACHED ) ) {
      lprintf("asc%d.read: unit was not attached successfully 0x%04x\n",
	     unit, scu->flags);
      return ENXIO;
  }

  if ( !(scu->flags & READING) ) { /*** first read... ***/
	/* allocate a buffer for reading data and init things */
      if ( (res = buffer_allocate(scu)) == SUCCESS ) scu->flags |= READING;
      else return res;
      asc_startread(scu);
      if ( scu->flags & PBM_MODE ) { /* initialize for pbm mode */
	  pbm_init(scu);
      }
  }
  
  lprintf("asc%d.read(before): "
      "sz 0x%x, rptr 0x%x, wptr 0x%x, cnt 0x%x bcnt 0x%x flags 0x%x icnt %ld\n",
	  unit, scu->sbuf.size, scu->sbuf.rptr,
	  scu->sbuf.wptr, scu->sbuf.count, scu->bcount,scu->flags,
	  scu->icnt);

  sps=spltty();
  if ( scu->sbuf.count == 0 ) { /* no data avail., must wait */
      if (!(scu->flags & DMA_ACTIVE)) dma_restart(scu);
      scu->flags |= SLEEPING;
      res = tsleep((caddr_t)scu, ASCPRI | PCATCH, "ascread", 0);
      scu->flags &= ~SLEEPING;
      if ( res == 0 ) res = SUCCESS;
  }
  splx(sps); /* lower priority... */
  if (scu->flags & FLAG_DEBUG)
      tsleep((caddr_t)scu, ASCPRI | PCATCH, "ascdly",hz);
  lprintf("asc%d.read(after): "
      "sz 0x%x, rptr 0x%x, wptr 0x%x, cnt 0x%x bcnt 0x%x flags 0x%x icnt %ld\n",
	  unit, scu->sbuf.size, scu->sbuf.rptr,
	  scu->sbuf.wptr, scu->sbuf.count, scu->bcount,scu->flags,scu->icnt);

	/* first, not more than available... */
  nbytes = min( uio->uio_resid, scu->sbuf.count );
	/* second, contiguous data... */
  nbytes = min( nbytes, (scu->sbuf.size - scu->sbuf.rptr) );
	/* third, one line (will remove this later, XXX) */
  nbytes = min( nbytes, scu->linesize );
  if ( (scu->flags & PBM_MODE) )
      nbytes = min( nbytes, scu->bcount );
  lprintf("asc%d.read: transferring 0x%x bytes\n", unit, nbytes);
  if (geomtab[scu->geometry].g_res!=0) { /* BW scanner */
  lprintf("asc%d.read: invert buffer\n",unit);
  for(p = scu->sbuf.base + scu->sbuf.rptr, res=nbytes; res; p++, res--)
	*p = ~*p;
  }
  res = uiomove(scu->sbuf.base + scu->sbuf.rptr, nbytes, uio);
  if ( res != SUCCESS ) {
      lprintf("asc%d.read: uiomove failed %d", unit, res);
      return res;
  }
  
  sps=spltty();
  scu->sbuf.rptr += nbytes;
  if (scu->sbuf.rptr >= scu->sbuf.size) scu->sbuf.rptr=0;
  scu->sbuf.count -= nbytes;
	/* having moved some data, can read mode */
  if (!(scu->flags & DMA_ACTIVE)) dma_restart(scu);
  splx(sps); /* lower priority... */
  if ( scu->flags & PBM_MODE ) scu->bcount -= nbytes;
  
  lprintf("asc%d.read: size 0x%x, pointer 0x%x, bcount 0x%x, ok\n",
	  unit, scu->sbuf.size, scu->sbuf.rptr, scu->bcount);
  
  return SUCCESS;
}

/**************************************************************************
 ***
 *** ascioctl
 ***/

STATIC int
ascioctl(dev_t dev, u_long cmd, caddr_t data, int flags, struct thread *td)
{
  int unit = UNIT(minor(dev));
  struct asc_unit *scu = unittab + unit;

  lprintf("asc%d.ioctl: minor %d\n",
	 unit, minor(dev));

  if ( unit >= NASC || !( scu->flags & ATTACHED ) ) {
      lprintf("asc%d.ioctl: unit was not attached successfully 0x%04x\n",
	     unit, scu->flags);
      return ENXIO;
  }
  switch(cmd) {
  case ASC_GRES:
    asc_reset(scu);
    get_resolution(scu);
    *(int *)data=geomtab[scu->geometry].dpi;
    lprintf("asc%d.ioctl:ASC_GRES %ddpi\n", unit, *(int *)data);
    return SUCCESS;    
  case ASC_GWIDTH:
    *(int *)data=geomtab[scu->geometry].dpl;
    lprintf("asc%d.ioctl:ASC_GWIDTH %d\n", unit, *(int *)data);
    return SUCCESS;    
  case ASC_GHEIGHT:
    *(int *)data=scu->height;
    lprintf("asc%d.ioctl:ASC_GHEIGHT %d\n", unit, *(int *)data);
    return SUCCESS;
  case ASC_SHEIGHT:
    lprintf("asc%d.ioctl:ASC_SHEIGHT %d\n", unit, *(int *)data);
    if ( scu->flags & READING ) { 
	lprintf("asc%d:ioctl on already reading unit\n", unit);
	return EBUSY;
    }
    scu->height=*(int *)data;
    return SUCCESS;
#if 0  
  case ASC_GBLEN:
    *(int *)data=scu->blen;
    lprintf("asc%d.ioctl:ASC_GBLEN %d\n", unit, *(int *)data);
    return SUCCESS;
  case ASC_SBLEN:
    lprintf("asc%d.ioctl:ASC_SBLEN %d\n", unit, *(int *)data);
    if (*(int *)data * geomtab[scu->geometry].dpl / 8 > MAX_BUFSIZE)
      {
	lprintf("asc%d:ioctl buffer size too high\n", unit);
	return ENOMEM;
      }
    scu->blen=*(int *)data;
    return SUCCESS;
  case ASC_GBTIME:
    *(int *)data = scu->btime / hz;
    lprintf("asc%d.ioctl:ASC_GBTIME %d\n", unit, *(int *)data);
    return SUCCESS;
  case ASC_SBTIME:
    scu->btime = *(int *)data * hz;
    lprintf("asc%d.ioctl:ASC_SBTIME %d\n", unit, *(int *)data);
    return SUCCESS;
#endif
  default: return ENOTTY;
  }
  return SUCCESS;
}

STATIC int
ascpoll(dev_t dev, int events, struct thread *td)
{
    int unit = UNIT(minor(dev));
    struct asc_unit *scu = unittab + unit;
    int sps;
    int revents = 0;

    sps=spltty();

    if (events & (POLLIN | POLLRDNORM)) {
	if (scu->sbuf.count >0)
	    revents |= events & (POLLIN | POLLRDNORM);
	else {
	    if (!(scu->flags & DMA_ACTIVE))
		dma_restart(scu);
	    
	    selrecord(td, &scu->selp);
	}
    }
    splx(sps);
    return (revents);
}
