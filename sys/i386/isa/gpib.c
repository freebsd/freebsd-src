
/*
 * GPIB driver for FreeBSD.
 * Version 0.1 (No interrupts, no DMA)
 * Supports National Instruments AT-GPIB and AT-GPIB/TNT boards.
 * (AT-GPIB not tested, but it should work)
 *
 * Written by Fred Cawthorne (fcawth@delphi.umd.edu)
 * Some sections were based partly on the lpt driver.
 *  (some remnants may remain)
 *
 * This software is distributed with NO WARRANTIES, not even the implied
 * warranties for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * The author grants any other persons or organizations permission to use
 * or modify this software as long as this message is kept with the software,
 * all derivative works or modified versions.
 *
 *
 */
/*Please read the README file for usage information*/

#include "gp.h"

#if NGP > 0

#include "param.h"
#include "buf.h"
#include "systm.h"
#include "sys/ioctl.h"
#include "proc.h"
#include "user.h"
#include "uio.h"
#include "kernel.h"
#include "malloc.h"

#include <machine/clock.h>

#include "i386/isa/gpibreg.h"
#include "i386/isa/gpib.h"
#include "i386/isa/isa.h"
#include "i386/isa/isa_device.h"

#define MIN(a,b) ((a < b) ? a : b)

#define GPIBPRI  (PZERO+8)|PCATCH
#define SLEEP_MAX 1000
#define SLEEP_MIN 4

#ifdef JREMOD
#include <sys/conf.h>
#define CDEV_MAJOR 44
static void 	gp_devsw_install();
#endif /*JREMOD*/

int initgpib(void);
void closegpib(void);
int sendgpibfifo(unsigned char device,char *data,int count);
int sendrawgpib(unsigned char device,char *data,int count);
int sendrawgpibfifo(unsigned char device,char *data,int count);
int readgpibfifo(unsigned char device,char *data,int count);
void showregs(void);
void enableremote(unsigned char device);
void gotolocal(unsigned char device);
void menableremote(unsigned char *device);
void mgotolocal(unsigned char *device);
void mtrigger(unsigned char *device);
void trigger(unsigned char device);
void mdevclear(unsigned char *device);
void devclear(unsigned char device);
char spoll(unsigned char device);

int gpprobe(struct isa_device *dvp);
int gpattach();

struct   isa_driver gpdriver = {gpprobe, gpattach, "gp"};

#define   BUFSIZE      1024
#define   ATTACHED     0x08
#define   OPEN         0x04
#define   INIT	       0x02


static struct gpib_softc {
	char	*sc_cp;		/* current data to send		*/
	int	sc_count;	/* bytes queued in sc_inbuf	*/
	int	sc_type;	/* Type of gpib controller	*/
	u_char	sc_flags;	/* flags (open and internal)	*/
        char	sc_unit;	/* gpib device number		*/
        char	*sc_inbuf;	/* buffer for data		*/
} gpib_sc;
static int oldcount;
static char oldbytes[2];
/*Probe routine*/
/*This needs to be changed to be a bit more robust*/
int
gpprobe(struct isa_device *dvp)
{
	int	status;
        struct gpib_softc *sc = &gpib_sc;


	gpib_port = dvp->id_iobase;
        status=1;
        sc->sc_type=3;
if ((inb(KSR)&0xF7)==0x34) sc->sc_type=3;
else if ((inb(KSR)&0xF7)==0x24) sc->sc_type=2;
else if ((inb(KSR)&0xF7)==0x14) sc->sc_type=1;
          else status=0;

        return (status);
}

/*
 * gpattach()
 *  Attach device and print the type of card to the screen.
 */
int
gpattach(isdp)
	struct isa_device *isdp;
{
	struct   gpib_softc   *sc = &gpib_sc;

	sc->sc_unit = isdp->id_unit;
        if (sc->sc_type==3)
           printf ("gp%d: type AT-GPIB/TNT\n",sc->sc_unit);
        if (sc->sc_type==2)
           printf ("gp%d: type AT-GPIB chip NAT4882B\n",sc->sc_unit);
        if (sc->sc_type==1)
           printf ("gp%d: type AT-GPIB chip NAT4882A\n",sc->sc_unit);
        sc->sc_flags |=ATTACHED;
#ifdef JREMOD
        gp_devsw_install();
#endif /*JREMOD*/

        return (1);
}

/*
 * gpopen()
 *	New open on device.
 *
 * More than 1 open is not allowed on the entire device.
 * i.e. even if gpib5 is open, we can't open another minor device
 */
int
gpopen(dev, flags, fmt, p)
	dev_t dev;
	int flags;
	int fmt;
	struct proc *p;
{
	struct gpib_softc *sc = &gpib_sc;
	u_char unit;
	int status;

       unit= minor(dev);

	/* minor number out of limits ? */
	if (unit >= 32)
		return (ENXIO);

	/* Attached ? */
	if (!(sc->sc_flags&ATTACHED)) { /* not attached */
		return(ENXIO);
	}

	/* Already open  */
	if (sc->sc_flags&OPEN) { /* too late .. */
		return(EBUSY);
	}

	/* Have memory for buffer? */
	sc->sc_inbuf = malloc(BUFSIZE, M_DEVBUF, M_WAITOK);
	if (sc->sc_inbuf == 0)
		return(ENOMEM);

        if (initgpib()) return(EBUSY);
        sc->sc_flags |= OPEN;
	sc->sc_count = 0;
        oldcount=0;
if (unit!=0) {  /*Someone is trying to access an actual device*/
                /*So.. we'll address it to listen*/
enableremote(unit);
 do {
 status=inb(ISR2);
 }
 while (!(status&8)&&tsleep((caddr_t)&gpib_sc, GPIBPRI,"gpibpoll",1)==EWOULDBLOCK);

 outb(CDOR,(unit&31)+32);/*address device to listen*/

 do
 status=inb(ISR2);
 while (!(status&8)&&tsleep((caddr_t)&gpib_sc, GPIBPRI,"gpibpoll",1)==EWOULDBLOCK);
 outb (CDOR,64); /*Address controller (me) to talk*/
 do status=inb(ISR2);

 while (!(status&8)&&tsleep((caddr_t)&gpib_sc, GPIBPRI,"gpibpoll",1)==EWOULDBLOCK);
 outb(AUXMR,gts); /*Set to Standby (Controller)*/


 do
 status=inb(ISR1);
 while (!(status&2)&&tsleep((caddr_t)&gpib_sc, GPIBPRI,"gpibpoll",1)==EWOULDBLOCK);
/*Set up the TURBO488 registers*/
 outb(IMR2,0x30); /*we have to enable DMA (0x30) for turbo488 to work*/
 outb(CNT0,0);   /*NOTE this does not enable DMA to the host computer!!*/
 outb(CNT1,0);
 outb(CNT2,0);
 outb(CNT3,0);
 outb(CMDR,0x20);
 outb(CFG,0x47); /* 16 bit, write, fifo B first, TMOE TIM */
 outb(CMDR,0x10); /*RESET fifos*/
 outb(CMDR,0x04); /*Tell TURBO488 to GO*/
}
	return(0);
}


/*
 * gpclose()
 *	Close gpib device.
 */
int
gpclose(dev, flags, fmt, p)
	dev_t dev;
	int flags;
	int fmt;
	struct proc *p;
{
	struct gpib_softc *sc = &gpib_sc;
        unsigned char unit;
        unsigned char status;

        unit=minor(dev);
if (unit!=0) { /*Here we need to send the last character with EOS*/
               /*and unaddress the listening device*/


  status=EWOULDBLOCK;

  /*Wait for fifo to become empty*/
  do {
  status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",1);
   }
  while ((inb(ISR3)&0x04)&&status==EWOULDBLOCK); /*Fifo is not empty*/

 outb(CMDR,0x08); /*Issue STOP to TURBO488*/

  /*Wait for DONE and STOP*/
 if (status==EWOULDBLOCK) do {
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",1);
   }
  while (!(inb(ISR3)&0x11)&&status==EWOULDBLOCK); /*not done and stop*/

/*Shut down TURBO488 */
 outb(IMR2,0x00); /*DISABLE DMA to turbo488*/
 outb(CMDR,0x20); /*soft reset turbo488*/
 outb(CMDR,0x10); /*reset fifos*/


/*Send last byte with EOI set*/
/*Send second to last byte if there are 2 bytes left*/
if (status==EWOULDBLOCK)  {

do
 if (!(inb(ISR1)&2)) status=tsleep((caddr_t)&gpib_sc, GPIBPRI,"gpibpoll",1);
while (!(inb(ISR1)&2)&&(status==EWOULDBLOCK));
if (oldcount==2){
 outb(CDOR,oldbytes[0]); /*Send second to last byte*/
 while (!(inb(ISR1)&2)&&(status==EWOULDBLOCK));
  status=tsleep((caddr_t)&gpib_sc, GPIBPRI,"gpibpoll",1);
 }

   outb(AUXMR,seoi);  /*Set EOI for the last byte*/
   outb(AUXMR,0x5E); /*Clear SYNC*/
   if (oldcount==1)
   outb(CDOR,oldbytes[0]);
    else
   if (oldcount==2)
   outb(CDOR,oldbytes[1]);
   else {
   outb (CDOR,13); /*Send a CR.. we've got trouble*/
   printf("gpib: Warning: gpclose called with nothing left in buffer\n");
   }
}

do
 if (!(inb(ISR1)&2)) status=tsleep((caddr_t)&gpib_sc, GPIBPRI,"gpibpoll",1);
while (!(inb(ISR1)&2)&&(status==EWOULDBLOCK));


 if (!(inb(ISR1)&2)&&status==EWOULDBLOCK) do
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",1);
 while (!(inb(ISR1)&2)&&status==EWOULDBLOCK);


 outb(AUXMR,tca); /* Regain full control of the bus*/


 do
  status=inb(ISR2);
 while (!(status&8)&&tsleep((caddr_t)&gpib_sc, GPIBPRI,"gpibpoll",1)==EWOULDBLOCK);
 outb(CDOR,63); /*unlisten*/
 do
  status=inb(ISR2);
 while (!(status&8)&&tsleep((caddr_t)&gpib_sc, GPIBPRI,"gpibpoll",1)==EWOULDBLOCK);
 outb(AUXMR,0x5E); /*Clear SYNC*/
 outb (CDOR,95);/*untalk*/
 do
  status=inb(ISR2);
 while (!(status&8)&&tsleep((caddr_t)&gpib_sc, GPIBPRI,"gpibpoll",1)==EWOULDBLOCK);
/*gotolocal(minor(dev));*/
}
	closegpib();
	sc->sc_flags = ATTACHED;
	free(sc->sc_inbuf, M_DEVBUF);
	sc->sc_inbuf = 0;	/* Sanity */
	return(0);
}

/*
 * gpwrite()
 *	Copy from user's buffer, then write to GPIB device referenced
 *    by minor(dev).
 */
int
gpwrite(dev, uio, ioflag)
	dev_t dev;
	struct uio *uio;
	int ioflag;
{
	int err,count;

	/* main loop */
	while ((gpib_sc.sc_count = MIN(BUFSIZE-1, uio->uio_resid)) > 0) {
		/*  If there were >1 bytes left over, send them  */
                if (oldcount==2)
                  sendrawgpibfifo(minor(dev),oldbytes,2);

                /*If there was 1 character left, put it at the beginning
                   of the new buffer*/
                if (oldcount==1){
                   (gpib_sc.sc_inbuf)[0]=oldbytes[0];
	           gpib_sc.sc_cp = gpib_sc.sc_inbuf;
		/*  get from user-space  */
		   uiomove(gpib_sc.sc_inbuf+1, gpib_sc.sc_count, uio);
                   gpib_sc.sc_count++;
	           }
                 else {
                gpib_sc.sc_cp = gpib_sc.sc_inbuf;
		/*  get from user-space  */
		uiomove(gpib_sc.sc_inbuf, gpib_sc.sc_count, uio);
                    }

/*NOTE we always leave one byte in case this is the last write
  so close can send EOI with the last byte There may be 2 bytes
  since we are doing 16 bit transfers.(note the -1 in the count below)*/
      /*If count<=2 we'll either pick it up on the next write or on close*/
            if (gpib_sc.sc_count>2) {
 		count = sendrawgpibfifo(minor(dev),gpib_sc.sc_cp,gpib_sc.sc_count-1);
                err=!count;
		if (err)
			return(1);
                oldcount=gpib_sc.sc_count-count; /*Set # of remaining bytes*/
		gpib_sc.sc_count-=count;
                gpib_sc.sc_cp+=count; /*point char pointer to remaining bytes*/
              }
                else oldcount=gpib_sc.sc_count;
                  oldbytes[0]=gpib_sc.sc_cp[0];
                if (oldcount==2)
                  oldbytes[1]=gpib_sc.sc_cp[1];
	}
	return(0);
}
/* Here is how you would usually access a GPIB device
   An exception would be a plotter or printer that you can just
   write to using a minor device = its GPIB address */

int
gpioctl(dev_t dev, int cmd, caddr_t data, int flags, struct proc *p)
{
	struct gpibdata *gd = (struct gpibdata *)data;
	int	error,result;
	error = 0;

	switch (cmd) {
	case GPIBWRITE:
		sendgpibfifo(gd->address,gd->data,*(gd->count));
		error=0;
		break;
        case GPIBREAD:
                result=readgpibfifo(gd->address,gd->data,*(gd->count));
                *(gd->count)=result;
                error=0;
                break;
        case GPIBINIT:
                initgpib();
                error=0;
                break;
        case GPIBTRIGGER:
                trigger(gd->address);
                error=0;
                break;
        case GPIBREMOTE:
                enableremote(gd->address);
                error=0;
                break;
        case GPIBLOCAL:
                gotolocal(gd->address);
                error=0;
                break;

        case GPIBMTRIGGER:
                mtrigger(gd->data);
                error=0;
                break;
        case GPIBMREMOTE:
                menableremote(gd->data);
                error=0;
                break;
        case GPIBMLOCAL:
                mgotolocal(gd->data);
                error=0;
                break;
        case GPIBSPOLL:
                *(gd->data)=spoll(gd->address);
                error=0;
                break;
        default:
		error = ENODEV;
	}

	return(error);
}




/*Just in case you want a dump of the registers...*/

void showregs() {
 printf ("NAT4882:\n");
 printf ("ISR1=%X\t",inb(ISR1));
 printf ("ISR2=%X\t",inb(ISR2));
 printf ("SPSR=%X\t",inb(SPSR));
 printf ("KSR =%X\t",inb(KSR));
 printf ("ADSR=%X\t",inb(ADSR));
 printf ("CPTR=%X\t",inb(CPTR));
 printf ("SASR=%X\t",inb(SASR));
 printf ("ADR0=%X\t",inb(ADR0));
 printf ("ISR0=%X\t",inb(ISR0));
 printf ("ADR1=%X\t",inb(ADR1));
 printf ("BSR =%X\n",inb(BSR));

 printf ("Turbo488\n");
 printf ("STS1=%X ",inb(STS1));
 printf ("STS2=%X ",inb(STS2));
 printf ("ISR3=%X ",inb(ISR3));
 printf ("CNT0=%X ",inb(CNT0));
 printf ("CNT1=%X ",inb(CNT1));
 printf ("CNT2=%X ",inb(CNT2));
 printf ("CNT3=%X ",inb(CNT3));
 printf ("IMR3=%X ",inb(IMR3));
 printf ("TIMER=%X\n",inb(TIMER));


 }
/*Set up the NAT4882 and TURBO488 registers */
/*This will be nonsense to you unless you have a data sheet from
  National Instruments.  They should give you one if you call them*/

int initgpib() {
  outb(CMDR,0x20);
  outb(CFG,0x16);
  outb(IMR3,0);
  outb(CMDR,0x10);
  outb(CNT0,0);
  outb(CNT1,0);
  outb(CNT2,0);
  outb(CNT3,0);
  outb(INTR,0); /* Put interrupt line in tri-state mode??*/
  outb(AUXMR,chip_reset);

  outb(IMR1,0x10); /* send interrupt to TURBO488 when END received*/
  outb(IMR2,0);
  outb(IMR0,0x90); /* Do we want nba here too??? */
  outb(ADMR,1);
  outb(ADR,0);
  outb(ADR,128);
  outb(AUXMR,0xE9);
  outb(AUXMR,0x49);
  outb(AUXMR,0x70);
  outb(AUXMR,0xD0);
  outb(AUXMR,0xA0);

  outb(EOSR,10); /*set EOS message to newline*/
                 /*should I make the default to interpret END as EOS?*/
                 /*It isn't now.  The following changes this*/
  outb(AUXMR,0x80);    /*No special EOS handling*/
 /*outb(AUXMR,0x88) */ /* Transmit END with EOS*/
 /*outb(AUXMR,0x84) */ /* Set END on EOS received*/
 /*outb(AUXMR,0x8C) */ /* Do both of the above*/


 /* outb(AUXMR,hldi); */ /*Perform RFD Holdoff for all data in*/
                         /*Not currently supported*/

  outb(AUXMR,pon);
  outb(AUXMR,sic_rsc);
 tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",1);

  outb(AUXMR,sic_rsc_off);

return(0);


   }

/*This is kind of Brute force..  But it works*/

void closegpib() {
   outb(AUXMR,chip_reset);
   }

/*GPIB ROUTINES:
  These will also make little sense unless you have a data sheet.
  Note that the routines with an "m" in the beginning are for
  accessing multiple devices in one call*/


/*This is one thing I could not figure out how to do correctly.
  I tried to use the auxilary  command to enable remote, but it
  never worked.  Here, I bypass everything and write to the BSR
  to enable the remote line.  NOTE that these lines are effectively
  "OR'ed" with the actual lines, so writing a 1 to the bit in the BSR
  forces the GPIB line true, no matter what the fancy circuitry of the
  NAT4882 wants to do with it*/

void enableremote(unsigned char device)
{
 int status;

status=EWOULDBLOCK;
  if (status==EWOULDBLOCK) do {
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",2);
   }
  while (!(inb(ISR2)&0x08)&&status==EWOULDBLOCK); /*Wait to send next cmd*/

 outb(BSR,1);           /*Set REN bit on GPIB*/
  if (status==EWOULDBLOCK) do {
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",2);
   }
  while (!(inb(ISR2)&0x08)&&status==EWOULDBLOCK); /*Wait to send next cmd*/
 outb(CDOR,(device&31)+32); /*address device to listen*/
  if (status==EWOULDBLOCK) do {
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",2);
   }
  while (!(inb(ISR2)&0x08)&&status==EWOULDBLOCK); /*Wait to send next cmd*/
 outb (CDOR,63); /*Unaddress device*/
  if (status==EWOULDBLOCK) do {
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",2);
   }
  while (!(inb(ISR2)&0x08)&&status==EWOULDBLOCK); /*Wait to send next cmd*/

 }
/*This does not release the REM line on the gpib port, because if it did,
  all the remote devices would go to local mode.  This only sends the
  gotolocal message to one device.  Currently, REM is always held true
  after enableremote is called, and is reset only on a close of the
  gpib device */

void gotolocal(unsigned char device)
{ int status;
  status=EWOULDBLOCK;

  if (status==EWOULDBLOCK) do {
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",2);
   }
  while (!(inb(ISR2)&0x08)&&status==EWOULDBLOCK); /*Wait to send next cmd*/

outb(CDOR,(device&31)+32);

  if (status==EWOULDBLOCK) do {
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",2);
   }
  while (!(inb(ISR2)&0x08)&&status==EWOULDBLOCK); /*Wait to send next cmd*/

outb(AUXMR,0x5E);  /*Clear SYNC*/
 outb (CDOR,1);

  if (status==EWOULDBLOCK) do {
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",2);
   }
  while (!(inb(ISR2)&0x08)&&status==EWOULDBLOCK); /*Wait to send next cmd*/

 outb(AUXMR,0x5E);
 outb (CDOR,63);/*unaddress device*/

  if (status==EWOULDBLOCK) do {
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",2);
   }
  while (!(inb(ISR2)&0x08)&&status==EWOULDBLOCK); /*Wait to send next cmd*/

 }


void menableremote(unsigned char *device)
{
 int status, counter = 0;

status=EWOULDBLOCK;
  if (status==EWOULDBLOCK) do {
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",2);
   }
  while (!(inb(ISR2)&0x08)&&status==EWOULDBLOCK); /*Wait to send next cmd*/

 outb(BSR,1);           /*Set REN bit on GPIB*/
 do
  {
  if (status==EWOULDBLOCK) do {
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",2);
   }
  while (!(inb(ISR2)&0x08)&&status==EWOULDBLOCK); /*Wait to send next cmd*/
 outb(CDOR,(device[counter]&31)+32); /*address device to listen*/
 counter++;
 }
 while (device[counter]<32);

  if (status==EWOULDBLOCK) do {
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",2);
   }
  while (!(inb(ISR2)&0x08)&&status==EWOULDBLOCK); /*Wait to send next cmd*/

 outb (CDOR,63); /*Unaddress device*/
  if (status==EWOULDBLOCK) do {
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",2);
   }
  while (!(inb(ISR2)&0x08)&&status==EWOULDBLOCK); /*Wait to send next cmd*/

 }

void mgotolocal(unsigned char *device)
{ int status;
  int counter=0;
status=EWOULDBLOCK;
 if (device[counter]<32) do {
  if (status==EWOULDBLOCK) do {
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",2);
   }
  while (!(inb(ISR2)&0x08)&&status==EWOULDBLOCK); /*Wait to send next cmd*/
 outb(CDOR,(device[counter]&31)+32);
 counter++;
 } while (device[counter]<32);
  if (status==EWOULDBLOCK) do {
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",2);
   }
  while (!(inb(ISR2)&0x08)&&status==EWOULDBLOCK); /*Wait to send next cmd*/

 outb(AUXMR,0x5E);  /*Clear SYNC*/
 outb (CDOR,1);


  if (status==EWOULDBLOCK) do {
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",2);
   }
  while (!(inb(ISR2)&0x08)&&status==EWOULDBLOCK); /*Wait to send next cmd*/
 outb(AUXMR,0x5E);
 outb (CDOR,63);/*unaddress device*/
  if (status==EWOULDBLOCK) do {
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",2);
   }
  while (!(inb(ISR2)&0x08)&&status==EWOULDBLOCK); /*Wait to send next cmd*/


 }
/*Trigger a device.  What happens depends on how the device is
 configured.  */

void trigger(unsigned char device)
{ int status;

status=EWOULDBLOCK;
 if (device<32)  {
  if (!(inb(ISR2)&0x08)) do {
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",1);
   }
  while (!(inb(ISR2)&0x08)&&status==EWOULDBLOCK); /*Wait to send next cmd*/
 outb(CDOR,(device&31)+32); /*address device to listen*/
  if (!(inb(ISR2)&0x08)&&status==EWOULDBLOCK) do {
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",1);
   }
  while (!(inb(ISR2)&0x08)&&status==EWOULDBLOCK); /*Wait to send next cmd*/

 outb (CDOR,8);  /*send GET*/

  if (!(inb(ISR2)&0x08)&&status==EWOULDBLOCK) do {
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",1);
   }
  while (!(inb(ISR2)&0x08)&&status==EWOULDBLOCK); /*Wait to send next cmd*/
 outb (AUXMR,0x5E);
 outb (CDOR,63);/*unaddress device*/
  if (!(inb(ISR2)&0x08)&&status==EWOULDBLOCK) do {
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",1);
   }
  while (!(inb(ISR2)&0x08)&&status==EWOULDBLOCK); /*Wait to send next cmd*/


 }
}

/*Trigger multiple devices by addressing them all to listen, and then
  sending GET*/

void mtrigger(unsigned char *device)
{ int status=EWOULDBLOCK;
  int counter=0;
 if(device[0]<32){
 do {
 if (device[counter]<32)
  if (!(inb(ISR2)&0x08)&&status==EWOULDBLOCK) do {
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",1);
   }
  while (!(inb(ISR2)&0x08)&&status==EWOULDBLOCK); /*Wait to send next cmd*/
 outb(CDOR,(device[counter]&31)+32); /*address device to listen*/
 counter++;
   }
 while (device[counter]<32);
  if (!(inb(ISR2)&0x08)&&status==EWOULDBLOCK) do {
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",1);
   }
  while (!(inb(ISR2)&0x08)&&status==EWOULDBLOCK); /*Wait to send next cmd*/
 outb (CDOR,8);  /*send GET*/

  if (!(inb(ISR2)&0x08)&&status==EWOULDBLOCK) do {
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",1);
   }
  while (!(inb(ISR2)&0x08)&&status==EWOULDBLOCK); /*Wait to send next cmd*/
 outb (AUXMR,0x5E);
 outb (CDOR,63);/*unaddress device*/
  if (!(inb(ISR2)&0x08)&&status==EWOULDBLOCK) do {
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",1);
   }
  while (!(inb(ISR2)&0x08)&&status==EWOULDBLOCK); /*Wait to send next cmd*/


 }
}


void mdevclear(unsigned char *device)
{ int status=EWOULDBLOCK;
  int counter=0;

 if (device[counter]<32) do {
  if (!(inb(ISR2)&0x08)) do {
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",1);
   }
  while (!(inb(ISR2)&0x08)&&status==EWOULDBLOCK); /*Wait to send next cmd*/
 outb(CDOR,(device[counter]&31)+32);
 counter++;
 } while (device[counter]<32);

  if (!(inb(ISR2)&0x08)&&status==EWOULDBLOCK) do {
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",1);
   }
  while (!(inb(ISR2)&0x08)&&status==EWOULDBLOCK); /*Wait to send next cmd*/
 outb(AUXMR,0x5E);  /*Clear SYNC*/
 outb (CDOR,0x14);  /*send DCL*/


  if (!(inb(ISR2)&0x08)&&status==EWOULDBLOCK) do {
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",1);
   }
  while (!(inb(ISR2)&0x08)&&status==EWOULDBLOCK); /*Wait to send next cmd*/

 outb(AUXMR,0x5E);
 outb (CDOR,63);/*unaddress device*/


  if (!(inb(ISR2)&0x08)&&status==EWOULDBLOCK) do {
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",1);
   }
  while (!(inb(ISR2)&0x08)&&status==EWOULDBLOCK); /*Wait to send next cmd*/
 outb(AUXMR,0x5E); /*Clear SYNC*/
 outb (CDOR,63);
  if (!(inb(ISR2)&0x08)&&status==EWOULDBLOCK) do {
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",1);
   }
  while (!(inb(ISR2)&0x08)&&status==EWOULDBLOCK); /*Wait to send next cmd*/

 }
void devclear(unsigned char device)
{ int status=EWOULDBLOCK;


 if (device<32)  {
  if (!(inb(ISR2)&0x08)) do {
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",1);
   }
  while (!(inb(ISR2)&0x08)&&status==EWOULDBLOCK); /*Wait to send next cmd*/
 outb(CDOR,(device&31)+32);

 }

  if (!(inb(ISR2)&0x08)&&status==EWOULDBLOCK) do {
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",1);
   }
  while (!(inb(ISR2)&0x08)&&status==EWOULDBLOCK); /*Wait to send next cmd*/
 outb(AUXMR,0x5E);  /*Clear SYNC*/
 outb (CDOR,0x14);  /*send DCL*/


  if (!(inb(ISR2)&0x08)&&status==EWOULDBLOCK) do {
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",1);
   }
  while (!(inb(ISR2)&0x08)&&status==EWOULDBLOCK); /*Wait to send next cmd*/

 outb(AUXMR,0x5E);
 outb (CDOR,63);/*unaddress device*/
  if (!(inb(ISR2)&0x08)&&status==EWOULDBLOCK) do {
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",1);
   }
  while (!(inb(ISR2)&0x08)&&status==EWOULDBLOCK); /*Wait to send next cmd*/

 outb(AUXMR,0x5E); /*Clear SYNC*/
 outb (CDOR,63);
  if (!(inb(ISR2)&0x08)&&status==EWOULDBLOCK) do {
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",1);
   }
  while (!(inb(ISR2)&0x08)&&status==EWOULDBLOCK); /*Wait to send next cmd*/

 }
/*This is not used now, but it should work with NI's 8 bit gpib board
  since it does not use the TURBO488 registers at all */

int sendrawgpib(unsigned char device,char *data,int count)
 {
 int status;
 int counter;
 int counter2;
 int done;

 counter=0;



 do {
done=EWOULDBLOCK;
counter2=5;
do{
 status=inb(ISR1);
 if (!(status&2)&&counter2){ DELAY(4); counter2--;}
 if (!(status&2)&&!counter2) done=tsleep((caddr_t)&gpib_sc, GPIBPRI,"gpibpoll",1);
 }
 while (!(status&2)&&(done==EWOULDBLOCK));
   if (done!=EWOULDBLOCK) return(done);

   if ((data[counter+1]==0)||(count+1)==0){

   outb(AUXMR,seoi);  /*Set EOI for the last byte*/
   outb(AUXMR,0x5E); /*Clear SYNC*/
   outb(CDOR,data[counter]);
   }
  else outb(CDOR,data[counter]);
 counter++;
 count--;
 }
 while((data[counter-1]!=0)&&(count+1)!=0);
 do
  status=inb(ISR1);
 while (!(status&2)&&tsleep((caddr_t)&gpib_sc, GPIBPRI,"gpibpoll",1)==EWOULDBLOCK);
return(counter-1);

}

/*Send data through the TURBO488 FIFOS to a device that is already
 addressed to listen.  This is used by the write call when someone is
 writing to a printer or plotter, etc... */
/*The last byte of each write is held off until either the next
 write or close, so it can be sent with EOI set*/

int sendrawgpibfifo(unsigned char device,char *data,int count)
 {
 int status;
 int counter;
 int fifopos;
 int sleeptime;


 sleeptime=SLEEP_MIN;
 counter=0;


 fifopos=0;

status=EWOULDBLOCK;
 do {
  /*Wait for fifo to become not full if it is full */
  sleeptime=SLEEP_MIN;
  if (!(inb(ISR3)&0x08)) do {
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",sleeptime);
   if (sleeptime<SLEEP_MAX) sleeptime=sleeptime*2;
   }
  while (!(inb(ISR3)&0x08)&&(status==EWOULDBLOCK)); /*Fifo is full*/

   if((count>1)&&(inb(ISR3)&0x08)){
   outw(FIFOB,*(unsigned*)(data+counter));
 /*  printf ("gpib: sent:%c,%c\n",data[counter],data[counter+1]);*/

  counter+=2;
  count-=2;
   }
  }
 while ((count>1)&&(status==EWOULDBLOCK));
/*The write routine and close routine must check if there is 1
  byte left and handle it accordingly*/


/*Return the number of bytes written to the device*/
 return(counter);



}






int sendgpibfifo(unsigned char device,char *data,int count)
 {
 int status;
 int counter;
 int fifopos;
 int sleeptime;

outb(IMR2,0x30); /*we have to enable DMA (0x30) for turbo488 to work*/
 outb(CNT0,0);
 outb(CNT1,0);
 outb(CNT2,0);
 outb(CNT3,0);
status=EWOULDBLOCK;
 if (!(inb(ISR2)&8)) do
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",1);
 while (!(inb(ISR2)&8)&&status==EWOULDBLOCK);

 outb(CDOR,(device&31)+32);/*address device to listen*/

 if (!(inb(ISR2)&8)&&status==EWOULDBLOCK) do
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",1);
 while (!(inb(ISR2)&8)&&status==EWOULDBLOCK);
 outb (CDOR,64); /*Address controller (me) to talk*/

 if (!(inb(ISR2)&8)&&status==EWOULDBLOCK) do
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",1);
 while (!(inb(ISR2)&8)&&status==EWOULDBLOCK);

 outb(AUXMR,gts); /*Set to Standby (Controller)*/
 fifopos=0;

 sleeptime=SLEEP_MIN;
 counter=0;


 fifopos=0;

 outb(CMDR,0x20);
 outb(CFG,0x47); /* 16 bit, write, fifo B first, TMOE TIM */
 outb(CMDR,0x10); /*RESET fifos*/
 outb(CCRG,seoi); /*program to send EOI at end*/
 outb(CMDR,0x04); /*Tell TURBO488 to GO*/
status=EWOULDBLOCK;
 do {
  /*Wait for fifo to become not full if it is full */
  sleeptime=SLEEP_MIN;
  if (!(inb(ISR3)&0x08)) do {
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",sleeptime);
   if (sleeptime<SLEEP_MAX) sleeptime=sleeptime*2;
   }
  while (!(inb(ISR3)&0x08)&&(status==EWOULDBLOCK)); /*Fifo is full*/

   if((count>1)&&(inb(ISR3)&0x08)){
   /*if(count==2) outb(CFG,15+0x40); *//*send eoi when done*/
   outw(FIFOB,*(unsigned*)(data+counter));

  counter+=2;
  count-=2;
   }
  }
 while ((count>2)&&(status==EWOULDBLOCK));

 if (count==2&&status==EWOULDBLOCK) {
  /*Wait for fifo to become not full*/
  if(status==EWOULDBLOCK&&!(inb(ISR3)&0x08)) do {
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",SLEEP_MIN);
   }
  while (!(inb(ISR3)&0x08)&&status==EWOULDBLOCK); /*Fifo is full*/
  /*outb(CFG,0x40+15);*//*send eoi when done*/
  outb(FIFOB,data[counter]);
  counter++;
  count--;
  }


 /*outb(CMDR,0x04);*/

  /*Wait for fifo to become empty*/
  if (status==EWOULDBLOCK) do {
  status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",1);
   }
  while ((inb(ISR3)&0x04)&&status==EWOULDBLOCK); /*Fifo is not empty*/

 outb(CMDR,0x08); /*Issue STOP to TURBO488*/

  /*Wait for DONE and STOP*/
 if (status==EWOULDBLOCK) do {
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",1);
   }
  while (!(inb(ISR3)&0x11)&&status==EWOULDBLOCK); /*not done and stop*/

 outb(IMR2,0x00); /*we have to enable DMA (0x30) for turbo488 to work*/
 outb(CMDR,0x20); /*soft reset turbo488*/
 outb(CMDR,0x10); /*reset fifos*/


/*Send last byte with EOI set*/
/*Here EOI is handled correctly since the string to be sent */
/*is actually all sent during the ioctl.  (See above)*/

if (count==1&&status==EWOULDBLOCK)  {  /*Count should always=1 here*/

do
 if (!(inb(ISR1)&2)) status=tsleep((caddr_t)&gpib_sc, GPIBPRI,"gpibpoll",1);
while (!(inb(ISR1)&2)&&(status==EWOULDBLOCK));

   outb(AUXMR,seoi);  /*Set EOI for the last byte*/
   outb(AUXMR,0x5E); /*Clear SYNC*/
   outb(CDOR,data[counter]);
 counter++;
 count--;
}

do
 if (!(inb(ISR1)&2)) status=tsleep((caddr_t)&gpib_sc, GPIBPRI,"gpibpoll",1);
while (!(inb(ISR1)&2)&&(status==EWOULDBLOCK));


 if (!(inb(ISR1)&2)&&status==EWOULDBLOCK) do
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",1);
 while (!(inb(ISR1)&2)&&status==EWOULDBLOCK);
 outb(AUXMR,tca); /* Regain full control of the bus*/


 if (!(inb(ISR2)&8)&&status==EWOULDBLOCK) do
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",1);
   while (!(inb(ISR2)&8)&&status==EWOULDBLOCK);

   outb(CDOR,63); /*unlisten*/


 if (!(inb(ISR2)&8)&&status==EWOULDBLOCK) do
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",1);
 while (!(inb(ISR2)&8)&&status==EWOULDBLOCK);


outb(AUXMR,0x5E); /*Clear SYNC*/
 outb (CDOR,95);/*untalk*/
 if (!(inb(ISR2)&8)&&status==EWOULDBLOCK) do
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",1);
 while (!(inb(ISR2)&8)&&status==EWOULDBLOCK);


 return(counter);



}

int readgpibfifo(unsigned char device,char *data,int count)
{
 int status;
 int status2 = 0;
 int status1;
 int counter;
 int fifopos;
 unsigned inword;

 outb(IMR2,0x30); /*we have to enable DMA (0x30) for turbo488 to work*/
 /*outb(IMR3,0x1F);
 outb(INTR,1); */
 outb(CMDR,0x20);

 outb(CFG,14+0x60+1); /* Halt on int,read, fifo B first, CCEN TMOE TIM */
 outb(CMDR,0x10); /*RESET fifos*/
 outb(CCRG,tcs); /*program to tcs at end*/
 outb(CMDR,0x08);/*STOP??*/



status=EWOULDBLOCK;
do
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",1);
 while (!(inb(ISR2)&8)&&status==EWOULDBLOCK);

 outb (CDOR,32); /*Address controller (me) to listen*/

  do
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",1);
 while (!(inb(ISR2)&8)&&status==EWOULDBLOCK);

 outb(CDOR,(device&31)+64);/*address device to talk*/


  do
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",1);
 while (!(inb(ISR2)&8)&&status==EWOULDBLOCK);

 outb(AUXMR,gts); /*Set to Standby (Controller)*/

 counter=0;
 fifopos=0;

  outb(CMDR,0x04); /*Tell TURBO488 to GO*/


 do {
   status1=inb(ISR3);
   if (!(status1&0x01)&&(status1&0x04)){
   status2=inb(STS2);
   inword=inw(FIFOB);
   *(unsigned*)(data+counter)=inword;
  /* printf ("Read:%c,%c\n",data[counter],data[counter+1]);*/
  counter+=2;
  }
 else {
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",4);
  }
 }
 while (!(status1&0x01)&&status==EWOULDBLOCK);
 if(!(status2 & 0x04)){ /*Only 1 byte came in on last 16 bit transfer*/
  data[counter-1]=0;
  counter--; }
  else
    data[counter]=0;
 outb(CMDR,0x08); /*send STOP*/

 do{
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",1);
   }
 while(!(inb(ISR3)&0x11)&&status==EWOULDBLOCK); /*wait for DONE and STOP*/
 outb(AUXMR,0x55);

 outb(IMR2,0x00); /*we have to enable DMA (0x30) for turbo488 to work*/
 outb(CMDR,0x20); /*soft reset turbo488*/
 outb(CMDR,0x10); /*reset fifos*/

/* do
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",1);
 while (!(inb(ISR1)&2));*/
 outb(AUXMR,tca); /* Regain full control of the bus*/


 do
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",1);
 while (!(inb(ISR2)&8)&&status==EWOULDBLOCK);
 outb(CDOR,63); /*unlisten*/

 do
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",1);
 while (!(inb(ISR2)&8)&&status==EWOULDBLOCK);

outb(AUXMR,0x5E); /*Clear SYNC*/
 outb (CDOR,95);/*untalk*/
 do
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",1);
 while (!(inb(ISR2)&8)&&status==EWOULDBLOCK);

 return(counter);


}


/* Return the status byte from device */
char spoll(unsigned char device)
 {
 int status=EWOULDBLOCK;
 unsigned int statusbyte;

 if (!(inb(ISR2)&8)) do
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",1);
 while (!(inb(ISR2)&8)&&status==EWOULDBLOCK);

 outb(CDOR,(device&31)+64);/*address device to talk*/

 if (!(inb(ISR2)&8)&&status==EWOULDBLOCK) do
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",1);
 while (!(inb(ISR2)&8)&&status==EWOULDBLOCK);

 outb (CDOR,32); /*Address controller (me) to listen*/

 if (!(inb(ISR2)&8)&&status==EWOULDBLOCK) do
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",1);
 while (!(inb(ISR2)&8)&&status==EWOULDBLOCK);
 outb(AUXMR,0x5E);
 outb (CDOR,0x18); /*Send SPE (serial poll enable)*/
 if (!(inb(ISR2)&8)&&status==EWOULDBLOCK) do
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",1);
 while (!(inb(ISR2)&8)&&status==EWOULDBLOCK);

  /*wait for bus to be synced*/
 if (!(inb(ISR0)&1)&&status==EWOULDBLOCK) do
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",1);
 while (!(inb(ISR0)&1)&&status==EWOULDBLOCK);

 outb(AUXMR,gts); /*Set to Standby (Controller)*/

 if (!(inb(ISR1)&1)&&status==EWOULDBLOCK) do
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",1);
 while (!(inb(ISR1)&1)&&status==EWOULDBLOCK);
 outb(AUXMR,0x5E);
 outb(AUXMR,tcs); /* Take control after next read*/
 statusbyte=inb(DIR);

 if (!(inb(ISR2)&8)&&status==EWOULDBLOCK) do
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",1);
 while (!(inb(ISR2)&8)&&status==EWOULDBLOCK);

 outb(CDOR,0x19); /*SPD (serial poll disable)*/

  /*wait for bus to be synced*/
 if (!(inb(ISR0)&1)&&status==EWOULDBLOCK) do
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",1);
 while (!(inb(ISR0)&1)&&status==EWOULDBLOCK);


 if (!(inb(ISR2)&8)&&status==EWOULDBLOCK) do
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",1);
 while (!(inb(ISR2)&8)&&status==EWOULDBLOCK);

outb(CDOR,95); /*untalk*/

 if (!(inb(ISR2)&8)&&status==EWOULDBLOCK) do
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",1);
 while (!(inb(ISR2)&8)&&status==EWOULDBLOCK);
 outb(AUXMR,0x5E);
 outb (CDOR,63);/*unlisten*/
 if (!(inb(ISR2)&8)&&status==EWOULDBLOCK) do
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",1);
 while (!(inb(ISR2)&8)&&status==EWOULDBLOCK);

  /*wait for bus to be synced*/
 if (!(inb(ISR0)&1)&&status==EWOULDBLOCK) do
   status=tsleep((caddr_t)&gpib_sc,GPIBPRI,"gpibpoll",1);
 while (!(inb(ISR0)&1)&&status==EWOULDBLOCK);


 return(statusbyte);


}


#ifdef JREMOD
struct cdevsw gp_cdevsw = 
	{ gpopen,	gpclose,	noread,		gpwrite,	/*44*/
	  gpioctl,	nostop,		nullreset,	nodevtotty,/* GPIB */
          seltrue,	nommap,		NULL };

static gp_devsw_installed = 0;

static void 	gp_devsw_install()
{
	dev_t descript;
	if( ! gp_devsw_installed ) {
		descript = makedev(CDEV_MAJOR,0);
		cdevsw_add(&descript,&gp_cdevsw,NULL);
#if defined(BDEV_MAJOR)
		descript = makedev(BDEV_MAJOR,0);
		bdevsw_add(&descript,&gp_bdevsw,NULL);
#endif /*BDEV_MAJOR*/
		gp_devsw_installed = 1;
	}
}
#endif /* JREMOD */

#endif /* NGPIB > 0 */
