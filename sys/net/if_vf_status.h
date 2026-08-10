/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Kevin Bowling <kbowling@FreeBSD.org>
 */

#ifndef _NET_IF_VF_STATUS_H_
#define	_NET_IF_VF_STATUS_H_

#include <sys/types.h>
#ifndef _KERNEL
#include <stdbool.h>
#endif

#include <net/ethernet.h>

/*
 * Kernel-only snapshot of SR-IOV VF state.  The fields member is a bit mask
 * identifying which other members contain valid data.  This distinguishes a
 * missing value from a value of false or zero.  Drivers set bits only for
 * information they can report, and each transport converts the snapshot to
 * its own ABI.  VLAN PCP and protocol describe the PF-administered access
 * VLAN, not the VF's trunk filters.  Transmit rates describe an aggregate VF
 * policy.  When both are present and the maximum is nonzero, the minimum must
 * not exceed it.  A successful snapshot may contain zero VFs, allowing a
 * provider to report that SR-IOV is available but is not currently configured.
 */
#define	IFVF_MAX_VFS			UINT16_MAX
#define	IFVF_MAX_EXTENSIONS		16
#define	IFVF_MAX_EXTENSION_FIELDS	64

enum if_vf_field {
	IFVF_F_CONFIGURED		= 1ULL << 0,
	IFVF_F_INITIALIZED		= 1ULL << 1,
	IFVF_F_MAC			= 1ULL << 2,
	IFVF_F_VLAN_MODE		= 1ULL << 3,
	IFVF_F_VLAN			= 1ULL << 4,
	IFVF_F_VLAN_PCP			= 1ULL << 5,
	IFVF_F_VLAN_PROTO		= 1ULL << 6,
	IFVF_F_VLAN_COUNT		= 1ULL << 7,
	IFVF_F_VLAN_LIMIT		= 1ULL << 8,
	IFVF_F_NUM_TX_QUEUES		= 1ULL << 9,
	IFVF_F_NUM_RX_QUEUES		= 1ULL << 10,
	IFVF_F_MIN_TX_RATE		= 1ULL << 11,
	IFVF_F_MAX_TX_RATE		= 1ULL << 12,
	IFVF_F_ALLOW_SET_MAC		= 1ULL << 13,
	IFVF_F_ALLOW_SET_VLAN		= 1ULL << 14,
	IFVF_F_MAC_ANTI_SPOOF		= 1ULL << 15,
	IFVF_F_ALLOW_PROMISC		= 1ULL << 16,
	IFVF_F_TRAFFIC_ALLOWED		= 1ULL << 17,
	IFVF_F_FAULT_BLOCKED		= 1ULL << 18,
	IFVF_F_QUARANTINED		= 1ULL << 19,
	IFVF_F_API_VERSION		= 1ULL << 20,
	IFVF_F_LINK_STATE_POLICY	= 1ULL << 21,
};

enum if_vf_vlan_mode {
	IFVF_VLAN_UNKNOWN = 0,
	IFVF_VLAN_ACCESS,
	IFVF_VLAN_TRUNK,
};

enum if_vf_link_state {
	IFVF_LINK_UNKNOWN = 0,
	IFVF_LINK_DOWN,
	IFVF_LINK_UP,
	IFVF_LINK_AUTO,
};

/*
 * Drivers may add fields under a stable, versioned namespace.  The named,
 * typed representation lets transports and generic consumers carry and
 * display fields whose driver-specific schema they do not know.  The driver
 * owns the field names and meanings; common code does not interpret them.
 * Namespace and field names must remain valid until the snapshot is freed.
 */
enum if_vf_ext_type {
	IFVF_EXT_BOOL = 1,
	IFVF_EXT_NUMBER,
	IFVF_EXT_STRING,
	IFVF_EXT_BINARY,
};

struct if_vf_ext_field {
	const char *name;
	enum if_vf_ext_type type;
	union {
		bool boolean;
		uint64_t number;
		char *string;
		struct {
			void *data;
			uint32_t length;
		} binary;
	} value;
};

struct if_vf_extension {
	const char *name;
	uint32_t version;
	uint32_t num_fields;
	struct if_vf_ext_field *fields;
};

#define	IFVF_API_VERSION_MAX	16

struct if_vf_info {
	uint64_t fields;
	uint64_t min_tx_rate_bps;	/* Zero means no guaranteed allocation. */
	uint64_t max_tx_rate_bps;	/* Zero means unlimited. */
	uint32_t index;
	uint32_t vlan_count;
	uint32_t vlan_limit;
	uint16_t tx_queue_count;
	uint16_t rx_queue_count;
	uint16_t vlan;
	uint16_t vlan_proto;		/* Host-order Ethernet type. */
	uint8_t vlan_pcp;
	uint8_t mac[ETHER_ADDR_LEN];
	enum if_vf_vlan_mode vlan_mode;
	enum if_vf_link_state link_state_policy;
	bool configured:1;
	bool initialized:1;
	bool allow_set_mac:1;
	bool allow_set_vlan:1;
	bool mac_anti_spoof:1;
	bool allow_promisc:1;
	bool traffic_allowed:1;
	bool fault_blocked:1;
	bool quarantined:1;
	/* Negotiated PF/VF mailbox API, not a version of this structure. */
	char api_version[IFVF_API_VERSION_MAX];
	uint32_t num_extensions;
	struct if_vf_extension *extensions;
};

struct if_vf_status {
	uint32_t num_vfs;
	struct if_vf_info vfs[];
};

#ifdef _KERNEL
struct if_vf_status *if_vf_status_alloc(uint32_t);
void if_vf_status_free(struct if_vf_status *);
struct if_vf_extension *if_vf_status_add_extension(struct if_vf_info *,
    const char *, uint32_t, uint32_t);
void if_vf_extension_set_bool(struct if_vf_extension *, uint32_t,
    const char *, bool);
void if_vf_extension_set_number(struct if_vf_extension *, uint32_t,
    const char *, uint64_t);
void if_vf_extension_set_string(struct if_vf_extension *, uint32_t,
    const char *, const char *);
void if_vf_extension_set_binary(struct if_vf_extension *, uint32_t,
    const char *, const void *, uint32_t);
#endif

#endif /* _NET_IF_VF_STATUS_H_ */
