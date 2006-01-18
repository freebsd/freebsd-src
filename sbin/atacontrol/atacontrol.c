/*-
 * Copyright (c) 2000 - 2005 Søren Schmidt <sos@FreeBSD.org>
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

#include <sys/types.h>
#include <sys/ata.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

const char *mode2str(int mode);
int str2mode(char *str);
void usage(void);
int version(int ver);
void param_print(struct ata_params *parm);
void cap_print(struct ata_params *parm);
int ata_cap_print(int fd);
int info_print(int fd, int channel, int prchan);

const char *
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
	case ATA_UDMA6: return "UDMA133";
	case ATA_SA150: return "SATA150";
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
	if (!strcasecmp(str, "UDMA6")) return ATA_UDMA6;
	if (!strcasecmp(str, "UDMA133")) return ATA_UDMA6;
	if (!strcasecmp(str, "BIOSDMA")) return ATA_DMA;
	return -1;
}

void
usage()
{
	fprintf(stderr,
		"usage:  atacontrol <command> args:\n"
		"        atacontrol list\n"
		"        atacontrol info channel\n"
		"        atacontrol attach channel\n"
		"        atacontrol detach channel\n"
		"        atacontrol reinit channel\n"
		"        atacontrol create type [interleave] disk0 ... diskN\n"
		"        atacontrol delete array\n"
		"        atacontrol addspare array disk\n"
		"        atacontrol rebuild array\n"
		"        atacontrol status array\n"
		"        atacontrol mode device [mode]\n"
		"        atacontrol cap device\n"
	);
	exit(EX_USAGE);
}

int
version(int ver)
{
	int bit;

	if (ver == 0xffff)
		return 0;
	for (bit = 15; bit >= 0; bit--)
		if (ver & (1<<bit))
			return bit;
	return 0;
}

void
param_print(struct ata_params *parm)
{
	printf("<%.40s/%.8s> ", parm->model, parm->revision);
	if (parm->satacapabilities && parm->satacapabilities != 0xffff) {
		printf("satacap=0x%04x\n", parm->satacapabilities);
		if (parm->satacapabilities & ATA_SATA_GEN2)
			printf("Serial ATA II\n");
		else if (parm->satacapabilities & ATA_SATA_GEN1)
			printf("Serial ATA v1.0\n");
		else
			printf("Unknown serial ATA version\n");
	}
	else
		printf("ATA/ATAPI revision %d\n", version(parm->version_major));
}

void
cap_print(struct ata_params *parm)
{
	u_int32_t lbasize = (u_int32_t)parm->lba_size_1 |
				((u_int32_t)parm->lba_size_2 << 16);

	u_int64_t lbasize48 = ((u_int64_t)parm->lba_size48_1) |
				((u_int64_t)parm->lba_size48_2 << 16) |
				((u_int64_t)parm->lba_size48_3 << 32) |
				((u_int64_t)parm->lba_size48_4 << 48);

	printf("\n");
	printf("Protocol              ");
	if (parm->satacapabilities && parm->satacapabilities != 0xffff) {
		if (parm->satacapabilities & ATA_SATA_GEN2)
			printf("Serial ATA II\n");
		else if (parm->satacapabilities & ATA_SATA_GEN1)
			printf("Serial ATA v1.0\n");
		else
			printf("Unknown serial ATA version\n");
	}
	else
		printf("ATA/ATAPI revision %d\n", version(parm->version_major));
	printf("device model          %.40s\n", parm->model);
	printf("serial number         %.20s\n", parm->serial);
	printf("firmware revision     %.8s\n", parm->revision);

	printf("cylinders             %d\n", parm->cylinders);
	printf("heads                 %d\n", parm->heads);
	printf("sectors/track         %d\n", parm->sectors);

	printf("lba%ssupported         ",
		parm->capabilities1 & ATA_SUPPORT_LBA ? " " : " not ");
	if (lbasize)
		printf("%d sectors\n", lbasize);
	else
		printf("\n");

	printf("lba48%ssupported       ",
		parm->support.command2 & ATA_SUPPORT_ADDRESS48 ? " " : " not ");
	if (lbasize48)
		printf("%ju sectors\n", (uintmax_t)lbasize48);
	else
		printf("\n");

	printf("dma%ssupported\n",
		parm->capabilities1 & ATA_SUPPORT_DMA ? " " : " not ");

	printf("overlap%ssupported\n",
		parm->capabilities1 & ATA_SUPPORT_OVERLAP ? " " : " not ");

	printf("\nFeature                      "
		"Support  Enable    Value           Vendor\n");

	printf("write cache                    %s	%s\n",
		parm->support.command1 & ATA_SUPPORT_WRITECACHE ? "yes" : "no",
		parm->enabled.command1 & ATA_SUPPORT_WRITECACHE ? "yes" : "no");

	printf("read ahead                     %s	%s\n",
		parm->support.command1 & ATA_SUPPORT_LOOKAHEAD ? "yes" : "no",
		parm->enabled.command1 & ATA_SUPPORT_LOOKAHEAD ? "yes" : "no");

	if (parm->satacapabilities && parm->satacapabilities != 0xffff) {
		printf("Native Command Queuing (NCQ)   %s	%s"
			"	%d/0x%02X\n",
			parm->satacapabilities & ATA_SUPPORT_NCQ ?
				"yes" : "no", " -",
			(parm->satacapabilities & ATA_SUPPORT_NCQ) ?
				ATA_QUEUE_LEN(parm->queue) : 0,
			(parm->satacapabilities & ATA_SUPPORT_NCQ) ?
				ATA_QUEUE_LEN(parm->queue) : 0);
	}
	printf("Tagged Command Queuing (TCQ)   %s	%s	%d/0x%02X\n",
		parm->support.command2 & ATA_SUPPORT_QUEUED ? "yes" : "no",
		parm->enabled.command2 & ATA_SUPPORT_QUEUED ? "yes" : "no",
		ATA_QUEUE_LEN(parm->queue), ATA_QUEUE_LEN(parm->queue));

	printf("SMART                          %s	%s\n",
		parm->support.command1 & ATA_SUPPORT_SMART ? "yes" : "no",
		parm->enabled.command1 & ATA_SUPPORT_SMART ? "yes" : "no");

	printf("microcode download             %s	%s\n",
		parm->support.command2 & ATA_SUPPORT_MICROCODE ? "yes" : "no",
		parm->enabled.command2 & ATA_SUPPORT_MICROCODE ? "yes" : "no");

	printf("security                       %s	%s\n",
		parm->support.command1 & ATA_SUPPORT_SECURITY ? "yes" : "no",
		parm->enabled.command1 & ATA_SUPPORT_SECURITY ? "yes" : "no");

	printf("power management               %s	%s\n",
		parm->support.command1 & ATA_SUPPORT_POWERMGT ? "yes" : "no",
		parm->enabled.command1 & ATA_SUPPORT_POWERMGT ? "yes" : "no");

	printf("advanced power management      %s	%s	%d/0x%02X\n",
		parm->support.command2 & ATA_SUPPORT_APM ? "yes" : "no",
		parm->enabled.command2 & ATA_SUPPORT_APM ? "yes" : "no",
		parm->apm_value, parm->apm_value);

	printf("automatic acoustic management  %s	%s	"
		"%d/0x%02X	%d/0x%02X\n",
		parm->support.command2 & ATA_SUPPORT_AUTOACOUSTIC ? "yes" :"no",
		parm->enabled.command2 & ATA_SUPPORT_AUTOACOUSTIC ? "yes" :"no",
		ATA_ACOUSTIC_CURRENT(parm->acoustic),
		ATA_ACOUSTIC_CURRENT(parm->acoustic),
		ATA_ACOUSTIC_VENDOR(parm->acoustic),
		ATA_ACOUSTIC_VENDOR(parm->acoustic));
}

int
ata_cap_print(int fd)
{
	struct ata_params params;

	if (ioctl(fd, IOCATAGPARM, &params) < 0)
		return errno;
	cap_print(&params);
	return 0;
}

int
info_print(int fd, int channel, int prchan)
{
	struct ata_ioc_devices devices;

	devices.channel = channel;

	if (ioctl(fd, IOCATADEVICES, &devices) < 0)
		return errno;

	if (prchan)
		printf("ATA channel %d:\n", channel);
	printf("%sMaster: ", prchan ? "    " : "");
	if (*devices.name[0]) {
		printf("%4.4s ", devices.name[0]);
		param_print(&devices.params[0]);
	}
	else
		printf("     no device present\n");
	printf("%sSlave:  ", prchan ? "    " : "");
	if (*devices.name[1]) {
		printf("%4.4s ", devices.name[1]);
		param_print(&devices.params[1]);
	}
	else
		printf("     no device present\n");
	return 0;
}

int
main(int argc, char **argv)
{
	int fd;

	if (argc < 2)
		usage();

	if (!strcmp(argv[1], "mode") && (argc == 3 || argc == 4)) {
		int disk, mode;
		char device[64];

		if (!(sscanf(argv[2], "ad%d", &disk) == 1 ||
		      sscanf(argv[2], "acd%d", &disk) == 1 ||
		      sscanf(argv[2], "afd%d", &disk) == 1 ||
		      sscanf(argv[2], "ast%d", &disk) == 1)) {
			fprintf(stderr, "atacontrol: Invalid device %s\n",
				argv[2]);
			exit(EX_USAGE);
		}
		sprintf(device, "/dev/%s", argv[2]);
		if ((fd = open(device, O_RDONLY)) < 0)
			err(1, "device not found");
		if (argc == 4) {
			mode = str2mode(argv[3]);
			if (ioctl(fd, IOCATASMODE, &mode) < 0)
				warn("ioctl(IOCATASMODE)");
		}
		if (argc == 3 || argc == 4) {
			if (ioctl(fd, IOCATAGMODE, &mode) < 0)
				err(1, "ioctl(IOCATAGMODE)");
			printf("current mode = %s\n", mode2str(mode));
		}
		exit(EX_OK);
	}
	if (!strcmp(argv[1], "cap") && argc == 3) {
		int disk;
		char device[64];

		if (!(sscanf(argv[2], "ad%d", &disk) == 1 ||
		      sscanf(argv[2], "acd%d", &disk) == 1 ||
		      sscanf(argv[2], "afd%d", &disk) == 1 ||
		      sscanf(argv[2], "ast%d", &disk) == 1)) {
			fprintf(stderr, "atacontrol: Invalid device %s\n",
				argv[2]);
			exit(EX_USAGE);
		}
		sprintf(device, "/dev/%s", argv[2]);
		if ((fd = open(device, O_RDONLY)) < 0)
			err(1, "device not found");
		ata_cap_print(fd);
		exit(EX_OK);
	}

	if ((fd = open("/dev/ata", O_RDWR)) < 0)
		err(1, "control device not found");

	if (!strcmp(argv[1], "list") && argc == 2) {
		int maxchannel, channel;

		if (ioctl(fd, IOCATAGMAXCHANNEL, &maxchannel) < 0)
			err(1, "ioctl(IOCATAGMAXCHANNEL)");
		for (channel = 0; channel < maxchannel; channel++)
			info_print(fd, channel, 1);
		exit(EX_OK);
	}
	if (!strcmp(argv[1], "info") && argc == 3) {
		int channel;

		if (!(sscanf(argv[2], "ata%d", &channel) == 1)) {
			fprintf(stderr,
				"atacontrol: Invalid channel %s\n", argv[2]);
                        exit(EX_USAGE);
		}
		info_print(fd, channel, 0);
		exit(EX_OK);
	}
	if (!strcmp(argv[1], "detach") && argc == 3) {
		int channel;

		if (!(sscanf(argv[2], "ata%d", &channel) == 1)) {
			fprintf(stderr,
				"atacontrol: Invalid channel %s\n", argv[2]);
                        exit(EX_USAGE);
		}
		if (ioctl(fd, IOCATADETACH, &channel) < 0)
			err(1, "ioctl(IOCATADETACH)");
		exit(EX_OK);
	}
	if (!strcmp(argv[1], "attach") && argc == 3) {
		int channel;

		if (!(sscanf(argv[2], "ata%d", &channel) == 1)) {
			fprintf(stderr,
				"atacontrol: Invalid channel %s\n", argv[2]);
                        exit(EX_USAGE);
		}
		if (ioctl(fd, IOCATAATTACH, &channel) < 0)
			err(1, "ioctl(IOCATAATTACH)");
		info_print(fd, channel, 0);
		exit(EX_OK);
	}
	if (!strcmp(argv[1], "reinit") && argc == 3) {
		int channel;

		if (!(sscanf(argv[2], "ata%d", &channel) == 1)) {
			fprintf(stderr,
				"atacontrol: Invalid channel %s\n", argv[2]);
                        exit(EX_USAGE);
		}
		if (ioctl(fd, IOCATAREINIT, &channel) < 0)
			warn("ioctl(IOCATAREINIT)");
		info_print(fd, channel, 0);
		exit(EX_OK);
	}
	if (!strcmp(argv[1], "create")) {
		int disk, dev, offset;
		struct ata_ioc_raid_config config;

		bzero(&config, sizeof(config));
		if (argc > 2) {
			if (!strcasecmp(argv[2], "RAID0") ||
			    !strcasecmp(argv[2], "stripe"))
				config.type = AR_RAID0;
			if (!strcasecmp(argv[2], "RAID1") ||
			    !strcasecmp(argv[2],"mirror"))
				config.type = AR_RAID1;
			if (!strcasecmp(argv[2], "RAID0+1") ||
			    !strcasecmp(argv[2],"RAID10"))
				config.type = AR_RAID01;
			if (!strcasecmp(argv[2], "RAID5"))
				config.type = AR_RAID5;
			if (!strcasecmp(argv[2], "SPAN"))
				config.type = AR_SPAN;
			if (!strcasecmp(argv[2], "JBOD"))
				config.type = AR_JBOD;
		}
		if (!config.type) {
			fprintf(stderr, "atacontrol: Invalid RAID type %s\n",
				argv[2]);
			fprintf(stderr, "atacontrol: Valid RAID types: \n");
			fprintf(stderr, "            stripe | mirror | "
					"RAID0 | RAID1 | RAID0+1 | RAID5 | "
					"SPAN | JBOD\n");
			exit(EX_USAGE);
		}

		if (config.type == AR_RAID0 ||
		    config.type == AR_RAID01 ||
		    config.type == AR_RAID5) {
			if (argc < 4 ||
			    !sscanf(argv[3], "%d", &config.interleave) == 1) {
				fprintf(stderr,
					"atacontrol: Invalid interleave %s\n",
					argv[3]);
				exit(EX_USAGE);
			}
			offset = 4;
		}
		else
			offset = 3;

		for (disk = 0; disk < 16 && (offset + disk) < argc; disk++) {
			if (!(sscanf(argv[offset + disk], "ad%d", &dev) == 1)) {
				fprintf(stderr,
					"atacontrol: Invalid disk %s\n",
					argv[offset + disk]);
				exit(EX_USAGE);
			}
			config.disks[disk] = dev;
		}

		if ((config.type == AR_RAID1 || config.type == AR_RAID01) &&
		    disk < 2) {
			fprintf(stderr, "atacontrol: At least 2 disks must be "
				"specified\n");
			exit(EX_USAGE);
		}

		config.total_disks = disk;
		if (ioctl(fd, IOCATARAIDCREATE, &config) < 0)
			err(1, "ioctl(IOCATARAIDCREATE)");
		else
			printf("ar%d created\n", config.lun);
		exit(EX_OK);
	}
	if (!strcmp(argv[1], "delete") && argc == 3) {
		int array;

		if (!(sscanf(argv[2], "ar%d", &array) == 1)) {
			fprintf(stderr,
				"atacontrol: Invalid array %s\n", argv[2]);
                        exit(EX_USAGE);
		}
		if (ioctl(fd, IOCATARAIDDELETE, &array) < 0)
			warn("ioctl(IOCATARAIDDELETE)");
		exit(EX_OK);
	}
	if (!strcmp(argv[1], "addspare") && argc == 4) {
		struct ata_ioc_raid_config config;

		if (!(sscanf(argv[2], "ar%d", &config.lun) == 1)) {
			fprintf(stderr,
				"atacontrol: Invalid array %s\n", argv[2]);
			usage();
		}
		if (!(sscanf(argv[3], "ad%d", &config.disks[0]) == 1)) {
			fprintf(stderr,
				"atacontrol: Invalid disk %s\n", argv[3]);
			usage();
		}
		if (ioctl(fd, IOCATARAIDADDSPARE, &config) < 0)
			warn("ioctl(IOCATARAIDADDSPARE)");
		exit(EX_OK);
	}
	if (!strcmp(argv[1], "rebuild") && argc == 3) {
		int array;

		if (!(sscanf(argv[2], "ar%d", &array) == 1)) {
			fprintf(stderr,
				"atacontrol: Invalid array %s\n", argv[2]);
			usage();
		}
		if (ioctl(fd, IOCATARAIDREBUILD, &array) < 0)
			warn("ioctl(IOCATARAIDREBUILD)");
		else {
			char buffer[128];
			sprintf(buffer, "/usr/bin/nice -n 20 /bin/dd "
				"if=/dev/ar%d of=/dev/null bs=1m &",
				array);
			if (system(buffer))
				warn("background dd");
		}
		exit(EX_OK);
	}
	if (!strcmp(argv[1], "status") && argc == 3) {
		struct ata_ioc_raid_config config;
		int i;

		if (!(sscanf(argv[2], "ar%d", &config.lun) == 1)) {
			fprintf(stderr,
				"atacontrol: Invalid array %s\n", argv[2]);
			usage();
		}
		if (ioctl(fd, IOCATARAIDSTATUS, &config) < 0)
			err(1, "ioctl(IOCATARAIDSTATUS)");

		printf("ar%d: ATA ", config.lun);
		switch (config.type) {
		case AR_RAID0:
			printf("RAID0 stripesize=%d", config.interleave);
			break;
		case AR_RAID1:
			printf("RAID1");
			break;
		case AR_RAID01:
			printf("RAID0+1 stripesize=%d", config.interleave);
			break;
		case AR_RAID5:
			printf("RAID5 stripesize=%d", config.interleave);
			break;
		case AR_JBOD:
			printf("JBOD");
		case AR_SPAN:
			printf("SPAN");
			break;
		}
		printf(" subdisks: ");
		for (i = 0; i < config.total_disks; i++) {
			if (config.disks[i] >= 0)
				printf("ad%d ", config.disks[i]);
			else
				printf("DOWN ");
		}
		printf("status: ");
		switch (config.status) {
		case AR_READY:
			printf("READY\n");
			break;
		case AR_READY | AR_DEGRADED:
			printf("DEGRADED\n");
			break;
		case AR_READY | AR_DEGRADED | AR_REBUILDING:
			printf("REBUILDING %d%% completed\n",
				config.progress);
			break;
		default:
			printf("BROKEN\n");
		}
		exit(EX_OK);
	}
	usage();
	exit(EX_OK);
}
