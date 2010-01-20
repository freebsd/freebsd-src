/*
 * refclock_nmea.c - clock driver for an NMEA GPS CLOCK
 *		Michael Petry Jun 20, 1994
 *		 based on refclock_heathn.c
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#if defined(SYS_WINNT)
#undef close
#define close closesocket
#endif

#if defined(REFCLOCK) && defined(CLOCK_NMEA)

#include <stdio.h>
#include <ctype.h>

#include "ntpd.h"
#include "ntp_io.h"
#include "ntp_unixtime.h"
#include "ntp_refclock.h"
#include "ntp_stdlib.h"

#ifdef HAVE_PPSAPI
# include "ppsapi_timepps.h"
#endif /* HAVE_PPSAPI */

/*
 * This driver supports the NMEA GPS Receiver with
 *
 * Protype was refclock_trak.c, Thanks a lot.
 *
 * The receiver used spits out the NMEA sentences for boat navigation.
 * And you thought it was an information superhighway.  Try a raging river
 * filled with rapids and whirlpools that rip away your data and warp time.
 *
 * If HAVE_PPSAPI is defined code to use the PPSAPI will be compiled in.
 * On startup if initialization of the PPSAPI fails, it will fall back
 * to the "normal" timestamps.
 *
 * The PPSAPI part of the driver understands fudge flag2 and flag3. If
 * flag2 is set, it will use the clear edge of the pulse. If flag3 is
 * set, kernel hardpps is enabled.
 *
 * GPS sentences other than RMC (the default) may be enabled by setting
 * the relevent bits of 'mode' in the server configuration line
 * server 127.127.20.x mode X
 * 
 * bit 0 - enables RMC (1)
 * bit 1 - enables GGA (2)
 * bit 2 - enables GLL (4)
 * multiple sentences may be selected
 */

/*
 * Definitions
 */
#ifdef SYS_WINNT
# define DEVICE "COM%d:" 	/* COM 1 - 3 supported */
#else
# define DEVICE	"/dev/gps%d"	/* name of radio device */
#endif
#define	SPEED232	B4800	/* uart speed (4800 bps) */
#define	PRECISION	(-9)	/* precision assumed (about 2 ms) */
#define	PPS_PRECISION	(-20)	/* precision assumed (about 1 us) */
#define	REFID		"GPS\0"	/* reference id */
#define	DESCRIPTION	"NMEA GPS Clock" /* who we are */
#define NANOSECOND	1000000000 /* one second (ns) */
#define RANGEGATE	500000	/* range gate (ns) */

#define LENNMEA		75	/* min timecode length */

/*
 * Tables to compute the ddd of year form icky dd/mm timecode. Viva la
 * leap.
 */
static int day1tab[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
static int day2tab[] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

/*
 * Unit control structure
 */
struct nmeaunit {
	int	pollcnt;	/* poll message counter */
	int	polled;		/* Hand in a sample? */
	l_fp	tstamp;		/* timestamp of last poll */
#ifdef HAVE_PPSAPI
	struct timespec ts;	/* last timestamp */
	pps_params_t pps_params; /* pps parameters */
	pps_info_t pps_info;	/* last pps data */
	pps_handle_t handle;	/* pps handlebars */
#endif /* HAVE_PPSAPI */
};

/*
 * Function prototypes
 */
static	int	nmea_start	P((int, struct peer *));
static	void	nmea_shutdown	P((int, struct peer *));
#ifdef HAVE_PPSAPI
static	void	nmea_control	P((int, struct refclockstat *, struct
				    refclockstat *, struct peer *));
static	int	nmea_ppsapi	P((struct peer *, int, int));
static	int	nmea_pps	P((struct nmeaunit *, l_fp *));
#endif /* HAVE_PPSAPI */
static	void	nmea_receive	P((struct recvbuf *));
static	void	nmea_poll	P((int, struct peer *));
static	void	gps_send	P((int, const char *, struct peer *));
static	char	*field_parse	P((char *, int));

/*
 * Transfer vector
 */
struct	refclock refclock_nmea = {
	nmea_start,		/* start up driver */
	nmea_shutdown,	/* shut down driver */
	nmea_poll,		/* transmit poll message */
#ifdef HAVE_PPSAPI
	nmea_control,		/* fudge control */
#else
	noentry,		/* fudge control */
#endif /* HAVE_PPSAPI */
	noentry,		/* initialize driver */
	noentry,		/* buginfo */
	NOFLAGS			/* not used */
};

/*
 * nmea_start - open the GPS devices and initialize data for processing
 */
static int
nmea_start(
	int unit,
	struct peer *peer
	)
{
	register struct nmeaunit *up;
	struct refclockproc *pp;
	int fd;
	char device[20];

	/*
	 * Open serial port. Use CLK line discipline, if available.
	 */
	(void)sprintf(device, DEVICE, unit);

	fd = refclock_open(device, SPEED232, LDISC_CLK);
	if (fd <= 0) {
#ifdef HAVE_READLINK
          /* nmead support added by Jon Miner (cp_n18@yahoo.com)
           *
           * See http://home.hiwaay.net/~taylorc/gps/nmea-server/
           * for information about nmead
           *
           * To use this, you need to create a link from /dev/gpsX to
           * the server:port where nmead is running.  Something like this:
           *
           * ln -s server:port /dev/gps1
           */
          char buffer[80];
          char *nmea_host;
          int   nmea_port;
          int   len;
          struct hostent *he;
          struct protoent *p;
          struct sockaddr_in so_addr;

          if ((len = readlink(device,buffer,sizeof(buffer))) == -1)
            return(0);
          buffer[len] = 0;

          if ((nmea_host = strtok(buffer,":")) == NULL)
            return(0);
         
          nmea_port = atoi(strtok(NULL,":"));

          if ((he = gethostbyname(nmea_host)) == NULL)
            return(0);
          if ((p = getprotobyname("ip")) == NULL)
            return(0);
          so_addr.sin_family = AF_INET;
          so_addr.sin_port = htons(nmea_port);
          so_addr.sin_addr = *((struct in_addr *) he->h_addr);

          if ((fd = socket(PF_INET,SOCK_STREAM,p->p_proto)) == -1)
            return(0);
          if (connect(fd,(struct sockaddr *)&so_addr,SOCKLEN(&so_addr)) == -1) {
            close(fd);
            return (0);
          }
#else
            return (0);
#endif
        }

	/*
	 * Allocate and initialize unit structure
	 */
	up = (struct nmeaunit *)emalloc(sizeof(struct nmeaunit));
	if (up == NULL) {
		(void) close(fd);
		return (0);
	}
	memset((char *)up, 0, sizeof(struct nmeaunit));
	pp = peer->procptr;
	pp->io.clock_recv = nmea_receive;
	pp->io.srcclock = (caddr_t)peer;
	pp->io.datalen = 0;
	pp->io.fd = fd;
	if (!io_addclock(&pp->io)) {
		(void) close(fd);
		free(up);
		return (0);
	}
	pp->unitptr = (caddr_t)up;

	/*
	 * Initialize miscellaneous variables
	 */
	peer->precision = PRECISION;
	pp->clockdesc = DESCRIPTION;
	memcpy((char *)&pp->refid, REFID, 4);
	up->pollcnt = 2;
	gps_send(pp->io.fd,"$PMOTG,RMC,0000*1D\r\n", peer);

#ifdef HAVE_PPSAPI
	/*
	 * Start the PPSAPI interface if it is there. Default to use
	 * the assert edge and do not enable the kernel hardpps.
	 */
	if (time_pps_create(fd, &up->handle) < 0) {
		up->handle = 0;
		msyslog(LOG_ERR,
		    "refclock_nmea: time_pps_create failed: %m");
		return (1);
	}
	return(nmea_ppsapi(peer, 0, 0));
#else
	return (1);
#endif /* HAVE_PPSAPI */
}

/*
 * nmea_shutdown - shut down a GPS clock
 */
static void
nmea_shutdown(
	int unit,
	struct peer *peer
	)
{
	register struct nmeaunit *up;
	struct refclockproc *pp;

	pp = peer->procptr;
	up = (struct nmeaunit *)pp->unitptr;
#ifdef HAVE_PPSAPI
	if (up->handle != 0)
		time_pps_destroy(up->handle);
#endif /* HAVE_PPSAPI */
	io_closeclock(&pp->io);
	free(up);
}

#ifdef HAVE_PPSAPI
/*
 * nmea_control - fudge control
 */
static void
nmea_control(
	int unit,		/* unit (not used */
	struct refclockstat *in, /* input parameters (not uded) */
	struct refclockstat *out, /* output parameters (not used) */
	struct peer *peer	/* peer structure pointer */
	)
{
	struct refclockproc *pp;

	pp = peer->procptr;
	nmea_ppsapi(peer, pp->sloppyclockflag & CLK_FLAG2,
	    pp->sloppyclockflag & CLK_FLAG3);
}


/*
 * Initialize PPSAPI
 */
int
nmea_ppsapi(
	struct peer *peer,	/* peer structure pointer */
	int enb_clear,		/* clear enable */
	int enb_hardpps		/* hardpps enable */
	)
{
	struct refclockproc *pp;
	struct nmeaunit *up;
	int capability;

	pp = peer->procptr;
	up = (struct nmeaunit *)pp->unitptr;
	if (time_pps_getcap(up->handle, &capability) < 0) {
		msyslog(LOG_ERR,
		    "refclock_nmea: time_pps_getcap failed: %m");
		return (0);
	}
	memset(&up->pps_params, 0, sizeof(pps_params_t));
	if (enb_clear)
		up->pps_params.mode = capability & PPS_CAPTURECLEAR;
	else
		up->pps_params.mode = capability & PPS_CAPTUREASSERT;
	if (!up->pps_params.mode) {
		msyslog(LOG_ERR,
		    "refclock_nmea: invalid capture edge %d",
		    !enb_clear);
		return (0);
	}
	up->pps_params.mode |= PPS_TSFMT_TSPEC;
	if (time_pps_setparams(up->handle, &up->pps_params) < 0) {
		msyslog(LOG_ERR,
		    "refclock_nmea: time_pps_setparams failed: %m");
		return (0);
	}
	if (enb_hardpps) {
		if (time_pps_kcbind(up->handle, PPS_KC_HARDPPS,
				    up->pps_params.mode & ~PPS_TSFMT_TSPEC,
				    PPS_TSFMT_TSPEC) < 0) {
			msyslog(LOG_ERR,
			    "refclock_nmea: time_pps_kcbind failed: %m");
			return (0);
		}
		pps_enable = 1;
	}
	peer->precision = PPS_PRECISION;

#if DEBUG
	if (debug) {
		time_pps_getparams(up->handle, &up->pps_params);
		printf(
		    "refclock_ppsapi: capability 0x%x version %d mode 0x%x kern %d\n",
		    capability, up->pps_params.api_version,
		    up->pps_params.mode, enb_hardpps);
	}
#endif

	return (1);
}

/*
 * Get PPSAPI timestamps.
 *
 * Return 0 on failure and 1 on success.
 */
static int
nmea_pps(
	struct nmeaunit *up,
	l_fp *tsptr
	)
{
	pps_info_t pps_info;
	struct timespec timeout, ts;
	double dtemp;
	l_fp tstmp;

	/*
	 * Convert the timespec nanoseconds field to ntp l_fp units.
	 */ 
	if (up->handle == 0)
		return (0);
	timeout.tv_sec = 0;
	timeout.tv_nsec = 0;
	memcpy(&pps_info, &up->pps_info, sizeof(pps_info_t));
	if (time_pps_fetch(up->handle, PPS_TSFMT_TSPEC, &up->pps_info,
	    &timeout) < 0)
		return (0);
	if (up->pps_params.mode & PPS_CAPTUREASSERT) {
		if (pps_info.assert_sequence ==
		    up->pps_info.assert_sequence)
			return (0);
		ts = up->pps_info.assert_timestamp;
	} else if (up->pps_params.mode & PPS_CAPTURECLEAR) {
		if (pps_info.clear_sequence ==
		    up->pps_info.clear_sequence)
			return (0);
		ts = up->pps_info.clear_timestamp;
	} else {
		return (0);
	}
	if ((up->ts.tv_sec == ts.tv_sec) && (up->ts.tv_nsec == ts.tv_nsec))
		return (0);
	up->ts = ts;

	tstmp.l_ui = ts.tv_sec + JAN_1970;
	dtemp = ts.tv_nsec * FRAC / 1e9;
	tstmp.l_uf = (u_int32)dtemp;
	*tsptr = tstmp;
	return (1);
}
#endif /* HAVE_PPSAPI */

/*
 * nmea_receive - receive data from the serial interface
 */
static void
nmea_receive(
	struct recvbuf *rbufp
	)
{
	register struct nmeaunit *up;
	struct refclockproc *pp;
	struct peer *peer;
	int month, day;
	int i;
	char *cp, *dp;
	int cmdtype;
	/* Use these variables to hold data until we decide its worth keeping */
	char	rd_lastcode[BMAX];
	l_fp	rd_tmp;
	u_short	rd_lencode;

	/*
	 * Initialize pointers and read the timecode and timestamp
	 */
	peer = (struct peer *)rbufp->recv_srcclock;
	pp = peer->procptr;
	up = (struct nmeaunit *)pp->unitptr;
	rd_lencode = (u_short)refclock_gtlin(rbufp, rd_lastcode, BMAX, &rd_tmp);

	/*
	 * There is a case that a <CR><LF> gives back a "blank" line
	 */
	if (rd_lencode == 0)
	    return;

#ifdef DEBUG
	if (debug)
	    printf("nmea: gpsread %d %s\n", rd_lencode,
		   rd_lastcode);
#endif

	/*
	 * We check the timecode format and decode its contents. The
	 * we only care about a few of them.  The most important being
	 * the $GPRMC format
	 * $GPRMC,hhmmss,a,fddmm.xx,n,dddmmm.xx,w,zz.z,yyy.,ddmmyy,dd,v*CC
	 * For Magellan (ColorTrak) GLL probably datum (order of sentences)
	 * also mode (0,1,2,3) select sentence ANY/ALL, RMC, GGA, GLL
	 * $GPGLL,3513.8385,S,14900.7851,E,232420.594,A*21
  	 * $GPGGA,232420.59,3513.8385,S,14900.7851,E,1,05,3.4,00519,M,,,,*3F
	 * $GPRMB,...
	 * $GPRMC,232418.19,A,3513.8386,S,14900.7853,E,00.0,000.0,121199,12.,E*77
	 * $GPAPB,...
	 * $GPGSA,...
	 * $GPGSV,...
	 * $GPGSV,...
	 */
#define GPXXX	0
#define GPRMC	1
#define GPGGA	2
#define GPGLL	4
	cp = rd_lastcode;
	cmdtype=0;
	if(strncmp(cp,"$GPRMC",6)==0) {
		cmdtype=GPRMC;
	}
	else if(strncmp(cp,"$GPGGA",6)==0) {
		cmdtype=GPGGA;
	}
	else if(strncmp(cp,"$GPGLL",6)==0) {
		cmdtype=GPGLL;
	}
	else if(strncmp(cp,"$GPXXX",6)==0) {
		cmdtype=GPXXX;
	}
	else
	    return;


	/* See if I want to process this message type */
	if ( ((peer->ttl == 0) && (cmdtype != GPRMC))
           || ((peer->ttl != 0) && !(cmdtype & peer->ttl)) )
		return;

	pp->lencode = rd_lencode;
	strcpy(pp->a_lastcode,rd_lastcode);
	cp = pp->a_lastcode;

	pp->lastrec = up->tstamp = rd_tmp;
	up->pollcnt = 2;

#ifdef DEBUG
	if (debug)
	    printf("nmea: timecode %d %s\n", pp->lencode,
		   pp->a_lastcode);
#endif


	/* Grab field depending on clock string type */
	switch( cmdtype ) {
	    case GPRMC:
		/*
		 * Test for synchronization.  Check for quality byte.
		 */
		dp = field_parse(cp,2);
		if( dp[0] != 'A')
			pp->leap = LEAP_NOTINSYNC;
		else
			pp->leap = LEAP_NOWARNING;

		/* Now point at the time field */
		dp = field_parse(cp,1);
		break;


	    case GPGGA:
		/*
		 * Test for synchronization.  Check for quality byte.
		 */
		dp = field_parse(cp,6);
		if( dp[0] == '0')
			pp->leap = LEAP_NOTINSYNC;
		else
			pp->leap = LEAP_NOWARNING;

		/* Now point at the time field */
		dp = field_parse(cp,1);
		break;


	    case GPGLL:
		/*
		 * Test for synchronization.  Check for quality byte.
		 */
		dp = field_parse(cp,6);
		if( dp[0] != 'A')
			pp->leap = LEAP_NOTINSYNC;
		else
			pp->leap = LEAP_NOWARNING;

		/* Now point at the time field */
		dp = field_parse(cp,5);
		break;


	    case GPXXX:
		return;
	    default:
		return;

	}

		/*
		 *	Check time code format of NMEA
		 */

		if( !isdigit((int)dp[0]) ||
		    !isdigit((int)dp[1]) ||
		    !isdigit((int)dp[2]) ||
		    !isdigit((int)dp[3]) ||
		    !isdigit((int)dp[4]) ||
		    !isdigit((int)dp[5])	
		    ) {
			refclock_report(peer, CEVNT_BADREPLY);
			return;
		}


	/*
	 * Convert time and check values.
	 */
	pp->hour = ((dp[0] - '0') * 10) + dp[1] - '0';
	pp->minute = ((dp[2] - '0') * 10) + dp[3] -  '0';
	pp->second = ((dp[4] - '0') * 10) + dp[5] - '0';
	/* Default to 0 milliseconds, if decimal convert milliseconds in
	   one, two or three digits
	*/
	pp->nsec = 0; 
	if (dp[6] == '.') {
		if (isdigit((int)dp[7])) {
			pp->nsec = (dp[7] - '0') * 100000000;
			if (isdigit((int)dp[8])) {
				pp->nsec += (dp[8] - '0') * 10000000;
				if (isdigit((int)dp[9])) {
					pp->nsec += (dp[9] - '0') * 1000000;
				}
			}
		}
	}

	if (pp->hour > 23 || pp->minute > 59 || pp->second > 59
	  || pp->nsec > 1000000000) {
		refclock_report(peer, CEVNT_BADTIME);
		return;
	}


	/*
	 * Convert date and check values.
	 */
	if (cmdtype==GPRMC) {
	    dp = field_parse(cp,9);
	    day = dp[0] - '0';
	    day = (day * 10) + dp[1] - '0';
	    month = dp[2] - '0';
	    month = (month * 10) + dp[3] - '0';
	    pp->year = dp[4] - '0';
	    pp->year = (pp->year * 10) + dp[5] - '0';
	}
	else {
	/* only time */
	    time_t tt = time(NULL);
	    struct tm * t = gmtime(&tt);
	    day = t->tm_mday;
	    month = t->tm_mon + 1;
	    pp->year= t->tm_year;
	}

	if (month < 1 || month > 12 || day < 1) {
		refclock_report(peer, CEVNT_BADTIME);
		return;
	}

        /* Hmmmm this will be a nono for 2100,2200,2300 but I don't think I'll be here */
        /* good thing that 2000 is a leap year */
	/* pp->year will be 00-99 if read from GPS, 00->  (years since 1900) from tm_year */
	if (pp->year % 4) {
		if (day > day1tab[month - 1]) {
			refclock_report(peer, CEVNT_BADTIME);
			return;
		}
		for (i = 0; i < month - 1; i++)
		    day += day1tab[i];
	} else {
		if (day > day2tab[month - 1]) {
			refclock_report(peer, CEVNT_BADTIME);
			return;
		}
		for (i = 0; i < month - 1; i++)
		    day += day2tab[i];
	}
	pp->day = day;


#ifdef HAVE_PPSAPI
	/*
	 * If the PPSAPI is working, rather use its timestamps.
	 * assume that the PPS occurs on the second so blow any msec
	 */
	if (nmea_pps(up, &rd_tmp) == 1) {
		pp->lastrec = up->tstamp = rd_tmp;
		pp->nsec = 0;
	}
#endif /* HAVE_PPSAPI */

	/*
	 * Process the new sample in the median filter and determine the
	 * reference clock offset and dispersion. We use lastrec as both
	 * the reference time and receive time, in order to avoid being
	 * cute, like setting the reference time later than the receive
	 * time, which may cause a paranoid protocol module to chuck out
	 * the data.
	 */

	if (!refclock_process(pp)) {
		refclock_report(peer, CEVNT_BADTIME);
		return;
	}



	/*
	 * Only go on if we had been polled.
	 */
	if (!up->polled)
	    return;
	up->polled = 0;
	pp->lastref = pp->lastrec;
	refclock_receive(peer);

        /* If we get here - what we got from the clock is OK, so say so */
         refclock_report(peer, CEVNT_NOMINAL);

	record_clock_stats(&peer->srcadr, pp->a_lastcode);

}

/*
 * nmea_poll - called by the transmit procedure
 *
 * We go to great pains to avoid changing state here, since there may be
 * more than one eavesdropper receiving the same timecode.
 */
static void
nmea_poll(
	int unit,
	struct peer *peer
	)
{
	register struct nmeaunit *up;
	struct refclockproc *pp;

	pp = peer->procptr;
	up = (struct nmeaunit *)pp->unitptr;
	if (up->pollcnt == 0)
	    refclock_report(peer, CEVNT_TIMEOUT);
	else
	    up->pollcnt--;
	pp->polls++;
	up->polled = 1;

	/*
	 * usually nmea_receive can get a timestamp every second
	 */

	gps_send(pp->io.fd,"$PMOTG,RMC,0000*1D\r\n", peer);
}

/*
 *
 *	gps_send(fd,cmd, peer)  Sends a command to the GPS receiver.
 *	 as	gps_send(fd,"rqts,u\r", peer);
 *
 *	We don't currently send any data, but would like to send
 *	RTCM SC104 messages for differential positioning. It should
 *	also give us better time. Without a PPS output, we're
 *	Just fooling ourselves because of the serial code paths
 *
 */
static void
gps_send(
	int fd,
	const char *cmd,
	struct peer *peer
	)
{

	if (write(fd, cmd, strlen(cmd)) == -1) {
		refclock_report(peer, CEVNT_FAULT);
	}
}

static char *
field_parse(
	char *cp,
	int fn
	)
{
	char *tp;
	int i = fn;

	for (tp = cp; *tp != '\0'; tp++) {
		if (*tp == ',')
		    i--;
		if (i == 0)
		    break;
	}
	return (++tp);
}
#else
int refclock_nmea_bs;
#endif /* REFCLOCK */
