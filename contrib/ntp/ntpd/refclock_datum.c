/*
** refclock_datum - clock driver for the Datum Programmable Time Server
**
** Important note: This driver assumes that you have termios. If you have
** a system that does not have termios, you will have to modify this driver.
**
** Sorry, I have only tested this driver on SUN and HP platforms.
*/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#if defined(REFCLOCK) && defined(CLOCK_DATUM)

/*
** Include Files
*/

#include <stdio.h>
#include <ctype.h>
#include <sys/time.h>

#include "ntpd.h"
#include "ntp_io.h"
#include "ntp_refclock.h"
#include "ntp_unixtime.h"
#include "ntp_stdlib.h"

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
** This driver supports the Datum Programmable Time System (PTS) clock.
** The clock works in very straight forward manner. When it receives a
** time code request (e.g., the ascii string "//k/mn"), it responds with
** a seven byte BCD time code. This clock only responds with a
** time code after it first receives the "//k/mn" message. It does not
** periodically send time codes back at some rate once it is started.
** the returned time code can be broken down into the following fields.
**
**            _______________________________
** Bit Index | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
**            ===============================
** byte 0:   | -   -   -   - |      H D      |
**            ===============================
** byte 1:   |      T D      |      U D      |
**            ===============================
** byte 2:   | -   - |  T H  |      U H      |
**            ===============================
** byte 3:   | - |    T M    |      U M      |
**            ===============================
** byte 4:   | - |    T S    |      U S      |
**            ===============================
** byte 5:   |      t S      |      h S      |
**            ===============================
** byte 6:   |      m S      | -   -   -   - |
**            ===============================
**
** In the table above:
**
**	"-" means don't care
**	"H D", "T D", and "U D" means Hundreds, Tens, and Units of Days
**	"T H", and "UH" means Tens and Units of Hours
**	"T M", and "U M" means Tens and Units of Minutes
**	"T S", and "U S" means Tens and Units of Seconds
**	"t S", "h S", and "m S" means tenths, hundredths, and thousandths
**				of seconds
**
** The Datum PTS communicates throught the RS232 port on your machine.
** Right now, it assumes that you have termios. This driver has been tested
** on SUN and HP workstations. The Datum PTS supports various IRIG and
** NASA input codes. This driver assumes that the name of the device is
** /dev/datum. You will need to make a soft link to your RS232 device or
** create a new driver to use this refclock.
*/

/*
** Datum PTS defines
*/

/*
** Note that if GMT is defined, then the Datum PTS must use Greenwich
** time. Otherwise, this driver allows the Datum PTS to use the current
** wall clock for its time. It determines the time zone offset by minimizing
** the error after trying several time zone offsets. If the Datum PTS
** time is Greenwich time and GMT is not defined, everything should still
** work since the time zone will be found to be 0. What this really means
** is that your system time (at least to start with) must be within the
** correct time by less than +- 30 minutes. The default is for GMT to not
** defined. If you really want to force GMT without the funny +- 30 minute
** stuff then you must define (uncomment) GMT below.
*/

/*
#define GMT
#define DEBUG_DATUM_PTC
#define LOG_TIME_ERRORS
*/


#define	PTSPRECISION	(-10)		/* precision assumed 1/1024 ms */
#define	DATMREFID "DATM"		/* reference id */
#define DATUM_DISPERSION 0		/* fixed dispersion = 0 ms */
#define DATUM_MAX_ERROR 0.100		/* limits on sigma squared */

#define DATUM_MAX_ERROR2 (DATUM_MAX_ERROR*DATUM_MAX_ERROR)

/*
** The Datum PTS structure
*/

/*
** I don't use a fixed array of MAXUNITS like everyone else just because
** I don't like to program that way. Sorry if this bothers anyone. I assume
** that you can use any id for your unit and I will search for it in a
** dynamic array of units until I find it. I was worried that users might
** enter a bad id in their configuration file (larger than MAXUNITS) and
** besides, it is just cleaner not to have to assume that you have a fixed
** number of anything in a program.
*/

struct datum_pts_unit {
	struct peer *peer;		/* peer used by ntp */
	struct refclockio io;		/* io structure used by ntp */
	int PTS_fd;			/* file descriptor for PTS */
	u_int unit;			/* id for unit */
	u_long timestarted;		/* time started */
	l_fp lastrec;			/* time tag for the receive time (system) */
	l_fp lastref;			/* reference time (Datum time) */
	u_long yearstart;		/* the year that this clock started */
	int coderecv;			/* number of time codes received */
	int day;			/* day */
	int hour;			/* hour */
	int minute;			/* minutes */
	int second;			/* seconds */
	int msec;			/* miliseconds */
	int usec;			/* miliseconds */
	u_char leap;			/* funny leap character code */
	char retbuf[8];		/* returned time from the datum pts */
	char nbytes;			/* number of bytes received from datum pts */ 
	double sigma2;		/* average squared error (roughly) */
	int tzoff;			/* time zone offest from GMT */
};

/*
** PTS static constant variables for internal use
*/

static char TIME_REQUEST[6];	/* request message sent to datum for time */
static int nunits;		/* number of active units */
static struct datum_pts_unit
**datum_pts_unit;	/* dynamic array of datum PTS structures */

/*
** Callback function prototypes that ntpd needs to know about.
*/

static	int	datum_pts_start		P((int, struct peer *));
static	void	datum_pts_shutdown	P((int, struct peer *));
static	void	datum_pts_poll		P((int, struct peer *));
static	void	datum_pts_control	P((int, struct refclockstat *,
					   struct refclockstat *, struct peer *));
static	void	datum_pts_init		P((void));
static	void	datum_pts_buginfo	P((int, struct refclockbug *, struct peer *));

/*
** This is the call back function structure that ntpd actually uses for
** this refclock.
*/

struct	refclock refclock_datum = {
	datum_pts_start,		/* start up a new Datum refclock */
	datum_pts_shutdown,		/* shutdown a Datum refclock */
	datum_pts_poll,		/* sends out the time request */
	datum_pts_control,		/* not used */
	datum_pts_init,		/* initialization (called first) */
	datum_pts_buginfo,		/* not used */
	NOFLAGS			/* we are not setting any special flags */
};

/*
** The datum_pts_receive callback function is handled differently from the
** rest. It is passed to the ntpd io data structure. Basically, every
** 64 seconds, the datum_pts_poll() routine is called. It sends out the time
** request message to the Datum Programmable Time System. Then, ntpd
** waits on a select() call to receive data back. The datum_pts_receive()
** function is called as data comes back. We expect a seven byte time
** code to be returned but the datum_pts_receive() function may only get
** a few bytes passed to it at a time. In other words, this routine may
** get called by the io stuff in ntpd a few times before we get all seven
** bytes. Once the last byte is received, we process it and then pass the
** new time measurement to ntpd for updating the system time. For now,
** there is no 3 state filtering done on the time measurements. The
** jitter may be a little high but at least for its current use, it is not
** a problem. We have tried to keep things as simple as possible. This
** clock should not jitter more than 1 or 2 mseconds at the most once
** things settle down. It is important to get the right drift calibrated
** in the ntpd.drift file as well as getting the right tick set up right
** using tickadj for SUNs. Tickadj is not used for the HP but you need to
** remember to bring up the adjtime daemon because HP does not support
** the adjtime() call.
*/

static	void	datum_pts_receive	P((struct recvbuf *));

/*......................................................................*/
/*	datum_pts_start - start up the datum PTS. This means open the	*/
/*	RS232 device and set up the data structure for my unit.		*/
/*......................................................................*/

static int
datum_pts_start(
	int unit,
	struct peer *peer
	)
{
	struct datum_pts_unit **temp_datum_pts_unit;
	struct datum_pts_unit *datum_pts;

#ifdef HAVE_TERMIOS
	struct termios arg;
#endif

#ifdef DEBUG_DATUM_PTC
	if (debug)
	    printf("Starting Datum PTS unit %d\n", unit);
#endif

	/*
	** Create the memory for the new unit
	*/

	temp_datum_pts_unit = (struct datum_pts_unit **)
		malloc((nunits+1)*sizeof(struct datum_pts_unit *));
	if (nunits > 0) memcpy(temp_datum_pts_unit, datum_pts_unit,
			       nunits*sizeof(struct datum_pts_unit *));
	free(datum_pts_unit);
	datum_pts_unit = temp_datum_pts_unit;
	datum_pts_unit[nunits] = (struct datum_pts_unit *)
		malloc(sizeof(struct datum_pts_unit));
	datum_pts = datum_pts_unit[nunits];

	datum_pts->unit = unit;	/* set my unit id */
	datum_pts->yearstart = 0;	/* initialize the yearstart to 0 */
	datum_pts->sigma2 = 0.0;	/* initialize the sigma2 to 0 */

	/*
	** Open the Datum PTS device
	*/

	datum_pts->PTS_fd = open("/dev/datum",O_RDWR);

	fcntl(datum_pts->PTS_fd, F_SETFL, 0); /* clear the descriptor flags */

#ifdef DEBUG_DATUM_PTC
	if (debug)
	    printf("Opening RS232 port with file descriptor %d\n",
		   datum_pts->PTS_fd);
#endif

	/*
	** Set up the RS232 terminal device information. Note that we assume that
	** we have termios. This code has only been tested on SUNs and HPs. If your
	** machine does not have termios this driver cannot be initialized. You can change this
	** if you want by editing this source. Please give the changes back to the
	** ntp folks so that it can become part of their regular distribution.
	*/

#ifdef HAVE_TERMIOS

	arg.c_iflag = IGNBRK;
	arg.c_oflag = 0;
	arg.c_cflag = B9600 | CS8 | CREAD | PARENB | CLOCAL;
	arg.c_lflag = 0;
	arg.c_cc[VMIN] = 0;		/* start timeout timer right away (not used) */
	arg.c_cc[VTIME] = 30;		/* 3 second timout on reads (not used) */

	tcsetattr(datum_pts->PTS_fd, TCSANOW, &arg);

#else

	msyslog(LOG_ERR, "Datum_PTS: Termios not supported in this driver");
	(void)close(datum_pts->PTS_fd);

	return 0;

#endif

	/*
	** Initialize the ntpd IO structure
	*/

	datum_pts->peer = peer;
	datum_pts->io.clock_recv = datum_pts_receive;
	datum_pts->io.srcclock = (caddr_t)datum_pts;
	datum_pts->io.datalen = 0;
	datum_pts->io.fd = datum_pts->PTS_fd;

	if (!io_addclock(&(datum_pts->io))) {

#ifdef DEBUG_DATUM_PTC
		if (debug)
		    printf("Problem adding clock\n");
#endif

		msyslog(LOG_ERR, "Datum_PTS: Problem adding clock");
		(void)close(datum_pts->PTS_fd);

		return 0;
	}

	peer->precision = PTSPRECISION;
	peer->stratum = 0;
	memcpy((char *)&peer->refid, DATMREFID, 4);

	/*
	** Now add one to the number of units and return a successful code
	*/

	nunits++;
	return 1;

}


/*......................................................................*/
/*	datum_pts_shutdown - this routine shuts doen the device and	*/
/*	removes the memory for the unit.				*/
/*......................................................................*/

static void
datum_pts_shutdown(
	int unit,
	struct peer *peer
	)
{
	int i,j;
	struct datum_pts_unit **temp_datum_pts_unit;

#ifdef DEBUG_DATUM_PTC
	if (debug)
	    printf("Shutdown Datum PTS\n");
#endif

	msyslog(LOG_ERR, "Datum_PTS: Shutdown Datum PTS");

	/*
	** First we have to find the right unit (i.e., the one with the same id).
	** We do this by looping through the dynamic array of units intil we find
	** it. Note, that I don't simply use an array with a maximimum number of
	** Datum PTS units. Everything is completely dynamic.
	*/

	for (i=0; i<nunits; i++) {
		if (datum_pts_unit[i]->unit == unit) {

			/*
			** We found the unit so close the file descriptor and free up the memory used
			** by the structure.
			*/

			io_closeclock(&datum_pts_unit[i]->io);
			close(datum_pts_unit[i]->PTS_fd);
			free(datum_pts_unit[i]);

			/*
			** Now clean up the datum_pts_unit dynamic array so that there are no holes.
			** This may mean moving pointers around, etc., to keep things compact.
			*/

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
	if (debug)
	    printf("Error, could not shut down unit %d\n",unit);
#endif

	msyslog(LOG_ERR, "Datum_PTS: Could not shut down Datum PTS unit %d",unit);

}

/*......................................................................*/
/*	datum_pts_poll - this routine sends out the time request to the */
/*	Datum PTS device. The time will be passed back in the 		*/
/*	datum_pts_receive() routine.					*/
/*......................................................................*/

static void
datum_pts_poll(
	int unit,
	struct peer *peer
	)
{
	int i;
	int index;
	int error_code;
	struct datum_pts_unit *datum_pts;

#ifdef DEBUG_DATUM_PTC
	if (debug)
	    printf("Poll Datum PTS\n");
#endif

	/*
	** Find the right unit and send out a time request once it is found.
	*/

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

	/*
	** Print out an error message if we could not find the right unit.
	*/

	if (index == -1) {

#ifdef DEBUG_DATUM_PTC
		if (debug)
		    printf("Error, could not poll unit %d\n",unit);
#endif

		msyslog(LOG_ERR, "Datum_PTS: Could not poll unit %d",unit);
		return;

	}

}


/*......................................................................*/
/*	datum_pts_control - not used					*/
/*......................................................................*/

static void
datum_pts_control(
	int unit,
	struct refclockstat *in,
	struct refclockstat *out,
	struct peer *peer
	)
{

#ifdef DEBUG_DATUM_PTC
	if (debug)
	    printf("Control Datum PTS\n");
#endif

}


/*......................................................................*/
/*	datum_pts_init - initializes things for all possible Datum	*/
/*	time code generators that might be used. In practice, this is	*/
/*	only called once at the beginning before anything else is	*/
/*	called.								*/
/*......................................................................*/

static void
datum_pts_init(void)
{

	/*									*/
	/*...... open up the log file if we are debugging ......................*/
	/*									*/

	/*
	** Open up the log file if we are debugging. For now, send data out to the
	** screen (stdout).
	*/

#ifdef DEBUG_DATUM_PTC
	if (debug)
	    printf("Init Datum PTS\n");
#endif

	/*
	** Initialize the time request command string. This is the only message
	** that we ever have to send to the Datum PTS (although others are defined).
	*/

	memcpy(TIME_REQUEST, "//k/mn",6);

	/*
	** Initialize the number of units to 0 and set the dynamic array of units to
	** NULL since there are no units defined yet.
	*/

	datum_pts_unit = NULL;
	nunits = 0;

}


/*......................................................................*/
/*	datum_pts_buginfo - not used					*/
/*......................................................................*/

static void
datum_pts_buginfo(
	int unit,
	register struct refclockbug *bug,
	register struct peer *peer
	)
{

#ifdef DEBUG_DATUM_PTC
	if (debug)
	    printf("Buginfo Datum PTS\n");
#endif

}


/*......................................................................*/
/*	datum_pts_receive - receive the time buffer that was read in	*/
/*	by the ntpd io handling routines. When 7 bytes have been	*/
/*	received (it may take several tries before all 7 bytes are	*/
/*	received), then the time code must be unpacked and sent to	*/
/*	the ntpd clock_receive() routine which causes the systems	*/
/*	clock to be updated (several layers down).			*/
/*......................................................................*/

static void
datum_pts_receive(
	struct recvbuf *rbufp
	)
{
	int i;
	l_fp tstmp;
	struct datum_pts_unit *datum_pts;
	char *dpt;
	int dpend;
	int tzoff;
	int timerr;
	double ftimerr, abserr;
#ifdef DEBUG_DATUM_PTC
	double dispersion;
#endif
	int goodtime;
      /*double doffset;*/

	/*
	** Get the time code (maybe partial) message out of the rbufp buffer.
	*/

	datum_pts = (struct datum_pts_unit *)rbufp->recv_srcclock;
	dpt = (char *)&rbufp->recv_space;
	dpend = rbufp->recv_length;

#ifdef DEBUG_DATUM_PTC
	if (debug)
	    printf("Receive Datum PTS: %d bytes\n", dpend);
#endif

	/*									*/
	/*...... save the ntp system time when the first byte is received ......*/
	/*									*/

	/*
	** Save the ntp system time when the first byte is received. Note that
	** because it may take several calls to this routine before all seven
	** bytes of our return message are finally received by the io handlers in
	** ntpd, we really do want to use the time tag when the first byte is
	** received to reduce the jitter.
	*/

	if (datum_pts->nbytes == 0) {
		datum_pts->lastrec = rbufp->recv_time;
	}

	/*
	** Increment our count to the number of bytes received so far. Return if we
	** haven't gotten all seven bytes yet.
	*/

	for (i=0; i<dpend; i++) {
		datum_pts->retbuf[datum_pts->nbytes+i] = dpt[i];
	}

	datum_pts->nbytes += dpend;

	if (datum_pts->nbytes != 7) {
		return;
	}

	/*
	** Convert the seven bytes received in our time buffer to day, hour, minute,
	** second, and msecond values. The usec value is not used for anything
	** currently. It is just the fractional part of the time stored in units
	** of microseconds.
	*/

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
	if (debug)
	    printf("day %d, hour %d, minute %d, second %d, msec %d\n",
		   datum_pts->day,
		   datum_pts->hour,
		   datum_pts->minute,
		   datum_pts->second,
		   datum_pts->msec);
#endif

	/*
	** Get the GMT time zone offset. Note that GMT should be zero if the Datum
	** reference time is using GMT as its time base. Otherwise we have to
	** determine the offset if the Datum PTS is using time of day as its time
	** base.
	*/

	goodtime = 0;		/* We are not sure about the time and offset yet */

#ifdef GMT

	/*
	** This is the case where the Datum PTS is using GMT so there is no time
	** zone offset.
	*/

	tzoff = 0;		/* set time zone offset to 0 */

#else

	/*
	** This is the case where the Datum PTS is using regular time of day for its
	** time so we must compute the time zone offset. The way we do it is kind of
	** funny but it works. We loop through different time zones (0 to 24) and
	** pick the one that gives the smallest error (+- one half hour). The time
	** zone offset is stored in the datum_pts structure for future use. Normally,
	** the clocktime() routine is only called once (unless the time zone offset
	** changes due to daylight savings) since the goodtime flag is set when a
	** good time is found (with a good offset). Note that even if the Datum
	** PTS is using GMT, this mechanism will still work since it should come up
	** with a value for tzoff = 0 (assuming that your system clock is within
	** a half hour of the Datum time (even with time zone differences).
	*/

	for (tzoff=0; tzoff<24; tzoff++) {
		if (clocktime( datum_pts->day,
			       datum_pts->hour,
			       datum_pts->minute,
			       datum_pts->second,
			       (tzoff + datum_pts->tzoff) % 24,
			       datum_pts->lastrec.l_ui,
			       &datum_pts->yearstart,
			       &datum_pts->lastref.l_ui) ) {

			error = datum_pts->lastref.l_ui - datum_pts->lastrec.l_ui;

#ifdef DEBUG_DATUM_PTC
			printf("Time Zone (clocktime method) = %d, error = %d\n", tzoff, error);
#endif

			if ((error < 1799) && (error > -1799)) {
				tzoff = (tzoff + datum_pts->tzoff) % 24;
				datum_pts->tzoff = tzoff;
				goodtime = 1;

#ifdef DEBUG_DATUM_PTC
				printf("Time Zone found (clocktime method) = %d\n",tzoff);
#endif

				break;
			}

		}
	}

#endif

	/*
	** Make sure that we have a good time from the Datum PTS. Clocktime() also
	** sets yearstart and lastref.l_ui. We will have to set astref.l_uf (i.e.,
	** the fraction of a second) stuff later.
	*/

	if (!goodtime) {

		if (!clocktime( datum_pts->day,
				datum_pts->hour,
				datum_pts->minute,
				datum_pts->second,
				tzoff,
				datum_pts->lastrec.l_ui,
				&datum_pts->yearstart,
				&datum_pts->lastref.l_ui) ) {

#ifdef DEBUG_DATUM_PTC
			if (debug)
			{
				printf("Error: bad clocktime\n");
				printf("GMT %d, lastrec %d, yearstart %d, lastref %d\n",
				       tzoff,
				       datum_pts->lastrec.l_ui,
				       datum_pts->yearstart,
				       datum_pts->lastref.l_ui);
			}
#endif

			msyslog(LOG_ERR, "Datum_PTS: Bad clocktime");

			return;

		}else{

#ifdef DEBUG_DATUM_PTC
			if (debug)
			    printf("Good clocktime\n");
#endif

		}

	}

	/*
	** We have datum_pts->lastref.l_ui set (which is the integer part of the
	** time. Now set the microseconds field.
	*/

	TVUTOTSF(datum_pts->usec, datum_pts->lastref.l_uf);

	/*
	** Compute the time correction as the difference between the reference
	** time (i.e., the Datum time) minus the receive time (system time).
	*/

	tstmp = datum_pts->lastref;		/* tstmp is the datum ntp time */
	L_SUB(&tstmp, &datum_pts->lastrec);	/* tstmp is now the correction */
	datum_pts->coderecv++;		/* increment a counter */

#ifdef DEBUG_DATUM_PTC
	dispersion = DATUM_DISPERSION;	/* set the dispersion to 0 */
	ftimerr = dispersion;
	ftimerr /= (1024.0 * 64.0);
	if (debug)
	    printf("dispersion = %d, %f\n", dispersion, ftimerr);
#endif

	/*
	** Pass the new time to ntpd through the refclock_receive function. Note
	** that we are not trying to make any corrections due to the time it takes
	** for the Datum PTS to send the message back. I am (erroneously) assuming
	** that the time for the Datum PTS to send the time back to us is negligable.
	** I suspect that this time delay may be as much as 15 ms or so (but probably
	** less). For our needs at JPL, this kind of error is ok so it is not
	** necessary to use fudge factors in the ntp.conf file. Maybe later we will.
	*/
      /*LFPTOD(&tstmp, doffset);*/
	refclock_receive(datum_pts->peer);

	/*
	** Compute sigma squared (not used currently). Maybe later, this could be
	** used for the dispersion estimate. The problem is that ntpd does not link
	** in the math library so sqrt() is not available. Anyway, this is useful
	** for debugging. Maybe later I will just use absolute values for the time
	** error to come up with my dispersion estimate. Anyway, for now my dispersion
	** is set to 0.
	*/

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
	if (debug)
	    printf("Time error = %f seconds\n", ftimerr);
#endif

#if defined(DEBUG_DATUM_PTC) || defined(LOG_TIME_ERRORS)
	if (debug)
	    printf("PTS: day %d, hour %d, minute %d, second %d, msec %d, Time Error %f\n",
		   datum_pts->day,
		   datum_pts->hour,
		   datum_pts->minute,
		   datum_pts->second,
		   datum_pts->msec,
		   ftimerr);
#endif

}
#else
int refclock_datum_bs;
#endif /* REFCLOCK */
