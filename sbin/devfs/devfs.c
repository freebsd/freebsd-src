/*-
 * Copyright (c) 2002 Dima Dorfman.
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
 */

/*
 * DEVFS control.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>

#include <err.h>
#include <fcntl.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"

int mpfd;

static ctbl_t ctbl_main = {
	{ "rule",		rule_main },
	{ "ruleset",		ruleset_main },
	{ NULL,			NULL }
};

int
main(int ac, char **av)
{
	const char *mountpt;
	struct cmd *c;
	char ch;

	mountpt = NULL;
	while ((ch = getopt(ac, av, "m:")) != -1)
		switch (ch) {
		case 'm':
			mountpt = optarg;
			break;
		default:
			usage();
		}
	ac -= optind;
	av += optind;
	if (ac < 1)
		usage();

	if (mountpt == NULL)
		mountpt = _PATH_DEV;
	mpfd = open(mountpt, O_RDONLY);
	if (mpfd == -1)
		err(1, "open: %s", mountpt);

	for (c = ctbl_main; c->name != NULL; ++c)
		if (strcmp(c->name, av[0]) == 0)
			exit((*c->handler)(ac, av));
	errx(1, "unknown command: %s", av[0]);
}

/*
 * Convert an integer to a "number" (ruleset numbers and rule numbers
 * are 16-bit).  If the conversion is successful, num contains the
 * integer representation of s and 1 is returned; otherwise, 0 is
 * returned and num is unchanged.
 */
int
atonum(const char *s, uint16_t *num)
{
	unsigned long ul;
	char *cp;

	ul = strtoul(s, &cp, 10);
	if (ul > UINT16_MAX || *cp != '\0')
		return (0);
	*num = (uint16_t)ul;
	return (1);
}

/*
 * Convert user input in ASCII to an integer.
 */
int
eatoi(const char *s)
{
	char *cp;
	long l;

	l = strtol(s, &cp, 10);
	if (l > INT_MAX || *cp != '\0')
		errx(1, "error converting to integer: %s", s);
	return ((int)l);
}	

/*
 * As atonum(), but the result of failure is death.
 */
uint16_t
eatonum(const char *s)
{
	uint16_t num;

	if (!atonum(s, &num))
		errx(1, "error converting to number: %s", s); /* XXX clarify */
	return (num);
}	

void
usage(void)
{

	fprintf(stderr, "usage: devfs rule|ruleset arguments\n");
	exit(1);
}
