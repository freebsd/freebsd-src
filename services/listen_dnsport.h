/*
 * services/listen_dnsport.h - listen on port 53 for incoming DNS queries.
 *
 * Copyright (c) 2007, NLnet Labs. All rights reserved.
 *
 * This software is open source.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 
 * Neither the name of the NLNET LABS nor the names of its contributors may
 * be used to endorse or promote products derived from this software without
 * specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * \file
 *
 * This file has functions to get queries from clients.
 */

#ifndef LISTEN_DNSPORT_H
#define LISTEN_DNSPORT_H

#include "util/netevent.h"
#ifdef HAVE_NGHTTP2_NGHTTP2_H
#include <nghttp2/nghttp2.h>
#endif
struct listen_list;
struct config_file;
struct addrinfo;
struct sldns_buffer;
struct tcl_list;

/**
 * Listening for queries structure.
 * Contains list of query-listen sockets.
 */
struct listen_dnsport {
	/** Base for select calls */
	struct comm_base* base;

	/** buffer shared by UDP connections, since there is only one
	    datagram at any time. */
	struct sldns_buffer* udp_buff;
#ifdef USE_DNSCRYPT
	struct sldns_buffer* dnscrypt_udp_buff;
#endif
	/** list of comm points used to get incoming events */
	struct listen_list* cps;
};

/**
 * Single linked list to store event points.
 */
struct listen_list {
	/** next in list */
	struct listen_list* next;
	/** event info */
	struct comm_point* com;
};

/**
 * type of ports
 */
enum listen_type {
	/** udp type */
	listen_type_udp,
	/** tcp type */
	listen_type_tcp,
	/** udp ipv6 (v4mapped) for use with ancillary data */
	listen_type_udpancil,
	/** ssl over tcp type */
	listen_type_ssl,
	/** udp type  + dnscrypt*/
	listen_type_udp_dnscrypt,
	/** tcp type + dnscrypt */
	listen_type_tcp_dnscrypt,
	/** udp ipv6 (v4mapped) for use with ancillary data + dnscrypt*/
	listen_type_udpancil_dnscrypt,
	/** HTTP(2) over TLS over TCP */
	listen_type_http
};

/**
 * Single linked list to store shared ports that have been 
 * opened for use by all threads.
 */
struct listen_port {
	/** next in list */
	struct listen_port* next;
	/** file descriptor, open and ready for use */
	int fd;
	/** type of file descriptor, udp or tcp */
	enum listen_type ftype;
};

/**
 * Create shared listening ports
 * Getaddrinfo, create socket, bind and listen to zero or more 
 * interfaces for IP4 and/or IP6, for UDP and/or TCP.
 * On the given port number. It creates the sockets.
 * @param cfg: settings on what ports to open.
 * @param ifs: interfaces to open, array of IP addresses, "ip[@port]".
 * @param num_ifs: length of ifs.
 * @param reuseport: set to true if you want reuseport, or NULL to not have it,
 *   set to false on exit if reuseport failed to apply (because of no
 *   kernel support).
 * @return: linked list of ports or NULL on error.
 */
struct listen_port* listening_ports_open(struct config_file* cfg,
	char** ifs, int num_ifs, int* reuseport);

/**
 * Close and delete the (list of) listening ports.
 */
void listening_ports_free(struct listen_port* list);

/**
 * Resolve interface names in config and store result IP addresses
 * @param cfg: config
 * @param resif: string array (malloced array of malloced strings) with
 * 	result.  NULL if cfg has none.
 * @param num_resif: length of resif.  Zero if cfg has zero num_ifs.
 * @return 0 on failure.
 */
int resolve_interface_names(struct config_file* cfg, char*** resif,
	int* num_resif);

/**
 * Create commpoints with for this thread for the shared ports.
 * @param base: the comm_base that provides event functionality.
 *	for default all ifs.
 * @param ports: the list of shared ports.
 * @param bufsize: size of datagram buffer.
 * @param tcp_accept_count: max number of simultaneous TCP connections 
 * 	from clients.
 * @param tcp_idle_timeout: idle timeout for TCP connections in msec.
 * @param harden_large_queries: whether query size should be limited.
 * @param http_max_streams: maximum number of HTTP/2 streams per connection.
 * @param http_endpoint: HTTP endpoint to service queries on
 * @param tcp_conn_limit: TCP connection limit info.
 * @param sslctx: nonNULL if ssl context.
 * @param dtenv: nonNULL if dnstap enabled.
 * @param cb: callback function when a request arrives. It is passed
 *	  the packet and user argument. Return true to send a reply.
 * @param cb_arg: user data argument for callback function.
 * @return: the malloced listening structure, ready for use. NULL on error.
 */
struct listen_dnsport*
listen_create(struct comm_base* base, struct listen_port* ports,
	size_t bufsize, int tcp_accept_count, int tcp_idle_timeout,
	int harden_large_queries, uint32_t http_max_streams,
	char* http_endpoint, struct tcl_list* tcp_conn_limit, void* sslctx,
	struct dt_env* dtenv, comm_point_callback_type* cb, void *cb_arg);

/**
 * delete the listening structure
 * @param listen: listening structure.
 */
void listen_delete(struct listen_dnsport* listen);

/**
 * delete listen_list of commpoints. Calls commpointdelete() on items.
 * This may close the fds or not depending on flags.
 * @param list: to delete.
 */
void listen_list_delete(struct listen_list* list);

/**
 * get memory size used by the listening structs
 * @param listen: listening structure.
 * @return: size in bytes.
 */
size_t listen_get_mem(struct listen_dnsport* listen);

/**
 * stop accept handlers for TCP (until enabled again)
 * @param listen: listening structure.
 */
void listen_stop_accept(struct listen_dnsport* listen);

/**
 * start accept handlers for TCP (was stopped before)
 * @param listen: listening structure.
 */
void listen_start_accept(struct listen_dnsport* listen);

/**
 * Create and bind nonblocking UDP socket
 * @param family: for socket call.
 * @param socktype: for socket call.
 * @param addr: for bind call.
 * @param addrlen: for bind call.
 * @param v6only: if enabled, IP6 sockets get IP6ONLY option set.
 * 	if enabled with value 2 IP6ONLY option is disabled.
 * @param inuse: on error, this is set true if the port was in use.
 * @param noproto: on error, this is set true if cause is that the
	IPv6 proto (family) is not available.
 * @param rcv: set size on rcvbuf with socket option, if 0 it is not set.
 * @param snd: set size on sndbuf with socket option, if 0 it is not set.
 * @param listen: if true, this is a listening UDP port, eg port 53, and 
 * 	set SO_REUSEADDR on it.
 * @param reuseport: if nonNULL and true, try to set SO_REUSEPORT on
 * 	listening UDP port.  Set to false on return if it failed to do so.
 * @param transparent: set IP_TRANSPARENT socket option.
 * @param freebind: set IP_FREEBIND socket option.
 * @param use_systemd: if true, fetch sockets from systemd.
 * @param dscp: DSCP to use.
 * @return: the socket. -1 on error.
 */
int create_udp_sock(int family, int socktype, struct sockaddr* addr, 
	socklen_t addrlen, int v6only, int* inuse, int* noproto, int rcv,
	int snd, int listen, int* reuseport, int transparent, int freebind, int use_systemd, int dscp);

/**
 * Create and bind TCP listening socket
 * @param addr: address info ready to make socket.
 * @param v6only: enable ip6 only flag on ip6 sockets.
 * @param noproto: if error caused by lack of protocol support.
 * @param reuseport: if nonNULL and true, try to set SO_REUSEPORT on
 * 	listening UDP port.  Set to false on return if it failed to do so.
 * @param transparent: set IP_TRANSPARENT socket option.
 * @param mss: maximum segment size of the socket. if zero, leaves the default. 
 * @param nodelay: if true set TCP_NODELAY and TCP_QUICKACK socket options.
 * @param freebind: set IP_FREEBIND socket option.
 * @param use_systemd: if true, fetch sockets from systemd.
 * @param dscp: DSCP to use.
 * @return: the socket. -1 on error.
 */
int create_tcp_accept_sock(struct addrinfo *addr, int v6only, int* noproto,
	int* reuseport, int transparent, int mss, int nodelay, int freebind,
	int use_systemd, int dscp);

/**
 * Create and bind local listening socket
 * @param path: path to the socket.
 * @param noproto: on error, this is set true if cause is that local sockets
 *	are not supported.
 * @param use_systemd: if true, fetch sockets from systemd.
 * @return: the socket. -1 on error.
 */
int create_local_accept_sock(const char* path, int* noproto, int use_systemd);

/**
 * TCP request info.  List of requests outstanding on the channel, that
 * are asked for but not yet answered back.
 */
struct tcp_req_info {
	/** the TCP comm point for this.  Its buffer is used for read/write */
	struct comm_point* cp;
	/** the buffer to use to spool reply from mesh into,
	 * it can then be copied to the result list and written.
	 * it is a pointer to the shared udp buffer. */
	struct sldns_buffer* spool_buffer;
	/** are we in worker_handle function call (for recursion callback)*/
	int in_worker_handle;
	/** is the comm point dropped (by worker handle).
	 * That means we have to disconnect the channel. */
	int is_drop;
	/** is the comm point set to send_reply (by mesh new client in worker
	 * handle), if so answer is available in c.buffer */
	int is_reply;
	/** read channel has closed, just write pending results */
	int read_is_closed;
	/** read again */
	int read_again;
	/** number of outstanding requests */
	int num_open_req;
	/** list of outstanding requests */
	struct tcp_req_open_item* open_req_list;
	/** number of pending writeable results */
	int num_done_req;
	/** list of pending writable result packets, malloced one at a time */
	struct tcp_req_done_item* done_req_list;
};

/**
 * List of open items in TCP channel
 */
struct tcp_req_open_item {
	/** next in list */
	struct tcp_req_open_item* next;
	/** the mesh area of the mesh_state */
	struct mesh_area* mesh;
	/** the mesh state */
	struct mesh_state* mesh_state;
};

/**
 * List of done items in TCP channel
 */
struct tcp_req_done_item {
	/** next in list */
	struct tcp_req_done_item* next;
	/** the buffer with packet contents */
	uint8_t* buf;
	/** length of the buffer */
	size_t len;
};

/**
 * Create tcp request info structure that keeps track of open
 * requests on the TCP channel that are resolved at the same time,
 * and the pending results that have to get written back to that client.
 * @param spoolbuf: shared buffer
 * @return new structure or NULL on alloc failure.
 */
struct tcp_req_info* tcp_req_info_create(struct sldns_buffer* spoolbuf);

/**
 * Delete tcp request structure.  Called by owning commpoint.
 * Removes mesh entry references and stored results from the lists.
 * @param req: the tcp request info
 */
void tcp_req_info_delete(struct tcp_req_info* req);

/**
 * Clear tcp request structure.  Removes list entries, sets it up ready
 * for the next connection.
 * @param req: tcp request info structure.
 */
void tcp_req_info_clear(struct tcp_req_info* req);

/**
 * Remove mesh state entry from list in tcp_req_info.
 * caller has to manage the mesh state reply entry in the mesh state.
 * @param req: the tcp req info that has the entry removed from the list.
 * @param m: the state removed from the list.
 */
void tcp_req_info_remove_mesh_state(struct tcp_req_info* req,
	struct mesh_state* m);

/**
 * Handle write done of the last result packet
 * @param req: the tcp req info.
 */
void tcp_req_info_handle_writedone(struct tcp_req_info* req);

/**
 * Handle read done of a new request from the client
 * @param req: the tcp req info.
 */
void tcp_req_info_handle_readdone(struct tcp_req_info* req);

/**
 * Add mesh state to the tcp req list of open requests.
 * So the comm_reply can be removed off the mesh reply list when
 * the tcp channel has to be closed (for other reasons then that that
 * request was done, eg. channel closed by client or some format error).
 * @param req: tcp req info structure.  It keeps track of the simultaneous
 * 	requests and results on a tcp (or TLS) channel.
 * @param mesh: mesh area for the state.
 * @param m: mesh state to add.
 * @return 0 on failure (malloc failure).
 */
int tcp_req_info_add_meshstate(struct tcp_req_info* req,
	struct mesh_area* mesh, struct mesh_state* m);

/**
 * Send reply on tcp simultaneous answer channel.  May queue it up.
 * @param req: request info structure.
 */
void tcp_req_info_send_reply(struct tcp_req_info* req);

/** the read channel has closed
 * @param req: request. remaining queries are looked up and answered. 
 * @return zero if nothing to do, just close the tcp.
 */
int tcp_req_info_handle_read_close(struct tcp_req_info* req);

/** get the size of currently used tcp stream wait buffers (in bytes) */
size_t tcp_req_info_get_stream_buffer_size(void);

/** get the size of currently used HTTP2 query buffers (in bytes) */
size_t http2_get_query_buffer_size(void);
/** get the size of currently used HTTP2 response buffers (in bytes) */
size_t http2_get_response_buffer_size(void);

#ifdef HAVE_NGHTTP2
/** 
 * Create nghttp2 callbacks to handle HTTP2 requests.
 * @return malloc'ed struct, NULL on failure
 */
nghttp2_session_callbacks* http2_req_callbacks_create();

/** Free http2 stream buffers and decrease buffer counters */
void http2_req_stream_clear(struct http2_stream* h2_stream);

/**
 * DNS response ready to be submitted to nghttp2, to be prepared for sending
 * out. Response is stored in c->buffer. Copy to rbuffer because the c->buffer
 * might be used before this will be send out.
 * @param h2_session: http2 session, containing c->buffer which contains answer
 * @param h2_stream: http2 stream, containing buffer to store answer in
 * @return 0 on error, 1 otherwise
 */
int http2_submit_dns_response(struct http2_session* h2_session);
#else
int http2_submit_dns_response(void* v);
#endif /* HAVE_NGHTTP2 */

char* set_ip_dscp(int socket, int addrfamily, int ds);

#endif /* LISTEN_DNSPORT_H */
