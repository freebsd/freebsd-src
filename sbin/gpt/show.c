/*-
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>

#include <err.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
	static uuid_t efi_slice = GPT_ENT_TYPE_EFI;
	static uuid_t mslinux = GPT_ENT_TYPE_MS_BASIC_DATA;
	static uuid_t freebsd = GPT_ENT_TYPE_FREEBSD;
	static uuid_t linuxswap = GPT_ENT_TYPE_LINUX_SWAP;
	static uuid_t msr = GPT_ENT_TYPE_MS_RESERVED;
	static uuid_t swap = GPT_ENT_TYPE_FREEBSD_SWAP;
	static uuid_t ufs = GPT_ENT_TYPE_FREEBSD_UFS;
	static uuid_t vinum = GPT_ENT_TYPE_FREEBSD_VINUM;
	static char buf[80];
	char *s;

	if (uuid_equal(t, &efi_slice, NULL))
		return ("EFI System");
	if (uuid_equal(t, &swap, NULL))
		return ("FreeBSD swap");
	if (uuid_equal(t, &ufs, NULL))
		return ("FreeBSD UFS/UFS2");
	if (uuid_equal(t, &vinum, NULL))
		return ("FreeBSD vinum");

	if (uuid_equal(t, &freebsd, NULL))
		return ("FreeBSD legacy");
	if (uuid_equal(t, &mslinux, NULL))
		return ("Linux/Windows");
	if (uuid_equal(t, &linuxswap, NULL))
		return ("Linux swap");
	if (uuid_equal(t, &msr, NULL))
		return ("Windows reserved");

	uuid_to_string(t, &s, NULL);
	strlcpy(buf, s, sizeof buf);
	free(s);
	return (buf);
}

static void
show(int fd __unused)
{
	uuid_t type;
	off_t start;
	map_t *m, *p;
	struct mbr *mbr;
	struct gpt_ent *ent;
	unsigned int i;

	printf("  %*s", lbawidth, "start");
	printf("  %*s", lbawidth, "size");
	printf("  index  contents\n");

	m = map_first();
	while (m != NULL) {
		printf("  %*llu", lbawidth, (long long)m->map_start);
		printf("  %*llu", lbawidth, (long long)m->map_size);
		putchar(' ');
		putchar(' ');
		if (m->map_index > 0)
			printf("%5d", m->map_index);
		else
			printf("     ");
		putchar(' ');
		putchar(' ');
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
				start = le16toh(mbr->mbr_part[i].part_start_hi);
				start = (start << 16) +
				    le16toh(mbr->mbr_part[i].part_start_lo);
				if (m->map_start == p->map_start + start)
					break;
			}
			printf("%d", mbr->mbr_part[i].part_typ);
			break;
		case MAP_TYPE_GPT_PART:
			printf("GPT part ");
			ent = m->map_data;
			le_uuid_dec(&ent->ent_type, &type);
			printf("- %s", friendly(&type));
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
