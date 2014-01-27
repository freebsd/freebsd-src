/*
 * libfdt - Flat Device Tree manipulation
 *	Testcase/tool for rearranging blocks of a dtb
 * Copyright (C) 2006 David Gibson, IBM Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>

#include <libfdt.h>

#include "tests.h"
#include "testdata.h"

struct bufstate {
	char *buf;
	int size;
};

static void expand_buf(struct bufstate *buf, int newsize)
{
	buf->buf = realloc(buf->buf, newsize);
	if (!buf->buf)
		CONFIG("Allocation failure");
	buf->size = newsize;
}

static void new_header(struct bufstate *buf, int version, const void *fdt)
{
	int hdrsize;

	if (version == 16)
		hdrsize = FDT_V16_SIZE;
	else if (version == 17)
		hdrsize = FDT_V17_SIZE;
	else
		CONFIG("Bad version %d", version);

	expand_buf(buf, hdrsize);
	memset(buf->buf, 0, hdrsize);

	fdt_set_magic(buf->buf, FDT_MAGIC);
	fdt_set_version(buf->buf, version);
	fdt_set_last_comp_version(buf->buf, 16);
	fdt_set_boot_cpuid_phys(buf->buf, fdt_boot_cpuid_phys(fdt));
}

static void add_block(struct bufstate *buf, int version, char block, const void *fdt)
{
	int align, size, oldsize;
	const void *src;
	int offset;

	switch (block) {
	case 'm':
		/* Memory reserve map */
		align = 8;
		src = (const char *)fdt + fdt_off_mem_rsvmap(fdt);
		size = (fdt_num_mem_rsv(fdt) + 1)
			* sizeof(struct fdt_reserve_entry);
		break;

	case 't':
		/* Structure block */
		align = 4;
		src = (const char *)fdt + fdt_off_dt_struct(fdt);
		size = fdt_size_dt_struct(fdt);
		break;

	case 's':
		/* Strings block */
		align = 1;
		src = (const char *)fdt + fdt_off_dt_strings(fdt);
		size = fdt_size_dt_strings(fdt);
		break;
	default:
		CONFIG("Bad block '%c'", block);
	}

	oldsize = buf->size;
	offset = ALIGN(oldsize, align);
	expand_buf(buf, offset+size);
	memset(buf->buf + oldsize, 0, offset - oldsize);

	memcpy(buf->buf + offset, src, size);

	switch (block) {
	case 'm':
		fdt_set_off_mem_rsvmap(buf->buf, offset);
		break;

	case 't':
		fdt_set_off_dt_struct(buf->buf, offset);
		if (version >= 17)
			fdt_set_size_dt_struct(buf->buf, size);
		break;

	case 's':
		fdt_set_off_dt_strings(buf->buf, offset);
		fdt_set_size_dt_strings(buf->buf, size);
		break;
	}
}

int main(int argc, char *argv[])
{
	void *fdt;
	int version;
	const char *blockorder;
	struct bufstate buf = {NULL, 0};
	int err;
	const char *inname;
	char outname[PATH_MAX];

	test_init(argc, argv);
	if (argc != 4)
		CONFIG("Usage: %s <dtb file> <version> <block order>", argv[0]);

	inname = argv[1];
	fdt = load_blob(argv[1]);
	version = atoi(argv[2]);
	blockorder = argv[3];
	sprintf(outname, "v%d.%s.%s", version, blockorder, inname);

	if ((version != 16) && (version != 17))
		CONFIG("Version must be 16 or 17");

	if (fdt_version(fdt) < 17)
		CONFIG("Input tree must be v17");

	new_header(&buf, version, fdt);

	while (*blockorder) {
		add_block(&buf, version, *blockorder, fdt);
		blockorder++;
	}

	fdt_set_totalsize(buf.buf, buf.size);

	err = fdt_check_header(buf.buf);
	if (err)
		FAIL("Output tree fails check: %s", fdt_strerror(err));

	save_blob(outname, buf.buf);

	PASS();
}
