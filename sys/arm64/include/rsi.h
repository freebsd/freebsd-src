/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Arm Ltd
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

#ifndef _MACHINE_RSI_H_
#define	_MACHINE_RSI_H_

extern uint64_t prot_ns_shared;

bool in_realm(void);

void arm64_rsi_setup_memory(void);

/*
 * The major version number of the RSI implementation.  This is increased when
 * the binary format or semantics of the SMC calls change.
 */
#define RSI_ABI_VERSION_MAJOR	UL(1)

/*
 * The minor version number of the RSI implementation.  This is increased when
 * a bug is fixed, or a feature is added without breaking binary compatibility.
 */
#define RSI_ABI_VERSION_MINOR	UL(0)

#define RSI_ABI_VERSION		((RSI_ABI_VERSION_MAJOR << 16) | \
				    RSI_ABI_VERSION_MINOR)

#define RSI_ABI_VERSION_GET_MAJOR(_version)	((_version) >> 16)
#define RSI_ABI_VERSION_GET_MINOR(_version)	((_version) & 0xFFFF)

#define RSI_SUCCESS		UL(0)
#define RSI_ERROR_INPUT		UL(1)
#define RSI_ERROR_STATE		UL(2)
#define RSI_INCOMPLETE		UL(3)
#define RSI_ERROR_UNKNOWN	UL(4)

#define SMC_RSI_FID(n)		SMCCC_FUNC_ID(SMCCC_FAST_CALL,          \
				    SMCCC_64BIT_CALL,                   \
				    SMCCC_STD_SECURE_SERVICE_CALLS,     \
				    n)

/*
 * Returns RSI version.
 *
 * arg1 == Requested interface revision
 * ret0 == Status / error
 * ret1 == Lower implemented interface revision
 * ret2 == Higher implemented interface revision
 */
#define SMC_RSI_ABI_VERSION	SMC_RSI_FID(0x190)

/*
 * Read configuration for the current Realm.
 *
 * arg1 == struct realm_config addr
 * ret0 == Status / error
 */
#define SMC_RSI_REALM_CONFIG	SMC_RSI_FID(0x196)

struct realm_config {
	union {
		struct {
			unsigned long ipa_bits; /* Width of IPA in bits */
			unsigned long hash_algo; /* Hash algorithm */
		};
		uint8_t pad[0x200];
	};
	union {
		uint8_t rpv[64]; /* Realm Personalization Value */
		uint8_t pad2[0xe00];
	};
	/*
	 * The RMM requires the configuration structure to be aligned to a 4k
	 * boundary, ensure this happens by aligning this structure.
	 */
} __aligned(0x1000);

/*
 * Request RIPAS of a target IPA range to be changed to a specified value.
 *
 * arg1 == Base IPA address of target region
 * arg2 == Top of the region
 * arg3 == RIPAS value
 * arg4 == flags
 * ret0 == Status / error
 * ret1 == Top of modified IPA range
 * ret2 == Whether the Host accepted or rejected the request
 */
#define SMC_RSI_IPA_STATE_SET	SMC_RSI_FID(0x197)

#define RSI_NO_CHANGE_DESTROYED	UL(0)
#define RSI_CHANGE_DESTROYED	UL(1)

#define RSI_ACCEPT		UL(0)
#define RSI_REJECT		UL(1)

enum ripas {
	RSI_RIPAS_EMPTY = 0,
	RSI_RIPAS_RAM
};

unsigned long rsi_set_addr_range_state(vm_paddr_t start, vm_paddr_t end,
    enum ripas state, unsigned long flags, vm_paddr_t *top);

#endif	/* !_MACHINE_RSI_H_ */
