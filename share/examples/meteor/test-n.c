/* A simple program to test the communication between the matrox meteor
 * driver and an user application in the continous sync capture mode.
 * 
 * First the driver clears the mask and decrements the counter like it
 * would in a normal application. Then it purpose does not handle these
 * responsibilities to simulate an application falling behind. I use
 * the HUP signal to work as if the some of the  buffers were removed
 * (the second couter is used to cleared the correct bits.
 *
 *	build kernel with at least:
 *
 *		options "METEOR_ALLOC_PAGES=301"  
 *
 */
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
/*#include <machine/ioctl_meteor.h> */
#include "/sys/i386/include/ioctl_meteor.h"

typedef unsigned char uint8;
typedef signed char int8;

int i;
static uint8 *y;
extern int errno;
struct meteor_mem *common_mem;
int sig_cnt;
int sig_cnt2;

void
hup_catcher()
{
	/* clear 4 oldest active bits */
	common_mem->active &= ~(1 << (sig_cnt2++ % 32));
	common_mem->active &= ~(1 << (sig_cnt2++ % 32));
	common_mem->active &= ~(1 << (sig_cnt2++ % 32));
	common_mem->active &= ~(1 << (sig_cnt2++ % 32));
	/* lowat is low enough that I will need 2 hups to start saving again */
	common_mem->num_active_bufs -= 4;
	puts("hup caught");
}

void
usr2_catcher()
{
	int j;
	struct meteor_capframe capframe;

	printf("active %3d = %08x ", sig_cnt,common_mem->active);
	printf("# act  %3d = %d\n", sig_cnt, common_mem->num_active_bufs);

	if (sig_cnt < 80) {
		common_mem->active &= ~(1 << (sig_cnt % 32));
		common_mem->num_active_bufs--; 
		sig_cnt2++;
	}

	printf("data %08x\n", *((u_long *) (y + (sig_cnt % 32) *
						common_mem->frame_size)));
	if (++sig_cnt >= 200) {
		capframe.command=METEOR_CAP_STOP_FRAMES;

		if (ioctl(i, METEORCAPFRM, &capframe) < 0) {
			printf("METEORCAPFRM failed %d\n", errno);
			exit(1);
		}
		exit (0);
	}
}

main()
{
	struct meteor_geomet geo;
	int height, width, depth, frames, size;
	struct meteor_capframe capframe;

	if ((i = open("/dev/meteor", O_RDONLY)) < 0) {
		printf("open failed\n");
		exit(1);
	}
	printf("test %d %d\n", errno, i);

        height = geo.rows = 120;
        width= geo.columns = 160;
        frames = geo.frames = 32;
	depth = 2;			/* 2 bytes per pixel */

	printf("ioctl %d %d\n", errno, i);

        geo.oformat = METEOR_GEO_RGB16;

        if (ioctl(i, METEORSETGEO, &geo) < 0) {
		printf("METEORSETGEO failed %d\n", errno);
		exit(1);
	}

	printf("mmap %d %d\n", errno, i);
	size = ((width*height*depth*frames+4095)/4096)*4096;
        y=(uint8 *) mmap((caddr_t)0, size + 4096, PROT_READ |PROT_WRITE,MAP_SHARED, i, (off_t)0);

	if (y == (uint8 *) MAP_FAILED) return (0);

	common_mem = (struct meteor_mem *) (y + size);

	signal(1, hup_catcher);
	signal(31, usr2_catcher);

	capframe.command=METEOR_CAP_N_FRAMES;
	capframe.signal=31;
	capframe.lowat=25;
	capframe.hiwat=30;

	printf("ioctl %d %d\n", errno, i);
        if (ioctl(i, METEORCAPFRM, &capframe) < 0) {
		printf("METEORCAPFRM failed %d\n", errno);
		exit(1);
	}

	printf("signal = %d\n", common_mem->signal);
	printf("frame size = %d\n", common_mem->frame_size);
	printf("buffers = %d\n", common_mem->num_bufs);
	printf("hiwater = %d\n", common_mem->hiwat);
	printf("lowater = %d\n", common_mem->lowat);
	printf("active = %08x\n", common_mem->active);
	printf("# active = %d\n", common_mem->num_active_bufs);
  
	printf("sleep loop\n", errno, i);
	while (1) {
		sleep (60);
	}
}
