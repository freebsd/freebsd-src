/*-
 * Copyright (c) 2013 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Benno Rice under sponsorship from
 * the FreeBSD Foundation.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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

#include <bootstrap.h>
#include <stand.h>

#include <efi.h>
#include <efilib.h>
#include <machine/metadata.h>

static EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;

int
efi_find_framebuffer(struct efi_fb *efifb)
{
	EFI_GRAPHICS_OUTPUT			*gop;
	EFI_STATUS				status;
	EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE	*mode;
	EFI_GRAPHICS_OUTPUT_MODE_INFORMATION	*info;

	status = BS->LocateProtocol(&gop_guid, NULL, (VOID **)&gop);
	if (EFI_ERROR(status))
		return (1);

	mode = gop->Mode;
	info = gop->Mode->Info;

	efifb->fb_addr = mode->FrameBufferBase;
	efifb->fb_size = mode->FrameBufferSize;
	efifb->fb_height = info->VerticalResolution;
	efifb->fb_width = info->HorizontalResolution;
	efifb->fb_stride = info->PixelsPerScanLine;

	switch (info->PixelFormat) {
	case PixelRedGreenBlueReserved8BitPerColor:
		efifb->fb_mask_red = 0x000000ff;
		efifb->fb_mask_green = 0x0000ff00;
		efifb->fb_mask_blue = 0x00ff0000;
		efifb->fb_mask_reserved = 0xff000000;
		break;
	case PixelBlueGreenRedReserved8BitPerColor:
		efifb->fb_mask_red = 0x00ff0000;
		efifb->fb_mask_green = 0x0000ff00;
		efifb->fb_mask_blue = 0x000000ff;
		efifb->fb_mask_reserved = 0xff000000;
		break;
	case PixelBitMask:
		efifb->fb_mask_red = info->PixelInformation.RedMask;
		efifb->fb_mask_green = info->PixelInformation.GreenMask;
		efifb->fb_mask_blue = info->PixelInformation.BlueMask;
		efifb->fb_mask_reserved =
		    info->PixelInformation.ReservedMask;
		break;
	default:
		return (1);
	}
	return (0);
}

COMMAND_SET(gop, "gop", "graphics output protocol", command_gop);

static void
command_gop_display(u_int mode, EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info)
{

	printf("mode %u: %ux%u, stride=%u, color=", mode,
	    info->HorizontalResolution, info->VerticalResolution,
	    info->PixelsPerScanLine);
	switch (info->PixelFormat) {
	case PixelRedGreenBlueReserved8BitPerColor:
		printf("32-bit (RGB)");
		break;
	case PixelBlueGreenRedReserved8BitPerColor:
		printf("32-bit (BGR)");
		break;
	case PixelBitMask:
		printf("mask (R=%x, G=%x, B=%x, X=%x)",
		    info->PixelInformation.RedMask,
		    info->PixelInformation.GreenMask,
		    info->PixelInformation.BlueMask,
		    info->PixelInformation.ReservedMask);
		break;
	case PixelBltOnly:
		printf("unsupported (blt only)");
		break;
	default:
		printf("unsupported (unknown)");
		break;
	}
}

static int
command_gop(int argc, char *argv[])
{
	EFI_GRAPHICS_OUTPUT *gop;
	EFI_STATUS status;
	u_int mode;

	status = BS->LocateProtocol(&gop_guid, NULL, (VOID **)&gop);
	if (EFI_ERROR(status)) {
		sprintf(command_errbuf, "%s: Graphics Output Protocol not "
		    "present (error=%lu)", argv[0], status & ~EFI_ERROR_MASK);
		return (CMD_ERROR);
	}

	if (argc == 1)
		goto usage;

	if (!strcmp(argv[1], "set")) {
		char *cp;

		if (argc != 3)
			goto usage;
		mode = strtol(argv[2], &cp, 0);
		if (cp[0] != '\0') {
			sprintf(command_errbuf, "mode is an integer");
			return (CMD_ERROR);
		}
		status = gop->SetMode(gop, mode);
		if (EFI_ERROR(status)) {
			sprintf(command_errbuf, "%s: Unable to set mode to "
			    "%u (error=%lu)", argv[0], mode,
			    status & ~EFI_ERROR_MASK);
			return (CMD_ERROR);
		}
	} else if (!strcmp(argv[1], "get")) {
		command_gop_display(gop->Mode->Mode, gop->Mode->Info);
		printf("\n    frame buffer: address=%jx, size=%lx\n",
		    (uintmax_t)gop->Mode->FrameBufferBase,
		    gop->Mode->FrameBufferSize);
	} else if (!strcmp(argv[1], "list")) {
		EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info;
		UINTN infosz;

		pager_open();
		for (mode = 0; mode < gop->Mode->MaxMode; mode++) {
			status = gop->QueryMode(gop, mode, &infosz, &info);
			if (EFI_ERROR(status))
				continue;
			command_gop_display(mode, info);
			if (pager_output("\n"))
				break;
		}
		pager_close();
	}
	return (CMD_OK);

 usage:
	sprintf(command_errbuf, "usage: %s [list | get | set <mode>]",
	    argv[0]);
	return (CMD_ERROR);
}
