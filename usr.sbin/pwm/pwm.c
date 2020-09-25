/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2018 Emmanuel Vadot <manu@FreeBSD.org>
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
#include <dev/pwm/pwmc.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <capsicum_helpers.h>

#define	PWM_ENABLE	0x0001
#define	PWM_DISABLE	0x0002
#define	PWM_SHOW_CONFIG	0x0004
#define	PWM_PERIOD	0x0008
#define	PWM_DUTY	0x0010

static char device_name[PATH_MAX] = "/dev/pwm/pwmc0.0";

static void
set_device_name(const char *name)
{

	if (name[0] == '/')
		strlcpy(device_name, name, sizeof(device_name));
	else
		snprintf(device_name, sizeof(device_name), "/dev/pwm/%s", name);
}

static void
usage(void)
{
	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "\tpwm [-f dev] -C\n");
	fprintf(stderr, "\tpwm [-f dev] [-D | -E] [-p period] [-d duty[%%]]\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	struct pwm_state state;
	int fd;
	u_int period, duty;
	int action, ch;
	cap_rights_t right_ioctl;
	const unsigned long pwm_ioctls[] = {PWMGETSTATE, PWMSETSTATE};
	char *percent;
	bool setname;

	action = 0;
	setname = false;
	fd = -1;
	period = duty = -1;

	while ((ch = getopt(argc, argv, "f:EDCp:d:")) != -1) {
		switch (ch) {
		case 'E':
			if (action & (PWM_DISABLE | PWM_SHOW_CONFIG))
				usage();
			action |= PWM_ENABLE;
			break;
		case 'D':
			if (action & (PWM_ENABLE | PWM_SHOW_CONFIG))
				usage();
			action |= PWM_DISABLE;
			break;
		case 'C':
			if (action)
				usage();
			action = PWM_SHOW_CONFIG;
			break;
		case 'p':
			if (action & PWM_SHOW_CONFIG)
				usage();
			action |= PWM_PERIOD;
			period = strtoul(optarg, NULL, 10);
			break;
		case 'd':
			if (action & PWM_SHOW_CONFIG)
				usage();
			action |= PWM_DUTY;
			duty = strtoul(optarg, &percent, 10);
			if (*percent == '%') {
				if (duty > 100) {
					fprintf(stderr,
					    "Invalid duty percentage\n");
					usage();
				}
			} else if (*percent != '\0')
				usage();
			break;
		case 'f':
			setname = true;
			set_device_name(optarg);
			break;
		case '?':
			usage();
			break;
		}
	}

	if (action == 0)
		usage();

	if ((fd = open(device_name, O_RDWR)) == -1) {
		fprintf(stderr, "pwm: cannot open %s: %s\n",
		    device_name, strerror(errno));
		if (setname)
			exit(1);
		else
			usage();
	}

	if (caph_limit_stdio() < 0) {
		fprintf(stderr, "can't limit stdio rights");
		goto fail;
	}
	caph_cache_catpages();
	cap_rights_init(&right_ioctl, CAP_IOCTL);
	if (caph_rights_limit(fd, &right_ioctl) < 0) {
		fprintf(stderr, "cap_right_limit() failed\n");
		goto fail;
	}
	if (caph_ioctls_limit(fd, pwm_ioctls, nitems(pwm_ioctls)) < 0) {
		fprintf(stderr, "caph_ioctls_limit() failed\n");
		goto fail;
	}
	if (caph_enter() < 0) {
		fprintf(stderr, "failed to enter capability mode\n");
		goto fail;
	}

	/* Fill the common args */
	if (ioctl(fd, PWMGETSTATE, &state) == -1) {
		fprintf(stderr, "Cannot get current state of the pwm controller\n");
		goto fail;
	}

	if (action == PWM_SHOW_CONFIG) {
		printf("period: %u\nduty: %u\nenabled:%d\n",
		    state.period,
		    state.duty,
		    state.enable);
	} else {
		if (action & PWM_ENABLE)
			state.enable = true;
		if (action & PWM_DISABLE)
			state.enable = false;
		if (action & PWM_PERIOD)
			state.period = period;
		if (action & PWM_DUTY) {
			if (*percent != '\0')
				state.duty = (uint64_t)state.period * duty / 100;
			else
				state.duty = duty;
		}

		if (ioctl(fd, PWMSETSTATE, &state) == -1) {
			fprintf(stderr,
			  "Cannot configure the pwm controller\n");
			goto fail;
		}
	}

	close(fd);
	return (0);

fail:
	close(fd);
	return (1);
}
