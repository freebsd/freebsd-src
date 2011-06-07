/*-
 * Copyright(c) 2002-2011 Exar Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification are permitted provided the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 *    3. Neither the name of the Exar Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*$FreeBSD$*/

#ifndef	_VXGE_INFO_H_
#define	_VXGE_INFO_H_

#include "vxge_cmn.h"

/* Function declerations */

void	vxge_get_registers_all(void);
int	vxge_get_registers_toc(void);
int	vxge_get_registers_vpath(void);
int	vxge_get_registers_vpmgmt(void);
int	vxge_get_registers_legacy(void);
int	vxge_get_registers_srpcim(void);
int	vxge_get_registers_mrpcim(void);
int	vxge_get_registers_common(void);
int	vxge_get_registers_pcicfgmgmt(void);

int	vxge_get_stats_common(void);
int	vxge_get_stats_mrpcim(void);
int	vxge_get_stats_driver(int);
void	vxge_get_stats_all(void);

int	vxge_get_hw_info(void);
int	vxge_get_pci_config(void);
int	vxge_get_port_mode(void);
int	vxge_set_port_mode(int);

int	vxge_get_bw_priority(int, vxge_query_device_info_e);
int	vxge_set_bw_priority(int, int, int, vxge_query_device_info_e);

void	vxge_print_registers(void *);
void	vxge_print_registers_toc(void *);
void	vxge_print_registers_vpath(void *, int);
void	vxge_print_registers_vpmgmt(void *);
void	vxge_print_registers_legacy(void *);
void	vxge_print_registers_srpcim(void *);
void	vxge_print_registers_mrpcim(void *);
void	vxge_print_registers_pcicfgmgmt(void *);

void	vxge_print_hw_info(void *);
void	vxge_print_pci_config(void *);
void	vxge_print_stats(void *, int);
void	vxge_print_stats_drv(void *, int);
void	vxge_print_bw_priority(void *);
void	vxge_print_port_mode(void *);

void*	vxge_mem_alloc(u_long);

extern	vxge_pci_bar0_t reginfo_toc[];
extern	vxge_pci_bar0_t reginfo_vpath[];
extern	vxge_pci_bar0_t reginfo_legacy[];
extern	vxge_pci_bar0_t reginfo_vpmgmt[];
extern	vxge_pci_bar0_t reginfo_mrpcim[];
extern	vxge_pci_bar0_t reginfo_srpcim[];
extern	vxge_pci_bar0_t reginfo_registers[];
extern	vxge_pci_bar0_t reginfo_pcicfgmgmt[];

#endif	/* _VXGE_INFO_H_ */
