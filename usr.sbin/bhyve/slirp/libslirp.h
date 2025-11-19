/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1995,1996 Danny Gasparovski.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * DANNY GASPAROVSKI OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef LIBSLIRP_H
#define LIBSLIRP_H

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <in6addr.h>
#include <basetsd.h>
typedef SSIZE_T slirp_ssize_t;
#ifdef BUILDING_LIBSLIRP
# define SLIRP_EXPORT __declspec(dllexport)
#else
# define SLIRP_EXPORT __declspec(dllimport)
#endif
#else
#include <sys/types.h>
typedef ssize_t slirp_ssize_t;
#include <netinet/in.h>
#include <arpa/inet.h>
#define SLIRP_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque structure containing the slirp state */
typedef struct Slirp Slirp;

/* Flags passed to SlirpAddPollCb and to be returned by SlirpGetREventsCb. */
enum {
    SLIRP_POLL_IN = 1 << 0,
    SLIRP_POLL_OUT = 1 << 1,
    SLIRP_POLL_PRI = 1 << 2,
    SLIRP_POLL_ERR = 1 << 3,
    SLIRP_POLL_HUP = 1 << 4,
};

/* Callback for application to get data from the guest */
typedef slirp_ssize_t (*SlirpReadCb)(void *buf, size_t len, void *opaque);
/* Callback for application to send data to the guest */
typedef slirp_ssize_t (*SlirpWriteCb)(const void *buf, size_t len, void *opaque);
/* Timer callback */
typedef void (*SlirpTimerCb)(void *opaque);
/* Callback for libslirp to register polling callbacks */
typedef int (*SlirpAddPollCb)(int fd, int events, void *opaque);
/* Callback for libslirp to get polling result */
typedef int (*SlirpGetREventsCb)(int idx, void *opaque);

/* For now libslirp creates only a timer for the IPv6 RA */
typedef enum SlirpTimerId {
    SLIRP_TIMER_RA,
    SLIRP_TIMER_NUM,
} SlirpTimerId;

/*
 * Callbacks from slirp, to be set by the application.
 *
 * The opaque parameter is set to the opaque pointer given in the slirp_new /
 * slirp_init call.
 */
typedef struct SlirpCb {
    /*
     * Send an ethernet frame to the guest network. The opaque parameter is the
     * one given to slirp_init(). If the guest is not ready to receive a frame,
     * the function can just drop the data. TCP will then handle retransmissions
     * at a lower pace.
     * <0 reports an IO error.
     */
    SlirpWriteCb send_packet;
    /* Print a message for an error due to guest misbehavior.  */
    void (*guest_error)(const char *msg, void *opaque);
    /* Return the virtual clock value in nanoseconds */
    int64_t (*clock_get_ns)(void *opaque);
    /* Create a new timer with the given callback and opaque data. Not
     * needed if timer_new_opaque is provided. */
    void *(*timer_new)(SlirpTimerCb cb, void *cb_opaque, void *opaque);
    /* Remove and free a timer */
    void (*timer_free)(void *timer, void *opaque);
    /* Modify a timer to expire at @expire_time (ms) */
    void (*timer_mod)(void *timer, int64_t expire_time, void *opaque);
    /* Register a fd for future polling */
    void (*register_poll_fd)(int fd, void *opaque);
    /* Unregister a fd */
    void (*unregister_poll_fd)(int fd, void *opaque);
    /* Kick the io-thread, to signal that new events may be processed because some TCP buffer
     * can now receive more data, i.e. slirp_socket_can_recv will return 1. */
    void (*notify)(void *opaque);

    /*
     * Fields introduced in SlirpConfig version 4 begin
     */

    /* Initialization has completed and a Slirp* has been created.  */
    void (*init_completed)(Slirp *slirp, void *opaque);
    /* Create a new timer.  When the timer fires, the application passes
     * the SlirpTimerId and cb_opaque to slirp_handle_timer.  */
    void *(*timer_new_opaque)(SlirpTimerId id, void *cb_opaque, void *opaque);
} SlirpCb;

#define SLIRP_CONFIG_VERSION_MIN 1
#define SLIRP_CONFIG_VERSION_MAX 5

typedef struct SlirpConfig {
    /* Version must be provided */
    uint32_t version;
    /*
     * Fields introduced in SlirpConfig version 1 begin
     */
    /* Whether to prevent the guest from accessing the Internet */
    int restricted;
    /* Whether IPv4 is enabled */
    bool in_enabled;
    /* Virtual network for the guest */
    struct in_addr vnetwork;
    /* Mask for the virtual network for the guest */
    struct in_addr vnetmask;
    /* Virtual address for the host exposed to the guest */
    struct in_addr vhost;
    /* Whether IPv6 is enabled */
    bool in6_enabled;
    /* Virtual IPv6 network for the guest */
    struct in6_addr vprefix_addr6;
    /* Len of the virtual IPv6 network for the guest */
    uint8_t vprefix_len;
    /* Virtual address for the host exposed to the guest */
    struct in6_addr vhost6;
    /* Hostname exposed to the guest in DHCP hostname option */
    const char *vhostname;
    /* Hostname exposed to the guest in the DHCP TFTP server name option */
    const char *tftp_server_name;
    /* Path of the files served by TFTP */
    const char *tftp_path;
    /* Boot file name exposed to the guest via DHCP */
    const char *bootfile;
    /* Start of the DHCP range */
    struct in_addr vdhcp_start;
    /* Virtual address for the DNS server exposed to the guest */
    struct in_addr vnameserver;
    /* Virtual IPv6 address for the DNS server exposed to the guest */
    struct in6_addr vnameserver6;
    /* DNS search names exposed to the guest via DHCP */
    const char **vdnssearch;
    /* Domain name exposed to the guest via DHCP */
    const char *vdomainname;
    /* MTU when sending packets to the guest */
    /* Default: IF_MTU_DEFAULT */
    size_t if_mtu;
    /* MRU when receiving packets from the guest */
    /* Default: IF_MRU_DEFAULT */
    size_t if_mru;
    /* Prohibit connecting to 127.0.0.1:* */
    bool disable_host_loopback;
    /*
     * Enable emulation code (*warning*: this code isn't safe, it is not
     * recommended to enable it)
     */
    bool enable_emu;

    /*
     * Fields introduced in SlirpConfig version 2 begin
     */
    /* Address to be used when sending data to the Internet */
    struct sockaddr_in *outbound_addr;
    /* IPv6 Address to be used when sending data to the Internet */
    struct sockaddr_in6 *outbound_addr6;

    /*
     * Fields introduced in SlirpConfig version 3 begin
     */
    /* slirp will not redirect/serve any DNS packet */
    bool disable_dns;

    /*
     * Fields introduced in SlirpConfig version 4 begin
     */
    /* slirp will not reply to any DHCP requests */
    bool disable_dhcp;

    /*
     * Fields introduced in SlirpConfig version 5 begin
     */
    /* Manufacturer ID (IANA Private Enterprise number) */
    uint32_t mfr_id;
    /*
     * MAC address allocated for an out-of-band management controller, to be
     * retrieved through NC-SI.
     */
    uint8_t oob_eth_addr[6];
} SlirpConfig;

/* Create a new instance of a slirp stack */
SLIRP_EXPORT
Slirp *slirp_new(const SlirpConfig *cfg, const SlirpCb *callbacks,
                 void *opaque);
/* slirp_init is deprecated in favor of slirp_new */
SLIRP_EXPORT
Slirp *slirp_init(int restricted, bool in_enabled, struct in_addr vnetwork,
                  struct in_addr vnetmask, struct in_addr vhost,
                  bool in6_enabled, struct in6_addr vprefix_addr6,
                  uint8_t vprefix_len, struct in6_addr vhost6,
                  const char *vhostname, const char *tftp_server_name,
                  const char *tftp_path, const char *bootfile,
                  struct in_addr vdhcp_start, struct in_addr vnameserver,
                  struct in6_addr vnameserver6, const char **vdnssearch,
                  const char *vdomainname, const SlirpCb *callbacks,
                  void *opaque);
/* Shut down an instance of a slirp stack */
SLIRP_EXPORT
void slirp_cleanup(Slirp *slirp);

/* This is called by the application when it is about to sleep through poll().
 * *timeout is set to the amount of virtual time (in ms) that the application intends to
 * wait (UINT32_MAX if infinite). slirp_pollfds_fill updates it according to
 * e.g. TCP timers, so the application knows it should sleep a smaller amount of
 * time. slirp_pollfds_fill calls add_poll for each file descriptor
 * that should be monitored along the sleep. The opaque pointer is passed as
 * such to add_poll, and add_poll returns an index. */
SLIRP_EXPORT
void slirp_pollfds_fill(Slirp *slirp, uint32_t *timeout,
                        SlirpAddPollCb add_poll, void *opaque);

/* This is called by the application after sleeping, to report which file
 * descriptors are available. slirp_pollfds_poll calls get_revents on each file
 * descriptor, giving it the index that add_poll returned during the
 * slirp_pollfds_fill call, to know whether the descriptor is available for
 * read/write/etc. (SLIRP_POLL_*)
 * select_error should be passed 1 if poll() returned an error. */
SLIRP_EXPORT
void slirp_pollfds_poll(Slirp *slirp, int select_error,
                        SlirpGetREventsCb get_revents, void *opaque);

/* This is called by the application when the guest emits a packet on the
 * guest network, to be interpreted by slirp. */
SLIRP_EXPORT
void slirp_input(Slirp *slirp, const uint8_t *pkt, int pkt_len);

/* This is called by the application when a timer expires, if it provides
 * the timer_new_opaque callback.  It is not needed if the application only
 * uses timer_new. */
SLIRP_EXPORT
void slirp_handle_timer(Slirp *slirp, SlirpTimerId id, void *cb_opaque);

/* These set up / remove port forwarding between a host port in the real world
 * and the guest network. */
SLIRP_EXPORT
int slirp_add_hostfwd(Slirp *slirp, int is_udp, struct in_addr host_addr,
                      int host_port, struct in_addr guest_addr, int guest_port);
SLIRP_EXPORT
int slirp_remove_hostfwd(Slirp *slirp, int is_udp, struct in_addr host_addr,
                         int host_port);

#define SLIRP_HOSTFWD_UDP 1
#define SLIRP_HOSTFWD_V6ONLY 2
SLIRP_EXPORT
int slirp_add_hostxfwd(Slirp *slirp,
                       const struct sockaddr *haddr, socklen_t haddrlen,
                       const struct sockaddr *gaddr, socklen_t gaddrlen,
                       int flags);
SLIRP_EXPORT
int slirp_remove_hostxfwd(Slirp *slirp,
                          const struct sockaddr *haddr, socklen_t haddrlen,
                          int flags);

/* Set up port forwarding between a port in the guest network and a
 * command running on the host */
SLIRP_EXPORT
int slirp_add_exec(Slirp *slirp, const char *cmdline,
                   struct in_addr *guest_addr, int guest_port);
/* Set up port forwarding between a port in the guest network and a
 * Unix port on the host */
SLIRP_EXPORT
int slirp_add_unix(Slirp *slirp, const char *unixsock,
                   struct in_addr *guest_addr, int guest_port);
/* Set up port forwarding between a port in the guest network and a
 * callback that will receive the data coming from the port */
SLIRP_EXPORT
int slirp_add_guestfwd(Slirp *slirp, SlirpWriteCb write_cb, void *opaque,
                       struct in_addr *guest_addr, int guest_port);

/* TODO: rather identify a guestfwd through an opaque pointer instead of through
 * the guest_addr */

/* This is called by the application for a guestfwd, to determine how much data
 * can be received by the forwarded port through a call to slirp_socket_recv. */
SLIRP_EXPORT
size_t slirp_socket_can_recv(Slirp *slirp, struct in_addr guest_addr,
                             int guest_port);
/* This is called by the application for a guestfwd, to provide the data to be
 * sent on the forwarded port */
SLIRP_EXPORT
void slirp_socket_recv(Slirp *slirp, struct in_addr guest_addr, int guest_port,
                       const uint8_t *buf, int size);

/* Remove entries added by slirp_add_exec, slirp_add_unix or slirp_add_guestfwd */
SLIRP_EXPORT
int slirp_remove_guestfwd(Slirp *slirp, struct in_addr guest_addr,
                          int guest_port);

/* Return a human-readable state of the slirp stack */
SLIRP_EXPORT
char *slirp_connection_info(Slirp *slirp);

/* Return a human-readable state of the NDP/ARP tables */
SLIRP_EXPORT
char *slirp_neighbor_info(Slirp *slirp);

/* Save the slirp state through the write_cb. The opaque pointer is passed as
 * such to the write_cb. */
SLIRP_EXPORT
int slirp_state_save(Slirp *s, SlirpWriteCb write_cb, void *opaque);

/* Returns the version of the slirp state, to be saved along the state */
SLIRP_EXPORT
int slirp_state_version(void);

/* Load the slirp state through the read_cb. The opaque pointer is passed as
 * such to the read_cb. The version should be given as it was obtained from
 * slirp_state_version when slirp_state_save was called. */
SLIRP_EXPORT
int slirp_state_load(Slirp *s, int version_id, SlirpReadCb read_cb,
                     void *opaque);

/* Return the version of the slirp implementation */
SLIRP_EXPORT
const char *slirp_version_string(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* LIBSLIRP_H */
