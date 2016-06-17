/*
 * include/asm-shmedia/linux_logo.h
 *
 * Copyright (C) 1996 Larry Ewing (lewing@isc.tamu.edu)
 * Copyright (C) 1996 Jakub Jelinek (jj@sunsite.mff.cuni.cz)
 *
 * You can put anything here, but:
 * LINUX_LOGO_COLORS has to be less than 224
 * image size has to be 80x80
 * values have to start from 0x20
 * (i.e. RGB(linux_logo_red[0],
 *	     linux_logo_green[0],
 *	     linux_logo_blue[0]) is color 0x20)
 * BW image has to be 80x80 as well, with MS bit
 * on the left
 * Serial_console ascii image can be any size,
 * but should contain %s to display the version
 */
 
#include <linux/init.h>
#include <linux/version.h>

#define linux_logo_banner "Linux/SHmedia version " UTS_RELEASE

#define LINUX_LOGO_COLORS 214

#ifdef INCLUDE_LINUX_LOGO_DATA

#define INCLUDE_LINUX_LOGOBW
#define INCLUDE_LINUX_LOGO16

#include <linux/linux_logo.h>

#else

/* prototypes only */
extern unsigned char linux_logo_red[];
extern unsigned char linux_logo_green[];
extern unsigned char linux_logo_blue[];
extern unsigned char linux_logo[];
extern unsigned char linux_logo_bw[];
extern unsigned char linux_logo16_red[];
extern unsigned char linux_logo16_green[];
extern unsigned char linux_logo16_blue[];
extern unsigned char linux_logo16[];

#endif
