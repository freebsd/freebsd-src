/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2006, 2008 Stanislav Sedov <stas@FreeBSD.org>.
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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <err.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/ioccom.h>
#include <sys/cpuctl.h>

#include <machine/cpufunc.h>
#include <machine/specialreg.h>

#include "cpucontrol.h"
#include "intel.h"

#define	DEFAULT_UCODE_SIZE	2000 /* Size of update data if not specified. */

int
intel_probe(int fd)
{
	char vendor[13];
	int error;
	cpuctl_cpuid_args_t idargs = {
		.level  = 0,
	};

	error = ioctl(fd, CPUCTL_CPUID, &idargs);
	if (error < 0) {
		WARN(0, "ioctl()");
		return (1);
	}
	((uint32_t *)vendor)[0] = idargs.data[1];
	((uint32_t *)vendor)[1] = idargs.data[3];
	((uint32_t *)vendor)[2] = idargs.data[2];
	vendor[12] = '\0';
	if (strncmp(vendor, INTEL_VENDOR_ID, sizeof(INTEL_VENDOR_ID)) != 0)
		return (1);
	return (0);
}

void
intel_update(const char *dev, const char *path)
{
	int fd, devfd;
	struct stat st;
	uint32_t *fw_image;
	int have_ext_table;
	uint32_t sum;
	unsigned int i;
	size_t payload_size;
	intel_fw_header_t *fw_header;
	intel_cpu_signature_t *ext_table;
	intel_ext_header_t *ext_header;
	uint32_t sig, signature, flags;
	int32_t revision;
	ssize_t ext_size;
	size_t ext_table_size;
	void *fw_data;
	size_t data_size, total_size;
	cpuctl_msr_args_t msrargs = {
		.msr = MSR_BIOS_SIGN,
		.data = 0,
	};
	cpuctl_cpuid_args_t idargs = {
		.level  = 1,	/* Signature. */
	};
	cpuctl_update_args_t args;
	int error;

	assert(path);
	assert(dev);

	fd = -1;
	fw_image = MAP_FAILED;
	ext_table = NULL;
	ext_header = NULL;
	devfd = open(dev, O_RDWR);
	if (devfd < 0) {
		WARN(0, "could not open %s for writing", dev);
		return;
	}
	error = ioctl(devfd, CPUCTL_WRMSR, &msrargs);
	if (error < 0) {
		WARN(0, "ioctl(%s)", dev);
		goto fail;
	}
	error = ioctl(devfd, CPUCTL_CPUID, &idargs);
	if (error < 0) {
		WARN(0, "ioctl(%s)", dev);
		goto fail;
	}
	signature = idargs.data[0];
	msrargs.msr = MSR_IA32_PLATFORM_ID;
	error = ioctl(devfd, CPUCTL_RDMSR, &msrargs);
	if (error < 0) {
		WARN(0, "ioctl(%s)", dev);
		goto fail;
	}

	/*
	 * MSR_IA32_PLATFORM_ID contains flag in BCD in bits 52-50.
	 */
	flags = 1 << ((msrargs.data >> 50) & 7);
	msrargs.msr = MSR_BIOS_SIGN;
	error = ioctl(devfd, CPUCTL_RDMSR, &msrargs);
	if (error < 0) {
		WARN(0, "ioctl(%s)", dev);
		goto fail;
	}
	revision = msrargs.data >> 32; /* Revision in the high dword. */
	WARNX(2, "found cpu type %#x family %#x model %#x stepping %#x.",
	    (signature >> 12) & 0x03, (signature >> 8) & 0x0f,
	    (signature >> 4) & 0x0f, (signature >> 0) & 0x0f);
	/*
	 * Open firmware image.
	 */
	fd = open(path, O_RDONLY, 0);
	if (fd < 0) {
		WARN(0, "open(%s)", path);
		goto fail;
	}
	error = fstat(fd, &st);
	if (error != 0) {
		WARN(0, "fstat(%s)", path);
		goto fail;
	}
	if (st.st_size < 0 || (unsigned)st.st_size < sizeof(*fw_header)) {
		WARNX(2, "file too short: %s", path);
		goto fail;
	}

	/*
	 * mmap the whole image.
	 */
	fw_image = (uint32_t *)mmap(NULL, st.st_size, PROT_READ,
	    MAP_PRIVATE, fd, 0);
	if  (fw_image == MAP_FAILED) {
		WARN(0, "mmap(%s)", path);
		goto fail;
	}
	fw_header = (intel_fw_header_t *)fw_image;
	if (fw_header->header_version != INTEL_HEADER_VERSION ||
	    fw_header->loader_revision != INTEL_LOADER_REVISION) {
		WARNX(2, "%s is not a valid intel firmware: version mismatch",
		    path);
		goto fail;
	}
	/*
	 * According to spec, if data_size == 0, then size of ucode = 2000.
	 */
	if (fw_header->data_size == 0)
		data_size = DEFAULT_UCODE_SIZE;
	else
		data_size = fw_header->data_size;
	if (fw_header->total_size == 0)
		total_size = data_size + sizeof(*fw_header);
	else
		total_size = fw_header->total_size;
	if (total_size > (unsigned)st.st_size || st.st_size < 0) {
		WARNX(2, "file too short: %s", path);
		goto fail;
	}
	payload_size = data_size + sizeof(*fw_header);

	/*
	 * Check the primary checksum.
	 */
	sum = 0;
	for (i = 0; i < (payload_size / sizeof(uint32_t)); i++)
		sum += *((uint32_t *)fw_image + i);
	if (sum != 0) {
		WARNX(2, "%s: update data checksum invalid", path);
		goto fail;
	}

	/*
	 * Check if there is an extended signature table.
	 */
	ext_size = total_size - payload_size;
	have_ext_table = 0;

	if (ext_size > (signed)sizeof(*ext_header)) {
		ext_header =
		    (intel_ext_header_t *)((char *)fw_image + payload_size);
		ext_table = (intel_cpu_signature_t *)(ext_header + 1);

		/*
		 * Check the extended table size.
		 */
		ext_table_size = sizeof(*ext_header) +
		    ext_header->sig_count * sizeof(*ext_table);
		if (ext_table_size + payload_size > total_size) {
			WARNX(2, "%s: broken extended signature table", path);
			goto no_table;
		}

		/*
		 * Check the extended table signature.
		 */
		sum = 0;
		for (i = 0; i < (ext_table_size / sizeof(uint32_t)); i++)
			sum += *((uint32_t *)ext_header + i);
		if (sum != 0) {
			WARNX(2,
			    "%s: extended signature table checksum invalid",
			    path);
			goto no_table;
		}
		have_ext_table = 1;
	}

no_table:
	fw_data = fw_header + 1; /* Pointer to the update data. */

	/*
	 * Check if the given image is ok for this cpu.
	 */
	if (signature == fw_header->cpu_signature &&
	    (flags & fw_header->cpu_flags) != 0)
		goto matched;
	else if (have_ext_table != 0) {
		for (i = 0; i < ext_header->sig_count; i++) {
			sig = ext_table[i].cpu_signature;
			if (signature == sig &&
			    (flags & ext_table[i].cpu_flags) != 0)
				goto matched;
		}
	} else
		goto fail;

matched:
	if (revision >= fw_header->revision) {
		WARNX(1, "skipping %s of rev %#x: up to date",
		    path, fw_header->revision);
		goto fail;
	}
	fprintf(stderr, "%s: updating cpu %s from rev %#x to rev %#x... ",
	    path, dev, revision, fw_header->revision);
	args.data = fw_data;
	args.size = data_size;
	error = ioctl(devfd, CPUCTL_UPDATE, &args);
	if (error < 0) {
		error = errno;
		fprintf(stderr, "failed.\n");
		errno = error;
		WARN(0, "ioctl()");
		goto fail;
	}
	fprintf(stderr, "done.\n");

fail:
	if (fw_image != MAP_FAILED)
		if (munmap(fw_image, st.st_size) != 0)
			warn("munmap(%s)", path);
	if (devfd >= 0)
		close(devfd);
	if (fd >= 0)
		close(fd);
}
