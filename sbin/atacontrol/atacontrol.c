/*-
 * Copyright (c) 2000,2001 Søren Schmidt
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
 *
 * $FreeBSD$
 */

#include <stdio.h>
#include <fcntl.h>
#include <sys/ata.h>

char *
mode2str(int mode)
{
    switch (mode) {
    case ATA_PIO: return "BIOSPIO";
    case ATA_PIO0: return "PIO0";
    case ATA_PIO1: return "PIO1";
    case ATA_PIO2: return "PIO2";
    case ATA_PIO3: return "PIO3";
    case ATA_PIO4: return "PIO4";
    case ATA_WDMA2: return "WDMA2";
    case ATA_UDMA2: return "UDMA33";
    case ATA_UDMA4: return "UDMA66";
    case ATA_UDMA5: return "UDMA100";
    case ATA_DMA: return "BIOSDMA";
    default: return "???";
    }
}

int
str2mode(char *str)
{
    if (!strcasecmp(str, "BIOSPIO")) return ATA_PIO;
    if (!strcasecmp(str, "PIO0")) return ATA_PIO0;
    if (!strcasecmp(str, "PIO1")) return ATA_PIO1;
    if (!strcasecmp(str, "PIO2")) return ATA_PIO2;
    if (!strcasecmp(str, "PIO3")) return ATA_PIO3;
    if (!strcasecmp(str, "PIO4")) return ATA_PIO4;
    if (!strcasecmp(str, "WDMA2")) return ATA_WDMA2;
    if (!strcasecmp(str, "UDMA2")) return ATA_UDMA2;
    if (!strcasecmp(str, "UDMA33")) return ATA_UDMA2;
    if (!strcasecmp(str, "UDMA4")) return ATA_UDMA4;
    if (!strcasecmp(str, "UDMA66")) return ATA_UDMA4;
    if (!strcasecmp(str, "UDMA5")) return ATA_UDMA5;
    if (!strcasecmp(str, "UDMA100")) return ATA_UDMA5;
    if (!strcasecmp(str, "BIOSDMA")) return ATA_DMA;
    return -1;
}


void
usage()
{
	printf("usage: atacontrol <command> channel [args]\n");
}

int
version(int version)
{
    int bit;
    
    if (version == 0xffff)
        return 0;
    for (bit = 15; bit >= 0; bit--)
        if (version & (1<<bit))
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
	ioctl(fd, ATAGPARM, &param);
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
	int unit;
	int fd = open("/dev/ata", O_RDWR);

	if (fd < 0)
		errx(1, "/dev/ata not found - exiting\n");

	if (!strcmp(argv[1], "detach")) {
		if (argc != 3)
			usage();
		unit = atoi(argv[2]);
		ioctl(fd, ATADETACH, &unit);
	}
	else if (!strcmp(argv[1], "attach")) {
		if (argc != 3)
			usage();
		unit = atoi(argv[2]);
		ioctl(fd, ATAATTACH, &unit);
		info_print(fd, unit);
	}
	else if (!strcmp(argv[1], "reinit")) {
		if (argc != 3)
			usage();
		unit = atoi(argv[2]);
		ioctl(fd, ATAREINIT, &unit);
		info_print(fd, unit);
	}
	else if (!strcmp(argv[1], "mode")) {
		struct ata_modes modes;

		bzero(&modes, sizeof(struct ata_modes));
		if (argc == 3) {
			modes.channel = atoi(argv[2]);
			ioctl(fd, ATAGMODE, &modes);
			printf("Master = %s \nSlave  = %s\n",
				mode2str(modes.mode[0]), 
				mode2str(modes.mode[1]));
		}
		else if (argc == 5) {
			modes.channel = atoi(argv[2]);
			modes.mode[0] = str2mode(argv[3]);
			modes.mode[1] = str2mode(argv[4]);
			ioctl(fd, ATASMODE, &modes);
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
}
