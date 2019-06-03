/*
 * Copyright (c) 2019 Oleg Derevenetz <oleg.derevenetz@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice(s), this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified other than the possible
 *    addition of one or more copyright notices.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice(s), this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER(S) ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER(S) BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#include <sys/mount.h>

#include <isofs/cd9660/cd9660_mount.h>
#include <ufs/ufs/ufsmount.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "libc_private.h"

static void
conv_oexport_to_export(struct oexport_args *oexp, struct export_args *exp)
{
	memcpy(exp, oexp, sizeof(*oexp));

	exp->ex_numsecflavors = 0;
}

static void
add_to_iovec(struct iovec **iov, size_t *iov_size, const char *name, void *value, size_t value_size)
{
	void         *tmp_str;
	struct iovec *tmp_iov;

	tmp_iov = realloc(*iov, sizeof(**iov) * (*iov_size + 2));

	if (tmp_iov != NULL) {
		*iov = tmp_iov;

		tmp_str = strdup(name);

		(*iov)[*iov_size].iov_base = tmp_str;

		if (tmp_str != NULL) {
			(*iov)[*iov_size].iov_len = strlen(tmp_str) + 1;
		} else {
			(*iov)[*iov_size].iov_len = 0;
		}

		(*iov_size)++;

		(*iov)[*iov_size].iov_base = value;

		if (value != NULL) {
			if (value_size > 0) {
				(*iov)[*iov_size].iov_len = value_size;
			} else {
				(*iov)[*iov_size].iov_len = strlen(value) + 1;
			}
		} else {
			(*iov)[*iov_size].iov_len = 0;
		}

		(*iov_size)++;
	}
}

static void
free_iovec(struct iovec **iov, size_t *iov_size)
{
	size_t i;

	for (i = 0; i < *iov_size; i += 2) {
		free((*iov)[i].iov_base);
	}

	free(*iov);

	*iov      = NULL;
	*iov_size = 0;
}

static void
make_iovec_for_ufs(struct iovec **iov, size_t *iov_size, void *data)
{
	struct ufs_args   *args;
	struct export_args exp;

	if (data != NULL) {
		args = data;

		conv_oexport_to_export(&args->export, &exp);

		add_to_iovec(iov, iov_size, "from",   args->fspec, 0);
		add_to_iovec(iov, iov_size, "export", &exp,        sizeof(exp));
	}
}

static void
make_iovec_for_cd9660(struct iovec **iov, size_t *iov_size, void *data)
{
	char               ssector_str[64];
	struct iso_args   *args;
	struct export_args exp;

	if (data != NULL) {
		args = data;

		snprintf(ssector_str, sizeof(ssector_str), "%d", args->ssector);

		conv_oexport_to_export(&args->export, &exp);

		add_to_iovec(iov, iov_size, "from",     args->fspec,    0);
		add_to_iovec(iov, iov_size, "export",   &exp,           sizeof(exp));
		add_to_iovec(iov, iov_size, "ssector",  ssector_str,    0);
		add_to_iovec(iov, iov_size, "cs_disk",  args->cs_disk,  0);
		add_to_iovec(iov, iov_size, "cs_local", args->cs_local, 0);

		if (args->flags & ISOFSMNT_NORRIP) {
			add_to_iovec(iov, iov_size, "norrip", NULL, 0);
		}
		if (!(args->flags & ISOFSMNT_GENS)) {
			add_to_iovec(iov, iov_size, "nogens", NULL, 0);
		}
		if (!(args->flags & ISOFSMNT_EXTATT)) {
			add_to_iovec(iov, iov_size, "noextatt", NULL, 0);
		}
		if (args->flags & ISOFSMNT_NOJOLIET) {
			add_to_iovec(iov, iov_size, "nojoliet", NULL, 0);
		}
		if (!(args->flags & ISOFSMNT_BROKENJOLIET)) {
			add_to_iovec(iov, iov_size, "nobrokenjoliet", NULL, 0);
		}
		if (!(args->flags & ISOFSMNT_KICONV)) {
			add_to_iovec(iov, iov_size, "nokiconv", NULL, 0);
		}
	}
}

__weak_reference(__sys_mount, __mount);

#pragma weak mount
int
mount(const char *type, const char *dir, int flags, void *data)
{
	int           result;
	size_t        iov_size;
	struct iovec *iov;

	fprintf(stderr, "WRAPPER CALLED\n");

	iov      = NULL;
	iov_size = 0;

	add_to_iovec(&iov, &iov_size, "fstype", (void*)type, 0);
	add_to_iovec(&iov, &iov_size, "fspath", (void*)dir,  0);

	if (strcmp(type, "ufs") == 0) {
		make_iovec_for_ufs(&iov, &iov_size, data);
	} else if (strcmp(type, "cd9660") == 0) {
		make_iovec_for_cd9660(&iov, &iov_size, data);
	}

	result = nmount(iov, iov_size, flags);

	free_iovec(&iov, &iov_size);

	return result;
}
