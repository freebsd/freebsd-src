/* capture a PPM image using YUV planer format and single capture mode */
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


typedef unsigned char uint8;
typedef signed char int8;

static uint8 *yuv_data;
static int8 *ue, *uo, *ve, *vo;
extern int errno;
#define ROWS 480
#define COLS 640
#define SIZE (ROWS * COLS * 2)
main()
{
	struct meteor_geomet geo;
	char b[4],header[16],*p;
	int i,o,c,r;
	int y1,y2,u,v;
	int temp;

	if ((i = open("/dev/meteor", O_RDONLY)) < 0) {
		printf("open failed: %d\n", errno);
		exit(1);
	}
				/* set up the capture type and size */
        geo.rows = ROWS;
        geo.columns = COLS;
        geo.frames = 1;


        geo.oformat = METEOR_GEO_YUV_PLANER;

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

        yuv_data = (uint8 *)mmap((caddr_t)0,SIZE,PROT_READ,MAP_SHARED, i, (off_t)0);

	if (yuv_data == (uint8 *) MAP_FAILED) return (0);

	temp = ROWS * COLS;
	ue = yuv_data + temp;
	temp = temp / 4;
	ve = ue + temp;
	uo = ve + temp;
	vo = uo + temp;

printf("data locations = %08x %08x %08x %08x %08x \n", yuv_data,ue,ve,uo,vo);

	c = METEOR_CAP_SINGLE ;
        ioctl(i, METEORCAPTUR, &c);
  
	printf("read done %d %d\n", errno, i);
	if ((o = open("yuvpl.ppm", O_WRONLY | O_CREAT, 0644)) < 0) {
		printf("ppm open failed: %d\n", errno);
		exit(1);
	}

		/* make PPM header and save to file */
	strcpy(&header[0], "P6 640 480 255 ");
	header[2] = header[6]  = header[10] = header[14] = '\n';
	write (o, &header[0], 15);

	for (r = 0 ; r < ROWS ; r++) {
	   for (c = 0 ; c < COLS / 2; c++) {
		if ((y1 = *yuv_data++) < 0) y1 += 256;
		if ((y2 = *yuv_data++) < 0) y2 += 256;
		if (r & 1) { /* odd */
			u =   *uo++ ;
			v =   *vo++;
		}
		else { /* even */
			u =   *ue++;
			v =   *ve++;
		}
		b[0]= (double)y1  + 1.375 * (double)v ; /*r*/
		b[1]= (double)y1 - 0.703125 * (double)v -0.34375 * (double)u ;	/* g */
		b[2]= (double)y1 + 1.734375 * (double)u ;		/* b */
		b[3]= (double)y2  + 1.375 * (double)v ; /*r*/
		b[4]= (double)y2 - 0.703125 * (double)v -0.34375 * (double)u ;	/* g */
		b[5]= (double)y2 + 1.734375 * (double)u ;		/* b */
		write(o,&b[0], 6);
	 }
	}
	close(o);
	close(i);
	exit(0);
}
