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
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <err.h>
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
	fprintf(stderr, "usage: atacontrol <command> channel [args]\n");
	exit(1);
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
		parm->model, parm->revision, version(parm->version_major)); 
}

void
aparam_print(struct ata_params *parm)
{
	printf("                                 disk model name %.40s\n", parm->model);
	printf("                               firmware revision %.8s\n", parm->revision);
	printf("                            ata / atapi revision %d\n", version(parm->version_major));

	printf("                             number of cylinders %d\n", parm->cylinders);
	printf("                                 number of heads %d\n", parm->heads);
	printf("                               sectors per track %d\n", parm->sectors);	
	
	printf("                                     lba support %s\n", parm->support_lba ? "yes" : "no");
	printf("                                     lba sectors %d\n", parm->lba_size);	
	printf("                                     dma support %s\n", parm->support_dma ? "yes" : "no");
	printf("                                queueing support %s\n", parm->support_queueing ? "yes" : "no");
	if(parm->support_queueing)
	printf("                                          length %d\n", parm->queuelen);	

	printf("                                   SMART support %s\n", parm->support.smart ? "yes" : "no");
	if(parm->support.smart)
	printf("                                         enabled %s\n", parm->enabled.smart ? "yes" : "no");

	printf("                                security support %s\n", parm->support.smart ? "yes" : "no");
	if(parm->support.smart)
	printf("                                         enabled %s\n", parm->enabled.smart ? "yes" : "no");	

	printf("                        power management support %s\n", parm->support.power_mngt ? "yes" : "no");
	if(parm->support.power_mngt)
	printf("                                         enabled %s\n", parm->enabled.power_mngt ? "yes" : "no");	

	printf("                             write cache support %s\n", parm->support.write_cache ? "yes" : "no");
	if(parm->support.write_cache)
	printf("                                         enabled %s\n", parm->enabled.write_cache ? "yes" : "no");	

	printf("                              look ahead support %s\n", parm->support.look_ahead ? "yes" : "no");
	if(parm->support.look_ahead)
	printf("                                         enabled %s\n", parm->enabled.look_ahead ? "yes" : "no");	

	printf("                      microcode download support %s\n", parm->support.microcode ? "yes" : "no");
	if(parm->support.microcode)
	printf("                                         enabled %s\n", parm->enabled.microcode ? "yes" : "no");	

	printf("                        rd/wr dma queued support %s\n", parm->support.queued ? "yes" : "no");
	if(parm->support.queued)
	printf("                                         enabled %s\n", parm->enabled.queued ? "yes" : "no");	

	printf("               advanced power management support %s\n", parm->support.apm ? "yes" : "no");
	if(parm->support.apm)
	{
	printf("                                         enabled %s\n", parm->enabled.apm ? "yes" : "no");	
	printf("                                           value %d / 0x%02x\n", parm->apm_value, parm->apm_value);
	}	
	printf("           automatic acoustic management support %s\n", parm->support.auto_acoustic ? "yes" : "no");
	if(parm->support.auto_acoustic)
	{
	printf("                                         enabled %s\n", parm->enabled.auto_acoustic ? "yes" : "no");
	printf("     automatic acoustic management current value %d / 0x%02x\n", parm->current_acoustic, parm->current_acoustic);
	printf("                               recommended value %d / 0x%02x\n", parm->vendor_acoustic, parm->vendor_acoustic);	
	}
}

int
ata_params_print(int fd, int channel, int master)
{
	struct ata_cmd iocmd;

	bzero(&iocmd, sizeof(struct ata_cmd));

	iocmd.channel = channel;
	iocmd.device = -1;
	iocmd.cmd = ATAGPARM;

	if (ioctl(fd, IOCATA, &iocmd) < 0)
		return errno;

	if(master)
		master = 1;
	master = !master;
	
	printf("ATA channel %d, %s", channel, master==0 ? "Master" : "Slave");

	if (iocmd.u.param.type[master]) {
		printf(", device %s:\n", iocmd.u.param.name[master]);
		aparam_print(&iocmd.u.param.params[master]);
	}
	else
	{
		printf(": no device present\n");
	}
	return 0;
}

int
info_print(int fd, int channel, int prchan)
{
	struct ata_cmd iocmd;

	bzero(&iocmd, sizeof(struct ata_cmd));
	iocmd.channel = channel;
	iocmd.device = -1;
	iocmd.cmd = ATAGPARM;
	if (ioctl(fd, IOCATA, &iocmd) < 0)
		return errno;
	if (prchan)
		printf("ATA channel %d:\n", channel);
	printf("%sMaster: ", prchan ? "    " : "");
	if (iocmd.u.param.type[0]) {
		printf("%4.4s ", iocmd.u.param.name[0]);
		param_print(&iocmd.u.param.params[0]);
	}
	else
		printf("     no device present\n");
	printf("%sSlave:  ", prchan ? "    " : "");
	if (iocmd.u.param.type[1]) {
		printf("%4.4s ", iocmd.u.param.name[1]);
		param_print(&iocmd.u.param.params[1]);
	}
	else
		printf("     no device present\n");
	return 0;
}

int
main(int argc, char **argv)
{
	struct ata_cmd iocmd;
	int master;
	int fd;

	if ((fd = open("/dev/ata", O_RDWR)) < 0)
		err(1, "control device not found");

	if (argc < 2)
		usage();

	bzero(&iocmd, sizeof(struct ata_cmd));

	if (argc > 2)
		iocmd.channel = atoi(argv[2]);

	if (argc > 3)
		master = atoi(argv[3]);

	if (!strcmp(argv[1], "list") && argc == 2) {
		int unit = 0;

		while (info_print(fd, unit++, 1) != ENXIO);
	}
	else if (!strcmp(argv[1], "info") && argc == 3) {
		info_print(fd, iocmd.channel, 0);
	}
	else if (!strcmp(argv[1], "parm") && argc == 4) {
		ata_params_print(fd, iocmd.channel, master);
	}
	else if (!strcmp(argv[1], "detach") && argc == 3) {
		iocmd.cmd = ATADETACH;
		if (ioctl(fd, IOCATA, &iocmd) < 0)
			err(1, "ioctl(ATADETACH)");
	}
	else if (!strcmp(argv[1], "attach") && argc == 3) {
		iocmd.cmd = ATAATTACH;
		if (ioctl(fd, IOCATA, &iocmd) < 0)
			err(1, "ioctl(ATAATTACH)");
		info_print(fd, iocmd.channel, 0);
	}
	else if (!strcmp(argv[1], "reinit") && argc == 3) {
		iocmd.cmd = ATAREINIT;
		if (ioctl(fd, IOCATA, &iocmd) < 0)
			warn("ioctl(ATAREINIT)");
		info_print(fd, iocmd.channel, 0);
	}
	else if (!strcmp(argv[1], "mode") && (argc == 3 || argc == 5)) {
		if (argc == 5) {
			iocmd.cmd = ATASMODE;
			iocmd.device = -1;
			iocmd.u.mode.mode[0] = str2mode(argv[3]);
			iocmd.u.mode.mode[1] = str2mode(argv[4]);
			if (ioctl(fd, IOCATA, &iocmd) < 0)
				warn("ioctl(ATASMODE)");
		}
		if (argc == 3 || argc == 5) {
			iocmd.cmd = ATAGMODE;
			iocmd.device = -1;
			if (ioctl(fd, IOCATA, &iocmd) < 0)
				err(1, "ioctl(ATAGMODE)");
			printf("Master = %s \nSlave  = %s\n",
				mode2str(iocmd.u.mode.mode[0]), 
				mode2str(iocmd.u.mode.mode[1]));
		}
	}
	else
	    	usage();
	exit(0);
}
