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
 * --------------------------------------------------------------------------
 * This code uses the two devices
 *      /dev/oncore.serial.n
 *      /dev/oncore.pps.n
 * which may be linked to the same device.
 * and can read initialization data from the file
 *      /etc/ntp.oncoreN (where n and N are the unit number, viz 127.127.30.N)
 *  or	/etc/ntp.oncore
 * --------------------------------------------------------------------------
 * Reg.Clemens <reg@dwf.com> Sep98.
 *  Original code written for FreeBSD.
 *  With these mods it works on SunOS, Solaris (untested) and Linux
 *    (RedHat 5.1 2.0.35 + PPSKit, 2.1.126 + changes).
 *
 *  Lat,Long,Ht, cable-delay, offset, and the ReceiverID (along with the
 *  state machine state) are printed to CLOCKSTATS if that file is enabled
 *  in /etc/ntp.conf.
 *
 * --------------------------------------------------------------------------
 */

/*
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
 * copy of all types of messages we recognize.  This file can be mmap(2)'ed
 * by monitoring and statistics programs.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#if defined(REFCLOCK) && defined(CLOCK_ONCORE)

#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/time.h>
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
#  ifdef HAVE_TIMEPPS_H
#    include <timepps.h>
# else
#  ifdef HAVE_SYS_TIMEPPS_H
#    include <sys/timepps.h>
#  endif
# endif
#endif

#ifdef HAVE_SYS_SIO_H
# include <sys/sio.h>
#endif

#include "ntpd.h"
#include "ntp_io.h"
#include "ntp_unixtime.h"
#include "ntp_refclock.h"
#include "ntp_stdlib.h"

#ifdef HAVE_SYS_TERMIOS_H
#include <sys/termios.h>
#endif

#ifdef HAVE_SYS_PPSCLOCK_H
# include <sys/ppsclock.h>
#endif

#ifndef HAVE_STRUCT_PPSCLOCKEV
struct ppsclockev {
# ifdef HAVE_TIMESPEC
	struct timespec tv;
# else
	struct timeval tv;
# endif
	u_int serial;
};
#endif /* not HAVE_STRUCT_PPSCLOCKEV */

enum receive_state {
	ONCORE_NO_IDEA,
	ONCORE_RESET_SENT,
	ONCORE_TEST_SENT,
	ONCORE_ID_SENT,
	ONCORE_ALMANAC,
	ONCORE_RUN
};

enum site_survey_state {
	ONCORE_SS_UNKNOWN,
	ONCORE_SS_HW,
	ONCORE_SS_SW,
	ONCORE_SS_DONE
};

struct instance {
	int	unit;		/* 127.127.30.unit */
	int	ttyfd;		/* TTY file descriptor */
	int	ppsfd;		/* PPS file descriptor */
	int	statusfd;	/* Status shm descriptor */
	u_char	*shmem;
#ifdef HAVE_PPSAPI
	pps_handle_t pps_h;
	pps_params_t pps_p;
#endif
	enum receive_state o_state;		/* Receive state */

	enum site_survey_state site_survey;	/* Site Survey state */

	struct	refclockproc *pp;
	struct	peer *peer;

	int	Bj_day;

	long	delay;		/* ns */
	long	offset; 	/* ns */

	double	ss_lat;
	double	ss_long;
	double	ss_ht;
	int	ss_count;
	u_char	ss_ht_type;
	u_char  posn_set;

	u_char  printed;
	u_char  polled;
	int	pollcnt;
	u_int	ev_serial;
	int	Rcvptr;
	u_char	Rcvbuf[500];
	u_char	Ea[77];
	u_char	En[70];
	u_char	Cj[300];
	u_char	As;
	u_char	Ay;
	u_char	Az;
	u_char	init_type;
	s_char	saw_tooth;
	u_char  timeout;        /* flag to retry Cj after Fa reset */
	s_char  assert;
};

#define rcvbuf	instance->Rcvbuf
#define rcvptr	instance->Rcvptr

static	void	oncore_consume	  P((struct instance *));
static	void	oncore_poll	  P((int, struct peer *));
static	void	oncore_read_config	P((struct instance *));
static	void	oncore_receive	  P((struct recvbuf *));
static	void	oncore_sendmsg	  P((int fd, u_char *, u_int));
static	void	oncore_shutdown   P((int, struct peer *));
static	int	oncore_start	  P((int, struct peer *));

static	void	oncore_msg_any	P((struct instance *, u_char *, u_int, int));
static	void	oncore_msg_As	P((struct instance *, u_char *, u_int));
static	void	oncore_msg_At	P((struct instance *, u_char *, u_int));
static	void	oncore_msg_Ay	P((struct instance *, u_char *, u_int));
static	void	oncore_msg_Az	P((struct instance *, u_char *, u_int));
static	void	oncore_msg_Bj	P((struct instance *, u_char *, u_int));
static	void	oncore_msg_Cb	P((struct instance *, u_char *, u_int));
static	void	oncore_msg_Cf	P((struct instance *, u_char *, u_int));
static	void	oncore_msg_Cj	P((struct instance *, u_char *, u_int));
static	void	oncore_msg_Ea	P((struct instance *, u_char *, u_int));
static	void	oncore_msg_En	P((struct instance *, u_char *, u_int));
static	void	oncore_msg_Fa	P((struct instance *, u_char *, u_int));

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
 * for the the UT or VP Oncore.
 */

static struct msg_desc {
	const char	flag[3];
	const int	len;
	void		(*handler) P((struct instance *, u_char *, u_int));
	const char	*fmt;
	int		shmem;
} oncore_messages[] = {
	/* Ea and En first since they're most common */
	{ "Ea",  76,    oncore_msg_Ea,  "mdyyhmsffffaaaaoooohhhhmmmmvvhhddtntimsdimsdimsdimsdimsdimsdimsdimsdsC" },
	{ "En",  69,    oncore_msg_En,  "otaapxxxxxxxxxxpysreensffffsffffsffffsffffsffffsffffsffffsffffC" },
	{ "Ab",  10,    0,              "" },
	{ "Ac",  11,    0,              "" },
	{ "Ad",  11,    0,              "" },
	{ "Ae",  11,    0,              "" },
	{ "Af",  15,    0,              "" },
	{ "As",  20,    oncore_msg_As,  "" },
	{ "At",   8,    oncore_msg_At,  "" },
	{ "Aw",   8,    0,              "" },
	{ "Ay",  11,    oncore_msg_Ay,  "" },
	{ "Az",  11,    oncore_msg_Az,  "" },
	{ "AB",   8,    0,              "" },
	{ "Bb",  92,    0,              "" },
	{ "Bj",   8,    oncore_msg_Bj,  "" },
	{ "Cb",  33,    oncore_msg_Cb,  "" },
	{ "Cf",   7,    oncore_msg_Cf,  "" },
	{ "Cg",   8,    0,              "" },
	{ "Ch",   9,    0,              "" },
	{ "Cj", 294,    oncore_msg_Cj,  "" },
	{ "Ek",  71,    0,              "" },
	{ "Fa",   9,    oncore_msg_Fa,  "" },
	{ "Sz",   8,    0,              "" },
	{ {0},	  7,	0, ""}
};

static unsigned int oncore_shmem_Cb;

/*
 * Position Set.
 */
u_char oncore_cmd_Ad[] = { 'A', 'd', 0,0,0,0 };
u_char oncore_cmd_Ae[] = { 'A', 'e', 0,0,0,0 };
u_char oncore_cmd_Af[] = { 'A', 'f', 0,0,0,0, 0 };

/*
 * Position-Hold Mode
 *    Start automatic site survey
 */
static u_char oncore_cmd_At[] = { 'A', 't', 2 };

/*
 * Position-Hold Position
 */
u_char oncore_cmd_As[] = { 'A', 's', 0,0,0,0, 0,0,0,0, 0,0,0,0, 0 };
u_char oncore_cmd_Asx[]= { 'A', 's', 0x7f, 0xff, 0xff, 0xff,
				     0x7f, 0xff, 0xff, 0xff,
				     0x7f, 0xff, 0xff, 0xff, 0xff };

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
static u_char oncore_cmd_Ea[] = { 'E', 'a', 1 };

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
static u_char oncore_cmd_En[] = { 'E', 'n', 1, 1, 0,10, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

/*
 * Self-test
 */
static u_char oncore_cmd_Fa[] = { 'F', 'a' };

#define DEVICE1 	"/dev/oncore.serial.%d"   /* name of serial device */
#define DEVICE2 	"/dev/oncore.pps.%d"   /* name of pps device */
#define INIT_FILE	"/etc/ntp.oncore" /* optional init file */

#define SPEED		B9600		/* Oncore Binary speed (9600 bps) */

/*
 * Assemble and disassemble 32bit signed quantities from a buffer.
 *
 */

	/* to buffer, int w, u_char *buf */
#define w32_buf(buf,w)	{ unsigned int i_tmp;		   \
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

	/* Devices now open, initialize instance structure */

	if (!(instance = (struct instance *)emalloc(sizeof *instance))) {
		perror("malloc");
		close(fd1);
		return (0);
	}
	memset((char *) instance, 0, sizeof *instance);
	pp = peer->procptr;
	pp->unitptr = (caddr_t)instance;
	instance->unit	= unit;
	instance->ttyfd = fd1;
	instance->ppsfd = fd2;

	instance->Bj_day = -1;
	instance->assert = pps_assert;

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
	 *      on before we get here, leave it alone!
	 */

	if (instance->assert) {         /* nb, default or ON */
		instance->pps_p.mode = PPS_CAPTUREASSERT | PPS_OFFSETASSERT;
		instance->pps_p.assert_offset.tv_sec = 0;
		instance->pps_p.assert_offset.tv_nsec = 0;
	} else {
		instance->pps_p.mode = PPS_CAPTURECLEAR  | PPS_OFFSETCLEAR;
		instance->pps_p.clear_offset.tv_sec = 0;
		instance->pps_p.clear_offset.tv_nsec = 0;
	}
	instance->pps_p.mode |= PPS_TSFMT_TSPEC;
	instance->pps_p.mode &= mode;           /* only do it if it is legal */

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
			int     i;
	
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
			}
		}
	}
#endif

	instance->pp = pp;
	instance->peer = peer;
	instance->o_state = ONCORE_NO_IDEA;
	cp = "state = ONCORE_NO_IDEA";
	record_clock_stats(&(instance->peer->srcadr), cp);

	/*
	 * Initialize miscellaneous variables
	 */

	peer->precision = -26;
	peer->minpoll = 4;
	peer->maxpoll = 4;
	pp->clockdesc = "Motorola UT/VP Oncore GPS Receiver";
	memcpy((char *)&pp->refid, "GPS\0", 4);

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
	pp->unitptr = (caddr_t)instance;

	/*
	 * This will start the Oncore receiver.
	 * We send info from config to Oncore later.
	 */

	instance->timeout = 1;
	mode = instance->init_type;
	if (mode == 3 || mode == 4) {
		oncore_sendmsg(instance->ttyfd, oncore_cmd_Cf, sizeof oncore_cmd_Cf);
		instance->o_state = ONCORE_RESET_SENT;
		cp = "state = ONCORE_RESET_SENT";
		record_clock_stats(&(instance->peer->srcadr), cp);
	} else {
		oncore_sendmsg(instance->ttyfd, oncore_cmd_Fa, sizeof oncore_cmd_Fa);
		instance->o_state = ONCORE_TEST_SENT;
		cp = "state = ONCORE_TEST_SENT";
		record_clock_stats(&(instance->peer->srcadr), cp);
	}

	instance->pollcnt = 2;
	return (1);
}



static void
oncore_init_shmem(struct instance *instance, char *filename)
{
#ifdef ONCORE_SHMEM_STATUS
	int i, l, n;
	char *buf;
	struct msg_desc *mp;
	static unsigned int oncore_shmem_length;

	if (oncore_messages[0].shmem == 0) {
		n = 1;
		for (mp = oncore_messages; mp->flag[0]; mp++) {
			mp->shmem = n;
			/* Allocate space for multiplexed almanac */
			if (!strcmp(mp->flag, "Cb")) {
				oncore_shmem_Cb = n;
				n += (mp->len + 2) * 34;
			}
			n += mp->len + 2;
		}
		oncore_shmem_length = n + 2;
		fprintf(stderr, "ONCORE: SHMEM length: %d bytes\n", oncore_shmem_length);
	}
	instance->statusfd = open(filename, O_RDWR|O_CREAT|O_TRUNC, 0644);
	if (instance->statusfd < 0) {
		perror(filename);
		exit(4);
	}
	buf = malloc(oncore_shmem_length);
	if (buf == NULL) {
		perror("malloc");
		exit(4);
	}
	memset(buf, 0, sizeof(buf));
	i = write(instance->statusfd, buf, oncore_shmem_length);
	if (i != oncore_shmem_length) {
		perror(filename);
		exit(4);
	}
	free(buf);
	instance->shmem = (u_char *) mmap(0, oncore_shmem_length, 
	    PROT_READ | PROT_WRITE,
#ifdef MAP_HASSEMAPHORE
			       MAP_HASSEMAPHORE |
#endif
			       MAP_SHARED,
	    instance->statusfd, (off_t)0);
	if (instance->shmem == MAP_FAILED) {
		instance->shmem = 0;
		close (instance->statusfd);
		exit(4);
	}
	for (mp = oncore_messages; mp->flag[0]; mp++) {
		l = mp->shmem;
		instance->shmem[l + 0] = mp->len >> 8;
		instance->shmem[l + 1] = mp->len & 0xff;
		instance->shmem[l + 2] = '@';
		instance->shmem[l + 3] = '@';
		instance->shmem[l + 4] = mp->flag[0];
		instance->shmem[l + 5] = mp->flag[1];
		if (!strcmp(mp->flag, "Cb")) {
			for (i = 1; i < 35; i++) {
				instance->shmem[l + i * 35 + 0] = mp->len >> 8;
				instance->shmem[l + i * 35 + 1] = mp->len & 0xff;
				instance->shmem[l + i * 35 + 2] = '@';
				instance->shmem[l + i * 35 + 3] = '@';
				instance->shmem[l + i * 35 + 4] = mp->flag[0];
				instance->shmem[l + i * 35 + 5] = mp->flag[1];
			}
		}
	}
#endif /* ONCORE_SHMEM_STATUS */
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
 * First we try to open the configuration file /etc/ntp.oncoreN, where
 * N is the unit number viz 127.127.30.N.
 * If we don't find it, then we try the file /etc/ntp.oncore.
 *
 * If we find NEITHER then we don't have the cable delay or PPS offset
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
 *   MODE, LAT, LON, (HT, HTGPS, HTMSL), DELAY, OFFSET
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
 *	Expect to see one line with 'HT' (or 'HTMSL' or 'HTGPS') as first field.
 *	   followed by 1-2 fields.  First is a number, the second is 'FT' or 'M'.
 *	   for feet or meters.	HT is the same as HTGPS.
 *	     HTMSL = HT above mean_sea_level,
 *	     HTGPS = HT above GPS ellipse.
 *
 *	There are two optional lines, starting with DELAY and OFFSET, followed
 *	   by 1 or two fields.	The first is a number (a time) the second is
 *	   'MS', 'US' or 'NS' for miliseconds, microseconds or nanoseconds.
 *	     DELAY  is cable delay, typically a few tens of ns.
 *	     OFFSET is the offset of the PPS pulse from 0. (only fully implemented
 *		with the PPSAPI, we need to be able to tell the Kernel about this
 *		offset if the Kernel PLL is in use, but can only do this presently
 *		when using the PPSAPI interface.  If not using the Kernel PLL,
 *		then there is no problem.
 *
 *	There is another optional line, with either ASSERT or CLEAR on it, which
 *	   determine which transition of the PPS signal is used for timing by the
 *	   PPSAPI.  If neither is present, then ASSERT is assumed.
 *
 * So acceptable input would be
 *	# these are my coordinates (RWC)
 *	LON  -106 34.610
 *	LAT    35 08.999
 *	HT	1589	# could equally well say HT 5215 FT
 *	DELAY  60 ns
 */

	FILE	*fd;
	char	*cp, *cc, *ca, line[100], units[2], device[20];
	int	i, sign, lat_flg, long_flg, ht_flg, mode;
	double	f1, f2, f3;

	sprintf(device, "%s%d", INIT_FILE, instance->unit);
	if ((fd=fopen(device, "r")) == NULL)
		if ((fd=fopen(INIT_FILE, "r")) == NULL) {
			instance->init_type = 4;
			return;
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

		/* Lowercase the command and find the arg */
		for (ca = cc; *ca; ca++) {
			if (isascii((int)*ca) && islower((int)*ca)) {
				*ca = toupper(*ca);
			} else if (isascii((int)*ca) && isspace((int)*ca)) {
				break;
			} else if (*ca == '=') {
				*ca = ' ';
				break;
			}
		}
		
		/* Remove space leading the arg */
		for (; *ca && isascii((int)*ca) && isspace((int)*ca); ca++)
			continue;

		if (!strncmp(cc, "STATUS", 6)) {
			oncore_init_shmem(instance, ca);
			continue;
		}

		/* Uppercase argument as well */
		for (cp = ca; *cp; cp++)
			if (isascii((int)*cp) && islower((int)*cp))
				*cp = toupper(*cp);

		if (!strncmp(cc, "LAT", 3)) {
			f1 = f2 = f3 = 0;
			sscanf(ca, "%lf %lf %lf", &f1, &f2, &f3);
			sign = 1;
			if (f1 < 0) {
				f1 = -f1;
				sign = -1;
			}
			instance->ss_lat = sign*1000*(fabs(f3) + 60*(fabs(f2) + 60*f1)); /*miliseconds*/
			lat_flg++;
		} else if (!strncmp(cc, "LON", 3)) {
			f1 = f2 = f3 = 0;
			sscanf(ca, "%lf %lf %lf", &f1, &f2, &f3);
			sign = 1;
			if (f1 < 0) {
				f1 = -f1;
				sign = -1;
			}
			instance->ss_long = sign*1000*(fabs(f3) + 60*(fabs(f2) + 60*f1)); /*miliseconds*/
			long_flg++;
		} else if (!strncmp(cc, "HT", 2)) {
			if (!strncmp(cc, "HTGPS", 5)) 
				instance->ss_ht_type = 0;
			else if (!strncmp(cc, "HTMSL", 5))
				instance->ss_ht_type = 1;
			else 
				instance->ss_ht_type = 0;
			f1 = 0;
			units[0] = '\0';
			sscanf(ca, "%lf %1s", &f1, units);
			if (units[0] == 'F')
				f1 = 0.3048 * f1;
			instance->ss_ht = 100 * f1;    /* cm */
			ht_flg++;
		} else if (!strncmp(cc, "DELAY", 5)) {
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
			instance->delay = f1;		/* delay in ns */
		} else if (!strncmp(cc, "OFFSET", 6)) {
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
			instance->offset = f1;		/* offset in ns */
		} else if (!strncmp(cc, "MODE", 4)) {
			sscanf(ca, "%d", &mode);
			if (mode < 0 || mode > 4)
				mode = 4;
			instance->init_type = mode;
		} else if (!strncmp(cc, "ASSERT", 6)) {
			instance->assert = 1;
		} else if (!strncmp(cc, "CLEAR", 5)) {
			instance->assert = 0;
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
		if (mode == 1 || mode == 3)
			instance->init_type++;
	}
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
		char    *cp;

		oncore_sendmsg(instance->ttyfd, oncore_cmd_Cj, sizeof oncore_cmd_Cj);
		instance->o_state = ONCORE_ID_SENT;
		cp = "state = ONCORE_ID_SENT";
		record_clock_stats(&(instance->peer->srcadr), cp);
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
 * move dta from NTP to buffer (toss in unlikely case it wont fit)
 */
static void
oncore_receive(
	struct recvbuf *rbufp
	)
{
	u_int	i;
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
				printf("ONCORE: >>> skipping %d chars\n", i);
			if (i != rcvptr)
				memcpy(rcvbuf, rcvbuf+i, (unsigned)(rcvptr-i));
			rcvptr -= i;
		}

		/* Ok, we have a header now */
		l = sizeof(oncore_messages)/sizeof(oncore_messages[0]) -1;
		for(m=0; m<l; m++)
			if (!strncmp(oncore_messages[m].flag, (char *)(rcvbuf+2), 2))
				break;
		l = oncore_messages[m].len;
#if 0
		if (debug > 3)
			printf("ONCORE: GOT: %c%c  %d of %d entry %d\n", rcvbuf[2], rcvbuf[3], rcvptr, l, m);
#endif
		/* Got the entire message ? */

		if (rcvptr < l)
			return;

		/* Check the checksum */

		j = 0;
		for (i = 2; i < l-3; i++)
			j ^= rcvbuf[i];
		if (j == rcvbuf[l-3]) {
			if (instance->shmem != NULL) 
				memcpy(instance->shmem + oncore_messages[m].shmem + 2,
				    rcvbuf, l);
			oncore_msg_any(instance, rcvbuf, (unsigned) (l-3), m);
			if (oncore_messages[m].handler)
				oncore_messages[m].handler(instance, rcvbuf, (unsigned) (l-3));
		} else if (debug) {
			printf("ONCORE: Checksum mismatch! calc %o is %o\n", j, rcvbuf[l-3]);
			printf("ONCORE: @@%c%c ", rcvbuf[2], rcvbuf[3]);
			for (i=4; i<l; i++)
				printf("%03o ", rcvbuf[i]);
			printf("\n");
		}

		if (l != rcvptr)
			memcpy(rcvbuf, rcvbuf+l, (unsigned) (rcvptr-l));
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
	u_int len
	)
{
	u_char cs = 0;

	printf("ONCORE: Send @@%c%c %d\n", ptr[0], ptr[1], len);
	write(fd, "@@", 2);
	write(fd, ptr, len);
	while (len--)
		cs ^= *ptr++;
	write(fd, &cs, 1);
	write(fd, "\r\n", 2);
}



static void
oncore_msg_any(
	struct instance *instance,
	u_char *buf,
	u_int len,
	int idx
	)
{
	int i;
	const char *fmt = oncore_messages[idx].fmt;
	const char *p;
	struct timeval tv;

	if (debug > 3) {
		GETTIMEOFDAY(&tv, 0);
		printf("ONCORE: %ld.%06ld\n", (long) tv.tv_sec, (long) tv.tv_usec);

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
	u_int len
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
	i *= 35;
	memcpy(instance->shmem + oncore_shmem_Cb + i + 2, buf, len + 3);
}

/*
 * Set to Factory Defaults (Reasonable for UT w/ no Battery Backup
 *	not so for VP (eeprom) or UT with battery
 */
static void
oncore_msg_Cf(
	struct instance *instance,
	u_char *buf,
	u_int len
	)
{
	const char *cp;

	if (instance->o_state == ONCORE_RESET_SENT) {
		oncore_sendmsg(instance->ttyfd, oncore_cmd_Fa, sizeof oncore_cmd_Fa);
		instance->o_state = ONCORE_TEST_SENT;
		cp = "state = ONCORE_TEST_SENT";
		record_clock_stats(&(instance->peer->srcadr), cp);
	}
}



/* there are good reasons NOT to do a @@Fa command with the ONCORE.
 * Doing it, it was found that under some circumstances the following
 * command would fail if issued immediately after the return from the
 * @@Fa, but a 2sec delay seemed to fix things.  Since simply calling
 * sleep(2) is wastefull, and may cause trouble for some OS's, repeating
 * itimer, we set a flag, and test it at the next POLL.  If it hasnt
 * been cleared, we reissue the @@Ca that is issued below.
 */

static void
oncore_msg_Fa(
	struct instance *instance,
	u_char *buf,
	u_int len
	)
{
	const char *cp;

	if (instance->o_state == ONCORE_TEST_SENT) {
		if (debug > 2)
			printf("ONCORE: >>@@Fa %x %x\n", buf[4], buf[5]);
		if (buf[4] || buf[5]) {
			printf("ONCORE: SELF TEST FAILED\n");
			exit(1);
		}

		oncore_sendmsg(instance->ttyfd, oncore_cmd_Cj, sizeof oncore_cmd_Cj);
		instance->o_state = ONCORE_ID_SENT;
		cp = "state = ONCORE_ID_SENT";
		record_clock_stats(&(instance->peer->srcadr), cp);
	}
}



/*
 * preliminaries out of the way, this is the REAL start of initialization
 */
static void
oncore_msg_Cj(
	struct instance *instance,
	u_char *buf,
	u_int len
	)
{
	char *cp, *cp1;
	int	mode;

	instance->timeout = 0;
	if (instance->o_state != ONCORE_ID_SENT)
		return;

	memcpy(instance->Cj, buf, len);

	/* Write Receiver ID to clockstats file */

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
#ifdef HAVE_PPSAPI
	if (instance->assert)
		cp = "Timing on Assert.";
	else
		cp = "Timing on Clear.";
	record_clock_stats(&(instance->peer->srcadr), cp);
#endif

	oncore_sendmsg(instance->ttyfd, oncore_cmd_Cg, sizeof oncore_cmd_Cg); /* Set Posn Fix mode (not Idle (VP)) */
	oncore_sendmsg(instance->ttyfd, oncore_cmd_Bb, sizeof oncore_cmd_Bb); /* turn off */
	oncore_sendmsg(instance->ttyfd, oncore_cmd_Ek, sizeof oncore_cmd_Ek); /* turn off */
	oncore_sendmsg(instance->ttyfd, oncore_cmd_Aw, sizeof oncore_cmd_Aw); /* UTC time */
	oncore_sendmsg(instance->ttyfd, oncore_cmd_AB, sizeof oncore_cmd_AB); /* Appl type static */
	oncore_sendmsg(instance->ttyfd, oncore_cmd_Be, sizeof oncore_cmd_Be); /* Tell us the Almanac */

	mode = instance->init_type;
	if (debug)
		printf("ONCORE: INIT mode = %d\n", mode);

	/* If there is Position input in the Config file
	 * and mode = (1,3) set it as posn hold posn, goto 0D mode.
	 *  or mode = (2,4) set it as INITIAL position, and Site Survey.
	 */

	switch (mode) {
	case 0: /* NO initialization, don't change anything */
		instance->site_survey = ONCORE_SS_DONE;
		break;

	case 1:
	case 3:
		w32_buf(&oncore_cmd_As[2],  (int) instance->ss_lat);
		w32_buf(&oncore_cmd_As[6],  (int) instance->ss_long);
		w32_buf(&oncore_cmd_As[10], (int) instance->ss_ht);
		oncore_cmd_As[14] = instance->ss_ht_type;
		oncore_sendmsg(instance->ttyfd, oncore_cmd_As,	sizeof oncore_cmd_As);

		instance->site_survey = ONCORE_SS_DONE;
		oncore_cmd_At[2] = 1;
		oncore_sendmsg(instance->ttyfd, oncore_cmd_At,	sizeof oncore_cmd_At);
		record_clock_stats(&(instance->peer->srcadr), "Now in 0D mode");
		break;

	case 2:
	case 4:
		if (instance->posn_set) {
			w32_buf(&oncore_cmd_Ad[2], (int) instance->ss_lat);
			w32_buf(&oncore_cmd_Ae[2], (int) instance->ss_long);
			w32_buf(&oncore_cmd_Af[2], (int) instance->ss_ht);
			oncore_cmd_Af[6] = instance->ss_ht_type;
			oncore_sendmsg(instance->ttyfd, oncore_cmd_Ad,	sizeof oncore_cmd_Ad);
			oncore_sendmsg(instance->ttyfd, oncore_cmd_Ae,	sizeof oncore_cmd_Ae);
			oncore_sendmsg(instance->ttyfd, oncore_cmd_Af,	sizeof oncore_cmd_Af);
		}
		instance->site_survey = ONCORE_SS_UNKNOWN;
		oncore_cmd_At[2] = 2;
		oncore_sendmsg(instance->ttyfd, oncore_cmd_At,	sizeof oncore_cmd_At);
		break;
	}

	if (mode != 0) {
			/* cable delay in ns */
		w32_buf(&oncore_cmd_Az[2], instance->delay);
		oncore_sendmsg(instance->ttyfd, oncore_cmd_Az,	sizeof oncore_cmd_Az);

			/* PPS offset in ns */
		w32_buf(&oncore_cmd_Ay[2], instance->offset);
		oncore_sendmsg(instance->ttyfd, oncore_cmd_Ay,	sizeof oncore_cmd_Ay);
	}

	/* 8chan - Position/Status/Data Output Message, 1/s */

	oncore_sendmsg(instance->ttyfd, oncore_cmd_Ea,	sizeof oncore_cmd_Ea);

	instance->o_state = ONCORE_ALMANAC;
	cp = "state = ONCORE_ALMANAC";
	record_clock_stats(&(instance->peer->srcadr), cp);
}



static void
oncore_msg_Ea(
	struct instance *instance,
	u_char *buf,
	u_int len
	)
{
	const char	*cp;
	char		Msg[160];

	if (instance->o_state != ONCORE_ALMANAC && instance->o_state != ONCORE_RUN)
		return;

	memcpy(instance->Ea, buf, len);

	/* When we have an almanac, start the En messages */

	if (instance->o_state == ONCORE_ALMANAC) {
		if ((instance->Ea[72] & 1)) {
			if (debug)
				printf("ONCORE: waiting for almanac\n");
			return;
		} else {
			oncore_sendmsg(instance->ttyfd, oncore_cmd_En, sizeof oncore_cmd_En);
			instance->o_state = ONCORE_RUN;
			cp = "state = ONCORE_RUN";
			record_clock_stats(&(instance->peer->srcadr), cp);
		}
	}

	/* must be ONCORE_RUN if we are here */
	/* First check if Hardware SiteSurvey has Finished */

	if ((instance->site_survey == ONCORE_SS_HW) && !(instance->Ea[37] & 0x20)) {
		instance->site_survey = ONCORE_SS_DONE;
		record_clock_stats(&(instance->peer->srcadr), "Now in 0D mode");
	}

	if (!instance->printed && instance->site_survey == ONCORE_SS_DONE) {	/* will print to clockstat when all */
		instance->printed = 1;						/* three messages respond */
			/* Read back Position Hold Params */
		oncore_sendmsg(instance->ttyfd, oncore_cmd_Asx,  sizeof oncore_cmd_Asx);
			/* Read back PPS Offset for Output */
			/* Nb. This will fail silently for early UT (no plus) model */
		oncore_sendmsg(instance->ttyfd, oncore_cmd_Ayx,  sizeof oncore_cmd_Ayx);
			/* Read back Cable Delay for Output */
		oncore_sendmsg(instance->ttyfd, oncore_cmd_Azx,  sizeof oncore_cmd_Azx);
	}

	/* Check the leap second status once per day */

	/*
	 * The following additional check, checking for June/December, is a
	 * workaround for incorrect ONCORE firmware.  The oncore starts
	 * reporting the leap second when the GPS satellite data message
	 * (page 18, subframe 4) is updated to a date in the future, which
	 * which can be several months before the leap second.	WWV and other
	 * services seem to wait until the month of the event to turn
	 * on their indicators (which are usually a single bit).
	 */

	if ((buf[4] == 6) || (buf[4] == 12)) {
		if (instance->Bj_day != buf[5]) {     /* do this 1/day */
			instance->Bj_day = buf[5];
			oncore_sendmsg(instance->ttyfd, oncore_cmd_Bj, sizeof oncore_cmd_Bj);
		}
	}
	instance->pp->year = buf[6]*256+buf[7];
	instance->pp->day = ymd2yd(buf[6]*256+buf[7], buf[4], buf[5]);
	instance->pp->hour = buf[8];
	instance->pp->minute = buf[9];
	instance->pp->second = buf[10];

	if (instance->site_survey != ONCORE_SS_SW)
		return;

	/*
	 * We have to average our own position for the Position Hold Mode
	 */

	/* We only take PDOP/3D fixes */

	if (instance->Ea[37] & 1)
		return;

	/* Not if poor geometry or less than 3 sats */

	if (instance->Ea[72] & 0x52)
		return;

	/* Only 3D fix */

	if (!(instance->Ea[72] & 0x20))
		return;

	instance->ss_lat  += buf_w32(&instance->Ea[15]);
	instance->ss_long += buf_w32(&instance->Ea[19]);
	instance->ss_ht   += buf_w32(&instance->Ea[23]);  /* GPS ellipse */
	instance->ss_count++;

	if (instance->ss_count != POS_HOLD_AVERAGE)
		return;

	instance->ss_lat  /= POS_HOLD_AVERAGE;
	instance->ss_long /= POS_HOLD_AVERAGE;
	instance->ss_ht   /= POS_HOLD_AVERAGE;

	sprintf(Msg, "Surveyed posn:  lat %.3f long %.3f ht %.3f",
			instance->ss_lat, instance->ss_long, instance->ss_ht);
	record_clock_stats(&(instance->peer->srcadr), Msg);

	w32_buf(&oncore_cmd_As[2],  (int) instance->ss_lat);
	w32_buf(&oncore_cmd_As[6],  (int) instance->ss_long);
	w32_buf(&oncore_cmd_As[10], (int) instance->ss_ht);
	oncore_cmd_As[14] = 0;
	oncore_sendmsg(instance->ttyfd, oncore_cmd_As, sizeof oncore_cmd_As);

	oncore_cmd_At[2] = 1;
	oncore_sendmsg(instance->ttyfd, oncore_cmd_At, sizeof oncore_cmd_At);
	record_clock_stats(&(instance->peer->srcadr), "Now in 0D mode");
	instance->site_survey = ONCORE_SS_DONE;
}



static void
oncore_msg_En(
	struct instance *instance,
	u_char *buf,
	u_int len
	)
{
	int	j;
	l_fp ts, ts_tmp;
	double dmy;
#ifdef HAVE_TIMESPEC
	struct timespec *tsp = 0;
#else
	struct timeval	*tsp = 0;
#endif
#ifdef HAVE_PPSAPI
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

	if (instance->o_state != ONCORE_RUN)
		return;

	memcpy(instance->En, buf, len);

	/* Don't do anything without an almanac to define the GPS->UTC delta */

	if (instance->Ea[72] & 1)
		return;

	/* If Time RAIM doesn't like it, don't trust it */

	if (instance->En[21])
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

		if (debug > 2)
			printf("ONCORE: serial/j (%d, %d) %ld.%09ld\n",
			    pps_i.assert_sequence, j, tsp->tv_sec, tsp->tv_nsec);

		if (pps_i.assert_sequence == j) {
			printf("ONCORE: oncore_msg_En, error serial pps\n");
			return;
		}
		instance->ev_serial = pps_i.assert_sequence;
	} else {
		tsp = &pps_i.clear_timestamp;

		if (debug > 2)
			printf("ONCORE: serial/j (%d, %d) %ld.%09ld\n",
			    pps_i.clear_sequence, j, tsp->tv_sec, tsp->tv_nsec);

		if (pps_i.clear_sequence == j) {
			printf("ONCORE: oncore_msg_En, error serial pps\n");
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
		printf("ONCORE: serial/j (%d, %d) %ld.%06ld\n",
			ev.serial, j, tsp->tv_sec, tsp->tv_usec);

	if (ev.serial == j) {
		printf("ONCORE: oncore_msg_En, error serial pps\n");
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
	/* add in saw_tooth and offset */

	/* saw_tooth not really necessary if using TIMEVAL */
	/* since its only precise to us, but do it anyway. */

	/* offset in ns, and is positive (late), we subtract */
	/* to put the PPS time transition back where it belongs */

	j  = instance->saw_tooth + instance->offset;
	instance->saw_tooth = (s_char) buf[25]; /* update for next time */
#ifdef HAVE_PPSAPI
	/* must hand this offset off to the Kernel to do the addition */
	/* so that the Kernel PLL sees the offset too */

	if (instance->assert) {
		instance->pps_p.assert_offset.tv_nsec =
			 -(instance->saw_tooth + instance->offset);
	} else {
		instance->pps_p.clear_offset.tv_nsec =
			 -(instance->saw_tooth + instance->offset);
	}

	if (time_pps_setparams(instance->pps_h, &instance->pps_p))
		perror("time_pps_setparams");
#else
	/* if not PPSAPI, no way to inform kernel of OFFSET, just do it */

	dmy = -1.0e-9*j;
	DTOLFP(dmy, &ts_tmp);
	L_ADD(&ts, &ts_tmp);
#endif
	/* have time from UNIX origin, convert to NTP origin. */

	ts.l_ui += JAN_1970;
	instance->pp->lastrec = ts;
	instance->pp->msec = 0;

	ts_tmp = ts;
	ts_tmp.l_ui = 0;        /* zero integer part */
	LFPTOD(&ts_tmp, dmy);   /* convert fractional part to a double */
	j = 1.0e9*dmy;          /* then to integer ns */
	sprintf(instance->pp->a_lastcode,
	    "%u.%09u %d %d %2d %2d %2d %2ld rstat %02x dop %d nsat %2d,%d raim %d sigma %d neg-sawtooth %3d sat %d%d%d%d%d%d%d%d",
	    ts.l_ui, j,
	    instance->pp->year, instance->pp->day,
	    instance->pp->hour, instance->pp->minute, instance->pp->second,
	    (long) tsp->tv_sec % 60,

	    instance->Ea[72], instance->Ea[37], instance->Ea[38], instance->Ea[39], instance->En[21],
	    /*rstat           dop               nsat visible,     nsat tracked,     raim */
	    instance->En[23]*256+instance->En[24], (s_char) buf[25],
	    /* sigma				   neg-sawtooth */
  /*sat*/   instance->Ea[41], instance->Ea[45], instance->Ea[49], instance->Ea[53],
	    instance->Ea[57], instance->Ea[61], instance->Ea[65], instance->Ea[69]
	    );

	if (debug > 2) {
		int i;
		i = strlen(instance->pp->a_lastcode);
		printf("ONCORE: len = %d %s\n", i, instance->pp->a_lastcode);
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
	u_int len
	)
{
	if (instance->site_survey != ONCORE_SS_UNKNOWN)
		return;

	if (buf[4] == 2) {
		record_clock_stats(&(instance->peer->srcadr),
				"Initiating hardware 3D site survey");
		instance->site_survey = ONCORE_SS_HW;
	} else {
		char Msg[160];
		/*
		 * Probably a VP or an older UT which can't do site-survey.
		 * We will have to do it ourselves
		 */

		sprintf(Msg, "Initiating software 3D site survey (%d samples)",
				POS_HOLD_AVERAGE);
		record_clock_stats(&(instance->peer->srcadr), Msg);
		instance->site_survey = ONCORE_SS_SW;

		oncore_cmd_At[2] = 0;
		instance->ss_lat = instance->ss_long = instance->ss_ht = 0;
		oncore_sendmsg(instance->ttyfd, oncore_cmd_At, sizeof oncore_cmd_At);
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
	u_int len
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



/*
 * get Position hold position
 */
static void
oncore_msg_As(
	struct instance *instance,
	u_char *buf,
	u_int len
	)
{
	char Msg[120], ew, ns;
	const char *Ht;
	double xd, xm, xs, yd, ym, ys, hm, hft;
	int idx, idy, is, imx, imy;
	long lat, lon, ht;

	if (!instance->printed || instance->As)
		return;

	instance->As = 1;

	lat = buf_w32(&buf[4]);
	instance->ss_lat = lat;

	lon = buf_w32(&buf[8]);
	instance->ss_long = lon;

	ht = buf_w32(&buf[12]);
	instance->ss_ht = ht;

	instance->ss_ht_type = buf[16];

	/* Print out Position */

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
	Ht = instance->ss_ht_type ? "MSL" : "GPS";

	xd = lat/3600000.;	/* lat, lon in int msec arc, ht in cm. */
	yd = lon/3600000.;
	sprintf(Msg, "Lat = %c %11.7fdeg,    Long = %c %11.7fdeg,    Alt = %5.2fm (%5.2fft) %s", ns, xd, ew, yd, hm, hft, Ht);
	record_clock_stats(&(instance->peer->srcadr), Msg);

	idx = xd;
	idy = yd;
	imx = lat%3600000;
	imy = lon%3600000;
	xm = imx/60000.;
	ym = imy/60000.;
	sprintf(Msg, "Lat = %c %3ddeg %7.4fm,   Long = %c %3ddeg %8.5fm,  Alt = %5.2fm (%5.2fft) %s", ns, idx, xm, ew, idy, ym, hm, hft, Ht);
	record_clock_stats(&(instance->peer->srcadr), Msg);

	imx = xm;
	imy = ym;
	is  = lat%60000;
	xs  = is/1000.;
	is  = lon%60000;
	ys  = is/1000.;
	sprintf(Msg, "Lat = %c %3ddeg %2dm %5.2fs, Long = %c %3ddeg %2dm %5.2fs, Alt = %5.2fm (%5.2fft) %s", ns, idx, imx, xs, ew, idy, imy, ys, hm, hft, Ht);
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
	u_int len
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
	u_int len
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
#else
int refclock_oncore_bs;
#endif /* REFCLOCK */
