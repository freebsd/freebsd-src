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
#include <sys/gpt.h>

#include <err.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <uuid.h>

#include "map.h"
#include "gpt.h"

static void
usage_show(void)
{

	fprintf(stderr,
	    "usage: %s device ...\n", getprogname());
	exit(1);
}

static const char *
friendly(uuid_t *t)
{
	uuid_t efi_slice = GPT_ENT_TYPE_EFI;
	uuid_t freebsd = GPT_ENT_TYPE_FREEBSD;
	uuid_t swap = GPT_ENT_TYPE_FREEBSD_SWAP;
	uuid_t ufs = GPT_ENT_TYPE_FREEBSD_UFS;
	uuid_t vinum = GPT_ENT_TYPE_FREEBSD_VINUM;
	static char buf[80];
	char *s;

	if (memcmp(t, &efi_slice, sizeof(uuid_t)) == 0)
		return "EFI System partition";
	else if (memcmp(t, &freebsd, sizeof(uuid_t)) == 0)
		return "FreeBSD disklabel container";
	else if (memcmp(t, &swap, sizeof(uuid_t)) == 0)
		return "FreeBSD swap partition";
	else if (memcmp(t, &ufs, sizeof(uuid_t)) == 0)
		return "FreeBSD ufs partition";
	else if (memcmp(t, &vinum, sizeof(uuid_t)) == 0)
		return "FreeBSD vinum partition";
	uuid_to_string(t, &s, NULL);
	strlcpy(buf, s, sizeof buf);
	free(s);
	return buf;
}

static void
show(int fd __unused)
{
	off_t start, end;
	map_t *m, *p;
	struct mbr *mbr;
	struct gpt_ent *ent;
	unsigned int i;

	printf("  %*s", lbawidth, "start");
	printf("  %*s", lbawidth, "end");
	printf("  %*s", lbawidth, "size");
	printf("  %s\n", "contents");

	m = map_first();
	while (m != NULL) {
		end = m->map_start + m->map_size - 1;
		printf("  %*llu", lbawidth, (long long)m->map_start);
		printf("  %*llu", lbawidth, (long long)end);
		printf("  %*llu", lbawidth, (long long)m->map_size);

		putchar(' '); putchar(' ');
		switch (m->map_type) {
		case MAP_TYPE_MBR:
			if (m->map_start != 0)
				printf("Extended ");
			printf("MBR");
			break;
		case MAP_TYPE_PRI_GPT_HDR:
			printf("Pri GPT header");
			break;
		case MAP_TYPE_SEC_GPT_HDR:
			printf("Sec GPT header");
			break;
		case MAP_TYPE_PRI_GPT_TBL:
			printf("Pri GPT table");
			break;
		case MAP_TYPE_SEC_GPT_TBL:
			printf("Sec GPT table");
			break;
		case MAP_TYPE_MBR_PART:
			p = m->map_data;
			if (p->map_start != 0)
				printf("Extended ");
			printf("MBR part ");
			mbr = p->map_data;
			for (i = 0; i < 4; i++) {
				start = mbr->mbr_part[i].part_start_hi << 16;
				start += mbr->mbr_part[i].part_start_lo;
				if (m->map_start == p->map_start + start)
					break;
			}
			printf("%d", mbr->mbr_part[i].part_typ);
			break;
		case MAP_TYPE_GPT_PART:
			printf("GPT part ");
			ent = m->map_data;
			printf("- %s", friendly(&ent->ent_type));
			break;
		case MAP_TYPE_PMBR:
			printf("PMBR");
			break;
		}
		putchar('\n');
		m = m->map_next;
	}
}

int
cmd_show(int argc, char *argv[])
{
	int ch, fd;

	while ((ch = getopt(argc, argv, "")) != -1) {
		switch(ch) {
		default:
			usage_show();
		}
	}

	if (argc == optind)
		usage_show();

	while (optind < argc) {
		fd = gpt_open(argv[optind++]);
		if (fd == -1) {
			warn("unable to open device '%s'", device_name);
			continue;
		}

		show(fd);

		gpt_close(fd);
	}

	return (0);
}
