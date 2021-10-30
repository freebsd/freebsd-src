/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2021 Adrian Chadd <adrian@FreeBSD.org>
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

#ifndef	__QCOM_SCM_LEGACY_DEFS_H__
#define	__QCOM_SCM_LEGACY_DEFS_H__

/*
 * These definitions are specific to the 32 bit legacy SCM interface
 * used by the IPQ806x and IPQ401x SoCs.
 */

/*
 * Mapping of the SCM service/command fields into the a0 argument
 * in an SMC instruction call.
 *
 * This is particular to the legacy SCM interface, and is not the
 * same as the non-legacy 32/64 bit FNID mapping layout.
 */
#define	QCOM_SCM_LEGACY_SMC_FNID(s, c)		(((s) << 10) | ((c) & 0x3ff))

/*
 * There are two kinds of SCM calls in this legacy path.
 *
 * The first kind are the normal ones - up to a defined max of arguments,
 * a defined max of responses and some identifiers for all of it.
 * They can be issues in parallel on different cores, can be interrupted,
 * etc.
 *
 * The second kind are what are termed "atomic" SCM calls -
 * up to 5 argument DWORDs, up to 3 response DWORDs, done atomically,
 * not interruptable/parallel.
 *
 * The former use the structures below to represent the request and response
 * in memory.  The latter use defines and a direct SMC call with the
 * arguments in registers.
 */

struct qcom_scm_legacy_smc_args {
	uint32_t args[8];
};

/*
 * Atomic SCM call command/response buffer definitions.
 */
#define	QCOM_SCM_LEGACY_ATOMIC_MAX_ARGCOUNT		5
#define	QCOM_SCM_LEGACY_CLASS_REGISTER			(0x2 << 8)
#define	QCOM_SCM_LEGACY_MASK_IRQS			(1U << 5)

/*
 * Mapping an SCM service/command/argcount into the a0 register
 * for an SMC instruction call.
 */
#define	QCOM_SCM_LEGACY_ATOMIC_ID(svc, cmd, n) \
	    ((QCOM_SCM_LEGACY_SMC_FNID((svc), cmd) << 12) | \
	    QCOM_SCM_LEGACY_CLASS_REGISTER | \
	    QCOM_SCM_LEGACY_MASK_IRQS | \
	    ((n) & 0xf))

/*
 * Legacy command/response buffer definitions.
 *
 * The legacy path contains up to the defined maximum arguments
 * but only a single command/response pair per call.
 *
 * A command and response buffer is laid out in memory as such:
 *
 * | command header     |
 * | (buffer payload)   |
 * | response header    |
 * | (response payload) |
 */

/*
 * The command header.
 *
 * len - the length of the total command and response, including
 *       the headers.
 *
 * buf_offset - the offset inside the buffer, starting at the
 *       beginning of this command header, where the command buffer
 *       is found.  The end is the byte before the response_header_offset.
 *
 * response_header_offset - the offset inside the buffer where
 *       the response header is found.
 *
 * id - the QCOM_SCM_LEGACY_SMC_FNID() - service/command ids
 */
struct qcom_scm_legacy_command_header {
	uint32_t len;
	uint32_t buf_offset;
	uint32_t response_header_offset;
	uint32_t id;
};

/*
 * The response header.
 *
 * This is found immediately after the command header and command
 * buffer payload.
 *
 * len - the total amount of memory available for the response.
 *       Linux doesn't set this; it always passes in a response
 *       buffer large enough to store MAX_QCOM_SCM_RETS * DWORD
 *       bytes.
 *
 *       It's also possible this is set by the firmware.
 *
 * buf_offset - start of response buffer, relative to the beginning
 *       of the command header.  This also isn't set in Linux before
 *       calling the SMC instruction, but it is checked afterwards
 *       to assemble a pointer to the response data.  The firmware
 *       likely sets this.
 *
 * is_complete - true if complete.  Linux loops over DMA sync to
 *       check if this is complete even after the SMC call returns.
 */
struct qcom_scm_legacy_response_header {
	uint32_t len;
	uint32_t buf_offset;
	uint32_t is_complete;
};

#endif	/* __QCOM_SCM_LEGACY_DEFS_H__ */
