/*
 * refclock_irig - audio IRIG-B/E demodulator/decoder
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#if defined(REFCLOCK) && defined(CLOCK_IRIG)

#include "ntpd.h"
#include "ntp_io.h"
#include "ntp_refclock.h"
#include "ntp_calendar.h"
#include "ntp_stdlib.h"

#include <stdio.h>
#include <ctype.h>
#include <math.h>
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif /* HAVE_SYS_IOCTL_H */

#include "audio.h"

/*
 * Audio IRIG-B/E demodulator/decoder
 *
 * This driver receives, demodulates and decodes IRIG-B/E signals when
 * connected to the audio codec /dev/audio. The IRIG signal format is an
 * amplitude-modulated carrier with pulse-width modulated data bits. For
 * IRIG-B, the carrier frequency is 1000 Hz and bit rate 100 b/s; for
 * IRIG-E, the carrier frequenchy is 100 Hz and bit rate 10 b/s. The
 * driver automatically recognizes which format is in use.
 *
 * The program processes 8000-Hz mu-law companded samples using separate
 * signal filters for IRIG-B and IRIG-E, a comb filter, envelope
 * detector and automatic threshold corrector. Cycle crossings relative
 * to the corrected slice level determine the width of each pulse and
 * its value - zero, one or position identifier. The data encode 20 BCD
 * digits which determine the second, minute, hour and day of the year
 * and sometimes the year and synchronization condition. The comb filter
 * exponentially averages the corresponding samples of successive baud
 * intervals in order to reliably identify the reference carrier cycle.
 * A type-II phase-lock loop (PLL) performs additional integration and
 * interpolation to accurately determine the zero crossing of that
 * cycle, which determines the reference timestamp. A pulse-width
 * discriminator demodulates the data pulses, which are then encoded as
 * the BCD digits of the timecode.
 *
 * The timecode and reference timestamp are updated once each second
 * with IRIG-B (ten seconds with IRIG-E) and local clock offset samples
 * saved for later processing. At poll intervals of 64 s, the saved
 * samples are processed by a trimmed-mean filter and used to update the
 * system clock.
 *
 * An automatic gain control feature provides protection against
 * overdriven or underdriven input signal amplitudes. It is designed to
 * maintain adequate demodulator signal amplitude while avoiding
 * occasional noise spikes. In order to assure reliable capture, the
 * decompanded input signal amplitude must be greater than 100 units and
 * the codec sample frequency error less than 250 PPM (.025 percent).
 *
 * The program performs a number of error checks to protect against
 * overdriven or underdriven input signal levels, incorrect signal
 * format or improper hardware configuration. Specifically, if any of
 * the following errors occur for a time measurement, the data are
 * rejected.
 *
 * o The peak carrier amplitude is less than DRPOUT (100). This usually
 *   means dead IRIG signal source, broken cable or wrong input port.
 *
 * o The frequency error is greater than MAXFREQ +-250 PPM (.025%). This
 *   usually means broken codec hardware or wrong codec configuration.
 *
 * o The modulation index is less than MODMIN (0.5). This usually means
 *   overdriven IRIG signal or wrong IRIG format.
 *
 * o A frame synchronization error has occurred. This usually means wrong
 *   IRIG signal format or the IRIG signal source has lost
 *   synchronization (signature control).
 *
 * o A data decoding error has occurred. This usually means wrong IRIG
 *   signal format.
 *
 * o The current second of the day is not exactly one greater than the
 *   previous one. This usually means a very noisy IRIG signal or
 *   insufficient CPU resources.
 *
 * o An audio codec error (overrun) occurred. This usually means
 *   insufficient CPU resources, as sometimes happens with Sun SPARC
 *   IPCs when doing something useful.
 *
 * Note that additional checks are done elsewhere in the reference clock
 * interface routines.
 *
 * Debugging aids
 *
 * The timecode format used for debugging and data recording includes
 * data helpful in diagnosing problems with the IRIG signal and codec
 * connections. With debugging enabled (-d -d -d on the ntpd command
 * line), the driver produces one line for each timecode in the
 * following format:
 *
 * 00 1 98 23 19:26:52 721 143 0.694 47 20 0.083 66.5 3094572411.00027
 *
 * The most recent line is also written to the clockstats file at 64-s
 * intervals.
 *
 * The first field contains the error flags in hex, where the hex bits
 * are interpreted as below. This is followed by the IRIG status
 * indicator, year of century, day of year and time of day. The status
 * indicator and year are not produced by some IRIG devices. Following
 * these fields are the signal amplitude (0-8100), codec gain (0-255),
 * field phase (0-79), time constant (2-20), modulation index (0-1),
 * carrier phase error (0+-0.5) and carrier frequency error (PPM). The
 * last field is the on-time timestamp in NTP format.
 *
 * The fraction part of the on-time timestamp is a good indicator of how
 * well the driver is doing. With an UltrSPARC 30, this thing can keep
 * the clock within a few tens of microseconds relative to the IRIG-B
 * signal. Accuracy with IRIG-E is about ten times worse.
 *
 * Unlike other drivers, which can have multiple instantiations, this
 * one supports only one. It does not seem likely that more than one
 * audio codec would be useful in a single machine. More than one would
 * probably chew up too much CPU time anyway.
 *
 * Fudge factors
 *
 * Fudge flag2 selects the audio input port, where 0 is the mike port
 * (default) and 1 is the line-in port. It does not seem useful to
 * select the compact disc player port. Fudge flag3 enables audio
 * monitoring of the input signal. For this purpose, the speaker volume
 * must be set before the driver is started. Fudge flag4 causes the
 * debugging output described above to be recorded in the clockstats
 * file. Any of these flags can be changed during operation with the
 * ntpdc program.
 */

/*
 * Interface definitions
 */
#define	DEVICE_AUDIO	"/dev/audio" /* audio device name */
#define	PRECISION	(-17)	/* precision assumed (about 10 us) */
#define	REFID		"IRIG"	/* reference ID */
#define	DESCRIPTION	"Generic IRIG Audio Driver" /* WRU */

#define SECOND		8000	/* nominal sample rate (Hz) */
#define BAUD		80	/* samples per baud interval */
#define OFFSET		128	/* companded sample offset */
#define SIZE		256	/* decompanding table size */
#define CYCLE		8	/* samples per carrier cycle */
#define SUBFLD		10	/* bits per subfield */
#define FIELD		10	/* subfields per field */
#define MINTC		2	/* min PLL time constant */
#define MAXTC		20	/* max PLL time constant max */
#define	MAXSIG		6000.	/* maximum signal level */
#define DRPOUT		100.	/* dropout signal level */
#define MODMIN		0.5	/* minimum modulation index */
#define MAXFREQ		(250e-6 * SECOND) /* freq tolerance (.025%) */
#define PI		3.1415926535 /* the real thing */

/*
 * Experimentally determined fudge factors
 */
#define IRIG_B		.0019		/* IRIG-B phase delay */
#define IRIG_E		.0019		/* IRIG-E phase delay */

/*
 * Data bit definitions
 */
#define BIT0		0	/* zero */
#define BIT1		1	/* one */
#define BITP		2	/* position identifier */

/*
 * Error flags (up->errflg)
 */
#define IRIG_ERR_AMP	0x01	/* low carrier amplitude */
#define IRIG_ERR_FREQ	0x02	/* frequency tolerance exceeded */
#define IRIG_ERR_MOD	0x04	/* low modulation index */
#define IRIG_ERR_SYNCH	0x08	/* frame synch error */
#define IRIG_ERR_DECODE	0x10	/* frame decoding error */
#define IRIG_ERR_CHECK	0x20	/* second numbering discrepancy */
#define IRIG_ERR_ERROR	0x40	/* codec error (overrun) */

/*
 * IRIG unit control structure
 */
struct irigunit {
	u_char	timecode[21];	/* timecode string */
	l_fp	timestamp;	/* audio sample timestamp */
	l_fp	tick;		/* audio sample increment */
	double	comp[SIZE];	/* decompanding table */
	double	integ[BAUD];	/* baud integrator */
	double	phase, freq;	/* logical clock phase and frequency */
	double	zxing;		/* phase detector integrator */
	double	yxing;		/* phase detector display */
	double	modndx;		/* modulation index */
	double	irig_b;		/* IRIG-B signal amplitude */
	double	irig_e;		/* IRIG-E signal amplitude */
	int	errflg;		/* error flags */
	int	bufcnt;		/* samples in buffer */
	int	bufptr;		/* buffer index pointer */
	int	pollcnt;	/* poll counter */
	int	port;		/* codec port */
	int	gain;		/* codec gain */
	int	clipcnt;	/* sample clipped count */
	int	seccnt;		/* second interval counter */
	int	decim;		/* sample decimation factor */

	/*
	 * RF variables
	 */
	double	hpf[5];		/* IRIG-B filter shift register */
	double	lpf[5];		/* IRIG-E filter shift register */
	double	intmin, intmax;	/* integrated envelope min and max */
	double	envmax;		/* peak amplitude */
	double	envmin;		/* noise amplitude */
	double	maxsignal;	/* integrated peak amplitude */
	double	noise;		/* integrated noise amplitude */
	double	lastenv[CYCLE];	/* last cycle amplitudes */
	double	lastint[CYCLE];	/* last integrated cycle amplitudes */
	double	lastsig;	/* last carrier sample */
	double	xxing;		/* phase detector interpolated output */
	double	fdelay;		/* filter delay */
	int	envphase;	/* envelope phase */
	int	envptr;		/* envelope phase pointer */
	int	carphase;	/* carrier phase */
	int	envsw;		/* envelope state */
	int	envxing;	/* envelope slice crossing */
	int	tc;		/* time constant */
	int	tcount;		/* time constant counter */
	int	badcnt;		/* decimation interval counter */

	/*
	 * Decoder variables
	 */
	l_fp	montime;	/* reference timestamp for eyeball */
	int	timecnt;	/* timecode counter */
	int	pulse;		/* cycle counter */
	int	cycles;		/* carrier cycles */
	int	dcycles;	/* data cycles */
	int	xptr;		/* translate table pointer */
	int	lastbit;	/* last code element length */
	int	second;		/* previous second */
	int	fieldcnt;	/* subfield count in field */
	int	bits;		/* demodulated bits */
	int	bitcnt;		/* bit count in subfield */
};

/*
 * Function prototypes
 */
static	int	irig_start	P((int, struct peer *));
static	void	irig_shutdown	P((int, struct peer *));
static	void	irig_receive	P((struct recvbuf *));
static	void	irig_poll	P((int, struct peer *));

/*
 * More function prototypes
 */
static	void	irig_base	P((struct peer *, double));
static	void	irig_rf		P((struct peer *, double));
static	void	irig_decode	P((struct peer *, int));
static	void	irig_gain	P((struct peer *));

/*
 * Transfer vector
 */
struct	refclock refclock_irig = {
	irig_start,		/* start up driver */
	irig_shutdown,		/* shut down driver */
	irig_poll,		/* transmit poll message */
	noentry,		/* not used (old irig_control) */
	noentry,		/* initialize driver (not used) */
	noentry,		/* not used (old irig_buginfo) */
	NOFLAGS			/* not used */
};

/*
 * Global variables
 */
static char	hexchar[] = {	/* really quick decoding table */
	'0', '8', '4', 'c',		/* 0000 0001 0010 0011 */
	'2', 'a', '6', 'e',		/* 0100 0101 0110 0111 */
	'1', '9', '5', 'd',		/* 1000 1001 1010 1011 */
	'3', 'b', '7', 'f'		/* 1100 1101 1110 1111 */
};


/*
 * irig_start - open the devices and initialize data for processing
 */
static int
irig_start(
	int	unit,		/* instance number (not used) */
	struct peer *peer	/* peer structure pointer */
	)
{
	struct refclockproc *pp;
	struct irigunit *up;

	/*
	 * Local variables
	 */
	int	fd;		/* file descriptor */
	int	i;		/* index */
	double	step;		/* codec adjustment */

	/*
	 * Open audio device
	 */
	fd = audio_init(DEVICE_AUDIO);
	if (fd < 0)
		return (0);
#ifdef DEBUG
	if (debug)
		audio_show();
#endif

	/*
	 * Allocate and initialize unit structure
	 */
	if (!(up = (struct irigunit *)
	      emalloc(sizeof(struct irigunit)))) {
		(void) close(fd);
		return (0);
	}
	memset((char *)up, 0, sizeof(struct irigunit));
	pp = peer->procptr;
	pp->unitptr = (caddr_t)up;
	pp->io.clock_recv = irig_receive;
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
	up->tc = MINTC;
	up->decim = 1;
	up->fdelay = IRIG_B;
	up->gain = 127;
	up->pollcnt = 2;

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
	return (1);
}


/*
 * irig_shutdown - shut down the clock
 */
static void
irig_shutdown(
	int	unit,		/* instance number (not used) */
	struct peer *peer	/* peer structure pointer */
	)
{
	struct refclockproc *pp;
	struct irigunit *up;

	pp = peer->procptr;
	up = (struct irigunit *)pp->unitptr;
	io_closeclock(&pp->io);
	free(up);
}


/*
 * irig_receive - receive data from the audio device
 *
 * This routine reads input samples and adjusts the logical clock to
 * track the irig clock by dropping or duplicating codec samples.
 */
static void
irig_receive(
	struct recvbuf *rbufp	/* receive buffer structure pointer */
	)
{
	struct peer *peer;
	struct refclockproc *pp;
	struct irigunit *up;

	/*
	 * Local variables
	 */
	double	sample;		/* codec sample */
	u_char	*dpt;		/* buffer pointer */
	l_fp	ltemp;		/* l_fp temp */

	peer = (struct peer *)rbufp->recv_srcclock;
	pp = peer->procptr;
	up = (struct irigunit *)pp->unitptr;

	/*
	 * Main loop - read until there ain't no more. Note codec
	 * samples are bit-inverted.
	 */
	up->timestamp = rbufp->recv_time;
	up->bufcnt = rbufp->recv_length;
	DTOLFP((double)up->bufcnt / SECOND, &ltemp);
	L_SUB(&up->timestamp, &ltemp);
	dpt = rbufp->recv_buffer;
	for (up->bufptr = 0; up->bufptr < up->bufcnt; up->bufptr++) {
		sample = up->comp[~*dpt++ & 0xff];

		/*
		 * Clip noise spikes greater than MAXSIG. If no clips,
		 * increase the gain a tad; if the clips are too high, 
		 * decrease a tad. Choose either IRIG-B or IRIG-E
		 * according to the energy at the respective filter
		 * output.
		 */
		if (sample > MAXSIG) {
			sample = MAXSIG;
			up->clipcnt++;
		} else if (sample < -MAXSIG) {
			sample = -MAXSIG;
			up->clipcnt++;
		}

		/*
		 * Variable frequency oscillator. A phase change of one
		 * unit produces a change of 360 degrees; a frequency
		 * change of one unit produces a change of 1 Hz.
		 */
		up->phase += up->freq / SECOND;
		if (up->phase >= .5) {
			up->phase -= 1.;
		} else if (up->phase < -.5) {
			up->phase += 1.;
			irig_rf(peer, sample);
			irig_rf(peer, sample);
		} else {
			irig_rf(peer, sample);
		}
		L_ADD(&up->timestamp, &up->tick);

		/*
		 * Once each second, determine the IRIG format, codec
		 * port and gain.
		 */
		up->seccnt = (up->seccnt + 1) % SECOND;
		if (up->seccnt == 0) {
			if (up->irig_b > up->irig_e) {
				up->decim = 1;
				up->fdelay = IRIG_B;
			} else {
				up->decim = 10;
				up->fdelay = IRIG_E;
			}
			if (pp->sloppyclockflag & CLK_FLAG2)
			    up->port = 2;
			else
			    up->port = 1;
			irig_gain(peer);
			up->irig_b = up->irig_e = 0;
		}
	}

	/*
	 * Squawk to the monitor speaker if enabled.
	 */
	if (pp->sloppyclockflag & CLK_FLAG3)
	    if (write(pp->io.fd, (u_char *)&rbufp->recv_space,
		      (u_int)up->bufcnt) < 0)
		perror("irig:");
}

/*
 * irig_rf - RF processing
 *
 * This routine filters the RF signal using a highpass filter for IRIG-B
 * and a lowpass filter for IRIG-E. In case of IRIG-E, the samples are
 * decimated by a factor of ten. The lowpass filter functions also as a
 * decimation filter in this case. Note that the codec filters function
 * as roofing filters to attenuate both the high and low ends of the
 * passband. IIR filter coefficients were determined using Matlab Signal
 * Processing Toolkit.
 */
static void
irig_rf(
	struct peer *peer,	/* peer structure pointer */
	double	sample		/* current signal sample */
	)
{
	struct refclockproc *pp;
	struct irigunit *up;

	/*
	 * Local variables
	 */
	double	irig_b, irig_e;	/* irig filter outputs */

	pp = peer->procptr;
	up = (struct irigunit *)pp->unitptr;

	/*
	 * IRIG-B filter. 4th-order elliptic, 800-Hz highpass, 0.3 dB
	 * passband ripple, -50 dB stopband ripple, phase delay -.0022
	 * s)
	 */
	irig_b = (up->hpf[4] = up->hpf[3]) * 2.322484e-01;
	irig_b += (up->hpf[3] = up->hpf[2]) * -1.103929e+00;
	irig_b += (up->hpf[2] = up->hpf[1]) * 2.351081e+00;
	irig_b += (up->hpf[1] = up->hpf[0]) * -2.335036e+00;
	up->hpf[0] = sample - irig_b;
	irig_b = up->hpf[0] * 4.335855e-01
	    + up->hpf[1] * -1.695859e+00
	    + up->hpf[2] * 2.525004e+00
	    + up->hpf[3] * -1.695859e+00
	    + up->hpf[4] * 4.335855e-01;
	up->irig_b += irig_b * irig_b;

	/*
	 * IRIG-E filter. 4th-order elliptic, 130-Hz lowpass, 0.3 dB
	 * passband ripple, -50 dB stopband ripple, phase delay .0219 s.
	 */
	irig_e = (up->lpf[4] = up->lpf[3]) * 8.694604e-01;
	irig_e += (up->lpf[3] = up->lpf[2]) * -3.589893e+00;
	irig_e += (up->lpf[2] = up->lpf[1]) * 5.570154e+00;
	irig_e += (up->lpf[1] = up->lpf[0]) * -3.849667e+00;
	up->lpf[0] = sample - irig_e;
	irig_e = up->lpf[0] * 3.215696e-03
	    + up->lpf[1] * -1.174951e-02
	    + up->lpf[2] * 1.712074e-02
	    + up->lpf[3] * -1.174951e-02
	    + up->lpf[4] * 3.215696e-03;
	up->irig_e += irig_e * irig_e;

	/*
	 * Decimate by a factor of either 1 (IRIG-B) or 10 (IRIG-E).
	 */
	up->badcnt = (up->badcnt + 1) % up->decim;
	if (up->badcnt == 0) {
		if (up->decim == 1)
		    irig_base(peer, irig_b);
		else
		    irig_base(peer, irig_e);
	}
}

/*
 * irig_base - baseband processing
 *
 * This routine processes the baseband signal and demodulates the AM
 * carrier using a synchronous detector. It then synchronizes to the
 * data frame at the baud rate and decodes the data pulses.
 */
static void
irig_base(
	struct peer *peer,	/* peer structure pointer */
	double	sample		/* current signal sample */
	)
{
	struct refclockproc *pp;
	struct irigunit *up;

	/*
	 * Local variables
	 */
	double	lope;		/* integrator output */
	double	env;		/* envelope detector output */
	double	dtemp;		/* double temp */
	int	i;		/* index temp */

	pp = peer->procptr;
	up = (struct irigunit *)pp->unitptr;

	/*
	 * Synchronous baud integrator. Corresponding samples of current
	 * and past baud intervals are integrated to refine the envelope
	 * amplitude and phase estimate. We keep one cycle of both the
	 * raw and integrated data for later use.
	 */
	up->envphase = (up->envphase + 1) % BAUD;
	up->carphase = (up->carphase + 1) % CYCLE;
	up->integ[up->envphase] += (sample - up->integ[up->envphase]) /
	    (5 * up->tc);
	lope = up->integ[up->envphase];
	up->lastenv[up->carphase] = sample;
	up->lastint[up->carphase] = lope;

	/*
	 * Phase detector. Sample amplitudes are integrated over the
	 * baud interval. Cycle phase is determined from these
	 * amplitudes using an eight-sample cyclic buffer. A phase
	 * change of 360 degrees produces an output change of one unit.
	 */ 
	if (up->lastsig > 0 && lope <= 0) {
		up->xxing = lope / (up->lastsig - lope);
		up->zxing += (up->carphase - 4 + up->xxing) / 8.;
	}
	up->lastsig = lope;

	/*
	 * Update signal/noise estimates and PLL phase/frequency.
	 */
	if (up->envphase == 0) {

		/*
		 * Update envelope signal and noise estimates and mess
		 * with error bits.
		 */
		up->maxsignal = up->intmax;
		up->noise = up->intmin;
		if (up->maxsignal < DRPOUT)
		    up->errflg |= IRIG_ERR_AMP;
		if (up->intmax > 0)
		    up->modndx = (up->intmax - up->intmin) / up->intmax;
 		else
		    up->modndx = 0;
		if (up->modndx < MODMIN)
		    up->errflg |= IRIG_ERR_MOD;
		up->intmin = 1e6; up->intmax = 0;
		if (up->errflg & (IRIG_ERR_AMP | IRIG_ERR_FREQ |
				  IRIG_ERR_MOD | IRIG_ERR_SYNCH)) {
			up->tc = MINTC;
			up->tcount = 0;
		}

		/*
		 * Update PLL phase and frequency. The PLL time constant
		 * is set initially to stabilize the frequency within a
		 * minute or two, then increases to the maximum. The
		 * frequency is clamped so that the PLL capture range
		 * cannot be exceeded.
		 */
		dtemp = up->zxing * up->decim / BAUD;
		up->yxing = dtemp;
		up->zxing = 0.;
		up->phase += dtemp / up->tc;
		up->freq += dtemp / (4. * up->tc * up->tc);
		if (up->freq > MAXFREQ) {
			up->freq = MAXFREQ;
			up->errflg |= IRIG_ERR_FREQ;
		} else if (up->freq < -MAXFREQ) {
			up->freq = -MAXFREQ;
			up->errflg |= IRIG_ERR_FREQ;
		}
	}

	/*
	 * Synchronous demodulator. There are eight samples in the cycle
	 * and ten cycles in the baud interval. The amplitude of each
	 * cycle is determined at the last sample in the cycle. The
	 * beginning of the data pulse is determined from the integrated
	 * samples, while the end of the pulse is determined from the
	 * raw samples. The raw data bits are demodulated relative to
	 * the slice level and left-shifted in the decoding register.
	 */
	if (up->carphase != 7)
	    return;
	env = (up->lastenv[2] - up->lastenv[6]) / 2.;
	lope = (up->lastint[2] - up->lastint[6]) / 2.;
	if (lope > up->intmax)
	    up->intmax = lope;
	if (lope < up->intmin)
	    up->intmin = lope;

	/*
	 * Pulse code demodulator and reference timestamp. The decoder
	 * looks for a sequence of ten bits; the first two bits must be
	 * one, the last two bits must be zero. Frame synch is asserted
	 * when three correct frames have been found.
	 */
	up->pulse = (up->pulse + 1) % 10;
	if (up->pulse == 1)
	    up->envmax = env;
	else if (up->pulse == 9)
	    up->envmin = env;
	up->dcycles <<= 1;
	if (env >= (up->envmax + up->envmin) / 2.)
	    up->dcycles |= 1;
	up->cycles <<= 1;
	if (lope >= (up->maxsignal + up->noise) / 2.)
	    up->cycles |= 1;
	if ((up->cycles & 0x303c0f03) == 0x300c0300) {
		l_fp ltemp;
		int bitz;

		/*
		 * The PLL time constant starts out small, in order to
		 * sustain a frequency tolerance of 250 PPM. It
		 * gradually increases as the loop settles down. Note
		 * that small wiggles are not believed, unless they
		 * persist for lots of samples.
		 */
		if (up->pulse != 9)
		    up->errflg |= IRIG_ERR_SYNCH;
		up->pulse = 9;
		dtemp = BAUD - up->zxing;
		i = up->envxing - up->envphase;
		if (i < 0)
		    i -= i;
		if (i <= 1) {
			up->tcount++;
			if (up->tcount > 50 * up->tc) {
				up->tc++;
				if (up->tc > MAXTC)
				    up->tc = MAXTC;
				up->tcount = 0;
				up->envxing = up->envphase;
			} else {
				dtemp -= up->envxing - up->envphase;
			}
		} else {
			up->tcount = 0;
			up->envxing = up->envphase;
		}

		/*
		 * Determine a reference timestamp, accounting for the
		 * codec delay and filter delay. Note the timestamp is
		 * for the previous frame, so we have to backtrack for
		 * this plus the delay since the last carrier positive
		 * zero crossing.
		 */
		DTOLFP(up->decim * (dtemp / SECOND + 1.) + up->fdelay,
		       &ltemp);
		pp->lastrec = up->timestamp;
		L_SUB(&pp->lastrec, &ltemp);

		/*
		 * The data bits are collected in ten-bit frames. The
		 * first two and last two bits are determined by frame
		 * sync and ignored here; the resulting patterns
		 * represent zero (0-1 bits), one (2-4 bits) and
		 * position identifier (5-6 bits). The remaining
		 * patterns represent errors and are treated as zeros.
		 */
		bitz = up->dcycles & 0xfc;
		switch(bitz) {

		    case 0x00:
		    case 0x80:
			irig_decode(peer, BIT0);
			break;

		    case 0xc0:
		    case 0xe0:
		    case 0xf0:
			irig_decode(peer, BIT1);
			break;

		    case 0xf8:
		    case 0xfc:
			irig_decode(peer, BITP);
			break;

		    default:
			irig_decode(peer, 0);
			up->errflg |= IRIG_ERR_DECODE;
		}
	}
}


/*
 * irig_decode - decode the data
 *
 * This routine assembles bits into digits, digits into subfields and
 * subfields into the timecode field. Bits can have values of zero, one
 * or position identifier. There are four bits per digit, two digits per
 * subfield and ten subfields per field. The last bit in every subfield
 * and the first bit in the first subfield are position identifiers.
 */
static void
irig_decode(
	struct	peer *peer,	/* peer structure pointer */
	int	bit		/* data bit (0, 1 or 2) */
	)
{
	struct refclockproc *pp;
	struct irigunit *up;

	/*
	 * Local variables
	 */
	char	syncchar;	/* sync character (Spectracom only) */
	char	sbs[6];		/* binary seconds since 0h */
	char	spare[2];	/* mulligan digits */

        pp = peer->procptr;
	up = (struct irigunit *)pp->unitptr;

	/*
	 * Assemble subfield bits.
	 */
	up->bits <<= 1;
	if (bit == BIT1) {
		up->bits |= 1;
	} else if (bit == BITP && up->lastbit == BITP) {

		/*
		 * Frame sync - two adjacent position identifiers.
		 * Monitor the reference timestamp and wiggle the
		 * clock, but only if no errors have occurred.
		 */
		up->bitcnt = 1;
		up->fieldcnt = 0;
		up->lastbit = 0;
		up->montime = pp->lastrec;
		if (up->errflg == 0) {
			up->timecnt++;
			refclock_process(pp);
		}
		if (up->timecnt >= MAXSTAGE) {
			refclock_receive(peer);
			up->timecnt = 0;
			up->pollcnt = 2;
		}
		up->errflg = 0;
	}
	up->bitcnt = (up->bitcnt + 1) % SUBFLD;
	if (up->bitcnt == 0) {

		/*
		 * End of subfield. Encode two hexadecimal digits in
		 * little-endian timecode field.
		 */
		if (up->fieldcnt == 0)
		    up->bits <<= 1;
		if (up->xptr < 2)
		    up->xptr = 2 * FIELD;
		up->timecode[--up->xptr] = hexchar[(up->bits >> 5) &
						  0xf];
		up->timecode[--up->xptr] = hexchar[up->bits & 0xf];
		up->fieldcnt = (up->fieldcnt + 1) % FIELD;
		if (up->fieldcnt == 0) {

			/*
			 * End of field. Decode the timecode, adjust the
			 * gain and set the input port. Set the port
			 * here on the assumption somebody might even
			 * change it on-wing.
			 */
			up->xptr = 2 * FIELD;
			if (sscanf((char *)up->timecode,
				   "%6s%2d%c%2s%3d%2d%2d%2d",
				   sbs, &pp->year, &syncchar, spare, &pp->day,
				   &pp->hour, &pp->minute, &pp->second) != 8)
			    pp->leap = LEAP_NOTINSYNC;
			else
			    pp->leap = LEAP_NOWARNING;
			up->second = (up->second + up->decim) % 60;
			if (pp->second != up->second)
			    up->errflg |= IRIG_ERR_CHECK;
			up->second = pp->second;
			sprintf(pp->a_lastcode,
				"%02x %c %2d %3d %02d:%02d:%02d %4.0f %3d %6.3f %2d %2d %6.3f %6.1f %s",
				up->errflg, syncchar, pp->year, pp->day,
				pp->hour, pp->minute, pp->second,
				up->maxsignal, up->gain, up->modndx,
				up->envxing, up->tc, up->yxing, up->freq *
				1e6 / SECOND, ulfptoa(&up->montime, 6));
			pp->lencode = strlen(pp->a_lastcode);
			if (up->timecnt == 0 || pp->sloppyclockflag &
			    CLK_FLAG4)
			    record_clock_stats(&peer->srcadr,
					       pp->a_lastcode);
#ifdef DEBUG
			if (debug > 2)
			    printf("irig: %s\n", pp->a_lastcode);
#endif /* DEBUG */
		}
	}
	up->lastbit = bit;
}


/*
 * irig_poll - called by the transmit procedure
 *
 * This routine keeps track of status. If nothing is heard for two
 * successive poll intervals, a timeout event is declared and any
 * orphaned timecode updates are sent to foster care. 
 */
static void
irig_poll(
	int	unit,		/* instance number (not used) */
	struct peer *peer	/* peer structure pointer */
	)
{
	struct refclockproc *pp;
	struct irigunit *up;

	pp = peer->procptr;
	up = (struct irigunit *)pp->unitptr;

	/*
	 * Keep book for tattletales
	 */
	if (up->pollcnt == 0) {
		refclock_report(peer, CEVNT_TIMEOUT);
		up->timecnt = 0;
		return;
	}
	up->pollcnt--;
	pp->polls++;
	
}


/*
 * irig_gain - adjust codec gain
 *
 * This routine is called once each second. If the signal envelope
 * amplitude is too low, the codec gain is bumped up by four units; if
 * too high, it is bumped down. The decoder is relatively insensitive to
 * amplitude, so this crudity works just fine. The input port is set and
 * the error flag is cleared, mostly to be ornery.
 */
static void
irig_gain(
	struct peer *peer	/* peer structure pointer */
	)
{
	struct refclockproc *pp;
	struct irigunit *up;

	pp = peer->procptr;
	up = (struct irigunit *)pp->unitptr;

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


#else
int refclock_irig_bs;
#endif /* REFCLOCK */
