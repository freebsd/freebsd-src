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
 *
 * $FreeBSD$
 */

#pragma once

#include <sys/types.h>

#include <net/if.h>

#include <netinet/in.h>
#include <netinet6/in6_var.h>

#define ND6_IFF_DEFAULTIF    0x8000

typedef enum {
	OK = 0,
	OTHER,
	IOCTL,
	SOCKET
} ifconfig_errtype;

/*
 * Opaque definition so calling application can just pass a
 * pointer to it for library use.
 */
struct ifconfig_handle;
typedef struct ifconfig_handle ifconfig_handle_t;

struct carpreq;
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
	size_t members_count;		/**< how many member interfaces */
	uint32_t cache_size;		/**< size of address cache */
	uint32_t cache_lifetime;	/**< address cache entry lifetime */
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
int ifconfig_set_fib(ifconfig_handle_t *h, const char *name, int fib);
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

int ifconfig_carp_get_info(ifconfig_handle_t *h, const char *name,
    struct carpreq *carpr, int ncarpr);

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
