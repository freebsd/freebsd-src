/*-
 * Copyright (c) 2016 John Baldwin <jhb@FreeBSD.org>
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

#include <efi.h>
#include <efilib.h>

/* XXX: This belongs in an efifoo.h header. */
#define	EFI_LOADED_IMAGE_DEVICE_PATH_PROTOCOL_GUID			\
    { 0xbc62157e, 0x3e33, 0x4fec, { 0x99, 0x20, 0x2d, 0x3b, 0x36, 0xd7, 0x50, 0xdf } }

#define	EFI_DEVICE_PATH_TO_TEXT_PROTOCOL_GUID				\
    { 0x8b843e20, 0x8132, 0x4852, { 0x90, 0xcc, 0x55, 0x1a, 0x4e, 0x4a, 0x7f, 0x1c } }

INTERFACE_DECL(_EFI_DEVICE_PATH_PROTOCOL);

typedef
CHAR16*
(EFIAPI *EFI_DEVICE_PATH_TO_TEXT_NODE) (
    IN struct _EFI_DEVICE_PATH *This,
    IN BOOLEAN                 DisplayOnly,
    IN BOOLEAN                 AllowShortCuts
    );

typedef
CHAR16*
(EFIAPI *EFI_DEVICE_PATH_TO_TEXT_PATH) (
    IN struct _EFI_DEVICE_PATH *This,
    IN BOOLEAN                 DisplayOnly,
    IN BOOLEAN                 AllowShortCuts
    );

typedef struct _EFI_DEVICE_PATH_TO_TEXT_PROTOCOL {
	EFI_DEVICE_PATH_TO_TEXT_NODE ConvertDeviceNodeToText;
	EFI_DEVICE_PATH_TO_TEXT_PATH ConvertDevicePathToText;
} EFI_DEVICE_PATH_TO_TEXT_PROTOCOL;

static EFI_GUID ImageDevicePathGUID =
    EFI_LOADED_IMAGE_DEVICE_PATH_PROTOCOL_GUID;
static EFI_GUID DevicePathGUID = DEVICE_PATH_PROTOCOL;
static EFI_GUID DevicePathToTextGUID = EFI_DEVICE_PATH_TO_TEXT_PROTOCOL_GUID;
static EFI_DEVICE_PATH_TO_TEXT_PROTOCOL *textProtocol;

EFI_DEVICE_PATH *
efi_lookup_image_devpath(EFI_HANDLE handle)
{
	EFI_DEVICE_PATH *devpath;
	EFI_STATUS status;

	status = BS->HandleProtocol(handle, &ImageDevicePathGUID,
	    (VOID **)&devpath);
	if (EFI_ERROR(status))
		devpath = NULL;
	return (devpath);
}

EFI_DEVICE_PATH *
efi_lookup_devpath(EFI_HANDLE handle)
{
	EFI_DEVICE_PATH *devpath;
	EFI_STATUS status;

	status = BS->HandleProtocol(handle, &DevicePathGUID, (VOID **)&devpath);
	if (EFI_ERROR(status))
		devpath = NULL;
	return (devpath);
}

CHAR16 *
efi_devpath_name(EFI_DEVICE_PATH *devpath)
{
	static int once = 1;
	EFI_STATUS status;

	if (devpath == NULL)
		return (NULL);
	if (once) {
		status = BS->LocateProtocol(&DevicePathToTextGUID, NULL,
		    (VOID **)&textProtocol);
		if (EFI_ERROR(status))
			textProtocol = NULL;
		once = 0;
	}
	if (textProtocol == NULL)
		return (NULL);

	return (textProtocol->ConvertDevicePathToText(devpath, TRUE, TRUE));
}

void
efi_free_devpath_name(CHAR16 *text)
{

	BS->FreePool(text);
}

EFI_DEVICE_PATH *
efi_devpath_last_node(EFI_DEVICE_PATH *devpath)
{

	if (IsDevicePathEnd(devpath))
		return (NULL);
	while (!IsDevicePathEnd(NextDevicePathNode(devpath)))
		devpath = NextDevicePathNode(devpath);
	return (devpath);
}

EFI_DEVICE_PATH *
efi_devpath_trim(EFI_DEVICE_PATH *devpath)
{
	EFI_DEVICE_PATH *node, *copy;
	size_t prefix, len;

	node = efi_devpath_last_node(devpath);
	prefix = (UINT8 *)node - (UINT8 *)devpath;
	if (prefix == 0)
		return (NULL);
	len = prefix + DevicePathNodeLength(NextDevicePathNode(node));
	copy = malloc(len);
	memcpy(copy, devpath, prefix);
	node = (EFI_DEVICE_PATH *)((UINT8 *)copy + prefix);
	SetDevicePathEndNode(node);
	return (copy);
}

EFI_HANDLE
efi_devpath_handle(EFI_DEVICE_PATH *devpath)
{
	EFI_STATUS status;
	EFI_HANDLE h;

	/*
	 * There isn't a standard way to locate a handle for a given
	 * device path.  However, querying the EFI_DEVICE_PATH protocol
	 * for a given device path should give us a handle for the
	 * closest node in the path to the end that is valid.
	 */
	status = BS->LocateDevicePath(&DevicePathGUID, &devpath, &h);
	if (EFI_ERROR(status))
		return (NULL);
	return (h);
}
