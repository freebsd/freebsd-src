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

#include "ntp_types.h"

#if defined(REFCLOCK) && defined(CLOCK_NMEA)

#define NMEA_WRITE_SUPPORT 0 /* no write support at the moment */

#include <sys/stat.h>
#include <stdio.h>
#include <ctype.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#include "ntpd.h"
#include "ntp_io.h"
#include "ntp_unixtime.h"
#include "ntp_refclock.h"
#include "ntp_stdlib.h"
#include "ntp_calgps.h"
#include "timespecops.h"

#ifdef HAVE_PPSAPI
# include "ppsapi_timepps.h"
# include "refclock_atom.h"
#endif /* HAVE_PPSAPI */


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
#define NMEA_MESSAGE_MASK	0x0000FF0FU
#define NMEA_BAUDRATE_MASK	0x00000070U
#define NMEA_BAUDRATE_SHIFT	4

#define NMEA_DELAYMEAS_MASK	0x00000080U
#define NMEA_EXTLOG_MASK	0x00010000U
#define NMEA_QUIETPPS_MASK	0x00020000U
#define NMEA_DATETRUST_MASK	0x00040000U

#define NMEA_PROTO_IDLEN	4	/* tag name must be at least 4 chars */
#define NMEA_PROTO_MINLEN	6	/* min chars in sentence, excluding CS */
#define NMEA_PROTO_MAXLEN	80	/* max chars in sentence, excluding CS */
#define NMEA_PROTO_FIELDS	32	/* not official; limit on fields per record */

/*
 * We check the timecode format and decode its contents.  We only care
 * about a few of them, the most important being the $GPRMC format:
 *
 * $GPRMC,hhmmss,a,fddmm.xx,n,dddmmm.xx,w,zz.z,yyy.,ddmmyy,dd,v*CC
 *
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
 *
 * Defining PGRMF for Garmin GPS Fix Data
 * $PGRMF,WN,WS,DATE,TIME,LS,LAT,LAT_DIR,LON,LON_DIR,MODE,FIX,SPD,DIR,PDOP,TDOP
 * WN  -- GPS week number (weeks since 1980-01-06, mod 1024)
 * WS  -- GPS seconds in week
 * LS  -- GPS leap seconds, accumulated ( UTC + LS == GPS )
 * FIX -- Fix type: 0=nofix, 1=2D, 2=3D
 * DATE/TIME are standard date/time strings in UTC time scale
 *
 * The GPS time can be used to get the full century for the truncated
 * date spec.
 */

/*
 * Definitions
 */
#define	DEVICE		"/dev/gps%d"	/* GPS serial device */
#define	PPSDEV		"/dev/gpspps%d"	/* PPSAPI device override */
#define	SPEED232	B4800	/* uart speed (4800 bps) */
#define	PRECISION	(-9)	/* precision assumed (about 2 ms) */
#define	PPS_PRECISION	(-20)	/* precision assumed (about 1 us) */
#define	DATE_HOLD	16	/* seconds to hold on provided GPS date */
#define	DATE_HLIM	4	/* when do we take ANY date format */
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
#define NMEA_PGRMF	5
#define NMEA_PUBX04	6
#define NMEA_ARRAY_SIZE (NMEA_PUBX04 + 1)

/*
 * Sentence selection mode bits
 */
#define USE_GPRMC		0x00000001u
#define USE_GPGGA		0x00000002u
#define USE_GPGLL		0x00000004u
#define USE_GPZDA		0x00000008u
#define USE_PGRMF		0x00000100u
#define USE_PUBX04		0x00000200u

/* mapping from sentence index to controlling mode bit */
static const u_int32 sentence_mode[NMEA_ARRAY_SIZE] =
{
	USE_GPRMC,
	USE_GPGGA,
	USE_GPGLL,
	USE_GPZDA,
	USE_GPZDA,
	USE_PGRMF,
	USE_PUBX04
};

/* date formats we support */
enum date_fmt {
	DATE_1_DDMMYY,	/* use 1 field	with 2-digit year */
	DATE_3_DDMMYYYY	/* use 3 fields with 4-digit year */
};

/* date type */
enum date_type {
	DTYP_NONE,
	DTYP_Y2D,	/* 2-digit year */
	DTYP_W10B,	/* 10-bit week in GPS epoch */
	DTYP_Y4D,	/* 4-digit (full) year */
	DTYP_WEXT	/* extended week in GPS epoch */
};

/* results for 'field_init()'
 *
 * Note: If a checksum is present, the checksum test must pass OK or the
 * sentence is tagged invalid.
 */
#define CHECK_EMPTY  -1	/* no data			*/
#define CHECK_INVALID 0	/* not a valid NMEA sentence	*/
#define CHECK_VALID   1	/* valid but without checksum	*/
#define CHECK_CSVALID 2	/* valid with checksum OK	*/

/*
 * Unit control structure
 */
struct refclock_atom;
typedef struct refclock_atom TAtomUnit;
typedef struct {
#   ifdef HAVE_PPSAPI
	TAtomUnit	atom;		/* PPSAPI structure */
	int		ppsapi_fd;	/* fd used with PPSAPI */
	u_char		ppsapi_tried;	/* attempt PPSAPI once */
	u_char		ppsapi_lit;	/* time_pps_create() worked */
#   endif /* HAVE_PPSAPI */
	uint16_t	rcvtout;	/* one-shot for sample expiration */
	u_char		ppsapi_gate;	/* system is on PPS */
	u_char  	gps_time;	/* use GPS time, not UTC */
	l_fp		last_reftime;	/* last processed reference stamp */
	TNtpDatum	last_gpsdate;	/* last processed split date/time */
	u_short		hold_gpsdate;	/* validity ticker for above */
	u_short		type_gpsdate;	/* date info type for above */
	/* tally stats, reset each poll cycle */
	struct
	{
		u_int total;
		u_int accepted;
		u_int rejected;   /* GPS said not enough signal */
		u_int malformed;  /* Bad checksum, invalid date or time */
		u_int filtered;   /* mode bits, not GPZDG, same second */
		u_int pps_used;
	}
		tally;
	/* per sentence checksum seen flag */
	u_char		cksum_type[NMEA_ARRAY_SIZE];

	/* line assembly buffer (NMEAD support) */
	u_short	lb_len;
	char	lb_buf[BMAX];	/* assembly buffer */
} nmea_unit;

/*
 * helper for faster field access
 */
typedef struct {
	char  *base;	/* buffer base		*/
	char  *cptr;	/* current field ptr	*/
	int    blen;	/* buffer length	*/
	int    cidx;	/* current field index	*/
} nmea_data;

/*
 * Function prototypes
 */
static	int	nmea_start	(int, struct peer *);
static	void	nmea_shutdown	(int, struct peer *);
static	void	nmea_receive	(struct recvbuf *);
static	void	nmea_poll	(int, struct peer *);
static	void	nmea_procrec	(struct peer * const, l_fp);
#ifdef HAVE_PPSAPI
static	double	tabsdiffd	(l_fp, l_fp);
static	void	nmea_control	(int, const struct refclockstat *,
				 struct refclockstat *, struct peer *);
#define		NMEA_CONTROL	nmea_control
#else
#define		NMEA_CONTROL	noentry
#endif /* HAVE_PPSAPI */
static	void	nmea_timer	(int, struct peer *);

/* parsing helpers */
static int	field_init	(nmea_data * data, char * cp, int len);
static char *	field_parse	(nmea_data * data, int fn);
static void	field_wipe	(nmea_data * data, ...);
static u_char	parse_qual	(nmea_data * data, int idx,
				 char tag, int inv);
static int	parse_time	(TCivilDate * jd, l_fp * fofs,
				 nmea_data *, int idx);
static int	parse_date	(TCivilDate * jd, nmea_data *,
				 int idx, enum date_fmt fmt);
static int	parse_gpsw	(TGpsDatum *, nmea_data *,
				 int weekidx, int timeidx, int leapidx);

static int	nmead_open	(const char * device);

/*
 * If we want the driver to output sentences, too: re-enable the send
 * support functions by defining NMEA_WRITE_SUPPORT to non-zero...
 */
#if NMEA_WRITE_SUPPORT

static	void gps_send(int, const char *, struct peer *);
# ifdef SYS_WINNT
#  undef write	/* ports/winnt/include/config.h: #define write _write */
extern int async_write(int, const void *, unsigned int);
#  define write(fd, data, octets)	async_write(fd, data, octets)
# endif /* SYS_WINNT */

#endif /* NMEA_WRITE_SUPPORT */

/*
 * -------------------------------------------------------------------
 * Transfer vector
 * -------------------------------------------------------------------
 */
struct refclock refclock_nmea = {
	nmea_start,		/* start up driver */
	nmea_shutdown,		/* shut down driver */
	nmea_poll,		/* transmit poll message */
	NMEA_CONTROL,		/* fudge control */
	noentry,		/* initialize driver */
	noentry,		/* buginfo */
	nmea_timer		/* called once per second */
};


/*
 * -------------------------------------------------------------------
 * nmea_start - open the GPS devices and initialize data for processing
 *
 * return 0 on error, 1 on success. Even on error the peer structures
 * must be in a state that permits 'nmea_shutdown()' to clean up all
 * resources, because it will be called immediately to do so.
 * -------------------------------------------------------------------
 */
static int
nmea_start(
	int		unit,
	struct peer *	peer
	)
{
	struct refclockproc * const	pp = peer->procptr;
	nmea_unit * const		up = emalloc_zero(sizeof(*up));
	char				device[20];
	size_t				devlen;
	u_int32				rate;
	int				baudrate;
	const char *			baudtext;


	/* Get baudrate choice from mode byte bits 4/5/6 */
	rate = (peer->ttl & NMEA_BAUDRATE_MASK) >> NMEA_BAUDRATE_SHIFT;

	switch (rate) {
	case 0:
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
#   ifdef B57600
	case 4:
		baudrate = B57600;
		baudtext = "57600";
		break;
#   endif
#   ifdef B115200
	case 5:
		baudrate = B115200;
		baudtext = "115200";
		break;
#   endif
	default:
		baudrate = SPEED232;
		baudtext = "4800 (fallback)";
		break;
	}

	/* Allocate and initialize unit structure */
	pp->unitptr = (caddr_t)up;
	pp->io.fd = -1;
	pp->io.clock_recv = nmea_receive;
	pp->io.srcclock = peer;
	pp->io.datalen = 0;
	/* force change detection on first valid message */
	memset(&up->last_reftime, 0xFF, sizeof(up->last_reftime));
	memset(&up->last_gpsdate, 0x00, sizeof(up->last_gpsdate));
	/* force checksum on GPRMC, see below */
	up->cksum_type[NMEA_GPRMC] = CHECK_CSVALID;
#   ifdef HAVE_PPSAPI
	up->ppsapi_fd = -1;
#   endif /* HAVE_PPSAPI */
	ZERO(up->tally);

	/* Initialize miscellaneous variables */
	peer->precision = PRECISION;
	pp->clockdesc = DESCRIPTION;
	memcpy(&pp->refid, REFID, 4);

	/* Open serial port. Use CLK line discipline, if available. */
	devlen = snprintf(device, sizeof(device), DEVICE, unit);
	if (devlen >= sizeof(device)) {
		msyslog(LOG_ERR, "%s clock device name too long",
			refnumtoa(&peer->srcadr));
		return FALSE; /* buffer overflow */
	}
	pp->io.fd = refclock_open(device, baudrate, LDISC_CLK);
	if (0 >= pp->io.fd) {
		pp->io.fd = nmead_open(device);
		if (-1 == pp->io.fd)
			return FALSE;
	}
	LOGIF(CLOCKINFO, (LOG_NOTICE, "%s serial %s open at %s bps",
	      refnumtoa(&peer->srcadr), device, baudtext));

	/* succeed if this clock can be added */
	return io_addclock(&pp->io) != 0;
}

/*
 * -------------------------------------------------------------------
 * nmea_shutdown - shut down a GPS clock
 *
 * NOTE this routine is called after nmea_start() returns failure,
 * as well as during a normal shutdown due to ntpq :config unpeer.
 * -------------------------------------------------------------------
 */
static void
nmea_shutdown(
	int           unit,
	struct peer * peer
	)
{
	struct refclockproc * const pp = peer->procptr;
	nmea_unit	    * const up = (nmea_unit *)pp->unitptr;

	UNUSED_ARG(unit);

	if (up != NULL) {
#	    ifdef HAVE_PPSAPI
		if (up->ppsapi_lit)
			time_pps_destroy(up->atom.handle);
		if (up->ppsapi_tried && up->ppsapi_fd != pp->io.fd)
			close(up->ppsapi_fd);
#	    endif
		free(up);
	}
	pp->unitptr = (caddr_t)NULL;
	if (-1 != pp->io.fd)
		io_closeclock(&pp->io);
	pp->io.fd = -1;
}

/*
 * -------------------------------------------------------------------
 * nmea_control - configure fudge params
 * -------------------------------------------------------------------
 */
#ifdef HAVE_PPSAPI
static void
nmea_control(
	int                         unit,
	const struct refclockstat * in_st,
	struct refclockstat       * out_st,
	struct peer               * peer
	)
{
	struct refclockproc * const pp = peer->procptr;
	nmea_unit	    * const up = (nmea_unit *)pp->unitptr;

	char   device[32];
	size_t devlen;

	UNUSED_ARG(in_st);
	UNUSED_ARG(out_st);

	/*
	 * PPS control
	 *
	 * If /dev/gpspps$UNIT can be opened that will be used for
	 * PPSAPI.  Otherwise, the GPS serial device /dev/gps$UNIT
	 * already opened is used for PPSAPI as well. (This might not
	 * work, in which case the PPS API remains unavailable...)
	 */

	/* Light up the PPSAPI interface if not yet attempted. */
	if ((CLK_FLAG1 & pp->sloppyclockflag) && !up->ppsapi_tried) {
		up->ppsapi_tried = TRUE;
		devlen = snprintf(device, sizeof(device), PPSDEV, unit);
		if (devlen < sizeof(device)) {
			up->ppsapi_fd = open(device, PPSOPENMODE,
					     S_IRUSR | S_IWUSR);
		} else {
			up->ppsapi_fd = -1;
			msyslog(LOG_ERR, "%s PPS device name too long",
				refnumtoa(&peer->srcadr));
		}
		if (-1 == up->ppsapi_fd)
			up->ppsapi_fd = pp->io.fd;
		if (refclock_ppsapi(up->ppsapi_fd, &up->atom)) {
			/* use the PPS API for our own purposes now. */
			up->ppsapi_lit = refclock_params(
				pp->sloppyclockflag, &up->atom);
			if (!up->ppsapi_lit) {
				/* failed to configure, drop PPS unit */
				time_pps_destroy(up->atom.handle);
				msyslog(LOG_WARNING,
					"%s set PPSAPI params fails",
					refnumtoa(&peer->srcadr));
			}
			/* note: the PPS I/O handle remains valid until
			 * flag1 is cleared or the clock is shut down.
			 */
		} else {
			msyslog(LOG_WARNING,
				"%s flag1 1 but PPSAPI fails",
				refnumtoa(&peer->srcadr));
		}
	}

	/* shut down PPS API if activated */
	if ( !(CLK_FLAG1 & pp->sloppyclockflag) && up->ppsapi_tried) {
		/* shutdown PPS API */
		if (up->ppsapi_lit)
			time_pps_destroy(up->atom.handle);
		up->atom.handle = 0;
		/* close/drop PPS fd */
		if (up->ppsapi_fd != pp->io.fd)
			close(up->ppsapi_fd);
		up->ppsapi_fd = -1;

		/* clear markers and peer items */
		up->ppsapi_gate  = FALSE;
		up->ppsapi_lit   = FALSE;
		up->ppsapi_tried = FALSE;

		peer->flags &= ~FLAG_PPS;
		peer->precision = PRECISION;
	}
}
#endif /* HAVE_PPSAPI */

/*
 * -------------------------------------------------------------------
 * nmea_timer - called once per second
 *
 * Usually 'nmea_receive()' can get a timestamp every second, but at
 * least one Motorola unit needs prompting each time. Doing so in
 * 'nmea_poll()' gives only one sample per poll cycle, which actually
 * defeats the purpose of the median filter. Polling once per second
 * seems a much better idea.
 *
 * Also takes care of sample expiration if the receiver fails to
 * provide new input data.
 * -------------------------------------------------------------------
 */
static void
nmea_timer(
	int	      unit,
	struct peer * peer
	)
{
	struct refclockproc * const pp = peer->procptr;
	nmea_unit	    * const up = (nmea_unit *)pp->unitptr;

	UNUSED_ARG(unit);

#   if NMEA_WRITE_SUPPORT

	if (-1 != pp->io.fd) /* any mode bits to evaluate here? */
		gps_send(pp->io.fd, "$PMOTG,RMC,0000*1D\r\n", peer);

#   endif /* NMEA_WRITE_SUPPORT */

	/* receive timeout occurred? */
	if (up->rcvtout) {
		--up->rcvtout;
	} else if (pp->codeproc != pp->coderecv) {
		/* expire one (the oldest) sample, if any */
		refclock_samples_expire(pp, 1);
		/* reset message assembly buffer */
		up->lb_buf[0] = '\0';
		up->lb_len    = 0;
	}

	if (up->hold_gpsdate && (--up->hold_gpsdate < DATE_HLIM))
		up->type_gpsdate = DTYP_NONE;
}

/*
 * -------------------------------------------------------------------
 * nmea_procrec - receive data from the serial interface
 *
 * This is the workhorse for NMEA data evaluation:
 *
 * + it checks all NMEA data, and rejects sentences that are not valid
 *   NMEA sentences
 * + it checks whether a sentence is known and to be used
 * + it parses the time and date data from the NMEA data string and
 *   augments the missing bits. (century in date, whole date, ...)
 * + it rejects data that is not from the first accepted sentence in a
 *   burst
 * + it eventually replaces the receive time with the PPS edge time.
 * + it feeds the data to the internal processing stages.
 *
 * This function assumes a non-empty line in the unit line buffer.
 * -------------------------------------------------------------------
 */
static void
nmea_procrec(
	struct peer * const	peer,
	l_fp 	  		rd_timestamp
	)
{
	/* declare & init control structure pointers */
	struct refclockproc * const pp = peer->procptr;
	nmea_unit	    * const up = (nmea_unit*)pp->unitptr;

	/* Use these variables to hold data until we decide its worth keeping */
	nmea_data rdata;
	l_fp 	  rd_reftime;

	/* working stuff */
	TCivilDate	date;	/* to keep & convert the time stamp */
	TGpsDatum	wgps;	/* week time storage */
	TNtpDatum	dntp;
	l_fp		tofs;	/* offset to full-second reftime */
	/* results of sentence/date/time parsing */
	u_char		sentence;	/* sentence tag */
	int		checkres;
	int		warp;		/* warp to GPS base date */
	char *		cp;
	int		rc_date, rc_time;
	u_short		rc_dtyp;
#   ifdef HAVE_PPSAPI
	int		withpps = 0;
#   endif /* HAVE_PPSAPI */

	/* make sure data has defined pristine state */
	ZERO(tofs);
	ZERO(date);
	ZERO(wgps);
	ZERO(dntp);

	/*
	 * Read the timecode and timestamp, then initialize field
	 * processing. The <CR><LF> at the NMEA line end is translated
	 * to <LF><LF> by the terminal input routines on most systems,
	 * and this gives us one spurious empty read per record which we
	 * better ignore silently.
	 */
	checkres = field_init(&rdata, up->lb_buf, up->lb_len);
	switch (checkres) {

	case CHECK_INVALID:
		DPRINTF(1, ("%s invalid data: '%s'\n",
			refnumtoa(&peer->srcadr), up->lb_buf));
		refclock_report(peer, CEVNT_BADREPLY);
		return;

	case CHECK_EMPTY:
		return;

	default:
		DPRINTF(1, ("%s gpsread: %d '%s'\n",
			refnumtoa(&peer->srcadr), up->lb_len,
			up->lb_buf));
		break;
	}
	up->tally.total++;

	/*
	 * --> below this point we have a valid NMEA sentence <--
	 *
	 * Check sentence name. Skip first 2 chars (talker ID) in most
	 * cases, to allow for $GLGGA and $GPGGA etc. Since the name
	 * field has at least 5 chars we can simply shift the field
	 * start.
	 */
	cp = field_parse(&rdata, 0);
	if      (strncmp(cp + 2, "RMC,", 4) == 0)
		sentence = NMEA_GPRMC;
	else if (strncmp(cp + 2, "GGA,", 4) == 0)
		sentence = NMEA_GPGGA;
	else if (strncmp(cp + 2, "GLL,", 4) == 0)
		sentence = NMEA_GPGLL;
	else if (strncmp(cp + 2, "ZDA,", 4) == 0)
		sentence = NMEA_GPZDA;
	else if (strncmp(cp + 2, "ZDG,", 4) == 0)
		sentence = NMEA_GPZDG;
	else if (strncmp(cp,   "PGRMF,", 6) == 0)
		sentence = NMEA_PGRMF;
	else if (strncmp(cp,   "PUBX,04,", 8) == 0)
		sentence = NMEA_PUBX04;
	else
		return;	/* not something we know about */

	/* Eventually output delay measurement now. */
	if (peer->ttl & NMEA_DELAYMEAS_MASK) {
		mprintf_clock_stats(&peer->srcadr, "delay %0.6f %.*s",
			 ldexp(rd_timestamp.l_uf, -32),
			 (int)(strchr(up->lb_buf, ',') - up->lb_buf),
			 up->lb_buf);
	}

	/* See if I want to process this message type */
	if ((peer->ttl & NMEA_MESSAGE_MASK) &&
	    !(peer->ttl & sentence_mode[sentence])) {
		up->tally.filtered++;
		return;
	}

	/*
	 * make sure it came in clean
	 *
	 * Apparently, older NMEA specifications (which are expensive)
	 * did not require the checksum for all sentences.  $GPMRC is
	 * the only one so far identified which has always been required
	 * to include a checksum.
	 *
	 * Today, most NMEA GPS receivers checksum every sentence.  To
	 * preserve its error-detection capabilities with modern GPSes
	 * while allowing operation without checksums on all but $GPMRC,
	 * we keep track of whether we've ever seen a valid checksum on
	 * a given sentence, and if so, reject future instances without
	 * checksum.  ('up->cksum_type[NMEA_GPRMC]' is set in
	 * 'nmea_start()' to enforce checksums for $GPRMC right from the
	 * start.)
	 */
	if (up->cksum_type[sentence] <= (u_char)checkres) {
		up->cksum_type[sentence] = (u_char)checkres;
	} else {
		DPRINTF(1, ("%s checksum missing: '%s'\n",
			refnumtoa(&peer->srcadr), up->lb_buf));
		refclock_report(peer, CEVNT_BADREPLY);
		up->tally.malformed++;
		return;
	}

	/*
	 * $GPZDG provides GPS time not UTC, and the two mix poorly.
	 * Once have processed a $GPZDG, do not process any further UTC
	 * sentences (all but $GPZDG currently).
	 */
	if (sentence == NMEA_GPZDG) {
		if (!up->gps_time) {
			msyslog(LOG_INFO,
				"%s using GPS time as if it were UTC",
				refnumtoa(&peer->srcadr));
			up->gps_time = 1;
		}
	} else {
		if (up->gps_time) {
			up->tally.filtered++;
			return;
		}
	}

	DPRINTF(1, ("%s processing %d bytes, timecode '%s'\n",
		refnumtoa(&peer->srcadr), up->lb_len, up->lb_buf));

	/*
	 * Grab fields depending on clock string type and possibly wipe
	 * sensitive data from the last timecode.
	 */
	rc_date = -1;	/* assume we have to do day-time mapping */
	rc_dtyp = DTYP_NONE;
       	switch (sentence) {

	case NMEA_GPRMC:
		/* Check quality byte, fetch data & time */
		rc_time	 = parse_time(&date, &tofs, &rdata, 1);
		pp->leap = parse_qual(&rdata, 2, 'A', 0);
		if (up->type_gpsdate <= DTYP_Y2D) {
			rc_date	= parse_date(&date, &rdata, 9, DATE_1_DDMMYY);
			rc_dtyp = DTYP_Y2D;
		}
 		if (CLK_FLAG4 & pp->sloppyclockflag)
			field_wipe(&rdata, 3, 4, 5, 6, -1);
		break;

	case NMEA_GPGGA:
		/* Check quality byte, fetch time only */
		rc_time	 = parse_time(&date, &tofs, &rdata, 1);
		pp->leap = parse_qual(&rdata, 6, '0', 1);
		if (CLK_FLAG4 & pp->sloppyclockflag)
			field_wipe(&rdata, 2, 4, -1);
		break;

	case NMEA_GPGLL:
		/* Check quality byte, fetch time only */
		rc_time	 = parse_time(&date, &tofs, &rdata, 5);
		pp->leap = parse_qual(&rdata, 6, 'A', 0);
		if (CLK_FLAG4 & pp->sloppyclockflag)
			field_wipe(&rdata, 1, 3, -1);
		break;

	case NMEA_GPZDA:
		/* No quality.	Assume best, fetch time & full date */
		rc_time	= parse_time(&date, &tofs, &rdata, 1);
		if (up->type_gpsdate <= DTYP_Y4D) {
			rc_date	= parse_date(&date, &rdata, 2, DATE_3_DDMMYYYY);
			rc_dtyp = DTYP_Y4D;
		}
		break;

	case NMEA_GPZDG:
		/* Check quality byte, fetch time & full date */
		rc_time	 = parse_time(&date, &tofs, &rdata, 1);
		pp->leap = parse_qual(&rdata, 4, '0', 1);
		--tofs.l_ui; /* GPZDG gives *following* second */
		if (up->type_gpsdate <= DTYP_Y4D) {
			rc_date	= parse_date(&date, &rdata, 2, DATE_3_DDMMYYYY);
			rc_dtyp = DTYP_Y4D;
		}
		break;

	case NMEA_PGRMF:
		/* get time, qualifier and GPS weektime. */
		rc_time = parse_time(&date, &tofs, &rdata, 4);
		if (up->type_gpsdate <= DTYP_W10B) {
			rc_date = parse_gpsw(&wgps, &rdata, 1, 2, 5);
			rc_dtyp = DTYP_W10B;
		}
		pp->leap = parse_qual(&rdata, 11, '0', 1);
		if (CLK_FLAG4 & pp->sloppyclockflag)
			field_wipe(&rdata, 6, 8, -1);
		break;

	case NMEA_PUBX04:
		/* PUBX,04 is peculiar. The UTC time-of-week is the *internal*
		 * time base, which is not exactly on par with the fix time.
		 */
		rc_time = parse_time(&date, &tofs, &rdata, 2);
		if (up->type_gpsdate <= DTYP_WEXT) {
			rc_date = parse_gpsw(&wgps, &rdata, 5, 4, -1);
			rc_dtyp = DTYP_WEXT;
		}
		break;

	default:
		INVARIANT(0);	/* Coverity 97123 */
		return;
	}

	/* check clock sanity; [bug 2143] */
	if (pp->leap == LEAP_NOTINSYNC) { /* no good status? */
		checkres = CEVNT_PROP;
		up->tally.rejected++;
	}
	/* Check sanity of time-of-day. */
	else if (rc_time == 0) {	/* no time or conversion error? */
		checkres = CEVNT_BADTIME;
		up->tally.malformed++;
	}
	/* Check sanity of date. */
	else if (rc_date == 0) {	/* no date or conversion error? */
		checkres = CEVNT_BADDATE;
		up->tally.malformed++;
	}
	else {
		checkres = -1;
	}

	if (checkres != -1) {
		refclock_save_lcode(pp, up->lb_buf, up->lb_len);
		refclock_report(peer, checkres);
		return;
	}

	/* See if we can augment the receive time stamp. If not, apply
	 * fudge time 2 to the receive time stamp directly.
	 */
#   ifdef HAVE_PPSAPI
	if (up->ppsapi_lit && pp->leap != LEAP_NOTINSYNC)
		withpps = refclock_ppsaugment(
			&up->atom, &rd_timestamp,
			pp->fudgetime2, pp->fudgetime1);
	else
#   endif /* HAVE_PPSAPI */
		rd_timestamp = ntpfp_with_fudge(
			rd_timestamp, pp->fudgetime2);

	/* set the GPS base date, if possible */
	warp = !(peer->ttl & NMEA_DATETRUST_MASK);
	if (rc_dtyp != DTYP_NONE) {
		DPRINTF(1, ("%s saving date, type=%hu\n",
			    refnumtoa(&peer->srcadr), rc_dtyp));
		switch (rc_dtyp) {
		case DTYP_W10B:
			up->last_gpsdate = gpsntp_from_gpscal_ex(
				&wgps, (warp = TRUE));
			break;
		case DTYP_WEXT:
			up->last_gpsdate = gpsntp_from_gpscal_ex(
				&wgps, warp);
			break;
		default:
			up->last_gpsdate = gpsntp_from_calendar_ex(
				&date, tofs, warp);
			break;
		}
		up->type_gpsdate = rc_dtyp;
		up->hold_gpsdate = DATE_HOLD;
	}
	/* now convert and possibly extend/expand the time stamp. */
	if (up->hold_gpsdate) {	/* time of day, based */
		dntp = gpsntp_from_daytime2_ex(
			&date, tofs, &up->last_gpsdate, warp);
	} else {		/* time of day, floating */
		dntp = gpsntp_from_daytime1_ex(
			&date, tofs, rd_timestamp, warp);
	}

	if (debug) {
		/* debug print time stamp */
		gpsntp_to_calendar(&date, &dntp);
#	    ifdef HAVE_PPSAPI
		DPRINTF(1, ("%s effective timecode: %s (%s PPS)\n",
			    refnumtoa(&peer->srcadr),
			    ntpcal_iso8601std(NULL, 0, &date),
			    (withpps ? "with" : "without")));
#	    else /* ?HAVE_PPSAPI */
		DPRINTF(1, ("%s effective timecode: %s\n",
			    refnumtoa(&peer->srcadr),
			    ntpcal_iso8601std(NULL, 0, &date)));
#	    endif /* !HAVE_PPSAPI */
	}

	/* Get the reference time stamp from the calendar buffer.
	 * Process the new sample in the median filter and determine the
	 * timecode timestamp, but only if the PPS is not in control.
	 * Discard sentence if reference time did not change.
	 */
	rd_reftime = ntpfp_from_ntpdatum(&dntp);
	if (L_ISEQU(&up->last_reftime, &rd_reftime)) {
		/* Do not touch pp->a_lastcode on purpose! */
		up->tally.filtered++;
		return;
	}
	up->last_reftime = rd_reftime;

	DPRINTF(1, ("%s using '%s'\n",
		    refnumtoa(&peer->srcadr), up->lb_buf));

	/* Data will be accepted. Update stats & log data. */
	up->tally.accepted++;
	refclock_save_lcode(pp, up->lb_buf, up->lb_len);
	pp->lastrec = rd_timestamp;

	/* If we have PPS augmented receive time, we *must* have a
	 * working PPS source and we must set the flags accordingly.
	 */
#   ifdef HAVE_PPSAPI
	if (withpps) {
		up->ppsapi_gate = TRUE;
		peer->precision = PPS_PRECISION;
		if (tabsdiffd(rd_reftime, rd_timestamp) < 0.5) {
			if ( ! (peer->ttl & NMEA_QUIETPPS_MASK))
				peer->flags |= FLAG_PPS;
			DPRINTF(2, ("%s PPS_RELATE_PHASE\n",
				    refnumtoa(&peer->srcadr)));
			up->tally.pps_used++;
		} else {
			DPRINTF(2, ("%s PPS_RELATE_EDGE\n",
				    refnumtoa(&peer->srcadr)));
		}
		/* !Note! 'FLAG_PPS' is reset in 'nmea_poll()' */
	}
#   endif /* HAVE_PPSAPI */
	/* Whether the receive time stamp is PPS-augmented or not,
	 * the proper fudge offset is already applied. There's no
	 * residual fudge to process.
	 */
	refclock_process_offset(pp, rd_reftime, rd_timestamp, 0.0);
	up->rcvtout = 2;
}

/*
 * -------------------------------------------------------------------
 * nmea_receive - receive data from the serial interface
 *
 * With serial IO only, a single call to 'refclock_gtlin()' to get the
 * string would suffice to get the NMEA data. When using NMEAD, this
 * does unfortunately no longer hold, since TCP is stream oriented and
 * not line oriented, and there's no one to do the line-splitting work
 * of the TTY driver in line/cooked mode.
 *
 * So we have to do this manually here, and we have to live with the
 * fact that there could be more than one sentence in a receive buffer.
 * Likewise, there can be partial messages on either end. (Strictly
 * speaking, a receive buffer could also contain just a single fragment,
 * though that's unlikely.)
 *
 * We deal with that by scanning the input buffer, copying bytes from
 * the receive buffer to the assembly buffer as we go and calling the
 * record processor every time we hit a CR/LF, provided the resulting
 * line is not empty. Any leftovers are kept for the next round.
 *
 * Note: When used with a serial data stream, there's no change to the
 * previous line-oriented input: One line is copied to the buffer and
 * processed per call. Only with NMEAD the behavior changes, and the
 * timing is badly affected unless a PPS channel is also associated with
 * the clock instance. TCP leaves us nothing to improve on here.
 * -------------------------------------------------------------------
 */
static void
nmea_receive(
	struct recvbuf * rbufp
	)
{
	/* declare & init control structure pointers */
	struct peer	    * const peer = rbufp->recv_peer;
	struct refclockproc * const pp = peer->procptr;
	nmea_unit	    * const up = (nmea_unit*)pp->unitptr;

	const char *sp, *se;
	char	   *dp, *de;

	/* paranoia check: */
	if (up->lb_len >= sizeof(up->lb_buf))
		up->lb_len = 0;

	/* pick up last assembly position; leave room for NUL */
	dp = up->lb_buf + up->lb_len;
	de = up->lb_buf + sizeof(up->lb_buf) - 1;
	/* set up input range */
	sp = (const char *)rbufp->recv_buffer;
	se = sp + rbufp->recv_length;

	/* walk over the input data, dropping parity bits and control
	 * chars as we go, and calling the record processor for each
	 * complete non-empty line.
	 */
	while (sp != se) {
		char ch = (*sp++ & 0x7f);
		if (dp == up->lb_buf) {
			if (ch == '$')
				*dp++ = ch;
		} else if (dp > de) {
			dp = up->lb_buf;
		} else if (ch == '\n' || ch == '\r') {
			*dp = '\0';
			up->lb_len = (int)(dp - up->lb_buf);
			dp = up->lb_buf;
			nmea_procrec(peer, rbufp->recv_time);
		} else if (ch >= 0x20 && ch < 0x7f) {
			*dp++ = ch;
		}
	}
	/* update state to keep for next round */
	*dp = '\0';
	up->lb_len = (int)(dp - up->lb_buf);
}

/*
 * -------------------------------------------------------------------
 * nmea_poll - called by the transmit procedure
 *
 * Does the necessary bookkeeping stuff to keep the reported state of
 * the clock in sync with reality.
 *
 * We go to great pains to avoid changing state here, since there may
 * be more than one eavesdropper receiving the same timecode.
 * -------------------------------------------------------------------
 */
static void
nmea_poll(
	int           unit,
	struct peer * peer
	)
{
	struct refclockproc * const pp = peer->procptr;
	nmea_unit	    * const up = (nmea_unit *)pp->unitptr;

	/*
	 * Process median filter samples. If none received, declare a
	 * timeout and keep going.
	 */
#   ifdef HAVE_PPSAPI
	/*
	 * If we don't have PPS pulses and time stamps, turn PPS down
	 * for now.
	 */
	if (!up->ppsapi_gate) {
		peer->flags &= ~FLAG_PPS;
		peer->precision = PRECISION;
	} else {
		up->ppsapi_gate = FALSE;
	}
#   endif /* HAVE_PPSAPI */

	/*
	 * If the median filter is empty, claim a timeout. Else process
	 * the input data and keep the stats going.
	 */
	if (pp->coderecv == pp->codeproc) {
		peer->flags &= ~FLAG_PPS;
		if (pp->currentstatus < CEVNT_TIMEOUT)
		    refclock_report(peer, CEVNT_TIMEOUT);
		memset(&up->last_gpsdate, 0, sizeof(up->last_gpsdate));
	} else {
		pp->polls++;
		pp->lastref = pp->lastrec;
		refclock_receive(peer);
		if (pp->currentstatus > CEVNT_NOMINAL)
		    refclock_report(peer, CEVNT_NOMINAL);
	}

	/*
	 * If extended logging is required, write the tally stats to the
	 * clockstats file; otherwise just do a normal clock stats
	 * record. Clear the tally stats anyway.
	*/
	if (peer->ttl & NMEA_EXTLOG_MASK) {
		/* Log & reset counters with extended logging */
		const char *nmea = pp->a_lastcode;
		if (*nmea == '\0') nmea = "(none)";
		mprintf_clock_stats(
		  &peer->srcadr, "%s  %u %u %u %u %u %u",
		  nmea,
		  up->tally.total, up->tally.accepted,
		  up->tally.rejected, up->tally.malformed,
		  up->tally.filtered, up->tally.pps_used);
	} else {
		record_clock_stats(&peer->srcadr, pp->a_lastcode);
	}
	ZERO(up->tally);
}

#if NMEA_WRITE_SUPPORT
/*
 * -------------------------------------------------------------------
 *  gps_send(fd, cmd, peer)	Sends a command to the GPS receiver.
 *   as in gps_send(fd, "rqts,u", peer);
 *
 * If 'cmd' starts with a '$' it is assumed that this command is in raw
 * format, that is, starts with '$', ends with '<cr><lf>' and that any
 * checksum is correctly provided; the command will be send 'as is' in
 * that case. Otherwise the function will create the necessary frame
 * (start char, chksum, final CRLF) on the fly.
 *
 * We don't currently send any data, but would like to send RTCM SC104
 * messages for differential positioning. It should also give us better
 * time. Without a PPS output, we're Just fooling ourselves because of
 * the serial code paths
 * -------------------------------------------------------------------
 */
static void
gps_send(
	int           fd,
	const char  * cmd,
	struct peer * peer
	)
{
	/* $...*xy<CR><LF><NUL> add 7 */
	char	      buf[NMEA_PROTO_MAXLEN + 7];
	int	      len;
	u_char	      dcs;
	const u_char *beg, *end;

	if (*cmd != '$') {
		/* get checksum and length */
		beg = end = (const u_char*)cmd;
		dcs = 0;
		while (*end >= ' ' && *end != '*')
			dcs ^= *end++;
		len = end - beg;
		/* format into output buffer with overflow check */
		len = snprintf(buf, sizeof(buf), "$%.*s*%02X\r\n",
			       len, beg, dcs);
		if ((size_t)len >= sizeof(buf)) {
			DPRINTF(1, ("%s gps_send: buffer overflow for command '%s'\n",
				    refnumtoa(&peer->srcadr), cmd));
			return;	/* game over player 1 */
		}
		cmd = buf;
	} else {
		len = strlen(cmd);
	}

	DPRINTF(1, ("%s gps_send: '%.*s'\n", refnumtoa(&peer->srcadr),
		len - 2, cmd));

	/* send out the whole stuff */
	if (write(fd, cmd, len) == -1)
		refclock_report(peer, CEVNT_FAULT);
}
#endif /* NMEA_WRITE_SUPPORT */

/*
 * -------------------------------------------------------------------
 * helpers for faster field splitting
 * -------------------------------------------------------------------
 *
 * set up a field record, check syntax and verify checksum
 *
 * format is $XXXXX,1,2,3,4*ML
 *
 * 8-bit XOR of characters between $ and * noninclusive is transmitted
 * in last two chars M and L holding most and least significant nibbles
 * in hex representation such as:
 *
 *   $GPGLL,5057.970,N,00146.110,E,142451,A*27
 *   $GPVTG,089.0,T,,,15.2,N,,*7F
 *
 * Some other constraints:
 * + The field name must be at least 5 upcase characters or digits and
 *   must start with a character.
 * + The checksum (if present) must be uppercase hex digits.
 * + The length of a sentence is limited to 80 characters (not including
 *   the final CR/LF nor the checksum, but including the leading '$')
 *
 * Return values:
 *  + CHECK_INVALID
 *	The data does not form a valid NMEA sentence or a checksum error
 *	occurred.
 *  + CHECK_VALID
 *	The data is a valid NMEA sentence but contains no checksum.
 *  + CHECK_CSVALID
 *	The data is a valid NMEA sentence and passed the checksum test.
 * -------------------------------------------------------------------
 */
static int
field_init(
	nmea_data * data,	/* context structure		       */
	char 	  * cptr,	/* start of raw data		       */
	int	    dlen	/* data len, not counting trailing NUL */
	)
{
	u_char cs_l;	/* checksum local computed	*/
	u_char cs_r;	/* checksum remote given	*/
	char * eptr;	/* buffer end end pointer	*/
	char   tmp;	/* char buffer 			*/

	cs_l = 0;
	cs_r = 0;
	/* some basic input constraints */
	if (dlen < 0)
		dlen = 0;
	eptr = cptr + dlen;
	*eptr = '\0';

	/* load data context */
	data->base = cptr;
	data->cptr = cptr;
	data->cidx = 0;
	data->blen = dlen;

	/* syntax check follows here. check allowed character
	 * sequences, updating the local computed checksum as we go.
	 *
	 * regex equiv: '^\$[A-Z][A-Z0-9]{4,}[^*]*(\*[0-9A-F]{2})?$'
	 */

	/* -*- start character: '^\$' */
	if (*cptr == '\0')
		return CHECK_EMPTY;
	if (*cptr++ != '$')
		return CHECK_INVALID;

	/* -*- advance context beyond start character */
	data->base++;
	data->cptr++;
	data->blen--;

	/* -*- field name: '[A-Z][A-Z0-9]{4,},' */
	if (*cptr < 'A' || *cptr > 'Z')
		return CHECK_INVALID;
	cs_l ^= *cptr++;
	while ((*cptr >= 'A' && *cptr <= 'Z') ||
	       (*cptr >= '0' && *cptr <= '9')  )
		cs_l ^= *cptr++;
	if (*cptr != ',' || (cptr - data->base) < NMEA_PROTO_IDLEN)
		return CHECK_INVALID;
	cs_l ^= *cptr++;

	/* -*- data: '[^*]*' */
	while (*cptr && *cptr != '*')
		cs_l ^= *cptr++;

	/* -*- checksum field: (\*[0-9A-F]{2})?$ */
	if (*cptr == '\0')
		return CHECK_VALID;
	if (*cptr != '*' || cptr != eptr - 3 ||
	    (cptr - data->base) >= NMEA_PROTO_MAXLEN)
		return CHECK_INVALID;

	for (cptr++; (tmp = *cptr) != '\0'; cptr++) {
		if (tmp >= '0' && tmp <= '9')
			cs_r = (cs_r << 4) + (tmp - '0');
		else if (tmp >= 'A' && tmp <= 'F')
			cs_r = (cs_r << 4) + (tmp - 'A' + 10);
		else
			break;
	}

	/* -*- make sure we are at end of string and csum matches */
	if (cptr != eptr || cs_l != cs_r)
		return CHECK_INVALID;

	return CHECK_CSVALID;
}

/*
 * -------------------------------------------------------------------
 * fetch a data field by index, zero being the name field. If this
 * function is called repeatedly with increasing indices, the total load
 * is O(n), n being the length of the string; if it is called with
 * decreasing indices, the total load is O(n^2). Try not to go backwards
 * too often.
 * -------------------------------------------------------------------
 */
static char *
field_parse(
	nmea_data * data,
	int 	    fn
	)
{
	char tmp;

	if (fn < data->cidx) {
		data->cidx = 0;
		data->cptr = data->base;
	}
	while ((fn > data->cidx) && (tmp = *data->cptr) != '\0') {
		data->cidx += (tmp == ',');
		data->cptr++;
	}
	return data->cptr;
}

/*
 * -------------------------------------------------------------------
 * Wipe (that is, overwrite with '_') data fields and the checksum in
 * the last timecode.  The list of field indices is given as integers
 * in a varargs list, preferably in ascending order, in any case
 * terminated by a negative field index.
 *
 * A maximum number of 8 fields can be overwritten at once to guard
 * against runaway (that is, unterminated) argument lists.
 *
 * This function affects what a remote user can see with
 *
 * ntpq -c clockvar <server>
 *
 * Note that this also removes the wiped fields from any clockstats
 * log.	 Some NTP operators monitor their NMEA GPS using the change in
 * location in clockstats over time as as a proxy for the quality of
 * GPS reception and thereby time reported.
 * -------------------------------------------------------------------
 */
static void
field_wipe(
	nmea_data * data,
	...
	)
{
	va_list	va;		/* vararg index list */
	int	fcnt;		/* safeguard against runaway arglist */
	int	fidx;		/* field to nuke, or -1 for checksum */
	char  * cp;		/* overwrite destination */

	fcnt = 8;
	cp = NULL;
	va_start(va, data);
	do {
		fidx = va_arg(va, int);
		if (fidx >= 0 && fidx <= NMEA_PROTO_FIELDS) {
			cp = field_parse(data, fidx);
		} else {
			cp = data->base + data->blen;
			if (data->blen >= 3 && cp[-3] == '*')
				cp -= 2;
		}
		for ( ; '\0' != *cp && '*' != *cp && ',' != *cp; cp++)
			if ('.' != *cp)
				*cp = '_';
	} while (fcnt-- && fidx >= 0);
	va_end(va);
}

/*
 * -------------------------------------------------------------------
 * PARSING HELPERS
 * -------------------------------------------------------------------
 */
typedef unsigned char const UCC;

static char const * const s_eof_chars = ",*\r\n";

static int field_length(UCC *cp, unsigned int nfields)
{
	char const * ep = (char const*)cp;
	ep = strpbrk(ep, s_eof_chars);
	if (ep && nfields)
		while (--nfields && ep && *ep == ',')
			ep = strpbrk(ep + 1, s_eof_chars);
	return (ep)
	    ? (int)((UCC*)ep - cp)
	    : (int)strlen((char const*)cp);
}

/* /[,*\r\n]/ --> skip */
static int _parse_eof(UCC *cp, UCC ** ep)
{
	int rc = (strchr(s_eof_chars, *(char const*)cp) != NULL);
	*ep = cp + rc;
	return rc;
}

/* /,/ --> skip */
static int _parse_sep(UCC *cp, UCC ** ep)
{
	int rc = (*cp == ',');
	*ep = cp + rc;
	return rc;
}

/* /[[:digit:]]{2}/ --> uint16_t */
static int _parse_num2d(UCC *cp, UCC ** ep, uint16_t *into)
{
	int	rc = FALSE;

	if (isdigit(cp[0]) && isdigit(cp[1])) {
		*into = (cp[0] - '0') * 10 + (cp[1] - '0');
		cp += 2;
		rc = TRUE;
	}
	*ep = cp;
	return rc;
}

/* /[[:digit:]]+/ --> uint16_t */
static int _parse_u16(UCC *cp, UCC **ep, uint16_t *into, unsigned int ndig)
{
	uint16_t	num = 0;
	int		rc  = FALSE;
	if (isdigit(*cp) && ndig) {
		rc = TRUE;
		do
			num = (num * 10) + (*cp - '0');
		while (isdigit(*++cp) && --ndig);
		*into = num;
	}
	*ep = cp;
	return rc;
}

/* /[[:digit:]]+/ --> uint32_t */
static int _parse_u32(UCC *cp, UCC **ep, uint32_t *into, unsigned int ndig)
{
	uint32_t	num = 0;
	int		rc  = FALSE;
	if (isdigit(*cp) && ndig) {
		rc = TRUE;
		do
			num = (num * 10) + (*cp - '0');
		while (isdigit(*++cp) && --ndig);
		*into = num;
	}
	*ep = cp;
	return rc;
}

/* /(\.[[:digit:]]*)?/ --> l_fp{0, f}
 * read fractional seconds, convert to l_fp
 *
 * Only the first 9 decimal digits are evaluated; any excess is parsed
 * away but silently ignored. (--> truncation to 1 nanosecond)
 */
static int _parse_frac(UCC *cp, UCC **ep, l_fp *into)
{
	static const uint32_t powtab[10] = {
		        0,
		100000000, 10000000, 1000000,
		   100000,    10000,    1000,
		      100,       10,       1
	};

	struct timespec	ts;
	ZERO(ts);
	if (*cp == '.') {
		uint32_t fval = 0;
		UCC *    sp   = cp + 1;
		if (_parse_u32(sp, &cp, &fval, 9))
			ts.tv_nsec = fval * powtab[(size_t)(cp - sp)];
		while (isdigit(*cp))
			++cp;
	}

	*ep   = cp;
	*into = tspec_intv_to_lfp(ts);
	return TRUE;
}

/* /[[:digit:]]{6}/ --> time-of-day
 * parses a number string representing 'HHMMSS'
 */
static int _parse_time(UCC *cp, UCC ** ep, TCivilDate *into)
{
	uint16_t	s, m, h;
	int		rc;
	UCC *		xp = cp;

	rc =   _parse_num2d(cp, &cp, &h) && (h < 24)
	    && _parse_num2d(cp, &cp, &m) && (m < 60)
	    && _parse_num2d(cp, &cp, &s) && (s < 61); /* leap seconds! */

	if (rc) {
		into->hour   = (uint8_t)h;
		into->minute = (uint8_t)m;
		into->second = (uint8_t)s;
		*ep = cp;
	} else {
		*ep = xp;
		DPRINTF(1, ("nmea: invalid time code: '%.*s'\n",
			    field_length(xp, 1), xp));
	}
	return rc;
}

/* /[[:digit:]]{6}/ --> civil date
 * parses a number string representing 'ddmmyy'
 */
static int _parse_date1(UCC *cp, UCC **ep, TCivilDate *into)
{
	unsigned short	d, m, y;
	int		rc;
	UCC *		xp = cp;

	rc =   _parse_num2d(cp, &cp, &d) && (d - 1 < 31)
	    && _parse_num2d(cp, &cp, &m) && (m - 1 < 12)
	    && _parse_num2d(cp, &cp, &y)
	    && _parse_eof(cp, ep);
	if (rc) {
		into->monthday = (uint8_t )d;
		into->month    = (uint8_t )m;
		into->year     = (uint16_t)y;
		*ep = cp;
	} else {
		*ep = xp;
		DPRINTF(1, ("nmea: invalid date code: '%.*s'\n",
			    field_length(xp, 1), xp));
	}
	return rc;
}

/* /[[:digit:]]+,[[:digit:]]+,[[:digit:]]+/ --> civil date
 * parses three successive numeric fields as date: day,month,year
 */
static int _parse_date3(UCC *cp, UCC **ep, TCivilDate *into)
{
	uint16_t	d, m, y;
	int		rc;
	UCC *		xp = cp;

	rc =   _parse_u16(cp, &cp, &d, 2) && (d - 1 < 31)
	    && _parse_sep(cp, &cp)
	    && _parse_u16(cp, &cp, &m, 2) && (m - 1 < 12)
	    && _parse_sep(cp, &cp)
	    && _parse_u16(cp, &cp, &y, 4) && (y > 1980)
	    && _parse_eof(cp, ep);
	if (rc) {
		into->monthday = (uint8_t )d;
		into->month    = (uint8_t )m;
		into->year     = (uint16_t)y;
		*ep = cp;
	} else {
		*ep = xp;
		DPRINTF(1, ("nmea: invalid date code: '%.*s'\n",
			    field_length(xp, 3), xp));
	}
	return rc;
}

/*
 * -------------------------------------------------------------------
 * Check sync status
 *
 * If the character at the data field start matches the tag value,
 * return LEAP_NOWARNING and LEAP_NOTINSYNC otherwise. If the 'inverted'
 * flag is given, just the opposite value is returned. If there is no
 * data field (*cp points to the NUL byte) the result is LEAP_NOTINSYNC.
 * -------------------------------------------------------------------
 */
static u_char
parse_qual(
	nmea_data * rd,
	int         idx,
	char        tag,
	int         inv
	)
{
	static const u_char table[2] = {
		LEAP_NOTINSYNC, LEAP_NOWARNING };

	char * dp = field_parse(rd, idx);

	return table[ *dp && ((*dp == tag) == !inv) ];
}

/*
 * -------------------------------------------------------------------
 * Parse a time stamp in HHMMSS[.sss] format with error checking.
 *
 * returns 1 on success, 0 on failure
 * -------------------------------------------------------------------
 */
static int
parse_time(
	struct calendar * jd,	/* result calendar pointer */
	l_fp		* fofs,	/* storage for nsec fraction */
	nmea_data       * rd,
	int		  idx
	)
{
	UCC * 	dp = (UCC*)field_parse(rd, idx);

	return _parse_time(dp, &dp, jd)
	    && _parse_frac(dp, &dp, fofs)
	    && _parse_eof (dp, &dp);
}

/*
 * -------------------------------------------------------------------
 * Parse a date string from an NMEA sentence. This could either be a
 * partial date in DDMMYY format in one field, or DD,MM,YYYY full date
 * spec spanning three fields. This function does some extensive error
 * checking to make sure the date string was consistent.
 *
 * returns 1 on success, 0 on failure
 * -------------------------------------------------------------------
 */
static int
parse_date(
	struct calendar * jd,	/* result pointer */
	nmea_data       * rd,
	int		  idx,
	enum date_fmt	  fmt
	)
{
	UCC  * dp = (UCC*)field_parse(rd, idx);

	switch (fmt) {
	case DATE_1_DDMMYY:
		return _parse_date1(dp, &dp, jd);
	case DATE_3_DDMMYYYY:
		return _parse_date3(dp, &dp, jd);
	default:
		DPRINTF(1, ("nmea: invalid parse format: %d\n", fmt));
		break;
	}
	return FALSE;
}

/*
 * -------------------------------------------------------------------
 * Parse GPS week time info from an NMEA sentence. This info contains
 * the GPS week number, the GPS time-of-week and the leap seconds GPS
 * to UTC.
 *
 * returns 1 on success, 0 on failure
 * -------------------------------------------------------------------
 */
static int
parse_gpsw(
	TGpsDatum *  wd,
	nmea_data *  rd,
	int          weekidx,
	int          timeidx,
	int          leapidx
	)
{
	uint32_t	secs;
	uint16_t	week, leap = 0;
	l_fp		fofs;
	int		rc;

	UCC *	dpw = (UCC*)field_parse(rd, weekidx);
	UCC *	dps = (UCC*)field_parse(rd, timeidx);

	rc =   _parse_u16 (dpw, &dpw, &week, 5)
	    && _parse_eof (dpw, &dpw)
	    && _parse_u32 (dps, &dps, &secs, 9)
	    && _parse_frac(dps, &dps, &fofs)
	    && _parse_eof (dps, &dps)
	    && (secs < 7*SECSPERDAY);
	if (rc && leapidx > 0) {
		UCC *	dpl = (UCC*)field_parse(rd, leapidx);
		rc =   _parse_u16 (dpl, &dpl, &leap, 5)
		    && _parse_eof (dpl, &dpl);
	}
	if (rc) {
		fofs.l_ui -= leap;
		*wd = gpscal_from_gpsweek(week, secs, fofs);
	} else {
		DPRINTF(1, ("nmea: parse_gpsw: invalid weektime spec\n"));
	}
	return rc;
}


#ifdef HAVE_PPSAPI
static double
tabsdiffd(
	l_fp	t1,
	l_fp	t2
	)
{
	double	dd;
	L_SUB(&t1, &t2);
	LFPTOD(&t1, dd);
	return fabs(dd);
}
#endif /* HAVE_PPSAPI */

/*
 * ===================================================================
 *
 * NMEAD support
 *
 * original nmead support added by Jon Miner (cp_n18@yahoo.com)
 *
 * See http://home.hiwaay.net/~taylorc/gps/nmea-server/
 * for information about nmead
 *
 * To use this, you need to create a link from /dev/gpsX to
 * the server:port where nmead is running.  Something like this:
 *
 * ln -s server:port /dev/gps1
 *
 * Split into separate function by Juergen Perlinger
 * (perlinger-at-ntp-dot-org)
 *
 * ===================================================================
 */
static int
nmead_open(
	const char * device
	)
{
	int	fd = -1;		/* result file descriptor */

#   ifdef HAVE_READLINK
	char	host[80];		/* link target buffer	*/
	char  * port;			/* port name or number	*/
	int	rc;			/* result code (several)*/
	int     sh;			/* socket handle	*/
	struct addrinfo	 ai_hint;	/* resolution hint	*/
	struct addrinfo	*ai_list;	/* resolution result	*/
	struct addrinfo *ai;		/* result scan ptr	*/

	fd = -1;

	/* try to read as link, make sure no overflow occurs */
	rc = readlink(device, host, sizeof(host));
	if ((size_t)rc >= sizeof(host))
		return fd;	/* error / overflow / truncation */
	host[rc] = '\0';	/* readlink does not place NUL	*/

	/* get port */
	port = strchr(host, ':');
	if (!port)
		return fd; /* not 'host:port' syntax ? */
	*port++ = '\0';	/* put in separator */

	/* get address infos and try to open socket
	 *
	 * This getaddrinfo() is naughty in ntpd's nonblocking main
	 * thread, but you have to go out of your wary to use this code
	 * and typically the blocking is at startup where its impact is
	 * reduced. The same holds for the 'connect()', as it is
	 * blocking, too...
	 */
	ZERO(ai_hint);
	ai_hint.ai_protocol = IPPROTO_TCP;
	ai_hint.ai_socktype = SOCK_STREAM;
	if (getaddrinfo(host, port, &ai_hint, &ai_list))
		return fd;

	for (ai = ai_list; ai && (fd == -1); ai = ai->ai_next) {
		sh = socket(ai->ai_family, ai->ai_socktype,
			    ai->ai_protocol);
		if (INVALID_SOCKET == sh)
			continue;
		rc = connect(sh, ai->ai_addr, ai->ai_addrlen);
		if (-1 != rc)
			fd = sh;
		else
			close(sh);
	}
	freeaddrinfo(ai_list);
	if (fd != -1)
		make_socket_nonblocking(fd);
#   else
	fd = -1;
#   endif

	return fd;
}
#else
NONEMPTY_TRANSLATION_UNIT
#endif /* REFCLOCK && CLOCK_NMEA */
