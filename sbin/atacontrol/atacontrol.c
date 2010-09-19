/*-
 * Copyright (c) 2000 - 2006 Søren Schmidt <sos@FreeBSD.org>
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
#include <sys/ata.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

static const char *
mode2str(int mode)
{
	switch (mode) {
	case ATA_PIO: return "BIOSPIO";
	case ATA_PIO0: return "PIO0";
	case ATA_PIO1: return "PIO1";
	case ATA_PIO2: return "PIO2";
	case ATA_PIO3: return "PIO3";
	case ATA_PIO4: return "PIO4";
	case ATA_WDMA0: return "WDMA0";
	case ATA_WDMA1: return "WDMA1";
	case ATA_WDMA2: return "WDMA2";
	case ATA_UDMA0: return "UDMA0";
	case ATA_UDMA1: return "UDMA1";
	case ATA_UDMA2: return "UDMA33";
	case ATA_UDMA3: return "UDMA44";
	case ATA_UDMA4: return "UDMA66";
	case ATA_UDMA5: return "UDMA100";
	case ATA_UDMA6: return "UDMA133";
	case ATA_SA150: return "SATA150";
	case ATA_SA300: return "SATA300";
	case ATA_USB: return "USB";
	case ATA_USB1: return "USB1";
	case ATA_USB2: return "USB2";
	case ATA_DMA: return "BIOSDMA";
	default: return "???";
	}
}

static int
str2mode(char *str)
{
	if (!strcasecmp(str, "BIOSPIO")) return ATA_PIO;
	if (!strcasecmp(str, "PIO0")) return ATA_PIO0;
	if (!strcasecmp(str, "PIO1")) return ATA_PIO1;
	if (!strcasecmp(str, "PIO2")) return ATA_PIO2;
	if (!strcasecmp(str, "PIO3")) return ATA_PIO3;
	if (!strcasecmp(str, "PIO4")) return ATA_PIO4;
	if (!strcasecmp(str, "WDMA0")) return ATA_WDMA0;
	if (!strcasecmp(str, "WDMA1")) return ATA_WDMA1;
	if (!strcasecmp(str, "WDMA2")) return ATA_WDMA2;
	if (!strcasecmp(str, "UDMA0")) return ATA_UDMA0;
	if (!strcasecmp(str, "UDMA1")) return ATA_UDMA1;
	if (!strcasecmp(str, "UDMA2")) return ATA_UDMA2;
	if (!strcasecmp(str, "UDMA33")) return ATA_UDMA2;
	if (!strcasecmp(str, "UDMA3")) return ATA_UDMA3;
	if (!strcasecmp(str, "UDMA44")) return ATA_UDMA3;
	if (!strcasecmp(str, "UDMA4")) return ATA_UDMA4;
	if (!strcasecmp(str, "UDMA66")) return ATA_UDMA4;
	if (!strcasecmp(str, "UDMA5")) return ATA_UDMA5;
	if (!strcasecmp(str, "UDMA100")) return ATA_UDMA5;
	if (!strcasecmp(str, "UDMA6")) return ATA_UDMA6;
	if (!strcasecmp(str, "UDMA133")) return ATA_UDMA6;
	if (!strcasecmp(str, "SATA150")) return ATA_SA150;
	if (!strcasecmp(str, "SATA300")) return ATA_SA300;
	if (!strcasecmp(str, "USB")) return ATA_USB;
	if (!strcasecmp(str, "USB1")) return ATA_USB1;
	if (!strcasecmp(str, "USB2")) return ATA_USB2;
	if (!strcasecmp(str, "BIOSDMA")) return ATA_DMA;
	return -1;
}

static void
usage(void)
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
		"        atacontrol spindown device [seconds]\n"
	);
	exit(EX_USAGE);
}

static int
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

static void
param_print(struct ata_params *parm)
{
	printf("<%.40s/%.8s> ", parm->model, parm->revision);
	if (parm->satacapabilities && parm->satacapabilities != 0xffff) {
		if (parm->satacapabilities & ATA_SATA_GEN2)
			printf("SATA revision 2.x\n");
		else if (parm->satacapabilities & ATA_SATA_GEN1)
			printf("SATA revision 1.x\n");
		else
			printf("Unknown SATA revision\n");
	}
	else
		printf("ATA/ATAPI revision %d\n", version(parm->version_major));
}

static void
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
			printf("SATA revision 2.x\n");
		else if (parm->satacapabilities & ATA_SATA_GEN1)
			printf("SATA revision 1.x\n");
		else
			printf("Unknown SATA revision\n");
	}
	else
		printf("ATA/ATAPI revision %d\n", version(parm->version_major));
	printf("device model          %.40s\n", parm->model);
	printf("serial number         %.20s\n", parm->serial);
	printf("firmware revision     %.8s\n", parm->revision);

	printf("cylinders             %d\n", parm->cylinders);
	printf("heads                 %d\n", parm->heads);
	printf("sectors/track         %d\n", parm->sectors);

	if (parm->config == ATA_PROTO_CFA ||
	    (parm->support.command2 & ATA_SUPPORT_CFA))
		printf("CFA supported\n");

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

static void
ata_cap_print(int fd)
{
	struct ata_params params;

	if (ioctl(fd, IOCATAGPARM, &params) < 0)
		err(1, "ioctl(IOCATAGPARM)");
	cap_print(&params);
}

static void
info_print(int fd, int channel, int prchan)
{
	struct ata_ioc_devices devices;

	devices.channel = channel;

	if (ioctl(fd, IOCATADEVICES, &devices) < 0) {
		if (!prchan)
			err(1, "ioctl(IOCATADEVICES)");
		return;
	}
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
}

static void
ata_spindown(int fd, const char *dev, const char *arg)
{
	int tmo;

	if (arg != NULL) {
		tmo = strtoul(arg, NULL, 0);
		if (ioctl(fd, IOCATASSPINDOWN, &tmo) < 0)
			err(1, "ioctl(IOCATASSPINDOWN)");
	} else {
		if (ioctl(fd, IOCATAGSPINDOWN, &tmo) < 0)
			err(1, "ioctl(IOCATAGSPINDOWN)");
		if (tmo == 0)
			printf("%s: idle spin down disabled\n", dev);
		else
			printf("%s: spin down after %d seconds idle\n",
			    dev, tmo);
	}
}

static int
open_dev(const char *arg, int mode)
{
	int disk, fd;
	char device[64];

	if (!(sscanf(arg, "ad%d", &disk) == 1 ||
	      sscanf(arg, "acd%d", &disk) == 1 ||
	      sscanf(arg, "afd%d", &disk) == 1 ||
	      sscanf(arg, "ast%d", &disk) == 1)) {
		fprintf(stderr, "atacontrol: Invalid device %s\n", arg);
		exit(EX_USAGE);
	}
	sprintf(device, "/dev/%s", arg);
	if ((fd = open(device, mode)) < 0)
		err(1, "device not found");
	return (fd);
}

static int
ar_arg(const char *arg)
{
	int array;

	if (!(sscanf(arg, "ar%d", &array) == 1)) {
		fprintf(stderr, "atacontrol: Invalid array %s\n", arg);
		exit(EX_USAGE);
	}
	return (array);
}

static int
ata_arg(const char *arg)
{
	int channel;

	if (!(sscanf(arg, "ata%d", &channel) == 1)) {
		fprintf(stderr, "atacontrol: Invalid channel %s\n", arg);
		exit(EX_USAGE);
	}
	return (channel);
}

int
main(int argc, char **argv)
{
	int fd, mode, channel, array;

	if (argc < 2)
		usage();

	if (!strcmp(argv[1], "mode") && (argc == 3 || argc == 4)) {
		fd = open_dev(argv[2], O_RDONLY);
		if (argc == 4) {
			mode = str2mode(argv[3]);
			if (mode == -1)
				errx(1, "unknown mode");
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
		fd = open_dev(argv[2], O_RDONLY);
		ata_cap_print(fd);
		exit(EX_OK);
	}

	if (!strcmp(argv[1], "spindown") && (argc == 3 || argc == 4)) {
		fd = open_dev(argv[2], O_RDONLY);
		ata_spindown(fd, argv[2], argv[3]);
		exit(EX_OK);
	}

	if ((fd = open("/dev/ata", O_RDWR)) < 0)
		err(1, "control device not found");

	if (!strcmp(argv[1], "list") && argc == 2) {
		int maxchannel;

		if (ioctl(fd, IOCATAGMAXCHANNEL, &maxchannel) < 0)
			err(1, "ioctl(IOCATAGMAXCHANNEL)");
		for (channel = 0; channel < maxchannel; channel++)
			info_print(fd, channel, 1);
		exit(EX_OK);
	}
	if (!strcmp(argv[1], "info") && argc == 3) {
		channel = ata_arg(argv[2]);
		info_print(fd, channel, 0);
		exit(EX_OK);
	}
	if (!strcmp(argv[1], "detach") && argc == 3) {
		channel = ata_arg(argv[2]);
		if (ioctl(fd, IOCATADETACH, &channel) < 0)
			err(1, "ioctl(IOCATADETACH)");
		exit(EX_OK);
	}
	if (!strcmp(argv[1], "attach") && argc == 3) {
		channel = ata_arg(argv[2]);
		if (ioctl(fd, IOCATAATTACH, &channel) < 0)
			err(1, "ioctl(IOCATAATTACH)");
		info_print(fd, channel, 0);
		exit(EX_OK);
	}
	if (!strcmp(argv[1], "reinit") && argc == 3) {
		channel = ata_arg(argv[2]);
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
		array = ar_arg(argv[2]);
		if (ioctl(fd, IOCATARAIDDELETE, &array) < 0)
			warn("ioctl(IOCATARAIDDELETE)");
		exit(EX_OK);
	}
	if (!strcmp(argv[1], "addspare") && argc == 4) {
		struct ata_ioc_raid_config config;

		config.lun = ar_arg(argv[2]);
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
		array = ar_arg(argv[2]);
		if (ioctl(fd, IOCATARAIDREBUILD, &array) < 0)
			warn("ioctl(IOCATARAIDREBUILD)");
		else {
			char device[64];
			char *buffer;
			ssize_t len;
			int arfd;

			if (daemon(0, 1) == -1)
				err(1, "daemon");
			nice(20);
			snprintf(device, sizeof(device), "/dev/ar%d",
			    array);
			if ((arfd = open(device, O_RDONLY)) == -1)
				err(1, "open %s", device);
			if ((buffer = malloc(1024 * 1024)) == NULL)
				err(1, "malloc");
			while ((len = read(arfd, buffer, 1024 * 1024)) > 0)
				;
			if (len == -1)
				err(1, "read");
			else
				fprintf(stderr,
				    "atacontrol: ar%d rebuild completed\n",
				    array);
			free(buffer);
			close(arfd);
		}
		exit(EX_OK);
	}
	if (!strcmp(argv[1], "status") && argc == 3) {
		struct ata_ioc_raid_status status;
		int i, lun, state;

		status.lun = ar_arg(argv[2]);
		if (ioctl(fd, IOCATARAIDSTATUS, &status) < 0)
			err(1, "ioctl(IOCATARAIDSTATUS)");

		printf("ar%d: ATA ", status.lun);
		switch (status.type) {
		case AR_RAID0:
			printf("RAID0 stripesize=%d", status.interleave);
			break;
		case AR_RAID1:
			printf("RAID1");
			break;
		case AR_RAID01:
			printf("RAID0+1 stripesize=%d", status.interleave);
			break;
		case AR_RAID5:
			printf("RAID5 stripesize=%d", status.interleave);
			break;
		case AR_JBOD:
			printf("JBOD");
			break;
		case AR_SPAN:
			printf("SPAN");
			break;
		}
		printf(" status: ");
		switch (status.status) {
		case AR_READY:
			printf("READY\n");
			break;
		case AR_READY | AR_DEGRADED:
			printf("DEGRADED\n");
			break;
		case AR_READY | AR_DEGRADED | AR_REBUILDING:
			printf("REBUILDING %d%% completed\n",
				status.progress);
			break;
		default:
			printf("BROKEN\n");
		}
		printf(" subdisks:\n");
		for (i = 0; i < status.total_disks; i++) {
			printf("  %2d ", i);
			lun = status.disks[i].lun;
			state = status.disks[i].state;
			if (lun < 0)
				printf("---- ");
			else
				printf("ad%-2d ", lun);
			if (state & AR_DISK_ONLINE)
				printf("ONLINE");
			else if (state & AR_DISK_SPARE)
				printf("SPARE");
			else if (state & AR_DISK_PRESENT)
				printf("OFFLINE");
			else
				printf("MISSING");
			printf("\n");
		}
		exit(EX_OK);
	}
	usage();
	exit(EX_OK);
}
