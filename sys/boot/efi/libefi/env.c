/*
 * Copyright (c) 2015 Netflix, Inc. All Rights Reserved.
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

#include <efi.h>
#include <efilib.h>

/*
 * Simple wrappers to the underlying UEFI functions.
 * See http://wiki.phoenix.com/wiki/index.php/EFI_RUNTIME_SERVICES
 * for details.
 */
EFI_STATUS
efi_get_next_variable_name(UINTN *variable_name_size, CHAR16 *variable_name, EFI_GUID *vendor_guid)
{
	return RS->GetNextVariableName(variable_name_size, variable_name, vendor_guid);
}

EFI_STATUS
efi_get_variable(CHAR16 *variable_name, EFI_GUID *vendor_guid, UINT32 *attributes, UINTN *data_size,
    void *data)
{
	return RS->GetVariable(variable_name, vendor_guid, attributes, data_size, data);
}

EFI_STATUS
efi_set_variable(CHAR16 *variable_name, EFI_GUID *vendor_guid, UINT32 attributes, UINTN data_size,
    void *data)
{
	return RS->SetVariable(variable_name, vendor_guid, attributes, data_size, data);
}
