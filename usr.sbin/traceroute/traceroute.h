/*-
 * Copyright (c) 2024 Lexi Winter
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef TRACEROUTE_H_INCLUDED
#define TRACEROUTE_H_INCLUDED

#include <sys/socket.h>

#include <netinet/in.h>

#include <libcasper.h>
#include <casper/cap_net.h>

#include <stdint.h>
#include <stdbool.h>

struct protocol;

//
// command-line options
//
struct options {
	// destination address or hostname, this is always set.
	char *destination_hostname;

	// packet size
	uint16_t packetlen;

	// -4, -6: address family
	int family;

	// -a: enable ASN lookups
	bool asn_lookups;
	// -A: set ASN lookup server
	char *asn_lookup_server;
	// -d: enable socket debugging
	bool socket_debug;
	// -D: print diff of received packet
	bool icmp_diff;
	// -e: use a fixed destination port
	bool fixed_port;
	// -E: detect ECN bleaching
	bool detect_ecn_bleaching;
	// -f: set the initial TTL (number of hops to skip)
	unsigned first_ttl;
	// -F: set DF
	bool set_df;
	// -g: list of gateways for source routing.
	char **gateways;
	size_t ngateways;
	// -i: set outgoing interface
	char *interface;
	// -P: protocol
	int protocol;
	// -m: set max ttl
	unsigned max_ttl;
	// -n: do not resolve hop addresses
	bool no_dns;
	// -p: base port number
	uint16_t port;
	// -q: number of probes per hop
	unsigned nprobes;
	// -r: bypass routing table
	bool send_direct;
	// -s: source address
	char *source_hostname;
	// -S: print per-hop summary
	bool summary;
	// -t: set type of service
	unsigned tos;
	// -v: verbose mode
	bool verbose;
	// -w: how long to wait for a probe
	unsigned wait_time;
	// -x: toggle whether we calculate checksums
	bool toggle_checksums;
	// -z: time to wait between probes
	unsigned pause_msecs;
};

struct options *parse_options(int *argc, char **argv);

#define RCVPKTSZ	512

//
// our context
//
struct context {
	// command-line options
	struct options *options;

	// source address (or null)
	struct sockaddr *source;

	// destination address
	struct sockaddr *destination;

	// ident marker to identify packets
	uint16_t ident;

	// the protocol we're using to send packets
	struct protocol const *protocol;

	// sending socket
	int sendsock;

	// receiving socket (for errors)
	int rcvsock;

	// casper net capability (for DNS)
	cap_channel_t *capnet;

	// ASN lookup database
	void *asndb;

	// the next packet to send
	uint8_t *send_packet;
	// size of the packet
	size_t send_packet_size;

	// end of the IP header in send_packet
	uint8_t *send_packet_data_start;

	// last received packet
	char packet[RCVPKTSZ];
};

// initialise casper
void setup_cap(struct context *ctx);

// set the source address; this requires that the destination address be
// already set.  if hostname is NULL, use the kernel default.
void set_source(struct context *, char const *hostname, char const *device);

// resolve the destination address; af_hint should be AF_INET/AF_INET6 or
// AF_UNSPEC.  exits on failure.
void set_destination(struct context *, char const *hostname, int af_hint);

// print a hop's hostname and IP address
void print_hop(struct context *, struct sockaddr const *);

// ASN lookup support
void *as_setup(const char *);
void as_shutdown(void *);
unsigned int as_lookup(void const *, char *, sa_family_t);

int	traceroute4(struct context *);
int	traceroute6(struct context *);

// return the difference of two timevals in floating seconds
double deltaT(struct timeval const *, struct timeval const *);

/* protocol to use for probes */
extern struct outproto *proto;

// convert a sockaddr to a printable string 
char const *str_ss(struct sockaddr const *addr);

// IP
uint16_t in_cksum(uint8_t const *data, size_t len);

// SCTP
uint32_t sctp_crc32c(void *packet, uint32_t len);

// send a probe packet to the destination
void send_probe(struct context *ctx, int seq, unsigned hops);

// wait for a reply to our probe
int wait_for_reply(struct context *ctx, struct msghdr *mhdr);

// packet creators
void *pkt_make_udp(struct context *ctx, uint8_t seq, size_t pktlen);
void *pkt_make_udplite(struct context *ctx, uint8_t seq, size_t pktlen);
void *pkt_make_icmp(struct context *ctx, uint8_t seq, size_t pktlen);
void *pkt_make_tcp(struct context *ctx, uint8_t seq, size_t pktlen);
void *pkt_make_sctp(struct context *ctx, uint8_t seq, size_t pktlen);
void *pkt_make_none(struct context *ctx, uint8_t seq, size_t pktlen);

typedef struct protocol {
	int	protocol_number;
	size_t	min_pktlen;
	int	_socket4; // private
	int	_socket6; // private
	void *	(*pkt_make)(struct context *ctx, uint8_t seq, size_t pktlen);
} protocol_t;

void			protocol_init(void);
protocol_t const *	protocol_find(int protocol_number);
int			protocol_get_socket(protocol_t const *, int family);

#endif	/* !TRACEROUTE_H_INCLUDED */
