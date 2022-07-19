/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2020 Emmanuel Vadot <manu@FreeBSD.org>
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
#include <sys/ioctl.h>
#include <stdbool.h>
#include <sys/capsicum.h>
#include <sys/backlight.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <capsicum_helpers.h>

#define	BACKLIGHT_QUERY			0x0001
#define	BACKLIGHT_SET_BRIGHTNESS	0x0002
#define	BACKLIGHT_INCR			0x0004
#define	BACKLIGHT_DECR			0x0008
#define	BACKLIGHT_INFO			0x0010

static char device_name[PATH_MAX] = "/dev/backlight/backlight0";

static void
set_device_name(const char *name)
{

	if (name[0] == '/')
		strlcpy(device_name, name, sizeof(device_name));
	else
		snprintf(device_name, sizeof(device_name), "/dev/backlight/%s", name);
}

static void
usage(void)
{
	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "\tbacklight [-q] [-f device]\n");
	fprintf(stderr, "\tbacklight [-q] [-f device] -i\n");
	fprintf(stderr, "\tbacklight [-f device] value\n");
	fprintf(stderr, "\tbacklight [-f device] incr|+ value\n");
	fprintf(stderr, "\tbacklight [-f device] decr|- value\n");
	exit(1);
}

static const char *
backlight_type_to_string(enum backlight_info_type type)
{
	switch (type) {
	case BACKLIGHT_TYPE_PANEL:
		return ("Panel");
	case BACKLIGHT_TYPE_KEYBOARD:
		return ("Keyboard");
	}

	return ("Unknown");
}

int
main(int argc, char *argv[])
{
	struct backlight_props props;
	struct backlight_info info;
	int fd;
	int action, ch;
	cap_rights_t right_ioctl;
	const unsigned long backlight_ioctls[] = {
		BACKLIGHTGETSTATUS,
		BACKLIGHTUPDATESTATUS,
		BACKLIGHTGETINFO};
	long percent = -1;
	const char *percent_error;
	uint32_t i;
	bool quiet = false;

	action = BACKLIGHT_QUERY;
	fd = -1;

	while ((ch = getopt(argc, argv, "f:qhi")) != -1) {
		switch (ch) {
		case 'q':
			quiet = true;
			break;
		case 'f':
			set_device_name(optarg);
			break;
		case 'i':
			action = BACKLIGHT_INFO;
			break;
		case 'h':
			usage();
			break;
		}
	}

	argc -= optind;
	argv += optind;
	if (argc != 0) {
		if (strcmp("incr", argv[0]) == 0 ||
		    strcmp("+", argv[0]) == 0) {
			action = BACKLIGHT_INCR;
			argc -= 1;
			argv += 1;
		}
		else if (strcmp("decr", argv[0]) == 0 ||
		    strcmp("-", argv[0]) == 0) {
			action = BACKLIGHT_DECR;
			argc -= 1;
			argv += 1;
		} else
			action = BACKLIGHT_SET_BRIGHTNESS;

		if (argc == 1) {
			/* ignore a trailing % for user friendlyness */
			if (strlen(argv[0]) > 0 &&
			    argv[0][strlen(argv[0]) - 1] == '%')
				argv[0][strlen(argv[0]) - 1] = '\0';
			percent = strtonum(argv[0], 0, 100, &percent_error);
			if (percent_error)
				errx(1, "Cannot parse brightness level %s: %s",
				    argv[0],
				    percent_error);
		}
	}

	if ((fd = open(device_name, O_RDWR)) == -1)
		errx(1, "cannot open %s: %s",
		    device_name, strerror(errno));

	if (caph_limit_stdio() < 0)
		errx(1, "can't limit stdio rights");
	caph_cache_catpages();
	cap_rights_init(&right_ioctl, CAP_IOCTL);
	if (caph_rights_limit(fd, &right_ioctl) < 0)
		errx(1, "cap_right_limit() failed");
	if (caph_ioctls_limit(fd, backlight_ioctls, nitems(backlight_ioctls)) < 0)
		errx(1, "caph_ioctls_limit() failed");
	if (caph_enter() < 0)
		errx(1, "failed to enter capability mode");

	switch (action) {
	case BACKLIGHT_QUERY:
		if (ioctl(fd, BACKLIGHTGETSTATUS, &props) == -1)
			errx(1, "Cannot query the backlight device");
		if (quiet)
			printf("%u\n", props.brightness);
		else {
			printf("brightness: %d\n", props.brightness);
			if (props.nlevels != 0) {
				printf("levels:");
				for (i = 0; i < props.nlevels; i++)
					printf(" %d", props.levels[i]);
				printf("\n");
			}
		}
		break;
	case BACKLIGHT_SET_BRIGHTNESS:
		if (percent == -1)
			usage();
		props.brightness = percent;
		if (ioctl(fd, BACKLIGHTUPDATESTATUS, &props) == -1)
			errx(1, "Cannot update the backlight device");
		break;
	case BACKLIGHT_INCR:
	case BACKLIGHT_DECR:
		if (percent == 0)
			/* Avoid any ioctl if we don't have anything to do */
			break;
		if (ioctl(fd, BACKLIGHTGETSTATUS, &props) == -1)
			errx(1, "Cannot query the backlight device");
		percent = percent == -1 ? 10 : percent;
		percent = action == BACKLIGHT_INCR ? percent : -percent;
		props.brightness += percent;
		if ((int)props.brightness < 0)
			props.brightness = 0;
		if (props.brightness > 100)
			props.brightness = 100;
		if (ioctl(fd, BACKLIGHTUPDATESTATUS, &props) == -1)
			errx(1, "Cannot update the backlight device");
		break;
	case BACKLIGHT_INFO:
		if (ioctl(fd, BACKLIGHTGETINFO, &info) == -1)
			errx(1, "Cannot query the backlight device");
		if (quiet == false) {
			printf("Backlight name: %s\n", info.name);
			printf("Backlight hardware type: %s\n", backlight_type_to_string(info.type));
		} else {
			printf("%s\n", info.name);
			printf("%s\n", backlight_type_to_string(info.type));
		}
		break;
	}

	close(fd);
	return (0);
}
