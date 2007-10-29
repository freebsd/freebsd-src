/*-
 * Copyright (c) 2002-2007 Neterion, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef XGE_CMN_H
#define XGE_CMN_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

#if BYTE_ORDER == BIG_ENDIAN
#define XGE_OS_HOST_BIG_ENDIAN 1
#endif

#define u64 unsigned long long
#define u32 unsigned int
#define u16 unsigned short
#define u8  unsigned char

#define XGE_COUNT_REGS           386
#define XGE_COUNT_STATS          160
#define XGE_COUNT_PCICONF        43
#define XGE_COUNT_DEVCONF        1677
#ifdef CONFIG_LRO
#define XGE_COUNT_INTRSTAT       26
#else
#define XGE_COUNT_INTRSTAT       20
#endif
#define XGE_COUNT_SWSTAT         54
#define XGE_COUNT_DRIVERSTATS    27
#define DEVICE_ID_XFRAME_II      0x5832
#define XGE_COUNT_EXTENDED_STATS 56

#define XGE_PRINT(fd, fmt...) {                                                \
	fprintf(fd, fmt);                                                      \
	fprintf(fd, "\n");                                                     \
	printf(fmt);                                                           \
	printf("\n");                                                          \
}

#define XGE_PRINT_LINE(fd)    XGE_PRINT(fd, line);

/* Read & Write Register */
typedef struct barregister
{
	char option[2];
	u64 offset;
	u64 value;
}xge_register_info_t;

/* Register Dump */
typedef struct xge_pci_bar0_t
{
	u8   name[32];                     /* Register name as in user guides */
	u64  offset;                       /* Offset from base address        */
	u64  value;                        /* Value                           */
	char type;                         /* 1: XframeII, 0: Common          */
} xge_pci_bar0_t;

/* Hardware Statistics */
typedef struct xge_stats_hw_info_t
{
	u8  name[32];                      /* Statistics name                 */
	u64 be_offset;                     /* Offset from base address (BE)   */
	u64 le_offset;                     /* Offset from base address (LE)   */
	u8  type;                          /* Type: 1, 2, 3 or 4 bytes        */
	u64 value;                         /* Value                           */
} xge_stats_hw_info_t;

/* PCI Configuration Space */
typedef struct xge_pci_config_t
{
	u8  name[32];                      /* Pci conf. name                  */
	u64 be_offset;                     /* Offset from base address (BE)   */
	u64 le_offset;                     /* Offset from base address (LE)   */
	u64 value;                         /* Value                           */
} xge_pci_config_t;

/* Device Configuration */
typedef struct xge_device_config_t
{
	u8  name[32];                      /* Device conf. name               */
	u64 value;                         /* Value                           */
} xge_device_config_t;

/* Interrupt Statistics */
typedef struct xge_stats_intr_info_t
{
	u8  name[32];                      /* Interrupt entry name            */
	u64 value;                         /* Value (count)                   */
} xge_stats_intr_info_t;

/* Tcode Statistics */
typedef struct xge_stats_tcode_info_t
{
    u8  name[32];                          /* Tcode entry name                */
    u64 value;                             /* Value (count)                   */
    u8  type;                              /* Type: 1, 2, 3 or 4 bytes        */
    u16 flag;
}xge_stats_tcode_info_t;

typedef struct xge_stats_driver_info_t
{
	u8 name[32];                       /* Driver statistics name          */
	u64 value;                         /* Value                           */
} xge_stats_driver_info_t;

#ifdef XGE_OS_HOST_BIG_ENDIAN
#define GET_OFFSET_STATS(index)       statsInfo[(index)].be_offset
#define GET_OFFSET_PCICONF(index)     pciconfInfo[(index)].be_offset
#else
#define GET_OFFSET_STATS(index)       statsInfo[(index)].le_offset
#define GET_OFFSET_PCICONF(index)     pciconfInfo[(index)].le_offset
#endif

#endif //XGE_CMN_H
