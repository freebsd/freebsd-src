/*
 * refclock_datum - clock driver for the Datum watchayamacallit
 */
#if defined(REFCLOCK) && (defined(DATUM) || defined(DATUMCLK) || defined(DATUMPPS))
/*									*/
/*...... Include Files .................................................*/
/*									*/


#include <stdio.h>
#include <ctype.h>
#include <sys/time.h>
#include <sys/errno.h>

#include "ntpd.h"
#include "ntp_io.h"
#include "ntp_refclock.h"
#include "ntp_unixtime.h"

#if defined(HAVE_BSD_TTYS)
#include <sgtty.h>
#endif /* HAVE_BSD_TTYS */

#if defined(HAVE_SYSV_TTYS)
#include <termio.h>
#endif /* HAVE_SYSV_TTYS */

#if defined(HAVE_TERMIOS)
#include <termios.h>
#endif
#if defined(STREAM)
#include <stropts.h>
#if defined(WWVBCLK)
#include <sys/clkdefs.h>
#endif /* WWVBCLK */
#endif /* STREAM */

#if defined (WWVBPPS)
#include <sys/ppsclock.h>
#endif /* WWVBPPS */

#include "ntp_stdlib.h"

/*
#include "temp.c"
void set_logfile();
*/


#define DEBUG_DATUM_PTC


/*									*/
/*...... #defines ......................................................*/
/*									*/

/*
#define refclock_datum_pts refclock_wwvb
*/

#define MAXUNITS 4
#define	PTSPRECISION	(-13)	/* precision assumed (about 100 us) */
#define GMT 7
#define DATUM_MAX_ERROR 0.100
#define DATUM_MAX_ERROR2 DATUM_MAX_ERROR*DATUM_MAX_ERROR

/*									*/
/*...... externals .....................................................*/
/*									*/

extern U_LONG current_time;	/* current time (s) */
extern int debug;		/* global debug flag */


/*									*/
/*...... My structure ..................................................*/
/*									*/

struct datum_pts_unit {
  struct peer *peer;		/* peer used by xntp */
  struct refclockio io;		/* io structure used by xntp */
  int PTS_fd;			/* file descriptor for PTS */
  int PTS_START;		/* ? */
  u_int unit;			/* id for unit */
  U_LONG timestarted;		/* time started */
  int inuse;			/* in use flag */
  l_fp lastrec;
  l_fp lastref;
  int yearstart;
  int coderecv;
  int day;			/* day */
  int hour;			/* hour */
  int minute;			/* minutes */
  int second;			/* seconds */
  int msec;			/* miliseconds */
  int usec;			/* miliseconds */
  u_char leap;
  char retbuf[8];		/* returned time from the datum pts */
  char nbytes;			/* number of bytes received from datum pts */ 
  double sigma2;		/* average squared error (roughly) */
};


/*									*/
/*...... pts static constant variables for internal use ................*/ 
/*									*/

static char STOP_GENERATOR[6];
static char START_GENERATOR[6];
static char TIME_REQUEST[6];

static int nunits;
static struct datum_pts_unit **datum_pts_unit;
static u_char stratumtouse[MAXUNITS];
static l_fp fudgefactor[MAXUNITS];

static FILE *logfile;

/*									*/
/*...... callback functions that xntp knows about ......................*/
/*									*/

static	int	datum_pts_start		P((u_int, struct peer *));
static	void	datum_pts_shutdown	P((int));
static	void	datum_pts_poll		P((int, struct peer *));
static	void	datum_pts_control	P((u_int, struct refclockstat *,
						  struct refclockstat *));
static	void	datum_pts_init		P((void));
static	void	datum_pts_buginfo	P((int, struct refclockbug *));

struct	refclock refclock_datum = {
  datum_pts_start,
  datum_pts_shutdown,
  datum_pts_poll,
  datum_pts_control,
  datum_pts_init,
  datum_pts_buginfo,
  NOFLAGS
};

/*
struct	refclock refclock_wvvb = {
  datum_pts_start,
  datum_pts_shutdown,
  datum_pts_poll,
  datum_pts_control,
  datum_pts_init,
  datum_pts_buginfo,
  NOFLAGS
};
*/

/*									*/
/*...... receive callback functions for xntp  ..........................*/
/*									*/

static	void	datum_pts_receive	P((struct recvbuf *));


/*......................................................................*/
/*	datum_pts_start							*/
/*......................................................................*/

static int datum_pts_start(unit, peer)
  u_int unit;
  struct peer *peer;
{
  struct datum_pts_unit **temp_datum_pts_unit;
  struct datum_pts_unit *datum_pts;
  struct termios arg;

#ifdef DEBUG_DATUM_PTC
  fprintf(logfile, "Starting Datum PTS unit %d\n", unit);
  fflush(logfile);
#endif

/*									*/
/*...... create the memory for the new unit ............................*/
/*									*/

  temp_datum_pts_unit = (struct datum_pts_unit **)
		malloc((nunits+1)*sizeof(struct datum_pts_unit *));
  if (nunits > 0) memcpy(temp_datum_pts_unit, datum_pts_unit,
		nunits*sizeof(struct datum_pts_unit *));
  free(datum_pts_unit);
  datum_pts_unit = temp_datum_pts_unit;
  datum_pts_unit[nunits] = (struct datum_pts_unit *)
				malloc(sizeof(struct datum_pts_unit));
  datum_pts = datum_pts_unit[nunits];

  datum_pts->unit = unit;
  datum_pts->yearstart = 0;
  datum_pts->sigma2 = 0.0;

/*									*/
/*...... open the datum pts device .....................................*/
/*									*/

  datum_pts->PTS_fd = open("/dev/ttya",O_RDWR);

  fcntl(datum_pts->PTS_fd, F_SETFL, 0);

#ifdef DEBUG_DATUM_PTC
  fprintf(logfile,"Opening RS232 port ttya with file descriptor %d\n",
	datum_pts->PTS_fd);
  fflush(logfile);
#endif

/*									*/
/*...... set up the RS232 terminal device information ..................*/
/*									*/

  arg.c_iflag = IGNBRK;
  arg.c_oflag = 0;
  arg.c_cflag = B9600 | CS8 | CREAD | PARENB | CLOCAL;
  arg.c_lflag = 0;
  arg.c_line = 0;
  arg.c_cc[VMIN] = 0;		/* start timeout timer right away */
  arg.c_cc[VTIME] = 30;		/* this is a 3 second timout on reads */

  tcsetattr(datum_pts->PTS_fd, TCSANOW, &arg);

/*									*/
/*...... initialize the io structure ...................................*/
/*									*/

  datum_pts->peer = peer;
  datum_pts->timestarted = current_time;

  datum_pts->io.clock_recv = datum_pts_receive;
  datum_pts->io.srcclock = (caddr_t)datum_pts;
  datum_pts->io.datalen = 0;
  datum_pts->io.fd = datum_pts->PTS_fd;

  if (!io_addclock(&(datum_pts->io))) {
#ifdef DEBUG_DATUM_PTC
    fprintf(logfile,"Problem adding clock\n");
    fflush(logfile);
#endif
    syslog(LOG_ERR, "Datum_PTS: Problem adding clock");
  }

  peer->precision = PTSPRECISION;
  peer->rootdelay = 0;
  peer->rootdispersion = 0;
  peer->stratum = stratumtouse[unit];

/*									*/
/*...... now add one to the number of units ............................*/
/*									*/

  nunits++;

/*									*/
/*...... return successful code ........................................*/
/*									*/

  return 1;

}


/*......................................................................*/
/*	datum_pts_shutdown						*/
/*......................................................................*/

static void datum_pts_shutdown(unit)
  int unit;
{
  int i,j;
  struct datum_pts_unit **temp_datum_pts_unit;

#ifdef DEBUG_DATUM_PTC
  fprintf(logfile,"Shutdown Datum PTS\n");
  fflush(logfile);
#endif
  syslog(LOG_ERR, "Datum_PTS: Shutdown Datum PTS");

/*									*/
/*...... loop until the right unit is found ............................*/
/*									*/

  for (i=0; i<nunits; i++) {
    if (datum_pts_unit[i]->unit == unit) {

/*									*/
/*...... found it so close the file descriptor and free up memory .....*/
/*									*/

      io_closeclock(&datum_pts_unit[i]->io);
      close(datum_pts_unit[i]->PTS_fd);
      free(datum_pts_unit[i]);

/*									*/
/*...... clean up the datum_pts_unit array (no holes) ..................*/
/*									*/

      if (nunits > 1) {

	temp_datum_pts_unit = (struct datum_pts_unit **)
		malloc((nunits-1)*sizeof(struct datum_pts_unit *));
	if (i!= 0) memcpy(temp_datum_pts_unit, datum_pts_unit,
		i*sizeof(struct datum_pts_unit *));

	for (j=i+1; j<nunits; j++) {
	  temp_datum_pts_unit[j-1] = datum_pts_unit[j];
	}

	free(datum_pts_unit);
	datum_pts_unit = temp_datum_pts_unit;

      }else{

	free(datum_pts_unit);
	datum_pts_unit = NULL;

      }

      return;

    }
  }

#ifdef DEBUG_DATUM_PTC
  fprintf(logfile,"Error, could not shut down unit %d\n",unit);
  fflush(logfile);
#endif
  syslog(LOG_ERR, "Datum_PTS: Could not shut down Datum PTS unit %d",unit);

}


/*......................................................................*/
/*	datum_pts_poll							*/
/*......................................................................*/

static void datum_pts_poll(unit, peer)
  int unit;
  struct peer *peer;
{
  int i;
  int index;
  int error_code;
  l_fp tstmp;
  struct datum_pts_unit *datum_pts;

#ifdef DEBUG_DATUM_PTC
  fprintf(logfile,"Poll Datum PTS\n");
  fflush(logfile);
#endif

/*									*/
/*...... find the unit and send out a time request .....................*/
/*									*/

  index = -1;
  for (i=0; i<nunits; i++) {
    if (datum_pts_unit[i]->unit == unit) {
      index = i;
      datum_pts = datum_pts_unit[i];
      error_code = write(datum_pts->PTS_fd, TIME_REQUEST, 6);
      if (error_code != 6) perror("TIME_REQUEST");
      datum_pts->nbytes = 0;
      break;
    }
  }

  if (index == -1) {
#ifdef DEBUG_DATUM_PTC
    fprintf(logfile,"Error, could not poll unit %d\n",unit);
    fflush(logfile);
#endif
    syslog(LOG_ERR, "Datum_PTS: Could not poll unit %d",unit);
    return;
  }

}


/*......................................................................*/
/*	datum_pts_control						*/
/*......................................................................*/

static void datum_pts_control(unit, in, out)
  u_int unit;
  struct refclockstat *in;
  struct refclockstat *out;
{
#ifdef DEBUG_DATUM_PTC
  fprintf(logfile,"Control Datum PTS\n");
  fflush(logfile);
#endif
}


/*......................................................................*/
/*	datum_pts_init							*/
/*......................................................................*/

static void datum_pts_init()
{

#ifdef DEBUG_DATUM_PTC
  logfile = fopen("xntpd.log", "w");
  fprintf(logfile,"Init Datum PTS\n");
  fflush(logfile);
#endif

  memcpy(START_GENERATOR, "//kk01",6);
  memcpy(STOP_GENERATOR, "//kk00",6);
  memcpy(TIME_REQUEST, "//k/mn",6);

  datum_pts_unit = NULL;
  nunits = 0;
}


/*......................................................................*/
/*	datum_pts_buginfo						*/
/*......................................................................*/

static void datum_pts_buginfo(unit, bug)
  int unit;
  register struct refclockbug *bug;
{
#ifdef DEBUG_DATUM_PTC
  fprintf(logfile,"Buginfo Datum PTS\n");
  fflush(logfile);
#endif
}


/*......................................................................*/
/*	datum_pts_receive						*/
/*......................................................................*/

static void datum_pts_receive(rbufp)
  struct recvbuf *rbufp;
{
  int i;
  l_fp tstmp, trtmp, tstmp1;
  struct datum_pts_unit *datum_pts;
  char *dpt;
  int dpend;
  time_t tim;
  struct tm *loctm;
  int tzoff;
  int timerr;
  double ftimerr, abserr;

  datum_pts = (struct datum_pts_unit *)rbufp->recv_srcclock;
  dpt = (char *)&rbufp->recv_space;
  dpend = rbufp->recv_length;

#ifdef DEBUG_DATUM_PTC
  fprintf(logfile,"Receive Datum PTS: %d bytes\n", dpend);
  fflush(logfile);
#endif

  for (i=0; i<dpend; i++) {
    datum_pts->retbuf[datum_pts->nbytes+i] = dpt[i];
  }
  datum_pts->nbytes += dpend;

  if (datum_pts->nbytes != 7) {
    return;
  }


/*									*/
/*...... save the ntp system time ......................................*/
/*									*/

  trtmp = rbufp->recv_time;
  datum_pts->lastrec = trtmp;

/*									*/
/*...... convert the time from the buffer ..............................*/
/*									*/

  datum_pts->day =	100*(datum_pts->retbuf[0] & 0x0f) +
				10*((datum_pts->retbuf[1] & 0xf0)>>4) +
				(datum_pts->retbuf[1] & 0x0f);

  datum_pts->hour =	10*((datum_pts->retbuf[2] & 0x30)>>4) +
				(datum_pts->retbuf[2] & 0x0f);

  datum_pts->minute =	10*((datum_pts->retbuf[3] & 0x70)>>4) +
				(datum_pts->retbuf[3] & 0x0f);

  datum_pts->second =	10*((datum_pts->retbuf[4] & 0x70)>>4) +
				(datum_pts->retbuf[4] & 0x0f);

  datum_pts->msec =	100*((datum_pts->retbuf[5] & 0xf0) >> 4) + 
				10*(datum_pts->retbuf[5] & 0x0f) +
				((datum_pts->retbuf[6] & 0xf0)>>4);

  datum_pts->usec =	1000*datum_pts->msec;

#ifdef DEBUG_DATUM_PTC
  fprintf(logfile,"day %d, hour %d, minute %d, second %d, msec %d\n",
	datum_pts->day,
	datum_pts->hour,
	datum_pts->minute,
	datum_pts->second,
	datum_pts->msec);
  fflush(logfile);
#endif

/*									*/
/*...... get the GMT time zone offset ..................................*/
/*									*/

  tim = trtmp.l_ui - JAN_1970;
  loctm = localtime(&tim);
  tzoff = -loctm->tm_gmtoff/3600;

#ifdef DEBUG_DATUM_PTC
  fprintf(logfile,"Time Zone = %d, time (sec) since 1970 = %d\n",tzoff, tim);
  fflush(logfile);
#endif

/*									*/
/*...... make sure that we have a good time from the Datum PTS .........*/
/*									*/

  if (!clocktime( datum_pts->day,
		  datum_pts->hour,
		  datum_pts->minute,
		  datum_pts->second,
		  tzoff,
		  datum_pts->lastrec.l_ui,
		  &datum_pts->yearstart,
		  &datum_pts->lastref.l_ui) ) {

#ifdef DEBUG_DATUM_PTC
    fprintf(logfile,"Error: bad clocktime\n");
    fprintf(logfile,"GMT %d, lastrec %d, yearstart %d, lastref %d\n",
		  tzoff,
		  datum_pts->lastrec.l_ui,
		  datum_pts->yearstart,
		  datum_pts->lastref.l_ui);
    fflush(logfile);
#endif
    syslog(LOG_ERR, "Datum_PTS: Bad clocktime");

    return;

  }else{

#ifdef DEBUG_DATUM_PTC
    fprintf(logfile,"Good clocktime\n");
    fflush(logfile);
#endif

  }

/*									*/
/*...... we have datum_pts->lastref.l_ui set, get useconds now .........*/
/*									*/

  TVUTOTSF(datum_pts->usec, datum_pts->lastref.l_uf);

/*									*/
/*...... pass the new time to xntpd ....................................*/
/*									*/

  tstmp = datum_pts->lastref;
  L_SUB(&tstmp, &datum_pts->lastrec); /* tstmp is the time correction */
  datum_pts->coderecv++;

/*
  set_logfile(logfile);
*/

  refclock_receive(	datum_pts->peer,
			&tstmp,
			tzoff,
			0,
			&datum_pts->lastrec,
			&datum_pts->lastrec,
			datum_pts->leap	);
  timerr = tstmp.l_ui<<20;
  timerr |= (tstmp.l_uf>>12) & 0x000fffff;
  ftimerr = timerr;
  ftimerr /= 1024*1024;
  abserr = ftimerr;
  if (ftimerr < 0.0) abserr = -ftimerr;

  if (datum_pts->sigma2 == 0.0) {
    if (abserr < DATUM_MAX_ERROR) {
      datum_pts->sigma2 = abserr*abserr;
    }else{
      datum_pts->sigma2 = DATUM_MAX_ERROR2;
    }
  }else{
    if (abserr < DATUM_MAX_ERROR) {
      datum_pts->sigma2 = 0.95*datum_pts->sigma2 + 0.05*abserr*abserr;
    }else{
      datum_pts->sigma2 = 0.95*datum_pts->sigma2 + 0.05*DATUM_MAX_ERROR2;
    }
  }

#ifdef DEBUG_DATUM_PTC
  fprintf(logfile,"Time error = %f seconds, Sigma Squared = %f\n",
	ftimerr, datum_pts->sigma2);
  fflush(logfile);
#endif

/*									*/
/*...... make sure that we understand the format for xntp time .........*/
/*									*/

#ifdef DEBUG_DATUM_PTC
  tstmp.l_ui = 2;
  TVUTOTSF(123456, tstmp.l_uf);

  timerr = tstmp.l_ui<<20;
  timerr |= (tstmp.l_uf>>12) & 0x000fffff;
  ftimerr = timerr;
  ftimerr /= 1024*1024;
  fprintf(logfile,"Test1 2.123456 = %f\n",ftimerr);
  fflush(logfile);

  tstmp1.l_ui = 3;
  TVUTOTSF(223456, tstmp1.l_uf);
  timerr = tstmp1.l_ui<<20;
  timerr |= (tstmp1.l_uf>>12) & 0x000fffff;
  ftimerr = timerr;
  ftimerr /= 1024*1024;
  fprintf(logfile,"Test2 3.223456 = %f\n",ftimerr);
  fflush(logfile);

  L_SUB(&tstmp, &tstmp1);
  timerr = tstmp.l_ui<<20;
  timerr |= (tstmp.l_uf>>12) & 0x000fffff;
  ftimerr = timerr;
  ftimerr /= 1024*1024;
  fprintf(logfile,"Test3 -1.100000 = %f\n",ftimerr);
  fflush(logfile);
#endif


}
#endif
