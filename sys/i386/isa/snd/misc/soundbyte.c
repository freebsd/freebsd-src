/*
 * Sound interface for Speak Freely for Unix
 * 
 * Designed and implemented in July of 1990 by John Walker
 * 
 * FreeBSD / voxware version
 */

#define BUFL	8000

#include "speakfree.h"

#include <sys/dir.h>
#include <sys/file.h>

/* #include <math.h> */
#include <errno.h>

#include <sys/ioctl.h>
#ifdef LINUX
#include <linux/soundcard.h>
#else
#include <machine/soundcard.h>
#endif
#define AUDIO_MIN_GAIN 0
#define AUDIO_MAX_GAIN 255
static int      abuf_size;

#define SoundFile       "/dev/audio"
#define AUDIO_CTLDEV    "/dev/mixer"

#define MAX_GAIN	100

struct sound_buf {
    struct sound_buf *snext;	/* Next sound buffer */
    int             sblen;	/* Length of this sound buffer */
    unsigned char   sbtext[2];	/* Actual sampled sound */
};

/* Local variables  */

static int      audiof = -1;	/* Audio device file descriptor */
static int      Audio_fd;	/* Audio control port */
struct sound_buf *sbchain = NULL,	/* Sound buffer chain links */
               *sbtail = NULL;
static int      sbtotal = 0;	/* Total sample bytes in memory */
static int      playing = FALSE;/* Replay in progress ? */
/* static int playqsize;  *//* Output queue size */
static int      playlen = 0;	/* Length left to play */
static unsigned char *playbuf = NULL;	/* Current play pointer */
static int      squelch = 0;	/* Squelch value */

/* Convert local gain into device parameters */

static unsigned
scale_gain(unsigned g)
{
    return (AUDIO_MIN_GAIN + (unsigned)
	    ((int) ((((double) (AUDIO_MAX_GAIN - AUDIO_MIN_GAIN)) *
		     ((double) g / (double) MAX_GAIN)) + 0.5)));
}

#ifdef  HALF_DUPLEX
static int      oldvol = -1;
#endif

/*
 * SOUNDINIT  --  Open the sound peripheral and initialise for access. Return
 * TRUE if successful, FALSE otherwise.
 */

int
soundinit(int iomode)
{
    int             attempts = 3;

    assert(audiof == -1);
    while (attempts-- > 0) {
	if ((audiof = open(SoundFile, iomode)) >= 0) {

	    if ((Audio_fd = open(AUDIO_CTLDEV, O_RDWR)) < 0) {
		perror(AUDIO_CTLDEV);
		return FALSE;
	    }
	    /* fcntl(audiof, F_SETFL, O_NDELAY); */
#ifndef AUDIO_BLOCKING
	    if (ioctl(audiof, SNDCTL_DSP_NONBLOCK, NULL) < 0) {
		perror("SNDCTL_DSP_NONBLOCK");
		return FALSE;
	    }
	    if (ioctl(audiof, SNDCTL_DSP_GETBLKSIZE, &abuf_size) < 0) {
		perror("SNDCTL_DSP_GETBLKSIZE");
		return FALSE;
	    }
#endif
#ifdef  HALF_DUPLEX
	    if (iomode == O_RDONLY) {
		if (oldvol == -1)
		    oldvol = soundgetvol();
		soundplayvol(0);
	    } else if (iomode == O_WRONLY && oldvol != -1 ) {
		if (soundgetvol() == 0)
		    soundplayvol(oldvol);
		oldvol = -1;
	    }
#endif
	    return TRUE;
	}
	if (errno != EINTR)
	    break;
	fprintf(stderr, "Audio open: retrying EINTR attempt %d\n", attempts);
    }
    return FALSE;
}

/* SOUNDTERM  --  Close the sound device. chan=1 for play, 2:capture */

void
soundterm(int chan)
{
    if (audiof >= 0) {
	int             arg;
#ifdef AIOSTOP	/* FreeBSD */
	if (chan == 2) {
	    arg = AIOSYNC_CAPTURE;
	    ioctl(audiof, AIOSTOP, &arg);
	}
#endif
#ifdef	SNDCTL_DSP_SYNC
	if (chan == 1)
	    ioctl(audiof, SNDCTL_DSP_SYNC);
#endif
#ifdef  HALF_DUPLEX
	if (oldvol != -1) {
	    if (soundgetvol() == 0)
		soundplayvol(oldvol);
	    oldvol = -1;
	}
#endif
	if (close(audiof) < 0)
	    perror("closing audio device");
	if (close(Audio_fd) < 0)
	    perror("closing audio control device");
	audiof = -1;
    }
}

/* SOUNDPLAY  --  Begin playing a sound.  */

void
soundplay(int len, unsigned char *buf)
{
    int             ios;

    assert(audiof != -1);
    while (TRUE) {
	ios = write(audiof, buf, len);
	if (ios == -1)
	    sf_usleep(100000);
	else {
	    if (ios < len) {
		buf += ios;
		len -= ios;
	    } else
		break;
	}
    }
}

/* SOUNDPLAYVOL  --  Set playback volume from 0 (silence) to 100 (full on). */

void
soundplayvol(int value)
{
    int             arg;

    arg = (value << 8) | value;

    if (ioctl(Audio_fd, SOUND_MIXER_WRITE_PCM, &arg) < 0)
	perror("SOUND_MIXER_WRITE_PCM");
}

#ifdef  HALF_DUPLEX

/* SOUNDGETVOL -- Get current playback volume. */

int
soundgetvol()
{
    int             arg, v1, v2;

    if (ioctl(Audio_fd, SOUND_MIXER_READ_PCM, &arg) < 0) {
	perror("SOUND_MIXER_READ_PCM");
	return -1;
    }
    v1 = arg & 0xFF;
    v2 = (arg >> 8) & 0xFF;
    return (v1 > v2) ? v1 : v2;
}
#endif

/* SOUNDRECGAIN  --  Set recording gain from 0 (minimum) to 100 (maximum).  */

void
soundrecgain(int value)
{
    int             arg;

    arg = (value << 8) | value;

    if (ioctl(Audio_fd, SOUND_MIXER_WRITE_RECLEV, &arg) < 0)
	perror("SOUND_MIXER_WRITE_RECLEV");
}

/*
 * SOUNDDEST  --  Set destination for generated sound.  If "where" is 0,
 * sound goes to the built-in speaker; if 1, to the audio output jack.
 */

void
sounddest(int where)
{
}

/* SOUNDGRAB  --  Return audio information in the record queue.  */

int
soundgrab(char *buf, int len)
{
    long            read_size;
    int             c;

    read_size = len;
#ifndef AUDIO_BLOCKING
    if (read_size > abuf_size) {
	read_size = abuf_size;
    }
#endif
    while (TRUE) {
	c = read(audiof, buf, read_size);
	if (c < 0) {
	    if (errno == EINTR) {
		continue;
	    } else if (errno == EAGAIN) {
		c = 0;
	    }
	}
	break;
    }
    if (c < 0) {
	perror("soundgrab");
    }
    return c;
}

/* SOUNDFLUSH	--  Flush any queued sound.  */

void
soundflush()
{
    char            sb[BUFL];
    int             c;

#ifndef AUDIO_BLOCKING
    while (TRUE) {
	c = read(audiof, sb, BUFL < abuf_size ? BUFL : abuf_size);
	if (c < 0 && errno == EAGAIN)
	    c = 0;
	if (c < 0)
	    perror("soundflush");
	if (c <= 0)
	    break;
    }
#endif
}
