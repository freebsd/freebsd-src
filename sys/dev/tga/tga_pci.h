/*
 * Copyright (c) 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 *
 * Copyright (c) 2000 Andrew Miklic
 *
 * $FreeBSD$
 */

#ifndef _PCI_TGA_H_
#define _PCI_TGA_H_

/*
 * Device-specific PCI register offsets and contents.
 */
#define	TGA_PCIREG_PVRR	0x40		/* PCI Address Extension Register */
#define	TGA_PCIREG_PAER	0x44		/* PCI VGA Redirect Register */

/*
 * TGA Memory Space offsets
 */
#define	TGA_MEM_ALTROM	0x0000000	/* 0MB -- Alternate ROM space */
#define TGA2_MEM_EXTDEV	0x0000000	/* 0MB -- External Device Access */
#define	TGA_MEM_CREGS	0x0100000	/* 1MB -- Core Registers */
#define TGA_CREGS_SIZE	0x0100000 	/* Core registers occupy 1MB */
#define TGA_CREGS_ALIAS	0x0000400	/* Register copies every 1kB */

#define TGA2_MEM_CLOCK	0x0060000	/* TGA2 Clock access */
#define TGA2_MEM_RAMDAC	0x0080000	/* TGA2 RAMDAC access */

/*
 * TGA Core Space register numbers and contents.
 */
#define	TGA_REG_GCBR0	0x000		/* Copy buffer 0 */
#define	TGA_REG_GCBR1	0x001		/* Copy buffer 1 */
#define	TGA_REG_GCBR2	0x002		/* Copy buffer 2 */
#define	TGA_REG_GCBR3	0x003		/* Copy buffer 3 */
#define	TGA_REG_GCBR4	0x004		/* Copy buffer 4 */
#define	TGA_REG_GCBR5	0x005		/* Copy buffer 5 */
#define	TGA_REG_GCBR6	0x006		/* Copy buffer 6 */
#define	TGA_REG_GCBR7	0x007		/* Copy buffer 7 */

#define	TGA_REG_GFGR	0x008		/* Foreground */
#define	TGA_REG_GBGR	0x009		/* Background */
#define	TGA_REG_GPMR	0x00a		/* Plane Mask */
#define	TGA_REG_GPXR_S	0x00b		/* Pixel Mask (one-shot) */
#define	TGA_REG_GMOR	0x00c		/* Mode */
#define	TGA_REG_GOPR	0x00d		/* Raster Operation */
#define	TGA_REG_GPSR	0x00e		/* Pixel Shift */
#define	TGA_REG_GADR	0x00f		/* Address */

#define	TGA_REG_GB1R	0x010		/* Bresenham 1 */
#define	TGA_REG_GB2R	0x011		/* Bresenham 2 */
#define	TGA_REG_GB3R	0x012		/* Bresenham 3 */

#define	TGA_REG_GCTR	0x013		/* Continue */
#define	TGA_REG_GDER	0x014		/* Deep */
#define TGA_REG_GREV	0x015		/* Start/Version on TGA,
					 * Revision on TGA2 */
#define	TGA_REG_GSMR	0x016		/* Stencil Mode */
#define	TGA_REG_GPXR_P	0x017		/* Pixel Mask (persistent) */
#define	TGA_REG_CCBR	0x018		/* Cursor Base Address */
#define	TGA_REG_VHCR	0x019		/* Horizontal Control */
#define	TGA_REG_VVCR	0x01a		/* Vertical Control */
#define	TGA_REG_VVBR	0x01b		/* Video Base Address */
#define	TGA_REG_VVVR	0x01c		/* Video Valid */
#define	TGA_REG_CXYR	0x01d		/* Cursor XY */
#define	TGA_REG_VSAR	0x01e		/* Video Shift Address */
#define	TGA_REG_SISR	0x01f		/* Interrupt Status */
#define	TGA_REG_GDAR	0x020		/* Data */
#define	TGA_REG_GRIR	0x021		/* Red Increment */
#define	TGA_REG_GGIR	0x022		/* Green Increment */
#define	TGA_REG_GBIR	0x023		/* Blue Increment */
#define	TGA_REG_GZIR_L	0x024		/* Z-increment Low */
#define	TGA_REG_GZIR_H	0x025		/* Z-Increment High */
#define	TGA_REG_GDBR	0x026		/* DMA Base Address */
#define	TGA_REG_GBWR	0x027		/* Bresenham Width */
#define	TGA_REG_GZVR_L	0x028		/* Z-value Low */
#define	TGA_REG_GZVR_H	0x029		/* Z-value High */
#define	TGA_REG_GZBR	0x02a		/* Z-base address */
/*	GADR alias	0x02b */
#define	TGA_REG_GRVR	0x02c		/* Red Value */
#define	TGA_REG_GGVR	0x02d		/* Green Value */
#define	TGA_REG_GBVR	0x02e		/* Blue Value */
#define	TGA_REG_GSWR	0x02f		/* Span Width */
#define	TGA_REG_EPSR	0x030		/* Pallete and DAC Setup */

/*	reserved	0x031 - 0x3f */

#define	TGA_REG_GSNR0	0x040		/* Slope-no-go 0 */
#define	TGA_REG_GSNR1	0x041		/* Slope-no-go 1 */
#define	TGA_REG_GSNR2	0x042		/* Slope-no-go 2 */
#define	TGA_REG_GSNR3	0x043		/* Slope-no-go 3 */
#define	TGA_REG_GSNR4	0x044		/* Slope-no-go 4 */
#define	TGA_REG_GSNR5	0x045		/* Slope-no-go 5 */
#define	TGA_REG_GSNR6	0x046		/* Slope-no-go 6 */
#define	TGA_REG_GSNR7	0x047		/* Slope-no-go 7 */

#define	TGA_REG_GSLR0	0x048		/* Slope 0 */
#define	TGA_REG_GSLR1	0x049		/* Slope 1 */
#define	TGA_REG_GSLR2	0x04a		/* Slope 2 */
#define	TGA_REG_GSLR3	0x04b		/* Slope 3 */
#define	TGA_REG_GSLR4	0x04c		/* Slope 4 */
#define	TGA_REG_GSLR5	0x04d		/* Slope 5 */
#define	TGA_REG_GSLR6	0x04e		/* Slope 6 */
#define	TGA_REG_GSLR7	0x04f		/* Slope 7 */

#define	TGA_REG_GBCR0	0x050		/* Block Color 0 */
#define	TGA_REG_GBCR1	0x051		/* Block Color 1 */
#define	TGA_REG_GBCR2	0x052		/* Block Color 2 */
#define	TGA_REG_GBCR3	0x053		/* Block Color 3 */
#define	TGA_REG_GBCR4	0x054		/* Block Color 4 */
#define	TGA_REG_GBCR5	0x055		/* Block Color 5 */
#define	TGA_REG_GBCR6	0x056		/* Block Color 6 */
#define	TGA_REG_GBCR7	0x057		/* Block Color 7 */

#define	TGA_REG_GCSR	0x058		/* Copy 64 Source */
#define	TGA_REG_GCDR	0x059		/* Copy 64 Destination */
/*	GC[SD]R aliases 0x05a - 0x05f */

/*	reserved	0x060 - 0x077 */

#define	TGA_REG_ERWR	0x078		/* EEPROM write */

/*	reserved	0x079 */

#define	TGA_REG_ECGR	0x07a		/* Clock */

/*	reserved	0x07b */

#define	TGA_REG_EPDR	0x07c		/* Pallete and DAC Data */

/*	reserved	0x07d */

#define	TGA_REG_SCSR	0x07e		/* Command Status */

/*	reserved	0x07f */

/*
 * Deep register
 */
#define GDER_HSS	0x00010000	/* Horizontal sync select */
#define GDER_SDAC	0x00004000	/* Slow DAC */
#define GDER_RWE	0x00001000	/* ROM write enable */
#define GDER_SAMS	0x00000400	/* Serial-access memory size */
#define GDER_CS		0x00000200	/* Column size */
#define GDER_BLOCK_MASK	0x000001e0	/* eight/four column segments */
#define GDER_BLOCK_SHIFT	5
#define GDER_ADDR_MASK	0x0000001c	/* PCI address mask */
#define GDER_ADDR_SHIFT	2
#define GDER_ADDR_4MB	0x00000000	/* PCI mask <24:22> =  4MB */
#define GDER_ADDR_8MB	0x00000001	/* PCI mask <24:23> =  8MB */
#define GDER_ADDR_16MB	0x00000003	/* PCI mask bit 24  = 16MB */
#define GDER_ADDR_32MB	0x00000007	/* No PCI masking   = 32MB */
#define GDER_DEEP	0x00000001	/* 32-bpp or 8 bpp frame buffer */

/*
 * Graphics mode register
 */
#define GMOR_CE		0x00008000	/* Cap ends */
#define GMOR_Z16	0x00004000	/* 16 or 24 bit Z valuesx */
#define GMOR_GE		0x00002000	/* Win32 or X environment */
#define GMOR_SBY_MASK	0x00001800	/* Source byte mask for 32-bpp FB */
#define GMOR_SBY_0	0x00000000	/* Source byte 0 */
#define GMOR_SBY_1	0x00000800	/* Source byte 1 */
#define GMOR_SBY_2	0x00001000	/* Source byte 2 */
#define GMOR_SBY_3	0x00001800	/* Source byte 3 */
#define GMOR_SBM_MASK	0x00000700	/* Source bitmap format (32-bpp FB) */
#define GMOR_SBM_8P	0x00000000	/* 8-bpp packed */
#define GMOR_SBM_8U	0x00000100	/* 8-bpp unpacked */
#define GMOR_SBM_12L	0x00000200	/* 12-bpp low */
#define GMOR_SBM_12H	0x00000600	/* 12-bpp high */
#define GMOR_SBM_24	0x00000300	/* 24-bpp */
#define GMOR_MODE_MASK	0x0000007f	/* Graphics mode mask */
#define GMOR_MODE_SIMPLE	0x0000	/* Simple */
#define GMOR_MODE_SIMPLEZ	0x0010	/* Simple Z */
#define GMOR_MODE_OPQ_STPL	0x0001	/* Opaque Stipple */
#define GMOR_MODE_OPQ_FILL	0x0021	/* Opaque Fill */
#define GMOR_MODE_TRN_STPL	0x0005	/* Transparent Stipple */
#define GMOR_MODE_TRN_FILL	0x0025	/* Transparent Fill */
#define GMOR_MODE_BLK_STPL	0x000d	/* Block Stipple */
#define GMOR_MODE_BLK_FILL	0x002d	/* Block Fill */
#define GMOR_MODE_OPQ_LINE	0x0002	/* Opaque Line */
#define GMOR_MODE_TRN_LINE	0x0006	/* Transparent Line */
#define GMOR_MODE_CITNDL	0x000e	/* Color-interpolated transparent */
					/* non-dithered line */
#define GMOR_MODE_CITDL		0x002e	/* Color-intrp. trans. dithered line */
#define GMOR_MODE_SITL		0x004e	/* Sequential-intrp. transp line */
#define GMOR_MODE_ZOPQ_LINE	0x0012	/* Z buffered Opaque Line */
#define GMOR_MODE_ZTRN_LINE	0x0016	/* Z buffered Trans Line */
#define GMOR_MODE_ZOCITNDL	0x001a	/* Z buffered Opaque CITND line */
#define GMOR_MODE_ZOCITDL	0x003a	/* Z buffered Opaque CITD line */
#define GMOR_MODE_ZOSITL	0x005a	/* Z buffered Opaque SIT line */
#define GMOR_MODE_ZTCITNDL	0x001e	/* Z buffered transparent CITND line */
#define GMOR_MODE_ZTCITDL	0x003e	/* Z buffered transparent CITD line */
#define GMOR_MODE_ZTSITL	0x005e	/* Z buffered transparent SIT line */
#define GMOR_MODE_COPY		0x0007	/* Copy */
#define GMOR_MODE_DRDND		0x0017	/* DMA-read copy, non-dithered */
#define GMOR_MODE_DRDD		0x0037	/* DMA-read copy, dithered */
#define GMOR_MODE_DWCOPY	0x001f	/* DMA-write copy */

/*
 * Video Horizontal Control Register
 */
#define VHCR_ODD	0x80000000	/* Enable 4-pixel line skew */
#define VHCR_HSP	0x40000000	/* Horizontal sync polarity */
#define VHCR_BPORCH_MASK 0xfe00000      /* Back porch pixels / 4 */
#define VHCR_BPORCH_SHIFT	21
#define VHCR_HSYNC_MASK 0x001fc000	/* Hsync width / 4 */
#define VHCR_HSYNC_SHIFT	14
#define VHCR_FPORCH_MASK	0x3e00	/* Front porch pixels / 4 */
#define VHCR_FPORCH_SHIFT	9
#define VHCR_ACTIVE_MASK	0x01ff	/* Active lines */

#define VHCR_REG2ACTIVE(reg)	((((reg) >> 19) & 0x0600) | ((reg) & 0x01ff))
#define VHCR_ACTIVE2REG(val)	((((val) & 0x0600) << 19) | ((val) & 0x01ff))

/*
 * Video Vertical Control Register
 */
#define VVCR_SE		0x80000000	/* Stereo Enable */
#define VVCR_VSP	0x40000000	/* Vertical sync polarity */
#define VVCR_BPORCH_MASK 0xfc00000	/* Back porch in lines */
#define VVCR_BPORCH_SHIFT	22
#define VVCR_VSYNC_MASK	0x003f0000	/* Vsync width in lines */
#define VVCR_VSYNC_SHIFT	16
#define VVCR_FPORCH_MASK	0xf800	/* Front porch in lines */
#define VVCR_FPORCH_SHIFT	11
#define VVCR_ACTIVE_MASK	0x07ff	/* Active lines */

/*
 * Video Valid Register
 */
#define	VVR_VIDEOVALID	0x00000001	/* 0 VGA, 1 TGA2 (TGA2 only) */
#define	VVR_BLANK	0x00000002	/* 0 active, 1 blank */
#define	VVR_CURSOR	0x00000004	/* 0 disable, 1 enable (TGA2 R/O) */
#define	VVR_INTERLACE	0x00000008	/* 0 N/Int, 1 Int. (TGA2 R/O) */
#define	VVR_DPMS_MASK	0x00000030	/* See "DMPS mask" below */
#define	VVR_DPMS_SHIFT	4
#define	VVR_DDC		0x00000040	/* DDC-in pin value (R/O) */
#define	VVR_TILED	0x00000400	/* 0 linear, 1 tiled (not on TGA2) */
#define	VVR_LDDLY_MASK	0x01ff0000	/* load delay in quad pixel clock ticks
					   (not on TGA2) */
#define	VVR_LDDLY_SHIFT	16

/* TGA PCI register values */

#define DEC_VENDORID            0x1011
#define DEC_DEVICEID_TGA        0x0004
#define DEC_DEVICEID_TGA2       0x000D /* This is 0x000C in the documentation,
                                          but probing yields 0x000D... */
#define TGA_TYPE_T8_01          0       /* 8bpp, 1MB */
#define TGA_TYPE_T8_02          1       /* 8bpp, 2MB */
#define TGA_TYPE_T8_22          2       /* 8bpp, 4MB */
#define TGA_TYPE_T8_44          3       /* 8bpp, 8MB */
#define TGA_TYPE_T32_04         4       /* 32bpp, 4MB */
#define TGA_TYPE_T32_08         5       /* 32bpp, 8MB */
#define TGA_TYPE_T32_88         6       /* 32bpp, 16MB */
#define TGA2_TYPE_3D30          7       /* 8bpp, 8MB */
#define TGA2_TYPE_4D20          8       /* 32bpp, 16MB */
#define TGA_TYPE_UNKNOWN        9       /* unknown */

/* Possible video modes for TGA2... */

#define TGA2_VGA_MODE		0
#define TGA2_2DA_MODE		1

/* TGA register access macros */

#define TGA_REG_SPACE_OFFSET		0x100000

#define BASIC_READ_TGA_REGISTER(adp, reg)			\
	*(u_int32_t *)((adp)->va_mem_base +			\
		       (vm_offset_t)TGA_REG_SPACE_OFFSET +	\
		       (((vm_offset_t)(reg) << 2L)))

#endif /* _PCI_TGA_H_ */
