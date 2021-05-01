/* SPDX-License-Identifier: BSD-2-Clause-NetBSD AND BSD-3-Clause */
/*	$NetBSD: qat_hw15var.h,v 1.1 2019/11/20 09:37:46 hikaru Exp $	*/

/*
 * Copyright (c) 2019 Internet Initiative Japan, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *   Copyright(c) 2007-2013 Intel Corporation. All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* $FreeBSD$ */

#ifndef _DEV_PCI_QAT_HW15VAR_H_
#define _DEV_PCI_QAT_HW15VAR_H_

CTASSERT(HASH_CONTENT_DESC_SIZE >=
   sizeof(struct fw_auth_hdr) + MAX_HASH_SETUP_BLK_SZ);
CTASSERT(CIPHER_CONTENT_DESC_SIZE >=
   sizeof(struct fw_cipher_hdr) + MAX_CIPHER_SETUP_BLK_SZ);
CTASSERT(CONTENT_DESC_MAX_SIZE >=
    roundup(HASH_CONTENT_DESC_SIZE + CIPHER_CONTENT_DESC_SIZE,
        QAT_OPTIMAL_ALIGN));
CTASSERT(QAT_SYM_REQ_PARAMS_SIZE_PADDED >=
    roundup(sizeof(struct fw_la_cipher_req_params) +
        sizeof(struct fw_la_auth_req_params), QAT_OPTIMAL_ALIGN));

/* length of the 5 long words of the request that are stored in the session
 * This is rounded up to 32 in order to use the fast memcopy function */
#define QAT_HW15_SESSION_REQ_CACHE_SIZE	(32)

void		qat_msg_req_type_populate(struct arch_if_req_hdr *,
		    enum arch_if_req, uint32_t);
void		qat_msg_cmn_hdr_populate(struct fw_la_bulk_req *, bus_addr_t,
		    uint8_t, uint8_t, uint16_t, uint32_t);
void		qat_msg_service_cmd_populate(struct fw_la_bulk_req *,
		    enum fw_la_cmd_id, uint16_t);
void		qat_msg_cmn_mid_populate(struct fw_comn_req_mid *, void *,
		    uint64_t , uint64_t);
void		qat_msg_req_params_populate(struct fw_la_bulk_req *, bus_addr_t,
		    uint8_t);
void		qat_msg_cmn_footer_populate(union fw_comn_req_ftr *, uint64_t);
void		qat_msg_params_populate(struct fw_la_bulk_req *,
		    struct qat_crypto_desc *, uint8_t, uint16_t,
		    uint16_t);


int		qat_adm_ring_init(struct qat_softc *);
int		qat_adm_ring_send_init(struct qat_softc *);

void		qat_hw15_crypto_setup_desc(struct qat_crypto *,
		    struct qat_session *, struct qat_crypto_desc *);
void		qat_hw15_crypto_setup_req_params(struct qat_crypto_bank *,
		    struct qat_session *, struct qat_crypto_desc const *,
		    struct qat_sym_cookie *, struct cryptop *);

#endif
