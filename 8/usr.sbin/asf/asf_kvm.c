/*-
 * Copyright (c) 2006 The FreeBSD Project
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/queue.h>	/* for <sys/linker.h> with _KERNEL defined */
#include <err.h>
#include <fcntl.h>
#include <kvm.h>
#include <limits.h>
#include <nlist.h>
#include <stdlib.h>
#include <string.h>

#define _KERNEL
#include <sys/linker.h>
#undef	_KERNEL

#include "asf.h"

/* Name of the head of the linker file list in /sys/kern/kern_linker.c */
#define LINKER_HEAD	"linker_files"

/*
 * Get the list of linker files using kvm(3).
 * Can work with a live kernel as well as with a crash dump.
 */
void
asf_kvm(const char *kernfile, const char *corefile)
{
	char errbuf[LINE_MAX];
	char name[PATH_MAX];
	kvm_t *kd;
	struct nlist nl[2];
	struct linker_file lf;
	linker_file_list_t linker_files;
	ssize_t n;
	void *kp;

	kd = kvm_openfiles(kernfile, corefile, NULL, O_RDONLY, errbuf);
	if (kd == NULL)
		errx(2, "open kernel memory: %s", errbuf);

	/*
	 * Locate the head of the linker file list using kernel symbols.
	 */
	strcpy(name, LINKER_HEAD);
	nl[0].n_name = name; /* can't use LINKER_HEAD here because it's const */
	nl[1].n_name = NULL; /* terminate the array for kvm_nlist() */
	switch (kvm_nlist(kd, nl)) {
	case 0:
		break;
	case -1:
		warnx("%s: %s", LINKER_HEAD, kvm_geterr(kd));
		kvm_close(kd);
		exit(2);
	default:
		kvm_close(kd);
		errx(2, "%s: symbol not found", LINKER_HEAD);
	}

	/*
	 * Read the head of the linker file list from kernel memory.
	 */
	n = kvm_read(kd, nl[0].n_value, &linker_files, sizeof(linker_files));
	if (n == -1)
		goto read_err;
	if (n != sizeof(linker_files)) {
		kvm_close(kd);
		errx(2, "%s: short read", LINKER_HEAD);
	}

	/*
	 * Traverse the linker file list starting at its head.
	 */
	for (kp = linker_files.tqh_first; kp; kp = lf.link.tqe_next) {
		/* Read a linker file structure */
		n = kvm_read(kd, (u_long)kp, &lf, sizeof(lf));
		if (n == -1)
			goto read_err;
		if (n != sizeof(lf)) {
			kvm_close(kd);
			errx(2, "kvm: short read");
		}
		/* Read the name of the file stored separately */
		bzero(name, sizeof(name));
		n = kvm_read(kd, (u_long)lf.filename, name, sizeof(name) - 1);
		if (n == -1)
			goto read_err;
		if (strcmp(name, KERNFILE) == 0)
			continue;
		/* Add this file to our list of linker files */
		kfile_add(name, lf.address);
	}
	kvm_close(kd);
	return;

read_err:	/* A common error case */
	warnx("read kernel memory: %s", kvm_geterr(kd));
	kvm_close(kd);
	exit(2);
}
