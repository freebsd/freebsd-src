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
#include "xge_cmn.h"
#endif

#define XGE_QUERY_STATS       1
#define XGE_QUERY_PCICONF     2
#define XGE_QUERY_DEVSTATS    3
#define XGE_QUERY_DEVCONF     4
#define XGE_READ_VERSION      5
#define XGE_QUERY_SWSTATS     6
#define XGE_QUERY_DRIVERSTATS 7
#define XGE_SET_BUFFER_MODE_1 8
#define XGE_SET_BUFFER_MODE_2 9
#define XGE_SET_BUFFER_MODE_5 10
#define XGE_QUERY_BUFFER_MODE 11


/* Function declerations */
int xge_get_pciconf(void);
int xge_get_devconf(void);
int xge_get_hwstats(void);
int xge_get_registers(void);
int xge_get_devstats(void);
int xge_get_swstats(void);
int xge_get_drvstats(void);
int xge_get_register(char *);
int xge_set_register(char *,char *);
int xge_get_drv_version(void);
int xge_get_buffer_mode(void);
int xge_change_buffer_mode(char *);
void xge_print_hwstats(void *,unsigned short);
void xge_print_pciconf(void *);
void xge_print_devconf(void *);
void xge_print_registers(void *);
void xge_print_register(u64,u64);
void xge_print_devstats(void *);
void xge_print_swstats(void *);
void xge_print_drvstats(void *);
void xge_print_drv_version(char *);

extern xge_pci_bar0_t         regInfo[];
extern xge_pci_config_t       pciconfInfo[];
extern xge_stats_hw_info_t    statsInfo[];
extern xge_device_config_t    devconfInfo[];
extern xge_stats_intr_info_t  intrInfo[];
extern xge_stats_tcode_info_t tcodeInfo[];
extern xge_stats_driver_info_t driverInfo[];

struct ifreq   ifreqp;
int    sockfd, indexer, buffer_size = 0;

