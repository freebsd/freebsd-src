/* capture a PPM image using RGB16  and single capture mode */

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
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/fcntl.h>
/*#include <machine/ioctl_meteor.h>*/
#include "/sys/i386/include/ioctl_meteor.h"

extern int errno;
#define ROWS 480
#define COLS 640
#define SIZE (ROWS * COLS * 2)
main()
{
	struct meteor_geomet geo;
	short *rgb16;
	char b[4];
	char header[16];
	int i,o,c;

	if ((i = open("/dev/meteor", O_RDONLY)) < 0) {
		printf("open failed: %d\n", errno);
		exit(1);
	}
				/* set up the capture type and size */
        geo.rows = ROWS;
        geo.columns = COLS;
        geo.frames = 1;

        geo.oformat = METEOR_GEO_RGB16;

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

        rgb16 = (short *)mmap((caddr_t)0,SIZE,PROT_READ,MAP_SHARED, i, (off_t)0);

	if (rgb16 == (short *) MAP_FAILED) return (0);

	c = METEOR_CAP_SINGLE ;
        ioctl(i, METEORCAPTUR, &c);
  
	if ((o = open("rgb16.ppm", O_WRONLY | O_CREAT, 0644)) < 0) {
		printf("ppm open failed: %d\n", errno);
		exit(1);
	}

		/* make PPM header and save to file */
	strcpy(&header[0], "P6 640 480 255 ");
	header[2] = header[6]  = header[10] = header[14] = '\n';
	write (o, &header[0], 15);

	for (c = 0 ; c < ROWS*COLS; c++) {
		b[0]= ((*rgb16 >> 7) & 0xf8);	/* r */
		b[1]= ((*rgb16 >> 2) & 0xf8);	/* g */
		b[2]= ((*rgb16++ << 3) & 0xf8);	/* b */
		write(o, &b[0], 3);
	}
	close(o);
	close(i);
	exit(0);
}
