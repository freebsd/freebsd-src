/*
 * apm / zzz	APM BIOS utility for FreeBSD
 *
 * Copyright (C) 1994-1996 by HOSOKAWA Tatasumi <hosokawa@mt.cs.keio.ac.jp>
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

#ifndef lint
static const char rcsid[] =
	"$Id: apm.c,v 1.10 1997/09/02 06:36:39 charnier Exp $";
#endif /* not lint */

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <machine/apm_bios.h>

#define APMDEV	"/dev/apm"

void
usage()
{
	fprintf(stderr, "%s\n%s\n",
		"usage: apm [-ablsz] [-d 1|0]",
		"       zzz");
	exit(1);
}

void 
apm_suspend(int fd)
{
	if (ioctl(fd, APMIO_SUSPEND, NULL) == -1)
		err(1, NULL);
}

void 
apm_getinfo(int fd, apm_info_t aip)
{
	if (ioctl(fd, APMIO_GETINFO, aip) == -1)
		err(1, NULL);
}

void 
print_all_info(apm_info_t aip)
{
	printf("APM version: %d.%d\n", aip->ai_major, aip->ai_minor);
	printf("APM Managment: %s\n", (aip->ai_status ? "Enabled" : "Disabled"));
	printf("AC Line status: ");
	if (aip->ai_acline == 255)
		printf("unknown");
	else if (aip->ai_acline > 1)
		printf("invalid value (0x%x)", aip->ai_acline);
	else {
		char messages[][10] = {"off-line", "on-line"};
		printf("%s", messages[aip->ai_acline]);
	}
	printf("\n");
	printf("Battery status: ");
	if (aip->ai_batt_stat == 255)
		printf("unknown");
	else if (aip->ai_batt_stat > 3)
			printf("invalid value (0x%x)", aip->ai_batt_stat);
	else {
		char messages[][10] = {"high", "low", "critical", "charging"};
		printf("%s", messages[aip->ai_batt_stat]);
	}
	printf("\n");
	printf("Remaining battery life: ");
	if (aip->ai_batt_life == 255)
		printf("unknown");
	else if (aip->ai_batt_life <= 100)
		printf("%d%%", aip->ai_batt_life);
	else
		printf("invalid value (0x%x)", aip->ai_batt_life);
	printf("\n");
}


/*
 * currently, it can turn off the display, but the display never comes
 * back until the machine suspend/resumes :-).
 */
void 
apm_display(int fd, int newstate)
{
	if (ioctl(fd, APMIO_DISPLAY, &newstate) == -1)
		err(1, NULL);
}


int 
main(int argc, char *argv[])
{
	int	c, fd;
	int     sleep = 0, all_info = 1, apm_status = 0, batt_status = 0;
	int     display = 0, batt_life = 0, ac_status = 0;
	char	*cmdname;


	if ((cmdname = strrchr(argv[0], '/')) != NULL)
		cmdname++;
	else
		cmdname = argv[0];

	if (strcmp(cmdname, "zzz") == 0) {
		sleep = 1;
		all_info = 0;
		goto finish_option;
	}
	while ((c = getopt(argc, argv, "ablszd:")) != -1) {
		switch (c) {
		case 'a':
			ac_status = 1;
			all_info = 0;
			break;
		case 'b':
			batt_status = 1;
			all_info = 0;
			break;
		case 'd':
			display = *optarg - '0';
			if (display < 0 || display > 1) {
				warnx("argument of option '-%c' is invalid", c);
				usage();
			}
			display++;
			all_info = 0;
			break;
		case 'l':
			batt_life = 1;
			all_info = 0;
			break;
		case 's':
			apm_status = 1;
			all_info = 0;
			break;
		case 'z':
			sleep = 1;
			all_info = 0;
			break;
		case '?':
		default:
			usage();
		}
		argc -= optind;
		argv += optind;
	}
finish_option:
	fd = open(APMDEV, O_RDWR);
	if (fd == -1) {
		warn("can't open %s", APMDEV);
		return 1;
	}
	if (sleep)
		apm_suspend(fd);
	else {
		struct apm_info info;

		apm_getinfo(fd, &info);
		if (all_info)
			print_all_info(&info);
		if (batt_status)
			printf("%d\n", info.ai_batt_stat);
		if (batt_life)
			printf("%d\n", info.ai_batt_life);
		if (ac_status)
			printf("%d\n", info.ai_acline);
		if (apm_status)
			printf("%d\n", info.ai_status);
		if (display)
			apm_display(fd, display - 1);
	}
	close(fd);
	return 0;
}
