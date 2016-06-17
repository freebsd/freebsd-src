/*
 * Copyright (C) 2001-2004 by Thomas Winischhofer, Vienna, Austria.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the named License,
 * or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA
 */

#ifndef _LINUX_SISFB
#define _LINUX_SISFB

#include <asm/ioctl.h>
#include <asm/types.h>

/**********************************************/
/*                   PUBLIC                   */
/**********************************************/

/* vbflags */
#define CRT2_DEFAULT            0x00000001
#define CRT2_LCD                0x00000002  /* TW: Never change the order of the CRT2_XXX entries */
#define CRT2_TV                 0x00000004  /*     (see SISCycleCRT2Type())                       */
#define CRT2_VGA                0x00000008
#define TV_NTSC                 0x00000010
#define TV_PAL                  0x00000020
#define TV_HIVISION             0x00000040
#define TV_YPBPR                0x00000080
#define TV_AVIDEO               0x00000100
#define TV_SVIDEO               0x00000200
#define TV_SCART                0x00000400
#define VB_CONEXANT		0x00000800	/* 661 series only */
#define VB_TRUMPION		VB_CONEXANT	/* 300 series only */
#define TV_PALM                 0x00001000
#define TV_PALN                 0x00002000
#define TV_NTSCJ		0x00001000
#define VB_302ELV		0x00004000
#define TV_CHSCART              0x00008000
#define TV_CHYPBPR525I          0x00010000
#define CRT1_VGA		0x00000000
#define CRT1_LCDA		0x00020000
#define VGA2_CONNECTED          0x00040000
#define VB_DISPTYPE_CRT1	0x00080000  	/* CRT1 connected and used */
#define VB_301                  0x00100000	/* Video bridge type */
#define VB_301B                 0x00200000
#define VB_302B                 0x00400000
#define VB_30xBDH		0x00800000      /* 30xB DH version (w/o LCD support) */
#define VB_LVDS                 0x01000000
#define VB_CHRONTEL             0x02000000
#define VB_301LV                0x04000000
#define VB_302LV                0x08000000
#define VB_301C			0x10000000
#define VB_SINGLE_MODE          0x20000000   	/* CRT1 or CRT2; determined by DISPTYPE_CRTx */
#define VB_MIRROR_MODE		0x40000000   	/* CRT1 + CRT2 identical (mirror mode) */
#define VB_DUALVIEW_MODE	0x80000000   	/* CRT1 + CRT2 independent (dual head mode) */

/* Aliases: */
#define CRT2_ENABLE		(CRT2_LCD | CRT2_TV | CRT2_VGA)
#define TV_STANDARD             (TV_NTSC | TV_PAL | TV_PALM | TV_PALN | TV_NTSCJ)
#define TV_INTERFACE            (TV_AVIDEO|TV_SVIDEO|TV_SCART|TV_HIVISION|TV_YPBPR|TV_CHSCART|TV_CHYPBPR525I)

/* Only if TV_YPBPR is set: */
#define TV_YPBPR525I		TV_NTSC
#define TV_YPBPR525P		TV_PAL
#define TV_YPBPR750P		TV_PALM
#define TV_YPBPR1080I		TV_PALN
#define TV_YPBPRALL 		(TV_YPBPR525I | TV_YPBPR525P | TV_YPBPR750P | TV_YPBPR1080I)

#define VB_SISBRIDGE            (VB_301|VB_301B|VB_301C|VB_302B|VB_301LV|VB_302LV|VB_302ELV)
#define VB_SISTVBRIDGE          (VB_301|VB_301B|VB_301C|VB_302B|VB_301LV|VB_302LV)
#define VB_VIDEOBRIDGE		(VB_SISBRIDGE | VB_LVDS | VB_CHRONTEL | VB_CONEXANT)

#define VB_DISPTYPE_DISP2	CRT2_ENABLE
#define VB_DISPTYPE_CRT2	CRT2_ENABLE
#define VB_DISPTYPE_DISP1	VB_DISPTYPE_CRT1
#define VB_DISPMODE_SINGLE	VB_SINGLE_MODE
#define VB_DISPMODE_MIRROR	VB_MIRROR_MODE
#define VB_DISPMODE_DUAL	VB_DUALVIEW_MODE
#define VB_DISPLAY_MODE       	(SINGLE_MODE | MIRROR_MODE | DUALVIEW_MODE)

/* *Never* change the order of the following enum */
typedef enum _SIS_CHIP_TYPE {
	SIS_VGALegacy = 0,	/* chip_id in sisfb_info */
	SIS_300,
	SIS_630,
	SIS_540,
	SIS_730,
	SIS_315H,
	SIS_315,
	SIS_315PRO,
	SIS_550,
	SIS_650,
	SIS_740,
	SIS_330,
	SIS_661,
	SIS_741,
	SIS_660,
	SIS_760,
	MAX_SIS_CHIP
} SIS_CHIP_TYPE;

/* Addtional IOCTLs for communication sisfb <> X driver                */
/* If changing this, vgatypes.h must also be changed (for X driver)    */

/* ioctl for identifying and giving some info (esp. memory heap start) */
#define SISFB_GET_INFO	  	_IOR('n',0xF8,__u32)
/* ioctrl to get current vertical retrace status */
#define SISFB_GET_VBRSTATUS  	_IOR('n',0xF9,__u32)
/* ioctl to enable/disable panning auto-maximize (like nomax parameter) */
#define SISFB_GET_AUTOMAXIMIZE 	_IOR('n',0xFA,__u32)
#define SISFB_SET_AUTOMAXIMIZE 	_IOW('n',0xFA,__u32)

/* TW: Structure argument for SISFB_GET_INFO ioctl  */
typedef struct _SISFB_INFO sisfb_info, *psisfb_info;

struct _SISFB_INFO {
	u32    	sisfb_id;         	/* for identifying sisfb */
#ifndef SISFB_ID
#define SISFB_ID	  0x53495346    /* Identify myself with 'SISF' */
#endif
 	u32    	chip_id;		/* PCI ID of detected chip */
	u32    	memory;			/* video memory in KB which sisfb manages */
	u32    	heapstart;            	/* heap start (= sisfb "mem" argument) in KB */
	u8     	fbvidmode;		/* current sisfb mode */

	u8     	sisfb_version;
	u8     	sisfb_revision;
	u8 	sisfb_patchlevel;

	u8 	sisfb_caps;		/* Sisfb capabilities */

	u32    	sisfb_tqlen;		/* turbo queue length (in KB) */

	u32 	sisfb_pcibus;      	/* The card's PCI ID */
	u32 	sisfb_pcislot;
	u32 	sisfb_pcifunc;

	u8 	sisfb_lcdpdc;		/* PanelDelayCompensation */

	u8 	sisfb_lcda;		/* Detected status of LCDA for low res/text modes */

	u32 	sisfb_vbflags;
	u32 	sisfb_currentvbflags;

	u32 	sisfb_scalelcd;
	u32 	sisfb_specialtiming;

	u8 	sisfb_haveemi;
	u8 	sisfb_emi30,sisfb_emi31,sisfb_emi32,sisfb_emi33;
	u8 	sisfb_haveemilcd;

	u8 	sisfb_lcdpdca;		/* PanelDelayCompensation for LCD-via-CRT1 */

	u8 	reserved[212]; 		/* for future use */
};

/* For fb memory manager */
struct sis_memreq {
	unsigned long offset;
	unsigned long size;
};

/* More or less deprecated stuff follows: */
typedef enum _TVTYPE {
	TVMODE_NTSC = 0,
	TVMODE_PAL,
	TVMODE_HIVISION,
	TVMODE_TOTAL
} SIS_TV_TYPE;

typedef enum _TVPLUGTYPE {
	TVPLUG_Legacy = 0,
	TVPLUG_COMPOSITE,
	TVPLUG_SVIDEO,
	TVPLUG_SCART,
	TVPLUG_TOTAL
} SIS_TV_PLUG;

struct mode_info {
	int    bpp;
	int    xres;
	int    yres;
	int    v_xres;		/* deprecated - use var instead */
	int    v_yres;		/* deprecated - use var instead */
	int    org_x;		/* deprecated - use var instead */
	int    org_y;		/* deprecated - use var instead */
	unsigned int  vrate;
};

struct ap_data {
	struct mode_info minfo;
	unsigned long iobase;
	unsigned int  mem_size;
	unsigned long disp_state;  /* deprecated */
	SIS_CHIP_TYPE chip;
	unsigned char hasVB;
	SIS_TV_TYPE TV_type;	   /* deprecated */
	SIS_TV_PLUG TV_plug;	   /* deprecated */
	unsigned long version;
	unsigned long vbflags;	   /* replaces deprecated entries above */
	unsigned long currentvbflags;
	char reserved[248];
};

/**********************************************/
/*                  PRIVATE                   */
/**********************************************/

#ifdef __KERNEL__
#include <linux/spinlock.h>

typedef enum _VGA_ENGINE {
	UNKNOWN_VGA = 0,
	SIS_300_VGA,
	SIS_315_VGA,
} VGA_ENGINE;

struct video_info {
	int           	chip_id;
	unsigned int  	video_size;
	unsigned long 	video_base;
	char  *       	video_vbase;
	unsigned long 	mmio_size;
	unsigned long 	mmio_base;
	char  *       	mmio_vbase;
	unsigned long 	vga_base;
	unsigned long 	mtrr;
	unsigned long 	heapstart;
	char  *	      	bios_vbase;
	char  *	      	bios_abase;

	int    		video_bpp;
	int    		video_cmap_len;
	int    		video_width;
	int    		video_height;
	int    		video_vwidth;		/* DEPRECATED - use var instead */
	int    		video_vheight;		/* DEPRECATED - use var instead */
	int    		org_x;			/* DEPRECATED - use var instead */
	int    		org_y;			/* DEPRECATED - use var instead */
	int    		video_linelength;
	unsigned int 	refresh_rate;

	unsigned long 	disp_state;		/* DEPRECATED */
	unsigned char 	hasVB;			/* DEPRECATED */
	unsigned char 	TV_type;		/* DEPRECATED */
	unsigned char 	TV_plug;		/* DEPRECATED */

	SIS_CHIP_TYPE chip;
	unsigned char revision_id;

        unsigned short 	DstColor;		/* For 2d acceleration */
	unsigned long  	SiS310_AccelDepth;
	unsigned long  	CommandReg;

	spinlock_t     	lockaccel;		/* Do not use outside of kernel! */

        unsigned int   	pcibus;
	unsigned int   	pcislot;
	unsigned int   	pcifunc;

	int 	       	accel;

	unsigned short 	subsysvendor;
	unsigned short 	subsysdevice;

	unsigned long  	vbflags;		/* Replacing deprecated stuff from above */
	unsigned long  	currentvbflags;

	int    		current_bpp;
	int    		current_width;
	int    		current_height;
	int    		current_htotal;
	int    		current_vtotal;
	__u32  		current_pixclock;
	int    		current_refresh_rate;
	
	u8  		mode_no;
	u8  		rate_idx;
	int    		modechanged;
	unsigned char 	modeprechange;
	
	int  		newrom;
	int  		registered;
	
	VGA_ENGINE 	sisvga_engine;
	int 		hwcursor_size;
	int 		CRT2_write_enable;
	u8            	caps;
	
	unsigned char 	detectedpdc;
	unsigned char 	detectedpdca;
	unsigned char 	detectedlcda;
	
	unsigned long 	hwcursor_vbase;

	char reserved[166];
};

extern struct video_info ivideo;

extern void sis_malloc(struct sis_memreq *req);
extern void sis_free(unsigned long base);
extern void sis_dispinfo(struct ap_data *rec);
#endif
#endif
