/*
 * APM BIOS utility for FreeBSD
 *
 * Copyright (C) 1994-1996 by Tatsumi Hosokawa <hosokawa@jp.FreeBSD.org>
 *
 * This software may be used, modified, copied, distributed, and sold,
 * in both source and binary form provided that the above copyright and
 * these terms are retained. Under no circumstances is the author
 * responsible for the proper functioning of this software, nor does
 * the author assume any responsibility for damages incurred with its
 * use.
 *
 * Sep., 1994	Implemented on FreeBSD 1.1.5.1R (Toshiba AVS001WD)
 */

#include <sys/ioctl.h>

#include <machine/apm_bios.h>

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define APMDEV	"/dev/apm"

#define APM_UNKNOWN	255

static void
usage(void)
{
	fprintf(stderr,
		"usage: apm [-abltzZ]\n");
	exit(1);
}

static void 
apm_suspend(int fd)
{
	if (ioctl(fd, APMIO_SUSPEND, NULL) == -1)
		err(1, "ioctl(APMIO_SUSPEND)");
}

static void 
apm_standby(int fd)
{
	if (ioctl(fd, APMIO_STANDBY, NULL) == -1)
		err(1, "ioctl(APMIO_STANDBY)");
}

static void 
apm_getinfo(int fd, apm_info_t aip)
{
	if (ioctl(fd, APMIO_GETINFO, aip) == -1)
		err(1, "ioctl(APMIO_GETINFO)");
}

static void
print_batt_time(int batt_time)
{
	printf("Remaining battery time: ");
	if (batt_time == -1)
		printf("unknown\n");
	else {
		int h, m, s;

		h = batt_time;
		s = h % 60;
		h /= 60;
		m = h % 60;
		h /= 60;
		printf("%2d:%02d:%02d\n", h, m, s);
	}
}

static void
print_batt_life(u_int batt_life)
{
	printf("Remaining battery life: ");
	if (batt_life == APM_UNKNOWN)
		printf("unknown\n");
	else if (batt_life <= 100)
		printf("%d%%\n", batt_life);
	else
		printf("invalid value (0x%x)\n", batt_life);
}

static void
print_batt_stat(u_int batt_stat)
{
	const char *batt_msg[] = { "high", "low", "critical", "charging" };

	printf("Battery Status: ");
	if (batt_stat == APM_UNKNOWN)
		printf("unknown\n");
	else if (batt_stat > 3)
		printf("invalid value (0x%x)\n", batt_stat);
	else
		printf("%s\n", batt_msg[batt_stat]);
}

static void 
print_all_info(int fd, apm_info_t aip)
{
	const char *line_msg[] = { "off-line", "on-line" , "backup power"};

	printf("APM version: %d.%d\n", aip->ai_major, aip->ai_minor);
	printf("APM Management: %s\n", aip->ai_status ? "Enabled" : "Disabled");
	printf("AC Line status: ");
	if (aip->ai_acline == APM_UNKNOWN)
		printf("unknown\n");
	else if (aip->ai_acline > 2)
		printf("invalid value (0x%x)\n", aip->ai_acline);
	else
		printf("%s\n", line_msg[aip->ai_acline]);

	print_batt_stat(aip->ai_batt_stat);
	print_batt_life(aip->ai_batt_life);
	print_batt_time(aip->ai_batt_time);

	if (aip->ai_infoversion >= 1) {
		printf("Number of batteries: ");
		if (aip->ai_batteries == ~0U)
			printf("unknown\n");
		else {
			u_int i;
			struct apm_pwstatus aps;

			printf("%d\n", aip->ai_batteries);
			for (i = 0; i < aip->ai_batteries; ++i) {
				bzero(&aps, sizeof(aps));
				aps.ap_device = PMDV_BATT0 + i;
				if (ioctl(fd, APMIO_GETPWSTATUS, &aps) == -1)
					continue;
				printf("Battery %d:\n", i);
				if (aps.ap_batt_flag & APM_BATT_NOT_PRESENT) {
					printf("not present\n");
					continue;
				}
				printf("\t");
				print_batt_stat(aps.ap_batt_stat);
				printf("\t");
				print_batt_life(aps.ap_batt_life);
				printf("\t");
				print_batt_time(aps.ap_batt_time);
			}
		}
	}

	if (aip->ai_infoversion >= 1) {
		if (aip->ai_capabilities == 0xff00)
		    return;
		printf("APM Capabilities:\n");
		if (aip->ai_capabilities & 0x01)
			printf("\tglobal standby state\n");
		if (aip->ai_capabilities & 0x02)
			printf("\tglobal suspend state\n");
		if (aip->ai_capabilities & 0x04)
			printf("\tresume timer from standby\n");
		if (aip->ai_capabilities & 0x08)
			printf("\tresume timer from suspend\n");
		if (aip->ai_capabilities & 0x10)
			printf("\tRI resume from standby\n");
		if (aip->ai_capabilities & 0x20)
			printf("\tRI resume from suspend\n");
		if (aip->ai_capabilities & 0x40)
			printf("\tPCMCIA RI resume from standby\n");
		if (aip->ai_capabilities & 0x80)
			printf("\tPCMCIA RI resume from suspend\n");
	}

}

int 
main(int argc, char *argv[])
{
	int	c, fd;
	int     dosleep = 0, all_info = 1, batt_status = 0;
	int     batt_life = 0, ac_status = 0, standby = 0;
	int	batt_time = 0;

	while ((c = getopt(argc, argv, "abltzZ")) != -1) {
		switch (c) {
		case 'a':
			ac_status = 1;
			all_info = 0;
			break;
		case 'b':
			batt_status = 1;
			all_info = 0;
			break;
		case 'l':
			batt_life = 1;
			all_info = 0;
			break;
		case 't':
			batt_time = 1;
			all_info = 0;
			break;
		case 'z':
			dosleep = 1;
			all_info = 0;
			break;
		case 'Z':
			standby = 1;
			all_info = 0;
			break;
		case '?':
		default:
			usage();
		}
		argc -= optind;
		argv += optind;
	}
	if (dosleep || standby)
		fd = open(APMDEV, O_RDWR);
	else
		fd = open(APMDEV, O_RDONLY);
	if (fd == -1)
		err(1, "can't open %s", APMDEV);
	if (dosleep)
		apm_suspend(fd);
	else if (standby)
		apm_standby(fd);
	else {
		struct apm_info info;

		apm_getinfo(fd, &info);
		if (all_info)
			print_all_info(fd, &info);
		if (ac_status)
			printf("%d\n", info.ai_acline);
		if (batt_status)
			printf("%d\n", info.ai_batt_stat);
		if (batt_life)
			printf("%d\n", info.ai_batt_life);
		if (batt_time)
			printf("%d\n", info.ai_batt_time);
	}
	close(fd);
	exit(0);
}
