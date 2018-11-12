/*-
 * Copyright (c) 2015 Solarflare Communications Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are
 * those of the authors and should not be interpreted as representing official
 * policies, either expressed or implied, of the FreeBSD Project.
 *
 * $FreeBSD$
 */

#ifndef	_SYS_EF10_IMPL_H
#define	_SYS_EF10_IMPL_H

#ifdef	__cplusplus
extern "C" {
#endif

#if (EFSYS_OPT_HUNTINGTON && EFSYS_OPT_MEDFORD)
#define	EF10_MAX_PIOBUF_NBUFS	MAX(HUNT_PIOBUF_NBUFS, MEDFORD_PIOBUF_NBUFS)
#elif EFSYS_OPT_HUNTINGTON
#define	EF10_MAX_PIOBUF_NBUFS	HUNT_PIOBUF_NBUFS
#elif EFSYS_OPT_MEDFORD
#define	EF10_MAX_PIOBUF_NBUFS	MEDFORD_PIOBUF_NBUFS
#endif

extern	__checkReturn	efx_rc_t
efx_mcdi_get_port_assignment(
	__in		efx_nic_t *enp,
	__out		uint32_t *portp);

extern	__checkReturn	efx_rc_t
efx_mcdi_get_port_modes(
	__in		efx_nic_t *enp,
	__out		uint32_t *modesp);

extern	__checkReturn	efx_rc_t
efx_mcdi_get_mac_address_pf(
	__in			efx_nic_t *enp,
	__out_ecount_opt(6)	uint8_t mac_addrp[6]);

extern	__checkReturn	efx_rc_t
efx_mcdi_get_mac_address_vf(
	__in			efx_nic_t *enp,
	__out_ecount_opt(6)	uint8_t mac_addrp[6]);

extern	__checkReturn	efx_rc_t
efx_mcdi_get_clock(
	__in		efx_nic_t *enp,
	__out		uint32_t *sys_freqp);

extern	__checkReturn	efx_rc_t
efx_mcdi_get_vector_cfg(
	__in		efx_nic_t *enp,
	__out_opt	uint32_t *vec_basep,
	__out_opt	uint32_t *pf_nvecp,
	__out_opt	uint32_t *vf_nvecp);

extern	__checkReturn	efx_rc_t
ef10_get_datapath_caps(
	__in		efx_nic_t *enp);

extern	__checkReturn		efx_rc_t
ef10_get_privilege_mask(
	__in			efx_nic_t *enp,
	__out			uint32_t *maskp);

extern	__checkReturn	efx_rc_t
ef10_external_port_mapping(
	__in		efx_nic_t *enp,
	__in		uint32_t port,
	__out		uint8_t *external_portp);


#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_EF10_IMPL_H */
