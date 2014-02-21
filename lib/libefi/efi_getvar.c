/*-
 * Copyright (c) 2010 Marcel Moolenaar
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

#include <libefi.h>
#include <stdlib.h>

#include "libefi_int.h"

/*
 * EFI_STATUS
 * GetVariable(
 *	IN CHAR16	*VariableName,
 *	IN EFI_GUID	*VendorGuid,
 *	OUT UINT32	*Attributes OPTIONAL,
 *	IN OUT UINTN	*DataSize,
 *	OUT VOID	*Data
 *   );
 */

int
efi_getvar(char *name, uuid_t *vendor, uint32_t *attrib, size_t *datasize,
    void *data)
{
	struct iodev_efivar_req req;
	int error;

	req.namesize = 0;
	error = libefi_utf8_to_ucs2(name, &req.namesize, &req.name);
	if (error)
		return (error);

	req.vendor = *vendor;
	req.datasize = *datasize;
	req.data = data;
	req.access = IODEV_EFIVAR_GETVAR;
	error = libefi_efivar(&req);
	*datasize = req.datasize;
	if (!error && attrib != NULL)
		*attrib = req.attrib;
	free(req.name);
	return (error);
}
