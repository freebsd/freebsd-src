/*
 * QuickCam(TM) driver control program.
 * Copyright (c) 1996, Paul Traina.
 *
 * QuickCam(TM) is a registered trademark of Connectix Inc.
 * Use this driver at your own risk, it is not warranted by
 * Connectix or the authors.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <machine/qcam.h>

#ifdef	LINUX
#include <getopt.h>
#endif

void print_data(struct qcam *data)
{
	fprintf(stderr, "version=%d, (%d,%d) at (%d,%d) @%dbpp "
		        "zoom=%d, exp=%d, b/w/c=%d/%d/%d\n",
			data->qc_version,
			data->qc_xsize, data->qc_ysize,
			data->qc_xorigin, data->qc_yorigin,
			data->qc_bpp,
			data->qc_zoom,
			data->qc_exposure,
			data->qc_brightness,
			data->qc_whitebalance,
			data->qc_contrast);
}

usage(void)
{
	fprintf(stderr, "usage: qcamcontrol [-p port] [-x xsize] [-y ysize] "
			"[-z zoom] [-d depth]\n"
			"                   [-b brightness] [-w whitebal] "
			"[-c contrast] [-e exposure]\n");
	exit(2);
}

main(int argc, char **argv)
{
	struct qcam info;
	int fd, len;
	size_t bytes;
	char opt;

	static char buffer[QC_MAX_XSIZE*QC_MAX_YSIZE];

	char *port = "/dev/qcam0";
	int x_size, y_size, zoom, depth, brightness, whitebalance, contrast,
	    exposure;

	/*
	 * Default everything to unset.
	 */
	x_size = y_size = zoom = depth = brightness = whitebalance =
	         contrast = exposure = -1;

	while ((opt = getopt(argc, argv, "p:x:y:z:d:b:w:c:e:")) != EOF) {
	    switch (opt) {
	    case 'p':
		port = optarg;
		break;

	    case 'x':
		x_size = atoi(optarg);
		if (x_size > QC_MAX_XSIZE) {
		    fprintf(stderr, "x size too large (max %d)\n",
			    QC_MAX_XSIZE);
		    exit(2);
		}
		break;

	    case 'y':
		y_size = atoi(optarg);
		if (y_size > QC_MAX_YSIZE) {
		    fprintf(stderr, "x size too large (max %d)\n",
			    QC_MAX_YSIZE);
		    exit(2);
		}
		break;

	    case 'z':
		zoom = atoi(optarg);
		if (zoom > QC_ZOOM_200) {
		    fprintf(stderr, "zoom too large (max %d)\n", QC_ZOOM_200);
		    exit(2);
		}
		break;

	    case 'd':
		depth = atoi(optarg);
		if (depth != 4 && depth != 6) {
		    fprintf(stderr, "invalid depth (4 or 6)\n");
		    exit(2);
		}
		break;

	    case 'b':
		brightness = atoi(optarg);
		if (brightness > 255) {
		    fprintf(stderr, "bad brightness (max 255)\n");
		    exit(2);
		}
		break;

	    case 'w':
		whitebalance = atoi(optarg);
		if (whitebalance > 255) {
		    fprintf(stderr, "bad white balance (max 255)\n");
		    exit(2);
		}
		break;

	    case 'c':
		contrast = atoi(optarg);
		if (contrast > 255) {
		    fprintf(stderr, "bad contrast (max 255)\n");
		    exit(2);
		}
		break;

	    case 'e':
		exposure = atoi(optarg);
		if (exposure < 100) {
		    fprintf(stderr, "bad exposure (min 100)\n");
		    exit(2);
		}
		break;

	    default:
		usage();
	    }
	}
	argc -= optind;
	argv += optind;

	/* open device */
	if ((fd = open(port, O_RDONLY)) < 0) {
		perror("open");
		exit(1);
	}



	if (ioctl(fd, QC_GET, &info) < 0) {	/* read in default info */
		perror("ioctl(QC_GET)");
		exit(1);
	}

	if (x_size > -1)
		info.qc_xsize = x_size;
	if (y_size > -1)
		info.qc_ysize = y_size;
	if (depth > -1)
		info.qc_bpp = depth;
	if (zoom > -1)
		info.qc_zoom  = zoom;
	if (brightness > -1)
		info.qc_brightness = brightness;
	if (whitebalance > -1)
		info.qc_whitebalance = whitebalance;
	if (contrast > -1)
		info.qc_contrast = contrast;
	if (exposure > -1)
		info.qc_exposure = exposure;

	/*
	 * make sure we're in sync with the kernel version of the driver
	 * ioctl structure
	 */
	info.qc_version = QC_IOCTL_VERSION;

	if (ioctl(fd, QC_SET, &info) < 0) {
		perror("ioctl(QC_SET)");
		exit(1);
	}

	/*
	 * Tell us what the kernel thinks we're asking for
	 */
	if (ioctl(fd, QC_GET, &info) < 0) {
		perror("ioctl(QC_SET)");
		exit(1);
	}

	print_data(&info);

	/*
	 * Grab a frame -- a single read will always work, but give a
	 * particularly paranoid example.
	 */
	len = info.qc_xsize * info.qc_ysize;
	while (len) {
		bytes = read(fd, buffer, len);
		if (bytes < 0) {
			perror("read");
			exit(1);
		}
		len -= bytes;

		if (bytes == 0)
			exit(0);
	}

	/*
	 * Write the frame to stdout as a PGM image.
	 */
	fprintf(stdout, "P5\n%d %d\n%d\n",
		info.qc_xsize, info.qc_ysize, (1<<info.qc_bpp) - 1);
	fflush(stdout);

	if (write(1, buffer, info.qc_xsize * info.qc_ysize) < 0) {
		perror("write");
		exit(1);
	}
}
