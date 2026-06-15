/*-
 * Broadcom NetXtreme-C/E network driver.
 *
 * Copyright (c) 2026 Broadcom Inc. All Rights Reserved.
 * The term Broadcom refers to Broadcom Limited and/or its subsidiaries
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _BNXT_COREDUMP_H
#define _BNXT_COREDUMP_H

#define BNXT_CRASH_DUMP_LEN	0x400000	/* 4MB default for SOC DDR */

/* Capability detection */
void bnxt_hwrm_dbg_qcaps(struct bnxt_softc *softc);

/* Memory management */
int bnxt_alloc_crash_dump_mem(struct bnxt_softc *softc);
void bnxt_free_crash_dump_mem(struct bnxt_softc *softc);
int bnxt_hwrm_crash_dump_mem_cfg(struct bnxt_softc *softc);
int bnxt_hwrm_get_dump_len(struct bnxt_softc *softc, uint16_t dump_type,
			   uint32_t *dump_len);

/* Coredump retrieval */
uint32_t bnxt_get_coredump_length(struct bnxt_softc *softc, uint16_t dump_type);
int bnxt_get_coredump(struct bnxt_softc *softc, uint16_t dump_type, void *buf,
		      uint32_t *dump_len);


#endif /* _BNXT_COREDUMP_H */
