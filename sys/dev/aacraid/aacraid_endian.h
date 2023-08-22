/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Leandro Lupori
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
 */

#ifndef AACRAID_ENDIAN_H
#define AACRAID_ENDIAN_H

#include <sys/endian.h>

#if _BYTE_ORDER == _LITTLE_ENDIAN

/* On Little-Endian (LE) hosts, make all FIB data conversion functions empty. */

/* Convert from Little-Endian to host order (TOH) */
#define aac_fib_header_toh(ptr)
#define aac_adapter_info_toh(ptr)
#define aac_container_creation_toh(ptr)
#define aac_mntobj_toh(ptr)
#define aac_mntinforesp_toh(ptr)
#define aac_fsa_ctm_toh(ptr)
#define aac_cnt_config_toh(ptr)
#define aac_ctcfg_resp_toh(ptr)
#define aac_getbusinf_toh(ptr)
#define aac_vmi_businf_resp_toh(ptr)
#define aac_srb_response_toh(ptr)

/* Convert from host order to Little-Endian (TOLE) */
#define aac_adapter_init_tole(ptr)
#define aac_fib_header_tole(ptr)
#define aac_mntinfo_tole(ptr)
#define aac_fsa_ctm_tole(ptr)
#define aac_cnt_config_tole(ptr)
#define aac_raw_io_tole(ptr)
#define aac_raw_io2_tole(ptr)
#define aac_fib_xporthdr_tole(ptr)
#define aac_ctcfg_tole(ptr)
#define aac_vmioctl_tole(ptr)
#define aac_pause_command_tole(ptr)
#define aac_srb_tole(ptr)
#define aac_sge_ieee1212_tole(ptr)
#define aac_sg_entryraw_tole(ptr)
#define aac_sg_entry_tole(ptr)
#define aac_sg_entry64_tole(ptr)
#define aac_blockread_tole(ptr)
#define aac_blockwrite_tole(ptr)
#define aac_blockread64_tole(ptr)
#define aac_blockwrite64_tole(ptr)

#else /* _BYTE_ORDER != _LITTLE_ENDIAN */

/* Convert from Little-Endian to host order (TOH) */
void aac_fib_header_toh(struct aac_fib_header *ptr);
void aac_adapter_info_toh(struct aac_adapter_info *ptr);
void aac_container_creation_toh(struct aac_container_creation *ptr);
void aac_mntobj_toh(struct aac_mntobj *ptr);
void aac_mntinforesp_toh(struct aac_mntinforesp *ptr);
void aac_fsa_ctm_toh(struct aac_fsa_ctm *ptr);
void aac_cnt_config_toh(struct aac_cnt_config *ptr);
void aac_ctcfg_resp_toh(struct aac_ctcfg_resp *ptr);
void aac_getbusinf_toh(struct aac_getbusinf *ptr);
void aac_vmi_businf_resp_toh(struct aac_vmi_businf_resp *ptr);
void aac_srb_response_toh(struct aac_srb_response *ptr);

/* Convert from host order to Little-Endian (TOLE) */
void aac_adapter_init_tole(struct aac_adapter_init *ptr);
void aac_fib_header_tole(struct aac_fib_header *ptr);
void aac_mntinfo_tole(struct aac_mntinfo *ptr);
void aac_fsa_ctm_tole(struct aac_fsa_ctm *ptr);
void aac_cnt_config_tole(struct aac_cnt_config *ptr);
void aac_raw_io_tole(struct aac_raw_io *ptr);
void aac_raw_io2_tole(struct aac_raw_io2 *ptr);
void aac_fib_xporthdr_tole(struct aac_fib_xporthdr *ptr);
void aac_ctcfg_tole(struct aac_ctcfg *ptr);
void aac_vmioctl_tole(struct aac_vmioctl *ptr);
void aac_pause_command_tole(struct aac_pause_command *ptr);
void aac_srb_tole(struct aac_srb *ptr);
void aac_sge_ieee1212_tole(struct aac_sge_ieee1212 *ptr);
void aac_sg_entryraw_tole(struct aac_sg_entryraw *ptr);
void aac_sg_entry_tole(struct aac_sg_entry *ptr);
void aac_sg_entry64_tole(struct aac_sg_entry64 *ptr);
void aac_blockread_tole(struct aac_blockread *ptr);
void aac_blockwrite_tole(struct aac_blockwrite *ptr);
void aac_blockread64_tole(struct aac_blockread64 *ptr);
void aac_blockwrite64_tole(struct aac_blockwrite64 *ptr);

#endif

#endif
