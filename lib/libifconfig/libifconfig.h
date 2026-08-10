/*
 * Copyright (c) 2016-2017, Marie Helene Kvello-Aune
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * thislist of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation and/or
 * other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <sys/types.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_bridgevar.h> /* for ifbvlan_set_t */
#include <netlink/route/interface.h>

#include <netinet/in.h>
#include <netinet/ip_carp.h>
#include <netinet6/in6_var.h>

#include <stdbool.h>

#define ND6_IFF_DEFAULTIF    0x8000

typedef enum {
	OK = 0,
	OTHER,
	IOCTL,
	SOCKET,
	NETLINK
} ifconfig_errtype;

/*
 * Opaque definition so calling application can just pass a
 * pointer to it for library use.
 */
struct ifconfig_handle;
typedef struct ifconfig_handle ifconfig_handle_t;

struct ifaddrs;
struct ifbropreq;
struct ifbreq;
struct in6_ndireq;
struct lagg_reqall;
struct lagg_reqflags;
struct lagg_reqopts;
struct lagg_reqport;

/** Stores extra info associated with a bridge(4) interface */
struct ifconfig_bridge_status {
	struct ifbropreq *params;	/**< current operational parameters */
	struct ifbreq *members;		/**< list of bridge members */
	ifbvlan_set_t *member_vlans;	/**< bridge member vlan sets */
	size_t members_count;		/**< how many member interfaces */
	uint32_t cache_size;		/**< size of address cache */
	uint32_t cache_lifetime;	/**< address cache entry lifetime */
	ifbr_flags_t flags;		/**< bridge flags */
	ether_vlanid_t defpvid;		/**< default pvid */
};

struct ifconfig_capabilities {
	/** Current capabilities (ifconfig prints this as 'options')*/
	int curcap;
	/** Requested capabilities (ifconfig prints this as 'capabilities')*/
	int reqcap;
};

/** Stores extra info associated with an inet address */
struct ifconfig_inet_addr {
	const struct sockaddr_in *sin;
	const struct sockaddr_in *netmask;
	const struct sockaddr_in *dst;
	const struct sockaddr_in *broadcast;
	int prefixlen;
	uint8_t vhid;
};

/** Stores extra info associated with an inet6 address */
struct ifconfig_inet6_addr {
	struct sockaddr_in6 *sin6;
	struct sockaddr_in6 *dstin6;
	struct in6_addrlifetime lifetime;
	int prefixlen;
	uint32_t flags;
	uint8_t vhid;
};

/** Stores extra info associated with a lagg(4) interface */
struct ifconfig_lagg_status {
	struct lagg_reqall *ra;
	struct lagg_reqopts *ro;
	struct lagg_reqflags *rf;
};

/** Retrieves a new state object for use in other API calls.
 * Example usage:
 *{@code
 * // Create state object
 * ifconfig_handle_t *lifh;
 * lifh = ifconfig_open();
 * if (lifh == NULL) {
 *     // Handle error
 * }
 *
 * // Do stuff with the handle
 *
 * // Dispose of the state object
 * ifconfig_close(lifh);
 * lifh = NULL;
 *}
 */
ifconfig_handle_t *ifconfig_open(void);

/** Frees resources held in the provided state object.
 * @param h The state object to close.
 * @see #ifconfig_open(void)
 */
void ifconfig_close(ifconfig_handle_t *h);

/** Identifies what kind of error occurred. */
ifconfig_errtype ifconfig_err_errtype(ifconfig_handle_t *h);

/** Retrieves the errno associated with the error, if any. */
int ifconfig_err_errno(ifconfig_handle_t *h);

typedef void (*ifconfig_foreach_func_t)(ifconfig_handle_t *h,
    struct ifaddrs *ifa, void *udata);

/** Iterate over every network interface
 * @param h	An open ifconfig state object
 * @param cb	A callback function to call with a pointer to each interface
 * @param udata	An opaque value that will be passed to the callback.
 * @return	0 on success, nonzero if the list could not be iterated
 */
int ifconfig_foreach_iface(ifconfig_handle_t *h, ifconfig_foreach_func_t cb,
    void *udata);

/** Iterate over every address on a single network interface
 * @param h	An open ifconfig state object
 * @param ifa	A pointer that was supplied by a previous call to
 *              ifconfig_foreach_iface
 * @param udata	An opaque value that will be passed to the callback.
 * @param cb	A callback function to call with a pointer to each ifaddr
 */
void ifconfig_foreach_ifaddr(ifconfig_handle_t *h, struct ifaddrs *ifa,
    ifconfig_foreach_func_t cb, void *udata);

/** If error type was IOCTL, this identifies which request failed. */
unsigned long ifconfig_err_ioctlreq(ifconfig_handle_t *h);
int ifconfig_get_description(ifconfig_handle_t *h, const char *name,
    char **description);
int ifconfig_set_description(ifconfig_handle_t *h, const char *name,
    const char *newdescription);
int ifconfig_unset_description(ifconfig_handle_t *h, const char *name);
int ifconfig_set_name(ifconfig_handle_t *h, const char *name,
    const char *newname);
int ifconfig_get_orig_name(ifconfig_handle_t *h, const char *ifname,
    char **orig_name);
int ifconfig_get_fib(ifconfig_handle_t *h, const char *name, int *fib);
int ifconfig_set_mtu(ifconfig_handle_t *h, const char *name, const int mtu);
int ifconfig_get_mtu(ifconfig_handle_t *h, const char *name, int *mtu);
int ifconfig_get_nd6(ifconfig_handle_t *h, const char *name,
    struct in6_ndireq *nd);
int ifconfig_set_metric(ifconfig_handle_t *h, const char *name,
    const int metric);
int ifconfig_get_metric(ifconfig_handle_t *h, const char *name, int *metric);
int ifconfig_set_capability(ifconfig_handle_t *h, const char *name,
    const int capability);
int ifconfig_get_capability(ifconfig_handle_t *h, const char *name,
    struct ifconfig_capabilities *capability);

/** Retrieve the list of groups to which this interface belongs
 * @param h	An open ifconfig state object
 * @param name	The interface name
 * @param ifgr	return argument.  The caller is responsible for freeing
 *              ifgr->ifgr_groups
 * @return	0 on success, nonzero on failure
 */
int ifconfig_get_groups(ifconfig_handle_t *h, const char *name,
    struct ifgroupreq *ifgr);
int ifconfig_get_ifstatus(ifconfig_handle_t *h, const char *name,
    struct ifstat *stat);

/*
 * SR-IOV VF status schema contract.
 *
 * Bits in the presence mask are indexed by IFLAF_VF_* and distinguish an
 * omitted fact from false or zero.  Driver-specific facts remain owned by a
 * stable, versioned driver namespace: that driver defines the names, types,
 * and meanings of its fields.  Their named, typed form lets generic consumers
 * carry or display extensions without knowing each driver's schema.  Consumers
 * that interpret extensions must ignore unknown namespaces and fields.
 * Additive optional fields retain a namespace version; an incompatible type or
 * semantic change requires a new version.  All pointed-to storage belongs to
 * the returned status object and is released by ifconfig_free_vf_status().
 * VF records are returned through a pointer vector so append-only growth of
 * struct ifconfig_vf_info does not change the array stride seen by existing
 * consumers.  VLAN PCP and protocol describe the PF-administered access VLAN;
 * they do not describe trunk-filter entries.
 */

enum ifconfig_vf_vlan_mode {
	IFCONFIG_VF_VLAN_UNKNOWN = IFLAF_VF_VLAN_UNKNOWN,
	IFCONFIG_VF_VLAN_ACCESS = IFLAF_VF_VLAN_ACCESS,
	IFCONFIG_VF_VLAN_TRUNK = IFLAF_VF_VLAN_TRUNK,
};

enum ifconfig_vf_link_state {
	IFCONFIG_VF_LINK_UNKNOWN = IFLAF_VF_LINK_UNKNOWN,
	IFCONFIG_VF_LINK_DOWN = IFLAF_VF_LINK_DOWN,
	IFCONFIG_VF_LINK_UP = IFLAF_VF_LINK_UP,
	IFCONFIG_VF_LINK_AUTO = IFLAF_VF_LINK_AUTO,
};

enum ifconfig_vf_extension_type {
	IFCONFIG_VF_EXT_BOOL = 1,
	IFCONFIG_VF_EXT_NUMBER,
	IFCONFIG_VF_EXT_STRING,
	IFCONFIG_VF_EXT_BINARY,
};

struct ifconfig_vf_extension_field {
	char *name;
	enum ifconfig_vf_extension_type type;
	union {
		bool boolean;
		uint64_t number;
		char *string;
		struct {
			void *data;
			size_t length;
		} binary;
	} value;
};

struct ifconfig_vf_extension {
	char *name;
	uint32_t version;
	size_t num_fields;
	struct ifconfig_vf_extension_field *fields;
};

struct ifconfig_vf_info {
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
	enum ifconfig_vf_vlan_mode vlan_mode;
	enum ifconfig_vf_link_state link_state_policy;
	bool configured;
	bool initialized;
	bool allow_set_mac;
	bool allow_set_vlan;
	bool mac_anti_spoof;
	bool allow_promisc;
	bool traffic_allowed;
	bool fault_blocked;
	bool quarantined;
	char *api_version;
	size_t num_extensions;
	struct ifconfig_vf_extension *extensions;
};

struct ifconfig_vf_status {
	uint64_t pf_link_speed;
	enum ifconfig_vf_link_state pf_link_state;
	bool pf_link_state_present;
	bool pf_link_speed_present;
	size_t num_vfs;
	struct ifconfig_vf_info **vfs;
};

/** Retrieve structured SR-IOV VF status for an interface through rtnetlink.
 * @param h	An open ifconfig state object
 * @param name	The PF interface name
 * @param statusp Return argument.  Free it with ifconfig_free_vf_status().
 * @return	0 on success, -1 on failure
 */
int ifconfig_get_vf_status(ifconfig_handle_t *h, const char *name,
    struct ifconfig_vf_status **statusp);
void ifconfig_free_vf_status(struct ifconfig_vf_status *status);

/** Retrieve the interface media information
 * @param h	An open ifconfig state object
 * @param name	The interface name
 * @param ifmr	Return argument.  The caller is responsible for freeing it
 * @return	0 on success, nonzero on failure
 */
int ifconfig_media_get_mediareq(ifconfig_handle_t *h, const char *name,
    struct ifmediareq **ifmr);

const char *ifconfig_media_get_status(const struct ifmediareq *ifmr);

typedef int ifmedia_t;

#define INVALID_IFMEDIA ((ifmedia_t)-1)

/** Retrieve the name of a media type
 * @param media	The media to be named
 * @return	A pointer to the media type name, or NULL on failure
 */
const char *ifconfig_media_get_type(ifmedia_t media);

/** Retrieve a media type by its name
 * @param name	The name of a media type
 * @return	The media type value, or INVALID_IFMEDIA on failure
 */
ifmedia_t ifconfig_media_lookup_type(const char *name);

/** Retrieve the name of a media subtype
 * @param media	The media subtype to be named
 * @return	A pointer to the media subtype name, or NULL on failure
 */
const char *ifconfig_media_get_subtype(ifmedia_t media);

/** Retrieve a media subtype by its name
 * @param media	The top level media type whose subtype we want
 * @param name	The name of a media subtype
 * @return	The media subtype value, or INVALID_IFMEDIA on failure
 */
ifmedia_t ifconfig_media_lookup_subtype(ifmedia_t media, const char *name);

/** Retrieve the name of a media mode
 * @param media	The media mode to be named
 * @return	A pointer to the media mode name, or NULL on failure
 */
const char *ifconfig_media_get_mode(ifmedia_t media);

/** Retrieve a media mode by its name
 * @param media	The top level media type whose mode we want
 * @param name	The name of a media mode
 * @return	The media mode value, or INVALID_IFMEDIA on failure
 */
ifmedia_t ifconfig_media_lookup_mode(ifmedia_t media, const char *name);

/** Retrieve an array of media options
 * @param media	The media for which to obtain the options
 * @return	Pointer to an array of pointers to option names,
 * 		terminated by a NULL pointer, or simply NULL on failure.
 * 		The caller is responsible for freeing the array but not its
 * 		contents.
 */
const char **ifconfig_media_get_options(ifmedia_t media);

/** Retrieve an array of media options by names
 * @param media	The top level media type whose options we want
 * @param opts	Pointer to an array of string pointers naming options
 * @param nopts Number of elements in the opts array
 * @return	Pointer to an array of media options, one for each option named
 * 		in opts.  NULL is returned instead with errno set to ENOMEM if
 * 		allocating the return array fails or EINVAL if media is not
 * 		valid.  A media option in the array will be INVALID_IFMEDIA
 * 		when lookup failed for the option named in that position in
 * 		opts.  The caller is responsible for freeing the array.
 */
ifmedia_t *ifconfig_media_lookup_options(ifmedia_t media, const char **opts,
    size_t nopts);

/** Retrieve the reason the interface is down
 * @param h	An open ifconfig state object
 * @param name	The interface name
 * @param ifdr	Return argument.
 * @return	0 on success, nonzero on failure
 */
int ifconfig_media_get_downreason(ifconfig_handle_t *h, const char *name,
    struct ifdownreason *ifdr);

struct ifconfig_carp {
	size_t		carpr_count;
	uint32_t	carpr_vhid;
	uint32_t	carpr_state;
	int32_t		carpr_advbase;
	int32_t		carpr_advskew;
	uint8_t		carpr_key[CARP_KEY_LEN];
	struct in_addr	carpr_addr;
	struct in6_addr	carpr_addr6;
	carp_version_t	carpr_version;
	uint8_t		carpr_vrrp_prio;
	uint16_t	carpr_vrrp_adv_inter;
};

int ifconfig_carp_get_vhid(ifconfig_handle_t *h, const char *name,
    struct ifconfig_carp *carpr, uint32_t vhid);
int ifconfig_carp_get_info(ifconfig_handle_t *h, const char *name,
    struct ifconfig_carp *carpr, size_t ncarp);
int ifconfig_carp_set_info(ifconfig_handle_t *h, const char *name,
    const struct ifconfig_carp *carpr);

/** Retrieve additional information about an inet address
 * @param h	An open ifconfig state object
 * @param name	The interface name
 * @param ifa	Pointer to the address structure of interest
 * @param addr	Return argument.  It will be filled with additional information
 *              about the address.
 * @return	0 on success, nonzero on failure.
 */
int ifconfig_inet_get_addrinfo(ifconfig_handle_t *h,
    const char *name, struct ifaddrs *ifa, struct ifconfig_inet_addr *addr);

/** Retrieve additional information about an inet6 address
 * @param h	An open ifconfig state object
 * @param name	The interface name
 * @param ifa	Pointer to the address structure of interest
 * @param addr	Return argument.  It will be filled with additional information
 *              about the address.
 * @return	0 on success, nonzero on failure.
 */
int ifconfig_inet6_get_addrinfo(ifconfig_handle_t *h,
    const char *name, struct ifaddrs *ifa, struct ifconfig_inet6_addr *addr);

/** Retrieve additional information about a bridge(4) interface */
int ifconfig_bridge_get_bridge_status(ifconfig_handle_t *h,
    const char *name, struct ifconfig_bridge_status **bridge);

/** Frees the structure returned by ifconfig_bridge_get_bridge_status.  Does
 * nothing if the argument is NULL
 * @param bridge	Pointer to the structure to free
 */
void ifconfig_bridge_free_bridge_status(struct ifconfig_bridge_status *bridge);

/** Retrieve additional information about a lagg(4) interface */
int ifconfig_lagg_get_lagg_status(ifconfig_handle_t *h,
    const char *name, struct ifconfig_lagg_status **lagg_status);

/** Retrieve additional information about a member of a lagg(4) interface */
int ifconfig_lagg_get_laggport_status(ifconfig_handle_t *h,
    const char *name, struct lagg_reqport *rp);

/** Frees the structure returned by ifconfig_lagg_get_lagg_status.  Does
 * nothing if the argument is NULL
 * @param laggstat	Pointer to the structure to free
 */
void ifconfig_lagg_free_lagg_status(struct ifconfig_lagg_status *laggstat);

/** Destroy a virtual interface
 * @param name Interface to destroy
 */
int ifconfig_destroy_interface(ifconfig_handle_t *h, const char *name);

/** Creates a (virtual) interface
 * @param name Name of interface to create. Example: bridge or bridge42
 * @param name ifname Is set to actual name of created interface
 */
int ifconfig_create_interface(ifconfig_handle_t *h, const char *name,
    char **ifname);

/** Creates a (virtual) interface
 * @param name Name of interface to create. Example: vlan0 or ix0.50
 * @param name ifname Is set to actual name of created interface
 * @param vlandev Name of interface to attach to
 * @param vlanid VLAN ID/Tag. Must not be 0.
 */
int ifconfig_create_interface_vlan(ifconfig_handle_t *h, const char *name,
    char **ifname, const char *vlandev, const unsigned short vlantag);

int ifconfig_set_vlantag(ifconfig_handle_t *h, const char *name,
    const char *vlandev, const unsigned short vlantag);

/** Gets the names of all interface cloners available on the system
 * @param bufp	Set to the address of the names buffer on success or NULL
 *              if an error occurs.  This buffer must be freed when done.
 * @param lenp	Set to the number of names in the returned buffer or 0
 * 		if an error occurs.  Each name is contained within an
 * 		IFNAMSIZ length slice of the buffer, for a total buffer
 * 		length of *lenp * IFNAMSIZ bytes.
 */
int ifconfig_list_cloners(ifconfig_handle_t *h, char **bufp, size_t *lenp);

/** Brings the interface up/down
 * @param h	    An open ifconfig state object
 * @param ifname    The interface name
 * @param up	    true to bring the interface up, false to bring it down
 * @return	    0 on success, nonzero on failure.
 *		    On failure, the error info on the handle is set.
 */
int ifconfig_set_up(ifconfig_handle_t *h, const char *ifname, bool up);
