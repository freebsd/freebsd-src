/*-
 * Copyright (c) 2012 Andriy Gapon <avg@FreeBSD.org>.
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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/ioccom.h>
#include <sys/cpuctl.h>

#include <machine/cpufunc.h>
#include <machine/specialreg.h>

#include <x86/ucode.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <err.h>

#include "cpucontrol.h"
#include "amd.h"

int
amd10h_probe(int fd)
{
	char vendor[13];
	cpuctl_cpuid_args_t idargs;
	uint32_t family;
	uint32_t signature;
	int error;

	idargs.level = 0;
	error = ioctl(fd, CPUCTL_CPUID, &idargs);
	if (error < 0) {
		WARN(0, "ioctl()");
		return (1);
	}
	((uint32_t *)vendor)[0] = idargs.data[1];
	((uint32_t *)vendor)[1] = idargs.data[3];
	((uint32_t *)vendor)[2] = idargs.data[2];
	vendor[12] = '\0';
	if (strncmp(vendor, AMD_VENDOR_ID, sizeof(AMD_VENDOR_ID)) != 0)
		return (1);

	idargs.level = 1;
	error = ioctl(fd, CPUCTL_CPUID, &idargs);
	if (error < 0) {
		WARN(0, "ioctl()");
		return (1);
	}
	signature = idargs.data[0];
	family = ((signature >> 8) & 0x0f) + ((signature >> 20) & 0xff);
	if (family < 0x10)
		return (1);
	return (0);
}

void
amd10h_update(const struct ucode_update_params *params)
{
	cpuctl_cpuid_args_t idargs;
	cpuctl_msr_args_t msrargs;
	cpuctl_update_args_t args;
	const uint8_t *fw_image;
	const char *dev, *path;
	const void *selected_fw;
	size_t fw_size;
	size_t selected_size;
	uint32_t revision;
	uint32_t new_rev;
	uint32_t signature;
	int devfd;
	int error;

	dev = params->dev_path;
	path = params->fw_path;
	devfd = params->devfd;
	fw_image = params->fwimage;
	fw_size = params->fwsize;

	assert(path);
	assert(dev);

	idargs.level = 1;
	error = ioctl(devfd, CPUCTL_CPUID, &idargs);
	if (error < 0) {
		WARN(0, "ioctl()");
		goto done;
	}
	signature = idargs.data[0];

	msrargs.msr = MSR_BIOS_SIGN;
	error = ioctl(devfd, CPUCTL_RDMSR, &msrargs);
	if (error < 0) {
		WARN(0, "ioctl(%s)", dev);
		goto done;
	}
	revision = (uint32_t)msrargs.data;

	selected_fw = ucode_amd_find(path, signature, revision, fw_image,
	    fw_size, &selected_size);

	if (selected_fw != NULL) {
		WARNX(1, "selected ucode size is %zu", selected_size);
		fprintf(stderr, "%s: updating cpu %s to revision %#x... ",
		    path, dev, revision);

		args.data = __DECONST(void *, selected_fw);
		args.size = selected_size;
		error = ioctl(devfd, CPUCTL_UPDATE, &args);
		if (error < 0) {
			fprintf(stderr, "failed.\n");
			warn("ioctl()");
			goto done;
		}
		fprintf(stderr, "done.\n");
	}

	msrargs.msr = MSR_BIOS_SIGN;
	error = ioctl(devfd, CPUCTL_RDMSR, &msrargs);
	if (error < 0) {
		WARN(0, "ioctl(%s)", dev);
		goto done;
	}
	new_rev = (uint32_t)msrargs.data;
	if (new_rev != revision)
		WARNX(0, "revision after update %#x", new_rev);

done:
	return;
}
