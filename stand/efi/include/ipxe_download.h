/*
 * Copyright (c) 2026 Netflix, Inc. Written by Warner Losh
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * IPXE protocol to download files
 *
 * Written from https://dox.ipxe.org/efi__download_8h.html using the names
 * contained there for compatibility. See that for the full docs. Provides the
 * same interface as <ipxe/efi/efi_download.h> from the ipxe distribution.
 */
#pragma once

typedef struct _IPXE_DOWNLOAD_PROTOCOL IPXE_DOWNLOAD_PROTOCOL;
typedef void *IPXE_DOWNLOAD_FILE;
typedef EFI_STATUS(EFIAPI *IPXE_DOWNLOAD_DATA_CALLBACK)(IN VOID *, IN VOID *, IN UINTN, IN UINTN);
typedef void(EFIAPI *IPXE_DOWNLOAD_FINISH_CALLBACK)(IN VOID *, IN EFI_STATUS);
typedef EFI_STATUS(EFIAPI *IPXE_DOWNLOAD_START)(IN IPXE_DOWNLOAD_PROTOCOL *,
    IN CHAR8 *, IN IPXE_DOWNLOAD_DATA_CALLBACK, IN IPXE_DOWNLOAD_FINISH_CALLBACK,
    IN VOID *, OUT IPXE_DOWNLOAD_FILE *);
typedef EFI_STATUS(EFIAPI *IPXE_DOWNLOAD_ABORT)(IN IPXE_DOWNLOAD_PROTOCOL *,
    IN IPXE_DOWNLOAD_FILE, IN EFI_STATUS);
typedef EFI_STATUS(EFIAPI *IPXE_DOWNLOAD_POLL) (IN IPXE_DOWNLOAD_PROTOCOL *);

struct _IPXE_DOWNLOAD_PROTOCOL {
	IPXE_DOWNLOAD_START 	Start;
	IPXE_DOWNLOAD_ABORT 	Abort;
	IPXE_DOWNLOAD_POLL 	Poll;
};
#define IPXE_DOWNLOAD_PROTOCOL_GUID \
	{ 0x3eaeaebd, 0xdecf, 0x493b, { 0x9b, 0xd1, 0xcd, 0xb2, 0xde, 0xca, 0xe7, 0x19 } }
