/* dhcpd.h

   Definitions for dhcpd... */

/*
 * Copyright (c) 1995, 1996, 1997, 1998, 1999
 * The Internet Software Consortium.    All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of The Internet Software Consortium nor the names
 *    of its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INTERNET SOFTWARE CONSORTIUM AND
 * CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE INTERNET SOFTWARE CONSORTIUM OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This software has been written for the Internet Software Consortium
 * by Ted Lemon <mellon@fugue.com> in cooperation with Vixie
 * Enterprises.  To learn more about the Internet Software Consortium,
 * see ``http://www.vix.com/isc''.  To learn more about Vixie
 * Enterprises, see ``http://www.vix.com''.
 */

#ifndef __CYGWIN32__
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <netdb.h>
#else
#define fd_set cygwin_fd_set
#include <sys/types.h>
#endif
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <ctype.h>
#include <time.h>

#include "cdefs.h"
#include "osdep.h"
#include "dhcp.h"
#include "tree.h"
#include "hash.h"
#include "inet.h"
#include "sysconf.h"

struct option_data {
	int len;
	u_int8_t *data;
};

struct string_list {
	struct string_list *next;
	char string [1];
};

/* A name server, from /etc/resolv.conf. */
struct name_server {
	struct name_server *next;
	struct sockaddr_in addr;
	TIME rcdate;
};

/* A domain search list element. */
struct domain_search_list {
	struct domain_search_list *next;
	char *domain;
	TIME rcdate;
};

/* A dhcp packet and the pointers to its option values. */
struct packet {
	struct dhcp_packet *raw;
	int packet_length;
	int packet_type;
	int options_valid;
	int client_port;
	struct iaddr client_addr;
	struct interface_info *interface;	/* Interface on which packet
						   was received. */
	struct hardware *haddr;		/* Physical link address
					   of local sender (maybe gateway). */
	struct shared_network *shared_network;
	struct option_data options [256];
	int got_requested_address;	/* True if client sent the
					   dhcp-requested-address option. */
};

struct hardware {
	u_int8_t htype;
	u_int8_t hlen;
	u_int8_t haddr [16];
};

/* A dhcp lease declaration structure. */
struct lease {
	struct lease *next;
	struct lease *prev;
	struct lease *n_uid, *n_hw;
	struct lease *waitq_next;

	struct iaddr ip_addr;
	TIME starts, ends, timestamp;
	unsigned char *uid;
	int uid_len;
	int uid_max;
	unsigned char uid_buf [32];
	char *hostname;
	char *client_hostname;
	struct host_decl *host;
	struct subnet *subnet;
	struct shared_network *shared_network;
	struct hardware hardware_addr;

	int flags;
#       define STATIC_LEASE		1
#       define BOOTP_LEASE		2
#	define DYNAMIC_BOOTP_OK		4
#	define PERSISTENT_FLAGS		(DYNAMIC_BOOTP_OK)
#	define EPHEMERAL_FLAGS		(BOOTP_LEASE)
#	define MS_NULL_TERMINATION	8
#	define ABANDONED_LEASE		16

	struct lease_state *state;
};

struct lease_state {
	struct lease_state *next;

	struct interface_info *ip;

	TIME offered_expiry;

	struct tree_cache *options [256];
	u_int32_t expiry, renewal, rebind;
	char filename [DHCP_FILE_LEN];
	char *server_name;

	struct iaddr from;

	int max_message_size;
	u_int8_t *prl;
	int prl_len;
	int got_requested_address;	/* True if client sent the
					   dhcp-requested-address option. */
	int got_server_identifier;	/* True if client sent the
					   dhcp-server-identifier option. */
	struct shared_network *shared_network;	/* Shared network of interface
						   on which request arrived. */

	u_int32_t xid;
	u_int16_t secs;
	u_int16_t bootp_flags;
	struct in_addr ciaddr;
	struct in_addr giaddr;
	u_int8_t hops;
	u_int8_t offer;
};

#define	ROOT_GROUP	0
#define HOST_DECL	1
#define SHARED_NET_DECL	2
#define SUBNET_DECL	3
#define CLASS_DECL	4
#define	GROUP_DECL	5

/* Possible modes in which discover_interfaces can run. */

#define DISCOVER_RUNNING	0
#define DISCOVER_SERVER		1
#define DISCOVER_UNCONFIGURED	2
#define DISCOVER_RELAY		3
#define DISCOVER_REQUESTED	4

/* Group of declarations that share common parameters. */
struct group {
	struct group *next;

	struct subnet *subnet;
	struct shared_network *shared_network;

	TIME default_lease_time;
	TIME max_lease_time;
	TIME bootp_lease_cutoff;
	TIME bootp_lease_length;

	char *filename;
	char *server_name;	
	struct iaddr next_server;

	int boot_unknown_clients;
	int dynamic_bootp;
	int allow_bootp;
	int allow_booting;
	int one_lease_per_client;
	int get_lease_hostnames;
	int use_host_decl_names;
	int use_lease_addr_for_default_route;
	int authoritative;
	int always_reply_rfc1048;

	struct tree_cache *options [256];
};

/* A dhcp host declaration structure. */
struct host_decl {
	struct host_decl *n_ipaddr;
	char *name;
	struct hardware interface;
	struct tree_cache *fixed_addr;
	struct group *group;
};

struct shared_network {
	struct shared_network *next;
	char *name;
	struct subnet *subnets;
	struct interface_info *interface;
	struct lease *leases;
	struct lease *insertion_point;
	struct lease *last_lease;

	struct group *group;
};

struct subnet {
	struct subnet *next_subnet;
	struct subnet *next_sibling;
	struct shared_network *shared_network;
	struct interface_info *interface;
	struct iaddr interface_address;
	struct iaddr net;
	struct iaddr netmask;

	struct group *group;
};

struct class {
	char *name;

	struct group *group;
};

/* DHCP client lease structure... */
struct client_lease {
	struct client_lease *next;		      /* Next lease in list. */
	TIME expiry, renewal, rebind;			  /* Lease timeouts. */
	struct iaddr address;			    /* Address being leased. */
	char *server_name;			     /* Name of boot server. */
	char *filename;		     /* Name of file we're supposed to boot. */
	struct string_list *medium;			  /* Network medium. */

	unsigned int is_static : 1;    /* If set, lease is from config file. */
	unsigned int is_bootp: 1;   /* If set, lease was aquired with BOOTP. */

	struct option_data options [256];    /* Options supplied with lease. */
};

/* Possible states in which the client can be. */
enum dhcp_state {
	S_REBOOTING,
	S_INIT,
	S_SELECTING,
	S_REQUESTING, 
	S_BOUND,
	S_RENEWING,
	S_REBINDING
};

/* Configuration information from the config file... */
struct client_config {
	struct option_data defaults [256]; /* Default values for options. */
	enum {
		ACTION_DEFAULT,		/* Use server value if present,
					   otherwise default. */
		ACTION_SUPERSEDE,	/* Always use default. */
		ACTION_PREPEND,		/* Prepend default to server. */
		ACTION_APPEND,		/* Append default to server. */
	} default_actions [256];

	struct option_data send_options [256]; /* Send these to server. */
	u_int8_t required_options [256]; /* Options server must supply. */
	u_int8_t requested_options [256]; /* Options to request from server. */
	int requested_option_count;	/* Number of requested options. */
	TIME timeout;			/* Start to panic if we don't get a
					   lease in this time period when
					   SELECTING. */
	TIME initial_interval;		/* All exponential backoff intervals
					   start here. */
	TIME retry_interval;		/* If the protocol failed to produce
					   an address before the timeout,
					   try the protocol again after this
					   many seconds. */
	TIME select_interval;		/* Wait this many seconds from the
					   first DHCPDISCOVER before
					   picking an offered lease. */
	TIME reboot_timeout;		/* When in INIT-REBOOT, wait this
					   long before giving up and going
					   to INIT. */
	TIME backoff_cutoff;		/* When doing exponential backoff,
					   never back off to an interval
					   longer than this amount. */
	struct string_list *media;	/* Possible network media values. */
	char *script_name;		/* Name of config script. */
	enum { IGNORE, ACCEPT, PREFER } bootp_policy;
					/* Ignore, accept or prefer BOOTP
					   responses. */
	struct string_list *medium;	/* Current network medium. */

	struct iaddrlist *reject_list;	/* Servers to reject. */
};

/* Per-interface state used in the dhcp client... */
struct client_state {
	struct client_lease *active;		  /* Currently active lease. */
	struct client_lease *new;			       /* New lease. */
	struct client_lease *offered_leases;	    /* Leases offered to us. */
	struct client_lease *leases;		/* Leases we currently hold. */
	struct client_lease *alias;			     /* Alias lease. */

	enum dhcp_state state;		/* Current state for this interface. */
	struct iaddr destination;		    /* Where to send packet. */
	u_int32_t xid;					  /* Transaction ID. */
	u_int16_t secs;			    /* secs value from DHCPDISCOVER. */
	TIME first_sending;			/* When was first copy sent? */
	TIME interval;		      /* What's the current resend interval? */
	struct string_list *medium;		   /* Last media type tried. */

	struct dhcp_packet packet;		    /* Outgoing DHCP packet. */
	int packet_length;	       /* Actual length of generated packet. */

	struct iaddr requested_address;	    /* Address we would like to get. */

	struct client_config *config;	    /* Information from config file. */

	struct string_list *env;	       /* Client script environment. */
	int envc;			/* Number of entries in environment. */
};

/* Information about each network interface. */

struct interface_info {
	struct interface_info *next;	/* Next interface in list... */
	struct shared_network *shared_network;
				/* Networks connected to this interface. */
	struct hardware hw_address;	/* Its physical address. */
	struct in_addr primary_address;	/* Primary interface address. */
	char name [IFNAMSIZ];		/* Its name... */
	int rfdesc;			/* Its read file descriptor. */
	int wfdesc;			/* Its write file descriptor, if
					   different. */
	unsigned char *rbuf;		/* Read buffer, if required. */
	size_t rbuf_max;		/* Size of read buffer. */
	size_t rbuf_offset;		/* Current offset into buffer. */
	size_t rbuf_len;		/* Length of data in buffer. */

	struct ifreq *ifp;		/* Pointer to ifreq struct. */
	u_int32_t flags;		/* Control flags... */
#define INTERFACE_REQUESTED 1
#define INTERFACE_AUTOMATIC 2

	/* Only used by DHCP client code. */
	struct client_state *client;
};

struct hardware_link {
	struct hardware_link *next;
	char name [IFNAMSIZ];
	struct hardware address;
};

struct timeout {
	struct timeout *next;
	TIME when;
	void (*func) PROTO ((void *));
	void *what;
};

struct protocol {
	struct protocol *next;
	int fd;
	void (*handler) PROTO ((struct protocol *));
	void *local;
};

/* Bitmask of dhcp option codes. */
typedef unsigned char option_mask [16];

/* DHCP Option mask manipulation macros... */
#define OPTION_ZERO(mask)	(memset (mask, 0, 16))
#define OPTION_SET(mask, bit)	(mask [bit >> 8] |= (1 << (bit & 7)))
#define OPTION_CLR(mask, bit)	(mask [bit >> 8] &= ~(1 << (bit & 7)))
#define OPTION_ISSET(mask, bit)	(mask [bit >> 8] & (1 << (bit & 7)))
#define OPTION_ISCLR(mask, bit)	(!OPTION_ISSET (mask, bit))

/* An option occupies its length plus two header bytes (code and
    length) for every 255 bytes that must be stored. */
#define OPTION_SPACE(x)		((x) + 2 * ((x) / 255 + 1))

/* Default path to dhcpd config file. */
#ifdef DEBUG
#undef _PATH_DHCPD_CONF
#define _PATH_DHCPD_CONF	"dhcpd.conf"
#undef _PATH_DHCPD_DB
#define _PATH_DHCPD_DB		"dhcpd.leases"
#else
#ifndef _PATH_DHCPD_CONF
#define _PATH_DHCPD_CONF	"/etc/dhcpd.conf"
#endif

#ifndef _PATH_DHCPD_DB
#define _PATH_DHCPD_DB		"/etc/dhcpd.leases"
#endif

#ifndef _PATH_DHCPD_PID
#define _PATH_DHCPD_PID		"/var/run/dhcpd.pid"
#endif
#endif

#ifndef _PATH_DHCLIENT_CONF
#define _PATH_DHCLIENT_CONF	"/etc/dhclient.conf"
#endif

#ifndef _PATH_DHCLIENT_PID
#define _PATH_DHCLIENT_PID	"/var/run/dhclient.pid"
#endif

#ifndef _PATH_DHCLIENT_DB
#define _PATH_DHCLIENT_DB	"/etc/dhclient.leases"
#endif

#ifndef _PATH_RESOLV_CONF
#define _PATH_RESOLV_CONF	"/etc/resolv.conf"
#endif

#ifndef _PATH_DHCRELAY_PID
#define _PATH_DHCRELAY_PID	"/var/run/dhcrelay.pid"
#endif

#ifndef DHCPD_LOG_FACILITY
#define DHCPD_LOG_FACILITY	LOG_DAEMON
#endif

#define MAX_TIME 0x7fffffff
#define MIN_TIME 0

/* External definitions... */

/* options.c */

void parse_options PROTO ((struct packet *));
void parse_option_buffer PROTO ((struct packet *, unsigned char *, int));
int cons_options PROTO ((struct packet *, struct dhcp_packet *, int,
			 struct tree_cache **, int, int, int,
			 u_int8_t *, int));
int store_options PROTO ((unsigned char *, int, struct tree_cache **,
			   unsigned char *, int, int, int, int));
char *pretty_print_option PROTO ((unsigned int,
				  unsigned char *, int, int, int));
void do_packet PROTO ((struct interface_info *,
		       struct dhcp_packet *, int,
		       unsigned int, struct iaddr, struct hardware *));

/* errwarn.c */
extern int warnings_occurred;
void error PROTO ((char *, ...));
int warn PROTO ((char *, ...));
int note PROTO ((char *, ...));
int debug PROTO ((char *, ...));
int parse_warn PROTO ((char *, ...));

/* dhcpd.c */
extern TIME cur_time;
extern struct group root_group;

extern u_int16_t local_port;
extern u_int16_t remote_port;
extern int log_priority;
extern int log_perror;

extern char *path_dhcpd_conf;
extern char *path_dhcpd_db;
extern char *path_dhcpd_pid;

int main PROTO ((int, char **, char **));
void cleanup PROTO ((void));
void lease_pinged PROTO ((struct iaddr, u_int8_t *, int));
void lease_ping_timeout PROTO ((void *));

/* conflex.c */
extern int lexline, lexchar;
extern char *token_line, *tlname;
extern char comments [4096];
extern int comment_index;
extern int eol_token;
void new_parse PROTO ((char *));
int next_token PROTO ((char **, FILE *));
int peek_token PROTO ((char **, FILE *));

/* confpars.c */
int readconf PROTO ((void));
void read_leases PROTO ((void));
int parse_statement PROTO ((FILE *,
			    struct group *, int, struct host_decl *, int));
void parse_allow_deny PROTO ((FILE *, struct group *, int));
void skip_to_semi PROTO ((FILE *));
int parse_boolean PROTO ((FILE *));
int parse_semi PROTO ((FILE *));
int parse_lbrace PROTO ((FILE *));
void parse_host_declaration PROTO ((FILE *, struct group *));
char *parse_host_name PROTO ((FILE *));
void parse_class_declaration PROTO ((FILE *, struct group *, int));
void parse_lease_time PROTO ((FILE *, TIME *));
void parse_shared_net_declaration PROTO ((FILE *, struct group *));
void parse_subnet_declaration PROTO ((FILE *, struct shared_network *));
void parse_group_declaration PROTO ((FILE *, struct group *));
void parse_hardware_param PROTO ((FILE *, struct hardware *));
char *parse_string PROTO ((FILE *));
struct tree *parse_ip_addr_or_hostname PROTO ((FILE *, int));
struct tree_cache *parse_fixed_addr_param PROTO ((FILE *));
void parse_option_param PROTO ((FILE *, struct group *));
TIME parse_timestamp PROTO ((FILE *));
struct lease *parse_lease_declaration PROTO ((FILE *));
void parse_address_range PROTO ((FILE *, struct subnet *));
TIME parse_date PROTO ((FILE *));
unsigned char *parse_numeric_aggregate PROTO ((FILE *,
					       unsigned char *, int *,
					       int, int, int));
void convert_num PROTO ((unsigned char *, char *, int, int));

/* tree.c */
pair cons PROTO ((caddr_t, pair));
struct tree_cache *tree_cache PROTO ((struct tree *));
struct tree *tree_host_lookup PROTO ((char *));
struct dns_host_entry *enter_dns_host PROTO ((char *));
struct tree *tree_const PROTO ((unsigned char *, int));
struct tree *tree_concat PROTO ((struct tree *, struct tree *));
struct tree *tree_limit PROTO ((struct tree *, int));
int tree_evaluate PROTO ((struct tree_cache *));

/* dhcp.c */
extern int outstanding_pings;

void dhcp PROTO ((struct packet *));
void dhcpdiscover PROTO ((struct packet *));
void dhcprequest PROTO ((struct packet *));
void dhcprelease PROTO ((struct packet *));
void dhcpdecline PROTO ((struct packet *));
void dhcpinform PROTO ((struct packet *));
void nak_lease PROTO ((struct packet *, struct iaddr *cip));
void ack_lease PROTO ((struct packet *, struct lease *, unsigned int, TIME));
void dhcp_reply PROTO ((struct lease *));
struct lease *find_lease PROTO ((struct packet *,
				 struct shared_network *, int *));
struct lease *mockup_lease PROTO ((struct packet *,
				   struct shared_network *,
				   struct host_decl *));

/* bootp.c */
void bootp PROTO ((struct packet *));

/* memory.c */
void enter_host PROTO ((struct host_decl *));
struct host_decl *find_hosts_by_haddr PROTO ((int, unsigned char *, int));
struct host_decl *find_hosts_by_uid PROTO ((unsigned char *, int));
struct subnet *find_host_for_network PROTO ((struct host_decl **,
					     struct iaddr *,
					     struct shared_network *));
void new_address_range PROTO ((struct iaddr, struct iaddr,
			       struct subnet *, int));
extern struct subnet *find_grouped_subnet PROTO ((struct shared_network *,
						  struct iaddr));
extern struct subnet *find_subnet PROTO ((struct iaddr));
void enter_shared_network PROTO ((struct shared_network *));
int subnet_inner_than PROTO ((struct subnet *, struct subnet *, int));
void enter_subnet PROTO ((struct subnet *));
void enter_lease PROTO ((struct lease *));
int supersede_lease PROTO ((struct lease *, struct lease *, int));
void release_lease PROTO ((struct lease *));
void abandon_lease PROTO ((struct lease *, char *));
struct lease *find_lease_by_uid PROTO ((unsigned char *, int));
struct lease *find_lease_by_hw_addr PROTO ((unsigned char *, int));
struct lease *find_lease_by_ip_addr PROTO ((struct iaddr));
void uid_hash_add PROTO ((struct lease *));
void uid_hash_delete PROTO ((struct lease *));
void hw_hash_add PROTO ((struct lease *));
void hw_hash_delete PROTO ((struct lease *));
struct class *add_class PROTO ((int, char *));
struct class *find_class PROTO ((int, unsigned char *, int));
struct group *clone_group PROTO ((struct group *, char *));
void write_leases PROTO ((void));
void dump_subnets PROTO ((void));

/* alloc.c */
VOIDPTR dmalloc PROTO ((int, char *));
void dfree PROTO ((VOIDPTR, char *));
struct packet *new_packet PROTO ((char *));
struct dhcp_packet *new_dhcp_packet PROTO ((char *));
struct tree *new_tree PROTO ((char *));
struct tree_cache *new_tree_cache PROTO ((char *));
struct hash_table *new_hash_table PROTO ((int, char *));
struct hash_bucket *new_hash_bucket PROTO ((char *));
struct lease *new_lease PROTO ((char *));
struct lease *new_leases PROTO ((int, char *));
struct subnet *new_subnet PROTO ((char *));
struct class *new_class PROTO ((char *));
struct shared_network *new_shared_network PROTO ((char *));
struct group *new_group PROTO ((char *));
struct protocol *new_protocol PROTO ((char *));
struct lease_state *new_lease_state PROTO ((char *));
struct domain_search_list *new_domain_search_list PROTO ((char *));
struct name_server *new_name_server PROTO ((char *));
void free_name_server PROTO ((struct name_server *, char *));
void free_domain_search_list PROTO ((struct domain_search_list *, char *));
void free_lease_state PROTO ((struct lease_state *, char *));
void free_protocol PROTO ((struct protocol *, char *));
void free_group PROTO ((struct group *, char *));
void free_shared_network PROTO ((struct shared_network *, char *));
void free_class PROTO ((struct class *, char *));
void free_subnet PROTO ((struct subnet *, char *));
void free_lease PROTO ((struct lease *, char *));
void free_hash_bucket PROTO ((struct hash_bucket *, char *));
void free_hash_table PROTO ((struct hash_table *, char *));
void free_tree_cache PROTO ((struct tree_cache *, char *));
void free_packet PROTO ((struct packet *, char *));
void free_dhcp_packet PROTO ((struct dhcp_packet *, char *));
void free_tree PROTO ((struct tree *, char *));

/* print.c */
char *print_hw_addr PROTO ((int, int, unsigned char *));
void print_lease PROTO ((struct lease *));
void dump_raw PROTO ((unsigned char *, int));
void dump_packet PROTO ((struct packet *));
void hash_dump PROTO ((struct hash_table *));

/* socket.c */
#if defined (USE_SOCKET_SEND) || defined (USE_SOCKET_RECEIVE) \
	|| defined (USE_SOCKET_FALLBACK)
int if_register_socket PROTO ((struct interface_info *));
#endif

#if defined (USE_SOCKET_FALLBACK) && !defined (USE_SOCKET_SEND)
void if_reinitialize_fallback PROTO ((struct interface_info *));
void if_register_fallback PROTO ((struct interface_info *));
ssize_t send_fallback PROTO ((struct interface_info *,
			      struct packet *, struct dhcp_packet *, size_t, 
			      struct in_addr,
			      struct sockaddr_in *, struct hardware *));
#endif

#ifdef USE_SOCKET_SEND
void if_reinitialize_send PROTO ((struct interface_info *));
void if_register_send PROTO ((struct interface_info *));
ssize_t send_packet PROTO ((struct interface_info *,
			    struct packet *, struct dhcp_packet *, size_t, 
			    struct in_addr,
			    struct sockaddr_in *, struct hardware *));
#endif
#if defined (USE_SOCKET_FALLBACK)
void fallback_discard PROTO ((struct protocol *));
#endif
#ifdef USE_SOCKET_RECEIVE
void if_reinitialize_receive PROTO ((struct interface_info *));
void if_register_receive PROTO ((struct interface_info *));
ssize_t receive_packet PROTO ((struct interface_info *,
			       unsigned char *, size_t,
			       struct sockaddr_in *, struct hardware *));
#endif
#if defined (USE_SOCKET_SEND)
int can_unicast_without_arp PROTO ((void));
int can_receive_unicast_unconfigured PROTO ((struct interface_info *));
void maybe_setup_fallback PROTO ((void));
#endif

/* bpf.c */
#if defined (USE_BPF_SEND) || defined (USE_BPF_RECEIVE)
int if_register_bpf PROTO ( (struct interface_info *));
#endif
#ifdef USE_BPF_SEND
void if_reinitialize_send PROTO ((struct interface_info *));
void if_register_send PROTO ((struct interface_info *));
ssize_t send_packet PROTO ((struct interface_info *,
			    struct packet *, struct dhcp_packet *, size_t,
			    struct in_addr,
			    struct sockaddr_in *, struct hardware *));
#endif
#ifdef USE_BPF_RECEIVE
void if_reinitialize_receive PROTO ((struct interface_info *));
void if_register_receive PROTO ((struct interface_info *));
ssize_t receive_packet PROTO ((struct interface_info *,
			       unsigned char *, size_t,
			       struct sockaddr_in *, struct hardware *));
#endif
#if defined (USE_BPF_SEND)
int can_unicast_without_arp PROTO ((void));
int can_receive_unicast_unconfigured PROTO ((struct interface_info *));
void maybe_setup_fallback PROTO ((void));
#endif

/* lpf.c */
#if defined (USE_LPF_SEND) || defined (USE_LPF_RECEIVE)
int if_register_lpf PROTO ( (struct interface_info *));
#endif
#ifdef USE_LPF_SEND
void if_reinitialize_send PROTO ((struct interface_info *));
void if_register_send PROTO ((struct interface_info *));
ssize_t send_packet PROTO ((struct interface_info *,
			    struct packet *, struct dhcp_packet *, size_t,
			    struct in_addr,
			    struct sockaddr_in *, struct hardware *));
#endif
#ifdef USE_LPF_RECEIVE
void if_reinitialize_receive PROTO ((struct interface_info *));
void if_register_receive PROTO ((struct interface_info *));
ssize_t receive_packet PROTO ((struct interface_info *,
			       unsigned char *, size_t,
			       struct sockaddr_in *, struct hardware *));
#endif
#if defined (USE_LPF_SEND)
int can_unicast_without_arp PROTO ((void));
int can_receive_unicast_unconfigured PROTO ((struct interface_info *));
void maybe_setup_fallback PROTO ((void));
#endif

/* nit.c */
#if defined (USE_NIT_SEND) || defined (USE_NIT_RECEIVE)
int if_register_nit PROTO ( (struct interface_info *));
#endif

#ifdef USE_NIT_SEND
void if_reinitialize_send PROTO ((struct interface_info *));
void if_register_send PROTO ((struct interface_info *));
ssize_t send_packet PROTO ((struct interface_info *,
			    struct packet *, struct dhcp_packet *, size_t,
			    struct in_addr,
			    struct sockaddr_in *, struct hardware *));
#endif
#ifdef USE_NIT_RECEIVE
void if_reinitialize_receive PROTO ((struct interface_info *));
void if_register_receive PROTO ((struct interface_info *));
ssize_t receive_packet PROTO ((struct interface_info *,
			       unsigned char *, size_t,
			       struct sockaddr_in *, struct hardware *));
#endif
#if defined (USE_NIT_SEND)
int can_unicast_without_arp PROTO ((void));
int can_receive_unicast_unconfigured PROTO ((struct interface_info *));
void maybe_setup_fallback PROTO ((void));
#endif

#ifdef USE_DLPI_SEND
void if_reinitialize_send PROTO ((struct interface_info *));
void if_register_send PROTO ((struct interface_info *));
ssize_t send_packet PROTO ((struct interface_info *,
			    struct packet *, struct dhcp_packet *, size_t,
			    struct in_addr,
			    struct sockaddr_in *, struct hardware *));
#endif
#ifdef USE_DLPI_RECEIVE
void if_reinitialize_receive PROTO ((struct interface_info *));
void if_register_receive PROTO ((struct interface_info *));
ssize_t receive_packet PROTO ((struct interface_info *,
			       unsigned char *, size_t,
			       struct sockaddr_in *, struct hardware *));
#endif
#if defined (USE_DLPI_SEND)
int can_unicast_without_arp PROTO ((void));
int can_receive_unicast_unconfigured PROTO ((struct interface_info *));
void maybe_setup_fallback PROTO ((void));
#endif

/* raw.c */
#ifdef USE_RAW_SEND
void if_reinitialize_send PROTO ((struct interface_info *));
void if_register_send PROTO ((struct interface_info *));
ssize_t send_packet PROTO ((struct interface_info *,
			    struct packet *, struct dhcp_packet *, size_t,
			    struct in_addr,
			    struct sockaddr_in *, struct hardware *));
int can_unicast_without_arp PROTO ((void));
int can_receive_unicast_unconfigured PROTO ((struct interface_info *));
void maybe_setup_fallback PROTO ((void));
#endif

/* dispatch.c */
extern struct interface_info *interfaces,
	*dummy_interfaces, *fallback_interface;
extern struct protocol *protocols;
extern int quiet_interface_discovery;
extern void (*bootp_packet_handler) PROTO ((struct interface_info *,
					    struct dhcp_packet *, int,
					    unsigned int,
					    struct iaddr, struct hardware *));
extern struct timeout *timeouts;
void discover_interfaces PROTO ((int));
struct interface_info *setup_fallback PROTO ((void));
void reinitialize_interfaces PROTO ((void));
void dispatch PROTO ((void));
int locate_network PROTO ((struct packet *));
void got_one PROTO ((struct protocol *));
void add_timeout PROTO ((TIME, void (*) PROTO ((void *)), void *));
void cancel_timeout PROTO ((void (*) PROTO ((void *)), void *));
void add_protocol PROTO ((char *, int,
			  void (*) PROTO ((struct protocol *)), void *));

void remove_protocol PROTO ((struct protocol *));

/* hash.c */
struct hash_table *new_hash PROTO ((void));
void add_hash PROTO ((struct hash_table *, unsigned char *,
		      int, unsigned char *));
void delete_hash_entry PROTO ((struct hash_table *, unsigned char *, int));
unsigned char *hash_lookup PROTO ((struct hash_table *, unsigned char *, int));

/* tables.c */
extern struct option dhcp_options [256];
extern unsigned char dhcp_option_default_priority_list [];
extern int sizeof_dhcp_option_default_priority_list;
extern char *hardware_types [256];
extern struct hash_table universe_hash;
extern struct universe dhcp_universe;
void initialize_universes PROTO ((void));

/* convert.c */
u_int32_t getULong PROTO ((unsigned char *));
int32_t getLong PROTO ((unsigned char *));
u_int16_t getUShort PROTO ((unsigned char *));
int16_t getShort PROTO ((unsigned char *));
void putULong PROTO ((unsigned char *, u_int32_t));
void putLong PROTO ((unsigned char *, int32_t));
void putUShort PROTO ((unsigned char *, unsigned int));
void putShort PROTO ((unsigned char *, int));

/* inet.c */
struct iaddr subnet_number PROTO ((struct iaddr, struct iaddr));
struct iaddr ip_addr PROTO ((struct iaddr, struct iaddr, u_int32_t));
struct iaddr broadcast_addr PROTO ((struct iaddr, struct iaddr));
u_int32_t host_addr PROTO ((struct iaddr, struct iaddr));
int addr_eq PROTO ((struct iaddr, struct iaddr));
char *piaddr PROTO ((struct iaddr));

/* dhclient.c */
extern char *path_dhclient_conf;
extern char *path_dhclient_db;
extern char *path_dhclient_pid;
extern int interfaces_requested;

extern struct client_config top_level_config;

void dhcpoffer PROTO ((struct packet *));
void dhcpack PROTO ((struct packet *));
void dhcpnak PROTO ((struct packet *));

void send_discover PROTO ((void *));
void send_request PROTO ((void *));
void send_release PROTO ((void *));
void send_decline PROTO ((void *));

void state_reboot PROTO ((void *));
void state_init PROTO ((void *));
void state_selecting PROTO ((void *));
void state_requesting PROTO ((void *));
void state_bound PROTO ((void *));
void state_panic PROTO ((void *));

void bind_lease PROTO ((struct interface_info *));

void make_discover PROTO ((struct interface_info *, struct client_lease *));
void make_request PROTO ((struct interface_info *, struct client_lease *));
void make_decline PROTO ((struct interface_info *, struct client_lease *));
void make_release PROTO ((struct interface_info *, struct client_lease *));

void free_client_lease PROTO ((struct client_lease *));
void rewrite_client_leases PROTO ((void));
void write_client_lease PROTO ((struct interface_info *,
				 struct client_lease *, int));

void script_init PROTO ((struct interface_info *, char *,
			 struct string_list *));
void script_write_params PROTO ((struct interface_info *,
				 char *, struct client_lease *));
int script_go PROTO ((struct interface_info *));
void client_envadd PROTO ((struct client_state *,
			   const char *, const char *, const char *, ...));
int dhcp_option_ev_name (char *, size_t, struct option *);

struct client_lease *packet_to_lease PROTO ((struct packet *));
void go_daemon PROTO ((void));
void write_client_pid_file PROTO ((void));
void status_message PROTO ((struct sysconf_header *, void *));
void client_location_changed PROTO ((void));

/* db.c */
int write_lease PROTO ((struct lease *));
int commit_leases PROTO ((void));
void db_startup PROTO ((void));
void new_lease_file PROTO ((void));

/* packet.c */
u_int32_t checksum PROTO ((unsigned char *, int, u_int32_t));
u_int32_t wrapsum PROTO ((u_int32_t));
void assemble_hw_header PROTO ((struct interface_info *, unsigned char *,
				int *, struct hardware *));
void assemble_udp_ip_header PROTO ((struct interface_info *, unsigned char *,
				    int *, u_int32_t, u_int32_t, unsigned int,
				    unsigned char *, int));
ssize_t decode_hw_header PROTO ((struct interface_info *, unsigned char *,
				 int, struct hardware *));
ssize_t decode_udp_ip_header PROTO ((struct interface_info *, unsigned char *,
				     int, struct sockaddr_in *,
				     unsigned char *, int));

/* ethernet.c */
void assemble_ethernet_header PROTO ((struct interface_info *, unsigned char *,
				      int *, struct hardware *));
ssize_t decode_ethernet_header PROTO ((struct interface_info *,
				       unsigned char *,
				       int, struct hardware *));

/* tr.c */
void assemble_tr_header PROTO ((struct interface_info *, unsigned char *,
				int *, struct hardware *));
ssize_t decode_tr_header PROTO ((struct interface_info *,
				 unsigned char *,
				 int, struct hardware *));

/* dhxpxlt.c */
void convert_statement PROTO ((FILE *));
void convert_host_statement PROTO ((FILE *, jrefproto));
void convert_host_name PROTO ((FILE *, jrefproto));
void convert_class_statement PROTO ((FILE *, jrefproto, int));
void convert_class_decl PROTO ((FILE *, jrefproto));
void convert_lease_time PROTO ((FILE *, jrefproto, char *));
void convert_shared_net_statement PROTO ((FILE *, jrefproto));
void convert_subnet_statement PROTO ((FILE *, jrefproto));
void convert_subnet_decl PROTO ((FILE *, jrefproto));
void convert_host_decl PROTO ((FILE *, jrefproto));
void convert_hardware_decl PROTO ((FILE *, jrefproto));
void convert_hardware_addr PROTO ((FILE *, jrefproto));
void convert_filename_decl PROTO ((FILE *, jrefproto));
void convert_servername_decl PROTO ((FILE *, jrefproto));
void convert_ip_addr_or_hostname PROTO ((FILE *, jrefproto, int));
void convert_fixed_addr_decl PROTO ((FILE *, jrefproto));
void convert_option_decl PROTO ((FILE *, jrefproto));
void convert_timestamp PROTO ((FILE *, jrefproto));
void convert_lease_statement PROTO ((FILE *, jrefproto));
void convert_address_range PROTO ((FILE *, jrefproto));
void convert_date PROTO ((FILE *, jrefproto, char *));
void convert_numeric_aggregate PROTO ((FILE *, jrefproto, int, int, int, int));
void indent PROTO ((int));

/* route.c */
void add_route_direct PROTO ((struct interface_info *, struct in_addr));
void add_route_net PROTO ((struct interface_info *, struct in_addr,
			   struct in_addr));
void add_route_default_gateway PROTO ((struct interface_info *, 
				       struct in_addr));
void remove_routes PROTO ((struct in_addr));
void remove_if_route PROTO ((struct interface_info *, struct in_addr));
void remove_all_if_routes PROTO ((struct interface_info *));
void set_netmask PROTO ((struct interface_info *, struct in_addr));
void set_broadcast_addr PROTO ((struct interface_info *, struct in_addr));
void set_ip_address PROTO ((struct interface_info *, struct in_addr));

/* clparse.c */
int read_client_conf PROTO ((void));
void read_client_leases PROTO ((void));
void parse_client_statement PROTO ((FILE *, struct interface_info *,
				    struct client_config *));
int parse_X PROTO ((FILE *, u_int8_t *, int));
int parse_option_list PROTO ((FILE *, u_int8_t *));
void parse_interface_declaration PROTO ((FILE *, struct client_config *));
struct interface_info *interface_or_dummy PROTO ((char *));
void make_client_state PROTO ((struct interface_info *));
void make_client_config PROTO ((struct interface_info *,
				struct client_config *));
void parse_client_lease_statement PROTO ((FILE *, int));
void parse_client_lease_declaration PROTO ((FILE *, struct client_lease *,
					    struct interface_info **));
struct option *parse_option_decl PROTO ((FILE *, struct option_data *));
void parse_string_list PROTO ((FILE *, struct string_list **, int));
int parse_ip_addr PROTO ((FILE *, struct iaddr *));
void parse_reject_statement PROTO ((FILE *, struct client_config *));

/* dhcrelay.c */
void relay PROTO ((struct interface_info *, struct dhcp_packet *, int,
		   unsigned int, struct iaddr, struct hardware *));

/* icmp.c */
void icmp_startup PROTO ((int, void (*) PROTO ((struct iaddr,
						u_int8_t *, int))));
int icmp_echorequest PROTO ((struct iaddr *));
void icmp_echoreply PROTO ((struct protocol *));

/* dns.c */
void dns_startup PROTO ((void));
int ns_inaddr_lookup PROTO ((u_int16_t, struct iaddr));
void dns_packet PROTO ((struct protocol *));

/* resolv.c */
extern char path_resolv_conf [];
struct name_server *name_servers;
struct domain_search_list *domains;

void read_resolv_conf PROTO ((TIME));
struct sockaddr_in *pick_name_server PROTO ((void));

/* inet_addr.c */
#ifdef NEED_INET_ATON
int inet_aton PROTO ((const char *, struct in_addr *));
#endif

/* sysconf.c */
void sysconf_startup PROTO ((void (*) (struct sysconf_header *, void *)));
void sysconf_restart PROTO ((void *));
void sysconf_message PROTO ((struct protocol *proto));
