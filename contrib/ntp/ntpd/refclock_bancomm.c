/* refclock_bancomm.c - clock driver for the  Datum/Bancomm bc635VME 
 * Time and Frequency Processor. It requires the BANCOMM bc635VME/
 * bc350VXI Time and Frequency Processor Module Driver for SunOS4.x 
 * and SunOS5.x UNIX Systems. It has been tested on a UltraSparc 
 * IIi-cEngine running Solaris 2.6.
 * 
 * Author(s): 	Ganesh Ramasivan & Gary Cliff, Computing Devices Canada,
 *		Ottawa, Canada
 *
 * Date: 	July 1999
 *
 * Note(s):	The refclock type has been defined as 16.
 *
 *		This program has been modelled after the Bancomm driver
 *		originally written by R. Schmidt of Time Service, U.S. 
 *		Naval Observatory for a HP-UX machine. Since the original
 *		authors no longer plan to maintain this code, all 
 *		references to the HP-UX vme2 driver subsystem bave been
 *		removed. Functions vme_report_event(), vme_receive(), 
 *		vme_control() and vme_buginfo() have been deleted because
 *		they are no longer being used.
 *
 *		The time on the bc635 TFP must be set to GMT due to the 
 *		fact that NTP makes use of GMT for all its calculations.
 *
 *		Installation of the Datum/Bancomm driver creates the 
 *		device file /dev/btfp0 
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#if defined(REFCLOCK) && defined(CLOCK_BANC) 

#include "ntpd.h"
#include "ntp_io.h"
#include "ntp_refclock.h"
#include "ntp_unixtime.h"
#include "ntp_stdlib.h"

#include <stdio.h>
#include <syslog.h>
#include <ctype.h>

/*  STUFF BY RES */
struct btfp_time                /* Structure for reading 5 time words   */
                                /* in one ioctl(2) operation.           */
{
	unsigned short btfp_time[5];  /* Time words 0,1,2,3, and 4. (16bit)*/
};

/* SunOS5 ioctl commands definitions.*/
#define BTFPIOC            ( 'b'<< 8 )
#define IOCIO( l, n )      ( BTFPIOC | n )
#define IOCIOR( l, n, s )  ( BTFPIOC | n )
#define IOCIORN( l, n, s ) ( BTFPIOC | n )
#define IOCIOWN( l, n, s ) ( BTFPIOC | n )

/***** Simple ioctl commands *****/
#define RUNLOCK     	IOCIOR(b, 19, int )  /* Release Capture Lockout */
#define RCR0      	IOCIOR(b, 22, int )  /* Read control register zero.*/
#define	WCR0		IOCIOWN(b, 23, int)	     /* Write control register zero*/

/***** Compound ioctl commands *****/

/* Read all 5 time words in one call.   */
#define READTIME	IOCIORN(b, 32, sizeof( struct btfp_time ))
#define VMEFD "/dev/btfp0"

struct vmedate {               /* structure returned by get_vmetime.c */
	unsigned short year;
	unsigned short day;
	unsigned short hr;
	unsigned short mn;
	unsigned short sec;
	unsigned long frac;
	unsigned short status;
};

/* END OF STUFF FROM RES */

/*
 * VME interface parameters. 
 */
#define VMEPRECISION    (-21)   /* precision assumed (1 us) */
#define USNOREFID       "BTFP"  /* or whatever */
#define VMEREFID        "BTFP"  /* reference id */
#define VMEDESCRIPTION  "Bancomm bc635 TFP" /* who we are */
#define VMEHSREFID      0x7f7f1000 /* 127.127.16.00 refid hi strata */
/* clock type 16 is used here  */
#define GMT           	0       /* hour offset from Greenwich */

/*
 * Imported from ntp_timer module
 */
extern u_long current_time;     /* current time(s) */

/*
 * Imported from ntpd module
 */
extern int debug;               /* global debug flag */

/*
 * VME unit control structure.
 * Changes made to vmeunit structure. Most members are now available in the 
 * new refclockproc structure in ntp_refclock.h - 07/99 - Ganesh Ramasivan
 */
struct vmeunit {
	struct vmedate vmedata; /* data returned from vme read */
	u_long lasttime;        /* last time clock heard from */
};

/*
 * Function prototypes
 */
static  void    vme_init        (void);
static  int     vme_start       (int, struct peer *);
static  void    vme_shutdown    (int, struct peer *);
static  void    vme_receive     (struct recvbuf *);
static  void    vme_poll        (int unit, struct peer *);
struct vmedate *get_datumtime(struct vmedate *);

/*
 * Transfer vector
 */
struct  refclock refclock_bancomm = {
	vme_start, 		/* start up driver */
	vme_shutdown,		/* shut down driver */
	vme_poll,		/* transmit poll message */
	noentry,		/* not used (old vme_control) */
	noentry,		/* initialize driver */ 
	noentry,		/* not used (old vme_buginfo) */ 
	NOFLAGS			/* not used */
};

int fd_vme;  /* file descriptor for ioctls */
int regvalue;


/*
 * vme_start - open the VME device and initialize data for processing
 */
static int
vme_start(
	int unit,
	struct peer *peer
	)
{
	register struct vmeunit *vme;
	struct refclockproc *pp;
	int dummy;
	char vmedev[20];

	/*
	 * Open VME device
	 */
#ifdef DEBUG

	printf("Opening DATUM VME DEVICE \n");
#endif
	if ( (fd_vme = open(VMEFD, O_RDWR)) < 0) {
		msyslog(LOG_ERR, "vme_start: failed open of %s: %m", vmedev);
		return (0);
	}
	else  { /* Release capture lockout in case it was set from before. */
		if( ioctl( fd_vme, RUNLOCK, &dummy ) )
		    msyslog(LOG_ERR, "vme_start: RUNLOCK failed %m");

		regvalue = 0; /* More esoteric stuff to do... */
		if( ioctl( fd_vme, WCR0, &regvalue ) )
		    msyslog(LOG_ERR, "vme_start: WCR0 failed %m");
	}

	/*
	 * Allocate unit structure
	 */
	vme = (struct vmeunit *)emalloc(sizeof(struct vmeunit));
	bzero((char *)vme, sizeof(struct vmeunit));


	/*
	 * Set up the structures
	 */
	pp = peer->procptr;
	pp->unitptr = (caddr_t) vme;
	pp->timestarted = current_time;

	pp->io.clock_recv = vme_receive;
	pp->io.srcclock = (caddr_t)peer;
	pp->io.datalen = 0;
	pp->io.fd = fd_vme;

	/*
	 * All done.  Initialize a few random peer variables, then
 	 * return success. Note that root delay and root dispersion are
	 * always zero for this clock.
	 */
	peer->precision = VMEPRECISION;
	memcpy(&pp->refid, USNOREFID,4);
	return (1);
}


/*
 * vme_shutdown - shut down a VME clock
 */
static void
vme_shutdown(
	int unit, 
	struct peer *peer
	)
{
	register struct vmeunit *vme;
	struct refclockproc *pp;

	/*
	 * Tell the I/O module to turn us off.  We're history.
	 */
	pp = peer->procptr;
	vme = (struct vmeunit *)pp->unitptr;
	io_closeclock(&pp->io);
	pp->unitptr = NULL;
	free(vme);
}


/*
 * vme_receive - receive data from the VME device.
 *
 * Note: This interface would be interrupt-driven. We don't use that
 * now, but include a dummy routine for possible future adventures.
 */
static void
vme_receive(
	struct recvbuf *rbufp
	)
{
}


/*
 * vme_poll - called by the transmit procedure
 */
static void
vme_poll(
	int unit,
	struct peer *peer
	)
{
	struct vmedate *tptr; 
	struct vmeunit *vme;
	struct refclockproc *pp;
	time_t tloc;
	struct tm *tadr;
        
	pp = peer->procptr;	 
	vme = (struct vmeunit *)pp->unitptr;        /* Here is the structure */

	tptr = &vme->vmedata; 
	if ((tptr = get_datumtime(tptr)) == NULL ) {
		refclock_report(peer, CEVNT_BADREPLY);
		return;
	}

	get_systime(&pp->lastrec);
	pp->polls++;
	vme->lasttime = current_time;

	/*
	 * Get VME time and convert to timestamp format. 
	 * The year must come from the system clock.
	 */
	
	  time(&tloc);
	  tadr = gmtime(&tloc);
	  tptr->year = (unsigned short)(tadr->tm_year + 1900);
	

	sprintf(pp->a_lastcode, 
		"%3.3d %2.2d:%2.2d:%2.2d.%.6ld %1d",
		tptr->day, 
		tptr->hr, 
		tptr->mn,
		tptr->sec, 
		tptr->frac, 
		tptr->status);

	pp->lencode = (u_short) strlen(pp->a_lastcode);

	pp->day =  tptr->day;
	pp->hour =   tptr->hr;
	pp->minute =  tptr->mn;
	pp->second =  tptr->sec;
	pp->usec =   tptr->frac;	

#ifdef DEBUG
	if (debug)
	    printf("pp: %3d %02d:%02d:%02d.%06ld %1x\n",
		   pp->day, pp->hour, pp->minute, pp->second,
		   pp->usec, tptr->status);
#endif
	if (tptr->status ) {       /*  Status 0 is locked to ref., 1 is not */
		refclock_report(peer, CEVNT_BADREPLY);
		return;
	}

	/*
	 * Now, compute the reference time value. Use the heavy
	 * machinery for the seconds and the millisecond field for the
	 * fraction when present. If an error in conversion to internal
	 * format is found, the program declares bad data and exits.
	 * Note that this code does not yet know how to do the years and
	 * relies on the clock-calendar chip for sanity.
	 */
	if (!refclock_process(pp)) {
		refclock_report(peer, CEVNT_BADTIME);
		return;
	}
	record_clock_stats(&peer->srcadr, pp->a_lastcode);
	refclock_receive(peer);
}

struct vmedate *
get_datumtime(struct vmedate *time_vme)
{
	unsigned short  status;
	char cbuf[7];
	struct btfp_time vts;
	
	if ( time_vme == (struct vmedate *)NULL) {
  	  time_vme = (struct vmedate *)malloc(sizeof(struct vmedate ));
	}

	if( ioctl(fd_vme, READTIME, &vts))
	    msyslog(LOG_ERR, "get_datumtime error: %m");

	/* if you want to actually check the validity of these registers, do a 
	   define of CHECK   above this.  I didn't find it necessary. - RES
	*/

#ifdef CHECK            

	/* Get day */
	sprintf(cbuf,"%3.3x", ((vts.btfp_time[ 0 ] & 0x000f) <<8) +
		((vts.btfp_time[ 1 ] & 0xff00) >> 8));  

	if (isdigit(cbuf[0]) && isdigit(cbuf[1]) && isdigit(cbuf[2]) )
	    time_vme->day = (unsigned short)atoi(cbuf);
	else
	    time_vme->day = (unsigned short) 0;

	/* Get hour */
	sprintf(cbuf,"%2.2x", vts.btfp_time[ 1 ] & 0x00ff);

	if (isdigit(cbuf[0]) && isdigit(cbuf[1]))
	    time_vme->hr = (unsigned short)atoi(cbuf);
	else
	    time_vme->hr = (unsigned short) 0;

	/* Get minutes */
	sprintf(cbuf,"%2.2x", (vts.btfp_time[ 2 ] & 0xff00) >>8);
	if (isdigit(cbuf[0]) && isdigit(cbuf[1]))
	    time_vme->mn = (unsigned short)atoi(cbuf);
	else
	    time_vme->mn = (unsigned short) 0;

	/* Get seconds */
	sprintf(cbuf,"%2.2x", vts.btfp_time[ 2 ] & 0x00ff);

	if (isdigit(cbuf[0]) && isdigit(cbuf[1]))
	    time_vme->sec = (unsigned short)atoi(cbuf);
	else
	    time_vme->sec = (unsigned short) 0;

	/* Get microseconds.  Yes, we ignore the 0.1 microsecond digit so we can
	   use the TVTOTSF function  later on...*/

	sprintf(cbuf,"%4.4x%2.2x", vts.btfp_time[ 3 ],
		vts.btfp_time[ 4 ]>>8);

	if (isdigit(cbuf[0]) && isdigit(cbuf[1]) && isdigit(cbuf[2])
	    && isdigit(cbuf[3]) && isdigit(cbuf[4]) && isdigit(cbuf[5]))
	    time_vme->frac = (u_long) atoi(cbuf);
	else
	    time_vme->frac = (u_long) 0;
#else

	/* DONT CHECK  just trust the card */

	/* Get day */
	sprintf(cbuf,"%3.3x", ((vts.btfp_time[ 0 ] & 0x000f) <<8) +
		((vts.btfp_time[ 1 ] & 0xff00) >> 8));  
	time_vme->day = (unsigned short)atoi(cbuf);

	/* Get hour */
	sprintf(cbuf,"%2.2x", vts.btfp_time[ 1 ] & 0x00ff);

	time_vme->hr = (unsigned short)atoi(cbuf);

	/* Get minutes */
	sprintf(cbuf,"%2.2x", (vts.btfp_time[ 2 ] & 0xff00) >>8);
	time_vme->mn = (unsigned short)atoi(cbuf);

	/* Get seconds */
	sprintf(cbuf,"%2.2x", vts.btfp_time[ 2 ] & 0x00ff);
	time_vme->sec = (unsigned short)atoi(cbuf);

	/* Get microseconds.  Yes, we ignore the 0.1 microsecond digit so we can
	   use the TVTOTSF function  later on...*/

	sprintf(cbuf,"%4.4x%2.2x", vts.btfp_time[ 3 ],
		vts.btfp_time[ 4 ]>>8);

	time_vme->frac = (u_long) atoi(cbuf);

#endif /* CHECK */

	/* Get status bit */
	status = (vts.btfp_time[0] & 0x0010) >>4;
	time_vme->status = status;  /* Status=0 if locked to ref. */
	/* Status=1 if flywheeling */
	if (status) {        /* lost lock ? */
		return ((void *)NULL);
	}
	else
	    return (time_vme);
}

#else
int refclock_bancomm_bs;
#endif /* REFCLOCK */
