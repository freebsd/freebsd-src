/* $NetBSD: cfe_ioctl.h,v 1.2 2003/02/07 17:52:08 cgd Exp $ */

/*-
 * Copyright 2000, 2001
 * Broadcom Corporation. All rights reserved.
 *
 * This software is furnished under license and may be used and copied only
 * in accordance with the following terms and conditions.  Subject to these
 * conditions, you may download, copy, install, use, modify and distribute
 * modified or unmodified copies of this software in source and/or binary
 * form. No title or ownership is transferred hereby.
 *
 * 1) Any source code used, modified or distributed must reproduce and
 *    retain this copyright notice and list of conditions as they appear in
 *    the source file.
 *
 * 2) No right is granted to use any trade name, trademark, or logo of
 *    Broadcom Corporation.  The "Broadcom Corporation" name may not be
 *    used to endorse or promote products derived from this software
 *    without the prior written permission of Broadcom Corporation.
 *
 * 3) THIS SOFTWARE IS PROVIDED "AS-IS" AND ANY EXPRESS OR IMPLIED
 *    WARRANTIES, INCLUDING BUT NOT LIMITED TO, ANY IMPLIED WARRANTIES OF
 *    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR
 *    NON-INFRINGEMENT ARE DISCLAIMED. IN NO EVENT SHALL BROADCOM BE LIABLE
 *    FOR ANY DAMAGES WHATSOEVER, AND IN PARTICULAR, BROADCOM SHALL NOT BE
 *    LIABLE FOR DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 *    BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *    WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 *    OR OTHERWISE), EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  IOCTL definitions			File: cfe_ioctl.h
    *  
    *  IOCTL function numbers and I/O data structures.
    *  
    *  Author:  Mitch Lichtenberg (mpl@broadcom.com)
    *  
    ********************************************************************* */


/*  *********************************************************************
    *  NVFAM and FLASH stuff
    ********************************************************************* */

#define IOCTL_NVRAM_GETINFO	1	/* return nvram_info_t */
#define IOCTL_NVRAM_ERASE	2	/* erase sector containing nvram_info_t area */
#define IOCTL_FLASH_ERASE_SECTOR 3	/* erase an arbitrary sector */
#define IOCTL_FLASH_ERASE_ALL 4		/* Erase the entire flash */

typedef struct nvram_info_s {
    int nvram_offset;		/* offset of environment area */
    int nvram_size;		/* size of environment area */
    int nvram_eraseflg;		/* true if we need to erase first */
} nvram_info_t;

/*  *********************************************************************
    *  Ethernet stuff
    ********************************************************************* */

#define IOCTL_ETHER_GETHWADDR	1

/*  *********************************************************************
    *  Block device stuff
    ********************************************************************* */

#define IOCTL_BLOCK_GETBLOCKSIZE 1
#define IOCTL_BLOCK_GETTOTALBLOCKS 2
