/*-
 * Copyright (c) 2009, Oleksandr Tymoshenko <gonzo@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/gpio.h>

struct flag_desc {
	const char *name;
	uint32_t flag;
};

static struct flag_desc gpio_flags[] = {
	{ "IN", GPIO_PIN_INPUT },
	{ "OUT", GPIO_PIN_OUTPUT },
	{ "OD", GPIO_PIN_OPENDRAIN },
	{ "PP", GPIO_PIN_PUSHPULL },
	{ "TS", GPIO_PIN_TRISTATE },
	{ "PU", GPIO_PIN_PULLUP },
	{ "PD", GPIO_PIN_PULLDOWN },
	{ "II", GPIO_PIN_INVIN },
	{ "IO", GPIO_PIN_INVOUT },
	{ "PULSE", GPIO_PIN_PULSATE },
	{ NULL, 0 },
};

int str2cap(const char *str);

static void
usage(void)
{
	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "\tgpioctl -f ctldev -l [-v]\n");
	fprintf(stderr, "\tgpioctl -f ctldev -t pin\n");
	fprintf(stderr, "\tgpioctl -f ctldev -c pin flag ...\n");
	fprintf(stderr, "\tgpioctl -f ctldev pin [0|1]\n");
	exit(1);
}

static const char *
cap2str(uint32_t cap)
{
	struct flag_desc * pdesc = gpio_flags;
	while (pdesc->name) {
		if (pdesc->flag == cap)
			return pdesc->name;
		pdesc++;
	}

	return "UNKNOWN";
}

int
str2cap(const char *str)
{
	struct flag_desc * pdesc = gpio_flags;
	while (pdesc->name) {
		if (strcasecmp(str, pdesc->name) == 0)
			return pdesc->flag;
		pdesc++;
	}

	return (-1);
}

/*
 * Our handmade function for converting string to number
 */
static int 
str2int(const char *s, int *ok)
{
	char *endptr;
	int res = strtod(s, &endptr);
	if (endptr != s + strlen(s) )
		*ok = 0;
	else
		*ok = 1;

	return res;
}

static void
print_caps(int caps)
{
	int i, need_coma;

	need_coma = 0;
	printf("<");
	for (i = 0; i < 32; i++) {
		if (caps & (1 << i)) {
			if (need_coma)
				printf(",");
			printf("%s", cap2str(1 << i));
			need_coma = 1;
		}
	}
	printf(">");
}

static void
dump_pins(int fd, int verbose)
{
	int i, maxpin;
	struct gpio_pin pin;
	struct gpio_req req;

	if (ioctl(fd, GPIOMAXPIN, &maxpin) < 0) {
		perror("ioctl(GPIOMAXPIN)");
		exit(1);
	}

	for (i = 0; i <= maxpin; i++) {
		pin.gp_pin = i;
		if (ioctl(fd, GPIOGETCONFIG, &pin) < 0)
			/* For some reason this pin is inaccessible */
			continue;

		req.gp_pin = i;
		if (ioctl(fd, GPIOGET, &req) < 0) {
			/* Now, that's wrong */
			perror("ioctl(GPIOGET)");
			exit(1);
		}

		printf("pin %02d:\t%d\t%s", pin.gp_pin, req.gp_value, 
		    pin.gp_name);

		print_caps(pin.gp_flags);

		if (verbose) {
			printf(", caps:");
			print_caps(pin.gp_caps);
		}
		printf("\n");
	}
}

static void 
fail(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	exit(1);
}

int 
main(int argc, char **argv)
{
	int i;
	struct gpio_pin pin;
	struct gpio_req req;
	char *ctlfile = NULL;
	int pinn, pinv, fd, ch;
	int flags, flag, ok;
	int config, toggle, verbose, list;

	config = toggle = verbose = list = pinn = 0;

	while ((ch = getopt(argc, argv, "c:f:lt:v")) != -1) {
		switch (ch) {
		case 'c':
			config = 1;
			pinn = str2int(optarg, &ok);
			if (!ok)
				fail("Invalid pin number: %s\n", optarg);
			break;
		case 'f':
			ctlfile = optarg;
			break;
		case 'l':
			list = 1;
			break;
		case 't':
			toggle = 1;
			pinn = str2int(optarg, &ok);
			if (!ok)
				fail("Invalid pin number: %s\n", optarg);
			break;
		case 'v':
			verbose = 1;
			break;
		default:
			usage();
			break;
		}
	}
	argv += optind;
	argc -= optind;
	for (i = 0; i < argc; i++)
		printf("%d/%s\n", i, argv[i]);

	if (ctlfile == NULL)
		fail("No gpioctl device provided\n");

	fd = open(ctlfile, O_RDONLY);
	if (fd < 0) {
		perror("open");
		exit(1);
	}

	if (list) {
		dump_pins(fd, verbose);
		close(fd);
		exit(0);
	}

	if (toggle) {
		/*
		 * -t pin assumes no additional arguments
		 */
		if(argc > 0) {
			usage();
			exit(1);
		}

		req.gp_pin = pinn;
		if (ioctl(fd, GPIOTOGGLE, &req) < 0) {
			perror("ioctl(GPIOTOGGLE)");
			exit(1);
		}

		close(fd);
		exit (0);
	}

	if (config) {
		flags = 0;
		for (i = 0; i < argc; i++) {
			flag = 	str2cap(argv[i]);
			if (flag < 0)
				fail("Invalid flag: %s\n", argv[i]);
			flags |= flag;
		}

		pin.gp_pin = pinn;
		pin.gp_flags = flags;
		if (ioctl(fd, GPIOSETCONFIG, &pin) < 0) {
			perror("ioctl(GPIOSETCONFIG)");
			exit(1);
		}

		exit(0);
	}

	/*
	 * Last two cases - set value or print value
	 */
	if ((argc == 0) || (argc > 2)) {
		usage();
		exit(1);
	}

	pinn = str2int(argv[0], &ok);
	if (!ok)
		fail("Invalid pin number: %s\n", argv[0]);

	/*
	 * Read pin value
	 */
	if (argc == 1) {
		req.gp_pin = pinn;
		if (ioctl(fd, GPIOGET, &req) < 0) {
			perror("ioctl(GPIOGET)");
			exit(1);
		}
		printf("%d\n", req.gp_value);
		exit (0);
	}

	/* Is it valid number (0 or 1) ? */
	pinv = str2int(argv[1], &ok);
	if (!ok || ((pinv != 0) && (pinv != 1)))
		fail("Invalid pin value: %s\n", argv[1]);

	/*
	 * Set pin value
	 */
	req.gp_pin = pinn;
	req.gp_value = pinv;
	if (ioctl(fd, GPIOSET, &req) < 0) {
		perror("ioctl(GPIOSET)");
		exit(1);
	}

	close(fd);
	exit(0);
}
