/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
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
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <machine/vmm.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

#include <vmmapi.h>
#include "bhyverun.h"
#include "bootrom.h"
#include "debug.h"

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
bootrom_loadrom(struct vmctx *ctx, const char *romfile)
{
	struct stat sbuf;
	ssize_t rlen;
	char *ptr;
	int fd, i, rv;

	rv = -1;
	fd = open(romfile, O_RDONLY);
	if (fd < 0) {
		EPRINTLN("Error opening bootrom \"%s\": %s",
		    romfile, strerror(errno));
		goto done;
	}

        if (fstat(fd, &sbuf) < 0) {
		EPRINTLN("Could not fstat bootrom file \"%s\": %s",
		    romfile, strerror(errno));
		goto done;
        }

	/* Map the bootrom into the guest address space */
	if (bootrom_alloc(ctx, sbuf.st_size, PROT_READ | PROT_EXEC,
	    BOOTROM_ALLOC_TOP, &ptr, NULL) != 0)
		goto done;

	/* Read 'romfile' into the guest address space */
	for (i = 0; i < sbuf.st_size / PAGE_SIZE; i++) {
		rlen = read(fd, ptr + i * PAGE_SIZE, PAGE_SIZE);
		if (rlen != PAGE_SIZE) {
			EPRINTLN("Incomplete read of page %d of bootrom "
			    "file %s: %ld bytes", i, romfile, rlen);
			goto done;
		}
	}
	rv = 0;
done:
	if (fd >= 0)
		close(fd);
	return (rv);
}
