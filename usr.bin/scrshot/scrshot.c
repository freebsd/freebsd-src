/*-
 * Copyright (c) 2001 Joel Holveck and Nik Clayton
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 *
 * $FreeBSD$
 */

#include <sys/types.h>
#include <sys/consio.h>
#include <sys/ioctl.h>
#include <sys/uio.h>

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define	 VERSION 	1	/* File format version */

/*
 * Given the path to a syscons terminal (e.g., "/dev/ttyv0"), tries to
 * snapshot the video memory of that terminal, using the CONS_SCRSHOT
 * ioctl, and writes the results to stdout.
 */
int
main(int argc, char *argv[])
{
	int fd;
	scrshot_t shot;
	vid_info_t info;

	if (argc != 2)
		errx(1, "improper # of args");

	fd = open(argv[1], O_RDWR);
	if (fd < 0)
		err(1, "%s", argv[1]);
	
	info.size = sizeof(info);
	if (ioctl(fd, CONS_GETINFO, &info) == -1)
		err(1, "ioctl(CONS_GETINFO)");
	
	shot.buf = malloc(info.mv_csz * info.mv_rsz * sizeof(u_int16_t));
	if (shot.buf == NULL)
		err(1, "couldn't allocate shot space");
	
	shot.xsize = info.mv_csz;
	shot.ysize = info.mv_rsz;
	if (ioctl(fd, CONS_SCRSHOT, &shot) == -1)
		err(1, "ioctl(CONS_SCRSHOT)");

	printf("SCRSHOT_%c%c%c%c", VERSION, 2, shot.xsize, shot.ysize);
	fflush(stdout);
	
	(void)write(STDOUT_FILENO, shot.buf,
	    shot.xsize * shot.ysize * sizeof(u_int16_t));

	exit(0);
}
