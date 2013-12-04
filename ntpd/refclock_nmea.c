/*
 * refclock_nmea.c - clock driver for an NMEA GPS CLOCK
 *		Michael Petry Jun 20, 1994
 *		 based on refclock_heathn.c
 *
 * Updated to add support for Accord GPS Clock
 *		Venu Gopal Dec 05, 2007
 *		neo.venu@gmail.com, venugopal_d@pgad.gov.in
 *
 * Updated to process 'time1' fudge factor
 *		Venu Gopal May 05, 2008
 *
 * Converted to common PPSAPI code, separate PPS fudge time1
 * from serial timecode fudge time2.
 *		Dave Hart July 1, 2009
 *		hart@ntp.org, davehart@davehart.com
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#if defined(REFCLOCK) && defined(CLOCK_NMEA)

#include <sys/stat.h>
#include <stdio.h>
#include <ctype.h>

#include "ntpd.h"
#include "ntp_io.h"
#include "ntp_unixtime.h"
#include "ntp_refclock.h"
#include "ntp_stdlib.h"
#include "ntp_calendar.h"

#ifdef HAVE_PPSAPI
# include "ppsapi_timepps.h"
# include "refclock_atom.h"
#endif /* HAVE_PPSAPI */

#ifdef SYS_WINNT
#undef write	/* ports/winnt/include/config.h: #define write _write */
extern int async_write(int, const void *, unsigned int);
#define write(fd, data, octets)	async_write(fd, data, octets)
#endif

#ifndef TIMESPECTOTS
#define TIMESPECTOTS(ptspec, pts)					\
	do {								\
		DTOLFP((ptspec)->tv_nsec * 1.0e-9, pts);		\
		(pts)->l_ui += (u_int32)((ptspec)->tv_sec) + JAN_1970;	\
	} while (0)
#endif


/*
 * This driver supports NMEA-compatible GPS receivers
 *
 * Prototype was refclock_trak.c, Thanks a lot.
 *
 * The receiver used spits out the NMEA sentences for boat navigation.
 * And you thought it was an information superhighway.	Try a raging river
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
 * bit 3 - enables ZDA (8) - Standard Time & Date
 * bit 3 - enables ZDG (8) - Accord GPS Clock's custom sentence with GPS time 
 *			     very close to standard ZDA
 * 
 * Multiple sentences may be selected except when ZDG/ZDA is selected.
 *
 * bit 4/5/6 - selects the baudrate for serial port :
 *		0 for 4800 (default) 
 *		1 for 9600 
 *		2 for 19200 
 *		3 for 38400 
 *		4 for 57600 
 *		5 for 115200 
 */
#define NMEA_MESSAGE_MASK_OLD	 0x07
#define NMEA_MESSAGE_MASK_SINGLE 0x08
#define NMEA_MESSAGE_MASK	 (NMEA_MESSAGE_MASK_OLD | NMEA_MESSAGE_MASK_SINGLE)

#define NMEA_BAUDRATE_MASK	 0x70
#define NMEA_BAUDRATE_SHIFT	 4

/*
 * Definitions
 */
#define	DEVICE		"/dev/gps%d"	/* GPS serial device */
#define	PPSDEV		"/dev/gpspps%d"	/* PPSAPI device override */
#define	SPEED232	B4800	/* uart speed (4800 bps) */
#define	PRECISION	(-9)	/* precision assumed (about 2 ms) */
#define	PPS_PRECISION	(-20)	/* precision assumed (about 1 us) */
#define	REFID		"GPS\0"	/* reference id */
#define	DESCRIPTION	"NMEA GPS Clock" /* who we are */
#ifndef O_NOCTTY
#define M_NOCTTY	0
#else
#define M_NOCTTY	O_NOCTTY
#endif
#ifndef O_NONBLOCK
#define M_NONBLOCK	0
#else
#define M_NONBLOCK	O_NONBLOCK
#endif
#define PPSOPENMODE	(O_RDWR | M_NOCTTY | M_NONBLOCK)

/* NMEA sentence array indexes for those we use */
#define NMEA_GPRMC	0	/* recommended min. nav. */
#define NMEA_GPGGA	1	/* fix and quality */
#define NMEA_GPGLL	2	/* geo. lat/long */
#define NMEA_GPZDA	3	/* date/time */
/*
 * $GPZDG is a proprietary sentence that violates the spec, by not
 * using $P and an assigned company identifier to prefix the sentence
 * identifier.	When used with this driver, the system needs to be
 * isolated from other NTP networks, as it operates in GPS time, not
 * UTC as is much more common.	GPS time is >15 seconds different from
 * UTC due to not respecting leap seconds since 1970 or so.  Other
 * than the different timebase, $GPZDG is similar to $GPZDA.
 */
#define NMEA_GPZDG	4
#define NMEA_ARRAY_SIZE (NMEA_GPZDG + 1)

/*
 * Sentence selection mode bits
 */
#define USE_ALL			0	/* any/all */
#define USE_GPRMC		1
#define USE_GPGGA		2
#define USE_GPGLL		4
#define USE_GPZDA_ZDG		8	/* affects both */

/* mapping from sentence index to controlling mode bit */
u_char sentence_mode[NMEA_ARRAY_SIZE] =
{
	USE_GPRMC,
	USE_GPGGA,
	USE_GPGLL,
	USE_GPZDA_ZDG,
	USE_GPZDA_ZDG
};

/*
 * Unit control structure
 */
struct nmeaunit {
#ifdef HAVE_PPSAPI
	struct refclock_atom atom; /* PPSAPI structure */
	int	ppsapi_tried;	/* attempt PPSAPI once */
	int	ppsapi_lit;	/* time_pps_create() worked */
	int	ppsapi_fd;	/* fd used with PPSAPI */
	int	ppsapi_gate;	/* allow edge detection processing */
	int	tcount;		/* timecode sample counter */
	int	pcount;		/* PPS sample counter */
#endif /* HAVE_PPSAPI */
	l_fp	tstamp;		/* timestamp of last poll */
	int	gps_time;	/* 0 UTC, 1 GPS time */
		/* per sentence checksum seen flag */
	struct calendar used;	/* hh:mm:ss of used sentence */
	u_char	cksum_seen[NMEA_ARRAY_SIZE];
};

/*
 * Function prototypes
 */
static	int	nmea_start	(int, struct peer *);
static	void	nmea_shutdown	(int, struct peer *);
static	void	nmea_receive	(struct recvbuf *);
static	void	nmea_poll	(int, struct peer *);
#ifdef HAVE_PPSAPI
static	void	nmea_control	(int, struct refclockstat *,
				 struct refclockstat *, struct peer *);
static	void	nmea_timer	(int, struct peer *);
#define		NMEA_CONTROL	nmea_control
#define		NMEA_TIMER	nmea_timer
#else
#define		NMEA_CONTROL	noentry
#define		NMEA_TIMER	noentry
#endif /* HAVE_PPSAPI */
static	void	gps_send	(int, const char *, struct peer *);
static	char *	field_parse	(char *, int);
static	int	nmea_checksum_ok(const char *);
static void nmea_day_unfold(struct calendar*);
static void nmea_century_unfold(struct calendar*);

/*
 * Transfer vector
 */
struct	refclock refclock_nmea = {
	nmea_start,		/* start up driver */
	nmea_shutdown,		/* shut down driver */
	nmea_poll,		/* transmit poll message */
	NMEA_CONTROL,		/* fudge control */
	noentry,		/* initialize driver */
	noentry,		/* buginfo */
	NMEA_TIMER		/* called once per second */
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
	int baudrate;
	char *baudtext;

	pp = peer->procptr;

	/*
	 * Open serial port. Use CLK line discipline, if available.
	 */
	snprintf(device, sizeof(device), DEVICE, unit);
	
	/*
	 * Opening the serial port with appropriate baudrate
	 * based on the value of bit 4/5/6
	 */
	switch ((peer->ttl & NMEA_BAUDRATE_MASK) >> NMEA_BAUDRATE_SHIFT) {
	case 0:
	case 6:
	case 7:
	default:
		baudrate = SPEED232;
		baudtext = "4800";
		break;
	case 1:
		baudrate = B9600;
		baudtext = "9600";
		break;
	case 2:
		baudrate = B19200;
		baudtext = "19200";
		break;
	case 3:
		baudrate = B38400;
		baudtext = "38400";
		break;
#ifdef B57600
	case 4:
		baudrate = B57600;
		baudtext = "57600";
		break;
#endif
#ifdef B115200
	case 5:
		baudrate = B115200;
		baudtext = "115200";
		break;
#endif
	}

	fd = refclock_open(device, baudrate, LDISC_CLK);
	
	if (fd <= 0) {
#ifdef HAVE_READLINK
		/* nmead support added by Jon Miner (cp_n18@yahoo.com)
		 *
		 * See http://home.hiwaay.net/~taylorc/gps/nmea-server/
		 * for information about nmead
		 *
		 * To use this, you need to create a link from /dev/gpsX
		 * to the server:port where nmead is running.  Something
		 * like this:
		 *
		 * ln -s server:port /dev/gps1
		 */
		char buffer[80];
		char *nmea_host, *nmea_tail;
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
		if ((nmea_tail = strtok(NULL,":")) == NULL)
			return(0);

		nmea_port = atoi(nmea_tail);

		if ((he = gethostbyname(nmea_host)) == NULL)
			return(0);
		if ((p = getprotobyname("tcp")) == NULL)
			return(0);
		memset(&so_addr, 0, sizeof(so_addr));
		so_addr.sin_family = AF_INET;
		so_addr.sin_port = htons(nmea_port);
		so_addr.sin_addr = *((struct in_addr *) he->h_addr);

		if ((fd = socket(PF_INET,SOCK_STREAM,p->p_proto)) == -1)
			return(0);
		if (connect(fd,(struct sockaddr *)&so_addr, sizeof(so_addr)) == -1) {
			close(fd);
			return (0);
		}
#else
		pp->io.fd = -1;
		return (0);
#endif
	}

	msyslog(LOG_NOTICE, "%s serial %s open at %s bps",
		refnumtoa(&peer->srcadr), device, baudtext);

	/*
	 * Allocate and initialize unit structure
	 */
	up = emalloc(sizeof(*up));
	memset(up, 0, sizeof(*up));
	pp->io.clock_recv = nmea_receive;
	pp->io.srcclock = (caddr_t)peer;
	pp->io.datalen = 0;
	pp->io.fd = fd;
	if (!io_addclock(&pp->io)) {
		pp->io.fd = -1;
		close(fd);
		free(up);
		return (0);
	}
	pp->unitptr = (caddr_t)up;

	/*
	 * Initialize miscellaneous variables
	 */
	peer->precision = PRECISION;
	pp->clockdesc = DESCRIPTION;
	memcpy(&pp->refid, REFID, 4);

	gps_send(fd,"$PMOTG,RMC,0000*1D\r\n", peer);

	return (1);
}


/*
 * nmea_shutdown - shut down a GPS clock
 * 
 * NOTE this routine is called after nmea_start() returns failure,
 * as well as during a normal shutdown due to ntpq :config unpeer.
 */
static void
nmea_shutdown(
	int unit,
	struct peer *peer
	)
{
	register struct nmeaunit *up;
	struct refclockproc *pp;

	UNUSED_ARG(unit);

	pp = peer->procptr;
	up = (struct nmeaunit *)pp->unitptr;
	if (up != NULL) {
#ifdef HAVE_PPSAPI
		if (up->ppsapi_lit) {
			time_pps_destroy(up->atom.handle);
			if (up->ppsapi_fd != pp->io.fd)
				close(up->ppsapi_fd);
		}
#endif
		free(up);
	}
	if (-1 != pp->io.fd)
		io_closeclock(&pp->io);
}

/*
 * nmea_control - configure fudge params
 */
#ifdef HAVE_PPSAPI
static void
nmea_control(
	int unit,
	struct refclockstat *in_st,
	struct refclockstat *out_st,
	struct peer *peer
	)
{
	char device[32];
	register struct nmeaunit *up;
	struct refclockproc *pp;
	int pps_fd;
	
	UNUSED_ARG(in_st);
	UNUSED_ARG(out_st);

	pp = peer->procptr;
	up = (struct nmeaunit *)pp->unitptr;

	if (!(CLK_FLAG1 & pp->sloppyclockflag)) {
		if (!up->ppsapi_tried)
			return;
		up->ppsapi_tried = 0;
		if (!up->ppsapi_lit)
			return;
		peer->flags &= ~FLAG_PPS;
		peer->precision = PRECISION;
		time_pps_destroy(up->atom.handle);
		if (up->ppsapi_fd != pp->io.fd)
			close(up->ppsapi_fd);
		up->atom.handle = 0;
		up->ppsapi_lit = 0;
		up->ppsapi_fd = -1;
		return;
	}

	if (up->ppsapi_tried)
		return;
	/*
	 * Light up the PPSAPI interface.
	 */
	up->ppsapi_tried = 1;

	/*
	 * if /dev/gpspps$UNIT can be opened that will be used for
	 * PPSAPI.  Otherwise, the GPS serial device /dev/gps$UNIT
	 * already opened is used for PPSAPI as well.
	 */
	snprintf(device, sizeof(device), PPSDEV, unit);

	pps_fd = open(device, PPSOPENMODE, S_IRUSR | S_IWUSR);

	if (-1 == pps_fd)
		pps_fd = pp->io.fd;
	
	if (refclock_ppsapi(pps_fd, &up->atom)) {
		up->ppsapi_lit = 1;
		up->ppsapi_fd = pps_fd;
		/* prepare to use the PPS API for our own purposes now. */
		refclock_params(pp->sloppyclockflag, &up->atom);
		return;
	}

	NLOG(NLOG_CLOCKINFO)
		msyslog(LOG_WARNING, "%s flag1 1 but PPSAPI fails",
			refnumtoa(&peer->srcadr));
}
#endif	/* HAVE_PPSAPI */


/*
 * nmea_timer - called once per second, fetches PPS
 *		timestamp and stuffs in median filter.
 */
#ifdef HAVE_PPSAPI
static void
nmea_timer(
	int		unit,
	struct peer *	peer
	)
{
	struct nmeaunit *up;
	struct refclockproc *pp;

	UNUSED_ARG(unit);

	pp = peer->procptr;
	up = (struct nmeaunit *)pp->unitptr;

	if (up->ppsapi_lit && up->ppsapi_gate &&
	    refclock_pps(peer, &up->atom, pp->sloppyclockflag) > 0) {
		up->pcount++,
		peer->flags |= FLAG_PPS;
		peer->precision = PPS_PRECISION;
	}
}
#endif	/* HAVE_PPSAPI */

#ifdef HAVE_PPSAPI
/*
 * This function is used to correlate a receive time stamp and a
 * reference time with a PPS edge time stamp. It applies the necessary
 * fudges (fudge1 for PPS, fudge2 for receive time) and then tries to
 * move the receive time stamp to the corresponding edge. This can
 * warp into future, if a transmission delay of more than 500ms is not
 * compensated with a corresponding fudge time2 value, because then
 * the next PPS edge is nearer than the last. (Similiar to what the
 * PPS ATOM driver does, but we deal with full time stamps here, not
 * just phase shift information.) Likewise, a negative fudge time2
 * value must be used if the reference time stamp correlates with the
 * *following* PPS pulse.
 *
 * Note that the receive time fudge value only needs to move the receive
 * stamp near a PPS edge but that close proximity is not required;
 * +/-100ms precision should be enough. But since the fudge value will
 * probably also be used to compensate the transmission delay when no PPS
 * edge can be related to the time stamp, it's best to get it as close
 * as possible.
 *
 * It should also be noted that the typical use case is matching to
 * the preceeding edge, as most units relate their sentences to the
 * current second.
 *
 * The function returns PPS_RELATE_NONE (0) if no PPS edge correlation
 * can be fixed; PPS_RELATE_EDGE (1) when a PPS edge could be fixed, but
 * the distance to the reference time stamp is too big (exceeds +/-400ms)
 * and the ATOM driver PLL cannot be used to fix the phase; and
 * PPS_RELATE_PHASE (2) when the ATOM driver PLL code can be used.
 *
 * On output, the receive time stamp is replaced with the
 * corresponding PPS edge time if a fix could be made; the PPS fudge
 * is updated to reflect the proper fudge time to apply. (This implies
 * that 'refclock_process_f()' must be used!)
 */
#define PPS_RELATE_NONE	 0	/* no pps correlation possible	  */
#define PPS_RELATE_EDGE	 1	/* recv time fixed, no phase lock */
#define PPS_RELATE_PHASE 2	/* recv time fixed, phase lock ok */

static int
refclock_ppsrelate(
	const struct refclockproc  *pp	    ,	/* for sanity	  */
	const struct refclock_atom *ap	    ,	/* for PPS io	  */
	const l_fp		   *reftime ,
	l_fp			   *rd_stamp,	/* i/o read stamp */
	double			    pp_fudge,	/* pps fudge	  */
	double			   *rd_fudge)	/* i/o read fudge */
{
	pps_info_t	pps_info;
	struct timespec timeout;
	l_fp		pp_stamp, pp_delta;
	double		delta, idelta;

	if (pp->leap == LEAP_NOTINSYNC)
		return PPS_RELATE_NONE;	/* clock is insane, no chance */
	
	memset(&timeout, 0, sizeof(timeout));
	memset(&pps_info, 0, sizeof(pps_info_t));

	if (time_pps_fetch(ap->handle, PPS_TSFMT_TSPEC,
			   &pps_info, &timeout) < 0)
		return PPS_RELATE_NONE;

	/* get last active PPS edge before receive */
	if (ap->pps_params.mode & PPS_CAPTUREASSERT)
		timeout = pps_info.assert_timestamp;
	else if (ap->pps_params.mode & PPS_CAPTURECLEAR)
		timeout = pps_info.clear_timestamp;
	else
		return PPS_RELATE_NONE;

	/* get delta between receive time and PPS time */
	TIMESPECTOTS(&timeout, &pp_stamp);
	pp_delta = *rd_stamp;
	L_SUB(&pp_delta, &pp_stamp);
	LFPTOD(&pp_delta, delta);
	delta += pp_fudge - *rd_fudge;
	if (fabs(delta) > 1.5)
		return PPS_RELATE_NONE; /* PPS timeout control */
	
	/* eventually warp edges, check phase */
	idelta	  = floor(delta + 0.5);
	pp_fudge -= idelta;
	delta	 -= idelta;
	if (fabs(delta) > 0.45)
		return PPS_RELATE_NONE; /* dead band control */

	/* we actually have a PPS edge to relate with! */
	*rd_stamp = pp_stamp;
	*rd_fudge = pp_fudge;

	/* if whole system out-of-sync, do not try to PLL */
	if (sys_leap == LEAP_NOTINSYNC)
		return PPS_RELATE_EDGE;	/* cannot PLL with atom code */

	/* check against reftime if ATOM PLL can be used */
	pp_delta = *reftime;
	L_SUB(&pp_delta, &pp_stamp);
	LFPTOD(&pp_delta, delta);
	delta += pp_fudge;
	if (fabs(delta) > 0.45)
		return PPS_RELATE_EDGE;	/* cannot PLL with atom code */

	/* all checks passed, gets an AAA rating here! */
	return PPS_RELATE_PHASE; /* can PLL with atom code */
}
#endif	/* HAVE_PPSAPI */

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
	char *cp, *dp, *msg;
	u_char sentence;
	/* Use these variables to hold data until we decide its worth
	 * keeping */
	char	rd_lastcode[BMAX];
	l_fp	rd_timestamp, reftime;
	int	rd_lencode;
	double	rd_fudge;
	struct calendar date;

	/*
	 * Initialize pointers and read the timecode and timestamp
	 */
	peer = rbufp->recv_peer;
	pp = peer->procptr;
	up = (struct nmeaunit *)pp->unitptr;

	rd_lencode = refclock_gtlin(
			rbufp, 
			rd_lastcode, 
			sizeof(rd_lastcode), 
			&rd_timestamp);

	/*
	 * There is a case that a <CR><LF> gives back a "blank" line.
	 * We can't have a well-formed sentence with less than 8 chars.
	 */
	if (0 == rd_lencode)
		return;

	if (rd_lencode < 8) {
		refclock_report(peer, CEVNT_BADREPLY);
		return;
	}

	DPRINTF(1, ("nmea: gpsread %d %s\n", rd_lencode, rd_lastcode));

	/*
	 * We check the timecode format and decode its contents. The
	 * we only care about a few of them.  The most important being
	 * the $GPRMC format
	 * $GPRMC,hhmmss,a,fddmm.xx,n,dddmmm.xx,w,zz.z,yyy.,ddmmyy,dd,v*CC
	 * mode (0,1,2,3) selects sentence ANY/ALL, RMC, GGA, GLL, ZDA
	 * $GPGLL,3513.8385,S,14900.7851,E,232420.594,A*21
	 * $GPGGA,232420.59,3513.8385,S,14900.7851,E,1,05,3.4,00519,M,,,,*3F
	 * $GPRMC,232418.19,A,3513.8386,S,14900.7853,E,00.0,000.0,121199,12.,E*77
	 *
	 * Defining GPZDA to support Standard Time & Date
	 * sentence. The sentence has the following format 
	 *  
	 *  $--ZDA,HHMMSS.SS,DD,MM,YYYY,TH,TM,*CS<CR><LF>
	 *
	 *  Apart from the familiar fields, 
	 *  'TH'    Time zone Hours
	 *  'TM'    Time zone Minutes
	 *
	 * Defining GPZDG to support Accord GPS Clock's custom NMEA 
	 * sentence. The sentence has the following format 
	 *  
	 *  $GPZDG,HHMMSS.S,DD,MM,YYYY,AA.BB,V*CS<CR><LF>
	 *
	 *  It contains the GPS timestamp valid for next PPS pulse.
	 *  Apart from the familiar fields, 
	 *  'AA.BB' denotes the signal strength( should be < 05.00 ) 
	 *  'V'	    denotes the GPS sync status : 
	 *	   '0' indicates INVALID time, 
	 *	   '1' indicates accuracy of +/-20 ms
	 *	   '2' indicates accuracy of +/-100 ns
	 */

	cp = rd_lastcode;
	if (cp[0] == '$') {
		/* Allow for GLGGA and GPGGA etc. */
		msg = cp + 3;

		if (strncmp(msg, "RMC", 3) == 0)
			sentence = NMEA_GPRMC;
		else if (strncmp(msg, "GGA", 3) == 0)
			sentence = NMEA_GPGGA;
		else if (strncmp(msg, "GLL", 3) == 0)
			sentence = NMEA_GPGLL;
		else if (strncmp(msg, "ZDG", 3) == 0)
			sentence = NMEA_GPZDG;
		else if (strncmp(msg, "ZDA", 3) == 0)
			sentence = NMEA_GPZDA;
		else
			return;
	} else
		return;

	/* See if I want to process this message type */
	if ((peer->ttl & NMEA_MESSAGE_MASK) &&
	   !(peer->ttl & sentence_mode[sentence]))
		return;

	/* 
	 * $GPZDG provides GPS time not UTC, and the two mix poorly.
	 * Once have processed a $GPZDG, do not process any further
	 * UTC sentences (all but $GPZDG currently).
	 */
	if (up->gps_time && NMEA_GPZDG != sentence)
		return;

	/*
	 * Apparently, older NMEA specifications (which are expensive)
	 * did not require the checksum for all sentences.  $GPMRC is
	 * the only one so far identified which has always been required
	 * to include a checksum.
	 *
	 * Today, most NMEA GPS receivers checksum every sentence.  To
	 * preserve its error-detection capabilities with modern GPSes
	 * while allowing operation without checksums on all but $GPMRC,
	 * we keep track of whether we've ever seen a checksum on a
	 * given sentence, and if so, reject future checksum failures.
	 */
	if (nmea_checksum_ok(rd_lastcode)) {
		up->cksum_seen[sentence] = TRUE;
	} else if (NMEA_GPRMC == sentence || up->cksum_seen[sentence]) {
		refclock_report(peer, CEVNT_BADREPLY);
		return;
	}

	cp = rd_lastcode;

	/* Grab field depending on clock string type */
	memset(&date, 0, sizeof(date));
	switch (sentence) {

	case NMEA_GPRMC:
		/*
		 * Test for synchronization.  Check for quality byte.
		 */
		dp = field_parse(cp, 2);
		if (dp[0] != 'A')
			pp->leap = LEAP_NOTINSYNC;
		else
			pp->leap = LEAP_NOWARNING;

		/* Now point at the time field */
		dp = field_parse(cp, 1);
		break;

	case NMEA_GPGGA:
		/*
		 * Test for synchronization.  Check for quality byte.
		 */
		dp = field_parse(cp, 6);
		if (dp[0] == '0')
			pp->leap = LEAP_NOTINSYNC;
		else
			pp->leap = LEAP_NOWARNING;

		/* Now point at the time field */
		dp = field_parse(cp, 1);
		break;

	case NMEA_GPGLL:
		/*
		 * Test for synchronization.  Check for quality byte.
		 */
		dp = field_parse(cp, 6);
		if (dp[0] != 'A')
			pp->leap = LEAP_NOTINSYNC;
		else
			pp->leap = LEAP_NOWARNING;

		/* Now point at the time field */
		dp = field_parse(cp, 5);
		break;
	
	case NMEA_GPZDG:
		/* For $GPZDG check for validity of GPS time. */
		dp = field_parse(cp, 6);
		if (dp[0] == '0') 
			pp->leap = LEAP_NOTINSYNC;
		else 
			pp->leap = LEAP_NOWARNING;
		/* fall through to NMEA_GPZDA */

	case NMEA_GPZDA:
		if (NMEA_GPZDA == sentence)
			pp->leap = LEAP_NOWARNING;

		/* Now point at the time field */
		dp = field_parse(cp, 1);
		break;

	default:
		return;
	}

	/*
	 * Check time code format of NMEA
	 */
	if (!isdigit((int)dp[0]) ||
	    !isdigit((int)dp[1]) ||
	    !isdigit((int)dp[2]) ||
	    !isdigit((int)dp[3]) ||
	    !isdigit((int)dp[4]) ||
	    !isdigit((int)dp[5])) {

		DPRINTF(1, ("NMEA time code %c%c%c%c%c%c non-numeric",
			    dp[0], dp[1], dp[2], dp[3], dp[4], dp[5]));
		refclock_report(peer, CEVNT_BADTIME);
		return;
	}

	/*
	 * Convert time and check values.
	 */
	date.hour = ((dp[0] - '0') * 10) + dp[1] - '0';
	date.minute = ((dp[2] - '0') * 10) + dp[3] -  '0';
	date.second = ((dp[4] - '0') * 10) + dp[5] - '0';
	/* 
	 * Default to 0 milliseconds, if decimal convert milliseconds in
	 * one, two or three digits
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

	if (date.hour > 23 || date.minute > 59 || 
	    date.second > 59 || pp->nsec > 1000000000) {

		DPRINTF(1, ("NMEA hour/min/sec/nsec range %02d:%02d:%02d.%09ld\n",
			    pp->hour, pp->minute, pp->second, pp->nsec));
		refclock_report(peer, CEVNT_BADTIME);
		return;
	}

	/*
	 * Used only the first recognized sentence each second.
	 */
	if (date.hour   == up->used.hour   &&
	    date.minute == up->used.minute &&
	    date.second == up->used.second)
		return;

	pp->lencode = (u_short)rd_lencode;
	memcpy(pp->a_lastcode, rd_lastcode, pp->lencode + 1);
	up->tstamp = rd_timestamp;
	pp->lastrec = up->tstamp;
	DPRINTF(1, ("nmea: timecode %d %s\n", pp->lencode, pp->a_lastcode));

	/*
	 * Convert date and check values.
	 */
	if (NMEA_GPRMC == sentence) {

		dp = field_parse(cp,9);
		date.monthday = 10 * (dp[0] - '0') + (dp[1] - '0');
		date.month    = 10 * (dp[2] - '0') + (dp[3] - '0');
		date.year     = 10 * (dp[4] - '0') + (dp[5] - '0');
		nmea_century_unfold(&date);

	} else if (NMEA_GPZDA == sentence || NMEA_GPZDG == sentence) {

		dp = field_parse(cp, 2);
		date.monthday = 10 * (dp[0] - '0') + (dp[1] - '0');
		dp = field_parse(cp, 3);
		date.month = 10 * (dp[0] - '0') + (dp[1] - '0');
		dp = field_parse(cp, 4);
		date.year = 1000 * (dp[0] - '0') + 100 * (dp[1] - '0')
			  + 10 * (dp[2] - '0') + (dp[3] - '0');

	} else
		nmea_day_unfold(&date);

	if (date.month < 1 || date.month > 12 ||
	    date.monthday < 1 || date.monthday > 31) {
		refclock_report(peer, CEVNT_BADDATE);
		return;
	}

	up->used.hour = date.hour;
	up->used.minute = date.minute;
	up->used.second = date.second;

	/*
	 * If "fudge 127.127.20.__ flag4 1" is configured in ntp.conf,
	 * remove the location and checksum from the NMEA sentence
	 * recorded as the last timecode and visible to remote users
	 * with:
	 *
	 * ntpq -c clockvar <server>
	 *
	 * Note that this also removes the location from the clockstats
	 * log (if it is enabled).  Some NTP operators monitor their
	 * NMEA GPS using the change in location in clockstats over
	 * time as as a proxy for the quality of GPS reception and
	 * thereby time reported.
	 */
	if (CLK_FLAG4 & pp->sloppyclockflag) {
		/*
		 * Start by pointing cp and dp at the fields with 
		 * longitude and latitude in the last timecode.
		 */
		switch (sentence) {

		case NMEA_GPGLL:
			cp = field_parse(pp->a_lastcode, 1);
			dp = field_parse(cp, 2);
			break;

		case NMEA_GPGGA:
			cp = field_parse(pp->a_lastcode, 2);
			dp = field_parse(cp, 2);
			break;

		case NMEA_GPRMC:
			cp = field_parse(pp->a_lastcode, 3);
			dp = field_parse(cp, 2);
			break;

		case NMEA_GPZDA:
		case NMEA_GPZDG:
		default:
			cp = dp = NULL;
		}

		/* Blank the entire latitude & longitude. */
		while (cp) {
			while (',' != *cp) {
				if ('.' != *cp)
					*cp = '_';
				cp++;
			}

			/* Longitude at cp then latitude at dp */
			if (cp < dp)
				cp = dp;
			else
				cp = NULL;
		}

		/* Blank the checksum, the last two characters */
		if (dp) {
			cp = pp->a_lastcode + pp->lencode - 2;
			if (0 == cp[2])
				cp[0] = cp[1] = '_';
		}

	}

	/*
	 * Get the reference time stamp from the calendar buffer.
	 * Process the new sample in the median filter and determine
	 * the timecode timestamp, but only if the PPS is not in
	 * control.
	 */
	rd_fudge = pp->fudgetime2;
	date.yearday = 0; /* make sure it's not used */
	DTOLFP(pp->nsec * 1.0e-9, &reftime);
	reftime.l_ui += caltontp(&date);

	/* $GPZDG postprocessing first... */
	if (NMEA_GPZDG == sentence) {
		/*
		 * Note if we're only using GPS timescale from now on.
		 */
		if (!up->gps_time) {
			up->gps_time = 1;
			NLOG(NLOG_CLOCKINFO)
			msyslog(LOG_INFO, "%s using only $GPZDG",
				refnumtoa(&peer->srcadr));
		}
		/*
		 * $GPZDG indicates the second after the *next* PPS
		 * pulse. So we remove 1 second from the reference
		 * time now.
		 */
		reftime.l_ui--;
	}

#ifdef HAVE_PPSAPI
	up->tcount++;
	/*
	 * If we have PPS running, we try to associate the sentence with
	 * the last active edge of the PPS signal.
	 */
	if (up->ppsapi_lit)
		switch (refclock_ppsrelate(pp, &up->atom, &reftime,
					  &rd_timestamp, pp->fudgetime1,
					  &rd_fudge))
		{
		case PPS_RELATE_EDGE:
			up->ppsapi_gate = 0;
			break;
		case PPS_RELATE_PHASE:
			up->ppsapi_gate = 1;
			break;
		default:
			break;
		}
	else 
		up->ppsapi_gate = 0;

	if (up->ppsapi_gate && (peer->flags & FLAG_PPS))
		return;
#endif /* HAVE_PPSAPI */

	refclock_process_offset(pp, reftime, rd_timestamp, rd_fudge);
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

	/*
	 * Process median filter samples. If none received, declare a
	 * timeout and keep going.
	 */
#ifdef HAVE_PPSAPI
	if (up->pcount == 0) {
		peer->flags &= ~FLAG_PPS;
		peer->precision = PRECISION;
	}
	if (up->tcount == 0) {
		pp->coderecv = pp->codeproc;
		refclock_report(peer, CEVNT_TIMEOUT);
		return;
	}
	up->pcount = up->tcount = 0;
#else /* HAVE_PPSAPI */
	if (pp->coderecv == pp->codeproc) {
		refclock_report(peer, CEVNT_TIMEOUT);
		return;
	}
#endif /* HAVE_PPSAPI */

	pp->polls++;
	pp->lastref = pp->lastrec;
	refclock_receive(peer);
	record_clock_stats(&peer->srcadr, pp->a_lastcode);

	/*
	 * usually nmea_receive can get a timestamp every second, 
	 * but at least one Motorola unit needs prompting each
	 * time.
	 */

	gps_send(pp->io.fd,"$PMOTG,RMC,0000*1D\r\n", peer);
}


/*
 *
 *	gps_send(fd,cmd, peer)	Sends a command to the GPS receiver.
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

	for (tp = cp; i && *tp; tp++)
		if (*tp == ',')
			i--;

	return tp;
}


/*
 * nmea_checksum_ok verifies 8-bit XOR checksum is correct then returns 1
 *
 * format is $XXXXX,1,2,3,4*ML
 *
 * 8-bit XOR of characters between $ and * noninclusive is transmitted
 * in last two chars M and L holding most and least significant nibbles
 * in hex representation such as:
 *
 *   $GPGLL,5057.970,N,00146.110,E,142451,A*27
 *   $GPVTG,089.0,T,,,15.2,N,,*7F
 */
int
nmea_checksum_ok(
	const char *sentence
	)
{
	u_char my_cs;
	u_long input_cs;
	const char *p;

	my_cs = 0;
	p = sentence;

	if ('$' != *p++)
		return 0;

	for ( ; *p && '*' != *p; p++) {

		my_cs ^= *p;
	}

	if ('*' != *p++)
		return 0;

	if (0 == p[0] || 0 == p[1] || 0 != p[2])
		return 0;

	if (0 == hextoint(p, &input_cs))
		return 0;

	if (my_cs != input_cs)
		return 0;

	return 1;
}

/*
 * -------------------------------------------------------------------
 * funny calendar-oriented stuff -- a bit hard to grok.
 * -------------------------------------------------------------------
 */
/*
 * Do a periodic unfolding of a truncated value around a given pivot
 * value.
 * The result r will hold to pivot <= r < pivot+period (period>0) or
 * pivot+period < r <= pivot (period < 0) and value % period == r % period,
 * using floor division convention.
 */
static time_t
nmea_periodic_unfold(
	time_t pivot,
	time_t value,
	time_t period)
{
	/*
	 * This will only work as long as 'value - pivot%period' does
	 * not create a signed overflow condition.
	 */
	value = (value - (pivot % period)) % period;
	if (value && (value ^ period) < 0)
		value += period;
	return pivot + value;
}

/*
 * Unfold a time-of-day (seconds since midnight) around the current
 * system time in a manner that guarantees an absolute difference of
 * less than 12hrs.
 *
 * This function is used for NMEA sentences that contain no date
 * information. This requires the system clock to be in +/-12hrs
 * around the true time, or the clock will synchronize the system 1day
 * off if not augmented with a time sources that also provide the
 * necessary date information.
 *
 * The function updates the refclockproc structure is also uses as
 * input to fetch the time from.
 */
static void
nmea_day_unfold(
	struct calendar *jd)
{
	time_t value, pivot;
	struct tm *tdate;

	value = ((time_t)jd->hour * MINSPERHR
		 + (time_t)jd->minute) * SECSPERMIN
		  + (time_t)jd->second;
	pivot = time(NULL) - SECSPERDAY/2;

	value = nmea_periodic_unfold(pivot, value, SECSPERDAY);
	tdate = gmtime(&value);
	if (tdate) {
		jd->year     = tdate->tm_year + 1900;
		jd->yearday  = tdate->tm_yday + 1;
		jd->month    = tdate->tm_mon + 1;
		jd->monthday = tdate->tm_mday;
		jd->hour     = tdate->tm_hour;
		jd->minute   = tdate->tm_min;
		jd->second   = tdate->tm_sec;
	} else {
		jd->year     = 0;
		jd->yearday  = 0;
		jd->month    = 0;
		jd->monthday = 0;
	}
}

/*
 * Unfold a 2-digit year into full year spec around the current year
 * of the system time. This requires the system clock to be in -79/+19
 * years around the true time, or the result will be off by
 * 100years. The assymetric behaviour was chosen to enable inital sync
 * for systems that do not have a battery-backup-clock and start with
 * a date that is typically years in the past.
 *
 * The function updates the calendar structure that is also used as
 * input to fetch the year from.
 */
static void
nmea_century_unfold(
	struct calendar *jd)
{
	time_t	   pivot_time;
	struct tm *pivot_date;
	time_t	   pivot_year;

	/* get warp limit and century start of pivot from system time */
	pivot_time = time(NULL);
	pivot_date = gmtime(&pivot_time);
	pivot_year = pivot_date->tm_year + 1900 - 20;
	jd->year = nmea_periodic_unfold(pivot_year, jd->year, 100);
}

#else
int refclock_nmea_bs;
#endif /* REFCLOCK && CLOCK_NMEA */
