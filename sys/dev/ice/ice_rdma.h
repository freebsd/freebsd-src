/* SPDX-License-Identifier: BSD-3-Clause */
/*  Copyright (c) 2023, Intel Corporation
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 *   3. Neither the name of the Intel Corporation nor the names of its
 *      contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file ice_rdma.h
 * @brief header file for RDMA client interface functions
 *
 * Contains definitions and function calls shared by the ice driver and the
 * RDMA client interface driver.
 *
 * Since these definitions are shared between drivers it is important that any
 * changes are considered carefully for backwards compatibility.
 */
#ifndef _ICE_RDMA_H_
#define _ICE_RDMA_H_

/*
 * The RDMA client interface version is used to help determine
 * incompatibilities between the interface definition shared between the main
 * driver and the client driver.
 *
 * It will follows the semantic version guidelines, that is:
 * Given the version number MAJOR.MINOR.PATCH, increment the:
 *
 * MAJOR version when you make incompatible changes,
 * MINOR version when you add functionality in a backwards-compatible manner, and
 * PATCH version when you make backwards-compatible bug fixes.
 *
 * Any change to this file, or one of the kobject interface files must come
 * with an associated change in one of the MAJOR, MINOR, or PATCH versions,
 * and care must be taken that backwards incompatible changes MUST increment
 * the MAJOR version.
 *
 * Note: Until the MAJOR version is set to at least 1, the above semantic
 * version guarantees may not hold, and this interface should not be
 * considered stable.
 */
#define ICE_RDMA_MAJOR_VERSION 1
#define ICE_RDMA_MINOR_VERSION 1
#define ICE_RDMA_PATCH_VERSION 0

/**
 * @def ICE_RDMA_MAX_MSIX
 * @brief Maximum number of MSI-X vectors that will be reserved
 *
 * Defines the maximum number of MSI-X vectors that an RDMA interface will
 * have reserved in advance. Does not guarantee that many vectors have
 * actually been enabled.
 */
#define ICE_RDMA_MAX_MSIX 64

/**
 * @struct ice_rdma_info
 * @brief RDMA information from the client driver
 *
 * The RDMA client driver will fill in this structure and pass its contents
 * back to the main driver using the ice_rdma_register function.
 *
 * It should fill the version in with the ICE_RDMA_* versions as defined in
 * the ice_rdma.h header.
 *
 * Additionally it must provide a pointer to a kobject class which extends the
 * ice_rdma_di_class with the operations defined in the rdma_if.m interface.
 *
 * If the version specified is not compatible, then the registration will
 * of the RDMA driver will fail.
 *
 * @var ice_rdma_info::major_version
 * 	describe major changes in the interface
 * @var ice_rdma_info::minor_version
 * 	describe changes and fixes with backward compatibility
 * @var ice_rdma_info::patch_version
 * 	changes without impact on compatibility or features
 * @var ice_rdma_info::rdma_class
 * 	kobject class
 */
struct ice_rdma_info {
	uint16_t major_version;
	uint16_t minor_version;
	uint16_t patch_version;

	kobj_class_t rdma_class;
};

#define ICE_RDMA_MAX_USER_PRIORITY	8
#define ICE_RDMA_MAX_MSIX		64

/* Declare the ice_rdma_di kobject class */
DECLARE_CLASS(ice_rdma_di_class);

/**
 * @struct ice_rdma_msix_mapping
 * @brief MSI-X mapping requested by the peer RDMA driver
 *
 * Defines a mapping for MSI-X vectors being requested by the peer RDMA driver
 * for a given PF.
 *
 */
struct ice_rdma_msix_mapping {
	uint8_t itr_indx;
	int aeq_vector;
	int ceq_cnt;
	int *ceq_vector;
};

/**
 * @struct ice_rdma_msix
 * @brief RDMA MSI-X vectors reserved for the peer RDMA driver
 *
 * Defines the segment of the MSI-X vectors for use by the RDMA driver. These
 * are reserved by the PF when it initializes.
 */
struct ice_rdma_msix {
	int base;
	int count;
};

/**
 * @struct ice_qos_info
 * @brief QoS information to be shared with RDMA driver
 */
struct ice_qos_info {
	uint64_t tc_ctx;
	uint8_t rel_bw;
	uint8_t prio_type;
	uint8_t egress_virt_up;
	uint8_t ingress_virt_up;
};

/**
 * @struct ice_qos_app_priority_table
 * @brief Application priority data
 */
struct ice_qos_app_priority_table {
	uint16_t prot_id;
	uint8_t priority;
	uint8_t selector;
};

#define IEEE_8021QAZ_MAX_TCS  8
#define ICE_TC_MAX_USER_PRIORITY 8
#define ICE_QOS_MAX_APPS 32
#define ICE_QOS_DSCP_NUM_VAL 64

/**
 * @struct ice_qos_params
 * @brief Holds all necessary data for RDMA to work with DCB
 *
 * Struct to hold QoS info
 * @var ice_qos_params::tc_info
 *	traffic class information
 * @var ice_qos_params::up2tc
 *	mapping from user priority to traffic class
 * @var ice_qos_params::vsi_relative_bw
 *	bandwidth settings
 * @var ice_qos_params::vsi_priority_type
 *	priority type
 * @var ice_qos_params::num_apps
 *	app count
 * @var ice_qos_params::pfc_mode
 *	PFC mode
 * @var ice_qos_params::dscp_map
 *	dscp mapping
 * @var ice_qos_params::apps
 *	apps
 * @var ice_qos_params::num_tc
 *	number of traffic classes
};
 */
struct ice_qos_params {
	struct ice_qos_info tc_info[IEEE_8021QAZ_MAX_TCS];
	uint8_t up2tc[ICE_TC_MAX_USER_PRIORITY];
	uint8_t vsi_relative_bw;
	uint8_t vsi_priority_type;
	uint32_t num_apps;
	uint8_t pfc_mode;
	uint8_t dscp_map[ICE_QOS_DSCP_NUM_VAL];
	struct ice_qos_app_priority_table apps[ICE_QOS_MAX_APPS];
	uint8_t num_tc;
};

/**
 * @struct ice_rdma_peer
 * @brief RDMA driver information
 *
 * Shared structure used by the RDMA client driver when talking with the main
 * device driver.
 *
 * Because the definition of this structure is shared between the two drivers,
 * its ABI should be handled carefully.
 *
 * @var ice_rdma_peer::ifp
 *	pointer to ifnet structure
 * @var ice_rdma_peer::dev
 *	device pointer
 * @var ice_rdma_peer::pci_mem
 *	information about PCI
 * @var ice_rdma_peer::initial_qos_info
 *	initial information on QoS
 * @var ice_rdma_peer::msix
 *	info about msix vectors
 * @var ice_rdma_peer::mtu
 *	initial mtu size
 * @var ice_rdma_peer::pf_vsi_num
 *	id of vsi
 * @var ice_rdma_peer::pf_id
 *	id of PF
 */
struct ice_rdma_peer {
	/**
	 * The KOBJ_FIELDS macro must come first, in order for it to be used
	 * as a kobject.
	 */
	KOBJ_FIELDS;

	struct ifnet *ifp;
	device_t dev;
	struct resource *pci_mem;
	struct ice_qos_params initial_qos_info;
	struct ice_rdma_msix msix;
	uint16_t mtu;
	uint16_t pf_vsi_num;
	uint8_t pf_id;
};

/**
 * @enum ice_res_type
 * @brief enum for type of resource registration
 *
 * enum for type of resource registration.
 * created for plausible compatibility with IDC
 */
enum ice_res_type {
	ICE_INVAL_RES = 0x0,
	ICE_RDMA_QSET_ALLOC = 0x8,
	ICE_RDMA_QSET_FREE = 0x18,
};

/**
 * @struct ice_rdma_qset_params
 * @brief struct to hold per RDMA Qset info
 *
 * @var ice_rdma_qset_params::teid
 *	qset teid
 * @var ice_rdma_qset_params::qs_handle
 *	qset from rdma driver
 * @var ice_rdma_qset_params::vsi_id
 *	vsi index
 * @var ice_rdma_qset_params::tc
 *	traffic class to which qset should belong to
 * @var ice_rdma_qset_params::reserved
 *	for future use
 */
struct ice_rdma_qset_params {
	uint32_t teid;
	uint16_t qs_handle;
	uint16_t vsi_id;
	uint8_t tc;
	uint8_t reserved[3];
};

#define ICE_MAX_TXQ_PER_TXQG 128
/**
 * @struct ice_rdma_qset_update
 * @brief struct used to register and unregister qsets for RDMA driver
 *
 * @var ice_rdma_qset_update::res_type
 *	ALLOC or FREE
 * @var ice_rdma_qset_update::cnt_req
 *	how many qsets are requested
 * @var ice_rdma_qset_update::res_allocated
 *	how many qsets are allocated
 * @var ice_rdma_qset_update::qsets
 *	rdma qset info
 */
struct ice_rdma_qset_update {
	enum ice_res_type res_type;
	uint16_t cnt_req;
	uint16_t res_allocated;
	uint32_t res_handle;
	struct ice_rdma_qset_params qsets;
};

/**
 * @enum ice_rdma_event_type
 * @brief enum for type of event from base driver
 */
enum ice_rdma_event_type {
	ICE_RDMA_EVENT_NONE = 0,
	ICE_RDMA_EVENT_LINK_CHANGE,
	ICE_RDMA_EVENT_MTU_CHANGE,
	ICE_RDMA_EVENT_TC_CHANGE,
	ICE_RDMA_EVENT_API_CHANGE,
	ICE_RDMA_EVENT_CRIT_ERR,
	ICE_RDMA_EVENT_RESET,
	ICE_RDMA_EVENT_QSET_REGISTER,
	ICE_RDMA_EVENT_VSI_FILTER_UPDATE,
	ICE_RDMA_EVENT_LAST
};

/**
 * @struct ice_rdma_event
 * @brief struct for event information to pass to RDMA driver
 *
 * @var ice_rdma_event::type
 *	event type
 */
struct ice_rdma_event {
	enum ice_rdma_event_type type;
	union {
		/* link change event */
		struct {
			int linkstate;
			uint64_t baudrate;
		};
		/* MTU change event */
		int mtu;
		/*
		 * TC/QoS/DCB change event
		 * prep: if true, this is a pre-event, post-event otherwise
		 */
		struct {
			struct ice_qos_params port_qos;
			bool prep;
		};
		/*
		 * CRIT_ERR event
		 */
		uint32_t oicr_reg;
	};
};

/**
 * @struct ice_rdma_request
 * @brief struct with data for a request from the RDMA driver
 *
 * @var ice_rdma_request::type
 *	event type
 */
struct ice_rdma_request {
	enum ice_rdma_event_type type;
	union {
		struct {
			struct ice_rdma_qset_update res;
		};
		struct {
			bool enable_filter;
		};
	};
};

int ice_rdma_register(struct ice_rdma_info *info);
int ice_rdma_unregister(void);

#endif
