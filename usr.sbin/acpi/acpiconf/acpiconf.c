/*-
 * Copyright (c) 1999 Mitsuru IWASAKI <iwasaki@FreeBSD.org>
 * All rights reserved.
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
 *	$Id: acpiconf.c,v 1.5 2000/08/08 14:12:19 iwasaki Exp $
 *	$FreeBSD$
 */

#include <sys/param.h>

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sysexits.h>
#include <unistd.h>

#include <dev/acpica/acpiio.h>
#include <contrib/dev/acpica/acpi.h>

#define ACPIDEV		"/dev/acpi"
#define RC_SUSPEND_PATH	"/etc/rc.suspend"
#define RC_RESUME_PATH	"/etc/rc.resume"

static int	acpifd;

static void
acpi_init(void)
{
	acpifd = open(ACPIDEV, O_RDWR);
	if (acpifd == -1)
		acpifd = open(ACPIDEV, O_RDONLY);
	if (acpifd == -1)
		err(EX_OSFILE, ACPIDEV);
}

static int
acpi_sleep(int sleep_type)
{
	char cmd[64];
	int ret;

	/* Run the suspend rc script, if available. */
	if (access(RC_SUSPEND_PATH, X_OK) == 0) {
		snprintf(cmd, sizeof(cmd), "%s acpi %d", RC_SUSPEND_PATH,
		    sleep_type);
		system(cmd);
	}

	ret = ioctl(acpifd, ACPIIO_SETSLPSTATE, &sleep_type);

	/* Run the resume rc script, if available. */
	if (access(RC_RESUME_PATH, X_OK) == 0) {
		snprintf(cmd, sizeof(cmd), "%s acpi %d", RC_RESUME_PATH,
		    sleep_type);
		system(cmd);
	}

	if (ret != 0)
		err(EX_IOERR, "sleep type (%d) failed", sleep_type);

	return (0);
}

/* should be a acpi define, but doesn't appear to be */
#define UNKNOWN_CAP 0xffffffff

static int
acpi_battinfo(int num)
{
	union acpi_battery_ioctl_arg battio;
	const char *pwr_units;

	if (num < 0 || num > 64)
		err(EX_USAGE, "invalid battery %d", num);

	battio.unit = num;
	if (ioctl(acpifd, ACPIIO_BATT_GET_BIF, &battio) == -1)
		err(EX_IOERR, "get battery info (%d) failed", num);
	printf("Battery %d information\n", num);
	if (battio.bif.units == 0)
		pwr_units = "mWh";
	else
		pwr_units = "mAh";

	if (battio.bif.dcap == UNKNOWN_CAP)
		printf("Design capacity:\tUnknown\n");
	else
		printf("Design capacity:\t%d %s\n", battio.bif.dcap, pwr_units);
	if (battio.bif.lfcap == UNKNOWN_CAP)
		printf("Last full capacity:\tUnknown\n");
	else
		printf("Last full capacity:\t%d %s\n", battio.bif.lfcap,
		    pwr_units);
	printf("Technology:\t\t%s\n", battio.bif.btech == 0 ?
	    "primary (non-rechargeable)" : "secondary (rechargeable)");
	if (battio.bif.dvol == UNKNOWN_CAP)
		printf("Design voltage:\t\tUnknown\n");
	else
		printf("Design voltage:\t\t%d mV\n", battio.bif.dvol);
	printf("Capacity (warn):\t%d %s\n", battio.bif.wcap, pwr_units);
	printf("Capacity (low):\t\t%d %s\n", battio.bif.lcap, pwr_units);
	printf("Low/warn granularity:\t%d %s\n", battio.bif.gra1, pwr_units);
	printf("Warn/full granularity:\t%d %s\n", battio.bif.gra2, pwr_units);
	printf("Model number:\t\t%s\n", battio.bif.model);
	printf("Serial number:\t\t%s\n", battio.bif.serial);
	printf("Type:\t\t\t%s\n", battio.bif.type);
	printf("OEM info:\t\t%s\n", battio.bif.oeminfo);

	battio.unit = num;
	if (ioctl(acpifd, ACPIIO_BATT_GET_BST, &battio) == -1)
		err(EX_IOERR, "get battery info (%d) failed", num);

	if (battio.bst.state != ACPI_BATT_STAT_NOT_PRESENT) {
		printf("State:\t\t\t");
		if (battio.bst.state & ACPI_BATT_STAT_CRITICAL)
			printf("CRITICAL ");
		if (battio.bst.state & ACPI_BATT_STAT_DISCHARG)
			printf("Discharging ");
		if (battio.bst.state & ACPI_BATT_STAT_CHARGING)
			printf("Charging");
		printf("\n");
		if (battio.bst.rate == UNKNOWN_CAP)
			printf("Present Rate:\t\tUnknown\n");
		else
			printf("Present Rate:\t\t%d %s\n", battio.bst.rate,
			    pwr_units);
		if (battio.bst.cap == UNKNOWN_CAP)
			printf("Remaining Capacity:\tUnknown\n");
		else
			printf("Remaining Capacity:\t%d %s\n", battio.bst.cap,
			    pwr_units);
		if (battio.bst.volt == UNKNOWN_CAP)
			printf("Volt:\t\t\tUnknown\n");
		else
			printf("Volt:\t\t\t%d mV\n", battio.bst.volt);
	} else {
		printf("State:\t\t\tNot Present\n");
	}
	return (0);
}

static void
usage(const char* prog)
{
	printf("usage: %s [-h] [-i batt] [-s 1-5]\n", prog);
	exit(0);
}

int
main(int argc, char *argv[])
{
	char	c, *prog;
	int	sleep_type;

	prog = argv[0];
	if (argc < 2)
		usage(prog);
		/* NOTREACHED */

	sleep_type = -1;
	acpi_init();
	while ((c = getopt(argc, argv, "hi:s:")) != -1) {
		switch (c) {
		case 'i':
			acpi_battinfo(atoi(optarg));
			break;
		case 's':
			if (optarg[0] == 'S')
				sleep_type = optarg[1] - '0';
			else
				sleep_type = optarg[0] - '0';
			if (sleep_type < 0 || sleep_type > 5)
				errx(EX_USAGE, "invalid sleep type (%d)",
				     sleep_type);
			break;
		case 'h':
		default:
			usage(prog);
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	if (sleep_type != -1) {
		sleep(1);	/* wait 1 sec. for key-release event */
		acpi_sleep(sleep_type);
	}

	close(acpifd);
	exit (0);
}
