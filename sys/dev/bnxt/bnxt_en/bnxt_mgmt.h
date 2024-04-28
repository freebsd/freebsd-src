/*
 * Broadcom NetXtreme-C/E network driver.
 *
 * Copyright (c) 2022 Broadcom, All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS'
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

#include "bnxt.h"
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/param.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/uio.h>
#include <sys/malloc.h>


#define	DRIVER_NAME				"if_bnxt"

#define	BNXT_MGMT_OPCODE_GET_DEV_INFO		0x80000000
#define	BNXT_MGMT_OPCODE_PASSTHROUGH_HWRM	0x80000001
#define	BNXT_MGMT_OPCODE_DCB_OPS		0x80000002

#define BNXT_MGMT_MAX_HWRM_REQ_LENGTH		HWRM_MAX_REQ_LEN
#define BNXT_MGMT_MAX_HWRM_RESP_LENGTH		(512)

struct bnxt_nic_info {
#define BNXT_MAX_STR 64
	char dev_name[BNXT_MAX_STR];
	char driver_version[BNXT_MAX_STR];
	char driver_name[BNXT_MAX_STR];
	char device_serial_number[64];
	uint32_t mtu;
	uint8_t mac[ETHER_ADDR_LEN];
	uint32_t pci_link_speed;
	uint32_t pci_link_width;
	uint32_t rsvd[4];
} __packed;

struct bnxt_pci_info {
        uint16_t domain_no;
        uint16_t bus_no;
        uint16_t device_no;
        uint16_t function_no;
        uint16_t vendor_id;
        uint16_t device_id;
        uint16_t sub_system_vendor_id;
        uint16_t sub_system_device_id;
        uint16_t revision;
        uint32_t chip_rev_id;
	uint32_t rsvd[2];
} __packed;

struct bnxt_dev_info {
        struct bnxt_nic_info nic_info; 
        struct bnxt_pci_info pci_info;
} __packed;

struct dma_info {
        uint64_t data;
        uint32_t length;
        uint16_t offset;
        uint8_t read_or_write;
        uint8_t unused;
};

struct bnxt_mgmt_fw_msg {
        uint64_t usr_req;
        uint64_t usr_resp;
        uint32_t len_req;
        uint32_t len_resp;
        uint32_t timeout;
        uint32_t num_dma_indications;
        struct dma_info dma[0];
};

struct bnxt_mgmt_generic_msg {
        uint8_t key;
#define BNXT_LFC_KEY_DOMAIN_NO  1
        uint8_t reserved[3];
        uint32_t value;
};

enum bnxt_mgmt_req_type {
        BNXT_MGMT_NVM_GET_VAR_REQ = 1,
        BNXT_MGMT_NVM_SET_VAR_REQ,
        BNXT_MGMT_NVM_FLUSH_REQ,
        BNXT_MGMT_GENERIC_HWRM_REQ,
};

struct bnxt_mgmt_req_hdr {
        uint32_t ver;
	uint32_t domain;
        uint32_t bus;
        uint32_t devfn;
        enum bnxt_mgmt_req_type req_type;
};

struct bnxt_mgmt_req {
	struct bnxt_mgmt_req_hdr hdr;
	union {
		uint64_t hreq;
	} req;
};

struct bnxt_mgmt_app_tlv {
	uint32_t num_app;
	struct bnxt_dcb_app app[128];
} __attribute__ ((__packed__));

struct bnxt_mgmt_dcb {
	struct bnxt_mgmt_req_hdr hdr;
#define BNXT_MGMT_DCB_GET_ETS	0x1
#define BNXT_MGMT_DCB_SET_ETS	0x2
#define BNXT_MGMT_DCB_GET_PFC	0x3
#define BNXT_MGMT_DCB_SET_PFC	0x4
#define BNXT_MGMT_DCB_SET_APP	0x5
#define BNXT_MGMT_DCB_DEL_APP	0x6
#define BNXT_MGMT_DCB_LIST_APP	0x7
#define BNXT_MGMT_DCB_MAX	BNXT_MGMT_DCB_LIST_APP
	uint32_t op;
	union {
		struct bnxt_ieee_ets ets;
		struct bnxt_ieee_pfc pfc;
		struct bnxt_mgmt_app_tlv app_tlv;
	} req;
} __attribute__ ((__packed__));
