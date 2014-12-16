/*-
 * Copyright (c) 2013 Robert N. M. Watson
 * Copyright (c) 2014 SRI International
 * Copyright (c) 2014 Simon W. Moore
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

#include <sys/types.h>
#include <sys/endian.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "hdmi.h"

#define	PATH_TEMPFANCTL		"/dev/de4tempfan"
#define	PATH_HDMI_RESET		"/dev/de4_hdmi_reset"
#define	PATH_HDMI_I2C		"/dev/de4_hdmi_i2c"
#define PATH_PIXELSTREAM 	"/dev/ps_reg0"
#define	OFF_TEMP		0
#define	OFF_FAN			4

int	qflag = 0;		/* Quiet flag -- print only numbers, not field names. */

static void
usage(void)
{
	fprintf(stderr,
		"usage: de4ctl [-q] fan | temp\n"
		"       de4ctl hdmi reset | setup\n"
		"       de4ctl hdmires\n"
		"       de4ctl hdmires x-res y-res vertical-refresh\n");
	exit(0);
}

static void
do_temp(void)
{
	uint32_t temp;
	ssize_t len;
	int fd;

	fd = open(PATH_TEMPFANCTL, O_RDONLY);
	if (fd < 0)
		err(1, "%s", PATH_TEMPFANCTL);

	len = pread(fd, &temp, sizeof(temp), OFF_TEMP);
	if (len < 0)
		err(1, "%s", PATH_TEMPFANCTL);
	if (len != sizeof(temp))
		errx(1, "%s: short read", PATH_TEMPFANCTL);
	printf("%s%u\n", qflag ? "" : "temp:\t", le32toh(temp));
	close(fd);
}

static void
do_hdmi_reset(void)
{
	char c;
	int fd;

	fd = open(PATH_HDMI_RESET, O_WRONLY);
	if (fd < 0)
		err(1, "%s", PATH_HDMI_RESET);

	c = 0;
	if (pwrite(fd, &c, 1, 0) != 1)
		err(1, "pwrite(%s, %c, 0)", PATH_HDMI_RESET, c);
	if (!qflag)
		printf("resetting HDMI chip\n");
	c = 1;
	if (pwrite(fd, &c, 1, 0) != 1)
		err(1, "pwrite(%s, %c, 0)", PATH_HDMI_RESET, c);
	close(fd);
}

static void
do_hdmi_setup(void)
{
	int fd;

	do_hdmi_reset();

	fd = open(PATH_HDMI_I2C, O_RDWR);
	if (fd < 0)
		err(1, "%s", PATH_HDMI_I2C);

	brute_force_write_seq(fd);
	close(fd);
}

static void
do_fan(void)
{
	uint32_t fan;
	ssize_t len;
	int fd;

	fd = open(PATH_TEMPFANCTL, O_RDONLY);
	if (fd < 0)
		err(1, "%s", PATH_TEMPFANCTL);

	len = pread(fd, &fan, sizeof(fan), OFF_FAN);
	if (len < 0)
		err(1, "%s", PATH_TEMPFANCTL);
	if (len != sizeof(fan))
		errx(1, "%s: short read", PATH_TEMPFANCTL);
	printf("%s%u\n", qflag ? "" : "fan:\t", le32toh(fan));
	close(fd);
}

int
main(int argc, char **argv)
{
	int ch;

	while ((ch = getopt(argc, argv, "q")) != -1) {
		switch (ch) {
		case 'q':
			qflag = 1;
			break;

		case '?':
		default:
			usage();
			/* NOTREACHED */
			break;
		}
	}

	argc -= optind;
	argv += optind;

	if (argc < 1)
		usage();

	if (strcmp(argv[0], "fan") == 0)
		do_fan();
	else if (strcmp(argv[0], "hdmi") == 0) {
		if (argc != 2)
			usage();
		if (strcmp(argv[1], "reset") == 0)
			do_hdmi_reset();
		else if (strcmp(argv[1], "setup") == 0)
			do_hdmi_setup();
		else
			usage();
	} else if(strcmp(argv[0], "hdmires") == 0) {
		if (!((argc == 4) || (argc == 1)))
			usage();
		int ps_fd = open(PATH_PIXELSTREAM, O_RDWR);
		if(ps_fd == -1)
			perror("open");
		if(argc==1)
			display_pixelstream_regs(ps_fd);
		else {
			int      xres = strtol(argv[1],NULL,10);
			int      yres = strtol(argv[2],NULL,10);
			if(errno!=0)
				perror("strtol");
			else {
				float refresh = strtof(argv[3],NULL);
				if(errno!=0)
					perror("strtof");
				else
					hdmi_set_res(ps_fd, xres, yres, refresh);
			}
		}
		close(ps_fd);
	} else if (strcmp(argv[0], "temp") == 0)
		do_temp();
	else
		usage();
	return (0);
}

