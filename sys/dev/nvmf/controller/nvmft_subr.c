/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023-2024 Chelsio Communications, Inc.
 * Written by: John Baldwin <jhb@FreeBSD.org>
 */

#include <sys/types.h>
#ifdef _KERNEL
#include <sys/libkern.h>
#else
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#endif

#include <dev/nvmf/nvmf_proto.h>

#include "nvmft_subr.h"

bool
nvmf_nqn_valid(const char *nqn)
{
	size_t len;

	len = strnlen(nqn, NVME_NQN_FIELD_SIZE);
	if (len == 0 || len > NVMF_NQN_MAX_LEN)
		return (false);
	return (true);
}

uint64_t
_nvmf_controller_cap(uint32_t max_io_qsize, uint8_t enable_timeout)
{
	uint32_t caphi, caplo;
	u_int mps;

	caphi = NVMEF(NVME_CAP_HI_REG_CMBS, 0) |
	    NVMEF(NVME_CAP_HI_REG_PMRS, 0);
	if (max_io_qsize != 0) {
		mps = ffs(PAGE_SIZE) - 1;
		if (mps < NVME_MPS_SHIFT)
			mps = 0;
		else
			mps -= NVME_MPS_SHIFT;
		caphi |= NVMEF(NVME_CAP_HI_REG_MPSMAX, mps) |
		    NVMEF(NVME_CAP_HI_REG_MPSMIN, mps);
	}
	caphi |= NVMEF(NVME_CAP_HI_REG_BPS, 0) |
	    NVMEF(NVME_CAP_HI_REG_CSS, NVME_CAP_HI_REG_CSS_NVM_MASK) |
	    NVMEF(NVME_CAP_HI_REG_NSSRS, 0) |
	    NVMEF(NVME_CAP_HI_REG_DSTRD, 0);

	caplo = NVMEF(NVME_CAP_LO_REG_TO, enable_timeout) |
	    NVMEF(NVME_CAP_LO_REG_AMS, 0) |
	    NVMEF(NVME_CAP_LO_REG_CQR, 1);

	if (max_io_qsize != 0)
		caplo |= NVMEF(NVME_CAP_LO_REG_MQES, max_io_qsize - 1);

	return ((uint64_t)caphi << 32 | caplo);
}

bool
_nvmf_validate_cc(uint32_t max_io_qsize __unused, uint64_t cap, uint32_t old_cc,
    uint32_t new_cc)
{
	uint32_t caphi, changes, field;

	changes = old_cc ^ new_cc;
	field = NVMEV(NVME_CC_REG_IOCQES, new_cc);
	if (field != 0) {
		/*
		 * XXX: Linux's initiator writes a non-zero value to
		 * IOCQES when connecting to a discovery controller.
		 */
#ifdef STRICT_CHECKS
		if (max_io_qsize == 0)
			return (false);
#endif
		if (field != 4)
			return (false);
	}
	field = NVMEV(NVME_CC_REG_IOSQES, new_cc);
	if (field != 0) {
		/*
		 * XXX: Linux's initiator writes a non-zero value to
		 * IOCQES when connecting to a discovery controller.
		 */
#ifdef STRICT_CHECKS
		if (max_io_qsize == 0)
			return (false);
#endif
		if (field != 6)
			return (false);
	}
	field = NVMEV(NVME_CC_REG_SHN, new_cc);
	if (field == 3)
		return (false);

	field = NVMEV(NVME_CC_REG_AMS, new_cc);
	if (field != 0)
		return (false);

	caphi = cap >> 32;
	field = NVMEV(NVME_CC_REG_MPS, new_cc);
	if (field < NVMEV(NVME_CAP_HI_REG_MPSMAX, caphi) ||
	    field > NVMEV(NVME_CAP_HI_REG_MPSMIN, caphi))
		return (false);

	field = NVMEV(NVME_CC_REG_CSS, new_cc);
	if (field != 0 && field != 0x7)
		return (false);

	/* AMS, MPS, and CSS can only be changed while CC.EN is 0. */
	if (NVMEV(NVME_CC_REG_EN, old_cc) != 0 &&
	    (NVMEV(NVME_CC_REG_AMS, changes) != 0 ||
	    NVMEV(NVME_CC_REG_MPS, changes) != 0 ||
	    NVMEV(NVME_CC_REG_CSS, changes) != 0))
		return (false);

	return (true);
}

void
nvmf_controller_serial(char *buf, size_t len, u_long hostid)
{
	snprintf(buf, len, "HI:%lu", hostid);
}

void
nvmf_strpad(char *dst, const char *src, size_t len)
{
	while (len > 0 && *src != '\0') {
		*dst++ = *src++;
		len--;
	}
	memset(dst, ' ', len);
}

void
_nvmf_init_io_controller_data(uint16_t cntlid, uint32_t max_io_qsize,
    const char *serial, const char *model, const char *firmware_version,
    const char *subnqn, int nn, uint32_t ioccsz, uint32_t iorcsz,
    struct nvme_controller_data *cdata)
{
	char *cp;

	nvmf_strpad(cdata->sn, serial, sizeof(cdata->sn));
	nvmf_strpad(cdata->mn, model, sizeof(cdata->mn));
	nvmf_strpad(cdata->fr, firmware_version, sizeof(cdata->fr));
	cp = memchr(cdata->fr, '-', sizeof(cdata->fr));
	if (cp != NULL)
		memset(cp, ' ', sizeof(cdata->fr) - (cp - (char *)cdata->fr));

	/* FreeBSD OUI */
	cdata->ieee[0] = 0xfc;
	cdata->ieee[1] = 0x9c;
	cdata->ieee[2] = 0x58;

	cdata->ctrlr_id = htole16(cntlid);
	cdata->ver = htole32(NVME_REV(1, 4));
	cdata->ctratt = htole32(
	    NVMEF(NVME_CTRLR_DATA_CTRATT_128BIT_HOSTID, 1) |
	    NVMEF(NVME_CTRLR_DATA_CTRATT_TBKAS, 1));
	cdata->cntrltype = 1;
	cdata->acl = 3;
	cdata->aerl = 3;

	/* 1 read-only firmware slot */
	cdata->frmw = NVMEF(NVME_CTRLR_DATA_FRMW_SLOT1_RO, 1) |
	    NVMEF(NVME_CTRLR_DATA_FRMW_NUM_SLOTS, 1);

	cdata->lpa = NVMEF(NVME_CTRLR_DATA_LPA_EXT_DATA, 1);

	/* Single power state */
	cdata->npss = 0;

	/*
	 * 1.2+ require a non-zero value for these even though it makes
	 * no sense for Fabrics.
	 */
	cdata->wctemp = htole16(0x0157);
	cdata->cctemp = cdata->wctemp;

	/* 1 second granularity for KeepAlive */
	cdata->kas = htole16(10);

	cdata->sqes = NVMEF(NVME_CTRLR_DATA_SQES_MAX, 6) |
	    NVMEF(NVME_CTRLR_DATA_SQES_MIN, 6);
	cdata->cqes = NVMEF(NVME_CTRLR_DATA_CQES_MAX, 4) |
	    NVMEF(NVME_CTRLR_DATA_CQES_MIN, 4);

	cdata->maxcmd = htole16(max_io_qsize);
	cdata->nn = htole32(nn);

	cdata->vwc =
	    NVMEF(NVME_CTRLR_DATA_VWC_ALL, NVME_CTRLR_DATA_VWC_ALL_NO) |
	    NVMEM(NVME_CTRLR_DATA_VWC_PRESENT);

	/* Transport-specific? */
	cdata->sgls = htole32(
	    NVMEF(NVME_CTRLR_DATA_SGLS_TRANSPORT_DATA_BLOCK, 1) |
	    NVMEF(NVME_CTRLR_DATA_SGLS_ADDRESS_AS_OFFSET, 1) |
	    NVMEF(NVME_CTRLR_DATA_SGLS_NVM_COMMAND_SET, 1));

	strlcpy(cdata->subnqn, subnqn, sizeof(cdata->subnqn));

	cdata->ioccsz = htole32(ioccsz / 16);
	cdata->iorcsz = htole32(iorcsz / 16);

	/* Transport-specific? */
	cdata->icdoff = 0;

	cdata->fcatt = 0;

	/* Transport-specific? */
	cdata->msdbd = 1;
}
