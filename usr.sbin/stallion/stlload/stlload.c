/*****************************************************************************/

/*
 * stlload.c  -- stallion intelligent multiport down loader.
 *
 * Copyright (c) 1994-1996 Greg Ungerer (gerg@stallion.oz.au).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Greg Ungerer.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*****************************************************************************/

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include <machine/cdk.h>

/*****************************************************************************/

char	*version = "0.0.5";
char	*defdevice = "/dev/staliomem%d";
char	*image = BOOTDIR "/cdk.sys";
char	*oldimage = BOOTDIR "/2681.sys";

char	*progname;
char	*memdevice;
char	devstr[128];
int	brdnr = 0;
int	verbose = 0;
int	reset = 0;

/*
 *	Define a local buffer for copying the image into the shared memory.
 */
#define	BUFSIZE		4096

char	buf[BUFSIZE];

/*
 *	Define the timeout length when waiting for slave to start up.
 *	The quantity is measured in seconds.
 */
#define	TIMEOUT		5

/*
 *	Set up a default feature area structure.
 */
cdkfeature_t	feature = { 0, 0, ETYP_CDK, 0, 0, 0, 0, 0 };

/*
 *	Have local copies of the board signatures ready.
 */
cdkecpsig_t	ecpsig;
cdkonbsig_t	onbsig;

/*****************************************************************************/

/*
 *	Declare internal function prototypes here.
 */
void	usage(void);
int	ecpfindports(cdkecpsig_t *sigp);
int	onbfindports(cdkonbsig_t *sigp);
int	download(void);

/*****************************************************************************/

void usage()
{
	fprintf(stderr, "Usage: %s [OPTION]\n\n", progname);
	fprintf(stderr, "    -h      print this information\n");
	fprintf(stderr, "    -v      print full diagnostic trace\n");
	fprintf(stderr, "    -V      show version information and exit\n");
	fprintf(stderr, "    -i      specify image file to use\n");
	fprintf(stderr, "    -b      download board number\n");
	fprintf(stderr, "    -d      specify memory device to use\n");
	fprintf(stderr, "    -B      enable slave boot banner\n");
	fprintf(stderr, "    -R      reset board only\n");
	fprintf(stderr, "    -t      set size of TX slave buffer\n");
	fprintf(stderr, "    -r      set size of RX slave buffer\n");
	exit(0);
}

/*****************************************************************************/

/*
 *	Given a boards signature determine how many ports it has. We need to
 *	know this to setup the slave feature arguments. This function is for
 *	ECP boards only.
 */

int ecpfindports(cdkecpsig_t *sigp)
{
	unsigned int	id;
	int		bank, nrports;

	nrports = 0;
	for (bank = 0; (bank < 8); bank++) {
		id = (unsigned int) sigp->panelid[bank];
		if (id == 0xff)
			break;
		if ((id & 0x07) != bank)
			break;
		if (id & 0x20) {
			nrports += 16;
			bank++;
		} else {
			nrports += 8;
		}
	}

	return(nrports);
}

/*****************************************************************************/

/*
 *	Given a boards signature determine how many ports it has. We need to
 *	know this to setup the slave feature arguments. This function is for
 *	ONboards and Brumbys.
 */

int onbfindports(cdkonbsig_t *sigp)
{
	int	i, nrports;

	if (sigp->amask1) {
		nrports = 32;
	} else {
		for (i = 0; (i < 16); i++) {
			if (((sigp->amask0 << i) & 0x8000) == 0)
				break;
		}
		nrports = i;
	}

	return(nrports);
}

/*****************************************************************************/

/*
 *	Download an image to the slave board. There is a long sequence of
 *	things to do to get the slave running, but it is basically a simple
 *	process. Main things to do are: copy slave image into shared memory,
 *	start slave running and then read shared memory map.
 */

int download()
{
	unsigned char	alivemarker;
	time_t		strttime;
	int		memfd, ifd;
	int		nrdevs, sigok, n, rc;

	if (verbose)
		printf("Opening shared memory device %s\n", memdevice);
	if ((memfd = open(memdevice, O_RDWR)) < 0) {
		fprintf(stderr,
			"%s: failed to open memory device %s, errno=%d\n",
			progname, memdevice, errno);
		return(-1);
	}

/*
 *	Before starting the download must tell driver that we are about to
 *	stop its slave. This is only important if it is already running.
 *	Once we have told the driver its stopped then do a hardware reset
 *	on it, to get it into a known state.
 */
	if (verbose)
		printf("Stoping any current slave\n");
	if (ioctl(memfd, STL_BSTOP, 0) < 0) {
		fprintf(stderr, "%s: ioctl(STL_BSTOP) failed, errno=%d\n",
			progname, errno);
		return(-1);
	}

	if (verbose)
		printf("Reseting the board\n");
	if (ioctl(memfd, STL_BRESET, 0) < 0) {
		fprintf(stderr, "%s: ioctl(STL_BRESET) failed, errno=%d\n",
			progname, errno);
		return(-1);
	}
	if (reset)
		return(0);

/*
 *	After reseting the board we need to send an interrupt to the older
 *	board types to get them to become active. Do that now.
 */
	if (verbose)
		printf("Interrupting board to activate shared memory\n");
	if (ioctl(memfd, STL_BINTR, 0) < 0) {
		fprintf(stderr, "%s: ioctl(STL_BINTR) failed, errno=%d\n",
			progname, errno);
		return(-1);
	}
	/*sleep(1);*/

	if (verbose)
		printf("Opening slave image file %s\n", image);
	if ((ifd = open(image, O_RDONLY)) < 0) {
		fprintf(stderr, "%s: failed to open image file %s, errno=%d\n",
			progname, image, errno);
		return(-1);
	}

/*
 *	At this point get the signature of the board from the shared memory.
 *	Do a double check that it is a board we know about. We will also need
 *	to calculate the number of ports on this board (to use later).
 */
	sigok = 0;
	if (verbose)
		printf("Reading ROM signature from board\n");

	if (lseek(memfd, CDK_SIGADDR, SEEK_SET) != CDK_SIGADDR) {
		fprintf(stderr,
			"%s: lseek(%x) failed on memory file, errno=%d\n",
			progname, CDK_FEATADDR, errno);
		return(-1);
	}
	if (read(memfd, &ecpsig, sizeof(cdkecpsig_t)) < 0) {
		fprintf(stderr, "%s: read of ROM signature failed, errno=%d\n",
			progname, errno);
		return(-1);
	}
	if (ecpsig.magic == ECP_MAGIC) {
		nrdevs = ecpfindports(&ecpsig);
		if (nrdevs < 0)
			return(-1);
		sigok++;
	}

	if (lseek(memfd, CDK_SIGADDR, SEEK_SET) != CDK_SIGADDR) {
		fprintf(stderr,
			"%s: lseek(%x) failed on memory file, errno=%d\n",
			 progname, CDK_FEATADDR, errno);
		return(-1);
	}
	if (read(memfd, &onbsig, sizeof(cdkonbsig_t)) < 0) {
		fprintf(stderr, "%s: read of ROM signature failed, errno=%d\n",
			progname, errno);
		return(-1);
	}
	if ((onbsig.magic0 == ONB_MAGIC0) && (onbsig.magic1 == ONB_MAGIC1) &&
			(onbsig.magic2 == ONB_MAGIC2) &&
			(onbsig.magic3 == ONB_MAGIC3)) {
		nrdevs = onbfindports(&onbsig);
		if (nrdevs < 0)
			return(-1);
		sigok++;
	}

	if (! sigok) {
		fprintf(stderr, "%s: unknown signature from board\n", progname);
		return(-1);
	}

	if (verbose)
		printf("Board signature reports %d ports\n", nrdevs);

/*
 *	Start to copy the image file into shared memory. The first thing to
 *	do is copy the vector region in from shared memory address 0. We will
 *	then skip over the signature and feature area and start copying the
 *	actual image data and code from 4k upwards.
 */
	if (verbose)
		printf("Copying vector table into shared memory\n");
	if ((n = read(ifd, buf, CDK_SIGADDR)) < 0) {
		fprintf(stderr, "%s: read of image file failed, errno=%d\n",
			progname, errno);
		return(-1);
	}
	if (lseek(memfd, 0, SEEK_SET) != 0) {
		fprintf(stderr,
			"%s: lseek(%x) failed on memory file, errno=%d\n",
			progname, CDK_FEATADDR, errno);
		return(-1);
	}
	if (write(memfd, buf, n) < 0) {
		fprintf(stderr,
			"%s: write to memory device failed, errno=%d\n",
			progname, errno);
		return(-1);
	}

	if (lseek(ifd, 0x1000, SEEK_SET) != 0x1000) {
		fprintf(stderr,
			"%s: lseek(%x) failed on image file, errno=%d\n",
			progname, CDK_FEATADDR, errno);
		return(-1);
	}
	if (lseek(memfd, 0x1000, SEEK_SET) != 0x1000) {
		fprintf(stderr,
			"%s: lseek(%x) failed on memory device, errno=%d\n",
			progname, CDK_FEATADDR, errno);
		return(-1);
	}

/*
 *	Copy buffer size chunks of data from the image file into shared memory.
 */
	do {
		if ((n = read(ifd, buf, BUFSIZE)) < 0) {
			fprintf(stderr,
				"%s: read of image file failed, errno=%d\n",
				progname, errno);
			return(-1);
		}
		if (write(memfd, buf, n) < 0) {
			fprintf(stderr,
				"%s: write to memory device failed, errno=%d\n",
				progname, errno);
			return(-1);
		}
	} while (n == BUFSIZE);

	close(ifd);

/*
 *	We need to down load the start up parameters for the slave. This is
 *	done via the feature area of shared memory. Think of the feature area
 *	as a way of passing "command line" arguments to the slave.
 *	FIX: should do something here to load "brdspec" as well...
 */
	feature.nrdevs = nrdevs;
	if (verbose)
		printf("Loading features into shared memory\n");
	if (lseek(memfd, CDK_FEATADDR, SEEK_SET) != CDK_FEATADDR) {
		fprintf(stderr,
			"%s: lseek(%x) failed on memory device, errno=%d\n",
			progname, CDK_FEATADDR, errno);
		return(-1);
	}
	if (write(memfd, &feature, sizeof(cdkfeature_t)) < 0) {
		fprintf(stderr,
			"%s: write to memory device failed, errno=%d\n",
			progname, errno);
		return(-1);
	}

/*
 *	Wait for board alive marker to be set. The slave image will set the
 *	byte at address CDK_RDYADDR to 0x13 after it has successfully started.
 *	If this doesn't happen we timeout and fail.
 */
	if (verbose)
		printf("Setting alive marker to 0\n");
	if (lseek(memfd, CDK_RDYADDR, SEEK_SET) != CDK_RDYADDR) {
		fprintf(stderr,
			"%s: lseek(%x) failed on memory device, errno=%d\n",
			progname, CDK_RDYADDR, errno);
		return(-1);
	}
	alivemarker = 0;
	if (write(memfd, &alivemarker, 1) < 0) {
		fprintf(stderr,
			"%s: write to memory device failed, errno=%d\n",
			progname, errno);
		return(-1);
	}

/*
 *	At this point the entire image is loaded into shared memory. To start
 *	it executiong we poke the board with an interrupt.
 */
	if (verbose)
		printf("Interrupting board to start slave image\n");
	if (ioctl(memfd, STL_BINTR, 0) < 0) {
		fprintf(stderr, "%s: ioctl(STL_BINTR) failed, errno=%d\n",
			progname, errno);
		return(-1);
	}

	strttime = time((time_t *) NULL);
	if (verbose)
		printf("Waiting for slave alive marker, time=%x timeout=%d\n",
			strttime, TIMEOUT);
	while (time((time_t *) NULL) < (strttime + TIMEOUT)) {
		if (lseek(memfd, CDK_RDYADDR, SEEK_SET) != CDK_RDYADDR) {
			fprintf(stderr,
				"%s: lseek(%x) failed on memory device, "
				"errno=%d\n", progname, CDK_RDYADDR, errno);
			return(-1);
		}
		if (read(memfd, &alivemarker, 1) < 0){
			fprintf(stderr,
				"%s: read of image file failed, errno=%d\n",
				progname, errno);
			return(-1);
		}
		if (alivemarker == CDK_ALIVEMARKER)
			break;
	}

	if (alivemarker != CDK_ALIVEMARKER) {
		fprintf(stderr, "%s: slave image failed to start\n", progname);
		return(-1);
	}

	if (lseek(memfd, CDK_RDYADDR, SEEK_SET) != CDK_RDYADDR) {
		fprintf(stderr,
			"%s: lseek(%x) failed on memory device, errno=%d\n",
			progname, CDK_RDYADDR, errno);
		return(-1);
	}
	alivemarker = 0;
	if (write(memfd, &alivemarker, 1) < 0) {
		fprintf(stderr, "%s: write to memory device failed, errno=%d\n",
			 progname, errno);
		return(-1);
	}

	if (verbose)
		printf("Slave image started successfully\n");

/*
 *	The last thing to do now is to get the driver started. Now that the
 *	slave is operational it must read in the memory map and gets its
 *	internal tables initialized.
 */
	if (verbose)
		printf("Driver initializing host shared memory interface\n");
	if (ioctl(memfd, STL_BSTART, 0) < 0) {
		fprintf(stderr, "%s: ioctl(STL_BSTART) failed, errno=%d\n",
			progname, errno);
		return(-1);
	}

	close(memfd);
	return(0);
}

/*****************************************************************************/

void main(int argc, char *argv[])
{
	struct stat	statinfo;
	int		optind, c;

	optind = 0;
	progname = argv[0];

	while ((c = getopt(argc, argv, "hvVRB:i:b:d:t:r:")) != -1) {
		switch (c) {
		case 'V':
			printf("%s version %s\n", progname, version);
			exit(0);
			break;
		case 'B':
			feature.banner = atol(optarg);
			break;
		case 'h':
			usage();
			break;
		case 'v':
			verbose++;
			break;
		case 'i':
			image = optarg;
			break;
		case 'R':
			reset++;
			break;
		case 'b':
			brdnr = atoi(optarg);
			break;
		case 'd':
			memdevice = optarg;
			break;
		case 't':
			feature.txrqsize = atol(optarg);
			break;
		case 'r':
			feature.rxrqsize = atol(optarg);
			break;
		case '?':
		default:
			usage();
			break;
		}
	}

	if (memdevice == (char *) NULL) {
		if ((brdnr < 0) || (brdnr >= 8)) {
			fprintf(stderr,
				"%s: invalid board number %d specified\n",
				progname, brdnr);
			exit(1);
		}
		sprintf(devstr, defdevice, brdnr);
		memdevice = &devstr[0];
		if (verbose)
			printf("Using shared memory device %s\n", memdevice);
	}

	if (verbose)
		printf("Downloading image %s to board %d\n", image, brdnr);

/*
 *	Check that the shared memory device exits and is a character device.
 */
	if (stat(memdevice, &statinfo) < 0) {
		fprintf(stderr, "%s: memory device %s does not exist\n",
			progname, memdevice);
		exit(1);
	}
	if ((statinfo.st_mode & S_IFMT) != S_IFCHR) {
		fprintf(stderr, "%s: memory device %s is not a char device\n",
			progname, memdevice);
		exit(1);
	}

	if (stat(image, &statinfo) < 0) {
		fprintf(stderr, "%s: image file %s does not exist\n",
			progname, image);
		exit(1);
	}

/*
 *	All argument checking is now done. So lets get this show on the road.
 */
	if (download() < 0)
		exit(1);
	exit(0);
}

/*****************************************************************************/
