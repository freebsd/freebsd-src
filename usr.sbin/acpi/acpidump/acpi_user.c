/*-
 * Copyright (c) 1999 Doug Rabson
 * Copyright (c) 2000 Mitsuru IWASAKI <iwasaki@FreeBSD.org>
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
 *	$Id: acpi_user.c,v 1.5 2000/08/09 14:47:52 iwasaki Exp $
 *	$FreeBSD$
 */

#include <sys/param.h>
#include <sys/mman.h>
#include <sys/queue.h>
#include <sys/stat.h>

#include <err.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "acpidump.h"

static int      acpi_mem_fd = -1;

struct acpi_user_mapping {
	LIST_ENTRY(acpi_user_mapping) link;
	vm_offset_t     pa;
	caddr_t         va;
	size_t          size;
};

LIST_HEAD(acpi_user_mapping_list, acpi_user_mapping) maplist;

static void
acpi_user_init()
{

	if (acpi_mem_fd == -1) {
		acpi_mem_fd = open("/dev/mem", O_RDONLY);
		if (acpi_mem_fd == -1)
			err(1, "opening /dev/mem");
		LIST_INIT(&maplist);
	}
}

static struct acpi_user_mapping *
acpi_user_find_mapping(vm_offset_t pa, size_t size)
{
	struct	acpi_user_mapping *map;

	/* First search for an existing mapping */
	for (map = LIST_FIRST(&maplist); map; map = LIST_NEXT(map, link)) {
		if (map->pa <= pa && map->size >= pa + size - map->pa)
			return (map);
	}

	/* Then create a new one */
	size = round_page(pa + size) - trunc_page(pa);
	pa = trunc_page(pa);
	map = malloc(sizeof(struct acpi_user_mapping));
	if (!map)
		errx(1, "out of memory");
	map->pa = pa;
	map->va = mmap(0, size, PROT_READ, MAP_SHARED, acpi_mem_fd, pa);
	map->size = size;
	if ((intptr_t) map->va == -1)
		err(1, "can't map address");
	LIST_INSERT_HEAD(&maplist, map, link);

	return (map);
}

/*
 * Public interfaces
 */

struct ACPIrsdp *
acpi_find_rsd_ptr()
{
	int		i;
	u_int8_t	buf[sizeof(struct ACPIrsdp)];

	acpi_user_init();
	for (i = 0; i < 1024 * 1024; i += 16) {
		read(acpi_mem_fd, buf, 16);
		if (!memcmp(buf, "RSD PTR ", 8)) {
			/* Read the rest of the structure */
			read(acpi_mem_fd, buf + 16, sizeof(struct ACPIrsdp) - 16);

			/* Verify checksum before accepting it. */
			if (acpi_checksum(buf, sizeof(struct ACPIrsdp)))
				continue;
			return (acpi_map_physical(i, sizeof(struct ACPIrsdp)));
		}
	}

	return (0);
}

void *
acpi_map_physical(vm_offset_t pa, size_t size)
{
	struct	acpi_user_mapping *map;

	map = acpi_user_find_mapping(pa, size);
	return (map->va + (pa - map->pa));
}

void
acpi_load_dsdt(char *dumpfile, u_int8_t **dpp, u_int8_t **endp)
{
	u_int8_t	*dp;
	u_int8_t	*end;
	struct	stat sb;

	if ((acpi_mem_fd = open(dumpfile, O_RDONLY)) == -1) {
		errx(1, "opening %s\n", dumpfile);
	}

	LIST_INIT(&maplist);

	if (fstat(acpi_mem_fd, &sb) == -1) {
		errx(1, "fstat %s\n", dumpfile);
	}

	dp = mmap(0, sb.st_size, PROT_READ, MAP_PRIVATE, acpi_mem_fd, 0);
	if (dp == NULL) {
		errx(1, "mmap %s\n", dumpfile);
	}

	if (strncmp(dp, "DSDT", 4) == 0) {
		memcpy(&dsdt_header, dp, SIZEOF_SDT_HDR);
		dp += SIZEOF_SDT_HDR;
		sb.st_size -= SIZEOF_SDT_HDR;
	}

	end = (u_int8_t *) dp + sb.st_size;
	*dpp = dp;
	*endp = end;
}
