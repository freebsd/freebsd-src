/*
 * refclock_chu - clock driver for Canadian radio CHU receivers
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#if defined(REFCLOCK) && defined(CLOCK_CHU)

/* #define AUDIO_CHUa */

#include <stdio.h>
#include <ctype.h>
#include <sys/time.h>
#include <time.h>
#include <math.h>

#ifdef AUDIO_CHU
#ifdef HAVE_SYS_AUDIOIO_H
#include <sys/audioio.h>
#endif /* HAVE_SYS_AUDIOIO_H */
#ifdef HAVE_SUN_AUDIOIO_H
#include <sun/audioio.h>
#endif /* HAVE_SUN_AUDIOIO_H */
#endif /* AUDIO_CHU */

#include "ntpd.h"
#include "ntp_io.h"
#include "ntp_refclock.h"
#include "ntp_calendar.h"
#include "ntp_stdlib.h"

/*
 * Clock driver for Canadian radio CHU receivers
 *
 * This driver synchronizes the computer time using data encoded in
 * radio transmissions from Canadian time/frequency station CHU in
 * Ottawa, Ontario. Transmissions are made continuously on 3330 kHz,
 * 7335 kHz and 14670 kHz in upper sideband, compatible AM mode. An
 * ordinary shortwave receiver can be tuned manually to one of these
 * frequencies or, in the case of ICOM receivers, the receiver can be
 * tuned automatically using the minimuf and icom programs as
 * propagation conditions change throughout the day and night.
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
 */

/*
 * Interface definitions
 */
#define	SPEED232	B300	/* uart speed (300 baud) */
#define	PRECISION	(-10)	/* precision assumed (about 1 ms) */
#define	REFID		"CHU"	/* reference ID */
#ifdef AUDIO_CHU
#define	DESCRIPTION	"CHU Modem Receiver" /* WRU */

/*
 * Audio demodulator definitions
 */
#define AUDIO_BUFSIZ	160	/* codec buffer size (Solaris only) */
#define SAMPLE		8000	/* nominal sample rate (Hz) */
#define BAUD		300	/* modulation rate (bps) */
#define OFFSET		128	/* companded sample offset */
#define SIZE		256	/* decompanding table size */
#define	MAXSIG		6000.	/* maximum signal level */
#define DRPOUT		100.	/* dropout signal level */
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
#define MINDEC		.5	/* decoder majority rule (of 1.) */
#define MINSTAMP	20	/* min timestamps (of 60) */

/*
 * Hex extension codes (>= 16)
 */
#define HEX_MISS	16	/* miss */
#define HEX_SOFT	17	/* soft error */
#define HEX_HARD	18	/* hard error */

/*
 * Error flags (up->errflg)
 */
#define CHU_ERR_RUNT	0x001	/* runt burst */
#define CHU_ERR_NOISE	0x002	/* noise burst */
#define CHU_ERR_BFRAME	0x004	/* invalid format B frame sync */
#define CHU_ERR_BFORMAT	0x008	/* invalid format B data */
#define CHU_ERR_AFRAME	0x010	/* invalid format A frame sync */
#define CHU_ERR_DECODE	0x020	/* invalid data decode */
#define CHU_ERR_STAMP	0x040	/* too few timestamps */
#define CHU_ERR_AFORMAT	0x080	/* invalid format A data */
#ifdef AUDIO_CHU
#define CHU_ERR_ERROR	0x100	/* codec error (overrun) */
#endif /* AUDIO_CHU */

#ifdef AUDIO_CHU
struct surv {
	double	shift[12];	/* mark register */
	double	max, min;	/* max/min envelope signals */
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
	int	bufptr;		/* buffer index pointer */
	int	pollcnt;	/* poll message counter */

	/*
	 * Character burst variables
	 */
	int	cbuf[BURST];	/* character buffer */
	int	ntstamp;	/* number of timestamp samples */
	int	ndx;		/* buffer start index */
	int	prevsec;	/* previous burst second */
	int	burdist;	/* burst distance */
	int	syndist;	/* sync distance */
	int	burstcnt;	/* format A bursts this minute */

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
static	void	chu_update	P((struct peer *, int));
static	void	chu_year	P((struct peer *, int));
static	int	chu_dist	P((int, int));
#ifdef AUDIO_CHU
static	void	chu_uart	P((struct surv *, double));
static	void	chu_rf		P((struct peer *, double));
static	void	chu_gain	P((struct peer *));
static	int	chu_audio	P((void));
static	void	chu_debug	P((void));
#endif /* AUDIO_CHU */

/*
 * Global variables
 */
static char hexchar[] = "0123456789abcdef_-=";
#ifdef AUDIO_CHU
#ifdef HAVE_SYS_AUDIOIO_H
struct	audio_device device;	/* audio device ident */
#endif /* HAVE_SYS_AUDIOIO_H */
static struct audio_info info;	/* audio device info */
static int	chu_ctl_fd;	/* audio control file descriptor */
#endif /* AUDIO_CHU */

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

	/*
	 * Local variables
	 */
	int	fd;		/* file descriptor */
#ifdef AUDIO_CHU
	int	i;		/* index */
	double	step;		/* codec adjustment */

	/*
	 * Open audio device
	 */
	fd = open("/dev/audio", O_RDWR | O_NONBLOCK, 0777);
	if (fd == -1) {
		perror("chu: audio");
		return (0);
	}
#else
	char device[20];	/* device name */

	/*
	 * Open serial port. Use RAW line discipline (required).
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
	up->pollcnt = 2;
#ifdef AUDIO_CHU
	up->gain = (AUDIO_MAX_GAIN - AUDIO_MIN_GAIN) / 2;
	if (chu_audio() < 0) {
		io_closeclock(&pp->io);
		free(up);
		return (0);
	}

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
	DTOLFP(1. / SAMPLE, &up->tick);
#endif /* AUDIO_CHU */
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

	/*
	 * Local variables
	 */
	double	sample;		/* codec sample */
	u_char	*dpt;		/* buffer pointer */
	l_fp	ltemp;		/* l_fp temp */
	double	dtemp;		/* double temp */
	int	isneg;		/* parity flag */
	int	i, j;		/* index temps */

	peer = (struct peer *)rbufp->recv_srcclock;
	pp = peer->procptr;
	up = (struct chuunit *)pp->unitptr;

	/*
	 * Main loop - read until there ain't no more. Note codec
	 * samples are bit-inverted.
	 */
	up->timestamp = rbufp->recv_time;
	up->bufcnt = rbufp->recv_length;
	DTOLFP(up->bufcnt * 1. / SAMPLE, &ltemp);
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
		up->seccnt = (up->seccnt + 1) % SAMPLE;
		if (up->seccnt == 0) {
			if (pp->sloppyclockflag & CLK_FLAG2)
				up->port = AUDIO_LINE_IN;
			else
				up->port = AUDIO_MICROPHONE;
			chu_gain(peer);
			up->clipcnt = 0;
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
void
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
	int	i, j;		/* index temps */

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
printf("%8.3f %8.3f\n", disc, lpf);
return;
*/
	/*
	 * Maximum likelihood decoder. The UART updates each of the
	 * eight survivors and determines the span, slice level and
	 * tentative decoded character. Valid 11-bit characters are
	 * framed so that bit 1 and bit 11 (stop bits) are mark and bit
	 * 2 (start bit) is space. When a valid character is found, the
	 * survivor with maximum distance determines the final decoded
	 * character.
	 */
	up->baud += 1. / SAMPLE;
	if (up->baud > 1. / (BAUD * 8.)) {
		up->baud -= 1. / (BAUD * 8.);
		sp = &up->surv[up->decptr];
		span = sp->max - sp->min;
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
void
chu_uart(
	struct surv *sp,	/* survivor structure pointer */
	double	sample		/* baseband signal */
	)
{
	/*
	 * Local variables
	 */
	double	max, min;	/* max/min envelope */
	double	slice;		/* slice level */
	double	dist;		/* distance */
	double	dtemp;		/* double temp */
	int	i;		/* index temp */

	/*
	 * Save the sample and shift right. At the same time, measure
	 * the maximum and minimum over all eleven samples.
	 */
	max = -1e6;
	min = 1e6;
	sp->shift[0] = sample;
	for (i = 11; i > 0; i--) {
		sp->shift[i] = sp->shift[i - 1];
		if (sp->shift[i] > max)
			max = sp->shift[i];
		if (sp->shift[i] < min)
			min = sp->shift[i];
	}

	/*
	 * Determine the slice level midway beteen the maximum and
	 * minimum and the span as the maximum less the minimum. Compute
	 * the distance on the assumption the first and last bits must
	 * be mark, the second space and the rest either mark or space.
	 */ 
	slice = (max + min) / 2.;
	dist = 0;
	sp->uart = 0;
	for (i = 1; i < 12; i++) {
		sp->uart <<= 1;
		dtemp = sp->shift[i];
		if (dtemp > slice)
			sp->uart |= 0x1;
		if (i == 1 || i == 11) {
			dist += dtemp - min;
		} else if (i == 10) {
			dist += max - dtemp;
		} else {
			if (dtemp > slice)
				dist += dtemp - min;
			else
				dist += max - dtemp;
		}
	}
	sp->max = max;
	sp->min = min;
	sp->dist = dist / (11 * (max - min));
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

	/*
	 * Local variables
	 */
	l_fp	tstmp;		/* timestamp temp */
	double	dtemp;		/* double temp */

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

	/*
	 * Local variables
	 */
	int	i;		/* index temp */

	pp = peer->procptr;
	up = (struct chuunit *)pp->unitptr;

	/*
	 * Correlate a block of five characters with the next block of
	 * five characters. The burst distance is defined as the number
	 * of bits that match in the two blocks for format A and that
	 * match the inverse for format B.
	 */
	if (up->ndx < MINCHAR) {
		up->errflg |= CHU_ERR_RUNT;
		return;
	}
	up->burdist = 0;
	for (i = 0; i < 5 && i < up->ndx - 5; i++)
		up->burdist += chu_dist(up->cbuf[i], up->cbuf[i + 5]);

	/*
	 * If the burst distance is at least MINDIST, this must be a
	 * format A burst; if the value is not greater than -MINDIST, it
	 * must be a format B burst; otherwise, it is a noise burst and
	 * of no use to anybody.
	 */
	if (up->burdist >= MINDIST) {
		chu_update(peer, up->ndx);
	} else if (up->burdist <= -MINDIST) {
		chu_year(peer, up->ndx);
	} else {
		up->errflg |= CHU_ERR_NOISE;
		return;
	}

	/*
	 * If this is a valid burst, wait a guard time of ten seconds to
	 * allow for more bursts, then arm the poll update routine to
	 * process the minute. Don't do this if this is called from the
	 * timer interrupt routine.
	 */
	if (peer->outdate == current_time)
		up->pollcnt = 2;
	else
		peer->nextdate = current_time + 10;
}


/*
 * chu_year - decode format B burst
 */
static void
chu_year(
	struct peer *peer,
	int	nchar
	)
{
	struct	refclockproc *pp;
	struct	chuunit *up;

	/*
	 * Local variables
	 */
	u_char	code[11];	/* decoded timecode */
	l_fp	offset;		/* timestamp offset */
	int	leap;		/* leap/dut code */
	int	dut;		/* UTC1 correction */
	int	tai;		/* TAI - UTC correction */
	int	dst;		/* Canadian DST code */
	int	i;		/* index temp */

	pp = peer->procptr;
	up = (struct chuunit *)pp->unitptr;

	/*
	 * In a format B burst, a character is considered valid only if
	 * the first occurrence matches the last occurrence. The burst
	 * is considered valid only if all characters are valid; that
	 * is, only if the distance is 40. 
	 */
	sprintf(pp->a_lastcode, "%2d %2d ", nchar, -up->burdist);
	for (i = 0; i < nchar; i++)
		sprintf(&pp->a_lastcode[strlen(pp->a_lastcode)], "%02x",
		    up->cbuf[i]);
	pp->lencode = strlen(pp->a_lastcode);
	if (pp->sloppyclockflag & CLK_FLAG4)
		record_clock_stats(&peer->srcadr, pp->a_lastcode);
#ifdef DEBUG
	if (debug > 2)
		printf("chu: %s\n", pp->a_lastcode);
#endif
	if (-up->burdist < 40) {
		up->errflg |= CHU_ERR_BFRAME;
		return;
	}

	/*
	 * Convert the burst data to internal format. If this succeeds,
	 * save the timestamps for later. The leap, dut, tai and dst are
	 * presently unused.
	 */
	for (i = 0; i < 5; i++) {
		code[2 * i] = hexchar[up->cbuf[i] & 0xf];
		code[2 * i + 1] = hexchar[(up->cbuf[i] >>
		    4) & 0xf];
	}
	if (sscanf((char *)code, "%1x%1d%4d%2d%2x", &leap, &dut,
	    &pp->year, &tai, &dst) != 5) {
		up->errflg |= CHU_ERR_BFORMAT;
		return;
	}
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
 * chu_update - decode format A burst
 */
static void
chu_update(
	struct peer *peer,
	int nchar
	)
{
	struct refclockproc *pp;
	struct chuunit *up;

	/*
	 * Local variables
	 */
	l_fp	offset;		/* timestamp offset */
	int	val;		/* distance */
	int	temp;		/* common temp */
	int	i, j, k;	/* index temps */

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
	sprintf(pp->a_lastcode, "%3d %4.0f %2d %2d %2d %2d %1d ",
	    up->gain, up->maxsignal, nchar, up->burdist, k, up->syndist,
	    temp);
#else
	sprintf(pp->a_lastcode, "%2d %2d %2d %2d %1d ", nchar,
	    up->burdist, k, up->syndist, temp);
#endif /* AUDIO_CHU */
	for (i = 0; i < nchar; i++)
		sprintf(&pp->a_lastcode[strlen(pp->a_lastcode)], "%02x",
		    up->cbuf[i]);
	pp->lencode = strlen(pp->a_lastcode);
	if (pp->sloppyclockflag & CLK_FLAG4)
		record_clock_stats(&peer->srcadr, pp->a_lastcode);
#ifdef DEBUG
	if (debug > 2)
		printf("chu: %s\n", pp->a_lastcode);
#endif
	if (up->syndist < MINSYNC) {
		up->errflg |= CHU_ERR_AFRAME;
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
		up->decode[i++][up->cbuf[j] & 0xf]++;
		up->decode[i++][(up->cbuf[j] >> 4) & 0xf]++;
	}
	up->burstcnt++;
}


/*
 * chu_poll - called by the transmit procedure
 */
static void
chu_poll(
	int unit,
	struct peer *peer
	)
{
	struct refclockproc *pp;
	struct chuunit *up;

	/*
	 * Local variables
	 */
	u_char	code[11];	/* decoded timecode */
	l_fp	toffset, offset; /* l_fp temps */
	int	mindist;	/* minimum distance */
	int	val1, val2;	/* maximum distance */
	int	synchar;	/* should be a 6 in traffic */
	double	dtemp;		/* double temp */
	int	temp;		/* common temp */
	int	i, j, k;	/* index temps */

	pp = peer->procptr;
	up = (struct chuunit *)pp->unitptr;

	/*
	 * Process the last burst, if still in the burst buffer.
	 * Don't mess with anything if nothing has been heard.
	 */
	chu_burst(peer);
	if (up->pollcnt == 0)
		refclock_report(peer, CEVNT_TIMEOUT);
	else
		up->pollcnt--;
	if (up->burstcnt == 0) {
		chu_clear(peer);
		return;
	}

	/*
	 * Majority decoder. Select the character with the most
	 * occurrences for each burst position. The distance for the
	 * character is this number of occurrences. If no occurrences
	 * are found, assume a miss '_'; if only a single occurrence is
	 * found, assume a soft error '-'; if two different characters
	 * with the same distance are found, assume a hard error '='.
	 * The decoding distance is defined as the minimum of the
	 * character distances.
	 */
	mindist = 16;
	for (i = 0; i < 10; i++) {
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
		if (val1 > 0 && val1 == val2)
			code[i] = HEX_HARD;
		else if (val1 < 2)
			code[i] = HEX_SOFT;
		else
			code[i] = k;
		if (val1 < mindist)
			mindist = val1;
		code[i] = hexchar[code[i]];
	}
	code[i] = 0;
	if (mindist < up->burstcnt * 2 * MINDEC)
		up->errflg |= CHU_ERR_DECODE;
	if (up->ntstamp < MINSTAMP)
		up->errflg |= CHU_ERR_STAMP;

	/*
	 * Compute the timecode timestamp from the days, hours and
	 * minutes of the timecode. Use clocktime() for the aggregate
	 * minutes and the minute offset computed from the burst
	 * seconds. Note that this code relies on the filesystem time
	 * for the years and does not use the years of the timecode.
	 */
	if (sscanf((char *)code, "%1x%3d%2d%2d", &synchar, &pp->day, &pp->hour,
	    &pp->minute) != 4)
		up->errflg |= CHU_ERR_AFORMAT;
	sprintf(pp->a_lastcode,
	    "%02x %4d %3d %02d:%02d:%02d %2d %2d %2d",
	    up->errflg, pp->year, pp->day, pp->hour, pp->minute,
	    pp->second, up->burstcnt, mindist, up->ntstamp);
	pp->lencode = strlen(pp->a_lastcode);
	record_clock_stats(&peer->srcadr, pp->a_lastcode);
#ifdef DEBUG
	if (debug > 2)
		printf("chu: %s\n", pp->a_lastcode);
#endif
	if (up->errflg & (CHU_ERR_DECODE | CHU_ERR_STAMP |
	    CHU_ERR_AFORMAT)) {
		refclock_report(peer, CEVNT_BADREPLY);
		chu_clear(peer);
		return;
	}
	L_CLR(&offset);
	if (!clocktime(pp->day, pp->hour, pp->minute, 0, GMT,
	    up->tstamp[0].l_ui, &pp->yearstart, &offset.l_ui)) {
		refclock_report(peer, CEVNT_BADTIME);
		chu_clear(peer);
		return;
	}
	pp->polls++;
	pp->leap = LEAP_NOWARNING;
	pp->lastref = offset;
	pp->variance = 0;
	for (i = 0; i < up->ntstamp; i++) {
		toffset = offset;
		L_SUB(&toffset, &up->tstamp[i]);
		LFPTOD(&toffset, dtemp);
		SAMPLE(dtemp + FUDGE + pp->fudgetime1);
	}
	if (i > 0)
		refclock_receive(peer);
	chu_clear(peer);
}


/*
 * chu_clear - clear decoding matrix
 */
static void
chu_clear(
	struct peer *peer
	)
{
	struct refclockproc *pp;
	struct chuunit *up;

	/*
	 * Local variables
	 */
	int	i, j;		/* index temps */

	pp = peer->procptr;
	up = (struct chuunit *)pp->unitptr;

	/*
	 * Clear stuff for following minute.
	 */
	up->ndx = up->ntstamp = up->prevsec = 0;
	up->errflg = 0;
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
	/*
	 * Local variables
	 */
	int	val;		/* bit count */ 
	int	temp;		/* misc temporary */
	int	i;		/* index temporary */

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
	 * wiggle the hardware bits. Set the new bits in the structure
	 * and call AUDIO_SETINFO. Upon return, the old bits are in the
	 * structure.
	 */
	if (up->clipcnt == 0) {
		up->gain += 4;
		if (up->gain > AUDIO_MAX_GAIN)
			up->gain = AUDIO_MAX_GAIN;
	} else if (up->clipcnt > SAMPLE / 100) {
		up->gain -= 4;
		if (up->gain < AUDIO_MIN_GAIN)
			up->gain = AUDIO_MIN_GAIN;
	}
	AUDIO_INITINFO(&info);
	info.record.port = up->port;
	info.record.gain = up->gain;
	info.record.error = 0;
	ioctl(chu_ctl_fd, (int)AUDIO_SETINFO, &info);
	if (info.record.error)
		up->errflg |= CHU_ERR_ERROR;
}


/*
 * chu_audio - initialize audio device
 *
 * This code works with SunOS 4.1.3 and Solaris 2.6; however, it is
 * believed generic and applicable to other systems with a minor twid
 * or two. All it does is open the device, set the buffer size (Solaris
 * only), preset the gain and set the input port. It assumes that the
 * codec sample rate (8000 Hz), precision (8 bits), number of channels
 * (1) and encoding (ITU-T G.711 mu-law companded) have been set by
 * default.
 */
static int
chu_audio(
	)
{
	/*
	 * Open audio control device
	 */
	if ((chu_ctl_fd = open("/dev/audioctl", O_RDWR)) < 0) {
		perror("audioctl");
		return(-1);
	}
#ifdef HAVE_SYS_AUDIOIO_H
	/*
	 * Set audio device parameters.
	 */
	AUDIO_INITINFO(&info);
	info.record.buffer_size = AUDIO_BUFSIZ;
	if (ioctl(chu_ctl_fd, (int)AUDIO_SETINFO, &info) < 0) {
		perror("AUDIO_SETINFO");
		close(chu_ctl_fd);
		return(-1);
	}
#endif /* HAVE_SYS_AUDIOIO_H */
#ifdef DEBUG
	chu_debug();
#endif /* DEBUG */
	return(0);
}


#ifdef DEBUG
/*
 * chu_debug - display audio parameters
 *
 * This code doesn't really do anything, except satisfy curiousity and
 * verify the ioctl's work.
 */
static void
chu_debug(
	)
{
	if (debug == 0)
		return;
#ifdef HAVE_SYS_AUDIOIO_H
	ioctl(chu_ctl_fd, (int)AUDIO_GETDEV, &device);
	printf("chu: name %s, version %s, config %s\n",
	    device.name, device.version, device.config);
#endif /* HAVE_SYS_AUDIOIO_H */
	ioctl(chu_ctl_fd, (int)AUDIO_GETINFO, &info);
	printf(
	    "chu: samples %d, channels %d, precision %d, encoding %d\n",
	    info.record.sample_rate, info.record.channels,
	    info.record.precision, info.record.encoding);
#ifdef HAVE_SYS_AUDIOIO_H
	printf("chu: gain %d, port %d, buffer %d\n",
	    info.record.gain, info.record.port,
	    info.record.buffer_size);
#else /* HAVE_SYS_AUDIOIO_H */
	printf("chu: gain %d, port %d\n",
	    info.record.gain, info.record.port);
#endif /* HAVE_SYS_AUDIOIO_H */
	printf(
	    "chu: samples %d, eof %d, pause %d, error %d, waiting %d, balance %d\n",
	    info.record.samples, info.record.eof,
	    info.record.pause, info.record.error,
	    info.record.waiting, info.record.balance);
	printf("chu: monitor %d, muted %d\n",
	    info.monitor_gain, info.output_muted);
}
#endif /* DEBUG */
#endif /* AUDIO_CHU */

#else
int refclock_chu_bs;
#endif /* REFCLOCK */
