/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.ORG> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * refclock_oncore.c
 *
 * Driver for some of the various the Motorola Oncore GPS receivers.
 *   should work with Basic, PVT6, VP, UT, UT+, GT, GT+, SL, M12.
 *	The receivers with TRAIM (VP, UT, UT+), will be more accurate than the others.
 *	The receivers without position hold (GT, GT+) will be less accurate.
 *
 * Tested with:
 *
 *		(UT)				   (VP)
 *   COPYRIGHT 1991-1997 MOTOROLA INC.	COPYRIGHT 1991-1996 MOTOROLA INC.
 *   SFTW P/N #     98-P36848P		SFTW P/N # 98-P36830P
 *   SOFTWARE VER # 2			SOFTWARE VER # 8
 *   SOFTWARE REV # 2			SOFTWARE REV # 8
 *   SOFTWARE DATE  APR 24 1998 	SOFTWARE DATE  06 Aug 1996
 *   MODEL #	R1121N1114		MODEL #    B4121P1155
 *   HWDR P/N # 1			HDWR P/N # _
 *   SERIAL #	R0010A			SERIAL #   SSG0226478
 *   MANUFACTUR DATE 6H07		MANUFACTUR DATE 7E02
 *					OPTIONS LIST	IB
 *
 *	      (Basic)				   (M12)
 *   COPYRIGHT 1991-1996 MOTOROLA INC.	COPYRIGHT 1991-2000 MOTOROLA INC.
 *   SFTW P/N # 98-P36830P		SFTW P/N # 61-G10002A
 *   SOFTWARE VER # 8			SOFTWARE VER # 1
 *   SOFTWARE REV # 8			SOFTWARE REV # 3
 *   SOFTWARE DATE  06 Aug 1996 	SOFTWARE DATE  Mar 13 2000
 *   MODEL #	B4121P1155		MODEL #    P143T12NR1
 *   HDWR P/N # _			HWDR P/N # 1
 *   SERIAL #	SSG0226478		SERIAL #   P003UD
 *   MANUFACTUR DATE 7E02		MANUFACTUR DATE 0C27
 *   OPTIONS LIST    IB
 *
 * --------------------------------------------------------------------------
 * This code uses the two devices
 *	/dev/oncore.serial.n
 *	/dev/oncore.pps.n
 * which may be linked to the same device.
 * and can read initialization data from the file
 *	/etc/ntp.oncoreN, /etc/ntp.oncore.N, or /etc/ntp.oncore, where
 *	n or N are the unit number, viz 127.127.30.N.
 * --------------------------------------------------------------------------
 * Reg.Clemens <reg@dwf.com> Sep98.
 *  Original code written for FreeBSD.
 *  With these mods it works on FreeBSD, SunOS, Solaris and Linux
 *    (SunOS 4.1.3 + ppsclock)
 *    (Solaris7 + MU4)
 *    (RedHat 5.1 2.0.35 + PPSKit, 2.1.126 + or later).
 *
 *  Lat,Long,Ht, cable-delay, offset, and the ReceiverID (along with the
 *  state machine state) are printed to CLOCKSTATS if that file is enabled
 *  in /etc/ntp.conf.
 *
 * --------------------------------------------------------------------------
 *
 * According to the ONCORE manual (TRM0003, Rev 3.2, June 1998, page 3.13)
 * doing an average of 10000 valid 2D and 3D fixes is what the automatic
 * site survey mode does.  Looking at the output from the receiver
 * it seems like it is only using 3D fixes.
 * When we do it ourselves, take 10000 3D fixes.
 */

#define POS_HOLD_AVERAGE	10000	/* nb, 10000s ~= 2h45m */

/*
 * ONCORE_SHMEM_STATUS will create a mmap(2)'ed file named according to a
 * "STATUS" line in the oncore config file, which contains the most recent
 * copy of all types of messages we recognize.	This file can be mmap(2)'ed
 * by monitoring and statistics programs.
 *
 * See separate HTML documentation for this option.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#if defined(REFCLOCK) && defined(CLOCK_ONCORE) && defined(HAVE_PPSAPI)

#include "ntpd.h"
#include "ntp_io.h"
#include "ntp_unixtime.h"
#include "ntp_refclock.h"
#include "ntp_stdlib.h"

#include <stdio.h>
#include <ctype.h>
#include <sys/stat.h>
#ifdef ONCORE_SHMEM_STATUS
# ifdef HAVE_SYS_MMAN_H
#  include <sys/mman.h>
#  ifndef MAP_FAILED
#   define MAP_FAILED ((u_char *) -1)
#  endif  /* not MAP_FAILED */
# endif /* HAVE_SYS_MMAN_H */
#endif /* ONCORE_SHMEM_STATUS */

#ifdef HAVE_PPSAPI
# ifdef HAVE_TIMEPPS_H
#  include <timepps.h>
# else
#  ifdef HAVE_SYS_TIMEPPS_H
#   include <sys/timepps.h>
#  endif
# endif
#endif

#ifdef HAVE_SYS_SIO_H
# include <sys/sio.h>
#endif

#ifdef HAVE_SYS_TERMIOS_H
# include <sys/termios.h>
#endif

#ifdef HAVE_SYS_PPSCLOCK_H
# include <sys/ppsclock.h>
#endif

#ifndef HAVE_STRUCT_PPSCLOCKEV
struct ppsclockev {
# ifdef HAVE_STRUCT_TIMESPEC
	struct timespec tv;
# else
	struct timeval tv;
# endif
	u_int serial;
};
#endif /* not HAVE_STRUCT_PPSCLOCKEV */

enum receive_state {
	ONCORE_NO_IDEA,
	ONCORE_ID_SENT,
	ONCORE_RESET_SENT,
	ONCORE_TEST_SENT,
	ONCORE_INIT,
	ONCORE_ALMANAC,
	ONCORE_RUN
};

enum site_survey_state {
	ONCORE_SS_UNKNOWN,
	ONCORE_SS_TESTING,
	ONCORE_SS_HW,
	ONCORE_SS_SW,
	ONCORE_SS_DONE
};

/* Model Name, derived from the @@Cj message.
 * Used to initialize some variables.
 */

enum oncore_model {
	ONCORE_BASIC,
	ONCORE_PVT6,
	ONCORE_VP,
	ONCORE_UT,
	ONCORE_UTPLUS,
	ONCORE_GT,
	ONCORE_GTPLUS,
	ONCORE_SL,
	ONCORE_M12,
	ONCORE_UNKNOWN
};

/* the bits that describe these properties are in the same place
 * on the VP/UT, but have moved on the M12.  As such we extract
 * them, and use them from this struct.
 *
 */

struct RSM {
	u_char	posn0D;
	u_char	posn2D;
	u_char	posn3D;
	u_char	bad_almanac;
	u_char	bad_fix;
};

/* It is possible to test the VP/UT each cycle (@@Ea or equivalent) to
 * see what mode it is in.  The bits on the M12 are multiplexed with
 * other messages, so we have to 'keep' the last known mode here.
 */

enum posn_mode {
	MODE_UNKNOWN,
	MODE_0D,
	MODE_2D,
	MODE_3D
};

struct instance {
	int	unit;		/* 127.127.30.unit */
	struct	refclockproc *pp;
	struct	peer *peer;

	int	ttyfd;		/* TTY file descriptor */
	int	ppsfd;		/* PPS file descriptor */
	int	statusfd;	/* Status shm descriptor */
#ifdef HAVE_PPSAPI
	pps_handle_t pps_h;
	pps_params_t pps_p;
#endif
	enum receive_state o_state;		/* Receive state */
	enum posn_mode mode;			/* 0D, 2D, 3D */
	enum site_survey_state site_survey;	/* Site Survey state */

	int	Bj_day;

	u_long	delay;		/* ns */
	long	offset; 	/* ns */

	u_char	*shmem;
	char	*shmem_fname;
	u_int	shmem_Cb;
	u_int	shmem_Ba;
	u_int	shmem_Ea;
	u_int	shmem_Ha;
	u_char	shmem_first;
	u_char	shmem_reset;
	u_char	shmem_Posn;

	double	ss_lat;
	double	ss_long;
	double	ss_ht;
	double	dH;
	int	ss_count;
	u_char	posn_set;

	enum oncore_model model;
	u_int	version;
	u_int	revision;

	u_char	chan;		/* 6 for PVT6 or BASIC, 8 for UT/VP, 12 for m12, 0 if unknown */
	s_char	traim;		/* do we have traim? yes UT/VP, no BASIC, GT, -1 unknown, 0 no, +1 yes */
	u_char	traim_delay;	/* seconds counter, waiting for reply */

	struct	RSM rsm;	/* bits extracted from Receiver Status Msg in @@Ea */
	u_char	printed;
	u_char	polled;
	int	pollcnt;
	u_int	ev_serial;
	int	Rcvptr;
	u_char	Rcvbuf[500];
	u_char	Ea[160];	/* Ba, Ea or Ha */
	u_char	En[70]; 	/* Bn or En */
	u_char	Cj[300];
	u_char	As;
	u_char	Ay;
	u_char	Az;
	u_char	have_dH;
	u_char	init_type;
	s_char	saw_tooth;
	u_int	timeout;	/* count to retry Cj after Fa self-test */
	u_char	count;		/* cycles thru Ea before starting */
	s_char	assert;
	u_int	saw_At;
};

#define rcvbuf	instance->Rcvbuf
#define rcvptr	instance->Rcvptr

static	void	oncore_consume	     P((struct instance *));
static	void	oncore_poll	     P((int, struct peer *));
static	void	oncore_read_config   P((struct instance *));
static	void	oncore_receive	     P((struct recvbuf *));
static	void	oncore_sendmsg	     P((int fd, u_char *, size_t));
static	void	oncore_shutdown      P((int, struct peer *));
static	int	oncore_start	     P((int, struct peer *));
static	void	oncore_get_timestamp P((struct instance *, long, long));
static	void	oncore_init_shmem    P((struct instance *));
static	void	oncore_print_As      P((struct instance *));

static	void	oncore_msg_any	   P((struct instance *, u_char *, size_t, int));
static	void	oncore_msg_As	   P((struct instance *, u_char *, size_t));
static	void	oncore_msg_At	   P((struct instance *, u_char *, size_t));
static	void	oncore_msg_Ay	   P((struct instance *, u_char *, size_t));
static	void	oncore_msg_Az	   P((struct instance *, u_char *, size_t));
static	void	oncore_msg_BaEaHa  P((struct instance *, u_char *, size_t));
static	void	oncore_msg_Bj	   P((struct instance *, u_char *, size_t));
static	void	oncore_msg_BnEn    P((struct instance *, u_char *, size_t));
static	void	oncore_msg_CaFaIa  P((struct instance *, u_char *, size_t));
static	void	oncore_msg_Cb	   P((struct instance *, u_char *, size_t));
static	void	oncore_msg_Cf	   P((struct instance *, u_char *, size_t));
static	void	oncore_msg_Cj	   P((struct instance *, u_char *, size_t));
static	void	oncore_msg_Cj_id   P((struct instance *, u_char *, size_t));
static	void	oncore_msg_Cj_init P((struct instance *, u_char *, size_t));
static	void	oncore_msg_Gj	   P((struct instance *, u_char *, size_t));
static	void	oncore_msg_Sz	   P((struct instance *, u_char *, size_t));

struct	refclock refclock_oncore = {
	oncore_start,		/* start up driver */
	oncore_shutdown,	/* shut down driver */
	oncore_poll,		/* transmit poll message */
	noentry,		/* not used */
	noentry,		/* not used */
	noentry,		/* not used */
	NOFLAGS 		/* not used */
};

/*
 * Understanding the next bit here is not easy unless you have a manual
 * for the the various Oncore Models.
 */

static struct msg_desc {
	const char	flag[3];
	const int	len;
	void		(*handler) P((struct instance *, u_char *, size_t));
	const char	*fmt;
	int		shmem;
} oncore_messages[] = {
			/* Ea and En first since they're most common */
	{ "Ea",  76,    oncore_msg_BaEaHa, "mdyyhmsffffaaaaoooohhhhmmmmvvhhddtntimsdimsdimsdimsdimsdimsdimsdimsdsC" },
	{ "Ba",  68,    oncore_msg_BaEaHa, "mdyyhmsffffaaaaoooohhhhmmmmvvhhddtntimsdimsdimsdimsdimsdimsdsC" },
	{ "Ha", 154,    oncore_msg_BaEaHa, "mdyyhmsffffaaaaoooohhhhmmmmaaaaoooohhhhmmmmVVvvhhddntimsiddimsiddimsiddimsiddimsiddimsiddimsiddimsiddimsiddimsiddimsiddimsiddssrrccooooTTushmvvvvvvC" },
	{ "En",  69,    oncore_msg_BnEn,   "otaapxxxxxxxxxxpysreensffffsffffsffffsffffsffffsffffsffffsffffC" },
	{ "Bn",  59,    oncore_msg_BnEn,   "otaapxxxxxxxxxxpysreensffffsffffsffffsffffsffffsffffC" },
	{ "Ab",  10,    0,                 "" },
	{ "Ac",  11,    0,                 "" },
	{ "Ad",  11,    0,                 "" },
	{ "Ae",  11,    0,                 "" },
	{ "Af",  15,    0,                 "" },
	{ "As",  20,    oncore_msg_As,     "" },
	{ "At",   8,    oncore_msg_At,     "" },
	{ "Au",  12,    0,                 "" },
	{ "Av",   8,    0,                 "" },
	{ "Aw",   8,    0,                 "" },
	{ "Ay",  11,    oncore_msg_Ay,     "" },
	{ "Az",  11,    oncore_msg_Az,     "" },
	{ "AB",   8,    0,                 "" },
	{ "Bb",  92,    0,                 "" },
	{ "Bj",   8,    oncore_msg_Bj,     "" },
	{ "Ca",   9,    oncore_msg_CaFaIa, "" },
	{ "Cb",  33,    oncore_msg_Cb,     "" },
	{ "Cf",   7,    oncore_msg_Cf,     "" },
	{ "Cg",   8,    0,                 "" },
	{ "Ch",   9,    0,                 "" },
	{ "Cj", 294,    oncore_msg_Cj,     "" },
	{ "Ek",  71,    0,                 "" },
	{ "Fa",   9,    oncore_msg_CaFaIa, "" },
	{ "Gd",   8,    0,                 "" },
	{ "Gj",  21,    oncore_msg_Gj,     "" },
	{ "Ia",  10,    oncore_msg_CaFaIa, "" },
	{ "Sz",   8,    oncore_msg_Sz,     "" },
	{ {0},	  7,	0,		   "" }
};

/*
 * Position Set.
 */
u_char oncore_cmd_Ad[] = { 'A', 'd', 0,0,0,0 };
u_char oncore_cmd_Ae[] = { 'A', 'e', 0,0,0,0 };
u_char oncore_cmd_Af[] = { 'A', 'f', 0,0,0,0, 0 };
u_char oncore_cmd_Ga[] = { 'G', 'a', 0,0,0,0, 0,0,0,0, 0,0,0,0, 0 };

/*
 * Position-Hold Mode
 *    Start automatic site survey
 */
static u_char oncore_cmd_At0[] = { 'A', 't', 0 };	/* Posn Hold off */
static u_char oncore_cmd_At1[] = { 'A', 't', 1 };	/* Posn Hold on  */
static u_char oncore_cmd_At2[] = { 'A', 't', 2 };	/* Start Site Survey */

/*
 * 0D/2D Position and Set.
 */
u_char oncore_cmd_As[] = { 'A', 's', 0,0,0,0, 0,0,0,0, 0,0,0,0, 0 };
u_char oncore_cmd_Asx[]= { 'A', 's', 0x7f, 0xff, 0xff, 0xff,
				     0x7f, 0xff, 0xff, 0xff,
				     0x7f, 0xff, 0xff, 0xff, 0xff };
u_char oncore_cmd_Au[] = { 'A', 'u', 0,0,0,0,0 };

u_char oncore_cmd_Av0[] = { 'A', 'v', 0 };
u_char oncore_cmd_Av1[] = { 'A', 'v', 1 };

u_char oncore_cmd_Gd0[] = { 'G', 'd', 0 };	/* 3D */
u_char oncore_cmd_Gd1[] = { 'G', 'd', 1 };	/* 0D */
u_char oncore_cmd_Gd2[] = { 'G', 'd', 2 };	/* 2D */

/*
 * Set to UTC time (not GPS).
 */
u_char oncore_cmd_Aw[] = { 'A', 'w', 1 };

/*
 * Output Almanac when it changes
 */
u_char oncore_cmd_Be[] = { 'B', 'e', 1 };

/*
 * Read back PPS Offset for Output
 */
u_char oncore_cmd_Ay[]	= { 'A', 'y', 0, 0, 0, 0 };
u_char oncore_cmd_Ayx[] = { 'A', 'y', 0xff, 0xff, 0xff, 0xff };

/*
 * Read back Cable Delay for Output
 */
u_char oncore_cmd_Az[]	= { 'A', 'z', 0, 0, 0, 0 };
u_char oncore_cmd_Azx[] = { 'A', 'z', 0xff, 0xff, 0xff, 0xff };

/*
 * Application type = static.
 */
u_char oncore_cmd_AB[] = { 'A', 'B', 4 };

/*
 * Visible Satellite Status Msg.
 */
u_char oncore_cmd_Bb[] = { 'B', 'b', 1 };

/*
 * Leap Second Pending Message
 *    Request message once
 */
u_char oncore_cmd_Bj[] = { 'B', 'j', 0 };
u_char oncore_cmd_Gj[] = { 'G', 'j' };

/*
 * Set to Defaults
 */
static u_char oncore_cmd_Cf[] = { 'C', 'f' };

/*
 * Set to Position Fix mode (only needed on VP).
 */
u_char oncore_cmd_Cg[] = { 'C', 'g', 1 };

/*
 * Receiver Id
 */
static u_char oncore_cmd_Cj[] = { 'C', 'j' };

/*
 * Position/Status/Data message
 *    Send once per second
 */
static u_char oncore_cmd_Ea[]  = { 'E', 'a', 1 };
static u_char oncore_cmd_Ba[]  = { 'B', 'a', 1 };
static u_char oncore_cmd_Ha[]  = { 'H', 'a', 1 };
static u_char oncore_cmd_Ea0[] = { 'E', 'a', 0 };
static u_char oncore_cmd_Ba0[] = { 'B', 'a', 0 };

/*
 * Position/Status Extension Msg
 */
u_char oncore_cmd_Ek[] = { 'E', 'k', 0 };	/* just turn off */

/*
 * Time Raim Setup & Status Message
 *    Send once per second
 *    Time-RAIM on
 *    Alarm limit 1us
 *    PPS on when we have the first sat
 */
static u_char oncore_cmd_En[]  = { 'E', 'n', 1, 1, 0,10, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static u_char oncore_cmd_En0[] = { 'E', 'n', 0, 1, 0,10, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static u_char oncore_cmd_Bn[]  = { 'B', 'n', 1, 1, 0,10, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static u_char oncore_cmd_Bn0[] = { 'B', 'n', 0, 1, 0,10, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

/*
 * Self-test
 */
static u_char oncore_cmd_Ca[] = { 'C', 'a' };	/*  6 Chan */
static u_char oncore_cmd_Fa[] = { 'F', 'a' };	/*  8 Chan */
static u_char oncore_cmd_Ia[] = { 'I', 'a' };	/* 12 Chan */

#define DEVICE1 	"/dev/oncore.serial.%d"   /* name of serial device */
#define DEVICE2 	"/dev/oncore.pps.%d"   /* name of pps device */
#define INIT_FILE	"/etc/ntp.oncore" /* optional init file */

#define SPEED		B9600		/* Oncore Binary speed (9600 bps) */

/*
 * Assemble and disassemble 32bit signed quantities from a buffer.
 *
 */

	/* to buffer, int w, u_char *buf */
#define w32_buf(buf,w)	{ u_int i_tmp;			   \
			  i_tmp = (w<0) ? (~(-w)+1) : (w); \
			  (buf)[0] = (i_tmp >> 24) & 0xff; \
			  (buf)[1] = (i_tmp >> 16) & 0xff; \
			  (buf)[2] = (i_tmp >>	8) & 0xff; \
			  (buf)[3] = (i_tmp	 ) & 0xff; \
			}

#define w32(buf)      (((buf)[0]&0xff) << 24 | \
		       ((buf)[1]&0xff) << 16 | \
		       ((buf)[2]&0xff) <<  8 | \
		       ((buf)[3]&0xff) )

	/* from buffer, char *buf, result to an int */
#define buf_w32(buf) (((buf)[0]&0200) ? (-(~w32(buf)+1)) : w32(buf))

extern int pps_assert;
extern int pps_hardpps;



/*
 * oncore_start - initialize data for processing
 */

static int
oncore_start(
	int unit,
	struct peer *peer
	)
{
	register struct instance *instance;
	struct refclockproc *pp;
	int fd1, fd2, mode;
	char device1[30], device2[30];
	const char *cp;
	struct stat stat1, stat2;

	/* OPEN DEVICES */
	/* opening different devices for fd1 and fd2 presents no problems */
	/* opening the SAME device twice, seems to be OS dependent.
		(a) on Linux (no streams) no problem
		(b) on SunOS (and possibly Solaris, untested), (streams)
			never see the line discipline.
	   Since things ALWAYS work if we only open the device once, we check
	     to see if the two devices are in fact the same, then proceed to
	     do one open or two.
	*/

	(void)sprintf(device1, DEVICE1, unit);
	(void)sprintf(device2, DEVICE2, unit);

	if (stat(device1, &stat1)) {
		perror("ONCORE: stat fd1");
		exit(1);
	}

	if (stat(device2, &stat2)) {
		perror("ONCORE: stat fd2");
		exit(1);
	}

	if ((stat1.st_dev == stat2.st_dev) && (stat1.st_ino == stat2.st_ino)) {
		/* same device here */
		if (!(fd1 = refclock_open(device1, SPEED, LDISC_RAW
#if !defined(HAVE_PPSAPI) && !defined(TIOCDCDTIMESTAMP)
		      | LDISC_PPS
#endif
		   ))) {
			perror("ONCORE: fd1");
			exit(1);
		}
		fd2 = fd1;
	} else { /* different devices here */
		if (!(fd1=refclock_open(device1, SPEED, LDISC_RAW))) {
			perror("ONCORE: fd1");
			exit(1);
		}
		if ((fd2=open(device2, O_RDWR)) < 0) {
			perror("ONCORE: fd2");
			exit(1);
		}
	}

	/* Devices now open, create instance structure for this unit */

	if (!(instance = (struct instance *) malloc(sizeof *instance))) {
		perror("malloc");
		close(fd1);
		return (0);
	}
	memset((char *) instance, 0, sizeof *instance);

	/* link instance up and down */

	pp = peer->procptr;
	pp->unitptr    = (caddr_t) instance;
	instance->pp   = pp;
	instance->unit = unit;
	instance->peer = peer;

	/* initialize miscellaneous variables */

	instance->o_state = ONCORE_NO_IDEA;
	cp = "state = ONCORE_NO_IDEA";
	record_clock_stats(&(instance->peer->srcadr), cp);

	instance->ttyfd = fd1;
	instance->ppsfd = fd2;

	instance->Bj_day = -1;
	instance->assert = pps_assert;
	instance->traim = -1;
	instance->model = ONCORE_UNKNOWN;
	instance->mode = MODE_UNKNOWN;
	instance->site_survey = ONCORE_SS_UNKNOWN;

	peer->precision = -26;
	peer->minpoll = 4;
	peer->maxpoll = 4;
	pp->clockdesc = "Motorola Oncore GPS Receiver";
	memcpy((char *)&pp->refid, "GPS\0", (size_t) 4);

	/* go read any input data in /etc/ntp.oncoreX */

	oncore_read_config(instance);

#ifdef HAVE_PPSAPI
	if (time_pps_create(fd2, &instance->pps_h) < 0) {
		perror("time_pps_create");
		return(0);
	}

	if (time_pps_getcap(instance->pps_h, &mode) < 0) {
		msyslog(LOG_ERR,
		    "refclock_ioctl: time_pps_getcap failed: %m");
		return (0);
	}

	if (time_pps_getparams(instance->pps_h, &instance->pps_p) < 0) {
		msyslog(LOG_ERR,
		    "refclock_ioctl: time_pps_getparams failed: %m");
		return (0);
	}

	/* nb. only turn things on, if someone else has turned something
	 *	on before we get here, leave it alone!
	 */

	if (instance->assert) { 	/* nb, default or ON */
		instance->pps_p.mode = PPS_CAPTUREASSERT | PPS_OFFSETASSERT;
		instance->pps_p.assert_offset.tv_sec = 0;
		instance->pps_p.assert_offset.tv_nsec = 0;
	} else {
		instance->pps_p.mode = PPS_CAPTURECLEAR  | PPS_OFFSETCLEAR;
		instance->pps_p.clear_offset.tv_sec = 0;
		instance->pps_p.clear_offset.tv_nsec = 0;
	}
	instance->pps_p.mode |= PPS_TSFMT_TSPEC;
	instance->pps_p.mode &= mode;		/* only set what is legal */

	if (time_pps_setparams(instance->pps_h, &instance->pps_p) < 0) {
		perror("time_pps_setparams");
		exit(1);
	}

	if (pps_device) {
		if (stat(pps_device, &stat1)) {
			perror("ONCORE: stat pps_device");
			return(0);
		}

		/* must have hardpps ON, and fd2 must be the same device as on the pps line */

		if (pps_hardpps && ((stat1.st_dev == stat2.st_dev) && (stat1.st_ino == stat2.st_ino))) {
			int	i;

			if (instance->assert)
				i = PPS_CAPTUREASSERT;
			else
				i = PPS_CAPTURECLEAR;

			if (i&mode) {
				if (time_pps_kcbind(instance->pps_h, PPS_KC_HARDPPS, i,
				    PPS_TSFMT_TSPEC) < 0) {
					msyslog(LOG_ERR,
					    "refclock_ioctl: time_pps_kcbind failed: %m");
					return (0);
				}
				pps_enable = 1;
			}
		}
	}
#endif

	pp->io.clock_recv = oncore_receive;
	pp->io.srcclock = (caddr_t)peer;
	pp->io.datalen = 0;
	pp->io.fd = fd1;
	if (!io_addclock(&pp->io)) {
		perror("io_addclock");
		(void) close(fd1);
		free(instance);
		return (0);
	}

	/*
	 * This will return the Model Number of the Oncore receiver.
	 */

	oncore_sendmsg(instance->ttyfd, oncore_cmd_Cj, sizeof(oncore_cmd_Cj));
	instance->o_state = ONCORE_ID_SENT;
	cp = "state = ONCORE_ID SENT";
	record_clock_stats(&(instance->peer->srcadr), cp);
	instance->timeout = 4;

	instance->pollcnt = 2;
	return (1);
}



/*
 * Read Input file if it exists.
 */

static void
oncore_read_config(
	struct instance *instance
	)
{
/*
 * First we try to open the configuration file
 *    /etc/oncoreN
 * where N is the unit number viz 127.127.30.N.
 * If we don't find it we try
 *    /etc/ntp.oncore.N
 * and then
 *    /etc/ntp.oncore
 *
 * If we don't find any then we don't have the cable delay or PPS offset
 * and we choose MODE (4) below.
 *
 * Five Choices for MODE
 *    (0) ONCORE is preinitialized, don't do anything to change it.
 *	    nb, DON'T set 0D mode, DON'T set Delay, position...
 *    (1) NO RESET, Read Position, delays from data file, lock it in, go to 0D mode.
 *    (2) NO RESET, Read Delays from data file, do SITE SURVEY to get position,
 *		    lock this in, go to 0D mode.
 *    (3) HARD RESET, Read Position, delays from data file, lock it in, go to 0D mode.
 *    (4) HARD RESET, Read Delays from data file, do SITE SURVEY to get position,
 *		    lock this in, go to 0D mode.
 *     NB. If a POSITION is specified in the config file with mode=(2,4) [SITE SURVEY]
 *	   then this position is set as the INITIAL position of the ONCORE.
 *	   This can reduce the time to first fix.
 * -------------------------------------------------------------------------------
 * Note that an Oncore UT without a battery backup retains NO information if it is
 *   power cycled, with a Battery Backup it remembers the almanac, etc.
 * For an Oncore VP, there is an eeprom that will contain this data, along with the
 *   option of Battery Backup.
 * So a UT without Battery Backup is equivalent to doing a HARD RESET on each
 *   power cycle, since there is nowhere to store the data.
 * -------------------------------------------------------------------------------
 *
 * If we open one or the other of the files, we read it looking for
 *   MODE, LAT, LON, (HT, HTGPS, HTMSL), DELAY, OFFSET, ASSERT, CLEAR, STATUS,
 *   POSN3D, POSN2D, CHAN, TRAIM
 * then initialize using method MODE.  For Mode = (1,3) all of (LAT, LON, HT) must
 *   be present or mode reverts to (2,4).
 *
 * Read input file.
 *
 *	# is comment to end of line
 *	= allowed between 1st and 2nd fields.
 *
 *	Expect to see one line with 'MODE' as first field, followed by an integer
 *	   in the range 0-4 (default = 4).
 *
 *	Expect to see two lines with 'LONG', 'LAT' followed by 1-3 fields.
 *	All numbers are floating point.
 *		DDD.ddd
 *		DDD  MMM.mmm
 *		DDD  MMM  SSS.sss
 *
 *	Expect to see one line with 'HT' as first field,
 *	   followed by 1-2 fields.  First is a number, the second is 'FT' or 'M'
 *	   for feet or meters.	HT is the height above the GPS ellipsoid.
 *	   If the reciever reports height in both GPS and MSL, then we will report
 *	   the difference GPS-MSL on the clockstats file.
 *
 *	There is an optional line, starting with DELAY, followed
 *	   by 1 or two fields.	The first is a number (a time) the second is
 *	   'MS', 'US' or 'NS' for miliseconds, microseconds or nanoseconds.
 *	    DELAY  is cable delay, typically a few tens of ns.
 *
 *	There is an optional line, starting with OFFSET, followed
 *	   by 1 or two fields.	The first is a number (a time) the second is
 *	   'MS', 'US' or 'NS' for miliseconds, microseconds or nanoseconds.
 *	   OFFSET is the offset of the PPS pulse from 0. (only fully implemented
 *		with the PPSAPI, we need to be able to tell the Kernel about this
 *		offset if the Kernel PLL is in use, but can only do this presently
 *		when using the PPSAPI interface.  If not using the Kernel PLL,
 *		then there is no problem.
 *
 *	There is an optional line, with either ASSERT or CLEAR on it, which
 *	   determine which transition of the PPS signal is used for timing by the
 *	   PPSAPI.  If neither is present, then ASSERT is assumed.
 *
 *	There are three options that have to do with using the shared memory opition.
 *	   First, to enable the option there must be an ASSERT line with a file name.
 *	   The file name is the file associated with the shared memory.
 *
 *	In the shared memory there are three 'records' containing the @@Ea (or equivalent)
 *	   data, and this contains the position data.  There will always be data in the
 *	   record cooresponding to the '0D' @@Ea record, and the user has a choice of
 *	   filling the '3D' @@Ea record by specifying POSN3D, or the '2D' record by
 *	   specifying POSN2D.  In either case the '2D' or '3D' record is filled once
 *	   every 15s.
 *
 *	Two additional variables that can be set are CHAN and TRAIM.  These should be
 *	   set correctly by the code examining the @@Cj record, but we bring them out here
 *	   to allow the user to override either the # of channels, or the existance of TRAIM.
 *	   CHAN expects to be followed by in integer: 6, 8, or 12. TRAIM expects to be
 *	   followed by YES or NO.
 *
 * So acceptable input would be
 *	# these are my coordinates (RWC)
 *	LON  -106 34.610
 *	LAT    35 08.999
 *	HT	1589	# could equally well say HT 5215 FT
 *	DELAY  60 ns
 */

	FILE	*fd;
	char	*cp, *cc, *ca, line[100], units[2], device[20], Msg[160];
	int	i, sign, lat_flg, long_flg, ht_flg, mode;
	double	f1, f2, f3;

	sprintf(device, "%s%d", INIT_FILE, instance->unit);             /* try "ntp.oncore0" first */
	if ((fd=fopen(device, "r")) == NULL) {                          /*   it was in the original documentation */
		sprintf(device, "%s.%d", INIT_FILE, instance->unit);    /* then try "ntp.oncore.0 */
		if ((fd=fopen(device, "r")) == NULL) {
			if ((fd=fopen(INIT_FILE, "r")) == NULL) {       /* and finally "ntp.oncore" */
				instance->init_type = 4;
				return;
			}
		}
	}

	mode = 0;
	lat_flg = long_flg = ht_flg = 0;
	while (fgets(line, 100, fd)) {

		/* Remove comments */
		if ((cp = strchr(line, '#')))
			*cp = '\0';

		/* Remove trailing space */
		for (i = strlen(line);
		     i > 0 && isascii((int)line[i - 1]) && isspace((int)line[i - 1]);
			)
			line[--i] = '\0';

		/* Remove leading space */
		for (cc = line; *cc && isascii((int)*cc) && isspace((int)*cc); cc++)
			continue;

		/* Stop if nothing left */
		if (!*cc)
			continue;

		/* Uppercase the command and find the arg */
		for (ca = cc; *ca; ca++) {
			if (isascii((int)*ca)) {
				if (islower((int)*ca)) {
					*ca = toupper(*ca);
				} else if (isspace((int)*ca) || (*ca == '='))
					break;
			}
		}

		/* Remove space (and possible =) leading the arg */
		for (; *ca && isascii((int)*ca) && (isspace((int)*ca) || (*ca == '=')); ca++)
			continue;

		/*
		 * move call to oncore_shmem_init() from here to after
		 * we have determined Oncore Model, so we can ignore
		 * request if model doesnt 'support' it
		 */

		if (!strncmp(cc, "STATUS", (size_t) 6) || !strncmp(cc, "SHMEM", (size_t) 5)) {
			i = strlen(ca);
			instance->shmem_fname = (char *) malloc((unsigned) (i+1));
			strcpy(instance->shmem_fname, ca);
			continue;
		}

		/* Uppercase argument as well */
		for (cp = ca; *cp; cp++)
			if (isascii((int)*cp) && islower((int)*cp))
				*cp = toupper(*cp);

		if (!strncmp(cc, "LAT", (size_t) 3)) {
			f1 = f2 = f3 = 0;
			sscanf(ca, "%lf %lf %lf", &f1, &f2, &f3);
			sign = 1;
			if (f1 < 0) {
				f1 = -f1;
				sign = -1;
			}
			instance->ss_lat = sign*1000*(fabs(f3) + 60*(fabs(f2) + 60*f1)); /*miliseconds*/
			lat_flg++;
		} else if (!strncmp(cc, "LON", (size_t) 3)) {
			f1 = f2 = f3 = 0;
			sscanf(ca, "%lf %lf %lf", &f1, &f2, &f3);
			sign = 1;
			if (f1 < 0) {
				f1 = -f1;
				sign = -1;
			}
			instance->ss_long = sign*1000*(fabs(f3) + 60*(fabs(f2) + 60*f1)); /*miliseconds*/
			long_flg++;
		} else if (!strncmp(cc, "HT", (size_t) 2)) {
			f1 = 0;
			units[0] = '\0';
			sscanf(ca, "%lf %1s", &f1, units);
			if (units[0] == 'F')
				f1 = 0.3048 * f1;
			instance->ss_ht = 100 * f1;    /* cm */
			ht_flg++;
		} else if (!strncmp(cc, "DELAY", (size_t) 5)) {
			f1 = 0;
			units[0] = '\0';
			sscanf(ca, "%lf %1s", &f1, units);
			if (units[0] == 'N')
				;
			else if (units[0] == 'U')
				f1 = 1000 * f1;
			else if (units[0] == 'M')
				f1 = 1000000 * f1;
			else
				f1 = 1000000000 * f1;
			if (f1 < 0 || f1 > 1.e9)
				f1 = 0;
			if (f1 < 0 || f1 > 999999) {
				sprintf(Msg, "PPS Cable delay of %fns out of Range, ignored", f1);
				record_clock_stats(&(instance->peer->srcadr), Msg);
			} else
				instance->delay = f1;		/* delay in ns */
		} else if (!strncmp(cc, "OFFSET", (size_t) 6)) {
			f1 = 0;
			units[0] = '\0';
			sscanf(ca, "%lf %1s", &f1, units);
			if (units[0] == 'N')
				;
			else if (units[0] == 'U')
				f1 = 1000 * f1;
			else if (units[0] == 'M')
				f1 = 1000000 * f1;
			else
				f1 = 1000000000 * f1;
			if (f1 < 0 || f1 > 1.e9)
				f1 = 0;
			if (f1 < 0 || f1 > 999999999.) {
				sprintf(Msg, "PPS Offset of %fns out of Range, ignored", f1);
				record_clock_stats(&(instance->peer->srcadr), Msg);
			} else
				instance->offset = f1;		/* offset in ns */
		} else if (!strncmp(cc, "MODE", (size_t) 4)) {
			sscanf(ca, "%d", &mode);
			if (mode < 0 || mode > 4)
				mode = 4;
		} else if (!strncmp(cc, "ASSERT", (size_t) 6)) {
			instance->assert = 1;
		} else if (!strncmp(cc, "CLEAR", (size_t) 5)) {
			instance->assert = 0;
		} else if (!strncmp(cc, "POSN2D", (size_t) 6)) {
			instance->shmem_Posn = 2;
		} else if (!strncmp(cc, "POSN3D", (size_t) 6)) {
			instance->shmem_Posn = 3;
		} else if (!strncmp(cc, "CHAN", (size_t) 4)) {
			sscanf(ca, "%d", &i);
			if ((i == 6) || (i == 8) || (i == 12))
				instance->chan = i;
		} else if (!strncmp(cc, "TRAIM", (size_t) 5)) {
			instance->traim = 1;				/* so TRAIM alone is YES */
			if (!strcmp(ca, "NO") || !strcmp(ca, "OFF"))    /* Yes/No, On/Off */
				instance->traim = 0;
		}
	}
	fclose(fd);

	/*
	 *    OK, have read all of data file, and extracted the good stuff.
	 *    If lat/long/ht specified they ALL must be specified for mode = (1,3).
	 */

	instance->posn_set = 1;
	if ((lat_flg || long_flg || ht_flg) && !(lat_flg * long_flg * ht_flg)) {
		printf("ONCORE: incomplete data on %s\n", INIT_FILE);
		instance->posn_set = 0;
		if (mode == 1 || mode == 3) {
			sprintf(Msg, "Input Mode = %d, but no/incomplete position, mode set to %d", mode, mode+1);
			record_clock_stats(&(instance->peer->srcadr), Msg);
			mode++;
		}
	}
	instance->init_type = mode;

	sprintf(Msg, "Input mode = %d", mode);
	record_clock_stats(&(instance->peer->srcadr), Msg);
}



static void
oncore_init_shmem(
	struct instance *instance
	)
{
#ifdef ONCORE_SHMEM_STATUS
	int i, l, n;
	char *buf;
	struct msg_desc *mp;
	size_t oncore_shmem_length;

	if (instance->shmem_first)
		return;

	instance->shmem_first++;

	if ((instance->statusfd = open(instance->shmem_fname, O_RDWR|O_CREAT|O_TRUNC, 0644)) < 0) {
		perror(instance->shmem_fname);
		return;
	}

	n = 1;
	for (mp = oncore_messages; mp->flag[0]; mp++) {
		mp->shmem = n;
		/* Allocate space for multiplexed almanac, and 0D/2D/3D @@Ea records */
		if (!strcmp(mp->flag, "Cb")) {
			instance->shmem_Cb = n;
			n += (mp->len + 3) * 34;
		}
		if (!strcmp(mp->flag, "Ba")) {
			instance->shmem_Ba = n;
			n += (mp->len + 3) * 3;
		}
		if (!strcmp(mp->flag, "Ea")) {
			instance->shmem_Ea = n;
			n += (mp->len + 3) * 3;
		}
		if (!strcmp(mp->flag, "Ha")) {
			instance->shmem_Ha = n;
			n += (mp->len + 3) * 3;
		}
		n += (mp->len + 3);
	}
	oncore_shmem_length = n + 2;
	fprintf(stderr, "ONCORE: SHMEM length: %d bytes\n", (int) oncore_shmem_length);

	buf = malloc(oncore_shmem_length);
	if (buf == NULL) {
		perror("malloc");
		return;
	}
	memset(buf, 0, sizeof(buf));
	i = write(instance->statusfd, buf, oncore_shmem_length);
	if (i != oncore_shmem_length) {
		perror(instance->shmem_fname);
		return;
	}
	free(buf);
	instance->shmem = (u_char *) mmap(0, oncore_shmem_length,
	    PROT_READ | PROT_WRITE,
#ifdef MAP_HASSEMAPHORE
			       MAP_HASSEMAPHORE |
#endif
			       MAP_SHARED,
	    instance->statusfd, (off_t)0);
	if (instance->shmem == (u_char *)MAP_FAILED) {
		instance->shmem = 0;
		close (instance->statusfd);
		return;
	}
	for (mp = oncore_messages; mp->flag[0]; mp++) {
		l = mp->shmem;
		instance->shmem[l + 0] = mp->len >> 8;
		instance->shmem[l + 1] = mp->len & 0xff;
		instance->shmem[l + 2] = 0;
		instance->shmem[l + 3] = '@';
		instance->shmem[l + 4] = '@';
		instance->shmem[l + 5] = mp->flag[0];
		instance->shmem[l + 6] = mp->flag[1];
		if (!strcmp(mp->flag, "Cb") || !strcmp(mp->flag, "Ba") || !strcmp(mp->flag, "Ea") || !strcmp(mp->flag, "Ha")) {
			if (!strcmp(mp->flag, "Cb"))
				n = 35;
			else
				n = 4;
			for (i = 1; i < n; i++) {
				instance->shmem[l + i * (mp->len+3) + 0] = mp->len >> 8;
				instance->shmem[l + i * (mp->len+3) + 1] = mp->len & 0xff;
				instance->shmem[l + i * (mp->len+3) + 2] = 0;
				instance->shmem[l + i * (mp->len+3) + 3] = '@';
				instance->shmem[l + i * (mp->len+3) + 4] = '@';
				instance->shmem[l + i * (mp->len+3) + 5] = mp->flag[0];
				instance->shmem[l + i * (mp->len+3) + 6] = mp->flag[1];
			}
		}
	}
#endif /* ONCORE_SHMEM_STATUS */
}



/*
 * oncore_shutdown - shut down the clock
 */

static void
oncore_shutdown(
	int unit,
	struct peer *peer
	)
{
	register struct instance *instance;
	struct refclockproc *pp;

	pp = peer->procptr;
	instance = (struct instance *) pp->unitptr;

	io_closeclock(&pp->io);

	free(instance);
}



/*
 * oncore_poll - called by the transmit procedure
 */

static void
oncore_poll(
	int unit,
	struct peer *peer
	)
{
	struct instance *instance;

	instance = (struct instance *) peer->procptr->unitptr;
	if (instance->timeout) {
		char	*cp;

		instance->timeout--;
		if (instance->timeout == 0) {
			cp = "Oncore: No response from @@Cj, shutting down driver";
			record_clock_stats(&(instance->peer->srcadr), cp);
			oncore_shutdown(unit, peer);
		} else {
			oncore_sendmsg(instance->ttyfd, oncore_cmd_Cj, sizeof(oncore_cmd_Cj));
			cp = "Oncore: Resend @@Cj";
			record_clock_stats(&(instance->peer->srcadr), cp);
		}
		return;
	}

	if (!instance->pollcnt)
		refclock_report(peer, CEVNT_TIMEOUT);
	else
		instance->pollcnt--;
	peer->procptr->polls++;
	instance->polled = 1;
}



/*
 * move data from NTP to buffer (toss in unlikely case it wont fit)
 */

static void
oncore_receive(
	struct recvbuf *rbufp
	)
{
	size_t i;
	u_char *p;
	struct peer *peer;
	struct instance *instance;

	peer = (struct peer *)rbufp->recv_srcclock;
	instance = (struct instance *) peer->procptr->unitptr;
	p = (u_char *) &rbufp->recv_space;

#if 0
	if (debug > 4) {
		int i;
		printf("ONCORE: >>>");
		for(i=0; i<rbufp->recv_length; i++)
			printf("%02x ", p[i]);
		printf("\n");
		printf("ONCORE: >>>");
		for(i=0; i<rbufp->recv_length; i++)
			printf("%03o ", p[i]);
		printf("\n");
	}
#endif

	i = rbufp->recv_length;
	if (rcvbuf+rcvptr+i > &rcvbuf[sizeof rcvbuf])
		i = sizeof(rcvbuf) - rcvptr;	/* and some char will be lost */
	memcpy(rcvbuf+rcvptr, p, i);
	rcvptr += i;
	oncore_consume(instance);
}



/*
 * Deal with any complete messages
 */

static void
oncore_consume(
	struct instance *instance
	)
{
	int i, j, m;
	unsigned l;

	while (rcvptr >= 7) {
		if (rcvbuf[0] != '@' || rcvbuf[1] != '@') {
			/* We're not in sync, lets try to get there */
			for (i=1; i < rcvptr-1; i++)
				if (rcvbuf[i] == '@' && rcvbuf[i+1] == '@')
					break;
			if (debug > 4)
				printf("ONCORE[%d]: >>> skipping %d chars\n", instance->unit, i);
			if (i != rcvptr)
				memcpy(rcvbuf, rcvbuf+i, (size_t)(rcvptr-i));
			rcvptr -= i;
			continue;
		}

		/* Ok, we have a header now */
		l = sizeof(oncore_messages)/sizeof(oncore_messages[0]) -1;
		for(m=0; m<l; m++)
			if (!strncmp(oncore_messages[m].flag, (char *)(rcvbuf+2), (size_t) 2))
				break;
		if (m == l) {
			if (debug > 4)
				printf("ONCORE[%d]: >>> Unknown MSG, skipping 4 (%c%c)\n", instance->unit, rcvbuf[2], rcvbuf[3]);
			memcpy(rcvbuf, rcvbuf+4, (size_t) 4);
			rcvptr -= 4;
			continue;
		}

		l = oncore_messages[m].len;
#if 0
		if (debug > 3)
			printf("ONCORE[%d]: GOT: %c%c  %d of %d entry %d\n", instance->unit, rcvbuf[2], rcvbuf[3], rcvptr, l, m);
#endif
		/* Got the entire message ? */

		if (rcvptr < l)
			return;

		/* are we at the end of message? should be <Cksum><CR><LF> */

		if (rcvbuf[l-2] != '\r' || rcvbuf[l-1] != '\n') {
			if (debug)
				printf("ONCORE[%d]: NO <CR><LF> at end of message\n", instance->unit);
		} else {	/* check the CheckSum */
			j = 0;
			for (i = 2; i < l-3; i++)
				j ^= rcvbuf[i];
			if (j == rcvbuf[l-3]) {
				if (instance->shmem != NULL) {
					instance->shmem[oncore_messages[m].shmem + 2]++;
					memcpy(instance->shmem + oncore_messages[m].shmem + 3,
					    rcvbuf, (size_t) l);
				}
				oncore_msg_any(instance, rcvbuf, (size_t) (l-3), m);
				if (oncore_messages[m].handler)
					oncore_messages[m].handler(instance, rcvbuf, (size_t) (l-3));
			} else if (debug) {
				printf("ONCORE[%d]: Checksum mismatch! calc %o is %o\n", instance->unit, j, rcvbuf[l-3]);
				printf("ONCORE[%d]: @@%c%c ", instance->unit, rcvbuf[2], rcvbuf[3]);
				for (i=4; i<l; i++)
					printf("%03o ", rcvbuf[i]);
				printf("\n");
			}
		}

		if (l != rcvptr)
			memcpy(rcvbuf, rcvbuf+l, (size_t) (rcvptr-l));
		rcvptr -= l;
	}
}



/*
 * write message to Oncore.
 */

static void
oncore_sendmsg(
	int	fd,
	u_char *ptr,
	size_t len
	)
{
	u_char cs = 0;

	printf("ONCORE: Send @@%c%c %d\n", ptr[0], ptr[1], (int) len);
	write(fd, "@@", (size_t) 2);
	write(fd, ptr, len);
	while (len--)
		cs ^= *ptr++;
	write(fd, &cs, (size_t) 1);
	write(fd, "\r\n", (size_t) 2);
}



/*
 * print Oncore response message.
 */

static void
oncore_msg_any(
	struct instance *instance,
	u_char *buf,
	size_t len,
	int idx
	)
{
	int i;
	const char *fmt = oncore_messages[idx].fmt;
	const char *p;
#ifdef HAVE_GETCLOCK
	struct timespec ts;
#endif
	struct timeval tv;

	if (debug > 3) {
#ifdef HAVE_GETCLOCK
		(void) getclock(TIMEOFDAY, &ts); 
		tv.tv_sec = ts.tv_sec; 
		tv.tv_usec = ts.tv_nsec / 1000; 
#else
		GETTIMEOFDAY(&tv, 0);
#endif
		printf("ONCORE[%d]: %ld.%06ld\n", instance->unit, (long) tv.tv_sec, (long) tv.tv_usec);

		if (!*fmt) {
			printf(">>@@%c%c ", buf[2], buf[3]);
			for(i=2; i < len && i < 2400 ; i++)
				printf("%02x", buf[i]);
			printf("\n");
			return;
		} else {
			printf("##");
			for (p = fmt; *p; p++) {
				putchar(*p);
				putchar('_');
			}
			printf("\n%c%c", buf[2], buf[3]);
			i = 4;
			for (p = fmt; *p; p++) {
				printf("%02x", buf[i++]);
			}
			printf("\n");
		}
	}
}



/*
 * Demultiplex the almanac into shmem
 */

static void
oncore_msg_Cb(
	struct instance *instance,
	u_char *buf,
	size_t len
	)
{
	int i;

	if (instance->shmem == NULL)
		return;

	if (buf[4] == 5)
		i = buf[5];
	else if (buf[4] == 4 && buf[5] <= 5)
		i = buf[5] + 24;
	else if (buf[4] == 4 && buf[5] <= 10)
		i = buf[5] + 23;
	else
		i = 34;
	i *= 36;
	instance->shmem[instance->shmem_Cb + i + 2]++;
	memcpy(instance->shmem + instance->shmem_Cb + i + 3, buf, (size_t) (len + 3));
}



/*
 * We do an @@Cj twice in the initialization sequence.
 * o Once at the very beginning to get the Model number so we know what commands
 *   we can issue,
 * o And once later after we have done a reset and test, (which may hang),
 *   as we are about to initialize the Oncore and start it running.
 * o We have one routine below for each case.
 */


/*
 * Determine the Type from the Model #, this determines #chan and if TRAIM is
 *   available.  We use ONLY the #chans, and determint TRAIM by trying it.
 */

static void
oncore_msg_Cj(
	struct instance *instance,
	u_char *buf,
	size_t len
	)
{
	memcpy(instance->Cj, buf, len);

	instance->timeout = 0;
	if (instance->o_state == ONCORE_ID_SENT)
		oncore_msg_Cj_id(instance, buf, len);
	else if (instance->o_state == ONCORE_INIT)
		oncore_msg_Cj_init(instance, buf, len);
}



/* The information on determing a Oncore 'Model', viz VP, UT, etc, from
 *	the Model Number comes from "Richard M. Hambly" <rick@cnssys.com>
 *	and from Motorola.  Until recently Rick was the only source of
 *	this information as Motorola didnt give the information out.
 */

static void
oncore_msg_Cj_id(
	struct instance *instance,
	u_char *buf,
	size_t len
	)
{
	char *cp, *cp1, *cp2, Model[21], Msg[160];
	int	mode;

	/* Write Receiver ID message to clockstats file */

	instance->Cj[294] = '\0';
	for (cp=(char *)instance->Cj; cp< (char *) &instance->Cj[294]; ) {
		cp1 = strchr(cp, '\r');
		if (!cp1)
			cp1 = (char *)&instance->Cj[294];
		*cp1 = '\0';
		record_clock_stats(&(instance->peer->srcadr), cp);
		*cp1 = '\r';
		cp = cp1+2;
	}

	/* next, the Firmware Version and Revision numbers */

	instance->version  = atoi(&instance->Cj[83]);
	instance->revision = atoi(&instance->Cj[111]);

	/* from model number decide which Oncore this is,
		and then the number of channels */

	for (cp=&instance->Cj[160]; *cp == ' '; cp++)	/* start right after 'Model #' */
		;
	cp1 = cp;
	cp2 = Model;
	for (; !isspace((int)*cp) && cp-cp1 < 20; cp++, cp2++)
		*cp2 = *cp;
	*cp2 = '\0';

	cp = 0;
	if (!strncmp(Model, "PVT6", (size_t) 4)) {
		cp = "PVT6";
		instance->model = ONCORE_PVT6;
	} else if (Model[0] == 'A') {
		cp = "Basic";
		instance->model = ONCORE_BASIC;
	} else if (Model[0] == 'B' || !strncmp(Model, "T8", (size_t) 2)) {
		cp = "VP";
		instance->model = ONCORE_VP;
	} else if (!strncmp(Model, "P1", (size_t) 2)) {
		cp = "M12";
		instance->model = ONCORE_M12;
	} else if (Model[0] == 'R') {
		if (Model[5] == 'N') {
			cp = "GT";
			instance->model = ONCORE_GT;
		} else if ((Model[1] == '3' || Model[1] == '4') && Model[5] == 'G') {
			cp = "GT+";
			instance->model = ONCORE_GTPLUS;
		} else if ((Model[1] == '5' && Model[5] == 'U') || (Model[1] == '1' && Model[5] == 'A')) {
				cp = "UT";
				instance->model = ONCORE_UT;
		} else if (Model[1] == '5' && Model[5] == 'G') {
			cp = "UT+";
			instance->model = ONCORE_UTPLUS;
		} else if (Model[1] == '6' && Model[5] == 'G') {
			cp = "SL";
			instance->model = ONCORE_SL;
		} else {
			cp = "Unknown";
			instance->model = ONCORE_UNKNOWN;
		}
	} else	{
		cp = "Unknown";
		instance->model = ONCORE_UNKNOWN;
	}

	sprintf(Msg, "This looks like an Oncore %s with version %d.%d firmware.", cp, instance->version, instance->revision);
	record_clock_stats(&(instance->peer->srcadr), Msg);

	if (instance->chan == 0) {	/* dont reset if set in input data */
		instance->chan = 8;	/* default */
		if (instance->model == ONCORE_BASIC || instance->model == ONCORE_PVT6)
			instance->chan = 6;
		else if (instance->model == ONCORE_VP || instance->model == ONCORE_UT || instance->model == ONCORE_UTPLUS)
			instance->chan = 8;
		else if (instance->model == ONCORE_M12)
			instance->chan = 12;
	}

	if (instance->traim == -1) {	/* dont reset if set in input data */
		instance->traim = 0;	/* default */
		if (instance->model == ONCORE_BASIC || instance->model == ONCORE_PVT6)
			instance->traim = 0;
		else if (instance->model == ONCORE_VP || instance->model == ONCORE_UT || instance->model == ONCORE_UTPLUS)
			instance->traim = 1;
		else if (instance->model == ONCORE_M12)
			instance->traim = 0;
	}

	sprintf(Msg, "Channels = %d, TRAIM = %s", instance->chan,
		((instance->traim < 0) ? "UNKNOWN" : ((instance->traim > 0) ? "ON" : "OFF")));
	record_clock_stats(&(instance->peer->srcadr), Msg);

	/* The M12 with 1.3 Firmware, looses track of all Satellites and has to
	 * start again if we go from 0D -> 3D, then looses them again when we
	 * go from 3D -> 0D.  We do this to get a @@Ea message for SHMEM.
	 * For NOW we have SHMEM turned off for the M12, v1.3
	 */

/*BAD M12*/ if (instance->model == ONCORE_M12 && instance->version == 1 && instance->revision <= 3) {
		instance->shmem_fname = 0;
		cp = "*** SHMEM turned off for ONCORE M12 ***";
		record_clock_stats(&(instance->peer->srcadr), cp);
	}

	/*
	 * we now know model number and have zeroed
	 * instance->shmem_fname if SHMEM is not supported
	 */

	if (instance->shmem_fname);
		oncore_init_shmem(instance);

	if (instance->shmem)
		cp = "SHMEM is available";
	else
		cp = "SHMEM is NOT available";
	record_clock_stats(&(instance->peer->srcadr), cp);

#ifdef HAVE_PPSAPI
	if (instance->assert)
		cp = "Timing on Assert.";
	else
		cp = "Timing on Clear.";
	record_clock_stats(&(instance->peer->srcadr), cp);
#endif

	mode = instance->init_type;
	if (mode == 3 || mode == 4) {	/* Cf will call Fa */
		oncore_sendmsg(instance->ttyfd, oncore_cmd_Cf, sizeof(oncore_cmd_Cf));
		instance->o_state = ONCORE_RESET_SENT;
		cp = "state = ONCORE_RESET_SENT";
		record_clock_stats(&(instance->peer->srcadr), cp);
	} else {
		if (instance->chan == 6)
			oncore_sendmsg(instance->ttyfd, oncore_cmd_Ca, sizeof(oncore_cmd_Ca));
		else if (instance->chan == 8)
			oncore_sendmsg(instance->ttyfd, oncore_cmd_Fa, sizeof(oncore_cmd_Fa));
		else if (instance->chan == 12)
			oncore_sendmsg(instance->ttyfd, oncore_cmd_Ia, sizeof(oncore_cmd_Ia));

		instance->o_state = ONCORE_TEST_SENT;
		cp = "state = ONCORE_TEST_SENT";
		record_clock_stats(&(instance->peer->srcadr), cp);
		instance->timeout = 4;
	}
}



static void
oncore_msg_Cj_init(
	struct instance *instance,
	u_char *buf,
	size_t len
	)
{
	char *cp, Cmd[20], Msg[160];
	int	mode;

	/* OK, know type of Oncore, have possibly reset, and have tested.
	 * If we have or don't have TRAIM and position hold may still be unknown.
	 * Now initialize.
	 */

	oncore_sendmsg(instance->ttyfd, oncore_cmd_Cg, sizeof(oncore_cmd_Cg)); /* Set Posn Fix mode (not Idle (VP)) */
	oncore_sendmsg(instance->ttyfd, oncore_cmd_Bb, sizeof(oncore_cmd_Bb)); /* turn on for shmem */
	oncore_sendmsg(instance->ttyfd, oncore_cmd_Ek, sizeof(oncore_cmd_Ek)); /* turn off */
	oncore_sendmsg(instance->ttyfd, oncore_cmd_Aw, sizeof(oncore_cmd_Aw)); /* UTC time */
	oncore_sendmsg(instance->ttyfd, oncore_cmd_AB, sizeof(oncore_cmd_AB)); /* Appl type static */
	oncore_sendmsg(instance->ttyfd, oncore_cmd_Be, sizeof(oncore_cmd_Be)); /* Tell us the Almanac for shmem */

	/* Turn OFF position hold, it needs to be off to set position (for some units),
	   will get set ON in @@Ea later */

	if (instance->chan == 12)
		oncore_sendmsg(instance->ttyfd, oncore_cmd_Gd0, sizeof(oncore_cmd_Gd0));
	else {
		oncore_sendmsg(instance->ttyfd, oncore_cmd_At0, sizeof(oncore_cmd_At0));
		oncore_sendmsg(instance->ttyfd, oncore_cmd_Av0, sizeof(oncore_cmd_Av0));
	}

	mode = instance->init_type;
	if (debug) {
		printf("ONCORE[%d]: INIT mode = %d\n", instance->unit, mode);
		printf("ONCORE[%d]: chan = %d\n", instance->unit, instance->chan);
	}

	/* If there is Position input in the Config file
	 * and mode = (1,3) set it as posn hold posn, goto 0D mode.
	 *  or mode = (2,4) set it as INITIAL position, and do Site Survey.
	 */


	if (instance->posn_set) {
		switch (mode) { /* if we have a position, put it in as posn and posn-hold posn */
		case 0:
			break;
		case 1:
		case 2:
		case 3:
		case 4:
			memcpy(Cmd, oncore_cmd_As, sizeof(oncore_cmd_As));	/* dont modify static variables */
			w32_buf(&Cmd[2],  (int) instance->ss_lat);
			w32_buf(&Cmd[6],  (int) instance->ss_long);
			w32_buf(&Cmd[10], (int) instance->ss_ht);
			Cmd[14] = 0;
			oncore_sendmsg(instance->ttyfd, Cmd,  sizeof(oncore_cmd_As));

			memcpy(Cmd, oncore_cmd_Au, sizeof(oncore_cmd_Au));
			w32_buf(&Cmd[2], (int) instance->ss_ht);
			Cmd[6] = 0;
			oncore_sendmsg(instance->ttyfd, Cmd,  sizeof(oncore_cmd_Au));

			if (instance->chan == 12) {
				memcpy(Cmd, oncore_cmd_Ga, sizeof(oncore_cmd_Ga));
				w32_buf(&Cmd[2], (int) instance->ss_lat);
				w32_buf(&Cmd[6], (int) instance->ss_long);
				w32_buf(&Cmd[10], (int) instance->ss_ht);
				Cmd[14] = 0;
				oncore_sendmsg(instance->ttyfd, Cmd,  sizeof(oncore_cmd_Ga));
				oncore_sendmsg(instance->ttyfd, oncore_cmd_Gd1,  sizeof(oncore_cmd_Gd1));
			} else {
				memcpy(Cmd, oncore_cmd_Ad, sizeof(oncore_cmd_Ad));
				w32_buf(&Cmd[2], (int) instance->ss_lat);
				oncore_sendmsg(instance->ttyfd, Cmd,  sizeof(oncore_cmd_Ad));

				memcpy(Cmd, oncore_cmd_Ae, sizeof(oncore_cmd_Ae));
				w32_buf(&Cmd[2], (int) instance->ss_long);
				oncore_sendmsg(instance->ttyfd, Cmd,  sizeof(oncore_cmd_Ae));

				memcpy(Cmd, oncore_cmd_Af, sizeof(oncore_cmd_Af));
				w32_buf(&Cmd[2], (int) instance->ss_ht);
				Cmd[6] = 0;
				oncore_sendmsg(instance->ttyfd, Cmd,  sizeof(oncore_cmd_Af));
				oncore_sendmsg(instance->ttyfd, oncore_cmd_At1,  sizeof(oncore_cmd_At1));
			}
			break;
		}
	}


	switch (mode) {
	case 0: /* NO initialization, don't change anything */
		instance->site_survey = ONCORE_SS_DONE;
		break;

	case 1:
	case 3: /* Use given Position */
		instance->site_survey = ONCORE_SS_DONE;
		break;

	case 2:
	case 4: /* Site Survey */
		if (instance->chan == 12) {	/* no 12chan site survey command */
			instance->site_survey = ONCORE_SS_SW;
			sprintf(Msg, "Initiating software 3D site survey (%d samples)", POS_HOLD_AVERAGE);
			record_clock_stats(&(instance->peer->srcadr), Msg);
		} else {
			instance->site_survey = ONCORE_SS_TESTING;
			oncore_sendmsg(instance->ttyfd, oncore_cmd_At2,  sizeof(oncore_cmd_At2));
		}
		break;
	}

	if (mode != 0) {
			/* cable delay in ns */
		memcpy(Cmd, oncore_cmd_Az, sizeof(oncore_cmd_Az));
		w32_buf(&Cmd[2], instance->delay);
		oncore_sendmsg(instance->ttyfd, Cmd,  sizeof(oncore_cmd_Az));

			/* PPS offset in ns */
		if (instance->offset) {
			if (instance->model == ONCORE_VP || instance->model == ONCORE_UT ||
			   instance->model == ONCORE_UTPLUS) {
				memcpy(Cmd, oncore_cmd_Ay, sizeof(oncore_cmd_Ay));
				w32_buf(&Cmd[2], instance->offset);
				oncore_sendmsg(instance->ttyfd, Cmd,  sizeof(oncore_cmd_Ay));
			} else {
				cp = "Can only set PPS OFFSET for VP/UT/UT+, offset ignored";
				record_clock_stats(&(instance->peer->srcadr), cp);
				instance->offset = 0;
			}
		}
	}

	/* 6, 8 12 chan - Position/Status/Data Output Message, 1/s */
	/* now we're really running */

	if (instance->chan == 6) { /* kill 8 chan commands, possibly testing VP in 6chan mode */
		oncore_sendmsg(instance->ttyfd, oncore_cmd_Ba,	sizeof(oncore_cmd_Ba));
		oncore_sendmsg(instance->ttyfd, oncore_cmd_Ea0, sizeof(oncore_cmd_Ea0));
		oncore_sendmsg(instance->ttyfd, oncore_cmd_En0, sizeof(oncore_cmd_En0));
	} else if (instance->chan == 8) {  /* kill 6chan commands */
		oncore_sendmsg(instance->ttyfd, oncore_cmd_Ea,	sizeof(oncore_cmd_Ea));
		oncore_sendmsg(instance->ttyfd, oncore_cmd_Ba0, sizeof(oncore_cmd_Ba0));
		oncore_sendmsg(instance->ttyfd, oncore_cmd_Bn0, sizeof(oncore_cmd_Bn0));
	} else if (instance->chan == 12)
		oncore_sendmsg(instance->ttyfd, oncore_cmd_Ha, sizeof(oncore_cmd_Ha));

	instance->count = 1;
	instance->o_state = ONCORE_ALMANAC;
	cp = "state = ONCORE_ALMANAC";
	record_clock_stats(&(instance->peer->srcadr), cp);
}



/*
 * Set to Factory Defaults (Reasonable for UT w/ no Battery Backup
 *	not so for VP (eeprom) or any unit with a battery
 */

static void
oncore_msg_Cf(
	struct instance *instance,
	u_char *buf,
	size_t len
	)
{
	const char *cp;

	if (instance->o_state == ONCORE_RESET_SENT) {
		if (instance->chan == 6)
			oncore_sendmsg(instance->ttyfd, oncore_cmd_Ca, sizeof(oncore_cmd_Ca));
		else if (instance->chan == 8)
			oncore_sendmsg(instance->ttyfd, oncore_cmd_Fa, sizeof(oncore_cmd_Fa));
		else if (instance->chan == 12)
			oncore_sendmsg(instance->ttyfd, oncore_cmd_Ia, sizeof(oncore_cmd_Ia));

		instance->o_state = ONCORE_TEST_SENT;
		cp = "state = ONCORE_TEST_SENT";
		record_clock_stats(&(instance->peer->srcadr), cp);
	}
}



/* Here for @@Ca, @@Fa and @@Ia messages */

/* There are good reasons NOT to do a @@Ca or @@Fa command with the ONCORE.
 * Doing it, it was found that under some circumstances the following
 * command would fail if issued immediately after the return from the
 * @@Fa, but a 2sec delay seemed to fix things.  Since simply calling
 * sleep(2) is wastefull, and may cause trouble for some OS's, repeating
 * itimer, we set a flag, and test it at the next POLL.  If it hasnt
 * been cleared, we reissue the @@Cj that is issued below.
 * Note that we do a @@Cj at the beginning, and again here.
 * The first is to get the info, the 2nd is just used as a safe command
 * after the @@Fa for all Oncores (and it was in this posn in the
 * original code).
 */

static void
oncore_msg_CaFaIa(
	struct instance *instance,
	u_char *buf,
	size_t len
	)
{
	char *cp;

	if (instance->o_state == ONCORE_TEST_SENT) {
		int	antenna;

		instance->timeout = 0;

		if (debug > 2) {
			if (buf[2] == 'I')
				printf("ONCORE[%d]: >>@@%ca %x %x %x\n", instance->unit, buf[2], buf[4], buf[5], buf[6]);
			else
				printf("ONCORE[%d]: >>@@%ca %x %x\n", instance->unit, buf[2], buf[4], buf[5]);
		}

		antenna = buf[4] & 0xc0;
		antenna >>= 6;
		buf[4] &= ~0xc0;

		if (buf[4] || buf[5] || ((buf[2] == 'I') && buf[6])) {
			cp = "ONCORE: Self Test Failed, shutting down driver";
			record_clock_stats(&(instance->peer->srcadr), cp);
			oncore_shutdown(instance->unit, instance->peer);
			return;
		}
		if (antenna) {
			char *cp1, Msg[160];

			cp1 = (antenna == 0x1) ? "(Over Current)" :
				((antenna == 0x2) ? "(Under Current)" : "(No Voltage)");

			cp = "ONCORE: Self Test, NonFatal Antenna Problems ";
			strcpy(Msg, cp);
			strcat(Msg, cp1);
			record_clock_stats(&(instance->peer->srcadr), Msg);
		}

		oncore_sendmsg(instance->ttyfd, oncore_cmd_Cj, sizeof(oncore_cmd_Cj));
		instance->o_state = ONCORE_INIT;
		cp = "state = ONCORE_INIT";
		record_clock_stats(&(instance->peer->srcadr), cp);
	}
}



/* Ba, Ea and Ha come here */

static void
oncore_msg_BaEaHa(
	struct instance *instance,
	u_char *buf,
	size_t len
	)
{
	const char	*cp;
	char		Msg[160], Cmd[20];
	u_char		*vp;	/* pointer to start of shared mem for Ba/Ea/Ha */
	size_t		Len;

	/* At the beginning of Ea here there are various 'timers'.
	 * We enter Ea 1/sec, and since the upper levels of NTP have usurped
	 * the use of timers, we use the 1/sec entry to Ea to do things that
	 * we would normally do with timers...
	 */

	if (instance->count) {
		if (instance->count++ < 5)	/* make sure results are stable, using position */
			return;
		instance->count = 0;
	}

	if (instance->o_state != ONCORE_ALMANAC && instance->o_state != ONCORE_RUN)
		return;

	Len = len+3;		/* message length @@ -> CR,LF */
	memcpy(instance->Ea, buf, Len); 	/* Ba, Ea or Ha */

	if (buf[2] == 'B') {			/* 6chan */
		if (instance->Ea[64]&0x8)
			instance->mode = MODE_0D;
		else if (instance->Ea[64]&0x10)
			instance->mode = MODE_2D;
		else if (instance->Ea[64]&0x20)
			instance->mode = MODE_3D;
	} else if (buf[2] == 'E') {		/* 8chan */
		if (instance->Ea[72]&0x8)
			instance->mode = MODE_0D;
		else if (instance->Ea[72]&0x10)
			instance->mode = MODE_2D;
		else if (instance->Ea[72]&0x20)
			instance->mode = MODE_3D;
	} else if (buf[2] == 'H') {		/* 12chan */
		int bits;

		bits = (instance->Ea[129]>>5) & 0x7;	    /* actually Ha */
		if (bits == 0x4)
			instance->mode = MODE_0D;
		else if (bits == 0x6)
			instance->mode = MODE_2D;
		else if (bits == 0x7)
			instance->mode = MODE_3D;
	}

	vp = (u_char) 0;	/* just to keep compiler happy */
	if (instance->chan == 6) {
		instance->rsm.bad_almanac = instance->Ea[64]&0x1;
		instance->rsm.bad_fix	  = instance->Ea[64]&0x52;
		vp = &instance->shmem[instance->shmem_Ba];
	} else if (instance->chan == 8) {
		instance->rsm.bad_almanac = instance->Ea[72]&0x1;
		instance->rsm.bad_fix	  = instance->Ea[72]&0x52;
		vp = &instance->shmem[instance->shmem_Ea];
	} else if (instance->chan == 12) {
		int bits1, bits2;

		bits1 = (instance->Ea[129]>>5) & 0x7;	     /* actually Ha */
		bits2 = instance->Ea[130];
		instance->rsm.bad_almanac = (bits2 & 0x80);
		instance->rsm.bad_fix	  = (bits2 & 0x8) || (bits1 == 0x2);
					  /* too few sat     Bad Geom	  */
		vp = &instance->shmem[instance->shmem_Ha];
#if 0
fprintf(stderr, "ONCORE: DEBUG BITS: (%x %x), (%x %x),  %x %x %x %x %x\n",
		instance->Ea[129], instance->Ea[130], bits1, bits2, instance->mode == MODE_0D, instance->mode == MODE_2D,
		instance->mode == MODE_3D, instance->rsm.bad_almanac, instance->rsm.bad_fix);
#endif
	}

	/* Here calculate dH = GPS - MSL for output message */
	/* also set Altitude Hold mode if GT */

	if (!instance->have_dH) {
		int	GPS, MSL;

		instance->have_dH++;
		if (instance->chan == 12) {
			GPS = buf_w32(&instance->Ea[39]);
			MSL = buf_w32(&instance->Ea[43]);
		} else {
			GPS = buf_w32(&instance->Ea[23]);
			MSL = buf_w32(&instance->Ea[27]);
		}
		instance->dH = GPS - MSL;
		instance->dH /= 100.;

		if (MSL) {	/* not set ! */
			sprintf(Msg, "dH = (GPS - MSL) = %.2fm", instance->dH);
			record_clock_stats(&(instance->peer->srcadr), Msg);
		}

		/* stuck in here as it only gets done once */

		if (instance->chan != 12 && !instance->saw_At) {
			cp = "Not Good, no @@At command, must be a GT/GT+";
			record_clock_stats(&(instance->peer->srcadr), cp);
			oncore_sendmsg(instance->ttyfd, oncore_cmd_Av1, sizeof(oncore_cmd_Av1));
		}
	}

	/*
	 * For instance->site_survey to be ONCORE_SS_TESTING, this must be the first
	 * time thru @@Ea.  There are two choices
	 *   (a) We did not get a response to the @@At0 or @@At2 commands,
	 *	   must be a GT/GT+/SL with no position hold mode.
	 *   (b) Saw the @@At0, @@At2 commands, but @@At2 failed,
	 *	   must be a VP or older UT which doesnt have Site Survey mode.
	 *	   We will have to do it ourselves.
	 */

	if (instance->site_survey == ONCORE_SS_TESTING) {	/* first time thru Ea */
		sprintf(Msg, "Initiating software 3D site survey (%d samples)",
				POS_HOLD_AVERAGE);
		record_clock_stats(&(instance->peer->srcadr), Msg);
		instance->site_survey = ONCORE_SS_SW;

		instance->ss_lat = instance->ss_long = instance->ss_ht = 0;
		oncore_sendmsg(instance->ttyfd, oncore_cmd_At0, sizeof(oncore_cmd_At0)); /* disable */
	}

	if (instance->shmem) {
		int	i;

		i = 0;
		if (instance->mode == MODE_0D)	       /* 0D, Position Hold */
			i = 1;
		else if (instance->mode == MODE_2D)    /* 2D, Altitude Hold */
			i = 2;
		else if (instance->mode == MODE_3D)    /* 3D fix */
			i = 3;
		if (i) {
			i *= (Len+3);
			vp[i + 2]++;
			memcpy(&vp[i+3], buf, Len);
		}
	}

	/* Almanac mode, waiting for Almanac, cant do anything till we have it */
	/* When we have an almanac, start the En/Bn messages */

	if (instance->o_state == ONCORE_ALMANAC) {
		if (instance->rsm.bad_almanac) {
			if (debug)
				printf("ONCORE: waiting for almanac\n");
			return;
		} else {  /* Here we have almanac.
			     Start TRAIM (@@En/@@Bn) dependant on TRAIM flag.
			     If flag == -1, then we dont know if this unit supports
			     traim, and we issue the command and then wait up to
			     5sec to see if we get a reply */

			if (instance->traim != 0) {	/* either yes or unknown */
				if (instance->chan == 6)
					oncore_sendmsg(instance->ttyfd, oncore_cmd_Bn, sizeof(oncore_cmd_Bn));
				else if (instance->chan == 8)
					oncore_sendmsg(instance->ttyfd, oncore_cmd_En, sizeof(oncore_cmd_En));

				if (instance->traim == -1)
					instance->traim_delay = 1;
			}
			instance->o_state = ONCORE_RUN;
			cp = "state = ONCORE_RUN";
			record_clock_stats(&(instance->peer->srcadr), cp);
		}
	}

	/*
	 * check if timer active
	 * if it hasnt been cleared, then @@En/@@Bn did not respond
	 */

	if (instance->traim_delay) {
		if (instance->traim_delay++ > 5) {
			instance->traim = 0;
			instance->traim_delay = 0;
			cp = "ONCORE: Did not detect TRAIM response, TRAIM = OFF";
			record_clock_stats(&(instance->peer->srcadr), cp);
		}
	}

	/*
	 * must be ONCORE_RUN if we are here.
	 */

	instance->pp->year = buf[6]*256+buf[7];
	instance->pp->day = ymd2yd(buf[6]*256+buf[7], buf[4], buf[5]);
	instance->pp->hour = buf[8];
	instance->pp->minute = buf[9];
	instance->pp->second = buf[10];

	/*
	 * Check to see if Hardware SiteSurvey has Finished.
	 */

	if ((instance->site_survey == ONCORE_SS_HW) && !(instance->Ea[37] & 0x20)) {
		record_clock_stats(&(instance->peer->srcadr), "Now in 0D mode");
		instance->site_survey = ONCORE_SS_DONE;
	}

	if (!instance->printed && instance->site_survey == ONCORE_SS_DONE) {
		instance->printed = 1;
			/* Read back Position Hold Params (cant for GT) */
		if (instance->saw_At)
			oncore_sendmsg(instance->ttyfd, oncore_cmd_Asx,  sizeof(oncore_cmd_Asx));
		else
			oncore_print_As(instance);

			/* Read back PPS Offset for Output */
			/* Nb. This will fail silently for early UT (no plus) and M12 models */
		oncore_sendmsg(instance->ttyfd, oncore_cmd_Ayx,  sizeof(oncore_cmd_Ayx));

			/* Read back Cable Delay for Output */
		oncore_sendmsg(instance->ttyfd, oncore_cmd_Azx,  sizeof(oncore_cmd_Azx));
	}

	/*
	 * Check the leap second status once per day.
	 */

	if (instance->Bj_day != buf[5]) {     /* do this 1/day */
		instance->Bj_day = buf[5];

		if (instance->chan == 12)
			oncore_sendmsg(instance->ttyfd, oncore_cmd_Gj, sizeof(oncore_cmd_Gj));
		else {
			/*
			 * The following additional check, checking for June/December, is a
			 * workaround for incorrect ONCORE firmware.  The oncore starts
			 * reporting the leap second when the GPS satellite data message
			 * (page 18, subframe 4) is updated to a date in the future, which
			 * can be several months before the leap second.  WWV and other
			 * services seem to wait until the month of the event to turn
			 * on their indicators (which is usually a single bit).
			 */

			if ((buf[4] == 6) || (buf[4] == 12))
				oncore_sendmsg(instance->ttyfd, oncore_cmd_Bj, sizeof(oncore_cmd_Bj));
		}
	}

	/*
	 * if SHMEM active, every 15s, steal one 'tick' to get 2D or 3D posn.
	 */

	if (instance->shmem && instance->shmem_Posn && (instance->site_survey == ONCORE_SS_DONE)) {	/* dont screw up the SS by changing mode */
		if (instance->pp->second%15 == 3) {	/* start the sequence */
			instance->shmem_reset = 1;
			if (instance->chan == 12) {
				if (instance->shmem_Posn == 2)
					oncore_sendmsg(instance->ttyfd, oncore_cmd_Gd2,  sizeof(oncore_cmd_Gd2));  /* 2D */
				else
					oncore_sendmsg(instance->ttyfd, oncore_cmd_Gd0,  sizeof(oncore_cmd_Gd0));  /* 3D */
			} else {
				if (instance->saw_At) {
					oncore_sendmsg(instance->ttyfd, oncore_cmd_At0, sizeof(oncore_cmd_At0)); /* out of 0D to 3D mode */
					if (instance->shmem_Posn == 2)
						oncore_sendmsg(instance->ttyfd, oncore_cmd_Av1, sizeof(oncore_cmd_Av1));   /* 3D to 2D mode */
				} else
					oncore_sendmsg(instance->ttyfd, oncore_cmd_Av0, sizeof(oncore_cmd_Av0));
			}
		} else if (instance->shmem_reset || (instance->mode != MODE_0D)) {
			instance->shmem_reset = 0;
			if (instance->chan == 12)
				oncore_sendmsg(instance->ttyfd, oncore_cmd_Gd1,  sizeof(oncore_cmd_Gd1));	/* 0D */
			else {
				if (instance->saw_At) {
					if (instance->mode == MODE_2D)
						oncore_sendmsg(instance->ttyfd, oncore_cmd_Av0, sizeof(oncore_cmd_Av0)); /* 2D -> 3D or 0D */
					oncore_sendmsg(instance->ttyfd, oncore_cmd_At1,  sizeof(oncore_cmd_At1));	 /* to 0D mode */
				} else
					oncore_sendmsg(instance->ttyfd, oncore_cmd_Av1,  sizeof(oncore_cmd_Av1));
			}
		}
	}

	if (instance->traim == 0)	/* NO traim, go get tick */
		oncore_get_timestamp(instance, instance->offset, instance->offset);

	if (instance->site_survey != ONCORE_SS_SW)
		return;

	/*
	 * We have to average our own position for the Position Hold Mode
	 *   We use Heights from the GPS ellipsoid.
	 */

	if (instance->rsm.bad_fix)	/* Not if poor geometry or less than 3 sats */
		return;

	if (instance->mode != MODE_3D)	/* Only 3D Fix */
		return;

	instance->ss_lat  += buf_w32(&instance->Ea[15]);
	instance->ss_long += buf_w32(&instance->Ea[19]);
	instance->ss_ht   += buf_w32(&instance->Ea[23]);  /* GPS ellipsoid */
	instance->ss_count++;

	if (instance->ss_count != POS_HOLD_AVERAGE)
		return;

	instance->ss_lat  /= POS_HOLD_AVERAGE;
	instance->ss_long /= POS_HOLD_AVERAGE;
	instance->ss_ht   /= POS_HOLD_AVERAGE;

	sprintf(Msg, "Surveyed posn:  lat %.3f long %.3f ht %.3f",
			instance->ss_lat, instance->ss_long, instance->ss_ht);
	record_clock_stats(&(instance->peer->srcadr), Msg);

	/* set newly determined position as 3D Position hold position */

	memcpy(Cmd, oncore_cmd_As, sizeof(oncore_cmd_As));
	w32_buf(&Cmd[2],  (int) instance->ss_lat);
	w32_buf(&Cmd[6],  (int) instance->ss_long);
	w32_buf(&Cmd[10], (int) instance->ss_ht);
	Cmd[14] = 0;
	oncore_sendmsg(instance->ttyfd, Cmd, sizeof(oncore_cmd_As));

	/* set height seperately for 2D */

	memcpy(Cmd, oncore_cmd_Au, sizeof(oncore_cmd_Au));
	w32_buf(&Cmd[2], (int) instance->ss_ht);
	Cmd[6] = 0;
	oncore_sendmsg(instance->ttyfd, Cmd, sizeof(oncore_cmd_Au));

	/* and set Position Hold */

	if (instance->chan == 12)
		oncore_sendmsg(instance->ttyfd, oncore_cmd_Gd1, sizeof(oncore_cmd_Gd1));
	else {
		if (instance->saw_At)
			oncore_sendmsg(instance->ttyfd, oncore_cmd_At1, sizeof(oncore_cmd_At1));
		else
			oncore_sendmsg(instance->ttyfd, oncore_cmd_Av1, sizeof(oncore_cmd_Av1));
	}

	record_clock_stats(&(instance->peer->srcadr), "Now in 0D mode");
	instance->site_survey = ONCORE_SS_DONE;
}



static void
oncore_msg_BnEn(
	struct instance *instance,
	u_char *buf,
	size_t len
	)
{
	long	dt1, dt2;
	char	*cp;

	if (instance->o_state != ONCORE_RUN)
		return;

	if (instance->traim_delay) {	 /* flag that @@En/@@Bn returned */
			instance->traim = 1;
			instance->traim_delay = 0;
			cp = "ONCORE: Detected TRAIM, TRAIM = ON";
			record_clock_stats(&(instance->peer->srcadr), cp);
	}

	memcpy(instance->En, buf, len); 	/* En or Bn */

	/* If Time RAIM doesn't like it, don't trust it */

	if (instance->En[21])
		return;

	dt1 = instance->saw_tooth + instance->offset;	 /* dt this time step */
	instance->saw_tooth = (s_char) instance->En[25]; /* update for next time */
	dt2 = instance->saw_tooth + instance->offset;	 /* dt next time step */

	oncore_get_timestamp(instance, dt1, dt2);
}



static void
oncore_get_timestamp(
	struct instance *instance,
	long dt1,	/* tick offset THIS time step */
	long dt2	/* tick offset NEXT time step */
	)
{
	int     Rsm;
	u_long  i, j;
	l_fp ts, ts_tmp;
	double dmy;
#ifdef HAVE_STRUCT_TIMESPEC
	struct timespec *tsp = 0;
#else
	struct timeval	*tsp = 0;
#endif
#ifdef HAVE_PPSAPI
	int	current_mode;
	pps_params_t current_params;
	struct timespec timeout;
	pps_info_t pps_i;
#else  /* ! HAVE_PPSAPI */
#ifdef HAVE_CIOGETEV
	struct ppsclockev ev;
	int r = CIOGETEV;
#endif
#ifdef HAVE_TIOCGPPSEV
	struct ppsclockev ev;
	int r = TIOCGPPSEV;
#endif
#if	TIOCDCDTIMESTAMP
	struct timeval	tv;
#endif
#endif	/* ! HAVE_PPS_API */

	if ((instance->site_survey == ONCORE_SS_DONE) && (instance->mode != MODE_0D))
		return;

	/* Don't do anything without an almanac to define the GPS->UTC delta */

	if (instance->rsm.bad_almanac)
		return;

#ifdef HAVE_PPSAPI
	j = instance->ev_serial;
	timeout.tv_sec = 0;
	timeout.tv_nsec = 0;
	if (time_pps_fetch(instance->pps_h, PPS_TSFMT_TSPEC, &pps_i,
	    &timeout) < 0) {
		printf("ONCORE: time_pps_fetch failed\n");
		return;
	}

	if (instance->assert) {
		tsp = &pps_i.assert_timestamp;

		if (debug > 2) {
			i = (u_long) pps_i.assert_sequence;
#ifdef HAVE_STRUCT_TIMESPEC
			printf("ONCORE[%d]: serial/j (%lu, %lu) %ld.%09ld\n",
			    instance->unit, i, j,
			    (long)tsp->tv_sec, (long)tsp->tv_nsec);
#else
			printf("ONCORE[%d]: serial/j (%lu, %lu) %ld.%06ld\n",
			    instance->unit, i, j,
			    (long)tsp->tv_sec, (long)tsp->tv_usec);
#endif
		}

		if (pps_i.assert_sequence == j) {
			printf("ONCORE: oncore_get_timestamp, error serial pps\n");
			return;
		}
		instance->ev_serial = pps_i.assert_sequence;
	} else {
		tsp = &pps_i.clear_timestamp;

		if (debug > 2) {
			i = (u_long) pps_i.clear_sequence;
#ifdef HAVE_STRUCT_TIMESPEC
			printf("ONCORE[%d]: serial/j (%lu, %lu) %ld.%09ld\n",
			    instance->unit, i, j, (long)tsp->tv_sec, (long)tsp->tv_nsec);
#else
			printf("ONCORE[%d]: serial/j (%lu, %lu) %ld.%06ld\n",
			    instance->unit, i, j, (long)tsp->tv_sec, (long)tsp->tv_usec);
#endif
		}

		if (pps_i.clear_sequence == j) {
			printf("ONCORE: oncore_get_timestamp, error serial pps\n");
			return;
		}
		instance->ev_serial = pps_i.clear_sequence;
	}

	/* convert timespec -> ntp l_fp */

	dmy = tsp->tv_nsec;
	dmy /= 1e9;
	ts.l_uf =  dmy * 4294967296.0;
	ts.l_ui = tsp->tv_sec;
#if 0
     alternate code for previous 4 lines is
	dmy = 1.0e-9*tsp->tv_nsec;	/* fractional part */
	DTOLFP(dmy, &ts);
	dmy = tsp->tv_sec;		/* integer part */
	DTOLFP(dmy, &ts_tmp);
	L_ADD(&ts, &ts_tmp);
     or more simply
	dmy = 1.0e-9*tsp->tv_nsec;	/* fractional part */
	DTOLFP(dmy, &ts);
	ts.l_ui = tsp->tv_sec;
#endif	/* 0 */
#else
# if defined(HAVE_TIOCGPPSEV) || defined(HAVE_CIOGETEV)
	j = instance->ev_serial;
	if (ioctl(instance->ppsfd, r, (caddr_t) &ev) < 0) {
		perror("ONCORE: IOCTL:");
		return;
	}

	tsp = &ev.tv;

	if (debug > 2)
#ifdef HAVE_STRUCT_TIMESPEC
		printf("ONCORE: serial/j (%d, %d) %ld.%09ld\n",
			ev.serial, j, tsp->tv_sec, tsp->tv_nsec);
#else
		printf("ONCORE: serial/j (%d, %d) %ld.%06ld\n",
			ev.serial, j, tsp->tv_sec, tsp->tv_usec);
#endif

	if (ev.serial == j) {
		printf("ONCORE: oncore_get_timestamp, error serial pps\n");
		return;
	}
	instance->ev_serial = ev.serial;

	/* convert timeval -> ntp l_fp */

	TVTOTS(tsp, &ts);
# else
#  if defined(TIOCDCDTIMESTAMP)
	if(ioctl(instance->ppsfd, TIOCDCDTIMESTAMP, &tv) < 0) {
		perror("ONCORE: ioctl(TIOCDCDTIMESTAMP)");
		return;
	}
	tsp = &tv;
	TVTOTS(tsp, &ts);
#  else
#error "Cannot compile -- no PPS mechanism configured!"
#  endif
# endif
#endif
	/* now have timestamp in ts */
	/* add in saw_tooth and offset, these will be ZERO if no TRAIM */

	/* saw_tooth not really necessary if using TIMEVAL */
	/* since its only precise to us, but do it anyway. */

	/* offset in ns, and is positive (late), we subtract */
	/* to put the PPS time transition back where it belongs */

#ifdef HAVE_PPSAPI
	/* must hand the offset for the NEXT sec off to the Kernel to do */
	/* the addition, so that the Kernel PLL sees the offset too */

	if (instance->assert)
		instance->pps_p.assert_offset.tv_nsec = -dt2;
	else
		instance->pps_p.clear_offset.tv_nsec  = -dt2;

	/* The following code is necessary, and not just a time_pps_setparams,
	 * using the saved instance->pps_p, since some other process on the
	 * machine may have diddled with the mode bits (say adding something
	 * that it needs).  We take what is there and ADD what we need.
	 * [[ The results from the time_pps_getcap is unlikely to change so
	 *    we could probably just save it, but I choose to do the call ]]
	 * Unfortunately, there is only ONE set of mode bits in the kernel per
	 * interface, and not one set for each open handle.
	 *
	 * There is still a race condition here where we might mess up someone
	 * elses mode, but if he is being careful too, he should survive.
	 */

	if (time_pps_getcap(instance->pps_h, &current_mode) < 0) {
		msyslog(LOG_ERR,
		    "refclock_ioctl: time_pps_getcap failed: %m");
		return;
	}

	if (time_pps_getparams(instance->pps_h, &current_params) < 0) {
		msyslog(LOG_ERR,
		    "refclock_ioctl: time_pps_getparams failed: %m");
		return;
	}

		/* or current and mine */
	current_params.mode |= instance->pps_p.mode;
		/* but only set whats legal */
	current_params.mode &= current_mode;

	current_params.assert_offset.tv_sec = 0;
	current_params.assert_offset.tv_nsec = -dt2;
	current_params.clear_offset.tv_sec = 0;
	current_params.clear_offset.tv_nsec = -dt2;

	if (time_pps_setparams(instance->pps_h, &current_params))
		perror("time_pps_setparams");
#else
	/* if not PPSAPI, no way to inform kernel of OFFSET, just add the */
	/* offset for THIS second */

	dmy = -1.0e-9*dt1;
	DTOLFP(dmy, &ts_tmp);
	L_ADD(&ts, &ts_tmp);
#endif
	/* have time from UNIX origin, convert to NTP origin. */

	ts.l_ui += JAN_1970;
	instance->pp->lastrec = ts;
	instance->pp->msec = 0;

	ts_tmp = ts;
	ts_tmp.l_ui = 0;	/* zero integer part */
	LFPTOD(&ts_tmp, dmy);	/* convert fractional part to a double */
	j = 1.0e9*dmy;		/* then to integer ns */

	Rsm = 0;
	if (instance->chan == 6)
		Rsm = instance->Ea[64];
	else if (instance->chan == 8)
		Rsm = instance->Ea[72];
	else if (instance->chan == 12)
		Rsm = ((instance->Ea[129]<<8) | instance->Ea[130]);

	if (instance->chan == 6 || instance->chan == 8) {
		sprintf(instance->pp->a_lastcode,	/* MAX length 128, currently at 117 */
		    "%u.%09lu %d %d %2d %2d %2d %2ld rstat   %02x dop %4.1f nsat %2d,%d traim %d sigma %d neg-sawtooth %3d sat %d%d%d%d%d%d%d%d",
		    ts.l_ui, j,
		    instance->pp->year, instance->pp->day,
		    instance->pp->hour, instance->pp->minute, instance->pp->second,
		    (long) tsp->tv_sec % 60,
		    Rsm, 0.1*(256*instance->Ea[35]+instance->Ea[36]),
		    /*rsat	dop */
		    instance->Ea[38], instance->Ea[39], instance->En[21],
		    /*	nsat visible,	  nsat tracked,     traim */
		    instance->En[23]*256+instance->En[24], (s_char) instance->En[25],
		    /* sigma				   neg-sawtooth */
	  /*sat*/   instance->Ea[41], instance->Ea[45], instance->Ea[49], instance->Ea[53],
		    instance->Ea[57], instance->Ea[61], instance->Ea[65], instance->Ea[69]
		    );					/* will be 0 for 6 chan */
	} else if (instance->chan == 12) {
		sprintf(instance->pp->a_lastcode,
		    "%u.%09lu %d %d %2d %2d %2d %2ld rstat %02x dop %4.1f nsat %2d,%d sat %d%d%d%d%d%d%d%d%d%d%d%d",
		    ts.l_ui, j,
		    instance->pp->year, instance->pp->day,
		    instance->pp->hour, instance->pp->minute, instance->pp->second,
		    (long) tsp->tv_sec % 60,
		    Rsm, 0.1*(256*instance->Ea[53]+instance->Ea[54]),
		    /*rsat	dop */
		    instance->Ea[55], instance->Ea[56],
		    /*	nsat visible,	  nsat tracked	*/
	  /*sat*/   instance->Ea[58], instance->Ea[64], instance->Ea[70], instance->Ea[76],
		    instance->Ea[82], instance->Ea[88], instance->Ea[94], instance->Ea[100],
		    instance->Ea[106], instance->Ea[112], instance->Ea[118], instance->Ea[124]
		    );
	}

	if (debug > 2) {
		int n;
		n = strlen(instance->pp->a_lastcode);
		printf("ONCORE[%d]: len = %d %s\n", instance->unit, n, instance->pp->a_lastcode);
	}

	if (!refclock_process(instance->pp)) {
		refclock_report(instance->peer, CEVNT_BADTIME);
		return;
	}

	record_clock_stats(&(instance->peer->srcadr), instance->pp->a_lastcode);
	instance->pollcnt = 2;

	if (instance->polled) {
		instance->polled = 0;
/*
		instance->pp->dispersion = instance->pp->skew = 0;
*/
		refclock_receive(instance->peer);
	}
}



/*
 * Try to use Oncore UT+ Auto Survey Feature
 *	If its not there (VP), set flag to do it ourselves.
 */

static void
oncore_msg_At(
	struct instance *instance,
	u_char *buf,
	size_t len
	)
{
	instance->saw_At = 1;
	if (instance->site_survey == ONCORE_SS_TESTING) {
		if (buf[4] == 2) {
			record_clock_stats(&(instance->peer->srcadr),
					"Initiating hardware 3D site survey");
			instance->site_survey = ONCORE_SS_HW;
		}
	}
}



/* get leap-second warning message */

/*
 * @@Bj does NOT behave as documented in current Oncore firmware.
 * It turns on the LEAP indicator when the data is set, and does not,
 * as documented, wait until the beginning of the month when the
 * leap second will occur.
 * Until this firmware bug is fixed, @@Bj is only called in June/December.
 */

static void
oncore_msg_Bj(
	struct instance *instance,
	u_char *buf,
	size_t len
	)
{
	const char	*cp;

	switch(buf[4]) {
	case 1:
		instance->peer->leap = LEAP_ADDSECOND;
		cp = "Set peer.leap to LEAP_ADDSECOND";
		break;
	case 2:
		instance->peer->leap = LEAP_DELSECOND;
		cp = "Set peer.leap to LEAP_DELSECOND";
		break;
	case 0:
	default:
		instance->peer->leap = LEAP_NOWARNING;
		cp = "Set peer.leap to LEAP_NOWARNING";
		break;
	}
	record_clock_stats(&(instance->peer->srcadr), cp);
}

/* Leap Second for M12, gives all info from satellite message */

static void
oncore_msg_Gj(
	struct instance *instance,
	u_char *buf,
	size_t len
	)
{
	int dt;
	char Msg[160], *cp;
	static char *Month[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jly",
		"Aug", "Sep", "Oct", "Nov", "Dec" };

	/* print the message to verify whats there */

	dt = buf[5] - buf[4];

#if 1
	sprintf(Msg, "ONCORE[%d]: Leap Sec Msg: %d %d %d %d %d %d %d %d %d %d",
			instance->unit,
			buf[4], buf[5], 256*buf[6]+buf[7], buf[8], buf[9], buf[10],
			(buf[14]+256*(buf[13]+256*(buf[12]+256*buf[11]))),
			buf[15], buf[16], buf[17]);
	record_clock_stats(&(instance->peer->srcadr), Msg);
#endif
	if (dt) {
		sprintf(Msg, "ONCORE[%d]: Leap second (%d) scheduled for %d%s%d at %d:%d:%d",
			instance->unit,
			dt, buf[9], Month[buf[8]], 256*buf[6]+buf[7],
			buf[15], buf[16], buf[17]);
		record_clock_stats(&(instance->peer->srcadr), Msg);
	}

	/* Only raise warning within a month of the leap second */

	instance->peer->leap = LEAP_NOWARNING;
	cp = "Set peer.leap to LEAP_NOWARNING";

	if (buf[6] == instance->Ea[6] && buf[7] == instance->Ea[7] && /* year */
	    buf[8] == instance->Ea[4]) {	/* month */
		if (dt) {
			if (dt < 0) {
				instance->peer->leap = LEAP_DELSECOND;
				cp = "Set peer.leap to LEAP_DELSECOND";
			} else {
				instance->peer->leap = LEAP_ADDSECOND;
				cp = "Set peer.leap to LEAP_ADDSECOND";
			}
		}
	}
	record_clock_stats(&(instance->peer->srcadr), cp);
}



/*
 * get Position hold position
 */

static void
oncore_msg_As(
	struct instance *instance,
	u_char *buf,
	size_t len
	)
{
	if (!instance->printed || instance->As)
		return;

	instance->As = 1;

	instance->ss_lat  = buf_w32(&buf[4]);
	instance->ss_long = buf_w32(&buf[8]);
	instance->ss_ht   = buf_w32(&buf[12]);

	/* Print out Position */
	oncore_print_As(instance);
}



static void
oncore_print_As(
	struct instance *instance
	)
{
	char Msg[120], ew, ns;
	double xd, xm, xs, yd, ym, ys, hm, hft;
	int idx, idy, is, imx, imy;
	long lat, lon;

	record_clock_stats(&(instance->peer->srcadr), "Posn:");
	ew = 'E';
	lon = instance->ss_long;
	if (lon < 0) {
		ew = 'W';
		lon = -lon;
	}

	ns = 'N';
	lat = instance->ss_lat;
	if (lat < 0) {
		ns = 'S';
		lat = -lat;
	}

	hm = instance->ss_ht/100.;
	hft= hm/0.3048;

	xd = lat/3600000.;	/* lat, lon in int msec arc, ht in cm. */
	yd = lon/3600000.;
	sprintf(Msg, "Lat = %c %11.7fdeg,    Long = %c %11.7fdeg,    Alt = %5.2fm (%5.2fft) GPS", ns, xd, ew, yd, hm, hft);
	record_clock_stats(&(instance->peer->srcadr), Msg);

	idx = xd;
	idy = yd;
	imx = lat%3600000;
	imy = lon%3600000;
	xm = imx/60000.;
	ym = imy/60000.;
	sprintf(Msg, "Lat = %c %3ddeg %7.4fm,   Long = %c %3ddeg %8.5fm,  Alt = %7.2fm (%7.2fft) GPS", ns, idx, xm, ew, idy, ym, hm, hft);
	record_clock_stats(&(instance->peer->srcadr), Msg);

	imx = xm;
	imy = ym;
	is  = lat%60000;
	xs  = is/1000.;
	is  = lon%60000;
	ys  = is/1000.;
	sprintf(Msg, "Lat = %c %3ddeg %2dm %5.2fs, Long = %c %3ddeg %2dm %5.2fs, Alt = %7.2fm (%7.2fft) GPS", ns, idx, imx, xs, ew, idy, imy, ys, hm, hft);
	record_clock_stats(&(instance->peer->srcadr), Msg);
}



/*
 * get PPS Offset
 * Nb. @@Ay is not supported for early UT (no plus) model
 */

static void
oncore_msg_Ay(
	struct instance *instance,
	u_char *buf,
	size_t len
	)
{
	char Msg[120];

	if (!instance->printed || instance->Ay)
		return;

	instance->Ay = 1;

	instance->offset = buf_w32(&buf[4]);

	sprintf(Msg, "PPS Offset  is set to %ld ns", instance->offset);
	record_clock_stats(&(instance->peer->srcadr), Msg);
}



/*
 * get Cable Delay
 */

static void
oncore_msg_Az(
	struct instance *instance,
	u_char *buf,
	size_t len
	)
{
	char Msg[120];

	if (!instance->printed || instance->Az)
		return;

	instance->Az = 1;

	instance->delay = buf_w32(&buf[4]);

	sprintf(Msg, "Cable delay is set to %ld ns", instance->delay);
	record_clock_stats(&(instance->peer->srcadr), Msg);
}

static void
oncore_msg_Sz(
	struct instance *instance,
	u_char *buf,
	size_t len
	)
{
	const char *cp;

	cp = "Oncore: System Failure at Power On";
	if (instance && instance->peer) {
		record_clock_stats(&(instance->peer->srcadr), cp);
		oncore_shutdown(instance->unit, instance->peer);
	}
}

#else
int refclock_oncore_bs;
#endif /* REFCLOCK */
