/*
 * Copyright (c) 1997 Jamil J. Weatherbee
 * All rights reserved.
 *
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
 *    derived from this software without specific prior written permission
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
 * Industrial Computer Source model AIO8-P
 * 8 channel, moderate speed analog to digital converter board with
 * 128 channel MUX capability via daisy chained AT-16P units
 * alog.c, character device driver, last revised December 9 1997
 * See http://www.indcompsrc.com/products/data/html/aio8g-p.html
 *     http://www.indcompsrc.com/products/data/html/at16-p.html
 *
 * Written by: Jamil J. Weatherbee <jamil@freebsd.org>
 *
 */


/* Include Files */

#include "alog.h"
#if NALOG > 0

#include <sys/param.h> 
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/buf.h> 
#include <sys/poll.h>
#include <sys/vnode.h>
#include <sys/filio.h>
#include <machine/clock.h>
#include <i386/isa/isa.h>
#include <i386/isa/isa_device.h>
#include <sys/alogio.h>
#include <sys/dataacq.h>

#ifdef DEVFS
#include <sys/devfsext.h>
#endif 

/* Local Defines */

/* Tests have shown that increasing the fifo size 
 * beyond 64 entries for this particular piece of hardware is
 * unproductive */

#ifdef ALOG_FIFOSIZE
#define FIFOSIZE ALOG_FIFOSIZE
#else
#define FIFOSIZE 64
#endif

#ifdef ALOG_FIFO_TRIGGER
#define DEFAULT_FIFO_TRIGGER ALOG_FIFO_TRIGGER
#else
#define DEFAULT_FIFO_TRIGGER 1
#endif

#ifdef ALOG_CHANNELS 
#define NUMCHANNELS ALOG_CHANNELS
#else
#define NUMCHANNELS 128
#endif

#ifdef ALOG_TIMO
#define READTIMO ALOG_TIMO
#else
#define READTIMO (MAX_MICRO_PERIOD*NUMCHANNELS/500000*hz)
#endif

#define CDEV_MAJOR 86
#define NUMPORTS 8
#define MAXUNITS 2
#define NUMIMUXES 8

#define ADLOW 0x0
#define ADHIGH 0x1
#define STATUS 0x2
#define CNTR0 0x4
#define CNTR1 0x5
#define CNTR2 0x6
#define CNTRCNTRL 0x7

#define DEVFORMAT "alog%d%c%d"
#define CLOCK2FREQ 4.165
#define MIN_MICRO_PERIOD 25  
#define MAX_MICRO_PERIOD (65535/CLOCK2FREQ*PRIMARY_STATES)
#define DEFAULT_MICRO_PERIOD MAX_MICRO_PERIOD
#define READMAXTRIG 0.75*FIFOSIZE
#define ALOGPRI PRIBIO
#define ALOGMSG "alogio"

#define PRIMARY_STATES 2  /* Setup and conversion are clock tick consuming */
#define STATE_SETUP 0
#define STATE_CONVERT 1
#define STATE_READ 2

/* Notes on interrupt driven A/D conversion:
 * On the AIO8-P, interrupt driven conversion (the only type supported by this
 * driver) is facilitated through 8253 timer #2.  In order for interrrupts to
 * be generated you must connect line 6 to line 24 (counter 2 output to 
 * interrupt input) and line 23 to line 29 (counter 2 gate to +5VDC). 
 * Due to the design of the AIO8-P this precludes the use of programmable 
 * gain control.
 */

/* mode bits for the status register */

#define EOC 0x80 
#define IEN 0x08  
#define IMUXMASK 0x07
#define EMUXMASK 0xf0

/* mode bits for counter controller */

#define LD2MODE4 0xb8

/* Minor allocations:
 * UCCCCMMM
 * U: board unit (0-1)
 * CCCC: external multiplexer channel (0-15) (on AT-16P units)
 * MMM: internal multiplexer channel (0-7) (on AIO8-P card)
 */

#define UNIT(dev) ((minor(dev) & 0x80) >> 7)
#define CHANNEL(dev) (minor(dev) & 0x7f) 
#define EMUX(chan) ((chan & 0x78) >> 3)
#define EMUXMAKE(chan) ((chan & 0x78) << 1)
#define IMUX(chan) (chan & 0x07)
#define LMINOR(unit, chan) ((unit << 7)+chan)

/* port statuses */

#define STATUS_UNUSED 0 
#define STATUS_INUSE 1
#define STATUS_STOPPED 2
#define STATUS_INIT 3

/* Type definitions */

typedef struct
{
  short status; /* the status of this chan */
  struct selinfo readpoll; /* the poll() info */
  u_short fifo[FIFOSIZE]; /* fifo for this chan */
  int fifostart, fifoend; /* the ptrs showing where info is stored in fifo */
  int fifosize, fifotrig; /* the current and trigger size of the fifo */
  void *devfs_token; /* the devfs token for this chan */
  int nextchan;  
} talog_chan;

typedef struct 
{ 
  struct isa_device *isaunit; /* ptr to isa device information */     
  talog_chan chan[NUMCHANNELS]; /* the device nodes */
  int curchan; /* the current chan being intr handled */
  int firstchan; /* the first chan to go to in list */
  int state; /* is the node in setup or convert mode */ 
  long microperiod; /* current microsecond period setting */ 
  u_char perlo, perhi; /* current values to send to clock 2 after every intr */
   
} talog_unit;

/* Function Prototypes */

static int alog_probe (struct isa_device *idp);  /* Check for alog board */
static int alog_attach (struct isa_device *idp);  /* Take alog board */
static int sync_clock2 (int unit, long period); /* setup clock 2 period */
static __inline int putfifo (talog_chan *pchan, u_short fifoent);
static int alog_open (dev_t dev, int oflags, int devtype, struct proc *p);
static int alog_close (dev_t dev, int fflag, int devtype, struct proc *p);
static int alog_ioctl (dev_t dev, int cmd, caddr_t data,
		        int fflag, struct proc *p);
static int alog_read (dev_t dev, struct uio *uio, int ioflag);
static int alog_poll (dev_t dev, int events, struct proc *p);

/* Global Data */

static int alog_devsw_installed = 0;  /* Protect against reinit multiunit */
static talog_unit *alog_unit[NALOG]; /* data structs for each unit */ 

/* Character device switching structure */
static struct cdevsw alog_cdevsw = { alog_open, alog_close, alog_read,
                                     nowrite, alog_ioctl, nostop, noreset,
                                     nodevtotty, alog_poll, nommap,
                                     nostrategy, "alog", NULL, -1 };

/* Structure expected to tell how to probe and attach the driver
 * Must be published externally (cannot be static) */
struct isa_driver alogdriver = { alog_probe, alog_attach, "alog", 0 };


/* handle the ioctls */
static int alog_ioctl (dev_t dev, int cmd, caddr_t data,
		        int fflag, struct proc *p)
{
  int unit = UNIT(dev);
  int chan = CHANNEL(dev);
  talog_unit *info = alog_unit[unit];
  int s;
   
  switch (cmd)
   {  
    case FIONBIO: return 0; /* this allows for non-blocking ioctls */ 

    case AD_NCHANS_GET: *(int *)data = NUMCHANNELS;
                        return 0;
    case AD_FIFOSIZE_GET: *(int *)data = FIFOSIZE;
                          return 0;
      
    case AD_FIFO_TRIGGER_GET: s = spltty();
	                      *(int *)data = info->chan[chan].fifotrig;
	                      splx(s);
                              return 0;
      
    case AD_FIFO_TRIGGER_SET: 
                  s = spltty();
                  if ((*(int *)data < 1) || (*(int *)data > FIFOSIZE))
	           {   
	             splx(s);
		     return EPERM; 
		   }
                  info->chan[chan].fifotrig = *(int *)data;
                  splx(s);
                  return 0;
      
    case AD_STOP: s = spltty();
                  info->chan[chan].status = STATUS_STOPPED;              
                  splx(s);
                  return 0;
      
    case AD_START: s = spltty();
                   info->chan[chan].status = STATUS_INUSE;
                   splx(s);
                   return 0;

    case AD_MICRO_PERIOD_SET: 
                   s = spltty();
                   if (sync_clock2 (unit, *(long *) data))
	            {          
                      splx(s);
		      return EPERM;
                    }
                   splx(s);
                   return 0;
      
    case AD_MICRO_PERIOD_GET: s = spltty();
                              *(long *)data = info->microperiod;
                              splx(s);
                              return 0;    
                              
   }
   
  return ENOTTY;  
}


/* handle poll() based read polling */
static int alog_poll (dev_t dev, int events, struct proc *p)
{
  int unit = UNIT(dev);
  int chan = CHANNEL(dev);
  talog_unit *info = alog_unit[unit];
  int s;
   
  s = spltty();
  if (events & (POLLIN | POLLRDNORM)) /* if polling for any/normal data */
   if (info->chan[chan].fifosize >= info->chan[chan].fifotrig)
    { 
      splx(s);
       
      return events & (POLLIN | POLLRDNORM); /* ready for any/read */
    }
   else    
    {
      /* record this request */
      selrecord (p, &(info->chan[chan].readpoll));
      splx(s);
      return 0; /* not ready, yet */
    }
     
  splx(s);
  return 0; /* not ready (any I never will be) */
}


/* how to read from the board */
static int alog_read (dev_t dev, struct uio *uio, int ioflag)
{
  int unit = UNIT(dev);
  int chan = CHANNEL(dev);
  talog_unit *info = alog_unit[unit];
  int s, oldtrig, toread, err = 0;

  s = spltty();
      
  oldtrig = info->chan[chan].fifotrig; /* save official trigger value */   
  while (uio->uio_resid >= sizeof(u_short)) /* while uio has space */
   {
     if (!info->chan[chan].fifosize) /* if we have an empty fifo */
      {
        if (ioflag & IO_NDELAY) /* exit if we are non-blocking */
	   { err = EWOULDBLOCK;
	     break; 
           }
        /* Start filling fifo on first blocking read */
	if (info->chan[chan].status == STATUS_INIT)
	     info->chan[chan].status = STATUS_INUSE;	 
        /* temporarily adjust the fifo trigger to be optimal size */
	info->chan[chan].fifotrig = 
	  min (READMAXTRIG, uio->uio_resid / sizeof(u_short));
	/* lets sleep until we have some io available or timeout */
        err = tsleep (&(info->chan[chan].fifo), ALOGPRI | PCATCH, ALOGMSG,
			 info->chan[chan].fifotrig*READTIMO);
	if (err == EWOULDBLOCK)
	 {    printf (DEVFORMAT ": read timeout\n", unit,
	 	       'a'+EMUX(chan), IMUX(chan));	    
	 }
        if (err == ERESTART) err = EINTR; /* don't know how to restart */
        if (err) break; /* exit if any kind of error or signal */
      }
	 
     /* ok, now if we got here there is something to read from the fifo */
     
     /* calculate how many entries we can read out from the fifostart
      * pointer */ 
     toread = min (uio->uio_resid / sizeof(u_short), 
		   min (info->chan[chan].fifosize, 
		         FIFOSIZE - info->chan[chan].fifostart));
     /* perform the move, if there is an error then exit */
     if (err = uiomove((caddr_t)
			&(info->chan[chan].fifo[info->chan[chan].fifostart]), 
	                   toread * sizeof(u_short), uio)) break;
     info->chan[chan].fifosize -= toread; /* fifo this much smaller */ 
     info->chan[chan].fifostart += toread; /* we got this many more */
     if (info->chan[chan].fifostart == FIFOSIZE)
       info->chan[chan].fifostart = 0; /* wrap around fifostart */
      
   }
  info->chan[chan].fifotrig = oldtrig; /* restore trigger changes */
  splx(s);		       
  return err;
}

   
/* open a channel */
static int alog_open (dev_t dev, int oflags, int devtype, struct proc *p)
{
  int unit = UNIT(dev); /* get unit no */
  int chan = CHANNEL(dev); /* get channel no */
  talog_unit *info; 
  int s; /* priority */
  int cur;
   
  if ((unit >= NALOG) || (unit >= MAXUNITS) || (chan >= NUMCHANNELS))
      return ENXIO; /* unit and channel no ok ? */
  if (!alog_unit[unit]) return ENXIO; /* unit attached */
  info = alog_unit[unit]; /* ok, this is valid now */
  
  if (info->chan[chan].status) return EBUSY; /* channel busy */
  if (oflags & FREAD)
   {
     s=spltty();
     info->chan[chan].status = STATUS_INIT; /* channel open, read waiting */
     info->chan[chan].fifostart = info->chan[chan].fifoend =
	info->chan[chan].fifosize = 0;/* fifo empty */
     info->chan[chan].fifotrig = DEFAULT_FIFO_TRIGGER;
     if (info->firstchan < 0) /* if empty chain */
      {
	info->firstchan = info->curchan = chan; /* rev up the list */
	info->chan[chan].nextchan = -1; /* end of the list */
      }
     else /* non empty list must insert */
      {	 
	if (chan < info->firstchan) /* this one must become first in list */
	 {
	   info->chan[chan].nextchan = info->firstchan;
	   info->firstchan = chan;  
	 }
 	else /* insert this one as second - last in chan list */
	 {
	   cur = info->firstchan;
	    
	   /* traverse list as long as cur is less than chan and cur is
	    * not last in list */
	   while ((info->chan[cur].nextchan < chan) && 
	           (info->chan[cur].nextchan >= 0))
	     cur = info->chan[cur].nextchan; 
	   
	   /* now cur should point to the entry right before yours */
	   info->chan[chan].nextchan = info->chan[cur].nextchan;
	   info->chan[cur].nextchan = chan; /* insert yours in */
	 }
      }
     splx(s); 
     return 0; /* open successful */  
   }
  return EPERM; /* this is a read only device */ 
}


/* close a channel */
static int alog_close (dev_t dev, int fflag, int devtype, struct proc *p)
{
  int unit = UNIT(dev);
  int chan = CHANNEL(dev);
  talog_unit *info = alog_unit[unit];
  int s;
  int cur; 
   
  s = spltty();
  info->chan[chan].status = STATUS_UNUSED; 
  
  /* what if we are in the middle of a conversion ?
   * then smoothly get us out of it: */
  if (info->curchan == chan)
   { /* if we are last in list set curchan to first in list */
     if ((info->curchan = info->chan[chan].nextchan) < 0)
      info->curchan = info->firstchan;

     info->state = STATE_SETUP;
   }
   
  /* if this is the first channel, then make the second channel the first
   * channel (note that if this is also the only channel firstchan becomes
   * -1 and so the list is marked as empty */
   
  if (chan == info->firstchan) 
   info->firstchan = info->chan[chan].nextchan;
  else /* ok, so there must be at least 2 channels (and it is not the first) */
   {  
     cur = info->firstchan;

     /* find the entry before it (which must exist if you are closing) */ 
     while (info->chan[cur].nextchan < chan) 
	cur = info->chan[cur].nextchan;
     /* at this point we must have the entry before ours */
     info->chan[cur].nextchan = info->chan[chan].nextchan; /* give our link */       
   
   }
  
  splx(s);
   
  return 0; /* close always successful */
}


/* The probing routine - returns number of bytes needed */
static int alog_probe (struct isa_device *idp)
{
  int unit = idp->id_unit;  /* this device unit number */
  int iobase = idp->id_iobase; /* the base address of the unit */
  int addr; 
   
  if ((unit < 0) || (unit >= NALOG) || (unit >= MAXUNITS))
   { 
     printf ("alog: invalid unit number (%d)\n", unit);
     return 0;
   }
   
  /* the unit number is ok, lets check if used */
  if (alog_unit[unit]) 
   {
     printf ("alog: unit (%d) already attached\n", unit);
     return 0;
   }

  if (inb (iobase+STATUS) & EOC) return 0; /* End of conv bit should be 0 */
  for (addr=0; addr<NUMIMUXES; addr++)
   {
     outb (iobase+STATUS, EMUXMASK|addr);/* output ones to upper nibbl+addr */ 
     /* get back a zero in MSB and the addr where you put it */
     if ((inb (iobase+STATUS) & (EOC|IMUXMASK)) != addr) return 0;
   }
 
  return NUMPORTS; /* this device needs this many ports */ 
}


/* setup the info structure correctly for reloading clock 2 after interrupt */
static int sync_clock2 (int unit, long period)
{
  int clockper;
  talog_unit *info = alog_unit[unit];
   
  if ((period > MAX_MICRO_PERIOD) || (period < MIN_MICRO_PERIOD))
     return -1; /* error period too long */
  info->microperiod = period; /* record the period */   
  clockper = (CLOCK2FREQ * period) / PRIMARY_STATES;
  info->perlo = clockper & 0xff; /* least sig byte of clock period */
  info->perhi = ((clockper & 0xff00) >> 8); /* most sig byte of clock period */
  return 0;
}


/* The attachment routine - returns true on success */
static int alog_attach (struct isa_device *idp)
{
  int unit = idp->id_unit;  /* this device unit number */   
  int iobase = idp->id_iobase; /* the base address of the unit */ 
  talog_unit *info; /* pointer to driver specific info for unit */
  int chan; /* the channel used for creating devfs nodes */
   
  if (!(info = malloc(sizeof(*info), M_DEVBUF, M_NOWAIT)))
   {
     printf ("alog%d: cannot allocate driver storage\n", unit);
     return 0;
   }
  alog_unit[unit] = info; /* make sure to save the pointer */
  bzero (info, sizeof(*info)); /* clear info structure to all false */
  info->isaunit = idp;  /* store ptr to isa device information */
  sync_clock2 (unit, DEFAULT_MICRO_PERIOD); /* setup perlo and perhi */ 
  info->firstchan = -1; /* channel lists are empty */
   
  /* insert devfs nodes */
  
#ifdef DEVFS   
  for (chan=0; chan<NUMCHANNELS; chan++)
    info->chan[chan].devfs_token = 
     devfs_add_devswf(&alog_cdevsw, LMINOR(unit, chan), DV_CHR,
	              UID_ROOT, GID_WHEEL, 0400, DEVFORMAT,
		        unit, 'a'+EMUX(chan), IMUX(chan));
#endif   

  printf ("alog%d: %d channels, %d bytes/FIFO, %d entry trigger\n",
            unit, NUMCHANNELS, FIFOSIZE*sizeof(u_short), 
            DEFAULT_FIFO_TRIGGER);
  alogintr (unit); /* start the periodic interrupting process */    
  return 1; /* obviously successful */
}


/* Unit interrupt handling routine (interrupts generated by clock 2) */
void alogintr (int unit)
{
  talog_unit *info = alog_unit[unit];
  int iobase = info->isaunit->id_iobase;
  u_short fifoent;
   
   
  if (info->firstchan >= 0) /* ? is there even a chan list to traverse */
   switch (info->state)
    {  
      case STATE_READ: 
       if (info->chan[info->curchan].status == STATUS_INUSE)
	{ 
         if (inb (iobase+STATUS) & EOC) /* check that conversion finished */
	  printf (DEVFORMAT ": incomplete conversion\n", unit,
	 	   'a'+EMUX(info->curchan), IMUX(info->curchan));	    
	 else /* conversion is finished (should always be) */
	  {   
	   fifoent = (inb (iobase+ADHIGH) << 8) +
	               inb (iobase+ADLOW);
           if (putfifo(&(info->chan[info->curchan]), fifoent))
    	    {
	       printf (DEVFORMAT ": fifo overflow\n", unit,
	 	       'a'+EMUX(info->curchan), IMUX(info->curchan));	    
	    }
           if (info->chan[info->curchan].fifosize >=
	        info->chan[info->curchan].fifotrig)
	    {
	      /* if we've reached trigger levels */
	      selwakeup (&(info->chan[info->curchan].readpoll));
	      wakeup (&(info->chan[info->curchan].fifo));
	    }
	  }
	 }
       /* goto setup state for next channel on list */
       if ((info->curchan = info->chan[info->curchan].nextchan) < 0)
	info->curchan = info->firstchan;  
       /* notice lack of break here this implys a STATE_SETUP */ 
      case STATE_SETUP: /* set the muxes and let them settle */
#if NUMCHANNELS > NUMIMUXES    /* only do this if using external muxes */
       outb (iobase+STATUS, 
	      EMUXMAKE(info->curchan) | IMUX(info->curchan) | IEN);
       info->state = STATE_CONVERT;
       break;
#endif
      case STATE_CONVERT: 
       outb (iobase+STATUS, 
	      EMUXMAKE(info->curchan) | IMUX(info->curchan) | IEN);
       outb (iobase+ADHIGH, 0); /* start the conversion */
       info->state = STATE_READ;
       break;
    }
  else /* this is kind of like an idle mode */ 
   {
      outb (iobase+STATUS, IEN); /* no list keep getting interrupts though */
      /* since we have no open channels spin clock rate down to 
       * minimum to save interrupt overhead */
      outb (iobase+CNTRCNTRL, LD2MODE4); /* counter 2 to mode 4 strobe */
      outb (iobase+CNTR2, 0xff); /* longest period we can generate */
      outb (iobase+CNTR2, 0xff); 
      return;   
   }
  outb (iobase+CNTRCNTRL, LD2MODE4); /* counter 2 to mode 4 strobe */
  outb (iobase+CNTR2, info->perlo); /* low part of the period count */
  outb (iobase+CNTR2, info->perhi); /* high part of the period count */
}

   
/* this will put an entry in fifo, returns 1 if the first item in 
 * fifo was wiped (overflow) or 0 if everything went fine */
static int __inline putfifo (talog_chan *pchan, u_short fifoent)
{   
   pchan->fifo[pchan->fifoend] = fifoent; /* insert the entry in */
   pchan->fifoend++; /* one more in fifo */
   if (pchan->fifoend == FIFOSIZE) pchan->fifoend = 0; /* wrap around */ 
   /* note: I did intend to write over the oldest entry on overflow */
   if (pchan->fifosize == FIFOSIZE) /* overflowing state already */
    {
       pchan->fifostart++;
       if (pchan->fifostart == FIFOSIZE) pchan->fifostart = 0;
       return 1; /* we overflowed */
    }
   pchan->fifosize++; /* actually one bigger, else same size */
   return 0; /* went in just fine */ 
}
   

/* Driver initialization */
static void alog_drvinit (void *unused)
{
  dev_t dev;  /* Type for holding device major/minor numbers (int) */

  if (!alog_devsw_installed)
   {
     dev = makedev (CDEV_MAJOR, 0);  /* description of device major */
     cdevsw_add (&dev, &alog_cdevsw, NULL);  /* put driver in cdev table */
     alog_devsw_installed=1;
   }
}

/* System initialization call instance */

SYSINIT (alogdev, SI_SUB_DRIVERS, SI_ORDER_MIDDLE+CDEV_MAJOR,
         alog_drvinit,NULL);

#endif
