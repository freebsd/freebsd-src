/*
 * Copyright (c) 2002 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
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
#include <sys/uuid.h>
#include <sys/gpt.h>

#include <err.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "map.h"
#include "gpt.h"

int recoverable;

static void
usage_destroy(void)
{

	fprintf(stderr,
	    "usage: %s [-r] device ...\n", getprogname());
	exit(1);
}

static void
destroy(int fd)
{
	map_t *pri_hdr, *sec_hdr;

	pri_hdr = map_find(MAP_TYPE_PRI_GPT_HDR);
	sec_hdr = map_find(MAP_TYPE_SEC_GPT_HDR);

	if (pri_hdr == NULL && sec_hdr == NULL) {
		warnx("%s: error: device doesn't contain a GPT", device_name);
		return;
	}

	if (recoverable && sec_hdr == NULL) {
		warnx("%s: error: recoverability not possible", device_name);
		return;
	}

	if (pri_hdr != NULL) {
		bzero(pri_hdr->map_data, secsz);
		gpt_write(fd, pri_hdr);
	}

	if (!recoverable && sec_hdr != NULL) {
		bzero(sec_hdr->map_data, secsz);
		gpt_write(fd, sec_hdr);
	}
}

int
cmd_destroy(int argc, char *argv[])
{
	int ch, fd;

	while ((ch = getopt(argc, argv, "r")) != -1) {
		switch(ch) {
		case 'r':
			recoverable = 1;
			break;
		default:
			usage_destroy();
		}
	}

	if (argc == optind)
		usage_destroy();

	while (optind < argc) {
		fd = gpt_open(argv[optind++]);
		if (fd == -1) {
			warn("unable to open device '%s'", device_name);
			continue;
		}

		destroy(fd);

		gpt_close(fd);
	}

	return (0);
}
