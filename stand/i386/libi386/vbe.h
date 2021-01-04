/*-
 * Copyright (c) 2009 Jared D. McNeill <jmcneill@invisible.ca>
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Default mode for VESA frame buffer.
 * This mode is selected when there is no EDID inormation and
 * mode is not provided by user.
 * To provide consistent look with UEFI GOP, we use 800x600 here,
 * and if this mode is not available, we fall back to text mode and
 * VESA disabled.
 */

#define VBE_DEFAULT_MODE	"800x600"

struct vbeinfoblock {
	char VbeSignature[4];
	uint16_t VbeVersion;
	uint32_t OemStringPtr;
	uint32_t Capabilities;
#define	VBE_CAP_DAC8	(1 << 0)	/* Can switch DAC */
#define	VBE_CAP_NONVGA	(1 << 1)	/* Controller is not VGA comp. */
#define	VBE_CAP_SNOW	(1 << 2)	/* Set data during Vertical Reterace */
	uint32_t VideoModePtr;
	uint16_t TotalMemory;
	uint16_t OemSoftwareRev;
	uint32_t OemVendorNamePtr, OemProductNamePtr, OemProductRevPtr;
	/* data area, in total max 512 bytes for VBE 2.0 */
	uint8_t Reserved[222];
	uint8_t OemData[256];
} __packed;

struct modeinfoblock {
	/* Mandatory information for all VBE revisions */
	uint16_t ModeAttributes;
	uint8_t WinAAttributes, WinBAttributes;
	uint16_t WinGranularity, WinSize, WinASegment, WinBSegment;
	uint32_t WinFuncPtr;
	uint16_t BytesPerScanLine;
	/* Mandatory information for VBE 1.2 and above */
	uint16_t XResolution, YResolution;
	uint8_t XCharSize, YCharSize, NumberOfPlanes, BitsPerPixel;
	uint8_t NumberOfBanks, MemoryModel, BankSize, NumberOfImagePages;
	uint8_t Reserved1;
	/* Direct Color fields
	   (required for direct/6 and YUV/7 memory models) */
	uint8_t RedMaskSize, RedFieldPosition;
	uint8_t GreenMaskSize, GreenFieldPosition;
	uint8_t BlueMaskSize, BlueFieldPosition;
	uint8_t RsvdMaskSize, RsvdFieldPosition;
	uint8_t DirectColorModeInfo;
	/* Mandatory information for VBE 2.0 and above */
	uint32_t PhysBasePtr;
	uint32_t OffScreenMemOffset;	/* reserved in VBE 3.0 and above */
	uint16_t OffScreenMemSize;	/* reserved in VBE 3.0 and above */

	/* Mandatory information for VBE 3.0 and above */
	uint16_t LinBytesPerScanLine;
	uint8_t BnkNumberOfImagePages;
	uint8_t LinNumberOfImagePages;
	uint8_t LinRedMaskSize, LinRedFieldPosition;
	uint8_t LinGreenMaskSize, LinGreenFieldPosition;
	uint8_t LinBlueMaskSize, LinBlueFieldPosition;
	uint8_t LinRsvdMaskSize, LinRsvdFieldPosition;
	uint32_t MaxPixelClock;
	/* + 1 will fix the size to 256 bytes */
	uint8_t Reserved4[189 + 1];
} __packed;

struct crtciinfoblock {
	uint16_t HorizontalTotal;
	uint16_t HorizontalSyncStart;
	uint16_t HorizontalSyncEnd;
	uint16_t VerticalTotal;
	uint16_t VerticalSyncStart;
	uint16_t VerticalSyncEnd;
	uint8_t Flags;
	uint32_t PixelClock;
	uint16_t RefreshRate;
	uint8_t Reserved[40];
} __packed;

struct paletteentry {
	uint8_t Blue;
	uint8_t Green;
	uint8_t Red;
	uint8_t Reserved;
} __packed;

struct flatpanelinfo
{
	uint16_t HorizontalSize;
	uint16_t VerticalSize;
	uint16_t PanelType;
	uint8_t RedBPP;
	uint8_t GreenBPP;
	uint8_t BlueBPP;
	uint8_t ReservedBPP;
	uint32_t ReservedOffScreenMemSize;
	uint32_t ReservedOffScreenMemPtr;

	uint8_t Reserved[14];
} __packed;

#define	VBE_BASE_MODE		(0x100)		/* VBE 3.0 page 18 */
#define	VBE_VALID_MODE(a)	((a) >= VBE_BASE_MODE)
#define	VBE_ERROR(a)		(((a) & 0xFF) != 0x4F || ((a) & 0xFF00) != 0)
#define	VBE_SUCCESS		(0x004F)
#define	VBE_FAILED		(0x014F)
#define	VBE_NOTSUP		(0x024F)
#define	VBE_INVALID		(0x034F)

#define	VGA_TEXT_MODE		(3)		/* 80x25 text mode */
#define	TEXT_ROWS		(25)		/* VGATEXT rows */
#define	TEXT_COLS		(80)		/* VGATEXT columns */

extern struct paletteentry *pe8;
extern int palette_format;

int vga_get_reg(int, int);
int vga_get_atr(int, int);
void vga_set_atr(int, int, int);
void vga_set_indexed(int, int, int, uint8_t, uint8_t);
int vga_get_indexed(int, int, int, uint8_t);
int vga_get_crtc(int, int);
void vga_set_crtc(int, int, int);
int vga_get_seq(int, int);
void vga_set_seq(int, int, int);
int vga_get_grc(int, int);
void vga_set_grc(int, int, int);

/* high-level VBE helpers, from vbe.c */
bool vbe_is_vga(void);
void bios_set_text_mode(int);
int biosvbe_palette_format(int *);
void vbe_init(void);
bool vbe_available(void);
int vbe_default_mode(void);
int vbe_set_mode(int);
int vbe_get_mode(void);
int vbe_set_palette(const struct paletteentry *, size_t);
void vbe_modelist(int);
