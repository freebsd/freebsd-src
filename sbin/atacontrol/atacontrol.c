/*-
 * Copyright (c) 2000, 2001 Søren Schmidt
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/types.h>
#include <sys/ata.h>

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pathnames.h"

struct mode2str {
	int mode;
	const char *str;
};

struct mode2str mode_str[] = {
	{ ATA_PIO, "BIOSPIO" },
	{ ATA_PIO0, "PIO0" },
	{ ATA_PIO1, "PIO1" },
	{ ATA_PIO2, "PIO2" },
	{ ATA_PIO3, "PIO3" },
	{ ATA_PIO4, "PIO4" },
	{ ATA_WDMA2, "WDMA2" },
	{ ATA_UDMA2, "UDMA33" },
	{ ATA_UDMA4, "UDMA66" },
	{ ATA_UDMA5, "UDMA100" },
	{ ATA_DMA, "BIOSDMA" },
	{ -1, NULL }
};

const char	*mode2str(int);
int		 str2mode(char *);
int		 version(int);
void		 param_print(struct ata_params *);
void		 info_print(int, int);
void		 usage(void);

const char *
mode2str(int mode)
{
	int m;

	for (m = 0; mode_str[m].mode != -1; m++)
		if (mode_str[m].mode == mode)
			return (mode_str[m].str);

	return "???";
}

int
str2mode(char *str)
{
	int m;

	for (m = 0; mode_str[m].mode != -1; m++)
		if (!strcasecmp(mode_str[m].str, str))
			return (mode_str[m].mode);

	return -1;
}


void
usage(void)
{

	fprintf(stderr, "usage: atacontrol <command> channel [args]\n");
	exit(1);
}

int
version(int vers)
{
	int bit;

	if (vers == 0xffff)
		return 0;

	for (bit = 15; bit >= 0; bit--)
		if (vers & (1 << bit))
			return bit;

	return 0;
}

void
param_print(struct ata_params *parm)
{
	printf("<%.40s/%.8s> ATA/ATAPI rev %d\n",
		parm->model, parm->revision, version(parm->versmajor));
}

void
info_print(int fd, int channel)
{
	struct ata_param param;

	bzero(&param, sizeof(struct ata_param));
	param.channel = channel;

	if (ioctl(fd, ATAGPARM, &param) == -1)
		err(1, "ioctl(ATAGPARM)");

	printf("Master slot: ");
	if (param.type[0]) {
		printf("%4.4s: ", param.name[0]);
		param_print(&param.params[0]);
	}
	else
		printf(" no device present\n");

	printf("Slave  slot: ");
	if (param.type[1]) {
		printf("%4.4s: ", param.name[1]);
		param_print(&param.params[1]);
	}
	else
		printf(" no device present\n");
}

int
main(int argc, char **argv)
{
	int fd, unit;

	fd = open(_PATH_ATA, O_RDWR);
	if (fd == -1)
		err(1, _PATH_ATA);

	if (argc < 3)
		usage();

	if (!strcmp(argv[1], "detach")) {
		if (argc != 3)
			usage();
		unit = atoi(argv[2]);
		if (ioctl(fd, ATADETACH, &unit) == -1)
			err(1, "ioctl(ATADETACH)");
	}
	else if (!strcmp(argv[1], "attach")) {
		if (argc != 3)
			usage();
		unit = atoi(argv[2]);
		if (ioctl(fd, ATAATTACH, &unit) == -1)
			err(1, "ioctl(ATAATTACH)");
		else
			info_print(fd, unit);
	}
	else if (!strcmp(argv[1], "reinit")) {
		if (argc != 3)
			usage();
		unit = atoi(argv[2]);
		if (ioctl(fd, ATAREINIT, &unit) == -1)
			err(1, "ioctl(ATAREINIT)");
		else
			info_print(fd, unit);
	}
	else if (!strcmp(argv[1], "mode")) {
		struct ata_modes modes;

		bzero(&modes, sizeof(struct ata_modes));
		if (argc == 3) {
			modes.channel = atoi(argv[2]);
			if (ioctl(fd, ATAGMODE, &modes) == -1)
				err(1, "ioctl(ATAGMODE)");
			else
				printf("Master = %s \nSlave  = %s\n",
					mode2str(modes.mode[0]),
					mode2str(modes.mode[1]));
		}
		else if (argc == 5) {
			modes.channel = atoi(argv[2]);
			modes.mode[0] = str2mode(argv[3]);
			modes.mode[1] = str2mode(argv[4]);
			if (modes.mode[0] == -1 || modes.mode[1] == -1)
				usage();
			if (ioctl(fd, ATASMODE, &modes) == -1)
				err(1, "ioctl(ATASMODE)");
			else
				printf("Master = %s \nSlave  = %s\n",
					mode2str(modes.mode[0]),
					mode2str(modes.mode[1]));
		}
		else
			usage();
	}
	else if (!strcmp(argv[1], "info")) {
		if (argc != 3)
			usage();
		info_print(fd, atoi(argv[2]));
	}
	else
	    	usage();

	exit(0);
}
