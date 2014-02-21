/*-
 * Copyright 2009 Solarflare Communications Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS AND
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

#ifndef _SYS_EFX_MCDI_H
#define	_SYS_EFX_MCDI_H

#include "efx.h"
#include "efx_regs.h"
#include "efx_regs_mcdi.h"

#ifdef	__cplusplus
extern "C" {
#endif

/* Number of retries attempted for init code */
#define	EFX_MCDI_REQ_RETRY_INIT 2

struct efx_mcdi_req_s {
	/* Inputs: Command #, input buffer and length */
	unsigned int	emr_cmd;
	uint8_t		*emr_in_buf;
	size_t		emr_in_length;
	/* Outputs: retcode, buffer, length, and length used*/
	int		emr_rc;
	uint8_t		*emr_out_buf;
	size_t		emr_out_length;
	size_t		emr_out_length_used;
};

typedef struct efx_mcdi_iface_s {
	const efx_mcdi_transport_t *emi_mtp;
	unsigned int		emi_port;
	unsigned int		emi_seq;
	efx_mcdi_req_t		*emi_pending_req;
	boolean_t		emi_ev_cpl;
	int			emi_aborted;
	uint32_t		emi_poll_cnt;
} efx_mcdi_iface_t;

extern			void
efx_mcdi_execute(
	__in		efx_nic_t *enp,
	__in		efx_mcdi_req_t *emrp);

extern			void
efx_mcdi_ev_cpl(
	__in		efx_nic_t *enp,
	__in		unsigned int seq,
	__in		unsigned int outlen,
	__in		int errcode);

extern			void
efx_mcdi_ev_death(
	__in		efx_nic_t *enp,
	__in		int rc);

typedef enum efx_mcdi_boot_e {
	EFX_MCDI_BOOT_PRIMARY,
	EFX_MCDI_BOOT_SECONDARY,
	EFX_MCDI_BOOT_ROM,
} efx_mcdi_boot_t;

extern	__checkReturn		int
efx_mcdi_version(
	__in			efx_nic_t *enp,
	__out_ecount_opt(4)	uint16_t versionp[4],
	__out_opt		uint32_t *buildp,
	__out_opt		efx_mcdi_boot_t *statusp);

#define	MCDI_IN(_emr, _type, _ofst)					\
	((_type *)((_emr).emr_in_buf + (_ofst)))

#define	MCDI_IN2(_emr, _type, _ofst)					\
	MCDI_IN(_emr, _type, MC_CMD_ ## _ofst ## _OFST)

#define	MCDI_IN_SET_BYTE(_emr, _ofst, _value)				\
	EFX_POPULATE_BYTE_1(*MCDI_IN2(_emr, efx_byte_t, _ofst),		\
		EFX_BYTE_0, _value)

#define	MCDI_IN_SET_DWORD(_emr, _ofst, _value)				\
	EFX_POPULATE_DWORD_1(*MCDI_IN2(_emr, efx_dword_t, _ofst),	\
		EFX_DWORD_0, _value)

#define	MCDI_IN_POPULATE_DWORD_1(_emr, _ofst, _field1, _value1)		\
	EFX_POPULATE_DWORD_1(*MCDI_IN2(_emr, efx_dword_t, _ofst),	\
		MC_CMD_ ## _field1, _value1)

#define	MCDI_IN_POPULATE_DWORD_2(_emr, _ofst, _field1, _value1,		\
		_field2, _value2)					\
	EFX_POPULATE_DWORD_2(*MCDI_IN2(_emr, efx_dword_t, _ofst),	\
		MC_CMD_ ## _field1, _value1,				\
		MC_CMD_ ## _field2, _value2)

#define	MCDI_IN_POPULATE_DWORD_3(_emr, _ofst, _field1, _value1,		\
		_field2, _value2, _field3, _value3)			\
	EFX_POPULATE_DWORD_3(*MCDI_IN2(_emr, efx_dword_t, _ofst),	\
		MC_CMD_ ## _field1, _value1,				\
		MC_CMD_ ## _field2, _value2,				\
		MC_CMD_ ## _field3, _value3)

#define	MCDI_IN_POPULATE_DWORD_4(_emr, _ofst, _field1, _value1,		\
		_field2, _value2, _field3, _value3, _field4, _value4)	\
	EFX_POPULATE_DWORD_4(*MCDI_IN2(_emr, efx_dword_t, _ofst),	\
		MC_CMD_ ## _field1, _value1,				\
		MC_CMD_ ## _field2, _value2,				\
		MC_CMD_ ## _field3, _value3,				\
		MC_CMD_ ## _field4, _value4)

#define	MCDI_IN_POPULATE_DWORD_5(_emr, _ofst, _field1, _value1,		\
		_field2, _value2, _field3, _value3, _field4, _value4,	\
		_field5, _value5)					\
	EFX_POPULATE_DWORD_5(*MCDI_IN2(_emr, efx_dword_t, _ofst),	\
		MC_CMD_ ## _field1, _value1,				\
		MC_CMD_ ## _field2, _value2,				\
		MC_CMD_ ## _field3, _value3,				\
		MC_CMD_ ## _field4, _value4,				\
		MC_CMD_ ## _field5, _value5)

#define	MCDI_IN_POPULATE_DWORD_6(_emr, _ofst, _field1, _value1,		\
		_field2, _value2, _field3, _value3, _field4, _value4,	\
		_field5, _value5, _field6, _value6)			\
	EFX_POPULATE_DWORD_6(*MCDI_IN2(_emr, efx_dword_t, _ofst),	\
		MC_CMD_ ## _field1, _value1,				\
		MC_CMD_ ## _field2, _value2,				\
		MC_CMD_ ## _field3, _value3,				\
		MC_CMD_ ## _field4, _value4,				\
		MC_CMD_ ## _field5, _value5,				\
		MC_CMD_ ## _field6, _value6)

#define	MCDI_IN_POPULATE_DWORD_7(_emr, _ofst, _field1, _value1,		\
		_field2, _value2, _field3, _value3, _field4, _value4,	\
		_field5, _value5, _field6, _value6, _field7, _value7)	\
	EFX_POPULATE_DWORD_7(MCDI_IN2(_emr, efx_dword_t, _ofst),	\
		MC_CMD_ ## _field1, _value1,				\
		MC_CMD_ ## _field2, _value2,				\
		MC_CMD_ ## _field3, _value3,				\
		MC_CMD_ ## _field4, _value4,				\
		MC_CMD_ ## _field5, _value5,				\
		MC_CMD_ ## _field6, _value6,				\
		MC_CMD_ ## _field7, _value7)

#define	MCDI_IN_POPULATE_DWORD_8(_emr, _ofst, _field1, _value1,		\
		_field2, _value2, _field3, _value3, _field4, _value4,	\
		_field5, _value5, _field6, _value6, _field7, _value7,	\
		_field8, _value8)					\
	EFX_POPULATE_DWORD_8(*MCDI_IN2(_emr, efx_dword_t, _ofst),	\
		MC_CMD_ ## _field1, _value1,				\
		MC_CMD_ ## _field2, _value2,				\
		MC_CMD_ ## _field3, _value3,				\
		MC_CMD_ ## _field4, _value4,				\
		MC_CMD_ ## _field5, _value5,				\
		MC_CMD_ ## _field6, _value6,				\
		MC_CMD_ ## _field7, _value7,				\
		MC_CMD_ ## _field8, _value8)

#define	MCDI_IN_POPULATE_DWORD_9(_emr, _ofst, _field1, _value1,		\
		_field2, _value2, _field3, _value3, _field4, _value4,	\
		_field5, _value5, _field6, _value6, _field7, _value7,	\
		_field8, _value8, _field9, _value9)			\
	EFX_POPULATE_DWORD_9(*MCDI_IN2(_emr, efx_dword_t, _ofst),	\
		MC_CMD_ ## _field1, _value1,				\
		MC_CMD_ ## _field2, _value2,				\
		MC_CMD_ ## _field3, _value3,				\
		MC_CMD_ ## _field4, _value4,				\
		MC_CMD_ ## _field5, _value5,				\
		MC_CMD_ ## _field6, _value6,				\
		MC_CMD_ ## _field7, _value7,				\
		MC_CMD_ ## _field8, _value8,				\
		MC_CMD_ ## _field9, _value9)

#define	MCDI_IN_POPULATE_DWORD_10(_emr, _ofst, _field1, _value1,	\
		_field2, _value2, _field3, _value3, _field4, _value4,	\
		_field5, _value5, _field6, _value6, _field7, _value7,	\
		_field8, _value8, _field9, _value9, _field10, _value10)	\
	EFX_POPULATE_DWORD_10(*MCDI_IN2(_emr, efx_dword_t, _ofst),	\
		MC_CMD_ ## _field1, _value1,				\
		MC_CMD_ ## _field2, _value2,				\
		MC_CMD_ ## _field3, _value3,				\
		MC_CMD_ ## _field4, _value4,				\
		MC_CMD_ ## _field5, _value5,				\
		MC_CMD_ ## _field6, _value6,				\
		MC_CMD_ ## _field7, _value7,				\
		MC_CMD_ ## _field8, _value8,				\
		MC_CMD_ ## _field9, _value9,				\
		MC_CMD_ ## _field10, _value10)

#define	MCDI_OUT(_emr, _type, _ofst)					\
	((_type *)((_emr).emr_out_buf + (_ofst)))

#define	MCDI_OUT2(_emr, _type, _ofst)					\
	MCDI_OUT(_emr, _type, MC_CMD_ ## _ofst ## _OFST)

#define	MCDI_OUT_BYTE(_emr, _ofst)					\
	EFX_BYTE_FIELD(*MCDI_OUT2(_emr, efx_byte_t, _ofst),		\
		    EFX_BYTE_0)

#define	MCDI_OUT_WORD(_emr, _ofst)					\
	EFX_WORD_FIELD(*MCDI_OUT2(_emr, efx_word_t, _ofst),		\
		    EFX_WORD_0)

#define	MCDI_OUT_DWORD(_emr, _ofst)					\
	EFX_DWORD_FIELD(*MCDI_OUT2(_emr, efx_dword_t, _ofst),		\
			EFX_DWORD_0)

#define	MCDI_OUT_DWORD_FIELD(_emr, _ofst, _field)			\
	EFX_DWORD_FIELD(*MCDI_OUT2(_emr, efx_dword_t, _ofst),		\
			MC_CMD_ ## _field)

#define	MCDI_EV_FIELD(_eqp, _field)					\
	EFX_QWORD_FIELD(*eqp, MCDI_EVENT_ ## _field)

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_EFX_MCDI_H */
