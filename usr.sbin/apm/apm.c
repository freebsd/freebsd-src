/*
 * apm / zzz	APM BIOS utility for FreeBSD
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

#ifndef lint
static const char rcsid[] =
	"$Id: apm.c,v 1.14 1998/09/04 16:08:54 imp Exp $";
#endif /* not lint */

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <machine/apm_bios.h>
#include <time.h>

#define APMDEV	"/dev/apm"

#define xh(a)	(((a) & 0xff00) >> 8)
#define xl(a)	((a) & 0xff)
#define APMERR(a) xh(a)

int cmos_wall = 0;	/* True when wall time is in cmos clock, else UTC */

void
usage()
{
	fprintf(stderr, "%s\n%s\n",
		"usage: apm [-ablstzZ] [-d 1|0] [-r delta]",
		"       zzz");
	exit(1);
}

int
int2bcd(int i)
{
	int retval = 0;
	int base = 0;

	if (i >= 10000)
		return -1;
    
	while (i) {
		retval |= (i % 10) << base;
		i /= 10;
		base += 4;
	}
	return retval;
}

int
bcd2int(int bcd)
{
	int retval = 0;
	int place = 1;

	if (bcd > 0x9999)
		return -1;

	while (bcd) {
		retval += (bcd & 0xf) * place;
		bcd >>= 4;
		place *= 10;
	}
	return retval;
}

void 
apm_suspend(int fd)
{
	if (ioctl(fd, APMIO_SUSPEND, NULL) == -1)
		err(1, NULL);
}

void 
apm_standby(int fd)
{
	if (ioctl(fd, APMIO_STANDBY, NULL) == -1)
		err(1, NULL);
}

void 
apm_getinfo(int fd, apm_info_t aip)
{
	if (ioctl(fd, APMIO_GETINFO, aip) == -1)
		err(1, NULL);
}

void 
print_all_info(int fd, apm_info_t aip)
{
	struct apm_bios_arg args;
	int apmerr;

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
	printf("Remaining battery time: ");
	if (aip->ai_batt_time == -1)
		printf("unknown");
	else {
		int t, h, m, s;

		t = aip->ai_batt_time;
		s = t % 60;
		t /= 60;
		m = t % 60;
		t /= 60;
		h = t;
		printf("%2d:%02d:%02d", h, m, s);
	}
	printf("\n");
	if (aip->ai_infoversion >= 1) {
		printf("Number of batteries: ");
		if (aip->ai_batteries == (u_int) -1)
			printf("unknown\n");
		else
			printf("%d\n", aip->ai_batteries);
	}

	/*
	 * try to get the suspend timer
	 */
	bzero(&args, sizeof(args));
	args.eax = (APM_BIOS) << 8 | APM_RESUMETIMER;
	args.ebx = PMDV_APMBIOS;
	args.ecx = 0x0001;
	if (ioctl(fd, APMIO_BIOS, &args)) {
		err(1,"Get resume timer");
	} else {
		apmerr = APMERR(args.eax);
		 if (apmerr == 0x0d || apmerr == 0x86)
			printf("Resume timer: disabled\n");
		else if (apmerr)
			fprintf(stderr, 
			    "Failed to get the resume timer: APM error0x%x\n",
			    apmerr);
		else {
			/*
			 * OK.  We have the time (all bcd).
			 * CH - seconds
			 * DH - hours
			 * DL - minutes
			 * xh(SI) - month (1-12)
			 * xl(SI) - day of month (1-31)
			 * DI - year
			 */
			struct tm tm;
			char buf[1024];
			time_t t;

			tm.tm_sec = bcd2int(xh(args.ecx));
			tm.tm_min = bcd2int(xl(args.edx));
			tm.tm_hour = bcd2int(xh(args.edx));
			tm.tm_mday = bcd2int(xl(args.esi));
			tm.tm_mon = bcd2int(xh(args.esi)) - 1;
			tm.tm_year = bcd2int(args.edi) - 1900;
			if (cmos_wall)
				t = mktime(&tm);
			else
				t = timegm(&tm);
			tm = *localtime(&t);
			strftime(buf, sizeof(buf), "%c", &tm);
			printf("Resume timer: %s\n", buf);
		}
	}

	/*
	 * Get the ring indicator resume state
	 */
	bzero(&args, sizeof(args));
	args.eax  = (APM_BIOS) << 8 | APM_RESUMEONRING;
	args.ebx = PMDV_APMBIOS;
	args.ecx = 0x0002;
	if (ioctl(fd, APMIO_BIOS, &args) == 0) {
		printf("Resume on ring indicator: %sabled\n",
			args.ecx ? "en" : "dis");
	}

	if (aip->ai_infoversion >= 1) {
		printf("APM Capacities:\n", aip->ai_capabilities);
		if (aip->ai_capabilities == 0xff00)
			printf("\tunknown\n");
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


void
apm_set_timer(int fd, int delta)
{
	time_t tmr;
	struct tm *tm;
	struct apm_bios_arg args;

	tmr = time(NULL) + delta;
	if (cmos_wall)
		tm = localtime(&tmr);
	else
		tm = gmtime(&tmr);
	bzero(&args, sizeof(args));
	args.eax = (APM_BIOS) << 8 | APM_RESUMETIMER;
	args.ebx = PMDV_APMBIOS;
	if (delta > 0) {
		args.ecx = (int2bcd(tm->tm_sec) << 8) | 0x02;
		args.edx = (int2bcd(tm->tm_hour) << 8) | int2bcd(tm->tm_min);
		args.esi = (int2bcd(tm->tm_mon + 1) << 8) | int2bcd(tm->tm_mday);
		args.edi = int2bcd(tm->tm_year + 1900);
	} else {
		args.ecx = 0x0000;
	}
	if (ioctl(fd, APMIO_BIOS, &args)) {
		err(1,"Set resume timer");
	}
}

int 
main(int argc, char *argv[])
{
	int	c, fd;
	int     sleep = 0, all_info = 1, apm_status = 0, batt_status = 0;
	int     display = 0, batt_life = 0, ac_status = 0, standby = 0;
	int	batt_time = 0, delta = 0;
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
	while ((c = getopt(argc, argv, "ablRr:stzd:Z")) != -1) {
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
		case 'R':
			delta = -1;
			break;
		case 'r':
			delta = atoi(optarg);
			break;
		case 's':
			apm_status = 1;
			all_info = 0;
			break;
		case 't':
			batt_time = 1;
			all_info = 0;
			break;
		case 'z':
			sleep = 1;
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
finish_option:
	fd = open(APMDEV, O_RDWR);
	if (fd == -1) {
		warn("can't open %s", APMDEV);
		return 1;
	}
	if (delta)
		apm_set_timer(fd, delta);
	if (sleep)
		apm_suspend(fd);
	else if (standby)
		apm_standby(fd);
	else if (delta == 0) {
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
		if (apm_status)
			printf("%d\n", info.ai_status);
		if (batt_time)
			printf("%d\n", info.ai_batt_time);
		if (display)
			apm_display(fd, display - 1);
	}
	close(fd);
	return 0;
}
