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
 *   should work with Basic, PVT6, VP, UT, UT+, GT, GT+, SL, M12, M12+T
 *	The receivers with TRAIM (VP, UT, UT+, M12+T), will be more accurate
 *	than the others.
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
 *   COPYRIGHT 1991-1994 MOTOROLA INC.	COPYRIGHT 1991-2000 MOTOROLA INC.
 *   SFTW P/N # 98-P39949M		SFTW P/N # 61-G10002A
 *   SOFTWARE VER # 5			SOFTWARE VER # 1
 *   SOFTWARE REV # 0			SOFTWARE REV # 3
 *   SOFTWARE DATE  20 JAN 1994 	SOFTWARE DATE  Mar 13 2000
 *   MODEL #	A11121P116		MODEL #    P143T12NR1
 *   HDWR P/N # _			HWDR P/N # 1
 *   SERIAL #	SSG0049809		SERIAL #   P003UD
 *   MANUFACTUR DATE 417AMA199		MANUFACTUR DATE 0C27
 *   OPTIONS LIST    AB
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
	ONCORE_CHECK_ID,
	ONCORE_CHECK_CHAN,
	ONCORE_HAVE_CHAN,
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

enum antenna_state {
      ONCORE_ANTENNA_UNKNOWN = -1,
      ONCORE_ANTENNA_OK      =	0,
      ONCORE_ANTENNA_OC      =	1,
      ONCORE_ANTENNA_UC      =	2,
      ONCORE_ANTENNA_NV      =	3
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
	int	shmemfd;	/* Status shm descriptor */
#ifdef HAVE_PPSAPI
	pps_handle_t pps_h;
	pps_params_t pps_p;
#endif
	enum receive_state o_state;		/* Receive state */
	enum posn_mode mode;			/* 0D, 2D, 3D */
	enum site_survey_state site_survey;	/* Site Survey state */
	enum antenna_state ant_state;		/* antenna state */

	int	Bj_day;

	u_long	delay;		/* ns */
	long	offset; 	/* ns */

	u_char	*shmem;
	char	*shmem_fname;
	u_int	shmem_Cb;
	u_int	shmem_Ba;
	u_int	shmem_Ea;
	u_int	shmem_Ha;
	u_char	shmem_reset;
	u_char	shmem_Posn;
	u_char	shmem_bad_Ea;
	u_char	almanac_from_shmem;

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
	s_char	traim;		/* do we have traim? yes UT/VP, no BASIC, GT, M12+T, -1 unknown, 0 no, +1 yes */

				/* the following 7 are all timing counters */
	u_char	traim_delay;	/* seconds counter, waiting for reply */
	u_char	count;		/* cycles thru Ea before starting */
	u_char	count1; 	/* cycles thru Ea after SS_TESTING, waiting for SS_HW */
	u_char	count2; 	/* cycles thru Ea after count, to check for @@Ea */
	u_char	count3; 	/* cycles thru Ea checking for # channels */
	u_char	count4; 	/* cycles thru leap after Gj to issue Bj */
	u_char	pollcnt;
	u_char	timeout;	/* count to retry Cj after Fa self-test */

	struct	RSM rsm;	/* bits extracted from Receiver Status Msg in @@Ea */
	u_char	printed;
	u_char	polled;
	u_long	ev_serial;
	int	Rcvptr;
	u_char	Rcvbuf[500];
	u_char	BEHa[160];	/* Ba, Ea or Ha */
	u_char	BEHn[80];	/* Bn , En , or Hn */
	u_char	Cj[300];
	u_char	Ag;		/* Satellite mask angle */
	u_char	saw_At;
	u_char	saw_Ay;
	u_char	saw_Az;
	s_char	saw_Gj;
	u_char	have_dH;
	u_char	init_type;
	s_char	saw_tooth;
	s_char	chan_in;	/* chan number from INPUT, will always use it */
	u_char	chan_id;	/* chan number determined from part number */
	u_char	chan_ck;	/* chan number determined by sending commands to hardware */
	s_char	traim_in;	/* TRAIM from INPUT, will always use it */
	s_char	traim_id;	/* TRAIM determined from part number */
	u_char	traim_ck;	/* TRAIM determined by sending commands to hardware */
	u_char	once;		/* one pass code at top of BaEaHa */
	s_char	assert;
	u_char	hardpps;
};

#define rcvbuf	instance->Rcvbuf
#define rcvptr	instance->Rcvptr

static	int	oncore_start	      P((int, struct peer *));
static	void	oncore_control	      P((int, struct refclockstat *, struct refclockstat *, struct peer *));
static	void	oncore_poll	      P((int, struct peer *));
static	void	oncore_shutdown       P((int, struct peer *));
static	void	oncore_consume	      P((struct instance *));
static	void	oncore_read_config    P((struct instance *));
static	void	oncore_receive	      P((struct recvbuf *));
static	int	oncore_ppsapi	      P((struct instance *));
static	void	oncore_get_timestamp  P((struct instance *, long, long));
static	void	oncore_init_shmem     P((struct instance *));

static	void	oncore_antenna_report P((struct instance *, enum antenna_state));
static	void	oncore_chan_test      P((struct instance *));
static	void	oncore_check_almanac  P((struct instance *));
static	void	oncore_check_antenna  P((struct instance *));
static	void	oncore_check_leap_sec P((struct instance *));
static	int	oncore_checksum_ok    P((u_char *, int));
static	void	oncore_compute_dH     P((struct instance *));
static	void	oncore_load_almanac   P((struct instance *));
static	void	oncore_print_Cb       P((struct instance *, u_char *));
/* static  void    oncore_print_array	 P((u_char *, int));	*/
static	void	oncore_print_posn     P((struct instance *));
static	void	oncore_sendmsg	      P((int, u_char *, size_t));
static	void	oncore_set_posn       P((struct instance *));
static	void	oncore_set_traim      P((struct instance *));
static	void	oncore_shmem_get_3D   P((struct instance *));
static	void	oncore_ss	      P((struct instance *));
static	int	oncore_wait_almanac   P((struct instance *));

static	void	oncore_msg_any	   P((struct instance *, u_char *, size_t, int));
static	void	oncore_msg_Adef    P((struct instance *, u_char *, size_t));
static	void	oncore_msg_Ag	   P((struct instance *, u_char *, size_t));
static	void	oncore_msg_As	   P((struct instance *, u_char *, size_t));
static	void	oncore_msg_At	   P((struct instance *, u_char *, size_t));
static	void	oncore_msg_Ay	   P((struct instance *, u_char *, size_t));
static	void	oncore_msg_Az	   P((struct instance *, u_char *, size_t));
static	void	oncore_msg_BaEaHa  P((struct instance *, u_char *, size_t));
static	void	oncore_msg_Bd	   P((struct instance *, u_char *, size_t));
static	void	oncore_msg_Bj	   P((struct instance *, u_char *, size_t));
static	void	oncore_msg_BnEnHn  P((struct instance *, u_char *, size_t));
static	void	oncore_msg_CaFaIa  P((struct instance *, u_char *, size_t));
static	void	oncore_msg_Cb	   P((struct instance *, u_char *, size_t));
static	void	oncore_msg_Cf	   P((struct instance *, u_char *, size_t));
static	void	oncore_msg_Cj	   P((struct instance *, u_char *, size_t));
static	void	oncore_msg_Cj_id   P((struct instance *, u_char *, size_t));
static	void	oncore_msg_Cj_init P((struct instance *, u_char *, size_t));
static	void	oncore_msg_Ga	   P((struct instance *, u_char *, size_t));
static	void	oncore_msg_Gb	   P((struct instance *, u_char *, size_t));
static	void	oncore_msg_Gd	   P((struct instance *, u_char *, size_t));
static	void	oncore_msg_Gj	   P((struct instance *, u_char *, size_t));
static	void	oncore_msg_Sz	   P((struct instance *, u_char *, size_t));

struct	refclock refclock_oncore = {
	oncore_start,		/* start up driver */
	oncore_shutdown,	/* shut down driver */
	oncore_poll,		/* transmit poll message */
	oncore_control, 	/* fudge (flag) control messages */
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
	{ "Bn",  59,    oncore_msg_BnEnHn, "otaapxxxxxxxxxxpysreensffffsffffsffffsffffsffffsffffC" },
	{ "En",  69,    oncore_msg_BnEnHn, "otaapxxxxxxxxxxpysreensffffsffffsffffsffffsffffsffffsffffsffffC" },
	{ "Hn",  78,    oncore_msg_BnEnHn, "" },
	{ "Ab",  10,    0,                 "" },
	{ "Ac",  11,    0,                 "" },
	{ "Ad",  11,    oncore_msg_Adef,   "" },
	{ "Ae",  11,    oncore_msg_Adef,   "" },
	{ "Af",  15,    oncore_msg_Adef,   "" },
	{ "Ag",   8,    oncore_msg_Ag,     "" }, /* Satellite mask angle */
	{ "As",  20,    oncore_msg_As,     "" },
	{ "At",   8,    oncore_msg_At,     "" },
	{ "Au",  12,    0,                 "" },
	{ "Av",   8,    0,                 "" },
	{ "Aw",   8,    0,                 "" },
	{ "Ay",  11,    oncore_msg_Ay,     "" },
	{ "Az",  11,    oncore_msg_Az,     "" },
	{ "AB",   8,    0,                 "" },
	{ "Bb",  92,    0,                 "" },
	{ "Bd",  23,    oncore_msg_Bd,     "" },
	{ "Bj",   8,    oncore_msg_Bj,     "" },
	{ "Ca",   9,    oncore_msg_CaFaIa, "" },
	{ "Cb",  33,    oncore_msg_Cb,     "" },
	{ "Cf",   7,    oncore_msg_Cf,     "" },
	{ "Cg",   8,    0,                 "" },
	{ "Ch",   9,    0,                 "" },
	{ "Cj", 294,    oncore_msg_Cj,     "" },
	{ "Ek",  71,    0,                 "" },
	{ "Fa",   9,    oncore_msg_CaFaIa, "" },
	{ "Ga",  20,    oncore_msg_Ga,     "" },
	{ "Gb",  17,    oncore_msg_Gb,     "" },
	{ "Gc",   8,    0,                 "" },
	{ "Gd",   8,    oncore_msg_Gd,     "" },
	{ "Ge",   8,    0,                 "" },
	{ "Gj",  21,    oncore_msg_Gj,     "" },
	{ "Ia",  10,    oncore_msg_CaFaIa, "" },
	{ "Sz",   8,    oncore_msg_Sz,     "" },
	{ {0},	  7,	0,		   "" }
};


static u_char oncore_cmd_Aa[]  = { 'A', 'a', 0, 0, 0 }; 			    /* 6/8	Time of Day				*/
static u_char oncore_cmd_Ab[]  = { 'A', 'b', 0, 0, 0 }; 			    /* 6/8	GMT Correction				*/
static u_char oncore_cmd_AB[]  = { 'A', 'B', 4 };				    /* VP	Application Type: Static		*/
static u_char oncore_cmd_Ac[]  = { 'A', 'c', 0, 0, 0, 0 };			    /* 6/8	Date					*/
static u_char oncore_cmd_Ad[]  = { 'A', 'd', 0,0,0,0 }; 			    /* 6/8	Latitude				*/
static u_char oncore_cmd_Ae[]  = { 'A', 'e', 0,0,0,0 }; 			    /* 6/8	Longitude				*/
static u_char oncore_cmd_Af[]  = { 'A', 'f', 0,0,0,0, 0 };			    /* 6/8	Height					*/
static u_char oncore_cmd_Ag[]  = { 'A', 'g', 0 };				    /* 6/8/12	Satellite Mask Angle			*/
static u_char oncore_cmd_Agx[] = { 'A', 'g', 0xff };				    /* 6/8/12	Satellite Mask Angle: read		*/
static u_char oncore_cmd_As[]  = { 'A', 's', 0,0,0,0, 0,0,0,0, 0,0,0,0, 0 };	    /* 6/8/12	Posn Hold Parameters			*/
static u_char oncore_cmd_Asx[] = { 'A', 's', 0x7f,0xff,0xff,0xff,		    /* 6/8/12	Posn Hold Readback			*/
					     0x7f,0xff,0xff,0xff,		    /*		 on UT+ this doesnt work with 0xff	*/
					     0x7f,0xff,0xff,0xff, 0xff };	    /*		 but does work with 0x7f (sigh).	*/
static u_char oncore_cmd_At0[] = { 'A', 't', 0 };				    /* 6/8	Posn Hold: off				*/
static u_char oncore_cmd_At1[] = { 'A', 't', 1 };				    /* 6/8	Posn Hold: on				*/
static u_char oncore_cmd_At2[] = { 'A', 't', 2 };				    /* 6/8	Posn Hold: Start Site Survey		*/
static u_char oncore_cmd_Atx[] = { 'A', 't', 0xff };				    /* 6/8	Posn Hold: Read Back			*/
static u_char oncore_cmd_Au[]  = { 'A', 'u', 0,0,0,0, 0 };			    /* GT/M12	Altitude Hold Ht.			*/
static u_char oncore_cmd_Av0[] = { 'A', 'v', 0 };				    /* VP/GT	Altitude Hold: off			*/
static u_char oncore_cmd_Av1[] = { 'A', 'v', 1 };				    /* VP/GT	Altitude Hold: on			*/
static u_char oncore_cmd_Aw[]  = { 'A', 'w', 1 };				    /* 6/8/12	UTC/GPS time selection			*/
static u_char oncore_cmd_Ay[]  = { 'A', 'y', 0, 0, 0, 0 };			    /* Timing	1PPS time offset: set			*/
static u_char oncore_cmd_Ayx[] = { 'A', 'y', 0xff, 0xff, 0xff, 0xff };		    /* Timing	1PPS time offset: Read			*/
static u_char oncore_cmd_Az[]  = { 'A', 'z', 0, 0, 0, 0 };			    /* 6/8UT/12 1PPS Cable Delay: set			*/
static u_char oncore_cmd_Azx[] = { 'A', 'z', 0xff, 0xff, 0xff, 0xff };		    /* 6/8UT/12 1PPS Cable Delay: Read			*/
static u_char oncore_cmd_Ba0[] = { 'B', 'a', 0 };				    /* 6	Position/Data/Status: off		*/
static u_char oncore_cmd_Ba[]  = { 'B', 'a', 1 };				    /* 6	Position/Data/Status: on		*/
static u_char oncore_cmd_Bb[]  = { 'B', 'b', 1 };				    /* 6/8/12	Visible Satellites			*/
static u_char oncore_cmd_Bd[]  = { 'B', 'd', 1 };				    /* 6/8/12?	Almanac Status Msg.			*/
static u_char oncore_cmd_Be[]  = { 'B', 'e', 1 };				    /* 6/8/12	Request Almanac Data			*/
static u_char oncore_cmd_Bj[]  = { 'B', 'j', 0 };				    /* 6/8	Leap Second Pending			*/
static u_char oncore_cmd_Bn0[] = { 'B', 'n', 0, 1, 0,10, 2, 0,0,0, 0,0,0,0,0,0,0 }; /* 6	TRAIM setup/status: msg off, traim on	*/
static u_char oncore_cmd_Bn[]  = { 'B', 'n', 1, 1, 0,10, 2, 0,0,0, 0,0,0,0,0,0,0 }; /* 6	TRAIM setup/status: msg on traim on	*/
static u_char oncore_cmd_Bnx[] = { 'B', 'n', 1, 0, 0,10, 2, 0,0,0, 0,0,0,0,0,0,0 }; /* 6	TRAIM setup/status: msg on traim off	*/
static u_char oncore_cmd_Ca[]  = { 'C', 'a' };					    /* 6	Self Test				*/
static u_char oncore_cmd_Cf[]  = { 'C', 'f' };					    /* 6/8/12	Set to Defaults 			*/
static u_char oncore_cmd_Cg[]  = { 'C', 'g', 1 };				    /* VP	Posn Fix/Idle Mode			*/
static u_char oncore_cmd_Cj[]  = { 'C', 'j' };					    /* 6/8/12	Receiver ID				*/
static u_char oncore_cmd_Ea0[] = { 'E', 'a', 0 };				    /* 8	Position/Data/Status: off		*/
static u_char oncore_cmd_Ea[]  = { 'E', 'a', 1 };				    /* 8	Position/Data/Status: on		*/
static u_char oncore_cmd_Ek[]  = { 'E', 'k', 0 }; /* just turn off */		    /* 8	Posn/Status/Data - extension		*/
static u_char oncore_cmd_En0[] = { 'E', 'n', 0, 1, 0,10, 2, 0,0,0, 0,0,0,0,0,0,0 }; /* 8/GT	TRAIM setup/status: msg off, traim on	*/
static u_char oncore_cmd_En[]  = { 'E', 'n', 1, 1, 0,10, 2, 0,0,0, 0,0,0,0,0,0,0 }; /* 8/GT	TRAIM setup/status: msg on traim on	*/
static u_char oncore_cmd_Enx[] = { 'E', 'n', 1, 0, 0,10, 2, 0,0,0, 0,0,0,0,0,0,0 }; /* 8/GT	TRAIM setup/status: msg on traim off	*/
static u_char oncore_cmd_Fa[]  = { 'F', 'a' };					    /* 8	Self Test				*/
static u_char oncore_cmd_Ga[]  = { 'G', 'a', 0,0,0,0, 0,0,0,0, 0,0,0,0, 0 };	    /* 12	Position Set				*/
static u_char oncore_cmd_Gax[] = { 'G', 'a', 0xff, 0xff, 0xff, 0xff,		    /* 12	Position Set: Read			*/
					     0xff, 0xff, 0xff, 0xff,		    /*							*/
					     0xff, 0xff, 0xff, 0xff, 0xff };	    /*							*/
static u_char oncore_cmd_Gb[]  = { 'G', 'b', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };	    /* 12	set Date/Time				*/
static u_char oncore_cmd_Gc[]  = { 'G', 'c', 1 };				    /* 12	PPS Control: On Cont			*/
static u_char oncore_cmd_Gd0[] = { 'G', 'd', 0 };				    /* 12	Position Control: 3D (no hold)		*/
static u_char oncore_cmd_Gd1[] = { 'G', 'd', 1 };				    /* 12	Position Control: 0D (3D hold)		*/
static u_char oncore_cmd_Gd2[] = { 'G', 'd', 2 };				    /* 12	Position Control: 2D (Alt Hold) 	*/
static u_char oncore_cmd_Gd3[] = { 'G', 'd', 3 };				    /* 12	Position Coltrol: Start Site Survey	*/
static u_char oncore_cmd_Ge0[] = { 'G', 'e', 0 };				    /* M12+T	TRAIM: off				*/
static u_char oncore_cmd_Ge[]  = { 'G', 'e', 1 };				    /* M12+T	TRAIM: on				*/
static u_char oncore_cmd_Gj[]  = { 'G', 'j' };					    /* 8?/12	Leap Second Pending			*/
static u_char oncore_cmd_Ha0[] = { 'H', 'a', 0 };				    /* 12	Position/Data/Status: off		*/
static u_char oncore_cmd_Ha[]  = { 'H', 'a', 1 };				    /* 12	Position/Data/Status: on		*/
static u_char oncore_cmd_Hn0[] = { 'H', 'n', 0 };				    /* 12	TRAIM Status: off			*/
static u_char oncore_cmd_Hn[]  = { 'H', 'n', 1 };				    /* 12	TRAIM Status: on			*/
static u_char oncore_cmd_Ia[]  = { 'I', 'a' };					    /* 12	Self Test				*/

/* it appears that as of 1997/1998, the UT had As,At, but not Au,Av
 *				    the GT had Au,Av, but not As,At
 * This was as of v2.0 of both firmware sets. possibly 1.3 for UT.
 * Bj in UT at v1.3
 * dont see Bd in UT/GT thru 1999
 * Gj in UT as of 3.0, 1999 , Bj as of 1.3
 */

static char *Month[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jly",
	"Aug", "Sep", "Oct", "Nov", "Dec" };

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
	int fd1, fd2;
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

	/* create instance structure for this unit */

	if (!(instance = (struct instance *) malloc(sizeof *instance))) {
		perror("malloc");
		return (0);
	}
	memset((char *) instance, 0, sizeof *instance);

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
	} else {			/* different devices here */
		if (!(fd1=refclock_open(device1, SPEED, LDISC_RAW))) {
			perror("ONCORE: fd1");
			exit(1);
		}
		if ((fd2=open(device2, O_RDWR)) < 0) {
			perror("ONCORE: fd2");
			exit(1);
		}
	}

	/* initialize miscellaneous variables */

	pp = peer->procptr;
	pp->unitptr    = (caddr_t) instance;
	instance->pp   = pp;
	instance->unit = unit;
	instance->peer = peer;
	instance->assert = 1;
	instance->once = 1;

	cp = "ONCORE DRIVER -- CONFIGURING";
	record_clock_stats(&(instance->peer->srcadr), cp);

	instance->o_state = ONCORE_NO_IDEA;
	cp = "state = ONCORE_NO_IDEA";
	record_clock_stats(&(instance->peer->srcadr), cp);

	instance->ttyfd = fd1;
	instance->ppsfd = fd2;

	instance->Bj_day = -1;
	instance->traim = -1;
	instance->traim_in = -1;
	instance->chan_in = -1;
	instance->model = ONCORE_UNKNOWN;
	instance->mode = MODE_UNKNOWN;
	instance->site_survey = ONCORE_SS_UNKNOWN;
	instance->Ag = 0xff;		/* Satellite mask angle, unset by user */
	instance->ant_state = ONCORE_ANTENNA_UNKNOWN;

	peer->precision = -26;
	peer->minpoll = 4;
	peer->maxpoll = 4;
	pp->clockdesc = "Motorola Oncore GPS Receiver";
	memcpy((char *)&pp->refid, "GPS\0", (size_t) 4);

	/* go read any input data in /etc/ntp.oncoreX or /etc/ntp/oncore.X */

	oncore_read_config(instance);

#ifdef HAVE_PPSAPI
	if (time_pps_create(fd2, &instance->pps_h) < 0) {
		perror("time_pps_create");
		return(0);
	}

	if (instance->assert)
		cp = "Initializing timing to Assert.";
	else
		cp = "Initializing timing to Clear.";
	record_clock_stats(&(instance->peer->srcadr), cp);

	if (instance->hardpps) {
		cp = "HARDPPS Set.";
		record_clock_stats(&(instance->peer->srcadr), cp);
	}

	if (!oncore_ppsapi(instance))
		return(0);
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

#ifdef ONCORE_SHMEM_STATUS
	/*
	 * Before starting ONCORE, lets setup SHMEM
	 * This will include merging an old SHMEM into the new one if
	 * an old one is found.
	 */

	oncore_init_shmem(instance);
#endif

	/*
	 * This will return the Model of the Oncore receiver.
	 * and start the Initialization loop in oncore_msg_Cj.
	 */

	instance->o_state = ONCORE_CHECK_ID;
	cp = "state = ONCORE_CHECK_ID";
	record_clock_stats(&(instance->peer->srcadr), cp);

	instance->timeout = 4;
	oncore_sendmsg(instance->ttyfd, oncore_cmd_Cg, sizeof(oncore_cmd_Cg)); /* Set Posn Fix mode (not Idle (VP)) */
	oncore_sendmsg(instance->ttyfd, oncore_cmd_Cj, sizeof(oncore_cmd_Cj));

	instance->pollcnt = 2;
	return (1);
}


/*
 * Fudge control (get Flag2 and Flag3, not available at oncore_start time.
 */

static void
oncore_control(
	int unit,		  /* unit (not used) */
	struct refclockstat *in,  /* input parameters  (not used) */
	struct refclockstat *out, /* output parameters (not used) */
	struct peer *peer	  /* peer structure pointer */
	)
{
	char *cp;
	struct refclockproc *pp;
	struct instance *instance;

	pp = peer->procptr;
	instance = (struct instance *) pp->unitptr;

	instance->assert  = !(pp->sloppyclockflag & CLK_FLAG2);
	instance->hardpps =   pp->sloppyclockflag & CLK_FLAG3;

	if (instance->assert)
		cp = "Resetting timing to Assert.";
	else
		cp = "Resetting timing to Clear.";
	record_clock_stats(&(instance->peer->srcadr), cp);

	if (instance->hardpps) {
		cp = "HARDPPS Set.";
		record_clock_stats(&(instance->peer->srcadr), cp);
	}

	(void) oncore_ppsapi(instance);
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

	close(instance->ttyfd);
	close(instance->ppsfd);
	if (instance->shmemfd)
		close(instance->shmemfd);
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
 * Initialize PPSAPI
 */

#ifdef HAVE_PPSAPI
static int
oncore_ppsapi(
	struct instance *instance
	)
{
	int mode;

	if (time_pps_getcap(instance->pps_h, &mode) < 0) {
		msyslog(LOG_ERR, "refclock_ioctl: time_pps_getcap failed: %m");
		return (0);
	}

	if (time_pps_getparams(instance->pps_h, &instance->pps_p) < 0) {
		msyslog(LOG_ERR, "refclock_ioctl: time_pps_getparams failed: %m");
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

	/* If HARDPPS is on, we tell kernel */

	if (instance->hardpps) {
		int	i;

		if (instance->assert)
			i = PPS_CAPTUREASSERT;
		else
			i = PPS_CAPTURECLEAR;

		if (i&mode) {
			if (time_pps_kcbind(instance->pps_h, PPS_KC_HARDPPS, i,
			    PPS_TSFMT_TSPEC) < 0) {
				msyslog(LOG_ERR, "refclock_ioctl: time_pps_kcbind failed: %m");
				return (0);
			}
			pps_enable = 1;
		}
	}
	return(1);
}
#endif



#ifdef ONCORE_SHMEM_STATUS
static void
oncore_init_shmem(
	struct instance *instance
	)
{
	int i, l, n, fd, shmem_old_size, n1;
	char *buf, Msg[160];
	u_char *cp, *cp1, *shmem_old;
	struct msg_desc *mp;
	struct stat sbuf;
	size_t shmem_length;

       /*
	* The first thing we do is see if there is an instance->shmem_fname file (still)
	* out there from a previous run.  If so, we copy it in and use it to initialize
	* shmem (so we won't lose our almanac if we need it).
	*/

	shmem_old = 0;
	if ((fd = open(instance->shmem_fname, O_RDONLY)) < 0)
		perror("LOAD:SHMEM");
	else {
		fstat(fd, &sbuf);
		shmem_old_size = sbuf.st_size;
		shmem_old = (u_char *) malloc((unsigned) sbuf.st_size);
		if (shmem_old == NULL) {
			perror("malloc");
			close(fd);
			return;
		}

		read(fd, shmem_old, shmem_old_size);
		close(fd);
	}

	/* OK, we now create the NEW SHMEM. */

	if ((instance->shmemfd = open(instance->shmem_fname, O_RDWR|O_CREAT|O_TRUNC, 0644)) < 0) {
		perror(instance->shmem_fname);
		return;
	}

	/* see how big it needs to be */

	n = 1;
	for (mp=oncore_messages; mp->flag[0]; mp++) {
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
	shmem_length = n + 2;
	fprintf(stderr, "ONCORE: SHMEM length: %d bytes\n", (int) shmem_length);

	buf = malloc(shmem_length);
	if (buf == NULL) {
		perror("malloc");
		close(instance->shmemfd);
		return;
	}

	memset(buf, 0, shmem_length);

	/* next build the new SHMEM buffer in memory */

	for (mp=oncore_messages; mp->flag[0]; mp++) {
		l = mp->shmem;
		buf[l + 0] = mp->len >> 8;
		buf[l + 1] = mp->len & 0xff;
		buf[l + 2] = 0;
		buf[l + 3] = '@';
		buf[l + 4] = '@';
		buf[l + 5] = mp->flag[0];
		buf[l + 6] = mp->flag[1];
		if (!strcmp(mp->flag, "Cb") || !strcmp(mp->flag, "Ba") || !strcmp(mp->flag, "Ea") || !strcmp(mp->flag, "Ha")) {
			if (!strcmp(mp->flag, "Cb"))
				n = 35;
			else
				n = 4;
			for (i=1; i<n; i++) {
				buf[l + i * (mp->len+3) + 0] = mp->len >> 8;
				buf[l + i * (mp->len+3) + 1] = mp->len & 0xff;
				buf[l + i * (mp->len+3) + 2] = 0;
				buf[l + i * (mp->len+3) + 3] = '@';
				buf[l + i * (mp->len+3) + 4] = '@';
				buf[l + i * (mp->len+3) + 5] = mp->flag[0];
				buf[l + i * (mp->len+3) + 6] = mp->flag[1];
			}
		}
	}

	/* we now walk thru the two buffers (shmem_old and buf, soon to become shmem)
	 * copying the data in shmem_old to buf.  When we are done we write it out
	 * and free both buffers.
	 * If the structures change (an addition or deletion) I will stop copying.
	 * The two will be the same unless we add/subtract from the oncore_messages list
	 * so this should work most of the time, and takes a lot less code than doing it right.
	 */

	if (shmem_old) {
		for (cp=buf+4, cp1=shmem_old+4; (n = 256*(*(cp-3)) + *(cp-2));	cp+=(n+3), cp1+=(n+3)) {
			n1 = 256*(*(cp1-3)) + *(cp1-2);
			if (n1 != n || strncmp(cp, cp1, 4))
				break;

			memcpy(cp, cp1, (size_t) n);
		}
		free(shmem_old);
	}

	i = write(instance->shmemfd, buf, shmem_length);
	free(buf);

	if (i != shmem_length) {
		perror(instance->shmem_fname);
		close(instance->shmemfd);
		return;
	}

	instance->shmem = (u_char *) mmap(0, shmem_length,
		PROT_READ | PROT_WRITE,
#ifdef MAP_HASSEMAPHORE
		MAP_HASSEMAPHORE |
#endif
		MAP_SHARED, instance->shmemfd, (off_t)0);

	if (instance->shmem == (u_char *)MAP_FAILED) {
		instance->shmem = 0;
		close(instance->shmemfd);
		return;
	}

	sprintf(Msg, "SHMEM (size = %d) is CONFIGURED and available as %s", shmem_length, instance->shmem_fname);
	record_clock_stats(&(instance->peer->srcadr), Msg);
}
#endif /* ONCORE_SHMEM_STATUS */



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
 *   MODE, LAT, LON, (HT, HTGPS, HTMSL), DELAY, OFFSET, ASSERT, CLEAR, HARDPPS,
 *   STATUS, POSN3D, POSN2D, CHAN, TRAIM
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
 *	   If the receiver reports height in both GPS and MSL, then we will report
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
 *	   ASSERT/CLEAR can also be set with FLAG2 of the ntp.conf input.
 *	   For Flag2, ASSERT=0, and hence is default.
 *
 *	There is an optional line, with HARDPPS on it.	Including this line causes
 *	   the PPS signal to control the kernel PLL.
 *	   HARDPPS can also be set with FLAG3 of the ntp.conf input.
 *	   For Flag3, 0 is disabled, and the default.
 *
 *	There are three options that have to do with using the shared memory option.
 *	   First, to enable the option there must be a SHMEM line with a file name.
 *	   The file name is the file associated with the shared memory.
 *
 *	In shared memory, there is one 'record' for each returned variable.
 *	For the @@Ea data there are three 'records' containing position data.
 *	   There will always be data in the record corresponding to the '0D' @@Ea record,
 *	   and the user has a choice of filling the '3D' record by specifying POSN3D,
 *	   or the '2D' record by specifying POSN2D.  In either case the '2D' or '3D'
 *	   record is filled once every 15s.
 *
 *	Two additional variables that can be set are CHAN and TRAIM.  These should be
 *	   set correctly by the code examining the @@Cj record, but we bring them out here
 *	   to allow the user to override either the # of channels, or the existence of TRAIM.
 *	   CHAN expects to be followed by in integer: 6, 8, or 12. TRAIM expects to be
 *	   followed by YES or NO.
 *
 *	There is an optional line with MASK on it followed by one integer field in the
 *	   range 0 to 89. This sets the satellite mask angle and will determine the minimum
 *	   elevation angle for satellites to be tracked by the receiver. The default value
 *	   is 10 deg for the VP and 0 deg for all other receivers.
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
	int	i, sign, lat_flg, long_flg, ht_flg, mode, mask;
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

	mode = mask = 0;
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
		} else if (!strncmp(cc, "HARDPPS", (size_t) 7)) {
			instance->hardpps = 1;
		} else if (!strncmp(cc, "POSN2D", (size_t) 6)) {
			instance->shmem_Posn = 2;
		} else if (!strncmp(cc, "POSN3D", (size_t) 6)) {
			instance->shmem_Posn = 3;
		} else if (!strncmp(cc, "CHAN", (size_t) 4)) {
			sscanf(ca, "%d", &i);
			if ((i == 6) || (i == 8) || (i == 12))
				instance->chan_in = i;
		} else if (!strncmp(cc, "TRAIM", (size_t) 5)) {
			instance->traim_in = 1; 	/* so TRAIM alone is YES */
			if (!strcmp(ca, "NO") || !strcmp(ca, "OFF"))    /* Yes/No, On/Off */
				instance->traim_in = 0;
		} else if (!strncmp(cc, "MASK", (size_t) 4)) {
			sscanf(ca, "%d", &mask);
			if (mask > -1 && mask < 90)
				instance->Ag = mask;			/* Satellite mask angle */
		}
	}
	fclose(fd);

	/*
	 *    OK, have read all of data file, and extracted the good stuff.
	 *    If lat/long/ht specified they ALL must be specified for mode = (1,3).
	 */

	instance->posn_set = 1;
	if (!( lat_flg && long_flg && ht_flg )) {
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



/*
 * move data from NTP to buffer (toss the extra in the unlikely case it won't fit)
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
	int i, m;
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
			if (oncore_checksum_ok(rcvbuf, l)) {
				if (instance->shmem != NULL) {
					instance->shmem[oncore_messages[m].shmem + 2]++;
					memcpy(instance->shmem + oncore_messages[m].shmem + 3,
					    rcvbuf, (size_t) l);
				}
				oncore_msg_any(instance, rcvbuf, (size_t) (l-3), m);
				if (oncore_messages[m].handler)
					oncore_messages[m].handler(instance, rcvbuf, (size_t) (l-3));
			} else if (debug) {
				printf("ONCORE[%d]: Checksum mismatch!\n", instance->unit);
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



static void
oncore_get_timestamp(
	struct instance *instance,
	long dt1,	/* tick offset THIS time step */
	long dt2	/* tick offset NEXT time step */
	)
{
	int	Rsm;
	u_long	i, j;
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

#if 1
	/* If we are in SiteSurvey mode, then we are in 3D mode, and we fall thru.
	 * If we have Finished the SiteSurvey, then we fall thru for the 14/15
	 *  times we get here in 0D mode (the 1/15 is in 3D for SHMEM).
	 * This gives good time, which gets better when the SS is done.
	 */

	if ((instance->site_survey == ONCORE_SS_DONE) && (instance->mode != MODE_0D))
#else
	/* old check, only fall thru for SS_DONE and 0D mode, 2h45m wait for ticks */

	if ((instance->site_survey != ONCORE_SS_DONE) || (instance->mode != MODE_0D))
#endif
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
		msyslog(LOG_ERR, "refclock_ioctl: time_pps_getcap failed: %m");
		return;
	}

	if (time_pps_getparams(instance->pps_h, &current_params) < 0) {
		msyslog(LOG_ERR, "refclock_ioctl: time_pps_getparams failed: %m");
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

	/* print out information about this timestamp (long line) */

	ts_tmp = ts;
	ts_tmp.l_ui = 0;	/* zero integer part */
	LFPTOD(&ts_tmp, dmy);	/* convert fractional part to a double */
	j = 1.0e9*dmy;		/* then to integer ns */

	Rsm = 0;
	if (instance->chan == 6)
		Rsm = instance->BEHa[64];
	else if (instance->chan == 8)
		Rsm = instance->BEHa[72];
	else if (instance->chan == 12)
		Rsm = ((instance->BEHa[129]<<8) | instance->BEHa[130]);

	if (instance->chan == 6 || instance->chan == 8) {
		sprintf(instance->pp->a_lastcode,	/* MAX length 128, currently at 117 */
	"%u.%09lu %d %d %2d %2d %2d %2ld rstat   %02x dop %4.1f nsat %2d,%d traim %d sigma %2d neg-sawtooth %3d sat %d%d%d%d%d%d%d%d",
		    ts.l_ui, j,
		    instance->pp->year, instance->pp->day,
		    instance->pp->hour, instance->pp->minute, instance->pp->second,
		    (long) tsp->tv_sec % 60,
		    Rsm, 0.1*(256*instance->BEHa[35]+instance->BEHa[36]),
		    /*rsat	dop */
		    instance->BEHa[38], instance->BEHa[39], instance->BEHn[21],
		    /*	nsat visible,	  nsat tracked,     traim */
		    instance->BEHn[23]*256+instance->BEHn[24], (s_char) instance->BEHn[25],
		    /* sigma				   neg-sawtooth */
	  /*sat*/   instance->BEHa[41], instance->BEHa[45], instance->BEHa[49], instance->BEHa[53],
		    instance->BEHa[57], instance->BEHa[61], instance->BEHa[65], instance->BEHa[69]
		    );					/* will be 0 for 6 chan */
	} else if (instance->chan == 12) {
		sprintf(instance->pp->a_lastcode,
 "%u.%09lu %d %d %2d %2d %2d %2ld rstat %02x dop %4.1f nsat %2d,%d traim %d sigma %d neg-sawtooth %3d sat %d%d%d%d%d%d%d%d%d%d%d%d",
		    ts.l_ui, j,
		    instance->pp->year, instance->pp->day,
		    instance->pp->hour, instance->pp->minute, instance->pp->second,
		    (long) tsp->tv_sec % 60,
		    Rsm, 0.1*(256*instance->BEHa[53]+instance->BEHa[54]),
		    /*rsat	dop */
		    instance->BEHa[55], instance->BEHa[56], instance->BEHn[6],
		    /*	nsat visible,	  nsat tracked	 traim */
		    instance->BEHn[12]*256+instance->BEHn[13], (s_char) instance->BEHn[14],
		    /* sigma				   neg-sawtooth */
	  /*sat*/   instance->BEHa[58], instance->BEHa[64], instance->BEHa[70], instance->BEHa[76],
		    instance->BEHa[82], instance->BEHa[88], instance->BEHa[94], instance->BEHa[100],
		    instance->BEHa[106], instance->BEHa[112], instance->BEHa[118], instance->BEHa[124]
		    );
	}

	if (debug > 2) {
		int n;
		n = strlen(instance->pp->a_lastcode);
		printf("ONCORE[%d]: len = %d %s\n", instance->unit, n, instance->pp->a_lastcode);
	}

	/* and some things I dont understnd (magic ntp things) */

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
		instance->pp->lastref = instance->pp->lastrec;
		refclock_receive(instance->peer);
	}
}


/*************** oncore_msg_XX routines start here *******************/


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



/* Latitude, Longitude, Height */

static void
oncore_msg_Adef(
	struct instance *instance,
	u_char *buf,
	size_t len
	)
{
}



/* Mask Angle */

static void
oncore_msg_Ag(
	struct instance *instance,
	u_char *buf,
	size_t len
	)
{		char  Msg[160], *cp;

		cp = "set to";
		if (instance->o_state == ONCORE_RUN)
			cp = "is";

		instance->Ag = buf[4];
		sprintf(Msg, "Satellite mask angle %s %d degrees", cp, (int) instance->Ag);
		record_clock_stats(&(instance->peer->srcadr), Msg);
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
	instance->ss_lat  = buf_w32(&buf[4]);
	instance->ss_long = buf_w32(&buf[8]);
	instance->ss_ht   = buf_w32(&buf[12]);

	/* Print out Position */
	oncore_print_posn(instance);
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
	char	*cp;

	instance->saw_At = 1;
	if (instance->site_survey == ONCORE_SS_TESTING) {
		if (buf[4] == 2) {
			record_clock_stats(&(instance->peer->srcadr),
					"Initiating hardware 3D site survey");

			cp = "SSstate = ONCORE_SS_HW";
			record_clock_stats(&(instance->peer->srcadr), cp);
			instance->site_survey = ONCORE_SS_HW;
		}
	}
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

	if (instance->saw_Ay)
		return;

	instance->saw_Ay = 1;

	instance->offset = buf_w32(&buf[4]);

	sprintf(Msg, "PPS Offset is set to %ld ns", instance->offset);
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

	if (instance->saw_Az)
		return;

	instance->saw_Az = 1;

	instance->delay = buf_w32(&buf[4]);

	sprintf(Msg, "Cable delay is set to %ld ns", instance->delay);
	record_clock_stats(&(instance->peer->srcadr), Msg);
}



/* Ba, Ea and Ha come here, these contain Position */

static void
oncore_msg_BaEaHa(
	struct instance *instance,
	u_char *buf,
	size_t len
	)
{
	const char	*cp;
	char		Msg[160];
	int		mode;

	/* OK, we are close to the RUN state now.
	 * But we have a few more items to initialize first.
	 *
	 * At the beginning of this routine there are several 'timers'.
	 * We enter this routine 1/sec, and since the upper levels of NTP have usurped
	 * the use of timers, we use the 1/sec entry to do things that
	 * we would normally do with timers...
	 */

	if (instance->o_state == ONCORE_CHECK_CHAN) {	/* here while checking for the # chan */
		if (buf[2] == 'B') {		/* 6chan */
			if (instance->chan_ck < 6) instance->chan_ck = 6;
		} else if (buf[2] == 'E') {	/* 8chan */
			if (instance->chan_ck < 8) instance->chan_ck = 8;
		} else if (buf[2] == 'H') {	/* 12chan */
			if (instance->chan_ck < 12) instance->chan_ck = 12;
		}

		if (instance->count3++ < 5)
			return;

		instance->count3 = 0;

		if (instance->chan_in != -1)	/* set in Input */
			instance->chan = instance->chan_in;
		else				/* set from test */
			instance->chan = instance->chan_ck;

		sprintf(Msg, "Input   says chan = %d", instance->chan_in);
		record_clock_stats(&(instance->peer->srcadr), Msg);
		sprintf(Msg, "Model # says chan = %d", instance->chan_id);
		record_clock_stats(&(instance->peer->srcadr), Msg);
		sprintf(Msg, "Testing says chan = %d", instance->chan_ck);
		record_clock_stats(&(instance->peer->srcadr), Msg);
		sprintf(Msg, "Using        chan = %d", instance->chan);
		record_clock_stats(&(instance->peer->srcadr), Msg);

		instance->o_state = ONCORE_HAVE_CHAN;
		cp = "state = ONCORE_HAVE_CHAN";
		record_clock_stats(&(instance->peer->srcadr), cp);

		instance->timeout = 4;
		oncore_sendmsg(instance->ttyfd, oncore_cmd_Cj, sizeof(oncore_cmd_Cj));
		return;
	}

	if (instance->o_state != ONCORE_ALMANAC && instance->o_state != ONCORE_RUN)
		return;

	/* PAUSE 5sec */

	if (instance->count) {
		if (instance->count++ < 5)	/* make sure results are stable, using position */
			return;
		instance->count = 0;
	}

	memcpy(instance->BEHa, buf, (size_t) (len+3));	/* Ba, Ea or Ha */

	/* check the antenna and almanac for changes (did it get unplugged, is it ready?) */

	oncore_check_almanac(instance);
	oncore_check_antenna(instance);

	/* Almanac mode, waiting for Almanac, we can't do anything till we have it */
	/* When we have an almanac, we will start the Bn/En/@@Hn messages */

	if (instance->o_state == ONCORE_ALMANAC)
		if (oncore_wait_almanac(instance))
			return;

	/* do some things once when we get this far in BaEaHa */

	if (instance->once) {
		instance->once = 0;
		instance->count2 = 1;

		/* Have we seen an @@At (position hold) command response */
		/* if not, message out */

		if (instance->chan != 12 && !instance->saw_At) {
			cp = "Not Good, no @@At command (no Position Hold), must be a GT/GT+";
			record_clock_stats(&(instance->peer->srcadr), cp);
			oncore_sendmsg(instance->ttyfd, oncore_cmd_Av1, sizeof(oncore_cmd_Av1));
		}

		/* have an Almanac, can start the SiteSurvey
		 * (actually only need to get past the almanac_load where we diddle with At
		 *  command,- we can't change it after we start the HW_SS below
		 */

		mode = instance->init_type;
		switch (mode) {
		case 0: /* NO initialization, don't change anything */
		case 1: /* Use given Position */
		case 3:
			instance->site_survey = ONCORE_SS_DONE;
			cp = "SSstate = ONCORE_SS_DONE";
			record_clock_stats(&(instance->peer->srcadr), cp);
			break;

		case 2:
		case 4: /* Site Survey */
			cp = "SSstate = ONCORE_SS_TESTING";
			record_clock_stats(&(instance->peer->srcadr), cp);
			instance->site_survey = ONCORE_SS_TESTING;
			instance->count1 = 1;
			if (instance->chan == 12)
				oncore_sendmsg(instance->ttyfd, oncore_cmd_Gd3,  sizeof(oncore_cmd_Gd3));  /* M12+T */
			else
				oncore_sendmsg(instance->ttyfd, oncore_cmd_At2,  sizeof(oncore_cmd_At2));  /* not GT, arg not VP */
			break;
		}

		/* Read back PPS Offset for Output */
		/* Nb. This will fail silently for early UT (no plus) and M12 models */

		oncore_sendmsg(instance->ttyfd, oncore_cmd_Ayx,  sizeof(oncore_cmd_Ayx));

		/* Read back Cable Delay for Output */

		oncore_sendmsg(instance->ttyfd, oncore_cmd_Azx,  sizeof(oncore_cmd_Azx));

		/* Read back Satellite Mask Angle for Output */

		oncore_sendmsg(instance->ttyfd, oncore_cmd_Agx,  sizeof(oncore_cmd_Agx));
	}

	if (instance->count1) {
		if (instance->count1++ > 5 || instance->site_survey == ONCORE_SS_HW) {
			instance->count1 = 0;
			if (instance->site_survey == ONCORE_SS_TESTING) {
				/*
				 * For instance->site_survey to still be ONCORE_SS_TESTING, then after a 5sec
				 * wait after the @@At2/@@Gd3 command we have not changed the state to
				 * ONCORE_SS_HW.  If the Hardware is capable of doing a Site Survey, then
				 * the variable would have been changed by now.
				 * There are three possibilities:
				 * 6/8chan
				 *   (a) We did not get a response to the @@At0 or @@At2 commands,
				 *	   and it must be a GT/GT+/SL with no position hold mode.
				 *	   We will have to do it ourselves.
				 *   (b) We saw the @@At0, @@At2 commands, but @@At2 failed,
				 *	   must be a VP or older UT which doesn't have Site Survey mode.
				 *	   We will have to do it ourselves.
				 * 12chan
				 *   (c) We saw the @@Gd command, but @@Gd3 failed,
				 *	   We will have to do it ourselves.
				 */

				sprintf(Msg, "Initiating software 3D site survey (%d samples)",
					POS_HOLD_AVERAGE);
				record_clock_stats(&(instance->peer->srcadr), Msg);

				record_clock_stats(&(instance->peer->srcadr), "SSstate = ONCORE_SS_SW");
				instance->site_survey = ONCORE_SS_SW;

				instance->ss_lat = instance->ss_long = instance->ss_ht = 0;
				if (instance->chan == 12)
					oncore_sendmsg(instance->ttyfd, oncore_cmd_Gd0, sizeof(oncore_cmd_Gd0)); /* disable */
				else {
					oncore_sendmsg(instance->ttyfd, oncore_cmd_At0, sizeof(oncore_cmd_At0)); /* disable */
					oncore_sendmsg(instance->ttyfd, oncore_cmd_Av0, sizeof(oncore_cmd_Av0)); /* disable */
				}
			}
		}
	}

	/* check the mode we are in 0/2/3D */

	if (instance->chan == 6) {
		if (instance->BEHa[64]&0x8)
			instance->mode = MODE_0D;
		else if (instance->BEHa[64]&0x10)
			instance->mode = MODE_2D;
		else if (instance->BEHa[64]&0x20)
			instance->mode = MODE_3D;
	} else if (instance->chan == 8) {
		if (instance->BEHa[72]&0x8)
			instance->mode = MODE_0D;
		else if (instance->BEHa[72]&0x10)
			instance->mode = MODE_2D;
		else if (instance->BEHa[72]&0x20)
			instance->mode = MODE_3D;
	} else if (instance->chan == 12) {
		int bits;

		bits = (instance->BEHa[129]>>5) & 0x7;	/* actually Ha */
		if (bits == 0x4)
			instance->mode = MODE_0D;
		else if (bits == 0x6)
			instance->mode = MODE_2D;
		else if (bits == 0x7)
			instance->mode = MODE_3D;
	}

	/* copy the record to the (extra) location in SHMEM */

	if (instance->shmem) {
		int	i;
		u_char	*smp;	 /* pointer to start of shared mem for Ba/Ea/Ha */

		switch(instance->chan) {
		case 6:   smp = &instance->shmem[instance->shmem_Ba]; break;
		case 8:   smp = &instance->shmem[instance->shmem_Ea]; break;
		case 12:  smp = &instance->shmem[instance->shmem_Ha]; break;
		default:  smp = (u_char) 0;			      break;
		}

		switch (instance->mode) {
		case MODE_0D:	i = 1; break;	/* 0D, Position Hold */
		case MODE_2D:	i = 2; break;	/* 2D, Altitude Hold */
		case MODE_3D:	i = 3; break;	/* 3D fix */
		default:	i = 0; break;
		}

		if (i) {
			i *= (len+6);
			smp[i + 2]++;
			memcpy(&smp[i+3], buf, (size_t) (len+3));
		}
	}

	/*
	 * check if timer active
	 * if it hasn't been cleared, then @@Bn/@@En/@@Hn did not respond
	 */

	if (instance->traim_delay) {
		if (instance->traim_delay++ > 5) {
			instance->traim = 0;
			instance->traim_delay = 0;
			cp = "ONCORE: Did not detect TRAIM response, TRAIM = OFF";
			record_clock_stats(&(instance->peer->srcadr), cp);

			oncore_set_traim(instance);
		} else
			return;

	}

	/* by now should have a @@Ba/@@Ea/@@Ha with good data in it */

	if (!instance->have_dH && !instance->traim_delay)
		oncore_compute_dH(instance);

	/*
	 * must be ONCORE_RUN if we are here.
	 * Have # chan and TRAIM by now.
	 */

	instance->pp->year   = buf[6]*256+buf[7];
	instance->pp->day    = ymd2yd(buf[6]*256+buf[7], buf[4], buf[5]);
	instance->pp->hour   = buf[8];
	instance->pp->minute = buf[9];
	instance->pp->second = buf[10];

	/*
	 * Are we doing a Hardware or Software Site Survey?
	 */

	if (instance->site_survey == ONCORE_SS_HW || instance->site_survey == ONCORE_SS_SW)
		oncore_ss(instance);

	/* see if we ever saw a response from the @@Ayx above */

	if (instance->count2) {
		if (instance->count2++ > 5) {	/* this delay to check on @@Ay command */
			instance->count2 = 0;

			/* Have we seen an Ay (1PPS time offset) command response */
			/* if not, and non-zero offset, zero the offset, and send message */

			if (!instance->saw_Ay && instance->offset) {
				cp = "No @@Ay command, PPS OFFSET ignored";
				record_clock_stats(&(instance->peer->srcadr), cp);
				instance->offset = 0;
			}
		}
	}

	/*
	 * Check the leap second status once per day.
	 */

	oncore_check_leap_sec(instance);

	/*
	 * if SHMEM active, every 15s, steal one 'tick' to get 2D or 3D posn.
	 */

	if (instance->shmem && !instance->shmem_bad_Ea && instance->shmem_Posn && (instance->site_survey == ONCORE_SS_DONE))
		oncore_shmem_get_3D(instance);

	if (!instance->traim)	/* NO traim, no BnEnHn, go get tick */
		oncore_get_timestamp(instance, instance->offset, instance->offset);
}



/* Almanac Status */

static void
oncore_msg_Bd(
	struct instance *instance,
	u_char *buf,
	size_t len
	)
{
	char Msg[160];

	sprintf(Msg, "Bd: Almanac %s, week = %d, t = %d, %d SVs: %x",
		((buf[4]) ? "LOADED" : "(NONE)"), buf[5], buf[6], buf[7], w32(&buf[8]) );
	record_clock_stats(&(instance->peer->srcadr), Msg);
}



/* get leap-second warning message */

/*
 * @@Bj does NOT behave as documented in current Oncore firmware.
 * It turns on the LEAP indicator when the data is set, and does not,
 * as documented, wait until the beginning of the month when the
 * leap second will occur.
 * Since this firmware bug will never be fixed in all the outstanding Oncore receivers
 * @@Bj is only called in June/December.
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



static void
oncore_msg_BnEnHn(
	struct instance *instance,
	u_char *buf,
	size_t	len
	)
{
	long	dt1, dt2;
	char	*cp;

	if (instance->o_state != ONCORE_RUN)
		return;

	if (instance->traim_delay) {	 /* flag that @@Bn/@@En/Hn returned */
			instance->traim_ck = 1;
			instance->traim_delay = 0;
			cp = "ONCORE: Detected TRAIM, TRAIM = ON";
			record_clock_stats(&(instance->peer->srcadr), cp);

			oncore_set_traim(instance);
	}

	memcpy(instance->BEHn, buf, (size_t) len);	/* Bn or En or Hn */

	/* If Time RAIM doesn't like it, don't trust it */

	if (buf[2] == 'H') {
		if (instance->BEHn[6])	/* bad TRAIM */
			return;

		dt1 = instance->saw_tooth + instance->offset;	 /* dt this time step */
		instance->saw_tooth = (s_char) instance->BEHn[10]; /* update for next time Hn[10] */
		dt2 = instance->saw_tooth + instance->offset;	 /* dt next time step */
	} else {
		if (instance->BEHn[21]) /* bad TRAIM */
			return;

		dt1 = instance->saw_tooth + instance->offset;	 /* dt this time step */
		instance->saw_tooth = (s_char) instance->BEHn[25]; /* update for next time */
		dt2 = instance->saw_tooth + instance->offset;	 /* dt next time step */
	}

	oncore_get_timestamp(instance, dt1, dt2);
}



/* Here for @@Ca, @@Fa and @@Ia messages */

/* These are Self test Commands for 6, 8, and 12 chan receivers.
 * There are good reasons NOT to do a @@Ca, @@Fa or @@Ia command with the ONCORE.
 * It was found that under some circumstances the following
 * command would fail if issued immediately after the return from the
 * @@Fa, but a 2sec delay seemed to fix things.  Since simply calling
 * sleep(2) is wasteful, and may cause trouble for some OS's, repeating
 * itimer, we set a flag, and test it at the next POLL.  If it hasn't
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
	int	i;

	if (instance->o_state == ONCORE_TEST_SENT) {
		enum antenna_state antenna;

		instance->timeout = 0;

		if (debug > 2) {
			if (buf[2] == 'I')
				printf("ONCORE[%d]: >>@@%ca %x %x %x\n", instance->unit, buf[2], buf[4], buf[5], buf[6]);
			else
				printf("ONCORE[%d]: >>@@%ca %x %x\n", instance->unit, buf[2], buf[4], buf[5]);
		}

		antenna = (buf[4] & 0xc0) >> 6;
		buf[4] &= ~0xc0;

		i = buf[4] || buf[5];
		if (buf[2] == 'I') i = i || buf[6];
		if (i) {
			if (buf[2] == 'I') {
				msyslog(LOG_ERR, "ONCORE[%d]: self test failed: result %02x %02x %02x",
					instance->unit, buf[4], buf[5], buf[6]);
			} else {
				msyslog(LOG_ERR, "ONCORE[%d]: self test failed: result %02x %02x",
					instance->unit, buf[4], buf[5]);
			}
			cp = "ONCORE: self test failed, shutting down driver";
			record_clock_stats(&instance->peer->srcadr, cp);

			refclock_report(instance->peer, CEVNT_FAULT);
			oncore_shutdown(instance->unit, instance->peer);
			return;
		}

		/* report the current antenna state */

		oncore_antenna_report(instance, antenna);

		instance->o_state = ONCORE_INIT;
		cp = "state = ONCORE_INIT";
		record_clock_stats(&(instance->peer->srcadr), cp);

		instance->timeout = 4;
		oncore_sendmsg(instance->ttyfd, oncore_cmd_Cj, sizeof(oncore_cmd_Cj));
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

	if (buf[4] == 5 && buf[5] > 0 && buf[5] < 26)
		i = buf[5];
	else if (buf[4] == 4 && buf[5] <= 5)
		i = buf[5] + 24;
	else if (buf[4] == 4 && buf[5] <= 10)
		i = buf[5] + 23;
	else if (buf[4] == 4 && buf[5] == 25)
		i = 34;
	else {
		char *cp;

		cp = "Cb: Response is NO ALMANAC";
		record_clock_stats(&(instance->peer->srcadr), cp);
		return;
	}

	i *= 36;
	instance->shmem[instance->shmem_Cb + i + 2]++;
	memcpy(instance->shmem + instance->shmem_Cb + i + 3, buf, (size_t) (len + 3));

#if 1
	{
	char Msg[160];
	sprintf(Msg, "See Cb [%d,%d]", buf[4], buf[5]);
	record_clock_stats(&(instance->peer->srcadr), Msg);
	}
#endif
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
		oncore_sendmsg(instance->ttyfd, oncore_cmd_Cg, sizeof(oncore_cmd_Cg)); /* Return to  Posn Fix mode */
										       /* Reset set VP to IDLE */
		instance->o_state = ONCORE_TEST_SENT;
		cp = "state = ONCORE_TEST_SENT";
		record_clock_stats(&(instance->peer->srcadr), cp);

		oncore_sendmsg(instance->ttyfd, oncore_cmd_Cj, sizeof(oncore_cmd_Cj));
	}
}



/*
 * This is the Grand Central Station for the Preliminary Initialization.
 * Once done here we move on to oncore_msg_BaEaHa for final Initialization and Running.
 *
 * We do an @@Cj whenever we need a safe command for all Oncores.
 * The @@Cj gets us back here where we can switch to the next phase of setup.
 *
 * o Once at the very beginning (in start) to get the Model number.
 *   This info is printed, but no longer used.
 * o Again after we have determined the number of Channels in the receiver.
 * o And once later after we have done a reset and test, (which may hang),
 *   as we are about to initialize the Oncore and start it running.
 * o We have one routine below for each case.
 */

static void
oncore_msg_Cj(
	struct instance *instance,
	u_char *buf,
	size_t len
	)
{
	int	mode;
	char	*cp;

	memcpy(instance->Cj, buf, len);

	instance->timeout = 0;
	if (instance->o_state == ONCORE_CHECK_ID) {
		oncore_msg_Cj_id(instance, buf, len);
		oncore_chan_test(instance);
	} else if (instance->o_state == ONCORE_HAVE_CHAN) {
		mode = instance->init_type;
		if (mode == 3 || mode == 4) {	/* Cf will return here to check for TEST */
			instance->o_state = ONCORE_RESET_SENT;
			cp = "state = ONCORE_RESET_SENT";
			record_clock_stats(&(instance->peer->srcadr), cp);
			oncore_sendmsg(instance->ttyfd, oncore_cmd_Cf, sizeof(oncore_cmd_Cf));
		} else {
			instance->o_state = ONCORE_TEST_SENT;
			cp = "state = ONCORE_TEST_SENT";
			record_clock_stats(&(instance->peer->srcadr), cp);
		}
	}

	if (instance->o_state == ONCORE_TEST_SENT) {
		if (instance->chan == 6)
			oncore_sendmsg(instance->ttyfd, oncore_cmd_Ca, sizeof(oncore_cmd_Ca));
		else if (instance->chan == 8)
			oncore_sendmsg(instance->ttyfd, oncore_cmd_Fa, sizeof(oncore_cmd_Fa));
		else if (instance->chan == 12)
			oncore_sendmsg(instance->ttyfd, oncore_cmd_Ia, sizeof(oncore_cmd_Ia));
	} else if (instance->o_state == ONCORE_INIT)
		oncore_msg_Cj_init(instance, buf, len);
}



/* The information on determining a Oncore 'Model', viz VP, UT, etc, from
 *	the Model Number comes from "Richard M. Hambly" <rick@cnssys.com>
 *	and from Motorola.  Until recently Rick was the only source of
 *	this information as Motorola didn't give the information out.
 *
 * Determine the Type from the Model #, this determines #chan and if TRAIM is
 *   available.
 *
 * The Information from this routine is NO LONGER USED.
 * The RESULTS are PRINTED, BUT NOT USED, and the routine COULD BE DELETED
 */

static void
oncore_msg_Cj_id(
	struct instance *instance,
	u_char *buf,
	size_t len
	)
{
	char *cp, *cp1, *cp2, Model[21], Msg[160];

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
	} else if (Model[0] == 'P') {
		cp = "M12";
		instance->model = ONCORE_M12;
	} else if (Model[0] == 'R' || Model[0] == 'D' || Model[0] == 'S') {
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

	/* use MODEL to set CHAN and TRAIM and possibly zero SHMEM */

	sprintf(Msg, "This looks like an Oncore %s with version %d.%d firmware.", cp, instance->version, instance->revision);
	record_clock_stats(&(instance->peer->srcadr), Msg);

	instance->chan_id = 8;	   /* default */
	if (instance->model == ONCORE_BASIC || instance->model == ONCORE_PVT6)
		instance->chan_id = 6;
	else if (instance->model == ONCORE_VP || instance->model == ONCORE_UT || instance->model == ONCORE_UTPLUS)
		instance->chan_id = 8;
	else if (instance->model == ONCORE_M12)
		instance->chan_id = 12;

	instance->traim_id = 0;    /* default */
	if (instance->model == ONCORE_BASIC || instance->model == ONCORE_PVT6)
		instance->traim_id = 0;
	else if (instance->model == ONCORE_VP || instance->model == ONCORE_UT || instance->model == ONCORE_UTPLUS)
		instance->traim_id = 1;
	else if (instance->model == ONCORE_M12)
		instance->traim_id = -1;

	sprintf(Msg, "Channels = %d, TRAIM = %s", instance->chan_id,
		((instance->traim_id < 0) ? "UNKNOWN" : ((instance->traim_id > 0) ? "ON" : "OFF")));
	record_clock_stats(&(instance->peer->srcadr), Msg);
}



/* OK, know type of Oncore, have possibly reset it, and have tested it.
 * We know the number of channels.
 * We will determine whether we have TRAIM before we actually start.
 * Now initialize.
 */

static void
oncore_msg_Cj_init(
	struct instance *instance,
	u_char *buf,
	size_t len
	)
{
	char *cp, Cmd[20], Msg[160];
	int	mode;


	/* The M12 with 1.3 or 2.0 Firmware, loses track of all Satellites and has to
	 * start again if we go from 0D -> 3D, then loses them again when we
	 * go from 3D -> 0D.  We do this to get a @@Ea message for SHMEM.
	 * For NOW we will turn this aspect of filling SHMEM off for the M12
	 */

	if (instance->chan == 12) {
		instance->shmem_bad_Ea = 1;
		sprintf(Msg, "*** SHMEM partially enabled for ONCORE M12 s/w v%d.%d ***", instance->version, instance->revision);
		record_clock_stats(&(instance->peer->srcadr), Msg);
	}

	oncore_sendmsg(instance->ttyfd, oncore_cmd_Cg, sizeof(oncore_cmd_Cg)); /* Return to  Posn Fix mode */
	oncore_sendmsg(instance->ttyfd, oncore_cmd_Bb, sizeof(oncore_cmd_Bb)); /* turn on for shmem (6/8/12) */
	oncore_sendmsg(instance->ttyfd, oncore_cmd_Ek, sizeof(oncore_cmd_Ek)); /* turn off (VP) */
	oncore_sendmsg(instance->ttyfd, oncore_cmd_Aw, sizeof(oncore_cmd_Aw)); /* UTC time (6/8/12) */
	oncore_sendmsg(instance->ttyfd, oncore_cmd_AB, sizeof(oncore_cmd_AB)); /* Appl type static (VP) */
	oncore_sendmsg(instance->ttyfd, oncore_cmd_Be, sizeof(oncore_cmd_Be)); /* Tell us the Almanac for shmem (6/8/12) */
	oncore_sendmsg(instance->ttyfd, oncore_cmd_Bd, sizeof(oncore_cmd_Bd)); /* Tell us when Almanac changes */

	mode = instance->init_type;

	/* If there is Position input in the Config file
	 * and mode = (1,3) set it as posn hold posn, goto 0D mode.
	 *  or mode = (2,4) set it as INITIAL position, and do Site Survey.
	 */

	if (instance->posn_set) {
		record_clock_stats(&(instance->peer->srcadr), "Setting Posn from input data");
		oncore_set_posn(instance);	/* this should print posn indirectly thru the As cmd */
	} else	/* must issue an @@At here to check on 6/8 Position Hold, set_posn would have */
		if (instance->chan != 12)
			oncore_sendmsg(instance->ttyfd, oncore_cmd_Atx, sizeof(oncore_cmd_Atx));

	if (mode != 0) {
			/* cable delay in ns */
		memcpy(Cmd, oncore_cmd_Az, (size_t) sizeof(oncore_cmd_Az));
		w32_buf(&Cmd[-2+4], instance->delay);
		oncore_sendmsg(instance->ttyfd, Cmd,  sizeof(oncore_cmd_Az));	/* 6,8,12 */

			/* PPS offset in ns */
		if (instance->offset) {
			memcpy(Cmd, oncore_cmd_Ay, (size_t) sizeof(oncore_cmd_Ay));	/* some have it, some don't */
			w32_buf(&Cmd[-2+4], instance->offset);			/* will check for hw response */
			oncore_sendmsg(instance->ttyfd, Cmd,  sizeof(oncore_cmd_Ay));
		}

		/* Satellite mask angle */

		if (instance->Ag != 0xff) {	/* will have 0xff in it if not set by user */
			memcpy(Cmd, oncore_cmd_Ag, (size_t) sizeof(oncore_cmd_Ag));
			Cmd[-2+4] = instance->Ag;
			oncore_sendmsg(instance->ttyfd, Cmd,  sizeof(oncore_cmd_Ag));
		}
	}

	/* 6, 8 12 chan - Position/Status/Data Output Message, 1/s
	 * now we're really running
	 * these were ALL started in the chan test,
	 * However, if we had mode=3,4 then commands got turned off, so we turn
	 * them on again here just in case
	 */

	if (instance->chan == 6) { /* start 6chan, kill 8,12chan commands, possibly testing VP in 6chan mode */
		oncore_sendmsg(instance->ttyfd, oncore_cmd_Ea0, sizeof(oncore_cmd_Ea0));
		oncore_sendmsg(instance->ttyfd, oncore_cmd_En0, sizeof(oncore_cmd_En0));
		oncore_sendmsg(instance->ttyfd, oncore_cmd_Ha0, sizeof(oncore_cmd_Ha0));
		oncore_sendmsg(instance->ttyfd, oncore_cmd_Hn0, sizeof(oncore_cmd_Hn0));
		oncore_sendmsg(instance->ttyfd, oncore_cmd_Ba,	sizeof(oncore_cmd_Ba ));
	} else if (instance->chan == 8) {  /* start 8chan, kill 6,12chan commands */
		oncore_sendmsg(instance->ttyfd, oncore_cmd_Ba0, sizeof(oncore_cmd_Ba0));
		oncore_sendmsg(instance->ttyfd, oncore_cmd_Bn0, sizeof(oncore_cmd_Bn0));
		oncore_sendmsg(instance->ttyfd, oncore_cmd_Ha0, sizeof(oncore_cmd_Ha0));
		oncore_sendmsg(instance->ttyfd, oncore_cmd_Hn0, sizeof(oncore_cmd_Hn0));
		oncore_sendmsg(instance->ttyfd, oncore_cmd_Ea,	sizeof(oncore_cmd_Ea ));
	} else if (instance->chan == 12){  /* start 12chan, kill 6,12chan commands */
		oncore_sendmsg(instance->ttyfd, oncore_cmd_Ba0, sizeof(oncore_cmd_Ba0));
		oncore_sendmsg(instance->ttyfd, oncore_cmd_Bn0, sizeof(oncore_cmd_Bn0));
		oncore_sendmsg(instance->ttyfd, oncore_cmd_Ea0, sizeof(oncore_cmd_Ea0));
		oncore_sendmsg(instance->ttyfd, oncore_cmd_En0, sizeof(oncore_cmd_En0));
		oncore_sendmsg(instance->ttyfd, oncore_cmd_Ha,	sizeof(oncore_cmd_Ha ));
	}

	instance->count = 1;
	instance->o_state = ONCORE_ALMANAC;
	cp = "state = ONCORE_ALMANAC";
	record_clock_stats(&(instance->peer->srcadr), cp);
}



/* 12chan position */

static void
oncore_msg_Ga(
	struct instance *instance,
	u_char *buf,
	size_t len
	)
{
	char Msg[160];
	long lat, lon, ht;
	double Lat, Lon, Ht;


	lat = buf_w32(&buf[4]);
	lon = buf_w32(&buf[8]);
	ht  = buf_w32(&buf[12]);  /* GPS ellipsoid */

	Lat = lat;
	Lon = lon;
	Ht  = ht;

	Lat /= 3600000;
	Lon /= 3600000;
	Ht  /= 100;


	sprintf(Msg, "Ga Posn Lat = %.7f, Lon = %.7f, Ht  = %.2f", Lat, Lon, Ht);
	record_clock_stats(&(instance->peer->srcadr), Msg);

	instance->ss_lat  = lat;
	instance->ss_long = lon;
	instance->ss_ht   = ht;

	oncore_print_posn(instance);
}



/* 12 chan time/date */

static void
oncore_msg_Gb(
	struct instance *instance,
	u_char *buf,
	size_t len
	)
{
	char	Msg[160], *gmts;
	int	mo, d, y, h, m, s, gmth, gmtm;

	mo = buf[4];
	d  = buf[5];
	y  = 256*buf[6]+buf[7];

	h  = buf[8];
	m  = buf[9];
	s  = buf[10];

	gmts = ((buf[11] == 0) ? "+" : "-");
	gmth = buf[12];
	gmtm = buf[13];

	sprintf(Msg, "Date/Time set to: %d%s%d %2d:%02d:%02d GMT (GMT offset is %s%02d:%02d)",
		d, Month[mo+1], y, h, m, s, gmts, gmth, gmtm);
	record_clock_stats(&(instance->peer->srcadr), Msg);
}



/*
 * Try to use Oncore M12+Timing Auto Survey Feature
 *	If its not there (M12), set flag to do it ourselves.
 */

static void
oncore_msg_Gd(
	struct instance *instance,
	u_char *buf,
	size_t len
	)
{
	char	*cp;

	if (instance->site_survey == ONCORE_SS_TESTING) {
		if (buf[4] == 3) {
			record_clock_stats(&(instance->peer->srcadr),
					"Initiating hardware 3D site survey");

			cp = "SSstate = ONCORE_SS_HW";
			record_clock_stats(&(instance->peer->srcadr), cp);
			instance->site_survey = ONCORE_SS_HW;
			}
	}
}



/* Leap Second for M12, gives all info from satellite message */
/* also in UT v3.0 */

static void
oncore_msg_Gj(
	struct instance *instance,
	u_char *buf,
	size_t len
	)
{
	int dt;
	char Msg[160], *cp;

	instance->saw_Gj = 1; /* flag, saw_Gj, dont need to try Bj in check_leap */

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

	if (buf[6] == instance->BEHa[6] && buf[7] == instance->BEHa[7] && /* year */
	    buf[8] == instance->BEHa[4]) {	/* month */
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



/* Power on failure */

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

/************** Small Subroutines ***************/


static void
oncore_antenna_report(
	struct instance *instance,
	enum antenna_state new_state)
{
	char *cp;

	if (instance->ant_state == new_state)
		return;

	switch (new_state) {
	case ONCORE_ANTENNA_OK: cp = "GPS antenna: OK";                   break;
	case ONCORE_ANTENNA_OC: cp = "GPS antenna: short (overcurrent)";  break;
	case ONCORE_ANTENNA_UC: cp = "GPS antenna: open (not connected)"; break;
	case ONCORE_ANTENNA_NV: cp = "GPS antenna: short (no voltage)";   break;
	default:		cp = "GPS antenna: ?";                    break;
	}

	instance->ant_state = new_state;
	record_clock_stats(&instance->peer->srcadr, cp);
}



static void
oncore_chan_test(
	struct instance *instance
	)
{
	char	*cp;

	/* subroutine oncore_Cj_id has determined the number of channels from the
	 * model number of the attached oncore.  This is not always correct since
	 * the oncore could have non-standard firmware.  Here we check (independently) by
	 * trying a 6, 8, and 12 chan command, and see which responds.
	 * Caution: more than one CAN respond.
	 *
	 * This #chan is used by the code rather than that calculated from the model number.
	 */

	instance->o_state = ONCORE_CHECK_CHAN;
	cp = "state = ONCORE_CHECK_CHAN";
	record_clock_stats(&(instance->peer->srcadr), cp);

	instance->count3 = 1;
	oncore_sendmsg(instance->ttyfd, oncore_cmd_Ba, sizeof(oncore_cmd_Ba));
	oncore_sendmsg(instance->ttyfd, oncore_cmd_Ea, sizeof(oncore_cmd_Ea));
	oncore_sendmsg(instance->ttyfd, oncore_cmd_Ha, sizeof(oncore_cmd_Ha));
}



/* check for a GOOD Almanac, have we got one yet? */

static void
oncore_check_almanac(
	struct instance *instance
	)
{
	if (instance->chan == 6) {
		instance->rsm.bad_almanac = instance->BEHa[64]&0x1;
		instance->rsm.bad_fix	  = instance->BEHa[64]&0x52;
	} else if (instance->chan == 8) {
		instance->rsm.bad_almanac = instance->BEHa[72]&0x1;
		instance->rsm.bad_fix	  = instance->BEHa[72]&0x52;
	} else if (instance->chan == 12) {
		int bits1, bits2;

		bits1 = (instance->BEHa[129]>>5) & 0x7; 	/* actually Ha */
		bits2 = instance->BEHa[130];
		instance->rsm.bad_almanac = (bits2 & 0x80);
		instance->rsm.bad_fix	  = (bits2 & 0x8) || (bits1 == 0x2);
					  /* too few sat     Bad Geom	  */
#if 0
		fprintf(stderr, "ONCORE[%d]: DEBUG BITS: (%x %x), (%x %x),  %x %x %x %x %x\n",
		instance->unit,
		instance->BEHa[129], instance->BEHa[130], bits1, bits2, instance->mode == MODE_0D,
		instance->mode == MODE_2D, instance->mode == MODE_3D,
		instance->rsm.bad_almanac, instance->rsm.bad_fix);
#endif
	}
}



/* check the antenna for changes (did it get unplugged?) */

static void
oncore_check_antenna(
	struct instance *instance
	)
{
	enum antenna_state antenna;		/* antenna state */

	antenna = instance->ant_state;
	if (instance->chan == 12)
		antenna = (instance->BEHa[130] & 0x6 ) >> 1;
	else
		antenna = (instance->BEHa[37] & 0xc0) >> 6;  /* prob unset 6, set GT, UT unset VP */

	oncore_antenna_report (instance, antenna);
}



/*
 * Check the leap second status once per day.
 *
 * Note that the ONCORE firmware for the Bj command is wrong at
 * least in the VP.
 * It starts advertising a LEAP SECOND as soon as the GPS satellite
 * data message (page 18, subframe 4) is updated to a date in the
 * future, and does not wait for the month that it will occur.
 * The event will usually be advertised several months in advance.
 * Since there is a one bit flag, there is no way to tell if it is
 * this month, or when...
 *
 * As such, we have the workaround below, of only checking for leap
 * seconds with the Bj command in June/December.
 *
 * The Gj command gives more information, and we can tell in which
 * month to apply the correction.
 *
 * Note that with the VP we COULD read the raw data message, and
 * interpret it ourselves, but since this is specific to this receiver
 * only, and the above workaround is adequate, we don't bother.
 */

static void
oncore_check_leap_sec(
	struct instance *instance
	)
{
	if (instance->Bj_day != instance->BEHa[5]) {	 /* do this 1/day */
		instance->Bj_day = instance->BEHa[5];

		if (instance->saw_Gj < 0) {	/* -1 DONT have Gj use Bj */
			if ((instance->BEHa[4] == 6) || (instance->BEHa[4] == 12))
				oncore_sendmsg(instance->ttyfd, oncore_cmd_Bj, sizeof(oncore_cmd_Bj));
			return;
		}

		if (instance->saw_Gj == 0)	/* 0 is dont know if we have Gj */
			instance->count4 = 1;

		oncore_sendmsg(instance->ttyfd, oncore_cmd_Gj, sizeof(oncore_cmd_Gj));
		return;
	}

	/* Gj works for some 6/8 chan UT and the M12	  */
	/* if no response from Gj in 5 sec, we try Bj	  */
	/* which isnt implemented in all the GT/UT either */

	if (instance->count4) { 	/* delay, waiting for Gj response */
		if (instance->saw_Gj == 1)
			instance->count4 = 0;
		else if (instance->count4++ > 5) {	/* delay, waiting for Gj response */
			instance->saw_Gj = -1;		/* didnt see it, will use Bj */
			instance->count4 = 0;
			if ((instance->BEHa[4] == 6) || (instance->BEHa[4] == 12))
				oncore_sendmsg(instance->ttyfd, oncore_cmd_Bj, sizeof(oncore_cmd_Bj));
		}
	}
}



/* check the message checksum,
 *  buf points to START of message ( @@ )
 *  len is length WITH CR/LF.
 */

static int
oncore_checksum_ok(
	u_char *buf,
	int	len
	)
{
	int	i, j;

	j = 0;
	for (i = 2; i < len-3; i++)
		j ^= buf[i];

	return(j == buf[len-3]);
}



static void
oncore_compute_dH(
	struct instance *instance
	)
{
	int GPS, MSL;
	char	Msg[160];

	/* Here calculate dH = GPS - MSL for output message */
	/* also set Altitude Hold mode if GT */

	instance->have_dH = 1;
	if (instance->chan == 12) {
		GPS = buf_w32(&instance->BEHa[39]);
		MSL = buf_w32(&instance->BEHa[43]);
	} else {
		GPS = buf_w32(&instance->BEHa[23]);
		MSL = buf_w32(&instance->BEHa[27]);
	}
	instance->dH = GPS - MSL;
	instance->dH /= 100.;

	/* if MSL is not set, the calculation is meaningless */

	if (MSL) {	/* not set ! */
		sprintf(Msg, "dH = (GPS - MSL) = %.2fm", instance->dH);
		record_clock_stats(&(instance->peer->srcadr), Msg);
	}
}



/*
 * try loading Almanac from shmem (where it was copied from shmem_old
 */

static void
oncore_load_almanac(
	struct instance *instance
	)
{
	u_char	*cp, Cmd[20];
	int	n;
	struct timeval tv;
	struct tm *tm;

	if (!instance->shmem)
		return;

#if 1
	for (cp=instance->shmem+4; (n = 256*(*(cp-3)) + *(cp-2)); cp+=(n+3)) {
		if (!strncmp(cp, "@@Cb", 4) &&
		    oncore_checksum_ok(cp, 33) &&
		    (*(cp+4) == 4 || *(cp+4) == 5)) {
			write(instance->ttyfd, cp, n);
#if 1
			oncore_print_Cb(instance, cp);
#endif
		}
	}
#else
/************DEBUG************/
	for (cp=instance->shmem+4; (n = 256*(*(cp-3)) + *(cp-2)); cp+=(n+3)) {
		char Msg[160];

		sprintf(Msg, "See %c%c%c%c %d", *(cp), *(cp+1), *(cp+2), *(cp+3), *(cp+4));
		record_clock_stats(&(instance->peer->srcadr), Msg);

		if (!strncmp(cp, "@@Cb", 4)) {
			oncore_print_Cb(instance, cp);
			if (oncore_checksum_ok(cp, 33)) {
				if (*(cp+4) == 4 || *(cp+4) == 5) {
					record_clock_stats(&(instance->peer->srcadr), "GOOD SF");
					write(instance->ttyfd, cp, n);
				} else
					record_clock_stats(&(instance->peer->srcadr), "BAD SF");
			} else
				record_clock_stats(&(instance->peer->srcadr), "BAD CHECKSUM");
		}
	}
/************DEBUG************/
#endif

	/* Must load position and time or the Almanac doesn't do us any good */

	if (!instance->posn_set) {	/* if we input a posn use it, else from SHMEM */
		record_clock_stats(&(instance->peer->srcadr), "Loading Posn from SHMEM");
		for (cp=instance->shmem+4; (n = 256*(*(cp-3)) + *(cp-2));  cp+=(n+3)) {
			if ((instance->chan == 6  && (!strncmp(cp, "@@Ba", 4) && oncore_checksum_ok(cp,  68))) ||
			    (instance->chan == 8  && (!strncmp(cp, "@@Ea", 4) && oncore_checksum_ok(cp,  76))) ||
			    (instance->chan == 12 && (!strncmp(cp, "@@Ha", 4) && oncore_checksum_ok(cp, 154)))) {
				int ii, jj, kk;

				instance->posn_set = 1;
				ii = buf_w32(cp + 15);
				jj = buf_w32(cp + 19);
				kk = buf_w32(cp + 23);
{
char Msg[160];
sprintf(Msg, "SHMEM posn = %d (%d, %d, %d)", cp-instance->shmem, ii, jj, kk);
record_clock_stats(&(instance->peer->srcadr), Msg);
}
				if (ii != 0 || jj != 0 || kk != 0) { /* phk asked for this test */
					instance->ss_lat  = ii;
					instance->ss_long = jj;
					instance->ss_ht   = kk;
				}
			}
		}
	}
	oncore_set_posn(instance);

	/* and set time to time from Computer clock */

	gettimeofday(&tv, 0);
	tm = gmtime((const time_t *) &tv.tv_sec);
#if 1
	{
	char Msg[160];
	sprintf(Msg, "DATE %d %d %d, %d %d %d", 1900+tm->tm_year, tm->tm_mon, tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec);
	record_clock_stats(&(instance->peer->srcadr), Msg);
	}
#endif
	if (instance->chan == 12) {
		memcpy(Cmd, oncore_cmd_Gb, (size_t) sizeof(oncore_cmd_Gb));
		Cmd[-2+4]  = tm->tm_mon;
		Cmd[-2+5]  = tm->tm_mday;
		Cmd[-2+6]  = (1900+tm->tm_year)/256;
		Cmd[-2+7]  = (1900+tm->tm_year)%256;
		Cmd[-2+8]  = tm->tm_hour;
		Cmd[-2+9]  = tm->tm_min;
		Cmd[-2+10] = tm->tm_sec;
		Cmd[-2+11] = 0;
		Cmd[-2+12] = 0;
		Cmd[-2+13] = 0;
		oncore_sendmsg(instance->ttyfd, Cmd,  sizeof(oncore_cmd_Gb));
	} else {
		/* First set GMT offset to zero */

		oncore_sendmsg(instance->ttyfd, oncore_cmd_Ab,	sizeof(oncore_cmd_Ab));

		memcpy(Cmd, oncore_cmd_Ac, (size_t) sizeof(oncore_cmd_Ac));
		Cmd[-2+4] = tm->tm_mon;
		Cmd[-2+5] = tm->tm_mday;
		Cmd[-2+6] = (1900+tm->tm_year)/256;
		Cmd[-2+7] = (1900+tm->tm_year)%256;
		oncore_sendmsg(instance->ttyfd, Cmd,  sizeof(oncore_cmd_Ac));

		memcpy(Cmd, oncore_cmd_Aa, (size_t) sizeof(oncore_cmd_Aa));
		Cmd[-2+4] = tm->tm_hour;
		Cmd[-2+5] = tm->tm_min;
		Cmd[-2+6] = tm->tm_sec;
		oncore_sendmsg(instance->ttyfd, Cmd,  sizeof(oncore_cmd_Aa));
	}

	record_clock_stats(&(instance->peer->srcadr), "Setting Posn and Time after Loading Almanac");
}



/* Almanac data input */

static void
oncore_print_Cb(
	struct instance *instance,
	u_char *cp
	)
{
#if 0
	int	ii;
	char	Msg[160];

	printf("DEBUG: See: %c%c%c%c\n", *(cp), *(cp+1), *(cp+2), *(cp+3));
	printf("DEBUG: Cb: [%d,%d]", *(cp+4), *(cp+5));
	for(ii=0; ii<33; ii++)
		printf(" %d", *(cp+ii));
	printf("\n");

	sprintf(Msg, "Debug: Cb: [%d,%d]", *(cp+4), *(cp+5));
	record_clock_stats(&(instance->peer->srcadr), Msg);
#endif
}


#if 0
static void
oncore_print_array(
	u_char *cp,
	int	n
	)
{
	int	jj, i, j, nn;

	nn = 0;
	printf("\nTOP\n");
	jj = n/16;
	for (j=0; j<jj; j++) {
		printf("%4d: ", nn);
		nn += 16;
		for (i=0; i<16; i++)
			printf(" %o", *cp++);
		printf("\n");
	}
}
#endif


static void
oncore_print_posn(
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
	sprintf(Msg,
	    "Lat = %c %3ddeg %7.4fm,   Long = %c %3ddeg %8.5fm,  Alt = %7.2fm (%7.2fft) GPS", ns, idx, xm, ew, idy, ym, hm, hft);
	record_clock_stats(&(instance->peer->srcadr), Msg);

	imx = xm;
	imy = ym;
	is  = lat%60000;
	xs  = is/1000.;
	is  = lon%60000;
	ys  = is/1000.;
	sprintf(Msg,
	    "Lat = %c %3ddeg %2dm %5.2fs, Long = %c %3ddeg %2dm %5.2fs, Alt = %7.2fm (%7.2fft) GPS", ns, idx, imx, xs, ew, idy, imy, ys, hm, hft);
	record_clock_stats(&(instance->peer->srcadr), Msg);
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

	if (debug > 4)
		printf("ONCORE: Send @@%c%c %d\n", ptr[0], ptr[1], (int) len);
	write(fd, "@@", (size_t) 2);
	write(fd, ptr, len);
	while (len--)
		cs ^= *ptr++;
	write(fd, &cs, (size_t) 1);
	write(fd, "\r\n", (size_t) 2);
}



static void
oncore_set_posn(
	struct instance *instance
	)
{
	int	mode;
	char	Cmd[20];

	/* Turn OFF position hold, it needs to be off to set position (for some units),
	   will get set ON in @@Ea later */

	if (instance->chan == 12)
		oncore_sendmsg(instance->ttyfd, oncore_cmd_Gd0, sizeof(oncore_cmd_Gd0)); /* (12) */
	else {
		oncore_sendmsg(instance->ttyfd, oncore_cmd_At0, sizeof(oncore_cmd_At0)); /* (6/8) */
		oncore_sendmsg(instance->ttyfd, oncore_cmd_Av0, sizeof(oncore_cmd_Av0)); /* (6/8) */
	}

	mode = instance->init_type;

	if (mode != 0) {	/* first set posn hold position */
		memcpy(Cmd, oncore_cmd_As, (size_t) sizeof(oncore_cmd_As));	/* don't modify static variables */
		w32_buf(&Cmd[-2+4],  (int) instance->ss_lat);
		w32_buf(&Cmd[-2+8],  (int) instance->ss_long);
		w32_buf(&Cmd[-2+12], (int) instance->ss_ht);
		Cmd[-2+16] = 0;
		oncore_sendmsg(instance->ttyfd, Cmd,  sizeof(oncore_cmd_As));	/* posn hold 3D posn (6/8/12) */

		memcpy(Cmd, oncore_cmd_Au, (size_t) sizeof(oncore_cmd_Au));
		w32_buf(&Cmd[-2+4], (int) instance->ss_ht);
		Cmd[-2+8] = 0;
		oncore_sendmsg(instance->ttyfd, Cmd,  sizeof(oncore_cmd_Au));	/* altitude hold (6/8/12 not UT, M12T) */

		/* next set current position */

		if (instance->chan == 12) {
			memcpy(Cmd, oncore_cmd_Ga, (size_t) sizeof(oncore_cmd_Ga));
			w32_buf(&Cmd[-2+4], (int) instance->ss_lat);
			w32_buf(&Cmd[-2+8], (int) instance->ss_long);
			w32_buf(&Cmd[-2+12],(int) instance->ss_ht);
			Cmd[-2+16] = 0;
			oncore_sendmsg(instance->ttyfd, Cmd,  sizeof(oncore_cmd_Ga));		  /* 3d posn (12) */
		} else {
			memcpy(Cmd, oncore_cmd_Ad, (size_t) sizeof(oncore_cmd_Ad));
			w32_buf(&Cmd[-2+4], (int) instance->ss_lat);
			oncore_sendmsg(instance->ttyfd, Cmd,  sizeof(oncore_cmd_Ad));	/* lat (6/8) */

			memcpy(Cmd, oncore_cmd_Ae, (size_t) sizeof(oncore_cmd_Ae));
			w32_buf(&Cmd[-2+4], (int) instance->ss_long);
			oncore_sendmsg(instance->ttyfd, Cmd,  sizeof(oncore_cmd_Ae));	/* long (6/8) */

			memcpy(Cmd, oncore_cmd_Af, (size_t) sizeof(oncore_cmd_Af));
			w32_buf(&Cmd[-2+4], (int) instance->ss_ht);
			Cmd[-2+8] = 0;
			oncore_sendmsg(instance->ttyfd, Cmd,  sizeof(oncore_cmd_Af));	/* ht (6/8) */
		}

		/* Finally, turn on position hold */

		if (instance->chan == 12)
			oncore_sendmsg(instance->ttyfd, oncore_cmd_Gd1,  sizeof(oncore_cmd_Gd1));
		else
			oncore_sendmsg(instance->ttyfd, oncore_cmd_At1,  sizeof(oncore_cmd_At1));
	}
}



static void
oncore_set_traim(
	struct instance *instance
	)
{
	char	Msg[160];

	if (instance->traim_in != -1)	/* set in Input */
		instance->traim = instance->traim_in;
	else
		instance->traim = instance->traim_ck;

	sprintf(Msg, "Input   says TRAIM = %d", instance->traim_in);
	record_clock_stats(&(instance->peer->srcadr), Msg);
	sprintf(Msg, "Model # says TRAIM = %d", instance->traim_id);
	record_clock_stats(&(instance->peer->srcadr), Msg);
	sprintf(Msg, "Testing says TRAIM = %d", instance->traim_ck);
	record_clock_stats(&(instance->peer->srcadr), Msg);
	sprintf(Msg, "Using        TRAIM = %d", instance->traim);
	record_clock_stats(&(instance->peer->srcadr), Msg);

	if (instance->traim_ck == 1 && instance->traim == 0) {
		/* if it should be off, and I turned it on during testing,
		   then turn it off again */
		if (instance->chan == 6)
			oncore_sendmsg(instance->ttyfd, oncore_cmd_Bnx, sizeof(oncore_cmd_Bnx));
		else if (instance->chan == 8)
			oncore_sendmsg(instance->ttyfd, oncore_cmd_Enx, sizeof(oncore_cmd_Enx));
		else	/* chan == 12 */
			oncore_sendmsg(instance->ttyfd, oncore_cmd_Ge0, sizeof(oncore_cmd_Ge0));
	}
}



/*
 * if SHMEM active, every 15s, steal one 'tick' to get 2D or 3D posn.
 */

static void
oncore_shmem_get_3D(
	struct instance *instance
	)
{
	if (instance->pp->second%15 == 3) {	/* start the sequence */			/* by changing mode */
		instance->shmem_reset = 1;
		if (instance->chan == 12) {
			if (instance->shmem_Posn == 2)
				oncore_sendmsg(instance->ttyfd, oncore_cmd_Gd2,  sizeof(oncore_cmd_Gd2));  /* 2D */
			else
				oncore_sendmsg(instance->ttyfd, oncore_cmd_Gd0,  sizeof(oncore_cmd_Gd0));  /* 3D */
		} else {
			if (instance->saw_At) { 		/* out of 0D -> 3D mode */
				oncore_sendmsg(instance->ttyfd, oncore_cmd_At0, sizeof(oncore_cmd_At0));
				if (instance->shmem_Posn == 2)	/* 3D -> 2D mode */
					oncore_sendmsg(instance->ttyfd, oncore_cmd_Av1, sizeof(oncore_cmd_Av1));
			} else
				oncore_sendmsg(instance->ttyfd, oncore_cmd_Av0, sizeof(oncore_cmd_Av0));
		}
	} else if (instance->shmem_reset || (instance->mode != MODE_0D)) {
		instance->shmem_reset = 0;
		if (instance->chan == 12)
			oncore_sendmsg(instance->ttyfd, oncore_cmd_Gd1,  sizeof(oncore_cmd_Gd1));	/* 0D */
		else {
			if (instance->saw_At) {
				if (instance->mode == MODE_2D)	/* 2D -> 3D or 0D mode */
					oncore_sendmsg(instance->ttyfd, oncore_cmd_Av0, sizeof(oncore_cmd_Av0));
				oncore_sendmsg(instance->ttyfd, oncore_cmd_At1,  sizeof(oncore_cmd_At1)); /* to 0D mode */
			} else
				oncore_sendmsg(instance->ttyfd, oncore_cmd_Av1,  sizeof(oncore_cmd_Av1));
		}
	}
}



/*
 * Here we do the Software SiteSurvey.
 * We have to average our own position for the Position Hold Mode
 *   We use Heights from the GPS ellipsoid.
 * We check for the END of either HW or SW SiteSurvey.
 */

static void
oncore_ss(
	struct instance *instance
	)
{
	char	*cp, Msg[160];
	double	lat, lon, ht;


	if (instance->site_survey == ONCORE_SS_HW) {

		/*
		 * Check to see if Hardware SiteSurvey has Finished.
		 */

		if ((instance->chan == 8  && !(instance->BEHa[37]  & 0x20)) ||
		    (instance->chan == 12 && !(instance->BEHa[130] & 0x10))) {
			record_clock_stats(&(instance->peer->srcadr), "Now in 0D mode");

			if (instance->chan == 12)
				oncore_sendmsg(instance->ttyfd, oncore_cmd_Gax, sizeof(oncore_cmd_Gax));
			else
				oncore_sendmsg(instance->ttyfd, oncore_cmd_Asx, sizeof(oncore_cmd_Asx));

			cp = "SSstate = ONCORE_SS_DONE";
			record_clock_stats(&(instance->peer->srcadr), cp);
			instance->site_survey = ONCORE_SS_DONE;
		}
	} else {
		/*
		 * Must be a Software Site Survey.
		 */

		if (instance->rsm.bad_fix)	/* Not if poor geometry or less than 3 sats */
			return;

		if (instance->mode != MODE_3D)	/* Use only 3D Fixes */
			return;

		instance->ss_lat  += buf_w32(&instance->BEHa[15]);
		instance->ss_long += buf_w32(&instance->BEHa[19]);
		instance->ss_ht   += buf_w32(&instance->BEHa[23]);  /* GPS ellipsoid */
		instance->ss_count++;

		if (instance->ss_count != POS_HOLD_AVERAGE)
			return;

		instance->ss_lat  /= POS_HOLD_AVERAGE;
		instance->ss_long /= POS_HOLD_AVERAGE;
		instance->ss_ht   /= POS_HOLD_AVERAGE;

		sprintf(Msg, "Surveyed posn: lat %.3f (mas) long %.3f (mas) ht %.3f (cm)",
			instance->ss_lat, instance->ss_long, instance->ss_ht);
		record_clock_stats(&(instance->peer->srcadr), Msg);
		lat = instance->ss_lat/3600000.;
		lon = instance->ss_long/3600000.;
		ht  = instance->ss_ht/100;
		sprintf(Msg, "Surveyed posn: lat %.7f (deg) long %.7f (deg) ht %.2f (m)",
			lat, lon, ht);
		record_clock_stats(&(instance->peer->srcadr), Msg);

		oncore_set_posn(instance);

		record_clock_stats(&(instance->peer->srcadr), "Now in 0D mode");

		cp = "SSstate = ONCORE_SS_DONE";
		record_clock_stats(&(instance->peer->srcadr), cp);
		instance->site_survey = ONCORE_SS_DONE;
	}
}



static int
oncore_wait_almanac(
	struct instance *instance
	)
{
	if (instance->rsm.bad_almanac) {
		if (debug)
			printf("ONCORE[%d]: waiting for almanac\n", instance->unit);

		/*
		 * If we get here (first time) then we don't have an almanac in memory.
		 * Check if we have a SHMEM, and if so try to load whatever is there.
		 */

		if (!instance->almanac_from_shmem) {
			instance->almanac_from_shmem = 1;
			oncore_load_almanac(instance);
		}
		return(1);
	} else {  /* Here we have the Almanac, we will be starting the @@Bn/@@En/@@Hn
		     commands, and can finally check for TRAIM.  Again, we set a delay
		     (5sec) and wait for things to settle down */

		if (instance->chan == 6)
			oncore_sendmsg(instance->ttyfd, oncore_cmd_Bn, sizeof(oncore_cmd_Bn));
		else if (instance->chan == 8)
			oncore_sendmsg(instance->ttyfd, oncore_cmd_En, sizeof(oncore_cmd_En));
		else if (instance->chan == 12) {
			oncore_sendmsg(instance->ttyfd, oncore_cmd_Gc, sizeof(oncore_cmd_Gc));	/* 1PPS on, continuous */
			oncore_sendmsg(instance->ttyfd, oncore_cmd_Ge, sizeof(oncore_cmd_Ge));	/* TRAIM on */
			oncore_sendmsg(instance->ttyfd, oncore_cmd_Hn, sizeof(oncore_cmd_Hn));	/* TRAIM status 1/s */
		}
		instance->traim_delay = 1;

		record_clock_stats(&(instance->peer->srcadr), "Have now loaded an ALMANAC");

		instance->o_state = ONCORE_RUN;
		record_clock_stats(&(instance->peer->srcadr), "state = ONCORE_RUN");
	}
	return(0);
}



#else
int refclock_oncore_bs;
#endif /* REFCLOCK */
