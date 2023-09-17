/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2015 Neel Natu <neel@freebsd.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND
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

#include <sys/param.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <machine/vmm.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

#include <vmmapi.h>

#include "bhyverun.h"
#include "bootrom.h"
#include "debug.h"
#include "mem.h"

#define	BOOTROM_SIZE	(16 * 1024 * 1024)	/* 16 MB */

/*
 * ROM region is 16 MB at the top of 4GB ("low") memory.
 *
 * The size is limited so it doesn't encroach into reserved MMIO space (e.g.,
 * APIC, HPET, MSI).
 *
 * It is allocated in page-multiple blocks on a first-come first-serve basis,
 * from high to low, during initialization, and does not change at runtime.
 */
static char *romptr;	/* Pointer to userspace-mapped bootrom region. */
static vm_paddr_t gpa_base;	/* GPA of low end of region. */
static vm_paddr_t gpa_allocbot;	/* Low GPA of free region. */
static vm_paddr_t gpa_alloctop;	/* High GPA, minus 1, of free region. */

#define CFI_BCS_WRITE_BYTE      0x10
#define CFI_BCS_CLEAR_STATUS    0x50
#define CFI_BCS_READ_STATUS     0x70
#define CFI_BCS_READ_ARRAY      0xff

static struct bootrom_var_state {
	uint8_t		*mmap;
	uint64_t	gpa;
	off_t		size;
	uint8_t		cmd;
} var = { NULL, 0, 0, CFI_BCS_READ_ARRAY };

/*
 * Emulate just those CFI basic commands that will convince EDK II
 * that the Firmware Volume area is writable and persistent.
 */
static int
bootrom_var_mem_handler(struct vcpu *vcpu __unused, int dir, uint64_t addr,
    int size, uint64_t *val, void *arg1 __unused, long arg2 __unused)
{
	off_t offset;

	offset = addr - var.gpa;
	if (offset + size > var.size || offset < 0 || offset + size <= offset)
		return (EINVAL);

	if (dir == MEM_F_WRITE) {
		switch (var.cmd) {
		case CFI_BCS_WRITE_BYTE:
			memcpy(var.mmap + offset, val, size);
			var.cmd = CFI_BCS_READ_ARRAY;
			break;
		default:
			var.cmd = *(uint8_t *)val;
		}
	} else {
		switch (var.cmd) {
		case CFI_BCS_CLEAR_STATUS:
		case CFI_BCS_READ_STATUS:
			memset(val, 0, size);
			var.cmd = CFI_BCS_READ_ARRAY;
			break;
		default:
			memcpy(val, var.mmap + offset, size);
			break;
		}
	}
	return (0);
}

void
init_bootrom(struct vmctx *ctx)
{
	romptr = vm_create_devmem(ctx, VM_BOOTROM, "bootrom", BOOTROM_SIZE);
	if (romptr == MAP_FAILED)
		err(4, "%s: vm_create_devmem", __func__);
	gpa_base = (1ULL << 32) - BOOTROM_SIZE;
	gpa_allocbot = gpa_base;
	gpa_alloctop = (1ULL << 32) - 1;
}

int
bootrom_alloc(struct vmctx *ctx, size_t len, int prot, int flags,
    char **region_out, uint64_t *gpa_out)
{
	static const int bootrom_valid_flags = BOOTROM_ALLOC_TOP;

	vm_paddr_t gpa;
	vm_ooffset_t segoff;

	if (flags & ~bootrom_valid_flags) {
		warnx("%s: Invalid flags: %x", __func__,
		    flags & ~bootrom_valid_flags);
		return (EINVAL);
	}
	if (prot & ~_PROT_ALL) {
		warnx("%s: Invalid protection: %x", __func__,
		    prot & ~_PROT_ALL);
		return (EINVAL);
	}

	if (len == 0 || len > BOOTROM_SIZE) {
		warnx("ROM size %zu is invalid", len);
		return (EINVAL);
	}
	if (len & PAGE_MASK) {
		warnx("ROM size %zu is not a multiple of the page size",
		    len);
		return (EINVAL);
	}

	if (flags & BOOTROM_ALLOC_TOP) {
		gpa = (gpa_alloctop - len) + 1;
		if (gpa < gpa_allocbot) {
			warnx("No room for %zu ROM in bootrom region", len);
			return (ENOMEM);
		}
	} else {
		gpa = gpa_allocbot;
		if (gpa > (gpa_alloctop - len) + 1) {
			warnx("No room for %zu ROM in bootrom region", len);
			return (ENOMEM);
		}
	}

	segoff = gpa - gpa_base;
	if (vm_mmap_memseg(ctx, gpa, VM_BOOTROM, segoff, len, prot) != 0) {
		int serrno = errno;
		warn("%s: vm_mmap_mapseg", __func__);
		return (serrno);
	}

	if (flags & BOOTROM_ALLOC_TOP)
		gpa_alloctop = gpa - 1;
	else
		gpa_allocbot = gpa + len;

	*region_out = romptr + segoff;
	if (gpa_out != NULL)
		*gpa_out = gpa;
	return (0);
}

int
bootrom_loadrom(struct vmctx *ctx, const nvlist_t *nvl)
{
	struct stat sbuf;
	ssize_t rlen;
	off_t rom_size, var_size, total_size;
	char *ptr, *romfile;
	int fd, varfd, i, rv;
	const char *bootrom, *varfile;

	rv = -1;
	varfd = -1;

	bootrom = get_config_value_node(nvl, "bootrom");
	if (bootrom == NULL) {
		return (-1);
	}

	/*
	 * get_config_value_node may use a thread local buffer to return
	 * variables. So, when we query the second variable, the first variable
	 * might get overwritten. For that reason, the bootrom should be
	 * duplicated.
	 */
	romfile = strdup(bootrom);
	if (romfile == NULL) {
		return (-1);
	}

	fd = open(romfile, O_RDONLY);
	if (fd < 0) {
		EPRINTLN("Error opening bootrom \"%s\": %s",
		    romfile, strerror(errno));
		goto done;
	}

	if (fstat(fd, &sbuf) < 0) {
		EPRINTLN("Could not fstat bootrom file \"%s\": %s", romfile,
		    strerror(errno));
		goto done;
	}

	rom_size = sbuf.st_size;

	varfile = get_config_value_node(nvl, "bootvars");
	var_size = 0;
	if (varfile != NULL) {
		varfd = open(varfile, O_RDWR);
		if (varfd < 0) {
			fprintf(stderr, "Error opening bootrom variable file "
			    "\"%s\": %s\n", varfile, strerror(errno));
			goto done;
		}

		if (fstat(varfd, &sbuf) < 0) {
			fprintf(stderr,
			    "Could not fstat bootrom variable file \"%s\": %s\n",
			    varfile, strerror(errno));
			goto done;
		}

		var_size = sbuf.st_size;
	}

	if (var_size > BOOTROM_SIZE ||
	    (var_size != 0 && var_size < PAGE_SIZE)) {
		fprintf(stderr, "Invalid bootrom variable size %ld\n",
		    var_size);
		goto done;
	}

	total_size = rom_size + var_size;

	if (total_size > BOOTROM_SIZE) {
		fprintf(stderr, "Invalid bootrom and variable aggregate size "
		    "%ld\n", total_size);
		goto done;
	}

	/* Map the bootrom into the guest address space */
	if (bootrom_alloc(ctx, rom_size, PROT_READ | PROT_EXEC,
	    BOOTROM_ALLOC_TOP, &ptr, NULL) != 0) {
		goto done;
	}

	/* Read 'romfile' into the guest address space */
	for (i = 0; i < rom_size / PAGE_SIZE; i++) {
		rlen = read(fd, ptr + i * PAGE_SIZE, PAGE_SIZE);
		if (rlen != PAGE_SIZE) {
			EPRINTLN("Incomplete read of page %d of bootrom "
			    "file %s: %ld bytes", i, romfile, rlen);
			goto done;
		}
	}

	if (varfd >= 0) {
		var.mmap = mmap(NULL, var_size, PROT_READ | PROT_WRITE,
		    MAP_SHARED, varfd, 0);
		if (var.mmap == MAP_FAILED)
			goto done;
		var.size = var_size;
		var.gpa = (gpa_alloctop - var_size) + 1;
		gpa_alloctop = var.gpa - 1;
		rv = register_mem(&(struct mem_range){
		    .name = "bootrom variable",
		    .flags = MEM_F_RW,
		    .handler = bootrom_var_mem_handler,
		    .base = var.gpa,
		    .size = var.size,
		});
		if (rv != 0)
			goto done;
	}

	rv = 0;
done:
	if (varfd >= 0)
		close(varfd);
	if (fd >= 0)
		close(fd);
	free(romfile);
	return (rv);
}
