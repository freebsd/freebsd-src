/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * Interface for the packet protocol functions.
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 */

/* RCSID("$OpenBSD: packet.h,v 1.17 2000/09/07 20:27:52 deraadt Exp $"); */
/* $FreeBSD$ */

#ifndef PACKET_H
#define PACKET_H

#include <openssl/bn.h>

/*
 * Sets the socket used for communication.  Disables encryption until
 * packet_set_encryption_key is called.  It is permissible that fd_in and
 * fd_out are the same descriptor; in that case it is assumed to be a socket.
 */
void    packet_set_connection(int fd_in, int fd_out);

/* Puts the connection file descriptors into non-blocking mode. */
void    packet_set_nonblocking(void);

/* Returns the file descriptor used for input. */
int     packet_get_connection_in(void);

/* Returns the file descriptor used for output. */
int     packet_get_connection_out(void);

/*
 * Closes the connection (both descriptors) and clears and frees internal
 * data structures.
 */
void    packet_close(void);

/*
 * Causes any further packets to be encrypted using the given key.  The same
 * key is used for both sending and reception.  However, both directions are
 * encrypted independently of each other.  Cipher types are defined in ssh.h.
 */
void
packet_set_encryption_key(const unsigned char *key, unsigned int keylen,
    int cipher_type);

/*
 * Sets remote side protocol flags for the current connection.  This can be
 * called at any time.
 */
void    packet_set_protocol_flags(unsigned int flags);

/* Returns the remote protocol flags set earlier by the above function. */
unsigned int packet_get_protocol_flags(void);

/* Enables compression in both directions starting from the next packet. */
void    packet_start_compression(int level);

/*
 * Informs that the current session is interactive.  Sets IP flags for
 * optimal performance in interactive use.
 */
void    packet_set_interactive(int interactive, int keepalives);

/* Returns true if the current connection is interactive. */
int     packet_is_interactive(void);

/* Starts constructing a packet to send. */
void    packet_start(int type);

/* Appends a character to the packet data. */
void    packet_put_char(int ch);

/* Appends an integer to the packet data. */
void    packet_put_int(unsigned int value);

/* Appends an arbitrary precision integer to packet data. */
void    packet_put_bignum(BIGNUM * value);
void    packet_put_bignum2(BIGNUM * value);

/* Appends a string to packet data. */
void    packet_put_string(const char *buf, unsigned int len);
void    packet_put_cstring(const char *str);
void    packet_put_raw(const char *buf, unsigned int len);

/*
 * Finalizes and sends the packet.  If the encryption key has been set,
 * encrypts the packet before sending.
 */
void    packet_send(void);

/* Waits until a packet has been received, and returns its type. */
int     packet_read(int *payload_len_ptr);

/*
 * Waits until a packet has been received, verifies that its type matches
 * that given, and gives a fatal error and exits if there is a mismatch.
 */
void    packet_read_expect(int *payload_len_ptr, int type);

/*
 * Checks if a full packet is available in the data received so far via
 * packet_process_incoming.  If so, reads the packet; otherwise returns
 * SSH_MSG_NONE.  This does not wait for data from the connection.
 * SSH_MSG_DISCONNECT is handled specially here.  Also, SSH_MSG_IGNORE
 * messages are skipped by this function and are never returned to higher
 * levels.
 */
int     packet_read_poll(int *packet_len_ptr);

/*
 * Buffers the given amount of input characters.  This is intended to be used
 * together with packet_read_poll.
 */
void    packet_process_incoming(const char *buf, unsigned int len);

/* Returns a character (0-255) from the packet data. */
unsigned int packet_get_char(void);

/* Returns an integer from the packet data. */
unsigned int packet_get_int(void);

/*
 * Returns an arbitrary precision integer from the packet data.  The integer
 * must have been initialized before this call.
 */
void    packet_get_bignum(BIGNUM * value, int *length_ptr);
void    packet_get_bignum2(BIGNUM * value, int *length_ptr);
char	*packet_get_raw(int *length_ptr);

/*
 * Returns a string from the packet data.  The string is allocated using
 * xmalloc; it is the responsibility of the calling program to free it when
 * no longer needed.  The length_ptr argument may be NULL, or point to an
 * integer into which the length of the string is stored.
 */
char   *packet_get_string(unsigned int *length_ptr);

/*
 * Logs the error in syslog using LOG_INFO, constructs and sends a disconnect
 * packet, closes the connection, and exits.  This function never returns.
 * The error message should not contain a newline.  The total length of the
 * message must not exceed 1024 bytes.
 */
void    packet_disconnect(const char *fmt,...) __attribute__((format(printf, 1, 2)));

/*
 * Sends a diagnostic message to the other side.  This message can be sent at
 * any time (but not while constructing another message). The message is
 * printed immediately, but only if the client is being executed in verbose
 * mode.  These messages are primarily intended to ease debugging
 * authentication problems.  The total length of the message must not exceed
 * 1024 bytes.  This will automatically call packet_write_wait.  If the
 * remote side protocol flags do not indicate that it supports SSH_MSG_DEBUG,
 * this will do nothing.
 */
void    packet_send_debug(const char *fmt,...) __attribute__((format(printf, 1, 2)));

/* Checks if there is any buffered output, and tries to write some of the output. */
void    packet_write_poll(void);

/* Waits until all pending output data has been written. */
void    packet_write_wait(void);

/* Returns true if there is buffered data to write to the connection. */
int     packet_have_data_to_write(void);

/* Returns true if there is not too much data to write to the connection. */
int     packet_not_very_much_data_to_write(void);

/* maximum packet size, requested by client with SSH_CMSG_MAX_PACKET_SIZE */
extern int max_packet_size;
int     packet_set_maxsize(int s);
#define packet_get_maxsize() max_packet_size

/* Stores tty modes from the fd into current packet. */
void    tty_make_modes(int fd);

/* Parses tty modes for the fd from the current packet. */
void    tty_parse_modes(int fd, int *n_bytes_ptr);

#define packet_integrity_check(payload_len, expected_len, type) \
do { \
  int _p = (payload_len), _e = (expected_len); \
  if (_p != _e) { \
    log("Packet integrity error (%d != %d) at %s:%d", \
	_p, _e, __FILE__, __LINE__); \
    packet_disconnect("Packet integrity error. (%d)", (type)); \
  } \
} while (0)

#define packet_done() \
do { \
	int _len = packet_remaining(); \
	if (_len > 0) { \
		log("Packet integrity error (%d bytes remaining) at %s:%d", \
		    _len ,__FILE__, __LINE__); \
		packet_disconnect("Packet integrity error."); \
	} \
} while (0)

/* remote host is connected via a socket/ipv4 */
int	packet_connection_is_on_socket(void);
int	packet_connection_is_ipv4(void);

/* enable SSH2 packet format */
void	packet_set_ssh2_format(void);

/* returns remaining payload bytes */
int	packet_remaining(void);

#endif				/* PACKET_H */
