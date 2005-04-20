	/* capture a PPM file using 24 bit RGB image and read() */
/* Copyright (c) 1995 Mark Tinguely and Jim Lowe 
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
 *      This product includes software developed by Mark Tinguely and Jim Lowe
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/fcntl.h>
#include <dev/bktr/ioctl_meteor.h>

extern int errno;
#define ROWS 300
#define COLS 400
#define SIZE (ROWS * COLS * 4)
main()
{
	struct meteor_geomet geo;
	char buf[SIZE],b[4],header[16],*p;
	int i,o,c;

	if ((i = open("/dev/meteor", O_RDONLY)) < 0) {
		printf("open failed: %d\n", errno);
		exit(1);
	}
				/* set up the capture type and size */
        geo.rows = ROWS;
        geo.columns = COLS;
        geo.frames = 1;
        geo.oformat = METEOR_GEO_RGB24 ;

        if (ioctl(i, METEORSETGEO, &geo) < 0) {
		printf("ioctl failed: %d\n", errno);
		exit(1);
	}

	c = METEOR_FMT_NTSC;

        if (ioctl(i, METEORSFMT, &c) < 0) {
		printf("ioctl failed: %d\n", errno);
		exit(1);
	}

	c = METEOR_INPUT_DEV0;

        if (ioctl(i, METEORSINPUT, &c) < 0) {
		printf("ioctl failed: %d\n", errno);
		exit(1);
	}

	if ((c=read(i, &buf[0], SIZE)) < SIZE) {
		printf("read failed %d %d %d\n", c, i, errno);
		close(i);
		exit(1);
	}
	close(i);

	if ((o = open("rgb24.ppm", O_WRONLY | O_CREAT, 0644)) < 0) {
		printf("ppm open failed: %d\n", errno);
		exit(1);
	}

		/* make PPM header and save to file */
	strcpy(&header[0], "P6 400 300 255 ");
	header[2] = header[6]  = header[10] = header[14] = '\n';
	write (o, &header[0], 15);
		/* save the RGB data to PPM file */
	for (p = &buf[0]; p < &buf[SIZE]; ) {
		b[2] = *p++;		/* blue */
		b[1] = *p++;		/* green */
		b[0] = *p++;		/* red */
		*p++;			/* NULL byte */
		write(o,&b[0], 3);	/* not very efficient */
	}
	close(o);
	exit(0);
}
