/*-
 * Copyright (c) 2001 Jonathan Lemon <jlemon@FreeBSD.org>
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
 * $FreeBSD$
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/sysctl.h>

static void __dead2
usage(void)
{
	const char *pname = getprogname();

	fprintf(stderr, "usage: %s [list]\n", pname);
	fprintf(stderr, "       %s mute on|off\n", pname);
	fprintf(stderr, "       %s add|delete console\n", pname);
	exit(1);
}

#define CONSBUFSIZE	32

static void
constatus(void)
{
	int mute;
	size_t len;
	char *buf, *p, *avail;

	len = sizeof(mute);
	if (sysctlbyname("kern.consmute", &mute, &len, NULL, 0) == -1)
		goto fail;

	len = 0;
alloc:
	len += CONSBUFSIZE;
	buf = malloc(len);
	if (buf == NULL)
		err(1, "Could not malloc sysctl buffer");

	if (sysctlbyname("kern.console", buf, &len, NULL, 0) == -1) {
		if (errno == ENOMEM) {
			free(buf);
			goto alloc;
		}
		goto fail;
	}
	avail = strchr(buf, '/');
	p = avail;
	*avail++ = '\0';
	if (p != buf)
		*--p = '\0';			/* remove trailing ',' */
	p = avail + strlen(avail);
	if (p != avail)
		*--p = '\0';			/* remove trailing ',' */
	printf("Configured: %s\n", buf);
	printf(" Available: %s\n", avail);
	printf("    Muting: %s\n", mute ? "on" : "off");
	free(buf);
	return;
fail:
	err(1, "Could not get console information");
}

static void
consmute(const char *onoff)
{
	int mute;
	size_t len;

	if (strcmp(onoff, "on") == 0)
		mute = 1;
	else if (strcmp(onoff, "off") == 0)
		mute = 0;
	else
		usage();
	len = sizeof(mute);
	if (sysctlbyname("kern.consmute", NULL, NULL, &mute, len) == -1)
		err(1, "Could not change console muting");
}

static void
consadd(char *devname)
{
	size_t len;

	len = strlen(devname);
	if (sysctlbyname("kern.console", NULL, NULL, devname, len) == -1)
		err(1, "Could not add %s as a console", devname);
}

#define MAXDEVNAME	32

static void
consdel(const char *devname)
{
	char buf[MAXDEVNAME];
	size_t len;

	snprintf(buf, MAXDEVNAME, "-%s", devname);
	len = strlen(buf);
	if (sysctlbyname("kern.console", NULL, NULL, &buf, len) == -1)
		err(1, "Could not remove %s as a console", devname);
}

int
main(int argc, char **argv)
{

	if (argc < 2 || strcmp(argv[1], "list") == 0)
		goto done;
	if (argc < 3)
		usage();
	if (strcmp(argv[1], "mute") == 0)
		consmute(argv[2]);
	else if (strcmp(argv[1], "add") == 0)
		consadd(argv[2]);
	else if (strcmp(argv[1], "delete") == 0)
		consdel(argv[2]);
	else
		usage();
done:
	constatus();
}
