/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.org> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * This is a V.21 modem for ISDN4BSD.
 *
 * $FreeBSD$
 */

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <err.h>
#include <sys/ioccom.h>
#include <errno.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <libutil.h>

#include <i4b/i4b_tel_ioctl.h>

static void create_session(void);
static void input_byte(int byte, int stopbit);
static void sample(int vol, int* tones);
static void tonedetect(unsigned char *ptr, int count);
static void uart(int bit);

static int dcd;			/* Carrier on ? */
static int ptyfd = -1;		/* PTY filedescriptor */
static int telfd = -1;		/* I4BTEL filedescriptor */

/*
 * Alaw to Linear [-32767..32767] conversion
 */

static int a2l[256] = { -5504, -5248, -6016, -5760, -4480, -4224,
-4992, -4736, -7552, -7296, -8064, -7808, -6528, -6272, -7040,
-6784, -2752, -2624, -3008, -2880, -2240, -2112, -2496, -2368,
-3776, -3648, -4032, -3904, -3264, -3136, -3520, -3392, -22016,
-20992, -24064, -23040, -17920, -16896, -19968, -18944, -30208,
-29184, -32256, -31232, -26112, -25088, -28160, -27136, -11008,
-10496, -12032, -11520, -8960, -8448, -9984, -9472, -15104, -14592,
-16128, -15616, -13056, -12544, -14080, -13568, -344, -328, -376,
-360, -280, -264, -312, -296, -472, -456, -504, -488, -408, -392,
-440, -424, -88, -72, -120, -104, -24, -8, -56, -40, -216, -200,
-248, -232, -152, -136, -184, -168, -1376, -1312, -1504, -1440,
-1120, -1056, -1248, -1184, -1888, -1824, -2016, -1952, -1632,
-1568, -1760, -1696, -688, -656, -752, -720, -560, -528, -624,
-592, -944, -912, -1008, -976, -816, -784, -880, -848, 5504, 5248,
6016, 5760, 4480, 4224, 4992, 4736, 7552, 7296, 8064, 7808, 6528,
6272, 7040, 6784, 2752, 2624, 3008, 2880, 2240, 2112, 2496, 2368,
3776, 3648, 4032, 3904, 3264, 3136, 3520, 3392, 22016, 20992, 24064,
23040, 17920, 16896, 19968, 18944, 30208, 29184, 32256, 31232,
26112, 25088, 28160, 27136, 11008, 10496, 12032, 11520, 8960, 8448,
9984, 9472, 15104, 14592, 16128, 15616, 13056, 12544, 14080, 13568,
344, 328, 376, 360, 280, 264, 312, 296, 472, 456, 504, 488, 408,
392, 440, 424, 88, 72, 120, 104, 24, 8, 56, 40, 216, 200, 248, 232,
152, 136, 184, 168, 1376, 1312, 1504, 1440, 1120, 1056, 1248, 1184,
1888, 1824, 2016, 1952, 1632, 1568, 1760, 1696, 688, 656, 752, 720,
560, 528, 624, 592, 944, 912, 1008, 976, 816, 784, 880, 848 };

/*
 * A High Q Tone detector
 */
		
#define NTONES	2		/* Number of tones to detect */
#define SCALE	4096		/* Scaling factor */
#define EXPAVG	14		/* Exponential Average factor */
#define POLERAD	3885		/* pole_radius ^ 2 * SCALE */

/* Table of "-cos(2 * PI * frequency / sample_rate) * SCALE" */
static int p[NTONES] = {
	-2941,			/* 980 Hz */
	-2460			/* 1180 Hz */
};

static void
tonedetect(unsigned char *ptr, int count)
{
	int i, j;
	int y;
	int c, d, f, n;
	static int k[NTONES], h[NTONES];
	static int tones[NTONES];
	static int amplitude;


	for (i = 0; i < count; i++) {
		y = a2l[*ptr++];
		if (y > 0)
			amplitude += (y - amplitude) / EXPAVG;
		else
			amplitude += (-y - amplitude) / EXPAVG;

		for(j = 0; j < NTONES; j++) {
			c = (POLERAD * (y - k[j])) / SCALE;
			d = y + c;
			f = (p[j] * (d - h[j])) / SCALE;
			n = y - k[j] - c;
			if (n < 0)
				n = -n;
			k[j] = h[j] + f;
			h[j] = f + d;
			tones[j] += (n - tones[j]) / EXPAVG;
		}
		sample(amplitude, tones);
	}
}

/*
 * Taste each sample, detect (loss off) carrier, and feed uart
 */

#define NCARRIER	1000	/* Samples of carrier for detection */

static void
sample(int vol, int* tones)
{
	static int carrier;
	
	if ((tones[0] + tones[1]) > vol * 3/2) {	/* XXX */
		if (carrier < NCARRIER)
			carrier ++;
	} else {
		if (carrier > 0)
			carrier --;
	}

	if (!dcd && carrier > NCARRIER / 2) {
		syslog(LOG_ERR, "CARRIER ON");
		dcd = 1;
	} else if (dcd && carrier < NCARRIER / 2) {
		syslog(LOG_ERR, "CARRIER OFF");
		dcd = 0;
	}

	if (!dcd)
		return;
		
	if (tones[0] > tones[1]) {
		uart(1);
	} else {
		uart(0);
	}
}

/*
 * A UART in software
 */

#define BITCENTER	13	/* Middle of a bit: 8000/300/2 */
static int bitsample[] = {	/* table of sampling points */
	BITCENTER,
	BITCENTER + 27,
	BITCENTER + 54,
	BITCENTER + 80,
	BITCENTER + 107,
	BITCENTER + 134,
	BITCENTER + 160,
	BITCENTER + 187,
	BITCENTER + 214,
	BITCENTER + 240
};

static void
uart(int bit)
{
	static int n, v, j;

	if (n == 0 && bit == 1)	
		return;		/* Waiting for start bit */
	if (n == 0) {
		j = 0;		/* Begin start bit */
		v = 0;
		n++;
	} else if (j == 0 && bit && n > bitsample[j]) {
		n = 0;		/* Gone by middle of start bit */
	} else if (n > bitsample[j]) {
		j++;		/* Sample point */
		if (j == 10) {
			n = 0;
			input_byte(v, bit);
		} else {
			v = v / 2 + 128 * bit;
			n++;
		}
	} else {
		n++;
	}
}

/*
 * Send a byte using kenrnel tone generation support
 */

static void
output_byte(int val)
{
	struct i4b_tel_tones tt;
	int i;

	i = 0;
	tt.frequency[i] = 1850; tt.duration[i++] = 27;

	tt.frequency[i] = val &   1 ? 1650 : 1850; tt.duration[i++] = 27;
	tt.frequency[i] = val &   2 ? 1650 : 1850; tt.duration[i++] = 26;
	tt.frequency[i] = val &   4 ? 1650 : 1850; tt.duration[i++] = 27;
	tt.frequency[i] = val &   8 ? 1650 : 1850; tt.duration[i++] = 27;
	tt.frequency[i] = val &  16 ? 1650 : 1850; tt.duration[i++] = 26;
	tt.frequency[i] = val &  32 ? 1650 : 1850; tt.duration[i++] = 27;
	tt.frequency[i] = val &  64 ? 1650 : 1850; tt.duration[i++] = 27;
	tt.frequency[i] = val & 128 ? 1650 : 1850; tt.duration[i++] = 26;

	tt.frequency[i] = 1650; tt.duration[i++] = 27;
	tt.frequency[i] = 1650; tt.duration[i++] = 0;

	i = ioctl(telfd, I4B_TEL_TONES, &tt);
	if (i != 0 && errno != EAGAIN) {
		syslog(LOG_ERR, "%d: *** %d/%d ***", __LINE__, i, errno);
		exit(0);
	}
}

/*
 * Create Session
 */

static void
create_session(void)
{
	int i;
	char buf[100];

	i = forkpty(&ptyfd, buf, 0, 0);
	if (i == 0) {
		execl("/usr/libexec/getty", "getty", "std.300", "-",
		    (char *)NULL);
		syslog(LOG_ERR, "exec getty %d", errno);
		exit(2);
	} else if (i < 0) {
		syslog(LOG_ERR, "forkpty failed %d", errno);
		exit(2);
	}
	syslog(LOG_ERR, "pty %s", buf);
}

static void 
input_byte(int byte, int stopbit)
{
	u_char c;
	int i;
	static int first;
	static u_char buf[80];

	if (!stopbit)
		return;
	c = byte;
	/*
	 * I have no idea why, but my TB2500 modem sends a sequence of
	 * 28 bytes after carrier is established at the link level, but
	 * before it is acceptted at the logical level.
	 *
	 *  [16100214010201060100000000ff0201020301080402400010034510]
	 *
	 * Unfortunately this contains a ^D which kills getty.
	 * The following code swallows this sequence, assuming that it
	 * is always the same length and always start with 0x16.
	 *
	 */
	if (first == 0 && c == 0x16) {
		sprintf(buf, "%02x", c);
		first = 27;
		return;
	} else if (first == 0) {
		first = -1;
		dcd = 2;
		return;
	}
	if (first > 0) {
		sprintf(buf + strlen(buf), "%02x", c);
		first--;
		if (!first) {
			syslog(LOG_NOTICE, "Got magic [%s]", buf);
			*buf = 0;
		}
		return;
	}
	if (ptyfd != -1 && dcd) {
		i = write(ptyfd, &c, 1);
		if (i != 1 && errno != EAGAIN) {
		syslog(LOG_ERR, "%d: *** %d/%d ***", __LINE__, i, errno);
			exit(0);
		}
	}
}

int
main(int argc, char **argv)
{
	char *device = "/dev/tel0";
	u_char ibuf[2048];
	int ii, io;
	int i, maxfd;
	struct i4b_tel_tones tt;
	fd_set rfd, wfd, efd;

	openlog("v21modem", LOG_PID, LOG_DAEMON);
	/* Find our device name */
	for (i = 0; i < argc; i++)
		if (!strcmp(argv[i], "-D"))
			device = argv[i + 1];
	telfd = open(device, O_RDWR, 0);
	if (telfd < 0) {
		syslog(LOG_ERR, "open %s: %m", device);
		exit (0);
	}
	syslog(LOG_NOTICE, "Running on %s", device);

	/* Output V.25 tone and carrier */
	i = 0;
	tt.frequency[i] =    0; tt.duration[i++] = 1000;
	tt.frequency[i] = 2100; tt.duration[i++] = 2*8000;
	tt.frequency[i] =    0; tt.duration[i++] = 400;
	tt.frequency[i] = 1650; tt.duration[i++] = 1;
	tt.frequency[i] = 1650; tt.duration[i++] = 0;
	tt.frequency[i] =    0; tt.duration[i++] = 0;
	i = ioctl(telfd, I4B_TEL_TONES, &tt);
	if (i < 0) {
		syslog(LOG_ERR, "hangup");
		exit(0);
	}

	create_session();

	/* Wait for carrier */
	do {
		ii = read(telfd, ibuf, sizeof ibuf);
		tonedetect(ibuf, ii);
	} while (ii > 0 && dcd != 2);
	if (ii < 0) {
		syslog(LOG_ERR, "hangup");
		exit(0);
	}

	maxfd = ptyfd;
	if (telfd > maxfd)
		maxfd = telfd;
	maxfd += 1;
	do {
		FD_ZERO(&rfd);
		FD_SET(telfd, &rfd);
		FD_SET(ptyfd, &rfd);
		FD_ZERO(&wfd);
		FD_ZERO(&efd);
		FD_SET(telfd, &efd);
		FD_SET(ptyfd, &efd);
		i = select(maxfd, &rfd, &wfd, &efd, NULL);
		if (FD_ISSET(telfd, &rfd)) {
			ii = read(telfd, ibuf, sizeof ibuf);
			if (ii > 0)
				tonedetect(ibuf, ii);
			else
				syslog(LOG_ERR, "hangup");
		}
		if (FD_ISSET(ptyfd, &rfd)) {
			io = read(ptyfd, ibuf, 1);
			if (io == 1)
				output_byte(*ibuf);
			else if (io == 0) {
				syslog(LOG_ERR, "Session EOF");
				exit(0);
			}
			
		}
		if (FD_ISSET(telfd, &efd)) {
			syslog(LOG_ERR, "Exception TELFD");
			exit (0);
		}
		if (FD_ISSET(ptyfd, &efd)) {
			syslog(LOG_ERR, "Exception PTYFD");
			exit (0);
		}
	} while (dcd);
	syslog(LOG_ERR, "Carrier Lost");
	exit(0);
}
