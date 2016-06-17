/*
 *  linux/drivers/video/cyber2000fb.h
 *
 *  Copyright (C) 1998-2000 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Integraphics Cyber2000 frame buffer device
 */
#include <linux/config.h>

/*
 * Internal CyberPro sizes and offsets.
 */
#define MMIO_OFFSET	0x00800000
#define MMIO_SIZE	0x000c0000

#define NR_PALETTE	256

#if defined(DEBUG) && defined(CONFIG_DEBUG_LL)
static void debug_printf(char *fmt, ...)
{
	extern void printascii(const char *);
	char buffer[128];
	va_list ap;

	va_start(ap, fmt);
	vsprintf(buffer, fmt, ap);
	va_end(ap);

	printascii(buffer);
}
#else
#define debug_printf(x...) do { } while (0)
#endif

#define PIXFORMAT_8BPP		0
#define PIXFORMAT_16BPP		1
#define PIXFORMAT_24BPP		2
#define PIXFORMAT_32BPP		3

#define VISUALID_256		1
#define VISUALID_64K		2
#define VISUALID_16M_32		3
#define VISUALID_16M		4
#define VISUALID_32K		6

#define FUNC_CTL		0x3c
#define FUNC_CTL_EXTREGENBL		0x80	/* enable access to 0xbcxxx		*/

#define BIU_BM_CONTROL		0x3e
#define BIU_BM_CONTROL_ENABLE		0x01	/* enable bus-master			*/
#define BIU_BM_CONTROL_BURST		0x02	/* enable burst				*/
#define BIU_BM_CONTROL_BACK2BACK	0x04	/* enable back to back			*/

#define X_V2_VID_MEM_START	0x40
#define X_V2_VID_SRC_WIDTH	0x43
#define X_V2_X_START		0x45
#define X_V2_X_END		0x47
#define X_V2_Y_START		0x49
#define X_V2_Y_END		0x4b
#define X_V2_VID_SRC_WIN_WIDTH	0x4d

#define Y_V2_DDA_X_INC		0x43
#define Y_V2_DDA_Y_INC		0x47
#define Y_V2_VID_FIFO_CTL	0x49
#define Y_V2_VID_FMT		0x4b
#define Y_V2_VID_DISP_CTL1	0x4c
#define Y_V2_VID_FIFO_CTL1	0x4d

#define J_X2_VID_MEM_START	0x40
#define J_X2_VID_SRC_WIDTH	0x43
#define J_X2_X_START		0x47
#define J_X2_X_END		0x49
#define J_X2_Y_START		0x4b
#define J_X2_Y_END		0x4d
#define J_X2_VID_SRC_WIN_WIDTH	0x4f

#define K_X2_DDA_X_INIT		0x40
#define K_X2_DDA_X_INC		0x42
#define K_X2_DDA_Y_INIT		0x44
#define K_X2_DDA_Y_INC		0x46
#define K_X2_VID_FMT		0x48
#define K_X2_VID_DISP_CTL1	0x49

#define K_CAP_X2_CTL1		0x49

#define CAP_X_START		0x60
#define CAP_X_END		0x62
#define CAP_Y_START		0x64
#define CAP_Y_END		0x66
#define CAP_DDA_X_INIT		0x68
#define CAP_DDA_X_INC		0x6a
#define CAP_DDA_Y_INIT		0x6c
#define CAP_DDA_Y_INC		0x6e

#define MEM_CTL1		0x71

#define MEM_CTL2		0x72
#define MEM_CTL2_SIZE_2MB		0x01
#define MEM_CTL2_SIZE_4MB		0x02
#define MEM_CTL2_SIZE_MASK		0x03
#define MEM_CTL2_64BIT			0x04

#define EXT_FIFO_CTL		0x74

#define CAP_PIP_X_START		0x80
#define CAP_PIP_X_END		0x82
#define CAP_PIP_Y_START		0x84
#define CAP_PIP_Y_END		0x86

#define CAP_NEW_CTL1		0x88

#define CAP_NEW_CTL2		0x89

#define BM_CTRL0		0x9c
#define BM_CTRL1		0x9d

#define CAP_MODE1		0xa4
#define CAP_MODE1_8BIT			0x01	/* enable 8bit capture mode		*/
#define CAP_MODE1_CCIR656		0x02	/* CCIR656 mode				*/
#define CAP_MODE1_IGNOREVGT		0x04	/* ignore VGT				*/
#define CAP_MODE1_ALTFIFO		0x10	/* use alternate FIFO for capture	*/
#define CAP_MODE1_SWAPUV		0x20	/* swap UV bytes			*/
#define CAP_MODE1_MIRRORY		0x40	/* mirror vertically			*/
#define CAP_MODE1_MIRRORX		0x80	/* mirror horizontally			*/

#define DCLK_MULT		0xb0
#define DCLK_DIV		0xb1
#define DCLK_DIV_VFSEL			0x20
#define MCLK_MULT		0xb2
#define MCLK_DIV		0xb3

#define CAP_MODE2		0xa5

#define Y_TV_CTL		0xae

#define EXT_MEM_START		0xc0		/* ext start address 21 bits		*/
#define HOR_PHASE_SHIFT		0xc2		/* high 3 bits				*/
#define EXT_SRC_WIDTH		0xc3		/* ext offset phase  10 bits		*/
#define EXT_SRC_HEIGHT		0xc4		/* high 6 bits				*/
#define EXT_X_START		0xc5		/* ext->screen, 16 bits			*/
#define EXT_X_END		0xc7		/* ext->screen, 16 bits			*/
#define EXT_Y_START		0xc9		/* ext->screen, 16 bits			*/
#define EXT_Y_END		0xcb		/* ext->screen, 16 bits			*/
#define EXT_SRC_WIN_WIDTH	0xcd		/* 8 bits				*/
#define EXT_COLOUR_COMPARE	0xce		/* 24 bits				*/
#define EXT_DDA_X_INIT		0xd1		/* ext->screen 16 bits			*/
#define EXT_DDA_X_INC		0xd3		/* ext->screen 16 bits			*/
#define EXT_DDA_Y_INIT		0xd5		/* ext->screen 16 bits			*/
#define EXT_DDA_Y_INC		0xd7		/* ext->screen 16 bits			*/

#define EXT_VID_FIFO_CTL	0xd9

#define EXT_VID_FMT		0xdb
#define EXT_VID_FMT_YUV422		0x00	/* formats - does this cause conversion? */
#define EXT_VID_FMT_RGB555		0x01
#define EXT_VID_FMT_RGB565		0x02
#define EXT_VID_FMT_RGB888_24		0x03
#define EXT_VID_FMT_RGB888_32		0x04
#define EXT_VID_FMT_DUP_PIX_ZOON	0x08	/* duplicate pixel zoom			*/
#define EXT_VID_FMT_MOD_3RD_PIX		0x20	/* modify 3rd duplicated pixel		*/
#define EXT_VID_FMT_DBL_H_PIX		0x40	/* double horiz pixels			*/
#define EXT_VID_FMT_UV128		0x80	/* UV data offset by 128		*/

#define EXT_VID_DISP_CTL1	0xdc
#define EXT_VID_DISP_CTL1_INTRAM	0x01	/* video pixels go to internal RAM	*/
#define EXT_VID_DISP_CTL1_IGNORE_CCOMP	0x02	/* ignore colour compare registers	*/
#define EXT_VID_DISP_CTL1_NOCLIP	0x04	/* do not clip to 16235,16240		*/
#define EXT_VID_DISP_CTL1_UV_AVG	0x08	/* U/V data is averaged			*/
#define EXT_VID_DISP_CTL1_Y128		0x10	/* Y data offset by 128			*/
#define EXT_VID_DISP_CTL1_VINTERPOL_OFF	0x20	/* vertical interpolation off		*/
#define EXT_VID_DISP_CTL1_FULL_WIN	0x40	/* video out window full		*/
#define EXT_VID_DISP_CTL1_ENABLE_WINDOW	0x80	/* enable video window			*/

#define EXT_VID_FIFO_CTL1	0xdd

#define VFAC_CTL1		0xe8
#define VFAC_CTL1_CAPTURE		0x01	/* capture enable			*/
#define VFAC_CTL1_VFAC_ENABLE		0x02	/* vfac enable				*/
#define VFAC_CTL1_FREEZE_CAPTURE	0x04	/* freeze capture			*/
#define VFAC_CTL1_FREEZE_CAPTURE_SYNC	0x08	/* sync freeze capture			*/
#define VFAC_CTL1_VALIDFRAME_SRC	0x10	/* select valid frame source		*/
#define VFAC_CTL1_PHILIPS		0x40	/* select Philips mode			*/
#define VFAC_CTL1_MODVINTERPOLCLK	0x80	/* modify vertical interpolation clocl	*/

#define VFAC_CTL2		0xe9
#define VFAC_CTL2_INVERT_VIDDATAVALID	0x01	/* invert video data valid		*/
#define VFAC_CTL2_INVERT_GRAPHREADY	0x02	/* invert graphic ready output sig	*/
#define VFAC_CTL2_INVERT_DATACLK	0x04	/* invert data clock signal		*/
#define VFAC_CTL2_INVERT_HSYNC		0x08	/* invert hsync input			*/
#define VFAC_CTL2_INVERT_VSYNC		0x10	/* invert vsync input			*/
#define VFAC_CTL2_INVERT_FRAME		0x20	/* invert frame odd/even input		*/
#define VFAC_CTL2_INVERT_BLANK		0x40	/* invert blank output			*/
#define VFAC_CTL2_INVERT_OVSYNC		0x80	/* invert other vsync input		*/

#define VFAC_CTL3		0xea
#define VFAC_CTL3_CAP_IRQ		0x40	/* enable capture interrupt		*/

#define CAP_MEM_START		0xeb		/* 18 bits				*/
#define CAP_MAP_WIDTH		0xed		/* high 6 bits				*/
#define CAP_PITCH		0xee		/* 8 bits				*/

#define CAP_CTL_MISC		0xef
#define CAP_CTL_MISC_HDIV		0x01
#define CAP_CTL_MISC_HDIV4		0x02
#define CAP_CTL_MISC_ODDEVEN		0x04
#define CAP_CTL_MISC_HSYNCDIV2		0x08
#define CAP_CTL_MISC_SYNCTZHIGH		0x10
#define CAP_CTL_MISC_SYNCTZOR		0x20
#define CAP_CTL_MISC_DISPUSED		0x80

#define REG_BANK		0xfa
#define REG_BANK_X			0x00
#define REG_BANK_Y			0x01
#define REG_BANK_W			0x02
#define REG_BANK_T			0x03
#define REG_BANK_J			0x04
#define REG_BANK_K			0x05

/*
 * Bus-master
 */
#define BM_VID_ADDR_LOW		0xbc040
#define BM_VID_ADDR_HIGH	0xbc044
#define BM_ADDRESS_LOW		0xbc080
#define BM_ADDRESS_HIGH		0xbc084
#define BM_LENGTH		0xbc088
#define BM_CONTROL		0xbc08c
#define BM_CONTROL_ENABLE		0x01	/* enable transfer			*/
#define BM_CONTROL_IRQEN		0x02	/* enable IRQ at end of transfer	*/
#define BM_CONTROL_INIT			0x04	/* initialise status & count		*/
#define BM_COUNT		0xbc090		/* read-only				*/

/*
 * Graphics Co-processor
 */
#define CO_CMD_L_PATTERN_FGCOL	0x8000
#define CO_CMD_L_INC_LEFT	0x0004
#define CO_CMD_L_INC_UP		0x0002

#define CO_CMD_H_SRC_PIXMAP	0x2000
#define CO_CMD_H_BLITTER	0x0800

#define CO_REG_CONTROL		0xbf011
#define CO_REG_SRC_WIDTH	0xbf018
#define CO_REG_PIX_FORMAT	0xbf01c
#define CO_REG_FORE_MIX		0xbf048
#define CO_REG_FOREGROUND	0xbf058
#define CO_REG_WIDTH		0xbf060
#define CO_REG_HEIGHT		0xbf062
#define CO_REG_X_PHASE		0xbf078
#define CO_REG_CMD_L		0xbf07c
#define CO_REG_CMD_H		0xbf07e
#define CO_REG_SRC_PTR		0xbf170
#define CO_REG_DEST_PTR		0xbf178
#define CO_REG_DEST_WIDTH	0xbf218

/*
 * Private structure
 */
struct cfb_info;

struct cyberpro_info {
	struct pci_dev	*dev;
	unsigned char	*regs;
	char		*fb;
	char		dev_name[32];
	unsigned int	fb_size;

	/*
	 * The following is a pointer to be passed into the
	 * functions below.  The modules outside the main
	 * cyber2000fb.c driver have no knowledge as to what
	 * is within this structure.
	 */
	struct cfb_info *info;

	/*
	 * Use these to enable the BM or TV registers.  In an SMP
	 * environment, these two function pointers should only be
	 * called from the module_init() or module_exit()
	 * functions.
	 */
	void (*enable_extregs)(struct cfb_info *);
	void (*disable_extregs)(struct cfb_info *);
};

/*
 * Note! Writing to the Cyber20x0 registers from an interrupt
 * routine is definitely a bad idea atm.
 */
int cyber2000fb_attach(struct cyberpro_info *info, int idx);
void cyber2000fb_detach(int idx);

