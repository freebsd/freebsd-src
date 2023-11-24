/*-
 * SPDX-License-Identifier: BSD-2-Clause
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
 */

#ifndef	__QCOM_SCM_DEFS_H__
#define	__QCOM_SCM_DEFS_H__

/*
 * Maximum SCM arguments and return values.
 */
#define	MAX_QCOM_SCM_ARGS			10
#define	MAX_QCOM_SCM_RETS			3

/*
 * SCM argument type definitions.
 */
#define	QCOM_SCM_ARGTYPE_VAL			0x00
#define	QCOM_SCM_ARGTYPE_RO			0x01
#define	QCOM_SCM_ARGTYPE_RW			0x02
#define	QCOM_SCM_ARGTYPE_BUFVAL			0x03

/*
 * SCM calls + arguments.
 */
#define	QCOM_SCM_SVC_BOOT			0x01
#define		QCOM_SCM_BOOT_SET_ADDR		0x01
#define		QCOM_SCM_BOOT_TERMINATE_PC	0x02
#define		QCOM_SCM_BOOT_SET_DLOAD_MODE	0x10
#define		QCOM_SCM_BOOT_SET_REMOTE_STATE	0x0a
#define		QCOM_SCM_FLUSH_FLAG_MASK	0x3

/* Flags for QCOM_SCM_BOOT_SET_ADDR argv[0] */
/* Note: no COLDBOOT for CPU0, it's already booted */
#define		QCOM_SCM_FLAG_COLDBOOT_CPU1	0x01
#define		QCOM_SCM_FLAG_WARMBOOT_CPU1	0x02
#define		QCOM_SCM_FLAG_WARMBOOT_CPU0	0x04
#define		QCOM_SCM_FLAG_COLDBOOT_CPU2	0x08
#define		QCOM_SCM_FLAG_WARMBOOT_CPU2	0x10
#define		QCOM_SCM_FLAG_COLDBOOT_CPU3	0x20
#define		QCOM_SCM_FLAG_WARMBOOT_CPU3	0x40

#define	QCOM_SCM_SVC_PIL			0x02
#define		QCOM_SCM_PIL_PAS_INIT_IMAGE	0x01
#define		QCOM_SCM_PIL_PAS_MEM_SETUP	0x02
#define		QCOM_SCM_PIL_PAS_AUTH_AND_RESET	0x05
#define		QCOM_SCM_PIL_PAS_SHUTDOWN	0x06
#define		QCOM_SCM_PIL_PAS_IS_SUPPORTED	0x07
#define		QCOM_SCM_PIL_PAS_MSS_RESET	0x0a

#define	QCOM_SCM_SVC_IO				0x05
#define		QCOM_SCM_IO_READ		0x01
#define		QCOM_SCM_IO_WRITE		0x02

/*
 * Fetch SCM call availability information.
 */
#define	QCOM_SCM_SVC_INFO			0x06
#define		QCOM_SCM_INFO_IS_CALL_AVAIL	0x01

#define	QCOM_SCM_SVC_MP				0x0c
#define		QCOM_SCM_MP_RESTORE_SEC_CFG	0x02
#define		QCOM_SCM_MP_IOMMU_SECURE_PTBL_SIZE	0x03
#define		QCOM_SCM_MP_IOMMU_SECURE_PTBL_INIT	0x04
#define		QCOM_SCM_MP_VIDEO_VAR		0x08
#define		QCOM_SCM_MP_ASSIGN		0x16

#define	QCOM_SCM_SVC_OCMEM			0x0f
#define		QCOM_SCM_OCMEM_LOCK_CMD		0x01
#define		QCOM_SCM_OCMEM_UNLOCK_CMD	0x02

#define	QCOM_SCM_SVC_ES				0x10
#define		QCOM_SCM_ES_INVALIDATE_ICE_KEY	0x03
#define		QCOM_SCM_ES_CONFIG_SET_ICE_KEY	0x04

#define	QCOM_SCM_SVC_HDCP			0x11
#define		QCOM_SCM_HDCP_INVOKE		0x01

#define	QCOM_SCM_SVC_LMH			0x13
#define		QCOM_SCM_LMH_LIMIT_PROFILE_CHANGE	0x01
#define		QCOM_SCM_LMH_LIMIT_DCVSH	0x10

#define	QCOM_SCM_SVC_SMMU_PROGRAM		0x15
#define		QCOM_SCM_SMMU_CONFIG_ERRATA1	0x03
#define		QCOM_SCM_SMMU_CONFIG_ERRATA1_CLIENT_ALL	0x02

/*
 * Return values from the SCM calls.
 */
#define	QCOM_SCM_RETVAL_V2_EBUSY		-12
#define	QCOM_SCM_RETVAL_ENOMEM			-5
#define	QCOM_SCM_RETVAL_EOPNOTSUPP		-4
#define	QCOM_SCM_RETVAL_EINVAL_ADDR		-3
#define	QCOM_SCM_RETVAL_EINVAL_ARG		-2
#define	QCOM_SCM_RETVAL_ERROR			-1
#define	QCOM_SCM_RETVAL_INTERRUPTED		1

#endif	/* __QCOM_SCM_DEFS_H__ */
