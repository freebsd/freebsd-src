/*-
 * Copyright (c) 1992, 1993, 1995 Eugene W. Stark
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
 *	This product includes software developed by Eugene W. Stark.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY EUGENE W. STARK (THE AUTHOR) ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "tw.h"
#if NTW > 0

/*
 * Driver configuration parameters
 */

/*
 * Time for 1/2 of a power line cycle, in microseconds.
 * Change this to 10000 for 50Hz power.  Phil Sampson
 * (vk2jnt@gw.vk2jnt.ampr.org OR sampson@gidday.enet.dec.com)
 * reports that this works (at least in Australia) using a
 * TW7223 module (a local version of the TW523).
 */
#define HALFCYCLE 8333			/* 1/2 cycle = 8333us at 60Hz */

/*
 * Undefine the following if you don't have the high-resolution "microtime"
 * routines (leave defined for FreeBSD, which has them).
 */
#define HIRESTIME

/*
 * End of driver configuration parameters
 */

/*
 * FreeBSD Device Driver for X-10 POWERHOUSE (tm)
 * Two-Way Power Line Interface, Model #TW523
 *
 * written by Eugene W. Stark (stark@cs.sunysb.edu)
 * December 2, 1992
 *
 * NOTES:
 *
 * The TW523 is a carrier-current modem for home control/automation purposes.
 * It is made by:
 *
 * 	X-10 Inc.
 * 	185A LeGrand Ave.
 * 	Northvale, NJ 07647
 * 	USA
 * 	(201) 784-9700 or 1-800-526-0027
 *
 * 	X-10 Home Controls Inc.
 * 	1200 Aerowood Drive, Unit 20
 * 	Mississauga, Ontario
 *	(416) 624-4446 or 1-800-387-3346
 *
 * The TW523 is designed for communications using the X-10 protocol,
 * which is compatible with a number of home control systems, including
 * Radio Shack "Plug 'n Power(tm)" and Stanley "Lightmaker(tm)."
 * I bought my TW523 from:
 *
 *	Home Control Concepts
 *	9353-C Activity Road
 *	San Diego, CA 92126
 *	(619) 693-8887
 *
 * They supplied me with the TW523 (which has an RJ-11 four-wire modular
 * telephone connector), a modular cable, an RJ-11 to DB-25 connector with
 * internal wiring, documentation from X-10 on the TW523 (very good),
 * an instruction manual by Home Control Concepts (not very informative),
 * and a floppy disk containing binary object code of some demonstration/test
 * programs and of a C function library suitable for controlling the TW523
 * by an IBM PC under MS-DOS (not useful to me other than to verify that
 * the unit worked).  I suggest saving money and buying the bare TW523
 * rather than the TW523 development kit (what I bought), because if you
 * are running FreeBSD you don't really care about the DOS binaries.
 *
 * The interface to the TW-523 consists of four wires on the RJ-11 connector,
 * which are jumpered to somewhat more wires on the DB-25 connector, which
 * in turn is intended to plug into the PC parallel printer port.  I dismantled
 * the DB-25 connector to find out what they had done:
 *
 *	Signal		RJ-11 pin	DB-25 pin(s)	Parallel Port
 *	Transmit TX	  4 (Y)		2, 4, 6, 8	Data out
 *	Receive RX	  3 (G)		10, 14		-ACK, -AutoFeed
 *	Common		  2 (R)		25		Common
 *	Zero crossing	  1 (B)		17 or 12	-Select or +PaperEnd
 *
 * NOTE: In the original cable I have (which I am still using, May, 1997)
 * the Zero crossing signal goes to pin 17 (-Select) on the parallel port.
 * In retrospect, this doesn't make a whole lot of sense, given that the
 * -Select signal propagates the other direction.  Indeed, some people have
 * reported problems with this, and have had success using pin 12 (+PaperEnd)
 * instead.  This driver searches for the zero crossing signal on either
 * pin 17 or pin 12, so it should work with either cable configuration.
 * My suggestion would be to start by making the cable so that the zero
 * crossing signal goes to pin 12 on the parallel port.
 *
 * The zero crossing signal is used to synchronize transmission to the
 * zero crossings of the AC line, as detailed in the X-10 documentation.
 * It would be nice if one could generate interrupts with this signal,
 * however one needs interrupts on both the rising and falling edges,
 * and the -ACK signal to the parallel port interrupts only on the falling
 * edge, so it can't be done without additional hardware.
 *
 * In this driver, the transmit function is performed in a non-interrupt-driven
 * fashion, by polling the zero crossing signal to determine when a transition
 * has occurred.  This wastes CPU time during transmission, but it seems like
 * the best that can be done without additional hardware.  One problem with
 * the scheme is that preemption of the CPU during transmission can cause loss
 * of sync.  The driver tries to catch this, by noticing that a long delay
 * loop has somehow become foreshortened, and the transmission is aborted with
 * an error return.  It is up to the user level software to handle this
 * situation (most likely by retrying the transmission).
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/uio.h>
#include <sys/syslog.h>
#include <sys/select.h>
#ifdef DEVFS
#include <sys/devfsext.h>
#endif /*DEVFS*/

#define MIN(a,b)	((a)<(b)?(a):(b))

#ifdef HIRESTIME
#include <sys/time.h>
#endif /* HIRESTIME */

#include <i386/isa/isa_device.h>

/*
 * Transmission is done by calling write() to send three byte packets of data.
 * The first byte contains a four bit house code (0=A to 15=P).
 * The second byte contains five bit unit/key code (0=unit 1 to 15=unit 16,
 * 16=All Units Off to 31 = Status Request).  The third byte specifies
 * the number of times the packet is to be transmitted without any
 * gaps between successive transmissions.  Normally this is 2, as per
 * the X-10 documentation, but sometimes (e.g. for bright and dim codes)
 * it can be another value.  Each call to write can specify an arbitrary
 * number of data bytes.  An incomplete packet is buffered until a subsequent
 * call to write() provides data to complete it.  At most one packet will
 * actually be processed in any call to write().  Successive calls to write()
 * leave a three-cycle gap between transmissions, per the X-10 documentation.
 *
 * Reception is done using read().
 * The driver produces a series of three-character packets.
 * In each packet, the first character consists of flags,
 * the second character is a four bit house code (0-15),
 * and the third character is a five bit key/function code (0-31).
 * The flags are the following:
 */

#define TW_RCV_LOCAL	1  /* The packet arrived during a local transmission */
#define TW_RCV_ERROR	2  /* An invalid/corrupted packet was received */

/*
 * IBM PC parallel port definitions relevant to TW523
 */

#define tw_data 0			/* Data to tw523 (R/W) */

#define tw_status 1			/* Status of tw523 (R) */
#define	TWS_RDATA		0x40	/* tw523 receive data */
#define	TWS_OUT			0x20	/* pin 12, out of paper */

#define tw_control 2			/* Control tw523 (R/W) */
#define	TWC_SYNC		0x08	/* tw523 sync (pin 17) */
#define	TWC_ENA			0x10	/* tw523 interrupt enable */

/*
 * Miscellaneous defines
 */

#define	TWUNIT(dev)	(minor(dev))	/* Extract unit number from device */
#define TWPRI		(PZERO+8)	/* I don't know any better, so let's */
					/* use the same as the line printer */

static int twprobe(struct isa_device *idp);
static int twattach(struct isa_device *idp);

struct isa_driver twdriver = {
  twprobe, twattach, "tw"
};

static	d_open_t	twopen;
static	d_close_t	twclose;
static	d_read_t	twread;
static	d_write_t	twwrite;
static	d_select_t	twselect;

#define CDEV_MAJOR 19
static struct cdevsw tw_cdevsw = 
	{ twopen,	twclose,	twread,		twwrite,	/*19*/
	  noioc,	nullstop,	nullreset,	nodevtotty, /* tw */
	  twselect,	nommap,		nostrat,	"tw",	NULL,	-1 };

/*
 * Software control structure for TW523
 */

#define TWS_XMITTING	 1	/* Transmission in progress */
#define TWS_RCVING	 2	/* Reception in progress */
#define TWS_WANT	 4	/* A process wants received data */
#define TWS_OPEN	 8	/* Is it currently open? */

#define TW_SIZE		3*60	/* Enough for about 10 sec. of input */
#define TW_MIN_DELAY	1500	/* Ignore interrupts of lesser latency */

static struct tw_sc {
  u_int sc_port;		/* I/O Port */
  u_int sc_state;		/* Current software control state */
  struct selinfo sc_selp;	/* Information for select() */
  u_char sc_xphase;		/* Current state of sync (for transmitter) */
  u_char sc_rphase;		/* Current state of sync (for receiver) */
  u_char sc_flags;		/* Flags for current reception */
  short sc_rcount;		/* Number of bits received so far */
  int sc_bits;			/* Bits received so far */
  u_char sc_pkt[3];		/* Packet not yet transmitted */
  short sc_pktsize;		/* How many bytes in the packet? */
  u_char sc_buf[TW_SIZE];	/* We buffer our own input */
  int sc_nextin;		/* Next free slot in circular buffer */
  int sc_nextout;		/* First used slot in circular buffer */
#ifdef HIRESTIME
  int sc_xtimes[22];		/* Times for bits in current xmit packet */
  int sc_rtimes[22];		/* Times for bits in current rcv packet */
  int sc_no_rcv;		/* number of interrupts received */
#define SC_RCV_TIME_LEN	128
  int sc_rcv_time[SC_RCV_TIME_LEN]; /* usec time stamp on interrupt */
#endif /* HIRESTIME */
#ifdef	DEVFS
  void	*devfs_token;		/* store the devfs handle */
#endif
} tw_sc[NTW];

static int tw_zcport;		/* offset of port for zero crossing signal */
static int tw_zcmask;		/* mask for the zero crossing signal */

static void twdelay25(void);
static void twdelayn(int n);
static void twsetuptimes(int *a);
static int wait_for_zero(struct tw_sc *sc);
static int twputpkt(struct tw_sc *sc, u_char *p);
static int twgetbytes(struct tw_sc *sc, u_char *p, int cnt);
static timeout_t twabortrcv;
static int twsend(struct tw_sc *sc, int h, int k, int cnt);
static int next_zero(struct tw_sc *sc);
static int twchecktime(int target, int tol);
static void twdebugtimes(struct tw_sc *sc);

/*
 * Counter value for delay loop.
 * It is adjusted by twprobe so that the delay loop takes about 25us.
 */

#define TWDELAYCOUNT 161		/* Works on my 486DX/33 */
static int twdelaycount;

/*
 * Twdelay25 is used for very short delays of about 25us.
 * It is implemented with a calibrated delay loop, and should be
 * fairly accurate ... unless we are preempted by an interrupt.
 *
 * We use this to wait for zero crossings because the X-10 specs say we
 * are supposed to assert carrier within 25us when one happens.
 * I don't really believe we can do this, but the X-10 devices seem to be
 * fairly forgiving.
 */

static void twdelay25(void)
{
  int cnt;
  for(cnt = twdelaycount; cnt; cnt--);	/* Should take about 25us */
}

/*
 * Twdelayn is used to time the length of the 1ms carrier pulse.
 * This is not very critical, but if we have high-resolution time-of-day
 * we check it every apparent 200us to make sure we don't get too far off
 * if we happen to be interrupted during the delay.
 */

static void twdelayn(int n)
{
#ifdef HIRESTIME
  int t, d;
  struct timeval tv;
  microtime(&tv);
  t = tv.tv_usec;
  t += n;
#endif /* HIRESTIME */
  while(n > 0) {
    twdelay25();
    n -= 25;
#ifdef HIRESTIME
    if((n & 0x7) == 0) {
      microtime(&tv);
      d = tv.tv_usec - t;
      if(d >= 0 && d < 1000000) return;
    }
#endif /* HIRESTIME */
  }
}

static int twprobe(idp)
     struct isa_device *idp;
{
  struct tw_sc sc;
  int d;
  int tries;

  sc.sc_port = idp->id_iobase;
  /* Search for the zero crossing signal at ports, bit combinations. */
  tw_zcport = tw_control;
  tw_zcmask = TWC_SYNC;
  sc.sc_xphase = inb(idp->id_iobase + tw_zcport) & tw_zcmask;
  if(wait_for_zero(&sc) < 0) {
    tw_zcport = tw_status;
    tw_zcmask = TWS_OUT;
    sc.sc_xphase = inb(idp->id_iobase + tw_zcport) & tw_zcmask;
  }
  if(wait_for_zero(&sc) < 0)
    return(0);
  /*
   * Iteratively check the timing of a few sync transitions, and adjust
   * the loop delay counter, if necessary, to bring the timing reported
   * by wait_for_zero() close to HALFCYCLE.  Give up if anything
   * ridiculous happens.
   */
  if(twdelaycount == 0) {  /* Only adjust timing for first unit */
    twdelaycount = TWDELAYCOUNT;
    for(tries = 0; tries < 10; tries++) {
      sc.sc_xphase = inb(idp->id_iobase + tw_zcport) & tw_zcmask;
      if(wait_for_zero(&sc) >= 0) {
	d = wait_for_zero(&sc);
	if(d <= HALFCYCLE/100 || d >= HALFCYCLE*100) {
	  twdelaycount = 0;
	  return(0);
	}
	twdelaycount = (twdelaycount * d)/HALFCYCLE;
      }
    }
  }
  /*
   * Now do a final check, just to make sure
   */
  sc.sc_xphase = inb(idp->id_iobase + tw_zcport) & tw_zcmask;
  if(wait_for_zero(&sc) >= 0) {
    d = wait_for_zero(&sc);
    if(d <= (HALFCYCLE * 110)/100 && d >= (HALFCYCLE * 90)/100) return(8);
  }
  return(0);
}

static int twattach(idp)
	struct isa_device *idp;
{
  struct tw_sc *sc;
  int	unit;

  sc = &tw_sc[unit = idp->id_unit];
  sc->sc_port = idp->id_iobase;
  sc->sc_state = 0;
  sc->sc_rcount = 0;

#ifdef DEVFS
	sc->devfs_token = 
		devfs_add_devswf(&tw_cdevsw, unit, DV_CHR, 0, 0, 
				 0600, "tw%d", unit);
#endif

  return (1);
}

int twopen(dev, flag, mode, p)
     dev_t dev;
     int flag;
     int mode;
     struct proc *p;
{
  struct tw_sc *sc = &tw_sc[TWUNIT(dev)];
  int s;
  int port;

  s = spltty();
  if(sc->sc_state == 0) {
    sc->sc_state = TWS_OPEN;
    sc->sc_nextin = sc->sc_nextout = 0;
    sc->sc_pktsize = 0;
    outb(sc->sc_port+tw_control, TWC_ENA);
  }
  splx(s);
  return(0);
}

int twclose(dev, flag, mode, p)
     dev_t dev;
     int flag;
     int mode;
     struct proc *p;
{
  struct tw_sc *sc = &tw_sc[TWUNIT(dev)];
  int s;
  int port = sc->sc_port;

  s = spltty();
  sc->sc_state = 0;
  outb(sc->sc_port+tw_control, 0);
  splx(s);
  return(0);
}

int twread(dev, uio, ioflag)
     dev_t dev;
     struct uio *uio;
     int ioflag;
{
  u_char buf[3];
  struct tw_sc *sc = &tw_sc[TWUNIT(dev)];
  int error, cnt, s;

  s = spltty();
  cnt = MIN(uio->uio_resid, 3);
  if((error = twgetbytes(sc, buf, cnt)) == 0) {
    error = uiomove(buf, cnt, uio);
  }
  splx(s);
  return(error);
}

int twwrite(dev, uio, ioflag)
     dev_t dev;
     struct uio *uio;
     int ioflag;
{
  struct tw_sc *sc;
  int house, key, reps;
  int s, error;
  int cnt;

  sc = &tw_sc[TWUNIT(dev)];
  /*
   * Note: Although I had intended to allow concurrent transmitters,
   * there is a potential problem here if two processes both write
   * into the sc_pkt buffer at the same time.  The following code
   * is an additional critical section that needs to be synchronized.
   */
  s = spltty();
  cnt = MIN(3 - sc->sc_pktsize, uio->uio_resid);
  if(error = uiomove(&(sc->sc_pkt[sc->sc_pktsize]), cnt, uio)) {
    splx(s);
    return(error);
  }
  sc->sc_pktsize += cnt;
  if(sc->sc_pktsize < 3) {  /* Only transmit 3-byte packets */
    splx(s);
    return(0);
  }
  sc->sc_pktsize = 0;
  /*
   * Collect house code, key code, and rep count, and check for sanity.
   */
  house = sc->sc_pkt[0];
  key = sc->sc_pkt[1];
  reps = sc->sc_pkt[2];
  if(house >= 16 || key >= 32) {
    splx(s);
    return(ENODEV);
  }
  /*
   * Synchronize with the receiver operating in the bottom half, and
   * also with concurrent transmitters.
   * We don't want to interfere with a packet currently being received,
   * and we would like the receiver to recognize when a packet has
   * originated locally.
   */
  while(sc->sc_state & (TWS_RCVING | TWS_XMITTING)) {
    if(error = tsleep((caddr_t)sc, TWPRI|PCATCH, "twwrite", 0)) {
      splx(s);
      return(error);
    }
  }
  sc->sc_state |= TWS_XMITTING;
  /*
   * Everything looks OK, let's do the transmission.
   */
  splx(s);  /* Enable interrupts because this takes a LONG time */
  error = twsend(sc, house, key, reps);
  s = spltty();
  sc->sc_state &= ~TWS_XMITTING;
  wakeup((caddr_t)sc);
  splx(s);
  if(error) return(EIO);
  else return(0);
}

/*
 * Determine if there is data available for reading
 */

int twselect(dev, rw, p)
     dev_t dev;
     int rw;
     struct proc *p;
{
  struct tw_sc *sc;
  struct proc *pp;
  int s, i;

  sc = &tw_sc[TWUNIT(dev)];
  s = spltty();
  if(sc->sc_nextin != sc->sc_nextout) {
    splx(s);
    return(1);
  }
  selrecord(p, &sc->sc_selp);
  splx(s);
  return(0);
}

/*
 * X-10 Protocol
 */

#define X10_START_LENGTH 4
static char X10_START[] = { 1, 1, 1, 0 };

/*
 * Each bit of the 4-bit house code and 5-bit key code
 * is transmitted twice, once in true form, and then in
 * complemented form.  This is already taken into account
 * in the following tables.
 */

#define X10_HOUSE_LENGTH 8
static char X10_HOUSE[16][8] = {
	0, 1, 1, 0, 1, 0, 0, 1,		/* A = 0110 */
	1, 0, 1, 0, 1, 0, 0, 1,		/* B = 1110 */
	0, 1, 0, 1, 1, 0, 0, 1,		/* C = 0010 */
	1, 0, 0, 1, 1, 0, 0, 1,		/* D = 1010 */
	0, 1, 0, 1, 0, 1, 1, 0,		/* E = 0001 */
	1, 0, 0, 1, 0, 1, 1, 0,		/* F = 1001 */
	0, 1, 1, 0, 0, 1, 1, 0,		/* G = 0101 */
	1, 0, 1, 0, 0, 1, 1, 0,		/* H = 1101 */
	0, 1, 1, 0, 1, 0, 1, 0,		/* I = 0111 */
	1, 0, 1, 0, 1, 0, 1, 0,		/* J = 1111 */
	0, 1, 0, 1, 1, 0, 1, 0,		/* K = 0011 */
	1, 0, 0, 1, 1, 0, 1, 0,		/* L = 1011 */
	0, 1, 0, 1, 0, 1, 0, 1,		/* M = 0000 */
	1, 0, 0, 1, 0, 1, 0, 1,		/* N = 1000 */
	0, 1, 1, 0, 0, 1, 0, 1,		/* O = 0100 */
	1, 0, 1, 0, 0, 1, 0, 1		/* P = 1100 */
};

#define X10_KEY_LENGTH 10
static char X10_KEY[32][10] = {
	0, 1, 1, 0, 1, 0, 0, 1, 0, 1,	/* 01100 => 1 */
	1, 0, 1, 0, 1, 0, 0, 1, 0, 1,	/* 11100 => 2 */
	0, 1, 0, 1, 1, 0, 0, 1, 0, 1,	/* 00100 => 3 */
	1, 0, 0, 1, 1, 0, 0, 1, 0, 1,	/* 10100 => 4 */
	0, 1, 0, 1, 0, 1, 1, 0, 0, 1,	/* 00010 => 5 */
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1,	/* 10010 => 6 */
	0, 1, 1, 0, 0, 1, 1, 0, 0, 1,	/* 01010 => 7 */
	1, 0, 1, 0, 0, 1, 1, 0, 0, 1,	/* 11010 => 8 */
	0, 1, 1, 0, 1, 0, 1, 0, 0, 1,	/* 01110 => 9 */
	1, 0, 1, 0, 1, 0, 1, 0, 0, 1,	/* 11110 => 10 */
	0, 1, 0, 1, 1, 0, 1, 0, 0, 1,	/* 00110 => 11 */
	1, 0, 0, 1, 1, 0, 1, 0, 0, 1,	/* 10110 => 12 */
	0, 1, 0, 1, 0, 1, 0, 1, 0, 1,	/* 00000 => 13 */
	1, 0, 0, 1, 0, 1, 0, 1, 0, 1,	/* 10000 => 14 */
	0, 1, 1, 0, 0, 1, 0, 1, 0, 1,	/* 01000 => 15 */
	1, 0, 1, 0, 0, 1, 0, 1, 0, 1,	/* 11000 => 16 */
	0, 1, 0, 1, 0, 1, 0, 1, 1, 0,	/* 00001 => All Units Off */
	0, 1, 0, 1, 0, 1, 1, 0, 1, 0,	/* 00011 => All Units On */
	0, 1, 0, 1, 1, 0, 0, 1, 1, 0,	/* 00101 => On */
	0, 1, 0, 1, 1, 0, 1, 0, 1, 0,	/* 00111 => Off */
	0, 1, 1, 0, 0, 1, 0, 1, 1, 0,	/* 01001 => Dim */
	0, 1, 1, 0, 0, 1, 1, 0, 1, 0,	/* 01011 => Bright */
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0,	/* 01101 => All LIGHTS Off */
	0, 1, 1, 0, 1, 0, 1, 0, 1, 0,	/* 01111 => Extended Code */
	1, 0, 0, 1, 0, 1, 0, 1, 1, 0,	/* 10001 => Hail Request */
	1, 0, 0, 1, 0, 1, 1, 0, 1, 0,	/* 10011 => Hail Acknowledge */
	1, 0, 0, 1, 1, 0, 0, 1, 1, 0,	/* 10101 => Preset Dim 0 */
	1, 0, 0, 1, 1, 0, 1, 0, 1, 0,	/* 10111 => Preset Dim 1 */
	1, 0, 1, 0, 0, 1, 0, 1, 0, 1,	/* 11000 => Extended Data (analog) */
	1, 0, 1, 0, 0, 1, 1, 0, 1, 0,	/* 11011 => Status = on */
	1, 0, 1, 0, 1, 0, 0, 1, 1, 0,	/* 11101 => Status = off */
	1, 0, 1, 0, 1, 0, 1, 0, 1, 0	/* 11111 => Status request */
};

/*
 * Tables for mapping received X-10 code back to house/key number.
 */

static short X10_HOUSE_INV[16] = {
      12,  4,  2, 10, 14,  6,  0,  8,
      13,  5,  3, 11, 15,  7,  1,  9
};

static short X10_KEY_INV[32] = { 
      12, 16,  4, 17,  2, 18, 10, 19,
      14, 20,  6, 21,  0, 22,  8, 23,
      13, 24,  5, 25,  3, 26, 11, 27,
      15, 28,  7, 29,  1, 30,  9, 31
};

static char *X10_KEY_LABEL[32] = {
 "1",
 "2",
 "3",
 "4",
 "5",
 "6",
 "7",
 "8",
 "9",
 "10",
 "11",
 "12",
 "13",
 "14",
 "15",
 "16",
 "All Units Off",
 "All Units On",
 "On",
 "Off",
 "Dim",
 "Bright",
 "All LIGHTS Off",
 "Extended Code",
 "Hail Request",
 "Hail Acknowledge",
 "Preset Dim 0",
 "Preset Dim 1",
 "Extended Data (analog)",
 "Status = on",
 "Status = off",
 "Status request"
};
/*
 * Transmit a packet containing house code h and key code k
 */

#define TWRETRY		10		/* Try 10 times to sync with AC line */

static int twsend(sc, h, k, cnt)
struct tw_sc *sc;
int h, k, cnt;
{
  int i;
  int port = sc->sc_port;

  /*
   * Make sure we get a reliable sync with a power line zero crossing
   */
  for(i = 0; i < TWRETRY; i++) {
    if(wait_for_zero(sc) > 100) goto insync;
  }
  log(LOG_ERR, "TWXMIT: failed to sync.\n");
  return(-1);

 insync:
  /*
   * Be sure to leave 3 cycles space between transmissions
   */
  for(i = 6; i > 0; i--)
	if(next_zero(sc) < 0) return(-1);
  /*
   * The packet is transmitted cnt times, with no gaps.
   */
  while(cnt--) {
    /*
     * Transmit the start code
     */
    for(i = 0; i < X10_START_LENGTH; i++) {
      outb(port+tw_data, X10_START[i] ? 0xff : 0x00);  /* Waste no time! */
#ifdef HIRESTIME
      if(i == 0) twsetuptimes(sc->sc_xtimes);
      if(twchecktime(sc->sc_xtimes[i], HALFCYCLE/20) == 0) {
	outb(port+tw_data, 0);
	return(-1);
      }
#endif /* HIRESTIME */
      twdelayn(1000);	/* 1ms pulse width */
      outb(port+tw_data, 0);
      if(next_zero(sc) < 0) return(-1);
    }
    /*
     * Transmit the house code
     */
    for(i = 0; i < X10_HOUSE_LENGTH; i++) {
      outb(port+tw_data, X10_HOUSE[h][i] ? 0xff : 0x00);  /* Waste no time! */
#ifdef HIRESTIME
      if(twchecktime(sc->sc_xtimes[i+X10_START_LENGTH], HALFCYCLE/20) == 0) {
	outb(port+tw_data, 0);
	return(-1);
      }
#endif /* HIRESTIME */
      twdelayn(1000);	/* 1ms pulse width */
      outb(port+tw_data, 0);
      if(next_zero(sc) < 0) return(-1);
    }
    /*
     * Transmit the unit/key code
     */
    for(i = 0; i < X10_KEY_LENGTH; i++) {
      outb(port+tw_data, X10_KEY[k][i] ? 0xff : 0x00);
#ifdef HIRESTIME
      if(twchecktime(sc->sc_xtimes[i+X10_START_LENGTH+X10_HOUSE_LENGTH],
			HALFCYCLE/20) == 0) {
	outb(port+tw_data, 0);
	return(-1);
      }
#endif /* HIRESTIME */
      twdelayn(1000);	/* 1ms pulse width */
      outb(port+tw_data, 0);
      if(next_zero(sc) < 0) return(-1);
    }
  }
  return(0);
}

/*
 * Waste CPU cycles to get in sync with a power line zero crossing.
 * The value returned is roughly how many microseconds we wasted before
 * seeing the transition.  To avoid wasting time forever, we give up after
 * waiting patiently for 1/4 sec (15 power line cycles at 60 Hz),
 * which is more than the 11 cycles it takes to transmit a full
 * X-10 packet.
 */

static int wait_for_zero(sc)
struct tw_sc *sc;
{
  int i, old, new, max;
  int port = sc->sc_port + tw_zcport;

  old = sc->sc_xphase;
  max = 10000;		/* 10000 * 25us = 0.25 sec */
  i = 0;
  while(max--) {
    new = inb(port) & tw_zcmask;
    if(new != old) {
      sc->sc_xphase = new;
      return(i*25);
    }
    i++;
    twdelay25();
  }
  return(-1);
}

/*
 * Wait for the next zero crossing transition, and if we don't have
 * high-resolution time-of-day, check to see that the zero crossing
 * appears to be arriving on schedule.
 * We expect to be waiting almost a full half-cycle (8.333ms-1ms = 7.333ms).
 * If we don't seem to wait very long, something is wrong (like we got
 * preempted!) and we should abort the transmission because
 * there's no telling how long it's really been since the
 * last bit was transmitted.
 */

static int next_zero(sc)
struct tw_sc *sc;
{
  int d;
#ifdef HIRESTIME
  if((d = wait_for_zero(sc)) < 0) {
#else
  if((d = wait_for_zero(sc)) < 6000 || d > 8500) {
	/* No less than 6.0ms, no more than 8.5ms */
#endif /* HIRESTIME */
    log(LOG_ERR, "TWXMIT framing error: %d\n", d);
    return(-1);
  }
  return(0);
}

/*
 * Put a three-byte packet into the circular buffer
 * Should be called at priority spltty()
 */

static int twputpkt(sc, p)
struct tw_sc *sc;
u_char *p;
{
  int i, next;

  for(i = 0; i < 3; i++) {
    next = sc->sc_nextin+1;
    if(next >= TW_SIZE) next = 0;
    if(next == sc->sc_nextout) {  /* Buffer full */
/*
      log(LOG_ERR, "TWRCV: Buffer overrun\n");
 */
      return(1);
    }
    sc->sc_buf[sc->sc_nextin] = *p++;
    sc->sc_nextin = next;
  }
  if(sc->sc_state & TWS_WANT) {
    sc->sc_state &= ~TWS_WANT;
    wakeup((caddr_t)(&sc->sc_buf));
  }
  selwakeup(&sc->sc_selp);
  return(0);
}

/*
 * Get bytes from the circular buffer
 * Should be called at priority spltty()
 */

static int twgetbytes(sc, p, cnt)
struct tw_sc *sc;
u_char *p;
int cnt;
{
  int error;

  while(cnt--) {
    while(sc->sc_nextin == sc->sc_nextout) {  /* Buffer empty */
      sc->sc_state |= TWS_WANT;
      if(error = tsleep((caddr_t)(&sc->sc_buf), TWPRI|PCATCH, "twread", 0)) {
	return(error);
      }
    }
    *p++ = sc->sc_buf[sc->sc_nextout++];
    if(sc->sc_nextout >= TW_SIZE) sc->sc_nextout = 0;
  }
  return(0);
}

/*
 * Abort reception that has failed to complete in the required time.
 */

static void
twabortrcv(arg)
	void *arg;
{
  struct tw_sc *sc = arg;
  int s;
  u_char pkt[3];

  s = spltty();
  sc->sc_state &= ~TWS_RCVING;
  /* simply ignore single isolated interrupts. */
  if (sc->sc_no_rcv > 1) {
      sc->sc_flags |= TW_RCV_ERROR;
      pkt[0] = sc->sc_flags;
      pkt[1] = pkt[2] = 0;
      twputpkt(sc, pkt);
      log(LOG_ERR, "TWRCV: aborting (%x, %d)\n", sc->sc_bits, sc->sc_rcount);
      twdebugtimes(sc);
  }
  wakeup((caddr_t)sc);
  splx(s);
}

static int
tw_is_within(int value, int expected, int tolerance)
{
  int diff;
  diff = value - expected;
  if (diff < 0)
    diff *= -1;
  if (diff < tolerance)
    return 1;
  return 0;
}

/*
 * This routine handles interrupts that occur when there is a falling
 * transition on the RX input.  There isn't going to be a transition
 * on every bit (some are zero), but if we are smart and keep track of
 * how long it's been since the last interrupt (via the zero crossing
 * detect line and/or high-resolution time-of-day routine), we can
 * reconstruct the transmission without having to poll.
 */

void twintr(unit)
int unit;
{
  struct tw_sc *sc = &tw_sc[unit];
  int port;
  int newphase;
  u_char pkt[3];
  int delay = 0;
  struct timeval tv;

  port = sc->sc_port;
  /*
   * Ignore any interrupts that occur if the device is not open.
   */
  if(sc->sc_state == 0) return;
  newphase = inb(port + tw_control) & TWC_SYNC;
  microtime(&tv);

  /*
   * NEW PACKET:
   * If we aren't currently receiving a packet, set up a new packet
   * and put in the first "1" bit that has just arrived.
   * Arrange for the reception to be aborted if too much time goes by.
   */
  if((sc->sc_state & TWS_RCVING) == 0) {
#ifdef HIRESTIME
    twsetuptimes(sc->sc_rtimes);
#endif /* HIRESTIME */
    sc->sc_state |= TWS_RCVING;
    sc->sc_rcount = 1;
    if(sc->sc_state & TWS_XMITTING) sc->sc_flags = TW_RCV_LOCAL;
    else sc->sc_flags = 0;
    sc->sc_bits = 0;
    sc->sc_rphase = newphase;
    /* 3 cycles of silence = 3/60 = 1/20 = 50 msec */
    timeout(twabortrcv, (caddr_t)sc, hz/20);
    sc->sc_rcv_time[0] = tv.tv_usec;
    sc->sc_no_rcv = 1;
    return;
  }
  untimeout((timeout_func_t)twabortrcv, (caddr_t)sc);
  timeout((timeout_func_t)twabortrcv, (caddr_t)sc, hz/20);
  newphase = inb(port + tw_zcport) & tw_zcmask;

  /* enforce a minimum delay since the last interrupt */
  delay = tv.tv_usec - sc->sc_rcv_time[sc->sc_no_rcv - 1];
  if (delay < 0)
    delay += 1000000;
  if (delay < TW_MIN_DELAY)
    return;

  sc->sc_rcv_time[sc->sc_no_rcv] = tv.tv_usec;
  if (sc->sc_rcv_time[sc->sc_no_rcv] < sc->sc_rcv_time[0])
    sc->sc_rcv_time[sc->sc_no_rcv] += 1000000;
  sc->sc_no_rcv++;

  /*
   * START CODE:
   * The second and third bits are a special case.
   */
  if (sc->sc_rcount < 3) {
    if (
#ifdef HIRESTIME
	tw_is_within(delay, HALFCYCLE, HALFCYCLE / 6)
#else
	newphase != sc->sc_rphase
#endif
	) {
      sc->sc_rcount++;
    } else {
      /*
       * Invalid start code -- abort reception.
       */
      sc->sc_state &= ~TWS_RCVING;
      sc->sc_flags |= TW_RCV_ERROR;
      untimeout(twabortrcv, (caddr_t)sc);
      log(LOG_ERR, "TWRCV: Invalid start code\n");
      twdebugtimes(sc);
      sc->sc_no_rcv = 0;
      return;
    }
    if(sc->sc_rcount == 3) {
      /*
       * We've gotten three "1" bits in a row.  The start code
       * is really 1110, but this might be followed by a zero
       * bit from the house code, so if we wait any longer we
       * might be confused about the first house code bit.
       * So, we guess that the start code is correct and insert
       * the trailing zero without actually having seen it.
       * We don't change sc_rphase in this case, because two
       * bit arrivals in a row preserve parity.
       */
      sc->sc_rcount++;
      return;
    }
    /*
     * Update sc_rphase to the current phase before returning.
     */
    sc->sc_rphase = newphase;
    return;
  }
  /*
   * GENERAL CASE:
   * Now figure out what the current bit is that just arrived.
   * The X-10 protocol transmits each data bit twice: once in
   * true form and once in complemented form on the next half
   * cycle.  So, there will be at least one interrupt per bit.
   * By comparing the phase we see at the time of the interrupt
   * with the saved sc_rphase, we can tell on which half cycle
   * the interrupt occrred.  This assumes, of course, that the
   * packet is well-formed.  We do the best we can at trying to
   * catch errors by aborting if too much time has gone by, and
   * by tossing out a packet if too many bits arrive, but the
   * whole scheme is probably not as robust as if we had a nice
   * interrupt on every half cycle of the power line.
   * If we have high-resolution time-of-day routines, then we
   * can do a bit more sanity checking.
   */

  /*
   * A complete packet is 22 half cycles.
   */
  if(sc->sc_rcount <= 20) {
#ifdef HIRESTIME
    int bit = 0, last_bit;
    if (sc->sc_rcount == 4)
      last_bit = 1;		/* Start (1110) ends in 10, a 'one' code. */
    else
      last_bit = sc->sc_bits & 0x1;
    if (   (   (last_bit == 1)
	    && (tw_is_within(delay, HALFCYCLE * 2, HALFCYCLE / 6)))
	|| (   (last_bit == 0)
	    && (tw_is_within(delay, HALFCYCLE * 1, HALFCYCLE / 6))))
      bit = 1;
    else if (   (   (last_bit == 1)
		 && (tw_is_within(delay, HALFCYCLE * 3, HALFCYCLE / 6)))
	     || (   (last_bit == 0)
		 && (tw_is_within(delay, HALFCYCLE * 2, HALFCYCLE / 6))))
      bit = 0;
    else {
      sc->sc_flags |= TW_RCV_ERROR;
      log(LOG_ERR, "TWRCV: %d cycle after %d bit, delay %d%%\n",
	  sc->sc_rcount, last_bit, 100 * delay / HALFCYCLE);
    }
    sc->sc_bits = (sc->sc_bits << 1) | bit;
#else
    sc->sc_bits = (sc->sc_bits << 1)
      | ((newphase == sc->sc_rphase) ? 0x0 : 0x1);
#endif /* HIRESTIME */
    sc->sc_rcount += 2;
  }
  if(sc->sc_rcount >= 22 || sc->sc_flags & TW_RCV_ERROR) {
    if(sc->sc_rcount != 22) {
      sc->sc_flags |= TW_RCV_ERROR;
      pkt[0] = sc->sc_flags;
      pkt[1] = pkt[2] = 0;
    } else {
      pkt[0] = sc->sc_flags;
      pkt[1] = X10_HOUSE_INV[(sc->sc_bits & 0x1e0) >> 5];
      pkt[2] = X10_KEY_INV[sc->sc_bits & 0x1f];
    }
    sc->sc_state &= ~TWS_RCVING;
    twputpkt(sc, pkt);
    untimeout(twabortrcv, (caddr_t)sc);
    if(sc->sc_flags & TW_RCV_ERROR) {
      log(LOG_ERR, "TWRCV: invalid packet: (%d, %x) %c %d\n",
	  sc->sc_rcount, sc->sc_bits, 'A' + pkt[1], X10_KEY_LABEL[pkt[2]]);
      twdebugtimes(sc);
    } else {
/*      log(LOG_ERR, "TWRCV: valid packet: (%d, %x) %c %s\n",
	  sc->sc_rcount, sc->sc_bits, 'A' + pkt[1], X10_KEY_LABEL[pkt[2]]); */
    }
    sc->sc_rcount = 0;
    wakeup((caddr_t)sc);
  }
}

static void twdebugtimes(struct tw_sc *sc)
{
    int i;
    for (i = 0; (i < sc->sc_no_rcv) && (i < SC_RCV_TIME_LEN); i++)
	log(LOG_ERR, "TWRCV: interrupt %2d: %d\t%d%%\n", i, sc->sc_rcv_time[i],
	    (sc->sc_rcv_time[i] - sc->sc_rcv_time[(i?i-1:0)])*100/HALFCYCLE);
}

#ifdef HIRESTIME
/*
 * Initialize an array of 22 times, starting from the current
 * microtime and continuing for the next 21 half cycles.
 * We use the times as a reference to make sure transmission
 * or reception is on schedule.
 */

static void twsetuptimes(int *a)
{
  struct timeval tv;
  int i, t;

  microtime(&tv);
  t = tv.tv_usec;
  for(i = 0; i < 22; i++) {
    *a++ = t;
    t += HALFCYCLE;
    if(t >= 1000000) t -= 1000000;
  }
}

/*
 * Check the current time against a slot in a previously set up
 * timing array, and make sure that it looks like we are still
 * on schedule.
 */

static int twchecktime(int target, int tol)
{
  struct timeval tv;
  int t, d;

  microtime(&tv);
  t = tv.tv_usec;
  d = (target - t) >= 0 ? (target - t) : (t - target);
  if(d > 500000) d = 1000000-d;
  if(d <= tol && d >= -tol) {
    return(1);
  } else {
    return(0);
  }
}
#endif /* HIRESTIME */


static tw_devsw_installed = 0;

static void 	tw_drvinit(void *unused)
{
	dev_t dev;

	if( ! tw_devsw_installed ) {
		dev = makedev(CDEV_MAJOR, 0);
		cdevsw_add(&dev,&tw_cdevsw, NULL);
		tw_devsw_installed = 1;
    	}
}

SYSINIT(twdev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,tw_drvinit,NULL)


#endif NTW
