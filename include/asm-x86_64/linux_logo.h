/* $Id: linux_logo.h,v 1.4 2001/07/05 23:44:45 ak Exp $
 * include/asm-x86_64/linux_logo.h: This is a linux logo
 *                                to be displayed on boot.
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

/* We should create logo of penguin with a big hammer (-: --pavel */

#define linux_logo_banner "Linux/x86-64 version " UTS_RELEASE

#include <linux/linux_logo.h>

