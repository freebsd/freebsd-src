/*
 * refclock_chu - clock driver for Canadian CHU time/frequency station
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#if defined(REFCLOCK) && defined(CLOCK_CHU)

#include <stdio.h>
#include <ctype.h>
#include <sys/time.h>
#include <time.h>
#include <math.h>

#include "ntpd.h"
#include "ntp_io.h"
#include "ntp_refclock.h"
#include "ntp_calendar.h"
#include "ntp_stdlib.h"
#ifdef AUDIO_CHU
#include "audio.h"
#endif /* AUDIO_CHU */

#define ICOM 	1		/* undefine to suppress ICOM code */

#ifdef ICOM
#include "icom.h"
#endif /* ICOM */

/*
 * Audio CHU demodulator/decoder
 *
 * This driver synchronizes the computer time using data encoded in
 * radio transmissions from Canadian time/frequency station CHU in
 * Ottawa, Ontario. Transmissions are made continuously on 3330 kHz,
 * 7335 kHz and 14670 kHz in upper sideband, compatible AM mode. An
 * ordinary shortwave receiver can be tuned manually to one of these
 * frequencies or, in the case of ICOM receivers, the receiver can be
 * tuned automatically using this program as propagation conditions
 * change throughout the day and night.
 *
 * The driver receives, demodulates and decodes the radio signals when
 * connected to the audio codec of a Sun workstation running SunOS or
 * Solaris, and with a little help, other workstations with similar
 * codecs or sound cards. In this implementation, only one audio driver
 * and codec can be supported on a single machine.
 *
 * The driver can be compiled to use a Bell 103 compatible modem or
 * modem chip to receive the radio signal and demodulate the data.
 * Alternatively, the driver can be compiled to use the audio codec of
 * the Sun workstation or another with compatible audio drivers. In the
 * latter case, the driver implements the modem using DSP routines, so
 * the radio can be connected directly to either the microphone on line
 * input port. In either case, the driver decodes the data using a
 * maximum likelihood technique which exploits the considerable degree
 * of redundancy available to maximize accuracy and minimize errors.
 *
 * The CHU time broadcast includes an audio signal compatible with the
 * Bell 103 modem standard (mark = 2225 Hz, space = 2025 Hz). It consist
 * of nine, ten-character bursts transmitted at 300 bps and beginning
 * each second from second 31 to second 39 of the minute. Each character
 * consists of eight data bits plus one start bit and two stop bits to
 * encode two hex digits. The burst data consist of five characters (ten
 * hex digits) followed by a repeat of these characters. In format A,
 * the characters are repeated in the same polarity; in format B, the
 * characters are repeated in the opposite polarity.
 *
 * Format A bursts are sent at seconds 32 through 39 of the minute in
 * hex digits
 *
 *	6dddhhmmss6dddhhmmss
 *
 * The first ten digits encode a frame marker (6) followed by the day
 * (ddd), hour (hh in UTC), minute (mm) and the second (ss). Since
 * format A bursts are sent during the third decade of seconds the tens
 * digit of ss is always 3. The driver uses this to determine correct
 * burst synchronization. These digits are then repeated with the same
 * polarity.
 *
 * Format B bursts are sent at second 31 of the minute in hex digits
 *
 *	xdyyyyttaaxdyyyyttaa
 *
 * The first ten digits encode a code (x described below) followed by
 * the DUT1 (d in deciseconds), Gregorian year (yyyy), difference TAI -
 * UTC (tt) and daylight time indicator (aa) peculiar to Canada. These
 * digits are then repeated with inverted polarity.
 *
 * The x is coded
 *
 * 1 Sign of DUT (0 = +)
 * 2 Leap second warning. One second will be added.
 * 4 Leap second warning. One second will be subtracted.
 * 8 Even parity bit for this nibble.
 *
 * By design, the last stop bit of the last character in the burst
 * coincides with 0.5 second. Since characters have 11 bits and are
 * transmitted at 300 bps, the last stop bit of the first character
 * coincides with 0.5 - 10 * 11/300 = 0.133 second. Depending on the
 * UART, character interrupts can vary somewhere between the beginning
 * of bit 9 and end of bit 11. These eccentricities can be corrected
 * along with the radio propagation delay using fudge time 1.
 *
 * Debugging aids
 *
 * The timecode format used for debugging and data recording includes
 * data helpful in diagnosing problems with the radio signal and serial
 * connections. With debugging enabled (-d -d -d on the ntpd command
 * line), the driver produces one line for each burst in two formats
 * corresponding to format A and B. Following is format A:
 *
 *	n b f s m code
 *
 * where n is the number of characters in the burst (0-11), b the burst
 * distance (0-40), f the field alignment (-1, 0, 1), s the
 * synchronization distance (0-16), m the burst number (2-9) and code
 * the burst characters as received. Note that the hex digits in each
 * character are reversed, so the burst
 *
 *	10 38 0 16 9 06851292930685129293
 *
 * is interpreted as containing 11 characters with burst distance 38,
 * field alignment 0, synchronization distance 16 and burst number 9.
 * The nibble-swapped timecode shows day 58, hour 21, minute 29 and
 * second 39.
 *
 * When the audio driver is compiled, format A is preceded by
 * the current gain (0-255) and relative signal level (0-9999). The
 * receiver folume control should be set so that the gain is somewhere
 * near the middle of the range 0-255, which results in a signal level
 * near 1000.
 *
 * Following is format B:
 * 
 *	n b s code
 *
 * where n is the number of characters in the burst (0-11), b the burst
 * distance (0-40), s the synchronization distance (0-40) and code the
 * burst characters as received. Note that the hex digits in each
 * character are reversed and the last ten digits inverted, so the burst
 *
 *	11 40 1091891300ef6e76ecff
 *
 * is interpreted as containing 11 characters with burst distance 40.
 * The nibble-swapped timecode shows DUT1 +0.1 second, year 1998 and TAI
 * - UTC 31 seconds.
 *
 * In addition to the above, the reference timecode is updated and
 * written to the clockstats file and debug score after the last burst
 * received in the minute. The format is
 *
 *	qq yyyy ddd hh:mm:ss nn dd tt
 *
 * where qq are the error flags, as described below, yyyy is the year,
 * ddd the day, hh:mm:ss the time of day, nn the number of format A
 * bursts received during the previous minute, dd the decoding distance
 * and tt the number of timestamps. The error flags are cleared after
 * every update.
 *
 * Fudge factors
 *
 * For accuracies better than the low millisceconds, fudge time1 can be
 * set to the radio propagation delay from CHU to the receiver. This can
 * be done conviently using the minimuf program. When the modem driver
 * is compiled, fudge flag3 enables the ppsclock line discipline. Fudge
 * flag4 causes the dubugging output described above to be recorded in
 * the clockstats file.
 *
 * When the audio driver is compiled, fudge flag2 selects the audio
 * input port, where 0 is the mike port (default) and 1 is the line-in
 * port. It does not seem useful to select the compact disc player port.
 * Fudge flag3 enables audio monitoring of the input signal. For this
 * purpose, the speaker volume must be set before the driver is started.
 *
 * The ICOM code is normally compiled in the driver. It isn't used,
 * unless the mode keyword on the server configuration command specifies
 * a nonzero ICOM ID select code. The C-IV trace is turned on if the
 * debug level is greater than one.
 */
/*
 * Interface definitions
 */
#define	SPEED232	B300	/* uart speed (300 baud) */
#define	PRECISION	(-10)	/* precision assumed (about 1 ms) */
#define	REFID		"CHU"	/* reference ID */
#ifdef ICOM
#define DWELL		5	/* minutes before qsy */
#define NCHAN		3	/* number of channels */
#endif /* ICOM */
#ifdef AUDIO_CHU
#define	DESCRIPTION	"CHU Modem Receiver" /* WRU */

/*
 * Audio demodulator definitions
 */
#define SECOND		8000	/* nominal sample rate (Hz) */
#define BAUD		300	/* modulation rate (bps) */
#define OFFSET		128	/* companded sample offset */
#define SIZE		256	/* decompanding table size */
#define	MAXSIG		6000.	/* maximum signal level */
#define LIMIT		1000.	/* soft limiter threshold */
#define AGAIN		6.	/* baseband gain */
#define LAG		10	/* discriminator lag */
#else
#define	DEVICE		"/dev/chu%d" /* device name and unit */
#define	SPEED232	B300	/* UART speed (300 baud) */
#define	DESCRIPTION	"CHU Audio Receiver" /* WRU */
#endif /* AUDIO_CHU */

/*
 * Decoder definitions
 */
#define CHAR		(11. / 300.) /* character time (s) */
#define	FUDGE		.185	/* offset to first stop bit (s) */
#define BURST		11	/* max characters per burst */
#define MINCHAR		9	/* min characters per burst */
#define MINDIST		28	/* min burst distance (of 40)  */
#define MINSYNC		8	/* min sync distance (of 16) */
#define MINSTAMP	20	/* min timestamps (of 60) */
#define PANIC		(4 * 1440) /* panic restart */

/*
 * Hex extension codes (>= 16)
 */
#define HEX_MISS	16	/* miss */
#define HEX_SOFT	17	/* soft error */
#define HEX_HARD	18	/* hard error */

/*
 * Status bits (status)
 */
#define RUNT		0x0001	/* runt burst */
#define NOISE		0x0002	/* noise burst */
#define BFRAME		0x0004	/* invalid format B frame sync */
#define BFORMAT		0x0008	/* invalid format B data */
#define AFRAME		0x0010	/* invalid format A frame sync */
#define AFORMAT		0x0020	/* invalid format A data */
#define DECODE		0x0040	/* invalid data decode */
#define STAMP		0x0080	/* too few timestamps */
#define INYEAR		0x0100	/* valid B frame */
#define INSYNC		0x0200	/* clock synchronized */

/*
 * Alarm status bits (alarm)
 *
 * These alarms are set at the end of a minute in which at least one
 * burst was received. SYNERR is raised if the AFRAME or BFRAME status
 * bits are set during the minute, FMTERR is raised if the AFORMAT or
 * BFORMAT status bits are set, DECERR is raised if the DECODE status
 * bit is set and TSPERR is raised if the STAMP status bit is set.
 */
#define SYNERR		0x01	/* frame sync error */
#define FMTERR		0x02	/* data format error */
#define DECERR		0x04	/* data decoding error */
#define TSPERR		0x08	/* insufficient data */

#ifdef AUDIO_CHU
struct surv {
	double	shift[12];	/* mark register */
	double	es_max, es_min;	/* max/min envelope signals */
	double	dist;		/* sample distance */
	int	uart;		/* decoded character */
};
#endif /* AUDIO_CHU */

/*
 * CHU unit control structure
 */
struct chuunit {
	u_char	decode[20][16];	/* maximum likelihood decoding matrix */
	l_fp	cstamp[BURST];	/* character timestamps */
	l_fp	tstamp[MAXSTAGE]; /* timestamp samples */
	l_fp	timestamp;	/* current buffer timestamp */
	l_fp	laststamp;	/* last buffer timestamp */
	l_fp	charstamp;	/* character time as a l_fp */
	int	errflg;		/* error flags */
	int	status;		/* status bits */
	int	bufptr;		/* buffer index pointer */
	char	ident[10];	/* transmitter frequency */
#ifdef ICOM
	int	chan;		/* frequency identifier */
	int	dwell;		/* dwell minutes at current frequency */
	int	fd_icom;	/* ICOM file descriptor */
#endif /* ICOM */

	/*
	 * Character burst variables
	 */
	int	cbuf[BURST];	/* character buffer */
	int	ntstamp;	/* number of timestamp samples */
	int	ndx;		/* buffer start index */
	int	prevsec;	/* previous burst second */
	int	burdist;	/* burst distance */
	int	mindist;	/* minimum distance */
	int	syndist;	/* sync distance */
	int	burstcnt;	/* format A bursts this minute */

	/*
	 * Format particulars
	 */
	int	leap;		/* leap/dut code */
	int	dut;		/* UTC1 correction */
	int	tai;		/* TAI - UTC correction */
	int	dst;		/* Canadian DST code */

#ifdef AUDIO_CHU
	/*
	 * Audio codec variables
	 */
	double	comp[SIZE];	/* decompanding table */
	int	port;		/* codec port */
	int	gain;		/* codec gain */
	int	bufcnt;		/* samples in buffer */
	int	clipcnt;	/* sample clip count */
	int	seccnt;		/* second interval counter */

	/*
	 * Modem variables
	 */
	l_fp	tick;		/* audio sample increment */
	double	bpf[9];		/* IIR bandpass filter */
	double	disc[LAG];	/* discriminator shift register */
	double	lpf[27];	/* FIR lowpass filter */
	double	monitor;	/* audio monitor */
	double	maxsignal;	/* signal level */
	int	discptr;	/* discriminator pointer */

	/*
	 * Maximum likelihood UART variables
	 */
	double	baud;		/* baud interval */
	struct surv surv[8];	/* UART survivor structures */
	int	decptr;		/* decode pointer */
	int	dbrk;		/* holdoff counter */
#endif /* AUDIO_CHU */
};

/*
 * Function prototypes
 */
static	int	chu_start	P((int, struct peer *));
static	void	chu_shutdown	P((int, struct peer *));
static	void	chu_receive	P((struct recvbuf *));
static	void	chu_poll	P((int, struct peer *));

/*
 * More function prototypes
 */
static	void	chu_decode	P((struct peer *, int));
static	void	chu_burst	P((struct peer *));
static	void	chu_clear	P((struct peer *));
static	void	chu_a		P((struct peer *, int));
static	void	chu_b		P((struct peer *, int));
static	int	chu_dist	P((int, int));
static	int	chu_major	P((struct peer *));
#ifdef AUDIO_CHU
static	void	chu_uart	P((struct surv *, double));
static	void	chu_rf		P((struct peer *, double));
static	void	chu_gain	P((struct peer *));
#endif /* AUDIO_CHU */

/*
 * Global variables
 */
static char hexchar[] = "0123456789abcdef_-=";
#ifdef ICOM
static double qsy[NCHAN] = {3.33, 7.335, 14.67}; /* frequencies (MHz) */
#endif /* ICOM */

/*
 * Transfer vector
 */
struct	refclock refclock_chu = {
	chu_start,		/* start up driver */
	chu_shutdown,		/* shut down driver */
	chu_poll,		/* transmit poll message */
	noentry,		/* not used (old chu_control) */
	noentry,		/* initialize driver (not used) */
	noentry,		/* not used (old chu_buginfo) */
	NOFLAGS			/* not used */
};


/*
 * chu_start - open the devices and initialize data for processing
 */
static int
chu_start(
	int	unit,		/* instance number (not used) */
	struct peer *peer	/* peer structure pointer */
	)
{
	struct chuunit *up;
	struct refclockproc *pp;
	int	fd;		/* file descriptor */
#ifdef ICOM
	char	tbuf[80];	/* trace buffer */
	int	temp;
#endif /* ICOM */
#ifdef AUDIO_CHU
	int	i;		/* index */
	double	step;		/* codec adjustment */

	/*
	 * Open audio device
	 */
	fd = audio_init();
	if (fd < 0)
		return (0);
#ifdef DEBUG
	if (debug)
		audio_show();
#endif
#else
	char device[20];	/* device name */

	/*
	 * Open serial port in raw mode.
	 */
	(void)sprintf(device, DEVICE, unit);
	if (!(fd = refclock_open(device, SPEED232, LDISC_RAW))) {
		return (0);
	}
#endif /* AUDIO_CHU */

	/*
	 * Allocate and initialize unit structure
	 */
	if (!(up = (struct chuunit *)
	      emalloc(sizeof(struct chuunit)))) {
		(void) close(fd);
		return (0);
	}
	memset((char *)up, 0, sizeof(struct chuunit));
	pp = peer->procptr;
	pp->unitptr = (caddr_t)up;
	pp->io.clock_recv = chu_receive;
	pp->io.srcclock = (caddr_t)peer;
	pp->io.datalen = 0;
	pp->io.fd = fd;
	if (!io_addclock(&pp->io)) {
		(void)close(fd);
		free(up);
		return (0);
	}

	/*
	 * Initialize miscellaneous variables
	 */
	peer->precision = PRECISION;
	pp->clockdesc = DESCRIPTION;
	memcpy((char *)&pp->refid, REFID, 4);
	DTOLFP(CHAR, &up->charstamp);
#ifdef AUDIO_CHU
	up->gain = 127;

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
#endif /* AUDIO_CHU */
	strcpy(up->ident, "X");
#ifdef ICOM
	temp = 0;
#ifdef DEBUG
	if (debug > 1)
		temp = P_TRACE;
#endif
	if (peer->ttl > 0) {
		if (peer->ttl & 0x80)
			up->fd_icom = icom_init("/dev/icom", B1200,
			    temp);
		else
			up->fd_icom = icom_init("/dev/icom", B9600,
			    temp);
	}
	if (up->fd_icom > 0) {
		if (icom_freq(up->fd_icom, peer->ttl & 0x7f,
		    qsy[up->chan]) < 0) {
			NLOG(NLOG_SYNCEVENT | NLOG_SYSEVENT)
			    msyslog(LOG_ERR,
			    "ICOM bus error; autotune disabled");
			up->errflg = CEVNT_FAULT;
			close(up->fd_icom);
			up->fd_icom = 0;
		} else {
			sprintf(up->ident, "%.1f", qsy[up->chan]); 
			sprintf(tbuf, "chu: QSY to %s MHz", up->ident);
			record_clock_stats(&peer->srcadr, tbuf);
#ifdef DEBUG
			if (debug)
				printf("%s\n", tbuf);
#endif
		}
	}
#endif /* ICOM */
	return (1);
}


/*
 * chu_shutdown - shut down the clock
 */
static void
chu_shutdown(
	int	unit,		/* instance number (not used) */
	struct peer *peer	/* peer structure pointer */
	)
{
	struct chuunit *up;
	struct refclockproc *pp;

	pp = peer->procptr;
	up = (struct chuunit *)pp->unitptr;
	io_closeclock(&pp->io);
	if (up->fd_icom > 0)
		close(up->fd_icom);
	free(up);
}

#ifdef AUDIO_CHU

/*
 * chu_receive - receive data from the audio device
 */
static void
chu_receive(
	struct recvbuf *rbufp	/* receive buffer structure pointer */
	)
{
	struct chuunit *up;
	struct refclockproc *pp;
	struct peer *peer;

	double	sample;		/* codec sample */
	u_char	*dpt;		/* buffer pointer */
	l_fp	ltemp;		/* l_fp temp */
	int	isneg;		/* parity flag */
	double	dtemp;
	int	i, j;

	peer = (struct peer *)rbufp->recv_srcclock;
	pp = peer->procptr;
	up = (struct chuunit *)pp->unitptr;

	/*
	 * Main loop - read until there ain't no more. Note codec
	 * samples are bit-inverted.
	 */
	up->timestamp = rbufp->recv_time;
	up->bufcnt = rbufp->recv_length;
	DTOLFP(up->bufcnt * 1. / SECOND, &ltemp);
	L_SUB(&up->timestamp, &ltemp);
	dpt = (u_char *)&rbufp->recv_space;
	for (up->bufptr = 0; up->bufptr < up->bufcnt; up->bufptr++) {
		sample = up->comp[~*dpt & 0xff];

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
		up->seccnt = (up->seccnt + 1) % SECOND;
		if (up->seccnt == 0) {
			if (pp->sloppyclockflag & CLK_FLAG2)
				up->port = 2;
			else
				up->port = 1;
			chu_gain(peer);
		}
		chu_rf(peer, sample);

		/*
		 * During development, it is handy to have an audio
		 * monitor that can be switched to various signals. This
		 * code converts the linear signal left in up->monitor
		 * to codec format. If we can get the grass out of this
		 * thing and improve modem performance, this expensive
		 * code will be permanently nixed.
		 */
		isneg = 0;
		dtemp = up->monitor;
		if (sample < 0) {
			isneg = 1;
			dtemp-= dtemp;
		}
		i = 0;
		j = OFFSET >> 1;
		while (j != 0) {
			if (dtemp > up->comp[i])
				i += j;
			else if (dtemp < up->comp[i])
				i -= j;
			else
				break;
			j >>= 1;
		}
		if (isneg)
			*dpt = ~(i + OFFSET);
		else
			*dpt = ~i;
		dpt++;
		L_ADD(&up->timestamp, &up->tick);
	}
	
	/*
	 * Squawk to the monitor speaker if enabled.
	 */
	if (pp->sloppyclockflag & CLK_FLAG3)
		if (write(pp->io.fd, (u_char *)&rbufp->recv_space,
		    (u_int)up->bufcnt) < 0)
			perror("chu:");
}


/*
 * chu_rf - filter and demodulate the FSK signal
 *
 * This routine implements a 300-baud Bell 103 modem with mark 2225 Hz
 * and space 2025 Hz. It uses a bandpass filter followed by a soft
 * limiter, FM discriminator and lowpass filter. A maximum likelihood
 * decoder samples the baseband signal at eight times the baud rate and
 * detects the start bit of each character.
 *
 * The filters are built for speed, which explains the rather clumsy
 * code. Hopefully, the compiler will efficiently implement the move-
 * and-muiltiply-and-add operations.
 */
static void
chu_rf(
	struct peer *peer,	/* peer structure pointer */
	double	sample		/* analog sample */
	)
{
	struct refclockproc *pp;
	struct chuunit *up;
	struct surv *sp;

	/*
	 * Local variables
	 */
	double	signal;		/* bandpass signal */
	double	limit;		/* limiter signal */
	double	disc;		/* discriminator signal */
	double	lpf;		/* lowpass signal */
	double	span;		/* UART signal span */
	double	dist;		/* UART signal distance */
	int	i, j;

	pp = peer->procptr;
	up = (struct chuunit *)pp->unitptr;

	/*
	 * Bandpass filter. 4th-order elliptic, 500-Hz bandpass centered
	 * at 2125 Hz. Passband ripple 0.3 dB, stopband ripple 50 dB.
	 */
	signal = (up->bpf[8] = up->bpf[7]) * 5.844676e-01;
	signal += (up->bpf[7] = up->bpf[6]) * 4.884860e-01;
	signal += (up->bpf[6] = up->bpf[5]) * 2.704384e+00;
	signal += (up->bpf[5] = up->bpf[4]) * 1.645032e+00;
	signal += (up->bpf[4] = up->bpf[3]) * 4.644557e+00;
	signal += (up->bpf[3] = up->bpf[2]) * 1.879165e+00;
	signal += (up->bpf[2] = up->bpf[1]) * 3.522634e+00;
	signal += (up->bpf[1] = up->bpf[0]) * 7.315738e-01;
	up->bpf[0] = sample - signal;
	signal = up->bpf[0] * 6.176213e-03
	    + up->bpf[1] * 3.156599e-03
	    + up->bpf[2] * 7.567487e-03
	    + up->bpf[3] * 4.344580e-03
	    + up->bpf[4] * 1.190128e-02
	    + up->bpf[5] * 4.344580e-03
	    + up->bpf[6] * 7.567487e-03
	    + up->bpf[7] * 3.156599e-03
	    + up->bpf[8] * 6.176213e-03;

	up->monitor = signal / 4.;	/* note monitor after filter */

	/*
	 * Soft limiter/discriminator. The 11-sample discriminator lag
	 * interval corresponds to three cycles of 2125 Hz, which
	 * requires the sample frequency to be 2125 * 11 / 3 = 7791.7
	 * Hz. The discriminator output varies +-0.5 interval for input
	 * frequency 2025-2225 Hz. However, we don't get to sample at
	 * this frequency, so the discriminator output is biased. Life
	 * at 8000 Hz sucks.
	 */
	limit = signal;
	if (limit > LIMIT)
		limit = LIMIT;
	else if (limit < -LIMIT)
		limit = -LIMIT;
	disc = up->disc[up->discptr] * -limit;
	up->disc[up->discptr] = limit;
	up->discptr = (up->discptr + 1 ) % LAG;
	if (disc >= 0)
		disc = sqrt(disc);
	else
		disc = -sqrt(-disc);

	/*
	 * Lowpass filter. Raised cosine, Ts = 1 / 300, beta = 0.1.
	 */
	lpf = (up->lpf[26] = up->lpf[25]) * 2.538771e-02;
	lpf += (up->lpf[25] = up->lpf[24]) * 1.084671e-01;
	lpf += (up->lpf[24] = up->lpf[23]) * 2.003159e-01;
	lpf += (up->lpf[23] = up->lpf[22]) * 2.985303e-01;
	lpf += (up->lpf[22] = up->lpf[21]) * 4.003697e-01;
	lpf += (up->lpf[21] = up->lpf[20]) * 5.028552e-01;
	lpf += (up->lpf[20] = up->lpf[19]) * 6.028795e-01;
	lpf += (up->lpf[19] = up->lpf[18]) * 6.973249e-01;
	lpf += (up->lpf[18] = up->lpf[17]) * 7.831828e-01;
	lpf += (up->lpf[17] = up->lpf[16]) * 8.576717e-01;
	lpf += (up->lpf[16] = up->lpf[15]) * 9.183463e-01;
	lpf += (up->lpf[15] = up->lpf[14]) * 9.631951e-01;
	lpf += (up->lpf[14] = up->lpf[13]) * 9.907208e-01;
	lpf += (up->lpf[13] = up->lpf[12]) * 1.000000e+00;
	lpf += (up->lpf[12] = up->lpf[11]) * 9.907208e-01;
	lpf += (up->lpf[11] = up->lpf[10]) * 9.631951e-01;
	lpf += (up->lpf[10] = up->lpf[9]) * 9.183463e-01;
	lpf += (up->lpf[9] = up->lpf[8]) * 8.576717e-01;
	lpf += (up->lpf[8] = up->lpf[7]) * 7.831828e-01;
	lpf += (up->lpf[7] = up->lpf[6]) * 6.973249e-01;
	lpf += (up->lpf[6] = up->lpf[5]) * 6.028795e-01;
	lpf += (up->lpf[5] = up->lpf[4]) * 5.028552e-01;
	lpf += (up->lpf[4] = up->lpf[3]) * 4.003697e-01;
	lpf += (up->lpf[3] = up->lpf[2]) * 2.985303e-01;
	lpf += (up->lpf[2] = up->lpf[1]) * 2.003159e-01;
	lpf += (up->lpf[1] = up->lpf[0]) * 1.084671e-01;
	lpf += up->lpf[0] = disc * 2.538771e-02;

	/*
	 * Maximum likelihood decoder. The UART updates each of the
	 * eight survivors and determines the span, slice level and
	 * tentative decoded character. Valid 11-bit characters are
	 * framed so that bit 1 and bit 11 (stop bits) are mark and bit
	 * 2 (start bit) is space. When a valid character is found, the
	 * survivor with maximum distance determines the final decoded
	 * character.
	 */
	up->baud += 1. / SECOND;
	if (up->baud > 1. / (BAUD * 8.)) {
		up->baud -= 1. / (BAUD * 8.);
		sp = &up->surv[up->decptr];
		span = sp->es_max - sp->es_min;
		up->maxsignal += (span - up->maxsignal) / 80.;
		if (up->dbrk > 0) {
			up->dbrk--;
		} else if ((sp->uart & 0x403) == 0x401 && span > 1000.)
		    {
			dist = 0;
			j = 0;
			for (i = 0; i < 8; i++) {
				if (up->surv[i].dist > dist) {
					dist = up->surv[i].dist;
					j = i;
				}
			}
			chu_decode(peer, (up->surv[j].uart >> 2) &
			    0xff);
			up->dbrk = 80;
		}
		up->decptr = (up->decptr + 1) % 8;
		chu_uart(sp, -lpf * AGAIN);
	}
}


/*
 * chu_uart - maximum likelihood UART
 *
 * This routine updates a shift register holding the last 11 envelope
 * samples. It then computes the slice level and span over these samples
 * and determines the tentative data bits and distance. The calling
 * program selects over the last eight survivors the one with maximum
 * distance to determine the decoded character.
 */
static void
chu_uart(
	struct surv *sp,	/* survivor structure pointer */
	double	sample		/* baseband signal */
	)
{
	double	es_max, es_min;	/* max/min envelope */
	double	slice;		/* slice level */
	double	dist;		/* distance */
	double	dtemp;
	int	i;

	/*
	 * Save the sample and shift right. At the same time, measure
	 * the maximum and minimum over all eleven samples.
	 */
	es_max = -1e6;
	es_min = 1e6;
	sp->shift[0] = sample;
	for (i = 11; i > 0; i--) {
		sp->shift[i] = sp->shift[i - 1];
		if (sp->shift[i] > es_max)
			es_max = sp->shift[i];
		if (sp->shift[i] < es_min)
			es_min = sp->shift[i];
	}

	/*
	 * Determine the slice level midway beteen the maximum and
	 * minimum and the span as the maximum less the minimum. Compute
	 * the distance on the assumption the first and last bits must
	 * be mark, the second space and the rest either mark or space.
	 */ 
	slice = (es_max + es_min) / 2.;
	dist = 0;
	sp->uart = 0;
	for (i = 1; i < 12; i++) {
		sp->uart <<= 1;
		dtemp = sp->shift[i];
		if (dtemp > slice)
			sp->uart |= 0x1;
		if (i == 1 || i == 11) {
			dist += dtemp - es_min;
		} else if (i == 10) {
			dist += es_max - dtemp;
		} else {
			if (dtemp > slice)
				dist += dtemp - es_min;
			else
				dist += es_max - dtemp;
		}
	}
	sp->es_max = es_max;
	sp->es_min = es_min;
	sp->dist = dist / (11 * (es_max - es_min));
}


#else /* AUDIO_CHU */
/*
 * chu_receive - receive data from the serial interface
 */
static void
chu_receive(
	struct recvbuf *rbufp	/* receive buffer structure pointer */
	)
{
	struct chuunit *up;
	struct refclockproc *pp;
	struct peer *peer;

	u_char	*dpt;		/* receive buffer pointer */

	peer = (struct peer *)rbufp->recv_srcclock;
	pp = peer->procptr;
	up = (struct chuunit *)pp->unitptr;

	/*
	 * Initialize pointers and read the timecode and timestamp.
	 */
	up->timestamp = rbufp->recv_time;
	dpt = (u_char *)&rbufp->recv_space;
	chu_decode(peer, *dpt);
}
#endif /* AUDIO_CHU */


/*
 * chu_decode - decode the data
 */
static void
chu_decode(
	struct peer *peer,	/* peer structure pointer */
	int	hexhex		/* data character */
	)
{
	struct refclockproc *pp;
	struct chuunit *up;

	l_fp	tstmp;		/* timestamp temp */
	double	dtemp;

	pp = peer->procptr;
	up = (struct chuunit *)pp->unitptr;

	/*
	 * If the interval since the last character is greater than the
	 * longest burst, process the last burst and start a new one. If
	 * the interval is less than this but greater than two
	 * characters, consider this a noise burst and reject it.
	 */
	tstmp = up->timestamp;
	if (L_ISZERO(&up->laststamp))
		up->laststamp = up->timestamp;
	L_SUB(&tstmp, &up->laststamp);
	up->laststamp = up->timestamp;
	LFPTOD(&tstmp, dtemp);
	if (dtemp > BURST * CHAR) {
		chu_burst(peer);
		up->ndx = 0;
	} else if (dtemp > 2.5 * CHAR) {
		up->ndx = 0;
	}

	/*
	 * Append the character to the current burst and append the
	 * timestamp to the timestamp list.
	 */
	if (up->ndx < BURST) {
		up->cbuf[up->ndx] = hexhex & 0xff;
		up->cstamp[up->ndx] = up->timestamp;
		up->ndx++;

	}
}


/*
 * chu_burst - search for valid burst format
 */
static void
chu_burst(
	struct peer *peer
	)
{
	struct chuunit *up;
	struct refclockproc *pp;

	int	i;

	pp = peer->procptr;
	up = (struct chuunit *)pp->unitptr;

	/*
	 * Correlate a block of five characters with the next block of
	 * five characters. The burst distance is defined as the number
	 * of bits that match in the two blocks for format A and that
	 * match the inverse for format B.
	 */
	if (up->ndx < MINCHAR) {
		up->status |= RUNT;
		return;
	}
	up->burdist = 0;
	for (i = 0; i < 5 && i < up->ndx - 5; i++)
		up->burdist += chu_dist(up->cbuf[i], up->cbuf[i + 5]);

	/*
	 * If the burst distance is at least MINDIST, this must be a
	 * format A burst; if the value is not greater than -MINDIST, it
	 * must be a format B burst. If the B burst is perfect, we
	 * believe it; otherwise, it is a noise burst and of no use to
	 * anybody.
	 */
	if (up->burdist >= MINDIST) {
		chu_a(peer, up->ndx);
	} else if (up->burdist <= -MINDIST) {
		chu_b(peer, up->ndx);
	} else {
		up->status |= NOISE;
		return;
	}

	/*
	 * If this is a valid burst, wait a guard time of ten seconds to
	 * allow for more bursts, then arm the poll update routine to
	 * process the minute. Don't do this if this is called from the
	 * timer interrupt routine.
	 */
	if (peer->outdate != current_time)
		peer->nextdate = current_time + 10;
}


/*
 * chu_b - decode format B burst
 */
static void
chu_b(
	struct peer *peer,
	int	nchar
	)
{
	struct	refclockproc *pp;
	struct	chuunit *up;

	u_char	code[11];	/* decoded timecode */
	char	tbuf[80];	/* trace buffer */
	l_fp	offset;		/* timestamp offset */
	int	i;

	pp = peer->procptr;
	up = (struct chuunit *)pp->unitptr;

	/*
	 * In a format B burst, a character is considered valid only if
	 * the first occurrence matches the last occurrence. The burst
	 * is considered valid only if all characters are valid; that
	 * is, only if the distance is 40. 
	 */
	sprintf(tbuf, "chuB %04x %2d %2d ", up->status, nchar,
	    -up->burdist);
	for (i = 0; i < nchar; i++)
		sprintf(&tbuf[strlen(tbuf)], "%02x",
		    up->cbuf[i]);
	if (pp->sloppyclockflag & CLK_FLAG4)
		record_clock_stats(&peer->srcadr, tbuf);
#ifdef DEBUG
	if (debug)
		printf("%s\n", tbuf);
#endif
	if (up->burdist > -40) {
		up->status |= BFRAME;
		return;
	}
	up->status |= INYEAR;

	/*
	 * Convert the burst data to internal format. If this succeeds,
	 * save the timestamps for later.
	 */
	for (i = 0; i < 5; i++) {
		code[2 * i] = hexchar[up->cbuf[i] & 0xf];
		code[2 * i + 1] = hexchar[(up->cbuf[i] >>
		    4) & 0xf];
	}
	if (sscanf((char *)code, "%1x%1d%4d%2d%2x", &up->leap, &up->dut,
	    &pp->year, &up->tai, &up->dst) != 5) {
		up->status |= BFORMAT;
		return;
	}
	if (up->leap & 0x8)
		up->dut = -up->dut;
	offset.l_ui = 31;
	offset.l_f = 0;
	for (i = 0; i < nchar && i < 10; i++) {
		up->tstamp[up->ntstamp] = up->cstamp[i];
		L_SUB(&up->tstamp[up->ntstamp], &offset);
		L_ADD(&offset, &up->charstamp);
		if (up->ntstamp < MAXSTAGE) 
			up->ntstamp++;
	}
}


/*
 * chu_a - decode format A burst
 */
static void
chu_a(
	struct peer *peer,
	int nchar
	)
{
	struct refclockproc *pp;
	struct chuunit *up;

	char	tbuf[80];	/* trace buffer */
	l_fp	offset;		/* timestamp offset */
	int	val;		/* distance */
	int	temp;
	int	i, j, k;

	pp = peer->procptr;
	up = (struct chuunit *)pp->unitptr;

	/*
	 * Determine correct burst phase. There are three cases
	 * corresponding to in-phase, one character early or one
	 * character late. These cases are distinguished by the position
	 * of the framing digits x6 at positions 0 and 5 and x3 at
	 * positions 4 and 9. The correct phase is when the distance
	 * relative to the framing digits is maximum. The burst is valid
	 * only if the maximum distance is at least MINSYNC.
	 */
	up->syndist = k = 0;
	val = -16;
	for (i = -1; i < 2; i++) {
		temp = up->cbuf[i + 4] & 0xf;
		if (i >= 0)
			temp |= (up->cbuf[i] & 0xf) << 4;
		val = chu_dist(temp, 0x63);
		temp = (up->cbuf[i + 5] & 0xf) << 4;
		if (i + 9 < nchar)
			temp |= up->cbuf[i + 9] & 0xf;
		val += chu_dist(temp, 0x63);
		if (val > up->syndist) {
			up->syndist = val;
			k = i;
		}
	}
	temp = (up->cbuf[k + 4] >> 4) & 0xf;
	if (temp > 9 || k + 9 >= nchar || temp != ((up->cbuf[k + 9] >>
	    4) & 0xf))
		temp = 0;
#ifdef AUDIO_CHU
	sprintf(tbuf, "chuA %04x %4.0f %2d %2d %2d %2d %1d ",
	    up->status, up->maxsignal, nchar, up->burdist, k,
	    up->syndist, temp);
#else
	sprintf(tbuf, "chuA %04x %2d %2d %2d %2d %1d ", up->status,
	    nchar, up->burdist, k, up->syndist, temp);
#endif /* AUDIO_CHU */
	for (i = 0; i < nchar; i++)
		sprintf(&tbuf[strlen(tbuf)], "%02x",
		    up->cbuf[i]);
	if (pp->sloppyclockflag & CLK_FLAG4)
		record_clock_stats(&peer->srcadr, tbuf);
#ifdef DEBUG
	if (debug)
		printf("%s\n", tbuf);
#endif
	if (up->syndist < MINSYNC) {
		up->status |= AFRAME;
		return;
	}

	/*
	 * A valid burst requires the first seconds number to match the
	 * last seconds number. If so, the burst timestamps are
	 * corrected to the current minute and saved for later
	 * processing. In addition, the seconds decode is advanced from
	 * the previous burst to the current one.
	 */
	if (temp != 0) {
		offset.l_ui = 30 + temp;
		offset.l_f = 0;
		i = 0;
		if (k < 0)
			offset = up->charstamp;
		else if (k > 0)
			i = 1;
		for (; i < nchar && i < k + 10; i++) {
			up->tstamp[up->ntstamp] = up->cstamp[i];
			L_SUB(&up->tstamp[up->ntstamp], &offset);
			L_ADD(&offset, &up->charstamp);
			if (up->ntstamp < MAXSTAGE) 
				up->ntstamp++;
		}
		while (temp > up->prevsec) {
			for (j = 15; j > 0; j--) {
				up->decode[9][j] = up->decode[9][j - 1];
				up->decode[19][j] =
				    up->decode[19][j - 1];
			}
			up->decode[9][j] = up->decode[19][j] = 0;
			up->prevsec++;
		}
	}
	i = -(2 * k);
	for (j = 0; j < nchar; j++) {
		if (i < 0 || i > 19) {
			i += 2;
			continue;
		}
		up->decode[i][up->cbuf[j] & 0xf]++;
		i++;
		up->decode[i][(up->cbuf[j] >> 4) & 0xf]++;
		i++;
	}
	up->burstcnt++;
}


/*
 * chu_poll - called by the transmit procedure
 */
static void
chu_poll(
	int unit,
	struct peer *peer	/* peer structure pointer */
	)
{
	struct refclockproc *pp;
	struct chuunit *up;
	char	synchar, qual, leapchar;
	int	minset;
	int	temp;
#ifdef ICOM
	char	tbuf[80];	/* trace buffer */
#endif /* ICOM */
	pp = peer->procptr;
	up = (struct chuunit *)pp->unitptr;
	if (pp->coderecv == pp->codeproc)
		up->errflg = CEVNT_TIMEOUT;
	else
		pp->polls++;
	minset = ((current_time - peer->update) + 30) / 60;
	if (up->status & INSYNC) {
		if (minset > PANIC)
			up->status = 0;
		else
			peer->reach |= 1;
	}

	/*
	 * Process the last burst, if still in the burst buffer.
	 * Don't mess with anything if nothing has been heard.
	 */
	chu_burst(peer);
#ifdef ICOM
	if (up->burstcnt > 2) {
		up->dwell = 0;
	} else if (up->dwell < DWELL) {
		up->dwell++;
	} else if (up->fd_icom > 0) {
		up->dwell = 0;
		up->chan = (up->chan + 1) % NCHAN;
		icom_freq(up->fd_icom, peer->ttl & 0x7f, qsy[up->chan]);
		sprintf(up->ident, "%.3f", qsy[up->chan]); 
		sprintf(tbuf, "chu: QSY to %s MHz", up->ident);
		record_clock_stats(&peer->srcadr, tbuf);
#ifdef DEBUG
		if (debug)
			printf("%s\n", tbuf);
#endif
	}
#endif /* ICOM */
	if (up->burstcnt == 0)
		return;
	temp = chu_major(peer);
	if (up->status & INYEAR)
		up->status |= INSYNC;
	qual = 0;
	if (up->status & (BFRAME | AFRAME))
		qual |= SYNERR;
	if (up->status & (BFORMAT | AFORMAT))
		qual |= FMTERR;
	if (up->status & DECODE)
		qual |= DECERR;
	if (up->status & STAMP)
		qual |= TSPERR;
	synchar = leapchar = ' ';
	if (!(up->status & INSYNC)) {
		pp->leap = LEAP_NOTINSYNC;
		synchar = '?';
	} else if (up->leap & 0x2) {
		pp->leap = LEAP_ADDSECOND;
		leapchar = 'L';
	} else {
		pp->leap = LEAP_NOWARNING;
	}
#ifdef AUDIO_CHU
	sprintf(pp->a_lastcode,
	    "%c%1X %4d %3d %02d:%02d:%02d.000 %c%x %+d %d %d %s %d %d %d %d",
	    synchar, qual, pp->year, pp->day, pp->hour, pp->minute,
	    pp->second, leapchar, up->dst, up->dut, minset, up->gain,
	    up->ident, up->tai, up->burstcnt, up->mindist, up->ntstamp);
#else
	sprintf(pp->a_lastcode,
	    "%c%1X %4d %3d %02d:%02d:%02d.000 %c%x %+d %d %s %d %d %d %d",
	    synchar, qual, pp->year, pp->day, pp->hour, pp->minute,
	    pp->second, leapchar, up->dst, up->dut, minset,
	    up->ident, up->tai, up->burstcnt, up->mindist, up->ntstamp);
#endif /* AUDIO_CHU */
	pp->lencode = strlen(pp->a_lastcode);

	/*
	 * If timestamps have been stuffed, the timecode is ipso fatso
	 * correct and can be selected to discipline the clock.
	 */
	if (temp > 0) {
		record_clock_stats(&peer->srcadr, pp->a_lastcode);
		refclock_receive(peer);
	} else if (pp->sloppyclockflag & CLK_FLAG4) {
		record_clock_stats(&peer->srcadr, pp->a_lastcode);
	}
#ifdef DEBUG
	if (debug)
		printf("chu: timecode %d %s\n", pp->lencode,
		    pp->a_lastcode);
#endif
	chu_clear(peer);
	if (up->errflg)
		refclock_report(peer, up->errflg);
	up->errflg = 0;
}


/*
 * chu_major - majority decoder
 */
static int
chu_major(
	struct peer *peer	/* peer structure pointer */
	)
{
	struct refclockproc *pp;
	struct chuunit *up;

	u_char	code[11];	/* decoded timecode */
	l_fp	toffset, offset; /* l_fp temps */
	int	val1, val2;	/* maximum distance */
	int	synchar;	/* stray cat */
	double	dtemp;
	int	temp;
	int	i, j, k;

	pp = peer->procptr;
	up = (struct chuunit *)pp->unitptr;

	/*
	 * Majority decoder. Each burst encodes two replications at each
	 * digit position in the timecode. Each row of the decoding
	 * matrix encodes the number of occurences of each digit found
	 * at the corresponding position. The maximum over all
	 * occurences at each position is the distance for this position
	 * and the corresponding digit is the maximumn likelihood
	 * candidate. If the distance is zero, assume a miss '_'; if the
	 * distance is not more than half the total number of
	 * occurences, assume a soft error '-'; if two different digits
	 * with the same distance are found, assume a hard error '='.
	 * These will later cause a format error when the timecode is
	 * interpreted. The decoding distance is defined as the minimum
	 * distance over the first nine digits. The tenth digit varies
	 * over the seconds, so we don't count it.
	 */
	up->mindist = 16;
	for (i = 0; i < 9; i++) {
		val1 = val2 = 0;
		k = 0;
		for (j = 0; j < 16; j++) {
			temp = up->decode[i][j] + up->decode[i + 10][j];
			if (temp > val1) {
				val2 = val1;
				val1 = temp;
				k = j;
			}
		}
		if (val1 == 0)
			code[i] = HEX_MISS;
		else if (val1 == val2)
			code[i] = HEX_HARD;
		else if (val1 <= up->burstcnt)
			code[i] = HEX_SOFT;
		else
			code[i] = k;
		if (val1 < up->mindist)
			up->mindist = val1;
		code[i] = hexchar[code[i]];
	}
	code[i] = 0;

	/*
	 * A valid timecode requires at least three bursts and a
	 * decoding distance greater than half the total number of
	 * occurences. A valid timecode also requires at least 20 valid
	 * timestamps.
	 */
	if (up->burstcnt < 3 || up->mindist <= up->burstcnt)
		up->status |= DECODE;
	if (up->ntstamp < MINSTAMP)
		up->status |= STAMP;

	/*
	 * Compute the timecode timestamp from the days, hours and
	 * minutes of the timecode. Use clocktime() for the aggregate
	 * minutes and the minute offset computed from the burst
	 * seconds. Note that this code relies on the filesystem time
	 * for the years and does not use the years of the timecode.
	 */
	if (sscanf((char *)code, "%1x%3d%2d%2d", &synchar, &pp->day,
	    &pp->hour, &pp->minute) != 4) {
		up->status |= AFORMAT;
		return (0);
	}
	if (up->status & (DECODE | STAMP)) {
		up->errflg = CEVNT_BADREPLY;
		return (0);
	}
	L_CLR(&offset);
	if (!clocktime(pp->day, pp->hour, pp->minute, 0, GMT,
	    up->tstamp[0].l_ui, &pp->yearstart, &offset.l_ui)) {
		up->errflg = CEVNT_BADTIME;
		return (0);
	}
	pp->lastref = offset;
	pp->variance = 0;
	for (i = 0; i < up->ntstamp; i++) {
		toffset = offset;
		L_SUB(&toffset, &up->tstamp[i]);
		LFPTOD(&toffset, dtemp);
		SAMPLE(dtemp + FUDGE + pp->fudgetime1);
	}
	return (i);
}


/*
 * chu_clear - clear decoding matrix
 */
static void
chu_clear(
	struct peer *peer	/* peer structure pointer */
	)
{
	struct refclockproc *pp;
	struct chuunit *up;
	int	i, j;

	pp = peer->procptr;
	up = (struct chuunit *)pp->unitptr;

	/*
	 * Clear stuff for the minute.
	 */
	up->ndx = up->prevsec = 0;
	up->burstcnt = up->mindist = up->ntstamp = 0;
	up->status &= INSYNC | INYEAR;
	up->burstcnt = 0;
	for (i = 0; i < 20; i++) {
		for (j = 0; j < 16; j++)
			up->decode[i][j] = 0;
	}
}


/*
 * chu_dist - determine the distance of two octet arguments
 */
static int
chu_dist(
	int	x,		/* an octet of bits */
	int	y		/* another octet of bits */
	)
{
	int	val;		/* bit count */ 
	int	temp;
	int	i;

	/*
	 * The distance is determined as the weight of the exclusive OR
	 * of the two arguments. The weight is determined by the number
	 * of one bits in the result. Each one bit increases the weight,
	 * while each zero bit decreases it.
	 */
	temp = x ^ y;
	val = 0;
	for (i = 0; i < 8; i++) {
		if ((temp & 0x1) == 0)
			val++;
		else
			val--;
		temp >>= 1;
	}
	return (val);
}


#ifdef AUDIO_CHU
/*
 * chu_gain - adjust codec gain
 *
 * This routine is called once each second. If the signal envelope
 * amplitude is too low, the codec gain is bumped up by four units; if
 * too high, it is bumped down. The decoder is relatively insensitive to
 * amplitude, so this crudity works just fine. The input port is set and
 * the error flag is cleared, mostly to be ornery.
 */
static void
chu_gain(
	struct peer *peer	/* peer structure pointer */
	)
{
	struct refclockproc *pp;
	struct chuunit *up;

	pp = peer->procptr;
	up = (struct chuunit *)pp->unitptr;

	/*
	 * Apparently, the codec uses only the high order bits of the
	 * gain control field. Thus, it may take awhile for changes to
	 * wiggle the hardware bits.
	 */
	if (up->clipcnt == 0) {
		up->gain += 4;
		if (up->gain > 255)
			up->gain = 255;
	} else if (up->clipcnt > SECOND / 100) {
		up->gain -= 4;
		if (up->gain < 0)
			up->gain = 0;
	}
	audio_gain(up->gain, up->port);
	up->clipcnt = 0;
}
#endif /* AUDIO_CHU */


#else
int refclock_chu_bs;
#endif /* REFCLOCK */
