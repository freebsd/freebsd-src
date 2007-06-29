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
/******************************************
 *  getinfo.h
 *
 *  To get the Tx, Rx, PCI, Interrupt statistics,
 *  PCI configuration space,device configuration
 *  and bar0 register values
 ******************************************/
#ifndef XGE_CMN_H
#include "xge_cmn.h"
#endif

#define XGE_QUERY_STATS       1
#define XGE_QUERY_PCICONF     2
#define XGE_QUERY_INTRSTATS   3 
#define XGE_QUERY_DEVCONF     4
#define XGE_READ_VERSION      5
#define XGE_QUERY_TCODE       6
#define XGE_SET_BUFFER_MODE_1 7
#define XGE_SET_BUFFER_MODE_2 8
#define XGE_SET_BUFFER_MODE_3 9
#define XGE_SET_BUFFER_MODE_5 10
#define XGE_QUERY_BUFFER_MODE 11


/* Function declerations */
int getPciConf();
int getDevConf();
int getStats();
int getRegInfo();
int getIntrStats();
int getTcodeStats();
int getReadReg(char *,char *);
int getWriteReg(char *,char *,char *);
int getDriverVersion();
int getBufMode();
int changeBufMode(char *);

void logStats(void *,unsigned short);
void logPciConf(void *);
void logDevConf(void *);
void logRegInfo(void *);
void logReadReg(u64,u64);
void logIntrStats(void *);
void logTcodeStats(void *);
void logDriverInfo(char *);

extern xge_pci_bar0_t         regInfo[];
extern xge_pci_config_t       pciconfInfo[];
extern xge_stats_hw_info_t    statsInfo[];
extern xge_device_config_t    devconfInfo[];
extern xge_stats_intr_info_t  intrInfo[];
extern xge_stats_tcode_info_t tcodeInfo[];
struct ifreq   ifreqp;
int            sockfd, indexer, bufferSize = 0;
char          *pAccess;

