/*-
 * Copyright (C) 2014 Nathan Whitehorn
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
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <fdt_platform.h>
#include <libfdt.h>
#include "kboot.h"

/* Fix up wrong values added to the device tree by prom_init() in Linux */

void
fdt_arch_fixups(void *fdtp)
{
	int offset, len;
	const void *prop;

	/*
	 * Remove /memory/available properties, which reflect long-gone OF
	 * state
	 */

	offset = fdt_path_offset(fdtp, "/memory@0");
	if (offset > 0)
		fdt_delprop(fdtp, offset, "available");

	/*
	 * Add reservations for OPAL and RTAS state if present
	 */

	offset = fdt_path_offset(fdtp, "/ibm,opal");
	if (offset > 0) {
		const uint64_t *base, *size;
		base = fdt_getprop(fdtp, offset, "opal-base-address",
		    &len);
		size = fdt_getprop(fdtp, offset, "opal-runtime-size",
		    &len);
		if (base != NULL && size != NULL)
			fdt_add_mem_rsv(fdtp, fdt64_to_cpu(*base),
			    fdt64_to_cpu(*size));
	}
	offset = fdt_path_offset(fdtp, "/rtas");
	if (offset > 0) {
		const uint32_t *base, *size;
		base = fdt_getprop(fdtp, offset, "linux,rtas-base", &len);
		size = fdt_getprop(fdtp, offset, "rtas-size", &len);
		if (base != NULL && size != NULL)
			fdt_add_mem_rsv(fdtp, fdt32_to_cpu(*base),
			    fdt32_to_cpu(*size));
	}

	/*
	 * Patch up /chosen nodes so that the stored handles mean something,
	 * where possible.
	 */
	offset = fdt_path_offset(fdtp, "/chosen");
	if (offset > 0) {
		fdt_delprop(fdtp, offset, "cpu"); /* This node not meaningful */

		offset = fdt_path_offset(fdtp, "/chosen");
		prop = fdt_getprop(fdtp, offset, "linux,stdout-package", &len);
		if (prop != NULL) {
			fdt_setprop(fdtp, offset, "stdout", prop, len);
			offset = fdt_path_offset(fdtp, "/chosen");
			fdt_setprop(fdtp, offset, "stdin", prop, len);
		}
	}
}
