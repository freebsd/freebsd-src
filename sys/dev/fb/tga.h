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

#ifndef _FB_TGA_H_
#define _FB_TGA_H_

/* TGA-specific FB stuff */

struct gfb_softc;
struct video_adapter;

/*
 * Register definitions for the Brooktree Bt463 135MHz Monolithic
 * CMOS TrueVu RAMDAC.
 */

/*
 * Directly-accessible registers.  Note the address register is
 * auto-incrementing.
 */
#define	BT463_REG_ADDR_LOW		0x00	/* C1,C0 == 0,0 */
#define	BT463_REG_ADDR_HIGH		0x01	/* C1,C0 == 0,1 */
#define	BT463_REG_IREG_DATA		0x02	/* C1,C0 == 1,0 */
#define	BT463_REG_CMAP_DATA		0x03	/* C1,C0 == 1,1 */

#define	BT463_REG_MAX			BT463_REG_CMAP_DATA

/*
 * All internal register access to the Bt463 is done indirectly via the
 * Address Register (mapped into the host bus in a device-specific
 * fashion).  The following register definitions are in terms of
 * their address register address values.
 */

/* C1,C0 must be 1,0 */
#define	BT463_IREG_CURSOR_COLOR_0	0x0100	/* 3 r/w cycles */
#define	BT463_IREG_CURSOR_COLOR_1	0x0101	/* 3 r/w cycles */
#define	BT463_IREG_ID			0x0200
#define	BT463_IREG_COMMAND_0		0x0201
#define	BT463_IREG_COMMAND_1		0x0202
#define	BT463_IREG_COMMAND_2		0x0203
#define	BT463_IREG_READ_MASK_P0_P7	0x0205
#define	BT463_IREG_READ_MASK_P8_P15	0x0206
#define	BT463_IREG_READ_MASK_P16_P23	0x0207
#define	BT463_IREG_READ_MASK_P24_P27	0x0208
#define	BT463_IREG_BLINK_MASK_P0_P7	0x0209
#define	BT463_IREG_BLINK_MASK_P8_P15	0x020a
#define	BT463_IREG_BLINK_MASK_P16_P23	0x020b
#define	BT463_IREG_BLINK_MASK_P24_P27	0x020c
#define	BT463_IREG_TEST			0x020d
#define	BT463_IREG_INPUT_SIG		0x020e	/* 2 of 3 r/w cycles */
#define	BT463_IREG_OUTPUT_SIG		0x020f	/* 3 r/w cycles */
#define	BT463_IREG_REVISION		0x0220
#define	BT463_IREG_WINDOW_TYPE_TABLE	0x0300	/* 3 r/w cycles */

#define	BT463_NWTYPE_ENTRIES		0x10	/* 16 window type entries */

/* C1,C0 must be 1,1 */
#define	BT463_IREG_CPALETTE_RAM		0x0000	/* 3 r/w cycles */

#define	BT463_NCMAP_ENTRIES		0x210	/* 528 CMAP entries */

#define	BT463_DATA_CURCMAP_CHANGED	0x01	/* cursor colormap changed */
#define	BT463_DATA_CMAP_CHANGED		0x02	/* colormap changed */
#define	BT463_DATA_WTYPE_CHANGED	0x04	/* window type table changed */
#define	BT463_DATA_ALL_CHANGED		0x07

/*
 * Register definitions for the Brooktree Bt485A 170MHz Monolithic
 * CMOS True-Color RAMDAC.
 */

/*
 * Directly-addressed registers.
 */

#define	BT485_REG_PCRAM_WRADDR	0x00
#define	BT485_REG_PALETTE	0x01
#define	BT485_REG_PIXMASK	0x02
#define	BT485_REG_PCRAM_RDADDR	0x03
#define	BT485_REG_COC_WRADDR	0x04
#define	BT485_REG_COCDATA	0x05
#define	BT485_REG_COMMAND_0	0x06
#define	BT485_REG_COC_RDADDR	0x07
#define	BT485_REG_COMMAND_1	0x08
#define	BT485_REG_COMMAND_2	0x09
#define	BT485_REG_STATUS	0x0a
#define	BT485_REG_EXTENDED	BT485_REG_STATUS
#define	BT485_REG_CURSOR_RAM	0x0b
#define	BT485_REG_CURSOR_X_LOW	0x0c
#define	BT485_REG_CURSOR_X_HIGH	0x0d
#define	BT485_REG_CURSOR_Y_LOW	0x0e
#define	BT485_REG_CURSOR_Y_HIGH	0x0f

#define	BT485_REG_MAX		0x0f

#define	BT485_IREG_STATUS	0x00
#define	BT485_IREG_COMMAND_3	0x01
#define	BT485_IREG_COMMAND_4	0x02
#define	BT485_IREG_RSA		0x20
#define	BT485_IREG_GSA		0x21
#define	BT485_IREG_BSA		0x22

#define BT485_DATA_ENB_CHANGED		0x01	/* cursor enable changed */
#define BT485_DATA_CURCMAP_CHANGED	0x02	/* cursor colormap changed */
#define BT485_DATA_CURSHAPE_CHANGED	0x04	/* cursor size, image, mask changed */
#define BT485_DATA_CMAP_CHANGED		0x08	/* colormap changed */
#define BT485_DATA_ALL_CHANGED		0x0f
#define CURSOR_MAX_SIZE			64

#define TGA_DRIVER_NAME		"tga"
#define TGA2_DRIVER_NAME	"tga2"

#define BTWREG(sc, addr, val) 					\
	sc->gfbc->ramdac_wr((sc), BT463_REG_ADDR_LOW, (addr) & 0xff);	\
	sc->gfbc->ramdac_wr((sc), BT463_REG_ADDR_HIGH, ((addr) >> 8) & 0xff);\
	(sc)->gfbc->ramdac_wr((sc), BT463_REG_IREG_DATA, (val))
#define BTWNREG(sc, val)					\
	(sc)->gfbc->ramdac_wr((sc), BT463_REG_IREG_DATA, (val))
#define BTRREG(sc, addr)					\
	sc->gfbc->ramdac_wr((sc), BT463_REG_ADDR_LOW, (addr) & 0xff);	\
	sc->gfbc->ramdac_wr((sc), BT463_REG_ADDR_HIGH, ((addr) >> 8) & 0xff);\
	(sc)->gfbc->ramdac_rd((sc), BT463_REG_IREG_DATA)
#define BTRNREG(sc)						\
	(sc)->gfbc->ramdac_rd((sc), BT463_REG_IREG_DATA)

#endif /* _FB_TGA_H_ */
