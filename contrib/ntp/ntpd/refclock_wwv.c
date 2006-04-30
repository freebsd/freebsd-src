/*
 * refclock_wwv - clock driver for NIST WWV/H time/frequency station
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#if defined(REFCLOCK) && defined(CLOCK_WWV)

#include "ntpd.h"
#include "ntp_io.h"
#include "ntp_refclock.h"
#include "ntp_calendar.h"
#include "ntp_stdlib.h"
#include "audio.h"

#include <stdio.h>
#include <ctype.h>
#include <math.h>
#ifdef HAVE_SYS_IOCTL_H
# include <sys/ioctl.h>
#endif /* HAVE_SYS_IOCTL_H */

#define ICOM 1

#ifdef ICOM
#include "icom.h"
#endif /* ICOM */

/*
 * Audio WWV/H demodulator/decoder
 *
 * This driver synchronizes the computer time using data encoded in
 * radio transmissions from NIST time/frequency stations WWV in Boulder,
 * CO, and WWVH in Kauai, HI. Transmissions are made continuously on
 * 2.5, 5, 10, 15 and 20 MHz in AM mode. An ordinary shortwave receiver
 * can be tuned manually to one of these frequencies or, in the case of
 * ICOM receivers, the receiver can be tuned automatically using this
 * program as propagation conditions change throughout the day and
 * night.
 *
 * The driver receives, demodulates and decodes the radio signals when
 * connected to the audio codec of a workstation running Solaris, SunOS
 * FreeBSD or Linux, and with a little help, other workstations with
 * similar codecs or sound cards. In this implementation, only one audio
 * driver and codec can be supported on a single machine.
 *
 * The demodulation and decoding algorithms used in this driver are
 * based on those developed for the TAPR DSP93 development board and the
 * TI 320C25 digital signal processor described in: Mills, D.L. A
 * precision radio clock for WWV transmissions. Electrical Engineering
 * Report 97-8-1, University of Delaware, August 1997, 25 pp., available
 * from www.eecis.udel.edu/~mills/reports.htm. The algorithms described
 * in this report have been modified somewhat to improve performance
 * under weak signal conditions and to provide an automatic station
 * identification feature.
 *
 * The ICOM code is normally compiled in the driver. It isn't used,
 * unless the mode keyword on the server configuration command specifies
 * a nonzero ICOM ID select code. The C-IV trace is turned on if the
 * debug level is greater than one.
 */
/*
 * Interface definitions
 */
#define	DEVICE_AUDIO	"/dev/audio" /* audio device name */
#define	AUDIO_BUFSIZ	320	/* audio buffer size (50 ms) */
#define	PRECISION	(-10)	/* precision assumed (about 1 ms) */
#define	DESCRIPTION	"WWV/H Audio Demodulator/Decoder" /* WRU */
#define SECOND		8000	/* second epoch (sample rate) (Hz) */
#define MINUTE		(SECOND * 60) /* minute epoch */
#define OFFSET		128	/* companded sample offset */
#define SIZE		256	/* decompanding table size */
#define	MAXSIG		6000.	/* max signal level reference */
#define	MAXCLP		100	/* max clips above reference per s */
#define MAXSNR		30.	/* max SNR reference */
#define DGAIN		20.	/* data channel gain reference */
#define SGAIN		10.	/* sync channel gain reference */
#define MAXFREQ		1.	/* max frequency tolerance (125 PPM) */
#define PI		3.1415926535 /* the real thing */
#define DATSIZ		(170 * MS) /* data matched filter size */
#define SYNSIZ		(800 * MS) /* minute sync matched filter size */
#define MAXERR		30	/* max data bit errors in minute */
#define NCHAN		5	/* number of radio channels */
#define	AUDIO_PHI	5e-6	/* dispersion growth factor */
#ifdef IRIG_SUCKS
#define	WIGGLE		11	/* wiggle filter length */
#endif /* IRIG_SUCKS */

/*
 * General purpose status bits (status)
 *
 * SELV and/or SELH are set when WWV or WWVH has been heard and cleared
 * on signal loss. SSYNC is set when the second sync pulse has been
 * acquired and cleared by signal loss. MSYNC is set when the minute
 * sync pulse has been acquired. DSYNC is set when a digit reaches the
 * threshold and INSYNC is set when all nine digits have reached the
 * threshold. The MSYNC, DSYNC and INSYNC bits are cleared only by
 * timeout, upon which the driver starts over from scratch.
 *
 * DGATE is set if a data bit is invalid and BGATE is set if a BCD digit
 * bit is invalid. SFLAG is set when during seconds 59, 0 and 1 while
 * probing alternate frequencies. LEPDAY is set when SECWAR of the
 * timecode is set on 30 June or 31 December. LEPSEC is set during the
 * last minute of the day when LEPDAY is set. At the end of this minute
 * the driver inserts second 60 in the seconds state machine and the
 * minute sync slips a second. The SLOSS and SJITR bits are for monitor
 * only.
 */
#define MSYNC		0x0001	/* minute epoch sync */
#define SSYNC		0x0002	/* second epoch sync */
#define DSYNC		0x0004	/* minute units sync */
#define INSYNC		0x0008	/* clock synchronized */
#define FGATE		0x0010	/* frequency gate */
#define DGATE		0x0020	/* data bit error */
#define BGATE		0x0040	/* BCD digit bit error */
#define SFLAG		0x1000	/* probe flag */
#define LEPDAY		0x2000	/* leap second day */
#define LEPSEC		0x4000	/* leap second minute */

/*
 * Station scoreboard bits
 *
 * These are used to establish the signal quality for each of the five
 * frequencies and two stations.
 */
#define SYNCNG		0x0001	/* sync or SNR below threshold */
#define DATANG		0x0002	/* data or SNR below threshold */
#define ERRRNG		0x0004	/* data error */
#define SELV		0x0100	/* WWV station select */
#define SELH		0x0200	/* WWVH station select */

/*
 * Alarm status bits (alarm)
 *
 * These bits indicate various alarm conditions, which are decoded to
 * form the quality character included in the timecode. If not tracking
 * second sync, the SYNERR alarm is raised. The data error counter is
 * incremented for each invalid data bit. If too many data bit errors
 * are encountered in one minute, the MODERR alarm is raised. The DECERR
 * alarm is raised if a maximum likelihood digit fails to compare with
 * the current clock digit. If the probability of any miscellaneous bit
 * or any digit falls below the threshold, the SYMERR alarm is raised.
 */
#define DECERR		1	/* BCD digit compare error */
#define SYMERR		2	/* low bit or digit probability */
#define MODERR		4	/* too many data bit errors */
#define SYNERR		8	/* not synchronized to station */

/*
 * Watchcat timeouts (watch)
 *
 * If these timeouts expire, the status bits are mashed to zero and the
 * driver starts from scratch. Suitably more refined procedures may be
 * developed in future. All these are in minutes.
 */
#define ACQSN		5	/* station acquisition timeout */
#define DIGIT		30	/* minute unit digit timeout */
#define HOLD		30	/* reachable timeout */
#define PANIC		(2 * 1440) /* panic timeout */

/*
 * Thresholds. These establish the minimum signal level, minimum SNR and
 * maximum jitter thresholds which establish the error and false alarm
 * rates of the driver. The values defined here may be on the
 * adventurous side in the interest of the highest sensitivity.
 */
#define MTHR		13.	/* acquisition signal gate (percent) */
#define TTHR		50.	/* tracking signal gate (percent) */
#define ATHR		2000.	/* acquisition amplitude threshold */
#define ASNR		6.	/* acquisition SNR threshold (dB) */
#define AWND		20.	/* acquisition jitter threshold (ms) */
#define AMIN		3	/* min bit count */
#define AMAX		6	/* max bit count */
#define QTHR		2000	/* QSY sync threshold */
#define QSNR		20.	/* QSY sync SNR threshold (dB) */
#define XTHR		1000.	/* QSY data threshold */
#define XSNR		10.	/* QSY data SNR threshold (dB) */
#define STHR		500	/* second sync amplitude threshold */
#define	SSNR		10.	/* second sync SNR threshold */
#define SCMP		10 	/* second sync compare threshold */
#define DTHR		1000	/* bit amplitude threshold */
#define DSNR		10.	/* bit SNR threshold (dB) */
#define BTHR		1000	/* digit amplitude threshold */
#define BSNR		3.	/* digit likelihood threshold (dB) */
#define BCMP		5	/* digit compare threshold */

/*
 * Tone frequency definitions. The increments are for 4.5-deg sine
 * table.
 */
#define MS		(SECOND / 1000) /* samples per millisecond */
#define IN100		((100 * 80) / SECOND) /* 100 Hz increment */
#define IN1000		((1000 * 80) / SECOND) /* 1000 Hz increment */
#define IN1200		((1200 * 80) / SECOND) /* 1200 Hz increment */

/*
 * Acquisition and tracking time constants. Usually powers of 2.
 */
#define MINAVG		8	/* min time constant */
#define MAXAVG		1024	/* max time constant */
#define TCONST		16	/* data bit/digit time constant */

/*
 * Miscellaneous status bits (misc)
 *
 * These bits correspond to designated bits in the WWV/H timecode. The
 * bit probabilities are exponentially averaged over several minutes and
 * processed by a integrator and threshold.
 */
#define DUT1		0x01	/* 56 DUT .1 */
#define DUT2		0x02	/* 57 DUT .2 */
#define DUT4		0x04	/* 58 DUT .4 */
#define DUTS		0x08	/* 50 DUT sign */
#define DST1		0x10	/* 55 DST1 leap warning */
#define DST2		0x20	/* 2 DST2 DST1 delayed one day */
#define SECWAR		0x40	/* 3 leap second warning */

/*
 * The on-time synchronization point for the driver is the second epoch
 * sync pulse produced by the FIR matched filters. As the 5-ms delay of
 * these filters is compensated, the program delay is 1.1 ms due to the
 * 600-Hz IIR bandpass filter. The measured receiver delay is 4.7 ms and
 * the codec delay less than 0.2 ms. The additional propagation delay
 * specific to each receiver location can be programmed in the fudge
 * time1 and time2 values for WWV and WWVH, respectively.
 */
#define PDELAY	(.0011 + .0047 + .0002)	/* net system delay (s) */

/*
 * Table of sine values at 4.5-degree increments. This is used by the
 * synchronous matched filter demodulators. The integral of sine-squared
 * over one complete cycle is PI, so the table is normallized by 1 / PI.
 */
double sintab[] = {
 0.000000e+00,  2.497431e-02,  4.979464e-02,  7.430797e-02, /* 0-3 */
 9.836316e-02,  1.218119e-01,  1.445097e-01,  1.663165e-01, /* 4-7 */
 1.870979e-01,  2.067257e-01,  2.250791e-01,  2.420447e-01, /* 8-11 */
 2.575181e-01,  2.714038e-01,  2.836162e-01,  2.940800e-01, /* 12-15 */
 3.027307e-01,  3.095150e-01,  3.143910e-01,  3.173286e-01, /* 16-19 */
 3.183099e-01,  3.173286e-01,  3.143910e-01,  3.095150e-01, /* 20-23 */
 3.027307e-01,  2.940800e-01,  2.836162e-01,  2.714038e-01, /* 24-27 */
 2.575181e-01,  2.420447e-01,  2.250791e-01,  2.067257e-01, /* 28-31 */
 1.870979e-01,  1.663165e-01,  1.445097e-01,  1.218119e-01, /* 32-35 */
 9.836316e-02,  7.430797e-02,  4.979464e-02,  2.497431e-02, /* 36-39 */
-0.000000e+00, -2.497431e-02, -4.979464e-02, -7.430797e-02, /* 40-43 */
-9.836316e-02, -1.218119e-01, -1.445097e-01, -1.663165e-01, /* 44-47 */
-1.870979e-01, -2.067257e-01, -2.250791e-01, -2.420447e-01, /* 48-51 */
-2.575181e-01, -2.714038e-01, -2.836162e-01, -2.940800e-01, /* 52-55 */
-3.027307e-01, -3.095150e-01, -3.143910e-01, -3.173286e-01, /* 56-59 */
-3.183099e-01, -3.173286e-01, -3.143910e-01, -3.095150e-01, /* 60-63 */
-3.027307e-01, -2.940800e-01, -2.836162e-01, -2.714038e-01, /* 64-67 */
-2.575181e-01, -2.420447e-01, -2.250791e-01, -2.067257e-01, /* 68-71 */
-1.870979e-01, -1.663165e-01, -1.445097e-01, -1.218119e-01, /* 72-75 */
-9.836316e-02, -7.430797e-02, -4.979464e-02, -2.497431e-02, /* 76-79 */
 0.000000e+00};						    /* 80 */

/*
 * Decoder operations at the end of each second are driven by a state
 * machine. The transition matrix consists of a dispatch table indexed
 * by second number. Each entry in the table contains a case switch
 * number and argument.
 */
struct progx {
	int sw;			/* case switch number */
	int arg;		/* argument */
};

/*
 * Case switch numbers
 */
#define IDLE		0	/* no operation */
#define COEF		1	/* BCD bit */
#define COEF2		2	/* BCD bit ignored */
#define DECIM9		3	/* BCD digit 0-9 */
#define DECIM6		4	/* BCD digit 0-6 */
#define DECIM3		5	/* BCD digit 0-3 */
#define DECIM2		6	/* BCD digit 0-2 */
#define MSCBIT		7	/* miscellaneous bit */
#define MSC20		8	/* miscellaneous bit */		
#define MSC21		9	/* QSY probe channel */		
#define MIN1		10	/* minute */		
#define MIN2		11	/* leap second */
#define SYNC2		12	/* QSY data channel */		
#define SYNC3		13	/* QSY data channel */		

/*
 * Offsets in decoding matrix
 */
#define MN		0	/* minute digits (2) */
#define HR		2	/* hour digits (2) */
#define DA		4	/* day digits (3) */
#define YR		7	/* year digits (2) */

struct progx progx[] = {
	{SYNC2,	0},		/* 0 latch sync max */
	{SYNC3,	0},		/* 1 QSY data channel */
	{MSCBIT, DST2},		/* 2 dst2 */
	{MSCBIT, SECWAR},	/* 3 lw */
	{COEF,	0},		/* 4 1 year units */
	{COEF,	1},		/* 5 2 */
	{COEF,	2},		/* 6 4 */
	{COEF,	3},		/* 7 8 */
	{DECIM9, YR},		/* 8 */
	{IDLE,	0},		/* 9 p1 */
	{COEF, 0},		/* 10 1 minute units */
	{COEF,	1},		/* 11 2 */
	{COEF,	2},		/* 12 4 */
	{COEF,	3},		/* 13 8 */
	{DECIM9, MN},		/* 14 */
	{COEF,	0},		/* 15 10 minute tens */
	{COEF,	1},		/* 16 20 */
	{COEF,	2},		/* 17 40 */
	{COEF2,	3},		/* 18 80 (not used) */
	{DECIM6, MN + 1},	/* 19 p2 */
	{COEF,	0},		/* 20 1 hour units */
	{COEF,	1},		/* 21 2 */
	{COEF,	2},		/* 22 4 */
	{COEF,	3},		/* 23 8 */
	{DECIM9, HR},		/* 24 */
	{COEF,	0},		/* 25 10 hour tens */
	{COEF,	1},		/* 26 20 */
	{COEF2,	2},		/* 27 40 (not used) */
	{COEF2,	3},		/* 28 80 (not used) */
	{DECIM2, HR + 1},	/* 29 p3 */
	{COEF,	0},		/* 30 1 day units */
	{COEF,	1},		/* 31 2 */
	{COEF,	2},		/* 32 4 */
	{COEF,	3},		/* 33 8 */
	{DECIM9, DA},		/* 34 */
	{COEF,	0},		/* 35 10 day tens */
	{COEF,	1},		/* 36 20 */
	{COEF,	2},		/* 37 40 */
	{COEF,	3},		/* 38 80 */
	{DECIM9, DA + 1},	/* 39 p4 */
	{COEF,	0},		/* 40 100 day hundreds */
	{COEF,	1},		/* 41 200 */
	{COEF2,	2},		/* 42 400 (not used) */
	{COEF2,	3},		/* 43 800 (not used) */
	{DECIM3, DA + 2},	/* 44 */
	{IDLE,	0},		/* 45 */
	{IDLE,	0},		/* 46 */
	{IDLE,	0},		/* 47 */
	{IDLE,	0},		/* 48 */
	{IDLE,	0},		/* 49 p5 */
	{MSCBIT, DUTS},		/* 50 dut+- */
	{COEF,	0},		/* 51 10 year tens */
	{COEF,	1},		/* 52 20 */
	{COEF,	2},		/* 53 40 */
	{COEF,	3},		/* 54 80 */
	{MSC20, DST1},		/* 55 dst1 */
	{MSCBIT, DUT1},		/* 56 0.1 dut */
	{MSCBIT, DUT2},		/* 57 0.2 */
	{MSC21, DUT4},		/* 58 0.4 QSY probe channel */
	{MIN1,	0},		/* 59 p6 latch sync min */
	{MIN2,	0}		/* 60 leap second */
};

/*
 * BCD coefficients for maximum likelihood digit decode
 */
#define P15	1.		/* max positive number */
#define N15	-1.		/* max negative number */

/*
 * Digits 0-9
 */
#define P9	(P15 / 4)	/* mark (+1) */
#define N9	(N15 / 4)	/* space (-1) */

double bcd9[][4] = {
	{N9, N9, N9, N9}, 	/* 0 */
	{P9, N9, N9, N9}, 	/* 1 */
	{N9, P9, N9, N9}, 	/* 2 */
	{P9, P9, N9, N9}, 	/* 3 */
	{N9, N9, P9, N9}, 	/* 4 */
	{P9, N9, P9, N9}, 	/* 5 */
	{N9, P9, P9, N9}, 	/* 6 */
	{P9, P9, P9, N9}, 	/* 7 */
	{N9, N9, N9, P9}, 	/* 8 */
	{P9, N9, N9, P9}, 	/* 9 */
	{0, 0, 0, 0}		/* backstop */
};

/*
 * Digits 0-6 (minute tens)
 */
#define P6	(P15 / 3)	/* mark (+1) */
#define N6	(N15 / 3)	/* space (-1) */

double bcd6[][4] = {
	{N6, N6, N6, 0}, 	/* 0 */
	{P6, N6, N6, 0}, 	/* 1 */
	{N6, P6, N6, 0}, 	/* 2 */
	{P6, P6, N6, 0}, 	/* 3 */
	{N6, N6, P6, 0}, 	/* 4 */
	{P6, N6, P6, 0}, 	/* 5 */
	{N6, P6, P6, 0}, 	/* 6 */
	{0, 0, 0, 0}		/* backstop */
};

/*
 * Digits 0-3 (day hundreds)
 */
#define P3	(P15 / 2)	/* mark (+1) */
#define N3	(N15 / 2)	/* space (-1) */

double bcd3[][4] = {
	{N3, N3, 0, 0}, 	/* 0 */
	{P3, N3, 0, 0}, 	/* 1 */
	{N3, P3, 0, 0}, 	/* 2 */
	{P3, P3, 0, 0}, 	/* 3 */
	{0, 0, 0, 0}		/* backstop */
};

/*
 * Digits 0-2 (hour tens)
 */
#define P2	(P15 / 2)	/* mark (+1) */
#define N2	(N15 / 2)	/* space (-1) */

double bcd2[][4] = {
	{N2, N2, 0, 0}, 	/* 0 */
	{P2, N2, 0, 0}, 	/* 1 */
	{N2, P2, 0, 0}, 	/* 2 */
	{0, 0, 0, 0}		/* backstop */
};

/*
 * DST decode (DST2 DST1) for prettyprint
 */
char dstcod[] = {
	'S',			/* 00 standard time */
	'I',			/* 01 set clock ahead at 0200 local */
	'O',			/* 10 set clock back at 0200 local */
	'D'			/* 11 daylight time */
};

/*
 * The decoding matrix consists of nine row vectors, one for each digit
 * of the timecode. The digits are stored from least to most significant
 * order. The maximum likelihood timecode is formed from the digits
 * corresponding to the maximum likelihood values reading in the
 * opposite order: yy ddd hh:mm.
 */
struct decvec {
	int radix;		/* radix (3, 4, 6, 10) */
	int digit;		/* current clock digit */
	int mldigit;		/* maximum likelihood digit */
	int phase;		/* maximum likelihood digit phase */
	int count;		/* match count */
	double digprb;		/* max digit probability */
	double digsnr;		/* likelihood function (dB) */
	double like[10];	/* likelihood integrator 0-9 */
};

/*
 * The station structure is used to acquire the minute pulse from WWV
 * and/or WWVH. These stations are distinguished by the frequency used
 * for the second and minute sync pulses, 1000 Hz for WWV and 1200 Hz
 * for WWVH. Other than frequency, the format is the same.
 */
struct sync {
	double	epoch;		/* accumulated epoch differences */
	double	maxamp;		/* sync max envelope (square) */
	double	noiamp;		/* sync noise envelope (square) */
	long	pos;		/* max amplitude position */
	long	lastpos;	/* last max position */
	long	mepoch;		/* minute synch epoch */

	double	amp;		/* sync amplitude (I, Q squares) */
	double	synamp;		/* sync max envelope at 800 ms */
	double	synmax;		/* sync envelope at 0 s */
	double	synmin;		/* sync envelope at 59, 1 s */
	double	synsnr;		/* sync signal SNR */
	int	count;		/* bit counter */
	char	refid[5];	/* reference identifier */
	int	select;		/* select bits */
	int	reach;		/* reachability register */
};

/*
 * The channel structure is used to mitigate between channels.
 */
struct chan {
	int	gain;		/* audio gain */
	double	sigamp;		/* data max envelope (square) */
	double	noiamp;		/* data noise envelope (square) */
	double	datsnr;		/* data signal SNR */
	struct sync wwv;	/* wwv station */
	struct sync wwvh;	/* wwvh station */
};

/*
 * WWV unit control structure
 */
struct wwvunit {
	l_fp	timestamp;	/* audio sample timestamp */
	l_fp	tick;		/* audio sample increment */
	double	phase, freq;	/* logical clock phase and frequency */
	double	monitor;	/* audio monitor point */
	int	fd_icom;	/* ICOM file descriptor */
	int	errflg;		/* error flags */
	int	watch;		/* watchcat */

	/*
	 * Audio codec variables
	 */
	double	comp[SIZE];	/* decompanding table */
	int	port;		/* codec port */
	int	gain;		/* codec gain */
	int	mongain;	/* codec monitor gain */
	int	clipcnt;	/* sample clipped count */
#ifdef IRIG_SUCKS
	l_fp	wigwag;		/* wiggle accumulator */
	int	wp;		/* wiggle filter pointer */
	l_fp	wiggle[WIGGLE];	/* wiggle filter */
	l_fp	wigbot[WIGGLE];	/* wiggle bottom fisher*/
#endif /* IRIG_SUCKS */

	/*
	 * Variables used to establish basic system timing
	 */
	int	avgint;		/* master time constant */
	int	tepoch;		/* sync epoch median */
	int	yepoch;		/* sync epoch */
	int	repoch;		/* buffered sync epoch */
	double	epomax;		/* second sync amplitude */
	double	eposnr;		/* second sync SNR */
	double	irig;		/* data I channel amplitude */
	double	qrig;		/* data Q channel amplitude */
	int	datapt;		/* 100 Hz ramp */
	double	datpha;		/* 100 Hz VFO control */
	int	rphase;		/* second sample counter */
	long	mphase;		/* minute sample counter */

	/*
	 * Variables used to mitigate which channel to use
	 */
	struct chan mitig[NCHAN]; /* channel data */
	struct sync *sptr;	/* station pointer */
	int	dchan;		/* data channel */
	int	schan;		/* probe channel */
	int	achan;		/* active channel */

	/*
	 * Variables used by the clock state machine
	 */
	struct decvec decvec[9]; /* decoding matrix */
	int	rsec;		/* seconds counter */
	int	digcnt;		/* count of digits synchronized */

	/*
	 * Variables used to estimate signal levels and bit/digit
	 * probabilities
	 */
	double	sigsig;		/* data max signal */
	double	sigamp;		/* data max envelope (square) */
	double	noiamp;		/* data noise envelope (square) */
	double	datsnr;		/* data SNR (dB) */

	/*
	 * Variables used to establish status and alarm conditions
	 */
	int	status;		/* status bits */
	int	alarm;		/* alarm flashers */
	int	misc;		/* miscellaneous timecode bits */
	int	errcnt;		/* data bit error counter */
	int	errbit;		/* data bit errors in minute */
};

/*
 * Function prototypes
 */
static	int	wwv_start	P((int, struct peer *));
static	void	wwv_shutdown	P((int, struct peer *));
static	void	wwv_receive	P((struct recvbuf *));
static	void	wwv_poll	P((int, struct peer *));

/*
 * More function prototypes
 */
static	void	wwv_epoch	P((struct peer *));
static	void	wwv_rf		P((struct peer *, double));
static	void	wwv_endpoc	P((struct peer *, int));
static	void	wwv_rsec	P((struct peer *, double));
static	void	wwv_qrz		P((struct peer *, struct sync *,
				    double, int));
static	void	wwv_corr4	P((struct peer *, struct decvec *,
				    double [], double [][4]));
static	void	wwv_gain	P((struct peer *));
static	void	wwv_tsec	P((struct wwvunit *));
static	double	wwv_data	P((struct wwvunit *, double));
static	int	timecode	P((struct wwvunit *, char *));
static	double	wwv_snr		P((double, double));
static	int	carry		P((struct decvec *));
static	void	wwv_newchan	P((struct peer *));
static	void	wwv_newgame	P((struct peer *));
static	double	wwv_metric	P((struct sync *));
#ifdef ICOM
static	int	wwv_qsy		P((struct peer *, int));
#endif /* ICOM */

static double qsy[NCHAN] = {2.5, 5, 10, 15, 20}; /* frequencies (MHz) */

/*
 * Transfer vector
 */
struct	refclock refclock_wwv = {
	wwv_start,		/* start up driver */
	wwv_shutdown,		/* shut down driver */
	wwv_poll,		/* transmit poll message */
	noentry,		/* not used (old wwv_control) */
	noentry,		/* initialize driver (not used) */
	noentry,		/* not used (old wwv_buginfo) */
	NOFLAGS			/* not used */
};


/*
 * wwv_start - open the devices and initialize data for processing
 */
static int
wwv_start(
	int	unit,		/* instance number (used by PCM) */
	struct peer *peer	/* peer structure pointer */
	)
{
	struct refclockproc *pp;
	struct wwvunit *up;
#ifdef ICOM
	int	temp;
#endif /* ICOM */

	/*
	 * Local variables
	 */
	int	fd;		/* file descriptor */
	int	i;		/* index */
	double	step;		/* codec adjustment */

	/*
	 * Open audio device
	 */
	fd = audio_init(DEVICE_AUDIO, AUDIO_BUFSIZ, unit);
	if (fd < 0)
		return (0);
#ifdef DEBUG
	if (debug)
		audio_show();
#endif

	/*
	 * Allocate and initialize unit structure
	 */
	if (!(up = (struct wwvunit *)emalloc(sizeof(struct wwvunit)))) {
		close(fd);
		return (0);
	}
	memset(up, 0, sizeof(struct wwvunit));
	pp = peer->procptr;
	pp->unitptr = (caddr_t)up;
	pp->io.clock_recv = wwv_receive;
	pp->io.srcclock = (caddr_t)peer;
	pp->io.datalen = 0;
	pp->io.fd = fd;
	if (!io_addclock(&pp->io)) {
		close(fd);
		free(up);
		return (0);
	}

	/*
	 * Initialize miscellaneous variables
	 */
	peer->precision = PRECISION;
	pp->clockdesc = DESCRIPTION;

	/*
	 * The companded samples are encoded sign-magnitude. The table
	 * contains all the 256 values in the interest of speed.
	 */
	up->comp[0] = up->comp[OFFSET] = 0.;
	up->comp[1] = 1; up->comp[OFFSET + 1] = -1.;
	up->comp[2] = 3; up->comp[OFFSET + 2] = -3.;
	step = 2.;
	for (i = 3; i < OFFSET; i++) {
		up->comp[i] = up->comp[i - 1] + step;
		up->comp[OFFSET + i] = -up->comp[i];
                if (i % 16 == 0)
		    step *= 2.;
	}
	DTOLFP(1. / SECOND, &up->tick);

	/*
	 * Initialize the decoding matrix with the radix for each digit
	 * position.
	 */
	up->decvec[MN].radix = 10;	/* minutes */
	up->decvec[MN + 1].radix = 6;
	up->decvec[HR].radix = 10;	/* hours */
	up->decvec[HR + 1].radix = 3;
	up->decvec[DA].radix = 10;	/* days */
	up->decvec[DA + 1].radix = 10;
	up->decvec[DA + 2].radix = 4;
	up->decvec[YR].radix = 10;	/* years */
	up->decvec[YR + 1].radix = 10;
	wwv_newgame(peer);
	up->schan = up->achan = 3;

	/*
	 * Initialize autotune if available. Start out at 15 MHz. Note
	 * that the ICOM select code must be less than 128, so the high
	 * order bit can be used to select the line speed.
	 */
#ifdef ICOM
	temp = 0;
#ifdef DEBUG
	if (debug > 1)
		temp = P_TRACE;
#endif
	if (peer->ttl != 0) {
		if (peer->ttl & 0x80)
			up->fd_icom = icom_init("/dev/icom", B1200,
			    temp);
		else
			up->fd_icom = icom_init("/dev/icom", B9600,
			    temp);
	}
	if (up->fd_icom > 0) {
		if ((temp = wwv_qsy(peer, up->schan)) != 0) {
			NLOG(NLOG_SYNCEVENT | NLOG_SYSEVENT)
			    msyslog(LOG_NOTICE,
			    "icom: radio not found");
			up->errflg = CEVNT_FAULT;
			close(up->fd_icom);
			up->fd_icom = 0;
		} else {
			NLOG(NLOG_SYNCEVENT | NLOG_SYSEVENT)
			    msyslog(LOG_NOTICE,
			    "icom: autotune enabled");
		}
	}
#endif /* ICOM */
	return (1);
}


/*
 * wwv_shutdown - shut down the clock
 */
static void
wwv_shutdown(
	int	unit,		/* instance number (not used) */
	struct peer *peer	/* peer structure pointer */
	)
{
	struct refclockproc *pp;
	struct wwvunit *up;

	pp = peer->procptr;
	up = (struct wwvunit *)pp->unitptr;
	io_closeclock(&pp->io);
	if (up->fd_icom > 0)
		close(up->fd_icom);
	free(up);
}


/*
 * wwv_receive - receive data from the audio device
 *
 * This routine reads input samples and adjusts the logical clock to
 * track the A/D sample clock by dropping or duplicating codec samples.
 * It also controls the A/D signal level with an AGC loop to mimimize
 * quantization noise and avoid overload.
 */
static void
wwv_receive(
	struct recvbuf *rbufp	/* receive buffer structure pointer */
	)
{
	struct peer *peer;
	struct refclockproc *pp;
	struct wwvunit *up;

	/*
	 * Local variables
	 */
	double	sample;		/* codec sample */
	u_char	*dpt;		/* buffer pointer */
	int	bufcnt;		/* buffer counter */
	l_fp	ltemp;

	peer = (struct peer *)rbufp->recv_srcclock;
	pp = peer->procptr;
	up = (struct wwvunit *)pp->unitptr;

	/*
	 * Main loop - read until there ain't no more. Note codec
	 * samples are bit-inverted.
	 */
	DTOLFP((double)rbufp->recv_length / SECOND, &ltemp);
	L_SUB(&rbufp->recv_time, &ltemp);
	up->timestamp = rbufp->recv_time;
	dpt = rbufp->recv_buffer;
	for (bufcnt = 0; bufcnt < rbufp->recv_length; bufcnt++) {
		sample = up->comp[~*dpt++ & 0xff];

		/*
		 * Clip noise spikes greater than MAXSIG. If no clips,
		 * increase the gain a tad; if the clips are too high, 
		 * decrease a tad.
		 */
		if (sample > MAXSIG) {
			sample = MAXSIG;
			up->clipcnt++;
		} else if (sample < -MAXSIG) {
			sample = -MAXSIG;
			up->clipcnt++;
		}

		/*
		 * Variable frequency oscillator. The codec oscillator
		 * runs at the nominal rate of 8000 samples per second,
		 * or 125 us per sample. A frequency change of one unit
		 * results in either duplicating or deleting one sample
		 * per second, which results in a frequency change of
		 * 125 PPM.
		 */
		up->phase += up->freq / SECOND;
		if (up->phase >= .5) {
			up->phase -= 1.;
		} else if (up->phase < -.5) {
			up->phase += 1.;
			wwv_rf(peer, sample);
			wwv_rf(peer, sample);
		} else {
			wwv_rf(peer, sample);
		}
		L_ADD(&up->timestamp, &up->tick);
	}

	/*
	 * Set the input port and monitor gain for the next buffer.
	 */
	if (pp->sloppyclockflag & CLK_FLAG2)
		up->port = 2;
	else
		up->port = 1;
	if (pp->sloppyclockflag & CLK_FLAG3)
		up->mongain = MONGAIN;
	else
		up->mongain = 0;
}


/*
 * wwv_poll - called by the transmit procedure
 *
 * This routine keeps track of status. If no offset samples have been
 * processed during a poll interval, a timeout event is declared. If
 * errors have have occurred during the interval, they are reported as
 * well. Once the clock is set, it always appears reachable, unless
 * reset by watchdog timeout.
 */
static void
wwv_poll(
	int	unit,		/* instance number (not used) */
	struct peer *peer	/* peer structure pointer */
	)
{
	struct refclockproc *pp;
	struct wwvunit *up;

	pp = peer->procptr;
	up = (struct wwvunit *)pp->unitptr;
	if (pp->coderecv == pp->codeproc)
		up->errflg = CEVNT_TIMEOUT;
	if (up->errflg)
		refclock_report(peer, up->errflg);
	up->errflg = 0;
	pp->polls++;
}


/*
 * wwv_rf - process signals and demodulate to baseband
 *
 * This routine grooms and filters decompanded raw audio samples. The
 * output signals include the 100-Hz baseband data signal in quadrature
 * form, plus the epoch index of the second sync signal and the second
 * index of the minute sync signal.
 *
 * There are two 1-s ramps used by this program. Both count the 8000
 * logical clock samples spanning exactly one second. The epoch ramp
 * counts the samples starting at an arbitrary time. The rphase ramp
 * counts the samples starting at the 5-ms second sync pulse found
 * during the epoch ramp.
 *
 * There are two 1-m ramps used by this program. The mphase ramp counts
 * the 480,000 logical clock samples spanning exactly one minute and
 * starting at an arbitrary time. The rsec ramp counts the 60 seconds of
 * the minute starting at the 800-ms minute sync pulse found during the
 * mphase ramp. The rsec ramp drives the seconds state machine to
 * determine the bits and digits of the timecode. 
 *
 * Demodulation operations are based on three synthesized quadrature
 * sinusoids: 100 Hz for the data signal, 1000 Hz for the WWV sync
 * signal and 1200 Hz for the WWVH sync signal. These drive synchronous
 * matched filters for the data signal (170 ms at 100 Hz), WWV minute
 * sync signal (800 ms at 1000 Hz) and WWVH minute sync signal (800 ms
 * at 1200 Hz). Two additional matched filters are switched in
 * as required for the WWV second sync signal (5 ms at 1000 Hz) and
 * WWVH second sync signal (5 ms at 1200 Hz).
 */
static void
wwv_rf(
	struct peer *peer,	/* peerstructure pointer */
	double isig		/* input signal */
	)
{
	struct refclockproc *pp;
	struct wwvunit *up;
	struct sync *sp;

	static double lpf[5];	/* 150-Hz lpf delay line */
	double data;		/* lpf output */
	static double bpf[9];	/* 1000/1200-Hz bpf delay line */
	double syncx;		/* bpf output */
	static double mf[41];	/* 1000/1200-Hz mf delay line */
	double mfsync;		/* mf output */

	static int iptr;	/* data channel pointer */
	static double ibuf[DATSIZ]; /* data I channel delay line */
	static double qbuf[DATSIZ]; /* data Q channel delay line */

	static int jptr;	/* sync channel pointer */
	static double cibuf[SYNSIZ]; /* wwv I channel delay line */
	static double cqbuf[SYNSIZ]; /* wwv Q channel delay line */
	static double ciamp;	/* wwv I channel amplitude */
	static double cqamp;	/* wwv Q channel amplitude */
	static int csinptr;	/* wwv channel phase */
	static double hibuf[SYNSIZ]; /* wwvh I channel delay line */
	static double hqbuf[SYNSIZ]; /* wwvh Q channel delay line */
	static double hiamp;	/* wwvh I channel amplitude */
	static double hqamp;	/* wwvh Q channel amplitude */
	static int hsinptr;	/* wwvh channels phase */

	static double epobuf[SECOND]; /* epoch sync comb filter */
	static double epomax;	/* epoch sync amplitude buffer */
	static int epopos;	/* epoch sync position buffer */

	static int iniflg;	/* initialization flag */
	int	epoch;		/* comb filter index */
	int	pdelay;		/* propagation delay (samples) */
	double	dtemp;
	int	i;

	pp = peer->procptr;
	up = (struct wwvunit *)pp->unitptr;

	if (!iniflg) {
		iniflg = 1;
		memset((char *)lpf, 0, sizeof(lpf));
		memset((char *)bpf, 0, sizeof(bpf));
		memset((char *)mf, 0, sizeof(mf));
		memset((char *)ibuf, 0, sizeof(ibuf));
		memset((char *)qbuf, 0, sizeof(qbuf));
		memset((char *)cibuf, 0, sizeof(cibuf));
		memset((char *)cqbuf, 0, sizeof(cqbuf));
		memset((char *)hibuf, 0, sizeof(hibuf));
		memset((char *)hqbuf, 0, sizeof(hqbuf));
		memset((char *)epobuf, 0, sizeof(epobuf));
	}

	/*
	 * Baseband data demodulation. The 100-Hz subcarrier is
	 * extracted using a 150-Hz IIR lowpass filter. This attenuates
	 * the 1000/1200-Hz sync signals, as well as the 440-Hz and
	 * 600-Hz tones and most of the noise and voice modulation
	 * components.
	 *
	 * Matlab IIR 4th-order IIR elliptic, 150 Hz lowpass, 0.2 dB
	 * passband ripple, -50 dB stopband ripple.
	 */
	data = (lpf[4] = lpf[3]) * 8.360961e-01;
	data += (lpf[3] = lpf[2]) * -3.481740e+00;
	data += (lpf[2] = lpf[1]) * 5.452988e+00;
	data += (lpf[1] = lpf[0]) * -3.807229e+00;
	lpf[0] = isig - data;
	data = lpf[0] * 3.281435e-03
	    + lpf[1] * -1.149947e-02
	    + lpf[2] * 1.654858e-02
	    + lpf[3] * -1.149947e-02
	    + lpf[4] * 3.281435e-03;

	/*
	 * The I and Q quadrature data signals are produced by
	 * multiplying the filtered signal by 100-Hz sine and cosine
	 * signals, respectively. The data signals are demodulated by
	 * 170-ms synchronous matched filters to produce the amplitude
	 * and phase signals used by the decoder.
	 */
	i = up->datapt;
	up->datapt = (up->datapt + IN100) % 80;
	dtemp = sintab[i] * data / DATSIZ * DGAIN;
	up->irig -= ibuf[iptr];
	ibuf[iptr] = dtemp;
	up->irig += dtemp;
	i = (i + 20) % 80;
	dtemp = sintab[i] * data / DATSIZ * DGAIN;
	up->qrig -= qbuf[iptr];
	qbuf[iptr] = dtemp;
	up->qrig += dtemp;
	iptr = (iptr + 1) % DATSIZ;

	/*
	 * Baseband sync demodulation. The 1000/1200 sync signals are
	 * extracted using a 600-Hz IIR bandpass filter. This removes
	 * the 100-Hz data subcarrier, as well as the 440-Hz and 600-Hz
	 * tones and most of the noise and voice modulation components.
	 *
	 * Matlab 4th-order IIR elliptic, 800-1400 Hz bandpass, 0.2 dB
	 * passband ripple, -50 dB stopband ripple.
	 */
	syncx = (bpf[8] = bpf[7]) * 4.897278e-01;
	syncx += (bpf[7] = bpf[6]) * -2.765914e+00;
	syncx += (bpf[6] = bpf[5]) * 8.110921e+00;
	syncx += (bpf[5] = bpf[4]) * -1.517732e+01;
	syncx += (bpf[4] = bpf[3]) * 1.975197e+01;
	syncx += (bpf[3] = bpf[2]) * -1.814365e+01;
	syncx += (bpf[2] = bpf[1]) * 1.159783e+01;
	syncx += (bpf[1] = bpf[0]) * -4.735040e+00;
	bpf[0] = isig - syncx;
	syncx = bpf[0] * 8.203628e-03
	    + bpf[1] * -2.375732e-02
	    + bpf[2] * 3.353214e-02
	    + bpf[3] * -4.080258e-02
	    + bpf[4] * 4.605479e-02
	    + bpf[5] * -4.080258e-02
	    + bpf[6] * 3.353214e-02
	    + bpf[7] * -2.375732e-02
	    + bpf[8] * 8.203628e-03;

	/*
	 * The I and Q quadrature minute sync signals are produced by
	 * multiplying the filtered signal by 1000-Hz (WWV) and 1200-Hz
	 * (WWVH) sine and cosine signals, respectively. The resulting
	 * signals are demodulated by 800-ms synchronous matched filters
	 * to synchronize the second and minute and to detect which one
	 * (or both) the WWV or WWVH signal is present.
	 *
	 * Note the master timing ramps, which run continuously. The
	 * minute counter (mphase) counts the samples in the minute,
	 * while the second counter (epoch) counts the samples in the
	 * second.
	 */
	up->mphase = (up->mphase + 1) % MINUTE;
	epoch = up->mphase % SECOND;
	i = csinptr;
	csinptr = (csinptr + IN1000) % 80;
	dtemp = sintab[i] * syncx / SYNSIZ * SGAIN;
	ciamp = ciamp - cibuf[jptr] + dtemp;
	cibuf[jptr] = dtemp;
	i = (i + 20) % 80;
	dtemp = sintab[i] * syncx / SYNSIZ * SGAIN;
	cqamp = cqamp - cqbuf[jptr] + dtemp;
	cqbuf[jptr] = dtemp;
	sp = &up->mitig[up->schan].wwv;
	dtemp = ciamp * ciamp + cqamp * cqamp;
	sp->amp = dtemp;
	if (!(up->status & MSYNC))
		wwv_qrz(peer, sp, dtemp, (int)(pp->fudgetime1 *
		    SECOND));
	i = hsinptr;
	hsinptr = (hsinptr + IN1200) % 80;
	dtemp = sintab[i] * syncx / SYNSIZ * SGAIN;
	hiamp = hiamp - hibuf[jptr] + dtemp;
	hibuf[jptr] = dtemp;
	i = (i + 20) % 80;
	dtemp = sintab[i] * syncx / SYNSIZ * SGAIN;
	hqamp = hqamp - hqbuf[jptr] + dtemp;
	hqbuf[jptr] = dtemp;
	sp = &up->mitig[up->schan].wwvh;
	dtemp = hiamp * hiamp + hqamp * hqamp;
	sp->amp = dtemp;
	if (!(up->status & MSYNC))
		wwv_qrz(peer, sp, dtemp, (int)(pp->fudgetime2 *
		    SECOND));
	jptr = (jptr + 1) % SYNSIZ;

	/*
	 * The following section is called once per minute. It does
	 * housekeeping and timeout functions and empties the dustbins.
	 */
	if (up->mphase == 0) {
		up->watch++;
		if (!(up->status & MSYNC)) {

			/*
			 * If minute sync has not been acquired before
			 * timeout, or if no signal is heard, the
			 * program cycles to the next frequency and
			 * tries again.
			 */
			wwv_newchan(peer);
			if (!(up->status & (SELV | SELH)) || up->watch >
			    ACQSN) {
				wwv_newgame(peer);
#ifdef ICOM
				if (up->fd_icom > 0) {
					up->schan = (up->schan + 1) %
					    NCHAN;
					wwv_qsy(peer, up->schan);
				}
#endif /* ICOM */
			}
		} else {

			/*
			 * If the leap bit is set, set the minute epoch
			 * back one second so the station processes
			 * don't miss a beat.
			 */
			if (up->status & LEPSEC) {
				up->mphase -= SECOND;
				if (up->mphase < 0)
					up->mphase += MINUTE;
			}
		}
	}

	/*
	 * When the channel metric reaches threshold and the second
	 * counter matches the minute epoch within the second, the
	 * driver has synchronized to the station. The second number is
	 * the remaining seconds until the next minute epoch, while the
	 * sync epoch is zero. Watch out for the first second; if
	 * already synchronized to the second, the buffered sync epoch
	 * must be set. 
	 */
	if (up->status & MSYNC) {
		wwv_epoch(peer);
	} else if ((sp = up->sptr) != NULL) {
		struct chan *cp;

		if (sp->count >= AMIN && epoch == sp->mepoch % SECOND) {
			up->rsec = 60 - sp->mepoch / SECOND;
			up->rphase = 0;
			up->status |= MSYNC;
			up->watch = 0;
			if (!(up->status & SSYNC))
				up->repoch = up->yepoch = epoch;
			else
				up->repoch = up->yepoch;
			for (i = 0; i < NCHAN; i++) {
				cp = &up->mitig[i];
				cp->wwv.count = cp->wwv.reach = 0;
				cp->wwvh.count = cp->wwvh.reach = 0;
			}
		}
	}

	/*
	 * The second sync pulse is extracted using 5-ms (40 sample) FIR
	 * matched filters at 1000 Hz for WWV or 1200 Hz for WWVH. This
	 * pulse is used for the most precise synchronization, since if
	 * provides a resolution of one sample (125 us). The filters run
	 * only if the station has been reliably determined.
	 */
	if (up->status & SELV) {
		pdelay = (int)(pp->fudgetime1 * SECOND);

		/*
		 * WWV FIR matched filter, five cycles of 1000-Hz
		 * sinewave.
		 */
		mf[40] = mf[39];
		mfsync = (mf[39] = mf[38]) * 4.224514e-02;
		mfsync += (mf[38] = mf[37]) * 5.974365e-02;
		mfsync += (mf[37] = mf[36]) * 4.224514e-02;
		mf[36] = mf[35];
		mfsync += (mf[35] = mf[34]) * -4.224514e-02;
		mfsync += (mf[34] = mf[33]) * -5.974365e-02;
		mfsync += (mf[33] = mf[32]) * -4.224514e-02;
		mf[32] = mf[31];
		mfsync += (mf[31] = mf[30]) * 4.224514e-02;
		mfsync += (mf[30] = mf[29]) * 5.974365e-02;
		mfsync += (mf[29] = mf[28]) * 4.224514e-02;
		mf[28] = mf[27];
		mfsync += (mf[27] = mf[26]) * -4.224514e-02;
		mfsync += (mf[26] = mf[25]) * -5.974365e-02;
		mfsync += (mf[25] = mf[24]) * -4.224514e-02;
		mf[24] = mf[23];
		mfsync += (mf[23] = mf[22]) * 4.224514e-02;
		mfsync += (mf[22] = mf[21]) * 5.974365e-02;
		mfsync += (mf[21] = mf[20]) * 4.224514e-02;
		mf[20] = mf[19];
		mfsync += (mf[19] = mf[18]) * -4.224514e-02;
		mfsync += (mf[18] = mf[17]) * -5.974365e-02;
		mfsync += (mf[17] = mf[16]) * -4.224514e-02;
		mf[16] = mf[15];
		mfsync += (mf[15] = mf[14]) * 4.224514e-02;
		mfsync += (mf[14] = mf[13]) * 5.974365e-02;
		mfsync += (mf[13] = mf[12]) * 4.224514e-02;
		mf[12] = mf[11];
		mfsync += (mf[11] = mf[10]) * -4.224514e-02;
		mfsync += (mf[10] = mf[9]) * -5.974365e-02;
		mfsync += (mf[9] = mf[8]) * -4.224514e-02;
		mf[8] = mf[7];
		mfsync += (mf[7] = mf[6]) * 4.224514e-02;
		mfsync += (mf[6] = mf[5]) * 5.974365e-02;
		mfsync += (mf[5] = mf[4]) * 4.224514e-02;
		mf[4] = mf[3];
		mfsync += (mf[3] = mf[2]) * -4.224514e-02;
		mfsync += (mf[2] = mf[1]) * -5.974365e-02;
		mfsync += (mf[1] = mf[0]) * -4.224514e-02;
		mf[0] = syncx;
	} else if (up->status & SELH) {
		pdelay = (int)(pp->fudgetime2 * SECOND);

		/*
		 * WWVH FIR matched filter, six cycles of 1200-Hz
		 * sinewave.
		 */
		mf[40] = mf[39];
		mfsync = (mf[39] = mf[38]) * 4.833363e-02;
		mfsync += (mf[38] = mf[37]) * 5.681959e-02;
		mfsync += (mf[37] = mf[36]) * 1.846180e-02;
		mfsync += (mf[36] = mf[35]) * -3.511644e-02;
		mfsync += (mf[35] = mf[34]) * -5.974365e-02;
		mfsync += (mf[34] = mf[33]) * -3.511644e-02;
		mfsync += (mf[33] = mf[32]) * 1.846180e-02;
		mfsync += (mf[32] = mf[31]) * 5.681959e-02;
		mfsync += (mf[31] = mf[30]) * 4.833363e-02;
		mf[30] = mf[29];
		mfsync += (mf[29] = mf[28]) * -4.833363e-02;
		mfsync += (mf[28] = mf[27]) * -5.681959e-02;
		mfsync += (mf[27] = mf[26]) * -1.846180e-02;
		mfsync += (mf[26] = mf[25]) * 3.511644e-02;
		mfsync += (mf[25] = mf[24]) * 5.974365e-02;
		mfsync += (mf[24] = mf[23]) * 3.511644e-02;
		mfsync += (mf[23] = mf[22]) * -1.846180e-02;
		mfsync += (mf[22] = mf[21]) * -5.681959e-02;
		mfsync += (mf[21] = mf[20]) * -4.833363e-02;
		mf[20] = mf[19];
		mfsync += (mf[19] = mf[18]) * 4.833363e-02;
		mfsync += (mf[18] = mf[17]) * 5.681959e-02;
		mfsync += (mf[17] = mf[16]) * 1.846180e-02;
		mfsync += (mf[16] = mf[15]) * -3.511644e-02;
		mfsync += (mf[15] = mf[14]) * -5.974365e-02;
		mfsync += (mf[14] = mf[13]) * -3.511644e-02;
		mfsync += (mf[13] = mf[12]) * 1.846180e-02;
		mfsync += (mf[12] = mf[11]) * 5.681959e-02;
		mfsync += (mf[11] = mf[10]) * 4.833363e-02;
		mf[10] = mf[9];
		mfsync += (mf[9] = mf[8]) * -4.833363e-02;
		mfsync += (mf[8] = mf[7]) * -5.681959e-02;
		mfsync += (mf[7] = mf[6]) * -1.846180e-02;
		mfsync += (mf[6] = mf[5]) * 3.511644e-02;
		mfsync += (mf[5] = mf[4]) * 5.974365e-02;
		mfsync += (mf[4] = mf[3]) * 3.511644e-02;
		mfsync += (mf[3] = mf[2]) * -1.846180e-02;
		mfsync += (mf[2] = mf[1]) * -5.681959e-02;
		mfsync += (mf[1] = mf[0]) * -4.833363e-02;
		mf[0] = syncx;
	} else {
		mfsync = 0;
		pdelay = 0;
	}

	/*
	 * Enhance the seconds sync pulse using a 1-s (8000-sample) comb
	 * filter. Correct for the FIR matched filter delay, which is 5
	 * ms for both the WWV and WWVH filters, and also for the
	 * propagation delay. Once each second look for second sync. If
	 * not in minute sync, fiddle the codec gain. Note the SNR is
	 * computed from the maximum sample and the two samples 6 ms
	 * before and 6 ms after it, so if we slip more than a cycle the
	 * SNR should plummet.
	 */
	dtemp = (epobuf[epoch] += (mfsync - epobuf[epoch]) /
	    up->avgint);
	if (dtemp > epomax) {
		epomax = dtemp;
		epopos = epoch;
	}
	if (epoch == 0) {
		int k, j;

		up->epomax = epomax;
		k = epopos - 6 * MS;
		if (k < 0)
			k += SECOND;
		j = epopos + 6 * MS;
		if (j >= SECOND)
			i -= SECOND;
		up->eposnr = wwv_snr(epomax, max(abs(epobuf[k]),
		    abs(epobuf[j])));
		epopos -= pdelay + 5 * MS; 
		if (epopos < 0)
			epopos += SECOND;
		wwv_endpoc(peer, epopos);
		if (!(up->status & SSYNC))
			up->alarm |= SYNERR;
		epomax = 0;
		if (!(up->status & MSYNC))
			wwv_gain(peer);
	}
}


/*
 * wwv_qrz - identify and acquire WWV/WWVH minute sync pulse
 *
 * This routine implements a virtual station process used to acquire
 * minute sync and to mitigate among the ten frequency and station
 * combinations. During minute sync acquisition the process probes each
 * frequency in turn for the minute pulse from either station, which
 * involves searching through the entire minute of samples. After
 * finding a candidate, the process searches only the seconds before and
 * after the candidate for the signal and all other seconds for the
 * noise.
 *
 * Students of radar receiver technology will discover this algorithm
 * amounts to a range gate discriminator. The discriminator requires
 * that the peak minute pulse amplitude be at least 2000 and the SNR be
 * at least 6 dB. In addition after finding a candidate, The peak second
 * pulse amplitude must be at least 2000, the SNR at least 6 dB and the
 * difference between the current and previous epoch must be less than
 * 7.5 ms, which corresponds to a frequency error of 125 PPM.. A compare
 * counter keeps track of the number of successive intervals which
 * satisfy these criteria.
 *
 * Note that, while the minute pulse is found by by the discriminator,
 * the actual value is determined from the second epoch. The assumption
 * is that the discriminator peak occurs about 800 ms into the second,
 * so the timing is retarted to the previous second epoch.
 */
static void
wwv_qrz(
	struct peer *peer,	/* peer structure pointer */
	struct sync *sp,	/* sync channel structure */
	double	syncx,		/* bandpass filtered sync signal */
	int	pdelay		/* propagation delay (samples) */
	)
{
	struct refclockproc *pp;
	struct wwvunit *up;
	char tbuf[80];		/* monitor buffer */
	double snr;		/* on-pulse/off-pulse ratio (dB) */
	long epoch, fpoch;
	int isgood;

	pp = peer->procptr;
	up = (struct wwvunit *)pp->unitptr;

	/*
	 * Find the sample with peak energy, which defines the minute
	 * epoch. If a sample has been found with good amplitude,
	 * accumulate the noise squares for all except the second before
	 * and after that position.
	 */
	isgood = up->epomax > STHR && up->eposnr > SSNR;
	if (isgood) {
		fpoch = up->mphase % SECOND - up->tepoch;
		if (fpoch < 0)
			fpoch += SECOND;
	} else {
		fpoch = pdelay + SYNSIZ;
	}
	epoch = up->mphase - fpoch;
	if (epoch < 0)
		epoch += MINUTE;
	if (syncx > sp->maxamp) {
		sp->maxamp = syncx;
		sp->pos = epoch;
	}
	if (abs((epoch - sp->lastpos) % MINUTE) > SECOND)
		sp->noiamp += syncx;

	/*
	 * At the end of the minute, determine the epoch of the
	 * sync pulse, as well as the SNR and difference between
	 * the current and previous epoch, which represents the
	 * intrinsic frequency error plus jitter.
	 */
	if (up->mphase == 0) {
		sp->synmax = sqrt(sp->maxamp);
		sp->synmin = sqrt(sp->noiamp / (MINUTE - 2 * SECOND));
		epoch = (sp->pos - sp->lastpos) % MINUTE;

		/*
		 * If not yet in minute sync, we have to do a little
		 * dance to find a valid minute sync pulse, emphasis
		 * valid.
		 */
		snr = wwv_snr(sp->synmax, sp->synmin);
		isgood = isgood && sp->synmax > ATHR && snr > ASNR;
		switch (sp->count) {

		/*
		 * In state 0 the station was not heard during the
		 * previous probe. Look for the biggest blip greater
		 * than the amplitude threshold in the minute and assume
		 * that the minute sync pulse. We're fishing here, since
		 * the range gate has not yet been determined. If found,
		 * bump to state 1.
		 */
		case 0:
			if (sp->synmax >= ATHR)
				sp->count++;
			break;

		/*
		 * In state 1 a candidate blip has been found and the
		 * next minute has been searched for another blip. If
		 * none are found acceptable, drop back to state 0 and
		 * hunt some more. Otherwise, a legitimate minute pulse
		 * may have been found, so bump to state 2.
		 */
		case 1:
			if (!isgood) {
				sp->count = 0;
				break;
			}
			sp->count++;
			break;

		/*
		 * In states 2 and above, continue to groom samples as
		 * before and drop back to state 0 if the groom fails.
		 * If it succeeds, set the epoch and bump to the next
		 * state until reaching the threshold, if ever.
		 */
		default:
			if (!isgood || abs(epoch) > AWND * MS) {
				sp->count = 0;
				break;
			}
			sp->mepoch = sp->pos;
			sp->count++;
			break;
		}
		if (pp->sloppyclockflag & CLK_FLAG4) {
			sprintf(tbuf,
			    "wwv8 %d %3d %s %d %5.0f %5.1f %5ld %5d %ld",
			    up->port, up->gain, sp->refid, sp->count,
			    sp->synmax, snr, sp->pos, up->tepoch,
			    epoch);
			record_clock_stats(&peer->srcadr, tbuf);
#ifdef DEBUG
			if (debug)
				printf("%s\n", tbuf);
#endif
		}
		sp->lastpos = sp->pos;
		sp->maxamp = sp->noiamp = 0;
	}
}


/*
 * wwv_endpoc - identify and acquire second sync pulse
 *
 * This routine is called at the end of the second sync interval. It
 * determines the second sync epoch position within the interval and
 * disciplines the sample clock using a frequency-lock loop (FLL).
 *
 * Second sync is determined in the RF input routine as the maximum
 * over all 8000 samples in the second comb filter. To assure accurate
 * and reliable time and frequency discipline, this routine performs a
 * great deal of heavy-handed heuristic data filtering and grooming.
 *
 * Note that, since the minute sync pulse is very wide (800 ms), precise
 * minute sync epoch acquisition requires at least a rough estimate of
 * the second sync pulse (5 ms). This becomes more important in choppy
 * conditions at the lower frequencies at night, since sferics and
 * cochannel crude can badly distort the minute pulse. 
 */
static void
wwv_endpoc(
	struct peer *peer,	/* peer structure pointer */
	int epopos		/* epoch max position */
	)
{
	struct refclockproc *pp;
	struct wwvunit *up;
	static int epoch_mf[3]; /* epoch median filter */
 	static int xepoch;	/* last second epoch */
 	static int zepoch;	/* last averaging interval epoch */
	static int syncnt;	/* run length counter */
	static int maxrun;	/* longest run length */
	static int mepoch;	/* longest run epoch */
	static int avgcnt;	/* averaging interval counter */
	static int avginc;	/* averaging ratchet */
	static int iniflg;	/* initialization flag */
	char tbuf[80];		/* monitor buffer */
	double dtemp;
	int tmp2;

	pp = peer->procptr;
	up = (struct wwvunit *)pp->unitptr;
	if (!iniflg) {
		iniflg = 1;
		memset((char *)epoch_mf, 0, sizeof(epoch_mf));
	}

	/*
	 * A three-stage median filter is used to help denoise the
	 * second sync pulse. The median sample becomes the candidate
	 * epoch.
	 */
	epoch_mf[2] = epoch_mf[1];
	epoch_mf[1] = epoch_mf[0];
	epoch_mf[0] = epopos;
	if (epoch_mf[0] > epoch_mf[1]) {
		if (epoch_mf[1] > epoch_mf[2])
			up->tepoch = epoch_mf[1];	/* 0 1 2 */
		else if (epoch_mf[2] > epoch_mf[0])
			up->tepoch = epoch_mf[0];	/* 2 0 1 */
		else
			up->tepoch = epoch_mf[2];	/* 0 2 1 */
	} else {
		if (epoch_mf[1] < epoch_mf[2])
			up->tepoch = epoch_mf[1];	/* 2 1 0 */
		else if (epoch_mf[2] < epoch_mf[0])
			up->tepoch = epoch_mf[0];	/* 1 0 2 */
		else
			up->tepoch = epoch_mf[2];	/* 1 2 0 */
	}

	/*
	 * If the signal amplitude or SNR fall below thresholds or if no
	 * stations are heard, dim the second sync lamp and start over.
	 */
	if (!(up->status & (SELV | SELH)) || up->epomax < STHR ||
	    up->eposnr < SSNR) {
		up->status &= ~(SSYNC | FGATE);
		avgcnt = syncnt = maxrun = 0;
		return;
	}
	avgcnt++;

	/*
	 * If the epoch candidate is the same as the last one, increment
	 * the compare counter. If not, save the length and epoch of the
	 * current run for use later and reset the counter.
	 */
	tmp2 = (up->tepoch - xepoch) % SECOND;
	if (tmp2 == 0) {
		syncnt++;
	} else {
		if (maxrun > 0 && mepoch == xepoch) {
			maxrun += syncnt;
		} else if (syncnt > maxrun) {
			maxrun = syncnt;
			mepoch = xepoch;
		}
		syncnt = 0;
	}
	if ((pp->sloppyclockflag & CLK_FLAG4) && !(up->status & (SSYNC |
	    MSYNC))) {
		sprintf(tbuf,
		    "wwv1 %04x %5.0f %5.1f %5d %5d %4d %4d",
		    up->status, up->epomax, up->eposnr, up->tepoch,
		    tmp2, avgcnt, syncnt);
		record_clock_stats(&peer->srcadr, tbuf);
#ifdef DEBUG
		if (debug)
			printf("%s\n", tbuf);
#endif /* DEBUG */
	}

	/*
	 * The sample clock frequency is disciplined using a first order
	 * feedback loop with time constant consistent with the Allan
	 * intercept of typical computer clocks.
	 *
	 * The frequency update is calculated from the epoch change in
	 * 125-us units divided by the averaging interval in seconds.
	 * The averaging interval affects other receiver functions,
	 * including the the 1000/1200-Hz comb filter and codec clock
	 * loop. It also affects the 100-Hz subcarrier loop and the bit
	 * and digit comparison counter thresholds.
	 */
	if (avgcnt < up->avgint) {
		xepoch = up->tepoch;
		return;
	}

	/*
	 * During the averaging interval the longest run of identical
	 * epoches is determined. If the longest run is at least 10
	 * seconds, the SSYNC bit is lit and the value becomes the
	 * reference epoch for the next interval. If not, the second
	 * synd lamp is dark and flashers set.
	 */
	if (maxrun > 0 && mepoch == xepoch) {
		maxrun += syncnt;
	} else if (syncnt > maxrun) {
		maxrun = syncnt;
		mepoch = xepoch;
	}
	xepoch = up->tepoch;
	if (maxrun > SCMP) {
		up->status |= SSYNC;
		up->yepoch = mepoch;
	} else {
		up->status &= ~SSYNC;
	}

	/*
	 * If the epoch change over the averaging interval is less than
	 * 1 ms, the frequency is adjusted, but clamped at +-125 PPM. If
	 * greater than 1 ms, the counter is decremented. If the epoch
	 * change is less than 0.5 ms, the counter is incremented. If
	 * the counter increments to +3, the averaging interval is
	 * doubled and the counter set to zero; if it increments to -3,
	 * the interval is halved and the counter set to zero.
	 *
	 * Here be spooks. From careful observations, the epoch
	 * sometimes makes a long run of identical samples, then takes a
	 * lurch due apparently to lost interrupts or spooks. If this
	 * happens, the epoch change times the maximum run length will
	 * be greater than the averaging interval, so the lurch should
	 * be believed but the frequency left alone. Really intricate
	 * here.
	 */
	if (maxrun == 0)
		mepoch = up->tepoch;
	dtemp = (mepoch - zepoch) % SECOND;
	if (up->status & FGATE) {
		if (abs(dtemp) < MAXFREQ * MINAVG) {
			if (maxrun * abs(mepoch - zepoch) <
			    avgcnt) {
				up->freq += dtemp / avgcnt;
				if (up->freq > MAXFREQ)
					up->freq = MAXFREQ;
				else if (up->freq < -MAXFREQ)
					up->freq = -MAXFREQ;
			}
			if (abs(dtemp) < MAXFREQ * MINAVG / 2) {
				if (avginc < 3) {
					avginc++;
				} else {
					if (up->avgint < MAXAVG) {
						up->avgint <<= 1;
						avginc = 0;
					}
				}
			}
		} else {
			if (avginc > -3) {
				avginc--;
			} else {
				if (up->avgint > MINAVG) {
					up->avgint >>= 1;
					avginc = 0;
				}
			}
		}
	}
	if (pp->sloppyclockflag & CLK_FLAG4) {
		sprintf(tbuf,
		    "wwv2 %04x %4.0f %4d %4d %2d %4d %4.0f %6.1f",
		    up->status, up->epomax, mepoch, maxrun, avginc,
		    avgcnt, dtemp, up->freq * 1e6 / SECOND);
		record_clock_stats(&peer->srcadr, tbuf);
#ifdef DEBUG
		if (debug)
			printf("%s\n", tbuf);
#endif /* DEBUG */
	}
	up->status |= FGATE;
	zepoch = mepoch;
	avgcnt = syncnt = maxrun = 0;
}


/*
 * wwv_epoch - epoch scanner
 *
 * This routine scans the receiver second epoch to determine the signal
 * amplitudes and pulse timings. Receiver synchronization is determined
 * by the minute sync pulse detected in the wwv_rf() routine and the
 * second sync pulse detected in the wwv_epoch() routine. A pulse width
 * discriminator extracts data signals from the 100-Hz subcarrier. The
 * transmitted signals are delayed by the propagation delay, receiver
 * delay and filter delay of this program. Delay corrections are
 * introduced separately for WWV and WWVH. 
 *
 * Most communications radios use a highpass filter in the audio stages,
 * which can do nasty things to the subcarrier phase relative to the
 * sync pulses. Therefore, the data subcarrier reference phase is
 * disciplined using the hardlimited quadrature-phase signal sampled at
 * the same time as the in-phase signal. The phase tracking loop uses
 * phase adjustments of plus-minus one sample (125 us).
 */
static void
wwv_epoch(
	struct peer *peer	/* peer structure pointer */
	)
{
	struct refclockproc *pp;
	struct wwvunit *up;
	struct chan *cp;
	static double dpulse;	/* data pulse length */
	double dtemp;

	pp = peer->procptr;
	up = (struct wwvunit *)pp->unitptr;

	/*
	 * Sample the minute sync pulse envelopes at epoch 800 for both
	 * the WWV and WWVH stations. This will be used later for
	 * channel and station mitigation. Note that the seconds epoch
	 * is set here well before the end of the second to make sure we
	 * never seet the epoch backwards.
	 */
	if (up->rphase == 800 * MS) {
		up->repoch = up->yepoch;
		cp = &up->mitig[up->achan];
		cp->wwv.synamp = cp->wwv.amp;
		cp->wwvh.synamp = cp->wwvh.amp;
	}

	/*
	 * Sample the data subcarrier at epoch 15 ms, giving a guard
	 * time of +-15 ms from the beginning of the second until the
	 * pulse rises at 30 ms. The I-channel amplitude is used to
	 * calculate the slice level. The envelope amplitude is used
	 * during the probe seconds to determine the SNR. There is a
	 * compromise here; we want to delay the sample as long as
	 * possible to give the radio time to change frequency and the
	 * AGC to stabilize, but as early as possible if the second
	 * epoch is not exact.
	 */
	if (up->rphase == 15 * MS) {
		up->noiamp = up->irig * up->irig + up->qrig * up->qrig;

	/*
	 * Sample the data subcarrier at epoch 215 ms, giving a guard
	 * time of +-15 ms from the earliest the pulse peak can be
	 * reached to the earliest it can begin to fall. For the data
	 * channel latch the I-channel amplitude for all except the
	 * probe seconds and adjust the 100-Hz reference oscillator
	 * phase using the Q-channel amplitude at this epoch. For the
	 * probe channel latch the envelope amplitude.
	 */
	} else if (up->rphase == 215 * MS) {
		up->sigsig = up->irig;
		if (up->sigsig < 0)
			up->sigsig = 0;
		up->datpha = up->qrig / up->avgint;
		if (up->datpha >= 0) {
			up->datapt++;
			if (up->datapt >= 80)
				up->datapt -= 80;
		} else {
			up->datapt--;
			if (up->datapt < 0)
				up->datapt += 80;
		}
		up->sigamp = up->irig * up->irig + up->qrig * up->qrig;

	/*
	 * The slice level is set half way between the peak signal and
	 * noise levels. Sample the negative zero crossing after epoch
	 * 200 ms and record the epoch at that time. This defines the
	 * length of the data pulse, which will later be converted into
	 * scaled bit probabilities.
	 */
	} else if (up->rphase > 200 * MS) {
		dtemp = (up->sigsig + sqrt(up->noiamp)) / 2;
		if (up->irig < dtemp && dpulse == 0)
			dpulse = up->rphase;
	}

	/*
	 * At the end of the second crank the clock state machine and
	 * adjust the codec gain. Note the epoch is buffered from the
	 * center of the second in order to avoid jitter while the
	 * seconds synch is diddling the epoch. Then, determine the true
	 * offset and update the median filter in the driver interface.
	 *
	 * Sample the data subcarrier envelope at the end of the second
	 * to determine the SNR for the pulse. This gives a guard time
	 * of +-30 ms from the decay of the longest pulse to the rise of
	 * the next pulse.
	 */
	up->rphase++;
	if (up->mphase % SECOND == up->repoch) {
		up->datsnr = wwv_snr(up->sigsig, sqrt(up->noiamp));
		wwv_rsec(peer, dpulse);
		wwv_gain(peer);
		up->rphase = dpulse = 0;
	}
}


/*
 * wwv_rsec - process receiver second
 *
 * This routine is called at the end of each receiver second to
 * implement the per-second state machine. The machine assembles BCD
 * digit bits, decodes miscellaneous bits and dances the leap seconds.
 *
 * Normally, the minute has 60 seconds numbered 0-59. If the leap
 * warning bit is set, the last minute (1439) of 30 June (day 181 or 182
 * for leap years) or 31 December (day 365 or 366 for leap years) is
 * augmented by one second numbered 60. This is accomplished by
 * extending the minute interval by one second and teaching the state
 * machine to ignore it.
 */
static void
wwv_rsec(
	struct peer *peer,	/* peer structure pointer */
	double dpulse
	)
{
	static int iniflg;	/* initialization flag */
	static double bcddld[4]; /* BCD data bits */
	static double bitvec[61]; /* bit integrator for misc bits */
	struct refclockproc *pp;
	struct wwvunit *up;
	struct chan *cp;
	struct sync *sp, *rp;
	l_fp offset;		/* offset in NTP seconds */
	double bit;		/* bit likelihood */
	char tbuf[80];		/* monitor buffer */
	int sw, arg, nsec;
#ifdef IRIG_SUCKS
	int	i;
	l_fp	ltemp;
#endif /* IRIG_SUCKS */

	pp = peer->procptr;
	up = (struct wwvunit *)pp->unitptr;
	if (!iniflg) {
		iniflg = 1;
		memset((char *)bitvec, 0, sizeof(bitvec));
	}

	/*
	 * The bit represents the probability of a hit on zero (negative
	 * values), a hit on one (positive values) or a miss (zero
	 * value). The likelihood vector is the exponential average of
	 * these probabilities. Only the bits of this vector
	 * corresponding to the miscellaneous bits of the timecode are
	 * used, but it's easier to do them all. After that, crank the
	 * seconds state machine.
	 */
	nsec = up->rsec + 1;
	bit = wwv_data(up, dpulse);
	bitvec[up->rsec] += (bit - bitvec[up->rsec]) / TCONST;
	sw = progx[up->rsec].sw;
	arg = progx[up->rsec].arg;
	switch (sw) {

	/*
	 * Ignore this second.
	 */
	case IDLE:			/* 9, 45-49 */
		break;

	/*
	 * Probe channel stuff
	 *
	 * The WWV/H format contains data pulses in second 59 (position
	 * identifier), second 1 (not used) and the minute sync pulse in
	 * second 0. At the end of second 58, QSY to the probe channel,
	 * which rotates over all WWV/H frequencies. At the end of
	 * second 1 QSY back to the data channel.
	 *
	 * At the end of second 0 save the minute sync pulse peak value
	 * previously latched at 800 ms.
	 */
	case SYNC2:			/* 0 */
		cp = &up->mitig[up->achan];
		cp->wwv.synmax = sqrt(cp->wwv.synamp);
		cp->wwvh.synmax = sqrt(cp->wwvh.synamp);
		break;

	/*
	 * At the end of second 1 determine the minute sync pulse
	 * amplitude and SNR and set SYNCNG if these values are below
	 * thresholds. Determine the data pulse amplitude and SNR and
	 * set DATANG if these values are below thresholds. Set ERRRNG
	 * if data pulses in second 59 and second 1 are decoded in
	 * error. Shift a 1 into the reachability register if SYNCNG and
	 * DATANG are both lit; otherwise shift a 0. Ignore ERRRNG for
	 * the present. The number of 1 bits in the last six intervals
	 * represents the channel metric used by the mitigation routine.
	 * Finally, QSY back to the data channel.
	 */
	case SYNC3:			/* 1 */
		cp = &up->mitig[up->achan];
		cp->sigamp = sqrt(up->sigamp);
		cp->noiamp = sqrt(up->noiamp);
		cp->datsnr = wwv_snr(cp->sigamp, cp->noiamp);

		/*
		 * WWV station
		 */
		sp = &cp->wwv;
		sp->synmin = sqrt((sp->synmin + sp->synamp) / 2.);
		sp->synsnr = wwv_snr(sp->synmax, sp->synmin);
		sp->select &= ~(SYNCNG | DATANG | ERRRNG);
		if (sp->synmax < QTHR || sp->synsnr < QSNR)
			sp->select |= SYNCNG;
		if (cp->sigamp < XTHR || cp->datsnr < XSNR)
			sp->select |= DATANG;
		if (up->errcnt > 2)
			sp->select |= ERRRNG;
		sp->reach <<= 1;
		if (sp->reach & (1 << AMAX))
			sp->count--;
		if (!(sp->select & (SYNCNG | DATANG))) {
			sp->reach |= 1;
			sp->count++;
		}

		/*
		 * WWVH station
		 */
		rp = &cp->wwvh;
		rp->synmin = sqrt((rp->synmin + rp->synamp) / 2.);
		rp->synsnr = wwv_snr(rp->synmax, rp->synmin);
		rp->select &= ~(SYNCNG | DATANG | ERRRNG);
		if (rp->synmax < QTHR || rp->synsnr < QSNR)
			rp->select |= SYNCNG;
		if (cp->sigamp < XTHR || cp->datsnr < XSNR)
			rp->select |= DATANG;
		if (up->errcnt > 2)
			rp->select |= ERRRNG;
		rp->reach <<= 1;
		if (rp->reach & (1 << AMAX))
			rp->count--;
		if (!(rp->select & (SYNCNG | DATANG | ERRRNG))) {
			rp->reach |= 1;
			rp->count++;
		}

		/*
		 * Set up for next minute.
		 */
		if (pp->sloppyclockflag & CLK_FLAG4) {
			sprintf(tbuf,
			    "wwv5 %2d %04x %3d %4d %d %.0f/%.1f %s %04x %.0f %.0f/%.1f %s %04x %.0f %.0f/%.1f",
			    up->port, up->status, up->gain, up->yepoch,
			    up->errcnt, cp->sigamp, cp->datsnr,
			    sp->refid, sp->reach & 0xffff,
			    wwv_metric(sp), sp->synmax, sp->synsnr,
			    rp->refid, rp->reach & 0xffff,
			    wwv_metric(rp), rp->synmax, rp->synsnr);
			record_clock_stats(&peer->srcadr, tbuf);
#ifdef DEBUG
			if (debug)
				printf("%s\n", tbuf);
#endif /* DEBUG */
		}
#ifdef ICOM
		if (up->fd_icom > 0)
			wwv_qsy(peer, up->dchan);
#endif /* ICOM */
		up->status &= ~SFLAG;
		up->errcnt = 0;
		up->alarm = 0;
		wwv_newchan(peer);
		break;

	/*
	 * Save the bit probability in the BCD data vector at the index
	 * given by the argument. Note that all bits of the vector have
	 * to be above the data gate threshold for the digit to be
	 * considered valid. Bits not used in the digit are forced to
	 * zero and not checked for errors.
	 */
	case COEF:			/* 4-7, 10-13, 15-17, 20-23,
					   25-26, 30-33, 35-38, 40-41,
					   51-54 */ 
		if (up->status & DGATE)
			up->status |= BGATE;
		bcddld[arg] = bit;
		break;

	case COEF2:			/* 18, 27-28, 42-43 */
		bcddld[arg] = 0;
		break;

	/*
	 * Correlate coefficient vector with each valid digit vector and
	 * save in decoding matrix. We step through the decoding matrix
	 * digits correlating each with the coefficients and saving the
	 * greatest and the next lower for later SNR calculation.
	 */
	case DECIM2:			/* 29 */
		wwv_corr4(peer, &up->decvec[arg], bcddld, bcd2);
		break;

	case DECIM3:			/* 44 */
		wwv_corr4(peer, &up->decvec[arg], bcddld, bcd3);
		break;

	case DECIM6:			/* 19 */
		wwv_corr4(peer, &up->decvec[arg], bcddld, bcd6);
		break;

	case DECIM9:			/* 8, 14, 24, 34, 39 */
		wwv_corr4(peer, &up->decvec[arg], bcddld, bcd9);
		break;

	/*
	 * Miscellaneous bits. If above the positive threshold, declare
	 * 1; if below the negative threshold, declare 0; otherwise
	 * raise the SYMERR alarm. At the end of second 58, QSY to the
	 * probe channel. The design is intended to preserve the bits
	 * over periods of signal loss.
	 */
	case MSC20:			/* 55 */
		wwv_corr4(peer, &up->decvec[YR + 1], bcddld, bcd9);
		/* fall through */

	case MSCBIT:			/* 2-3, 50, 56-57 */
		if (bitvec[up->rsec] > BTHR)
			up->misc |= arg;
		else if (bitvec[up->rsec] < -BTHR)
			up->misc &= ~arg;
		else
			up->alarm |= SYMERR;
		break;

	/*
	 * Save the data channel gain, then QSY to the probe channel.
	 */
	case MSC21:			/* 58 */
		if (bitvec[up->rsec] > BTHR)
			up->misc |= arg;
		else if (bitvec[up->rsec] < -BTHR)
			up->misc &= ~arg;
		else
			up->alarm |= SYMERR;
		up->mitig[up->dchan].gain = up->gain;
#ifdef ICOM
		if (up->fd_icom > 0) {
			up->schan = (up->schan + 1) % NCHAN;
			wwv_qsy(peer, up->schan);
		}
#endif /* ICOM */
		up->status |= SFLAG | SELV | SELH;
		up->errbit = up->errcnt;
		up->errcnt = 0;
		break;

	/*
	 * The endgames
	 *
	 * During second 59 the receiver and codec AGC are settling
	 * down, so the data pulse is unusable. At the end of this
	 * second, latch the minute sync pulse noise floor. Then do the
	 * minute processing and update the system clock. If a leap
	 * second sail on to the next second (60); otherwise, set up for
	 * the next minute.
	 */
	case MIN1:			/* 59 */
		cp = &up->mitig[up->achan];
		cp->wwv.synmin = cp->wwv.synamp;
		cp->wwvh.synmin = cp->wwvh.synamp;

		/*
		 * Dance the leap if necessary and the kernel has the
		 * right stuff. Then, wind up the clock and initialize
		 * for the following minute. If the leap dance, note the
		 * kernel is armed one second before the actual leap is
		 * scheduled.
		 */
		if (up->status & SSYNC && up->digcnt >= 9)
			up->status |= INSYNC;
		if (up->status & LEPDAY) {
			pp->leap = LEAP_ADDSECOND;
		} else {
			pp->leap = LEAP_NOWARNING;
			wwv_tsec(up);
			nsec = up->digcnt = 0;
		}
		pp->lencode = timecode(up, pp->a_lastcode);
		record_clock_stats(&peer->srcadr, pp->a_lastcode);
#ifdef DEBUG
		if (debug)
			printf("wwv: timecode %d %s\n", pp->lencode,
			    pp->a_lastcode);
#endif /* DEBUG */
		if (up->status & INSYNC && up->watch < HOLD)
			refclock_receive(peer);
		break;

	/*
	 * If LEPDAY is set on the last minute of 30 June or 31
	 * December, the LEPSEC bit is set. At the end of the minute in
	 * which LEPSEC is set the transmitter and receiver insert an
	 * extra second (60) in the timescale and the minute sync skips
	 * a second. We only get to test this wrinkle at intervals of
	 * about 18 months; the actual mileage may vary.
	 */
	case MIN2:			/* 60 */
		wwv_tsec(up);
		nsec = up->digcnt = 0;
		break;
	}

	/*
	 * If digit sync has not been acquired before timeout or if no
	 * station has been heard, game over and restart from scratch.
	 */
	if (!(up->status & DSYNC) && (!(up->status & (SELV | SELH)) ||
	    up->watch > DIGIT)) {
		wwv_newgame(peer);
		return;
	}

	/*
	 * If no timestamps have been struck before timeout, game over
	 * and restart from scratch.
	 */
	if (up->watch > PANIC) {
		wwv_newgame(peer);
		return;
	}
	pp->disp += AUDIO_PHI;
	up->rsec = nsec;

#ifdef IRIG_SUCKS
	/*
	 * You really don't wanna know what comes down here. Leave it to
	 * say Solaris 2.8 broke the nice clean audio stream, apparently
	 * affected by a 5-ms sawtooth jitter. Sundown on Solaris. This
	 * leaves a little twilight.
	 *
	 * The scheme involves differentiation, forward learning and
	 * integration. The sawtooth has a period of 11 seconds. The
	 * timestamp differences are integrated and subtracted from the
	 * signal.
	 */
	ltemp = pp->lastrec;
	L_SUB(&ltemp, &pp->lastref);
	if (ltemp.l_f < 0)
		ltemp.l_i = -1;
	else
		ltemp.l_i = 0;
	pp->lastref = pp->lastrec;
	if (!L_ISNEG(&ltemp))
		L_CLR(&up->wigwag);
	else
		L_ADD(&up->wigwag, &ltemp);
	L_SUB(&pp->lastrec, &up->wigwag);
	up->wiggle[up->wp] = ltemp;

	/*
	 * Bottom fisher. To understand this, you have to know about
	 * velocity microphones and AM transmitters. No further
	 * explanation is offered, as this is truly a black art.
	 */
	up->wigbot[up->wp] = pp->lastrec;
	for (i = 0; i < WIGGLE; i++) {
		if (i != up->wp)
			up->wigbot[i].l_ui++;
		L_SUB(&pp->lastrec, &up->wigbot[i]);
		if (L_ISNEG(&pp->lastrec))
			L_ADD(&pp->lastrec, &up->wigbot[i]);
		else
			pp->lastrec = up->wigbot[i];
	}
	up->wp++;
	up->wp %= WIGGLE;
#endif /* IRIG_SUCKS */

	/*
	 * If victory has been declared and seconds sync is lit, strike
	 * a timestamp. It should not be a surprise, especially if the
	 * radio is not tunable, that sometimes no stations are above
	 * the noise and the reference ID set to NONE.
	 */
	if (up->status & INSYNC && up->status & SSYNC) {
		pp->second = up->rsec;
		pp->minute = up->decvec[MN].digit + up->decvec[MN +
		    1].digit * 10;
		pp->hour = up->decvec[HR].digit + up->decvec[HR +
		    1].digit * 10;
		pp->day = up->decvec[DA].digit + up->decvec[DA +
		    1].digit * 10 + up->decvec[DA + 2].digit * 100;
		pp->year = up->decvec[YR].digit + up->decvec[YR +
		    1].digit * 10;
		pp->year += 2000;
		L_CLR(&offset);
		if (!clocktime(pp->day, pp->hour, pp->minute,
		    pp->second, GMT, up->timestamp.l_ui,
		    &pp->yearstart, &offset.l_ui)) {
			up->errflg = CEVNT_BADTIME;
		} else {
			up->watch = 0;
			pp->disp = 0;
			refclock_process_offset(pp, offset,
			    up->timestamp, PDELAY);
		}
	}
	if ((pp->sloppyclockflag & CLK_FLAG4) && !(up->status &
	    DSYNC)) {
		sprintf(tbuf,
		    "wwv3 %2d %04x %5.0f %5.1f %5.0f %5.1f %5.0f",
		    up->rsec, up->status, up->epomax, up->eposnr,
		    up->sigsig, up->datsnr, bit);
		record_clock_stats(&peer->srcadr, tbuf);
#ifdef DEBUG
		if (debug)
			printf("%s\n", tbuf);
#endif /* DEBUG */
	}
}


/*
 * wwv_data - calculate bit probability
 *
 * This routine is called at the end of the receiver second to calculate
 * the probabilities that the previous second contained a zero (P0), one
 * (P1) or position indicator (P2) pulse. If not in sync or if the data
 * bit is bad, a bit error is declared and the probabilities are forced
 * to zero. Otherwise, the data gate is enabled and the probabilities
 * are calculated. Note that the data matched filter contributes half
 * the pulse width, or 85 ms.
 *
 * It's important to observe that there are three conditions to
 * determine: average to +1 (hit), average to -1 (miss) or average to
 * zero (erasure). The erasure condition results from insufficient
 * signal (noise) and has no bias toward either a hit or miss.
 */
static double
wwv_data(
	struct wwvunit *up,	/* driver unit pointer */
	double pulse		/* pulse length (sample units) */
	)
{
	double p0, p1, p2;	/* probabilities */
	double dpulse;		/* pulse length in ms */

	p0 = p1 = p2 = 0;
	dpulse = pulse - DATSIZ / 2;

	/*
	 * If no station is being tracked, if either the data amplitude
	 * or SNR are below threshold or if the pulse length is less
	 * than 170 ms, declare an erasure.
	 */
	if (!(up->status & (SELV | SELH)) || up->sigsig < DTHR ||
	    up->datsnr < DSNR || dpulse < DATSIZ) {
		up->status |= DGATE;
		up->errcnt++;
		if (up->errcnt > MAXERR)
			up->alarm |= MODERR;
		return (0); 
	}

	/*
	 * The probability of P0 is one below 200 ms falling to zero at
	 * 500 ms. The probability of P1 is zero below 200 ms rising to
	 * one at 500 ms and falling to zero at 800 ms. The probability
	 * of P2 is zero below 500 ms, rising to one above 800 ms.
	 */
	up->status &= ~DGATE;
	if (dpulse < (200 * MS)) {
		p0 = 1;
	} else if (dpulse < 500 * MS) {
		dpulse -= 200 * MS;
		p1 = dpulse / (300 * MS);
		p0 = 1 - p1;
	} else if (dpulse < 800 * MS) {
		dpulse -= 500 * MS;
		p2 = dpulse / (300 * MS);
		p1 = 1 - p2;
	} else {
		p2 = 1;
	}

	/*
	 * The ouput is a metric that ranges from -1 (P0), to +1 (P1)
	 * scaled for convenience. An output of zero represents an
	 * erasure, either because of a data error or pulse length
	 * greater than 500 ms. At the moment, we don't use P2.
	 */
	return ((p1 - p0) * MAXSIG);
}


/*
 * wwv_corr4 - determine maximum likelihood digit
 *
 * This routine correlates the received digit vector with the BCD
 * coefficient vectors corresponding to all valid digits at the given
 * position in the decoding matrix. The maximum value corresponds to the
 * maximum likelihood digit, while the ratio of this value to the next
 * lower value determines the likelihood function. Note that, if the
 * digit is invalid, the likelihood vector is averaged toward a miss.
 */
static void
wwv_corr4(
	struct peer *peer,	/* peer unit pointer */
	struct decvec *vp,	/* decoding table pointer */
	double data[],		/* received data vector */
	double tab[][4]		/* correlation vector array */
	)
{
	struct refclockproc *pp;
	struct wwvunit *up;

	double topmax, nxtmax;	/* metrics */
	double acc;		/* accumulator */
	char tbuf[80];		/* monitor buffer */
	int mldigit;		/* max likelihood digit */
	int diff;		/* decoding difference */
	int i, j;

	pp = peer->procptr;
	up = (struct wwvunit *)pp->unitptr;

	/*
	 * Correlate digit vector with each BCD coefficient vector. If
	 * any BCD digit bit is bad, consider all bits a miss.
	 */
	mldigit = 0;
	topmax = nxtmax = -MAXSIG;
	for (i = 0; tab[i][0] != 0; i++) {
		acc = 0;
		for (j = 0; j < 4; j++) {
			if (!(up->status & BGATE))
				acc += data[j] * tab[i][j];
		}
		acc = (vp->like[i] += (acc - vp->like[i]) / TCONST);
		if (acc > topmax) {
			nxtmax = topmax;
			topmax = acc;
			mldigit = i;
		} else if (acc > nxtmax) {
			nxtmax = acc;
		}
	}
	vp->mldigit = mldigit;
	vp->digprb = topmax;
	vp->digsnr = wwv_snr(topmax, nxtmax);

	/*
	 * The maximum likelihood digit is compared with the current
	 * clock digit. The difference represents the decoding phase
	 * error. If the clock is not yet synchronized, the phase error
	 * is corrected even of the digit probability and likelihood are
	 * below thresholds. This avoids lengthy averaging times should
	 * a carry mistake occur. However, the digit is not declared
	 * synchronized until these values are above thresholds and the
	 * last five decoded values are identical. If the clock is
	 * synchronized, the phase error is not corrected unless the
	 * last five digits are all above thresholds and identical. This
	 * avoids mistakes when the signal is coming out of the noise
	 * and the SNR is very marginal.
	 */
	diff = mldigit - vp->digit;
	if (diff < 0)
		diff += vp->radix;
	if (diff != vp->phase) {
		vp->count = 0;
		vp->phase = diff;
	}
	if (vp->digsnr < BSNR) {
		vp->count = 0;
		up->alarm |= SYMERR;
	} else if (vp->digprb < BTHR) {
		vp->count = 0;
		up->alarm |= SYMERR;
		if (!(up->status & INSYNC)) {
			vp->phase = 0;
			vp->digit = mldigit;
		}
	} else if (vp->count < BCMP) {
		vp->count++;
		up->status |= DSYNC;
		if (!(up->status & INSYNC)) {
			vp->phase = 0;
			vp->digit = mldigit;
		}
	} else {
		vp->phase = 0;
		vp->digit = mldigit;
		up->digcnt++;
	}
	if (vp->digit != mldigit)
		up->alarm |= DECERR;
	if ((pp->sloppyclockflag & CLK_FLAG4) && !(up->status &
	    INSYNC)) {
		sprintf(tbuf,
		    "wwv4 %2d %04x %5.0f %2d %d %d %d %d %5.0f %5.1f",
		    up->rsec, up->status, up->epomax, vp->radix,
		    vp->digit, vp->mldigit, vp->phase, vp->count,
		    vp->digprb, vp->digsnr);
		record_clock_stats(&peer->srcadr, tbuf);
#ifdef DEBUG
		if (debug)
			printf("%s\n", tbuf);
#endif /* DEBUG */
	}
	up->status &= ~BGATE;
}


/*
 * wwv_tsec - transmitter minute processing
 *
 * This routine is called at the end of the transmitter minute. It
 * implements a state machine that advances the logical clock subject to
 * the funny rules that govern the conventional clock and calendar.
 */
static void
wwv_tsec(
	struct wwvunit *up	/* driver structure pointer */
	)
{
	int minute, day, isleap;
	int temp;

	/*
	 * Advance minute unit of the day.
	 */
	temp = carry(&up->decvec[MN]);	/* minute units */

	/*
	 * Propagate carries through the day.
	 */ 
	if (temp == 0)			/* carry minutes */
		temp = carry(&up->decvec[MN + 1]);
	if (temp == 0)			/* carry hours */
		temp = carry(&up->decvec[HR]);
	if (temp == 0)
		temp = carry(&up->decvec[HR + 1]);

	/*
	 * Decode the current minute and day. Set leap day if the
	 * timecode leap bit is set on 30 June or 31 December. Set leap
	 * minute if the last minute on leap day. This code fails in
	 * 2400 AD.
	 */
	minute = up->decvec[MN].digit + up->decvec[MN + 1].digit *
	    10 + up->decvec[HR].digit * 60 + up->decvec[HR +
	    1].digit * 600;
	day = up->decvec[DA].digit + up->decvec[DA + 1].digit * 10 +
	    up->decvec[DA + 2].digit * 100;
	isleap = (up->decvec[YR].digit & 0x3) == 0;
	if (up->misc & SECWAR && (day == (isleap ? 182 : 183) || day ==
	    (isleap ? 365 : 366)) && up->status & INSYNC && up->status &
	    SSYNC)
		up->status |= LEPDAY;
	else
		up->status &= ~LEPDAY;
	if (up->status & LEPDAY && minute == 1439)
		up->status |= LEPSEC;
	else
		up->status &= ~LEPSEC;

	/*
	 * Roll the day if this the first minute and propagate carries
	 * through the year.
	 */
	if (minute != 1440)
		return;
	minute = 0;
	while (carry(&up->decvec[HR]) != 0); /* advance to minute 0 */
	while (carry(&up->decvec[HR + 1]) != 0);
	day++;
	temp = carry(&up->decvec[DA]);	/* carry days */
	if (temp == 0)
		temp = carry(&up->decvec[DA + 1]);
	if (temp == 0)
		temp = carry(&up->decvec[DA + 2]);

	/*
	 * Roll the year if this the first day and propagate carries
	 * through the century.
	 */
	if (day != (isleap ? 365 : 366))
		return;
	day = 1;
	while (carry(&up->decvec[DA]) != 1); /* advance to day 1 */
	while (carry(&up->decvec[DA + 1]) != 0);
	while (carry(&up->decvec[DA + 2]) != 0);
	temp = carry(&up->decvec[YR]);	/* carry years */
	if (temp)
		carry(&up->decvec[YR + 1]);
}


/*
 * carry - process digit
 *
 * This routine rotates a likelihood vector one position and increments
 * the clock digit modulo the radix. It returns the new clock digit or
 * zero if a carry occurred. Once synchronized, the clock digit will
 * match the maximum likelihood digit corresponding to that position.
 */
static int
carry(
	struct decvec *dp	/* decoding table pointer */
	)
{
	int temp;
	int j;

	dp->digit++;			/* advance clock digit */
	if (dp->digit == dp->radix) {	/* modulo radix */
		dp->digit = 0;
	}
	temp = dp->like[dp->radix - 1];	/* rotate likelihood vector */
	for (j = dp->radix - 1; j > 0; j--)
		dp->like[j] = dp->like[j - 1];
	dp->like[0] = temp;
	return (dp->digit);
}


/*
 * wwv_snr - compute SNR or likelihood function
 */
static double
wwv_snr(
	double signal,		/* signal */
	double noise		/* noise */
	)
{
	double rval;

	/*
	 * This is a little tricky. Due to the way things are measured,
	 * either or both the signal or noise amplitude can be negative
	 * or zero. The intent is that, if the signal is negative or
	 * zero, the SNR must always be zero. This can happen with the
	 * subcarrier SNR before the phase has been aligned. On the
	 * other hand, in the likelihood function the "noise" is the
	 * next maximum down from the peak and this could be negative.
	 * However, in this case the SNR is truly stupendous, so we
	 * simply cap at MAXSNR dB.
	 */
	if (signal <= 0) {
		rval = 0;
	} else if (noise <= 0) {
		rval = MAXSNR;
	} else {
		rval = 20 * log10(signal / noise);
		if (rval > MAXSNR)
			rval = MAXSNR;
	}
	return (rval);
}


/*
 * wwv_newchan - change to new data channel
 *
 * The radio actually appears to have ten channels, one channel for each
 * of five frequencies and each of two stations (WWV and WWVH), although
 * if not tunable only the 15 MHz channels appear live. While the radio
 * is tuned to the working data channel frequency and station for most
 * of the minute, during seconds 59, 0 and 1 the radio is tuned to a
 * probe frequency in order to search for minute sync pulse and data
 * subcarrier from other transmitters.
 *
 * The search for WWV and WWVH operates simultaneously, with WWV minute
 * sync pulse at 1000 Hz and WWVH at 1200 Hz. The probe frequency
 * rotates each minute over 2.5, 5, 10, 15 and 20 MHz in order and yes,
 * we all know WWVH is dark on 20 MHz, but few remember when WWV was lit
 * on 25 MHz.
 *
 * This routine selects the best channel using a metric computed from
 * the reachability register and minute pulse amplitude. Normally, the
 * award goes to the the channel with the highest metric; but, in case
 * of ties, the award goes to the channel with the highest minute sync
 * pulse amplitude and then to the highest frequency.
 *
 * The routine performs an important squelch function to keep dirty data
 * from polluting the integrators. During acquisition and until the
 * clock is synchronized, the signal metric must be at least MTR (13);
 * after that the metrict must be at least TTHR (50). If either of these
 * is not true, the station select bits are cleared so the second sync
 * is disabled and the data bit integrators averaged to a miss. 
 */
static void
wwv_newchan(
	struct peer *peer	/* peer structure pointer */
	)
{
	struct refclockproc *pp;
	struct wwvunit *up;
	struct sync *sp, *rp;
	double rank, dtemp;
	int i, j;

	pp = peer->procptr;
	up = (struct wwvunit *)pp->unitptr;

	/*
	 * Search all five station pairs looking for the channel with
	 * maximum metric. If no station is found above thresholds, the
	 * reference ID is set to NONE and we wait for hotter ions.
	 */
	j = 0;
	sp = NULL;
	rank = 0;
	for (i = 0; i < NCHAN; i++) {
		rp = &up->mitig[i].wwvh;
		dtemp = wwv_metric(rp);
		if (dtemp >= rank) {
			rank = dtemp;
			sp = rp;
			j = i;
		}
		rp = &up->mitig[i].wwv;
		dtemp = wwv_metric(rp);
		if (dtemp >= rank) {
			rank = dtemp;
			sp = rp;
			j = i;
		}
	}
	up->dchan = j;
	up->sptr = sp;
	up->status &= ~(SELV | SELH);
	memcpy(&pp->refid, "NONE", 4);
	if ((!(up->status & INSYNC) && rank >= MTHR) || ((up->status &
	    INSYNC) && rank >= TTHR)) {
		up->status |= sp->select & (SELV | SELH);
		memcpy(&pp->refid, sp->refid, 4);
	}
	if (peer->stratum <= 1)
		memcpy(&peer->refid, &pp->refid, 4);
}


/*
 * www_newgame - reset and start over
 */
static void
wwv_newgame(
	struct peer *peer	/* peer structure pointer */
	)
{
	struct refclockproc *pp;
	struct wwvunit *up;
	struct chan *cp;
	int i;

	pp = peer->procptr;
	up = (struct wwvunit *)pp->unitptr;

	/*
	 * Initialize strategic values. Note we set the leap bits
	 * NOTINSYNC and the refid "NONE".
	 */
	peer->leap = LEAP_NOTINSYNC;
	up->watch = up->status = up->alarm = 0;
	up->avgint = MINAVG;
	up->freq = 0;
	up->sptr = NULL;
	up->gain = MAXGAIN / 2;

	/*
	 * Initialize the station processes for audio gain, select bit,
	 * station/frequency identifier and reference identifier.
	 */
	memset(up->mitig, 0, sizeof(up->mitig));
	for (i = 0; i < NCHAN; i++) {
		cp = &up->mitig[i];
		cp->gain = up->gain;
		cp->wwv.select = SELV;
		sprintf(cp->wwv.refid, "WV%.0f", floor(qsy[i])); 
		cp->wwvh.select = SELH;
		sprintf(cp->wwvh.refid, "WH%.0f", floor(qsy[i])); 
	}
	wwv_newchan(peer);
}

/*
 * wwv_metric - compute station metric
 *
 * The most significant bits represent the number of ones in the
 * reachability register. The least significant bits represent the
 * minute sync pulse amplitude. The combined value is scaled 0-100.
 */
double
wwv_metric(
	struct sync *sp		/* station pointer */
	)
{
	double	dtemp;

	dtemp = sp->count * MAXSIG;
	if (sp->synmax < MAXSIG)
		dtemp += sp->synmax;
	else
		dtemp += MAXSIG - 1;
	dtemp /= (AMAX + 1) * MAXSIG;
	return (dtemp * 100.);
}


#ifdef ICOM
/*
 * wwv_qsy - Tune ICOM receiver
 *
 * This routine saves the AGC for the current channel, switches to a new
 * channel and restores the AGC for that channel. If a tunable receiver
 * is not available, just fake it.
 */
static int
wwv_qsy(
	struct peer *peer,	/* peer structure pointer */
	int	chan		/* channel */
	)
{
	int rval = 0;
	struct refclockproc *pp;
	struct wwvunit *up;

	pp = peer->procptr;
	up = (struct wwvunit *)pp->unitptr;
	if (up->fd_icom > 0) {
		up->mitig[up->achan].gain = up->gain;
		rval = icom_freq(up->fd_icom, peer->ttl & 0x7f,
		    qsy[chan]);
		up->achan = chan;
		up->gain = up->mitig[up->achan].gain;
	}
	return (rval);
}
#endif /* ICOM */


/*
 * timecode - assemble timecode string and length
 *
 * Prettytime format - similar to Spectracom
 *
 * sq yy ddd hh:mm:ss ld dut lset agc iden sig errs freq avgt
 *
 * s	sync indicator ('?' or ' ')
 * q	error bits (hex 0-F)
 * yyyy	year of century
 * ddd	day of year
 * hh	hour of day
 * mm	minute of hour
 * ss	second of minute)
 * l	leap second warning (' ' or 'L')
 * d	DST state ('S', 'D', 'I', or 'O')
 * dut	DUT sign and magnitude (0.1 s)
 * lset	minutes since last clock update
 * agc	audio gain (0-255)
 * iden	reference identifier (station and frequency)
 * sig	signal quality (0-100)
 * errs	bit errors in last minute
 * freq	frequency offset (PPM)
 * avgt	averaging time (s)
 */
static int
timecode(
	struct wwvunit *up,	/* driver structure pointer */
	char *ptr		/* target string */
	)
{
	struct sync *sp;
	int year, day, hour, minute, second, dut;
	char synchar, leapchar, dst;
	char cptr[50];
	

	/*
	 * Common fixed-format fields
	 */
	synchar = (up->status & INSYNC) ? ' ' : '?';
	year = up->decvec[YR].digit + up->decvec[YR + 1].digit * 10 +
	    2000;
	day = up->decvec[DA].digit + up->decvec[DA + 1].digit * 10 +
	    up->decvec[DA + 2].digit * 100;
	hour = up->decvec[HR].digit + up->decvec[HR + 1].digit * 10;
	minute = up->decvec[MN].digit + up->decvec[MN + 1].digit * 10;
	second = 0;
	leapchar = (up->misc & SECWAR) ? 'L' : ' ';
	dst = dstcod[(up->misc >> 4) & 0x3];
	dut = up->misc & 0x7;
	if (!(up->misc & DUTS))
		dut = -dut;
	sprintf(ptr, "%c%1X", synchar, up->alarm);
	sprintf(cptr, " %4d %03d %02d:%02d:%02d %c%c %+d",
	    year, day, hour, minute, second, leapchar, dst, dut);
	strcat(ptr, cptr);

	/*
	 * Specific variable-format fields
	 */
	sp = up->sptr;
	sprintf(cptr, " %d %d %s %.0f %d %.1f %d", up->watch,
	    up->mitig[up->dchan].gain, sp->refid, wwv_metric(sp),
	    up->errbit, up->freq / SECOND * 1e6, up->avgint);
	strcat(ptr, cptr);
	return (strlen(ptr));
}


/*
 * wwv_gain - adjust codec gain
 *
 * This routine is called at the end of each second. It counts the
 * number of signal clips above the MAXSIG threshold during the previous
 * second. If there are no clips, the gain is bumped up; if too many
 * clips, it is bumped down. The decoder is relatively insensitive to
 * amplitude, so this crudity works just fine. The input port is set and
 * the error flag is cleared, mostly to be ornery.
 */
static void
wwv_gain(
	struct peer *peer	/* peer structure pointer */
	)
{
	struct refclockproc *pp;
	struct wwvunit *up;

	pp = peer->procptr;
	up = (struct wwvunit *)pp->unitptr;

	/*
	 * Apparently, the codec uses only the high order bits of the
	 * gain control field. Thus, it may take awhile for changes to
	 * wiggle the hardware bits.
	 */
	if (up->clipcnt == 0) {
		up->gain += 4;
		if (up->gain > MAXGAIN)
			up->gain = MAXGAIN;
	} else if (up->clipcnt > MAXCLP) {
		up->gain -= 4;
		if (up->gain < 0)
			up->gain = 0;
	}
	audio_gain(up->gain, up->mongain, up->port);
	up->clipcnt = 0;
#if DEBUG
	if (debug > 1)
		audio_show();
#endif
}


#else
int refclock_wwv_bs;
#endif /* REFCLOCK */
