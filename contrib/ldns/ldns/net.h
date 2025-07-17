/*
 * net.h
 *
 * DNS Resolver definitions
 *
 * a Net::DNS like library for C
 *
 * (c) NLnet Labs, 2005-2006
 *
 * See the file LICENSE for the license
 */

#ifndef LDNS_NET_H
#define LDNS_NET_H

#include <ldns/ldns.h>
#include <sys/socket.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LDNS_DEFAULT_TIMEOUT_SEC 5
#define LDNS_DEFAULT_TIMEOUT_USEC 0

/**
 * \file
 *
 * Contains functions to send and receive packets over a network.
 */

/**
 * Sends a buffer to an ip using udp and return the response as a ldns_pkt
 * \param[in] qbin the ldns_buffer to be send
 * \param[in] to the ip addr to send to
 * \param[in] tolen length of the ip addr
 * \param[in] timeout the timeout value for the network
 * \param[out] answersize size of the packet
 * \param[out] result packet with the answer
 * \return status
 */
ldns_status ldns_udp_send(uint8_t **result, ldns_buffer *qbin, const struct sockaddr_storage *to, socklen_t tolen, struct timeval timeout, size_t *answersize);

/**
 * Send an udp query and don't wait for an answer but return
 * the socket
 * \param[in] qbin the ldns_buffer to be send
 * \param[in] to the ip addr to send to
 * \param[in] tolen length of the ip addr
 * \param[in] timeout *unused*, was the timeout value for the network
 * \return the socket used or -1 on failure
 */
int ldns_udp_bgsend2(ldns_buffer *qbin, const struct sockaddr_storage *to, socklen_t tolen, struct timeval timeout);

/**
 * Send an udp query and don't wait for an answer but return
 * the socket
 * This function has the flaw that it returns 0 on failure, but 0 could be a
 * valid socket.  Please use ldns_udp_bgsend2 instead of this function.
 * \param[in] qbin the ldns_buffer to be send
 * \param[in] to the ip addr to send to
 * \param[in] tolen length of the ip addr
 * \param[in] timeout *unused*, was the timeout value for the network
 * \return the socket used or 0 on failure
 */
int ldns_udp_bgsend(ldns_buffer *qbin, const struct sockaddr_storage *to, socklen_t tolen, struct timeval timeout);

/**
 * Send an tcp query and don't wait for an answer but return
 * the socket
 * \param[in] qbin the ldns_buffer to be send
 * \param[in] to the ip addr to send to
 * \param[in] tolen length of the ip addr
 * \param[in] timeout the timeout value for the connect attempt
 * \return the socket used or -1 on failure
 */
int ldns_tcp_bgsend2(ldns_buffer *qbin, const struct sockaddr_storage *to, socklen_t tolen, struct timeval timeout);

/**
 * Send an tcp query and don't wait for an answer but return
 * the socket
 * This function has the flaw that it returns 0 on failure, but 0 could be a
 * valid socket.  Please use ldns_tcp_bgsend2 instead of this function.
 * \param[in] qbin the ldns_buffer to be send
 * \param[in] to the ip addr to send to
 * \param[in] tolen length of the ip addr
 * \param[in] timeout the timeout value for the connect attempt
 * \return the socket used or 0 on failure
 */
int ldns_tcp_bgsend(ldns_buffer *qbin, const struct sockaddr_storage *to, socklen_t tolen, struct timeval timeout);

/**
 * Sends a buffer to an ip using tcp and return the response as a ldns_pkt
 * \param[in] qbin the ldns_buffer to be send
 * \param[in] qbin the ldns_buffer to be send
 * \param[in] to the ip addr to send to
 * \param[in] tolen length of the ip addr
 * \param[in] timeout the timeout value for the network
 * \param[out] answersize size of the packet
 * \param[out] result packet with the answer
 * \return status
 */
ldns_status ldns_tcp_send(uint8_t **result, ldns_buffer *qbin, const struct sockaddr_storage *to, socklen_t tolen, struct timeval timeout, size_t *answersize);

/**
 * Sends ptk to the nameserver at the resolver object. Returns the data
 * as a ldns_pkt
 * 
 * \param[out] pkt packet received from the nameserver
 * \param[in] r the resolver to use 
 * \param[in] query_pkt the query to send
 * \return status
 */
ldns_status ldns_send(ldns_pkt **pkt, ldns_resolver *r, const ldns_pkt *query_pkt);

/**
 * Sends and ldns_buffer (presumably containing a packet to the nameserver at the resolver object. Returns the data
 * as a ldns_pkt
 * 
 * \param[out] pkt packet received from the nameserver
 * \param[in] r the resolver to use 
 * \param[in] qb the buffer to send
 * \param[in] tsig_mac the tsig MAC to authenticate the response with (NULL to do no TSIG authentication)
 * \return status
 */
ldns_status ldns_send_buffer(ldns_pkt **pkt, ldns_resolver *r, ldns_buffer *qb, ldns_rdf *tsig_mac);

/**
 * Create a tcp socket to the specified address
 * \param[in] to ip and family
 * \param[in] tolen length of to
 * \param[in] timeout timeout for the connect attempt
 * \return a socket descriptor or -1 on failure
 */
int ldns_tcp_connect2(const struct sockaddr_storage *to, socklen_t tolen, struct timeval timeout);

/**
 * Create a tcp socket to the specified address
 * This function has the flaw that it returns 0 on failure, but 0 could be a
 * valid socket.  Please use ldns_tcp_connect2 instead of this function.
 * \param[in] to ip and family
 * \param[in] tolen length of to
 * \param[in] timeout timeout for the connect attempt
 * \return a socket descriptor or 0 on failure
 */
int ldns_tcp_connect(const struct sockaddr_storage *to, socklen_t tolen, struct timeval timeout);

/**
 * Create a udp socket to the specified address
 * \param[in] to ip and family
 * \param[in] timeout *unused*, was timeout for the socket
 * \return a socket descriptor or -1 on failure
 */
int ldns_udp_connect2(const struct sockaddr_storage *to, struct timeval timeout);

/**
 * Create a udp socket to the specified address
 * This function has the flaw that it returns 0 on failure, but 0 could be a
 * valid socket.  Please use ldns_udp_connect2 instead of this function.
 * \param[in] to ip and family
 * \param[in] timeout *unused*, was timeout for the socket
 * \return a socket descriptor or 0 on failure
 */
int ldns_udp_connect(const struct sockaddr_storage *to, struct timeval timeout);

/**
 * send a query via tcp to a server. Don't want for the answer
 *
 * \param[in] qbin the buffer to send
 * \param[in] sockfd the socket to use
 * \param[in] to which ip to send it
 * \param[in] tolen socketlen
 * \return number of bytes sent
 */
ssize_t ldns_tcp_send_query(ldns_buffer *qbin, int sockfd, const struct sockaddr_storage *to, socklen_t tolen);

/**
 * send a query via udp to a server. Don;t want for the answer
 *
 * \param[in] qbin the buffer to send
 * \param[in] sockfd the socket to use
 * \param[in] to which ip to send it
 * \param[in] tolen socketlen
 * \return number of bytes sent
 */
ssize_t ldns_udp_send_query(ldns_buffer *qbin, int sockfd, const struct sockaddr_storage *to, socklen_t tolen);

/**
 * Gives back a raw packet from the wire and reads the header data from the given
 * socket. Allocates the data (of size size) itself, so don't forget to free
 *
 * \param[in] sockfd the socket to read from
 * \param[out] size the number of bytes that are read
 * \param[in] timeout the time allowed between packets.
 * \return the data read
 */
uint8_t *ldns_tcp_read_wire_timeout(int sockfd, size_t *size, struct timeval timeout);

/**
 * This routine may block. Use ldns_tcp_read_wire_timeout, it checks timeouts.
 * Gives back a raw packet from the wire and reads the header data from the given
 * socket. Allocates the data (of size size) itself, so don't forget to free
 *
 * \param[in] sockfd the socket to read from
 * \param[out] size the number of bytes that are read
 * \return the data read
 */
uint8_t *ldns_tcp_read_wire(int sockfd, size_t *size);

/**
 * Gives back a raw packet from the wire and reads the header data from the given
 * socket. Allocates the data (of size size) itself, so don't forget to free
 *
 * \param[in] sockfd the socket to read from
 * \param[in] fr the address of the client (if applicable)
 * \param[in] *frlen the length of the client's addr (if applicable)
 * \param[out] size the number of bytes that are read
 * \return the data read
 */
uint8_t *ldns_udp_read_wire(int sockfd, size_t *size, struct sockaddr_storage *fr, socklen_t *frlen);

/**
 * returns the native sockaddr representation from the rdf.
 * \param[in] rd the ldns_rdf to operate on
 * \param[in] port what port to use. 0 means; use default (53)
 * \param[out] size what is the size of the sockaddr_storage
 * \return struct sockaddr* the address in the format so other
 * functions can use it (sendto)
 */
struct sockaddr_storage * ldns_rdf2native_sockaddr_storage(const ldns_rdf *rd, uint16_t port, size_t *size);

/**
 * returns an rdf with the sockaddr info. works for ip4 and ip6
 * \param[in] sock the struct sockaddr_storage to convert
 * \param[in] port what port was used. When NULL this is not set
 * \return ldns_rdf* with the address
 */
ldns_rdf * ldns_sockaddr_storage2rdf(const struct sockaddr_storage *sock, uint16_t *port);

/**
 * Prepares the resolver for an axfr query
 * The query is sent and the answers can be read with ldns_axfr_next
 * \param[in] resolver the resolver to use
 * \param[in] domain the domain to axfr
 * \param[in] c the class to use
 * \return ldns_status the status of the transfer
 */
ldns_status ldns_axfr_start(ldns_resolver *resolver, const ldns_rdf *domain, ldns_rr_class c);

#ifdef __cplusplus
}
#endif

#endif  /* LDNS_NET_H */
