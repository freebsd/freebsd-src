/*
 * audio.c - audio interface for reference clock audio drivers
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#if defined(HAVE_SYS_AUDIOIO_H) || defined(HAVE_SUN_AUDIOIO_H)

#include "audio.h"
#include "ntp_stdlib.h"
#include "ntp_syslog.h"
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <stdio.h>
#include "ntp_string.h"

#ifdef HAVE_SYS_AUDIOIO_H
#include <sys/audioio.h>
#endif /* HAVE_SYS_AUDIOIO_H */

#ifdef HAVE_SUN_AUDIOIO_H
#include <sys/ioccom.h>
#include <sun/audioio.h>
#endif /* HAVE_SUN_AUDIOIO_H */

#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif /* HAVE_SYS_IOCTL_H */

#include <fcntl.h>

/*
 * Global variables
 */
#ifdef HAVE_SYS_AUDIOIO_H
static struct audio_device device; /* audio device ident */
#endif /* HAVE_SYS_AUDIOIO_H */
static struct audio_info info;	/* audio device info */
static int ctl_fd;		/* audio control file descriptor */


/*
 * audio_init - open and initialize audio device
 *
 * This code works with SunOS 4.x and Solaris 2.x; however, it is
 * believed generic and applicable to other systems with a minor twid
 * or two. All it does is open the device, set the buffer size (Solaris
 * only), preset the gain and set the input port. It assumes that the
 * codec sample rate (8000 Hz), precision (8 bits), number of channels
 * (1) and encoding (ITU-T G.711 mu-law companded) have been set by
 * default.
 */
int
audio_init(
	char *dname		/* device name */
	)
{
	int fd;
	int rval;

	/*
	 * Open audio device. Do not complain if not there.
	 */
	fd = open(dname, O_RDWR | O_NONBLOCK, 0777);
	if (fd < 0)
		return (fd);

	/*
	 * Open audio control device.
	 */
	ctl_fd = open("/dev/audioctl", O_RDWR);
	if (ctl_fd < 0) {
		msyslog(LOG_ERR, "audio: invalid control device\n");
		close(fd);
		return(ctl_fd);
	}

	/*
	 * Set audio device parameters.
	 */
	rval = audio_gain((AUDIO_MAX_GAIN - AUDIO_MIN_GAIN) / 2,
	    AUDIO_MICROPHONE);
	if (rval < 0) {
		msyslog(LOG_ERR, "audio: invalid control device parameters\n");
		close(ctl_fd);
		close(fd);
		return(rval);
	}
	return (fd);
}


/*
 * audio_gain - adjust codec gain and port
 */
int
audio_gain(
	int gain,		/* gain 0-255 */
	int port		/* port */
	)
{
	int rval;

	AUDIO_INITINFO(&info);
#ifdef HAVE_SYS_AUDIOIO_H
	info.record.buffer_size = AUDIO_BUFSIZ;
#endif /* HAVE_SYS_AUDIOIO_H */
	info.record.gain = gain;
	info.record.port = port;
	info.record.error = 0;
	rval = ioctl(ctl_fd, (int)AUDIO_SETINFO, &info);
	if (rval < 0) {
		msyslog(LOG_ERR, "audio_gain: %m");
		return (rval);
	}
	return (info.record.error);
}


/*
 * audio_show - display audio parameters
 *
 * This code doesn't really do anything, except satisfy curiousity and
 * verify the ioctl's work.
 */
void
audio_show(void)
{
#ifdef HAVE_SYS_AUDIOIO_H
	ioctl(ctl_fd, (int)AUDIO_GETDEV, &device);
	printf("audio: name %s, version %s, config %s\n",
	    device.name, device.version, device.config);
#endif /* HAVE_SYS_AUDIOIO_H */
	ioctl(ctl_fd, (int)AUDIO_GETINFO, &info);
	printf(
	    "audio: samples %d, channels %d, precision %d, encoding %d, gain %d, port %d\n",
	    info.record.sample_rate, info.record.channels,
	    info.record.precision, info.record.encoding,
	    info.record.gain, info.record.port);
	printf(
	    "audio: samples %d, eof %d, pause %d, error %d, waiting %d, balance %d\n",
	    info.record.samples, info.record.eof,
	    info.record.pause, info.record.error,
	    info.record.waiting, info.record.balance);
}
#else
int audio_bs;
#endif /* HAVE_SYS_AUDIOIO_H HAVE_SUN_AUDIOIO_H */
