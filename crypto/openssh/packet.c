/*
 * 
 * packet.c
 * 
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * 
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * 
 * Created: Sat Mar 18 02:40:40 1995 ylo
 * 
 * This file contains code implementing the packet protocol and communication
 * with the other side.  This same code is used both on client and server side.
 * 
 */

#include "includes.h"
RCSID("$Id: packet.c,v 1.22 2000/02/05 10:13:11 markus Exp $");

#include "xmalloc.h"
#include "buffer.h"
#include "packet.h"
#include "bufaux.h"
#include "ssh.h"
#include "crc32.h"
#include "cipher.h"
#include "getput.h"

#include "compress.h"
#include "deattack.h"

/*
 * This variable contains the file descriptors used for communicating with
 * the other side.  connection_in is used for reading; connection_out for
 * writing.  These can be the same descriptor, in which case it is assumed to
 * be a socket.
 */
static int connection_in = -1;
static int connection_out = -1;

/*
 * Cipher type.  This value is only used to determine whether to pad the
 * packets with zeroes or random data.
 */
static int cipher_type = SSH_CIPHER_NONE;

/* Protocol flags for the remote side. */
static unsigned int remote_protocol_flags = 0;

/* Encryption context for receiving data.  This is only used for decryption. */
static CipherContext receive_context;

/* Encryption context for sending data.  This is only used for encryption. */
static CipherContext send_context;

/* Buffer for raw input data from the socket. */
static Buffer input;

/* Buffer for raw output data going to the socket. */
static Buffer output;

/* Buffer for the partial outgoing packet being constructed. */
static Buffer outgoing_packet;

/* Buffer for the incoming packet currently being processed. */
static Buffer incoming_packet;

/* Scratch buffer for packet compression/decompression. */
static Buffer compression_buffer;

/* Flag indicating whether packet compression/decompression is enabled. */
static int packet_compression = 0;

/* default maximum packet size */
int max_packet_size = 32768;

/* Flag indicating whether this module has been initialized. */
static int initialized = 0;

/* Set to true if the connection is interactive. */
static int interactive_mode = 0;

/*
 * Sets the descriptors used for communication.  Disables encryption until
 * packet_set_encryption_key is called.
 */

void
packet_set_connection(int fd_in, int fd_out)
{
	connection_in = fd_in;
	connection_out = fd_out;
	cipher_type = SSH_CIPHER_NONE;
	cipher_set_key(&send_context, SSH_CIPHER_NONE, (unsigned char *) "", 0, 1);
	cipher_set_key(&receive_context, SSH_CIPHER_NONE, (unsigned char *) "", 0, 0);
	if (!initialized) {
		initialized = 1;
		buffer_init(&input);
		buffer_init(&output);
		buffer_init(&outgoing_packet);
		buffer_init(&incoming_packet);
	}
	/* Kludge: arrange the close function to be called from fatal(). */
	fatal_add_cleanup((void (*) (void *)) packet_close, NULL);
}

/* Returns 1 if remote host is connected via socket, 0 if not. */

int
packet_connection_is_on_socket()
{
	struct sockaddr_storage from, to;
	socklen_t fromlen, tolen;

	/* filedescriptors in and out are the same, so it's a socket */
	if (connection_in == connection_out)
		return 1;
	fromlen = sizeof(from);
	memset(&from, 0, sizeof(from));
	if (getpeername(connection_in, (struct sockaddr *)&from, &fromlen) < 0)
		return 0;
	tolen = sizeof(to);
	memset(&to, 0, sizeof(to));
	if (getpeername(connection_out, (struct sockaddr *)&to, &tolen) < 0)
		return 0;
	if (fromlen != tolen || memcmp(&from, &to, fromlen) != 0)
		return 0;
	if (from.ss_family != AF_INET && from.ss_family != AF_INET6)
		return 0;
	return 1;
}

/* returns 1 if connection is via ipv4 */

int
packet_connection_is_ipv4()
{
	struct sockaddr_storage to;
	socklen_t tolen = sizeof(to);

	memset(&to, 0, sizeof(to));
	if (getsockname(connection_out, (struct sockaddr *)&to, &tolen) < 0)
		return 0;
	if (to.ss_family != AF_INET)
		return 0;
	return 1;
}

/* Sets the connection into non-blocking mode. */

void
packet_set_nonblocking()
{
	/* Set the socket into non-blocking mode. */
	if (fcntl(connection_in, F_SETFL, O_NONBLOCK) < 0)
		error("fcntl O_NONBLOCK: %.100s", strerror(errno));

	if (connection_out != connection_in) {
		if (fcntl(connection_out, F_SETFL, O_NONBLOCK) < 0)
			error("fcntl O_NONBLOCK: %.100s", strerror(errno));
	}
}

/* Returns the socket used for reading. */

int
packet_get_connection_in()
{
	return connection_in;
}

/* Returns the descriptor used for writing. */

int
packet_get_connection_out()
{
	return connection_out;
}

/* Closes the connection and clears and frees internal data structures. */

void
packet_close()
{
	if (!initialized)
		return;
	initialized = 0;
	if (connection_in == connection_out) {
		shutdown(connection_out, SHUT_RDWR);
		close(connection_out);
	} else {
		close(connection_in);
		close(connection_out);
	}
	buffer_free(&input);
	buffer_free(&output);
	buffer_free(&outgoing_packet);
	buffer_free(&incoming_packet);
	if (packet_compression) {
		buffer_free(&compression_buffer);
		buffer_compress_uninit();
	}
}

/* Sets remote side protocol flags. */

void
packet_set_protocol_flags(unsigned int protocol_flags)
{
	remote_protocol_flags = protocol_flags;
	channel_set_options((protocol_flags & SSH_PROTOFLAG_HOST_IN_FWD_OPEN) != 0);
}

/* Returns the remote protocol flags set earlier by the above function. */

unsigned int
packet_get_protocol_flags()
{
	return remote_protocol_flags;
}

/*
 * Starts packet compression from the next packet on in both directions.
 * Level is compression level 1 (fastest) - 9 (slow, best) as in gzip.
 */

void
packet_start_compression(int level)
{
	if (packet_compression)
		fatal("Compression already enabled.");
	packet_compression = 1;
	buffer_init(&compression_buffer);
	buffer_compress_init(level);
}

/*
 * Encrypts the given number of bytes, copying from src to dest. bytes is
 * known to be a multiple of 8.
 */

void
packet_encrypt(CipherContext * cc, void *dest, void *src,
	       unsigned int bytes)
{
	cipher_encrypt(cc, dest, src, bytes);
}

/*
 * Decrypts the given number of bytes, copying from src to dest. bytes is
 * known to be a multiple of 8.
 */

void
packet_decrypt(CipherContext * cc, void *dest, void *src,
	       unsigned int bytes)
{
	int i;

	if ((bytes % 8) != 0)
		fatal("packet_decrypt: bad ciphertext length %d", bytes);

	/*
	 * Cryptographic attack detector for ssh - Modifications for packet.c
	 * (C)1998 CORE-SDI, Buenos Aires Argentina Ariel Futoransky(futo@core-sdi.com)
	 */

	switch (cc->type) {
	case SSH_CIPHER_NONE:
		i = DEATTACK_OK;
		break;
	default:
		i = detect_attack(src, bytes, NULL);
		break;
	}

	if (i == DEATTACK_DETECTED)
		packet_disconnect("crc32 compensation attack: network attack detected");

	cipher_decrypt(cc, dest, src, bytes);
}

/*
 * Causes any further packets to be encrypted using the given key.  The same
 * key is used for both sending and reception.  However, both directions are
 * encrypted independently of each other.
 */

void
packet_set_encryption_key(const unsigned char *key, unsigned int keylen,
			  int cipher)
{
	/* All other ciphers use the same key in both directions for now. */
	cipher_set_key(&receive_context, cipher, key, keylen, 0);
	cipher_set_key(&send_context, cipher, key, keylen, 1);
}

/* Starts constructing a packet to send. */

void
packet_start(int type)
{
	char buf[9];

	buffer_clear(&outgoing_packet);
	memset(buf, 0, 8);
	buf[8] = type;
	buffer_append(&outgoing_packet, buf, 9);
}

/* Appends a character to the packet data. */

void
packet_put_char(int value)
{
	char ch = value;
	buffer_append(&outgoing_packet, &ch, 1);
}

/* Appends an integer to the packet data. */

void
packet_put_int(unsigned int value)
{
	buffer_put_int(&outgoing_packet, value);
}

/* Appends a string to packet data. */

void
packet_put_string(const char *buf, unsigned int len)
{
	buffer_put_string(&outgoing_packet, buf, len);
}

/* Appends an arbitrary precision integer to packet data. */

void
packet_put_bignum(BIGNUM * value)
{
	buffer_put_bignum(&outgoing_packet, value);
}

/*
 * Finalizes and sends the packet.  If the encryption key has been set,
 * encrypts the packet before sending.
 */

void
packet_send()
{
	char buf[8], *cp;
	int i, padding, len;
	unsigned int checksum;
	u_int32_t rand = 0;

	/*
	 * If using packet compression, compress the payload of the outgoing
	 * packet.
	 */
	if (packet_compression) {
		buffer_clear(&compression_buffer);
		/* Skip padding. */
		buffer_consume(&outgoing_packet, 8);
		/* padding */
		buffer_append(&compression_buffer, "\0\0\0\0\0\0\0\0", 8);
		buffer_compress(&outgoing_packet, &compression_buffer);
		buffer_clear(&outgoing_packet);
		buffer_append(&outgoing_packet, buffer_ptr(&compression_buffer),
			      buffer_len(&compression_buffer));
	}
	/* Compute packet length without padding (add checksum, remove padding). */
	len = buffer_len(&outgoing_packet) + 4 - 8;

	/* Insert padding. */
	padding = 8 - len % 8;
	if (cipher_type != SSH_CIPHER_NONE) {
		cp = buffer_ptr(&outgoing_packet);
		for (i = 0; i < padding; i++) {
			if (i % 4 == 0)
				rand = arc4random();
			cp[7 - i] = rand & 0xff;
			rand >>= 8;
		}
	}
	buffer_consume(&outgoing_packet, 8 - padding);

	/* Add check bytes. */
	checksum = crc32((unsigned char *) buffer_ptr(&outgoing_packet),
			 buffer_len(&outgoing_packet));
	PUT_32BIT(buf, checksum);
	buffer_append(&outgoing_packet, buf, 4);

#ifdef PACKET_DEBUG
	fprintf(stderr, "packet_send plain: ");
	buffer_dump(&outgoing_packet);
#endif

	/* Append to output. */
	PUT_32BIT(buf, len);
	buffer_append(&output, buf, 4);
	buffer_append_space(&output, &cp, buffer_len(&outgoing_packet));
	packet_encrypt(&send_context, cp, buffer_ptr(&outgoing_packet),
		       buffer_len(&outgoing_packet));

#ifdef PACKET_DEBUG
	fprintf(stderr, "encrypted: ");
	buffer_dump(&output);
#endif

	buffer_clear(&outgoing_packet);

	/*
	 * Note that the packet is now only buffered in output.  It won\'t be
	 * actually sent until packet_write_wait or packet_write_poll is
	 * called.
	 */
}

/*
 * Waits until a packet has been received, and returns its type.  Note that
 * no other data is processed until this returns, so this function should not
 * be used during the interactive session.
 */

int
packet_read(int *payload_len_ptr)
{
	int type, len;
	fd_set set;
	char buf[8192];

	/* Since we are blocking, ensure that all written packets have been sent. */
	packet_write_wait();

	/* Stay in the loop until we have received a complete packet. */
	for (;;) {
		/* Try to read a packet from the buffer. */
		type = packet_read_poll(payload_len_ptr);
		if (type == SSH_SMSG_SUCCESS
		    || type == SSH_SMSG_FAILURE
		    || type == SSH_CMSG_EOF
		    || type == SSH_CMSG_EXIT_CONFIRMATION)
			packet_integrity_check(*payload_len_ptr, 0, type);
		/* If we got a packet, return it. */
		if (type != SSH_MSG_NONE)
			return type;
		/*
		 * Otherwise, wait for some data to arrive, add it to the
		 * buffer, and try again.
		 */
		FD_ZERO(&set);
		FD_SET(connection_in, &set);

		/* Wait for some data to arrive. */
		select(connection_in + 1, &set, NULL, NULL, NULL);

		/* Read data from the socket. */
		len = read(connection_in, buf, sizeof(buf));
		if (len == 0) {
			log("Connection closed by %.200s", get_remote_ipaddr());
			fatal_cleanup();
		}
		if (len < 0)
			fatal("Read from socket failed: %.100s", strerror(errno));
		/* Append it to the buffer. */
		packet_process_incoming(buf, len);
	}
	/* NOTREACHED */
}

/*
 * Waits until a packet has been received, verifies that its type matches
 * that given, and gives a fatal error and exits if there is a mismatch.
 */

void
packet_read_expect(int *payload_len_ptr, int expected_type)
{
	int type;

	type = packet_read(payload_len_ptr);
	if (type != expected_type)
		packet_disconnect("Protocol error: expected packet type %d, got %d",
				  expected_type, type);
}

/* Checks if a full packet is available in the data received so far via
 * packet_process_incoming.  If so, reads the packet; otherwise returns
 * SSH_MSG_NONE.  This does not wait for data from the connection.
 *
 * SSH_MSG_DISCONNECT is handled specially here.  Also,
 * SSH_MSG_IGNORE messages are skipped by this function and are never returned
 * to higher levels.
 *
 * The returned payload_len does include space consumed by:
 * 	Packet length
 * 	Padding
 * 	Packet type
 * 	Check bytes
 */

int
packet_read_poll(int *payload_len_ptr)
{
	unsigned int len, padded_len;
	unsigned char *ucp;
	char buf[8], *cp, *msg;
	unsigned int checksum, stored_checksum;

restart:

	/* Check if input size is less than minimum packet size. */
	if (buffer_len(&input) < 4 + 8)
		return SSH_MSG_NONE;
	/* Get length of incoming packet. */
	ucp = (unsigned char *) buffer_ptr(&input);
	len = GET_32BIT(ucp);
	if (len < 1 + 2 + 2 || len > 256 * 1024)
		packet_disconnect("Bad packet length %d.", len);
	padded_len = (len + 8) & ~7;

	/* Check if the packet has been entirely received. */
	if (buffer_len(&input) < 4 + padded_len)
		return SSH_MSG_NONE;

	/* The entire packet is in buffer. */

	/* Consume packet length. */
	buffer_consume(&input, 4);

	/* Copy data to incoming_packet. */
	buffer_clear(&incoming_packet);
	buffer_append_space(&incoming_packet, &cp, padded_len);
	packet_decrypt(&receive_context, cp, buffer_ptr(&input), padded_len);
	buffer_consume(&input, padded_len);

#ifdef PACKET_DEBUG
	fprintf(stderr, "read_poll plain: ");
	buffer_dump(&incoming_packet);
#endif

	/* Compute packet checksum. */
	checksum = crc32((unsigned char *) buffer_ptr(&incoming_packet),
			 buffer_len(&incoming_packet) - 4);

	/* Skip padding. */
	buffer_consume(&incoming_packet, 8 - len % 8);

	/* Test check bytes. */

	if (len != buffer_len(&incoming_packet))
		packet_disconnect("packet_read_poll: len %d != buffer_len %d.",
				  len, buffer_len(&incoming_packet));

	ucp = (unsigned char *) buffer_ptr(&incoming_packet) + len - 4;
	stored_checksum = GET_32BIT(ucp);
	if (checksum != stored_checksum)
		packet_disconnect("Corrupted check bytes on input.");
	buffer_consume_end(&incoming_packet, 4);

	/* If using packet compression, decompress the packet. */
	if (packet_compression) {
		buffer_clear(&compression_buffer);
		buffer_uncompress(&incoming_packet, &compression_buffer);
		buffer_clear(&incoming_packet);
		buffer_append(&incoming_packet, buffer_ptr(&compression_buffer),
			      buffer_len(&compression_buffer));
	}
	/* Get packet type. */
	buffer_get(&incoming_packet, &buf[0], 1);

	/* Return length of payload (without type field). */
	*payload_len_ptr = buffer_len(&incoming_packet);

	/* Handle disconnect message. */
	if ((unsigned char) buf[0] == SSH_MSG_DISCONNECT) {
		msg = packet_get_string(NULL);
		log("Received disconnect: %.900s", msg);
		xfree(msg);
		fatal_cleanup();
	}	

	/* Ignore ignore messages. */
	if ((unsigned char) buf[0] == SSH_MSG_IGNORE)
		goto restart;

	/* Send debug messages as debugging output. */
	if ((unsigned char) buf[0] == SSH_MSG_DEBUG) {
		msg = packet_get_string(NULL);
		debug("Remote: %.900s", msg);
		xfree(msg);
		goto restart;
	}
	/* Return type. */
	return (unsigned char) buf[0];
}

/*
 * Buffers the given amount of input characters.  This is intended to be used
 * together with packet_read_poll.
 */

void
packet_process_incoming(const char *buf, unsigned int len)
{
	buffer_append(&input, buf, len);
}

/* Returns a character from the packet. */

unsigned int
packet_get_char()
{
	char ch;
	buffer_get(&incoming_packet, &ch, 1);
	return (unsigned char) ch;
}

/* Returns an integer from the packet data. */

unsigned int
packet_get_int()
{
	return buffer_get_int(&incoming_packet);
}

/*
 * Returns an arbitrary precision integer from the packet data.  The integer
 * must have been initialized before this call.
 */

void
packet_get_bignum(BIGNUM * value, int *length_ptr)
{
	*length_ptr = buffer_get_bignum(&incoming_packet, value);
}

/*
 * Returns a string from the packet data.  The string is allocated using
 * xmalloc; it is the responsibility of the calling program to free it when
 * no longer needed.  The length_ptr argument may be NULL, or point to an
 * integer into which the length of the string is stored.
 */

char *
packet_get_string(unsigned int *length_ptr)
{
	return buffer_get_string(&incoming_packet, length_ptr);
}

/*
 * Sends a diagnostic message from the server to the client.  This message
 * can be sent at any time (but not while constructing another message). The
 * message is printed immediately, but only if the client is being executed
 * in verbose mode.  These messages are primarily intended to ease debugging
 * authentication problems.   The length of the formatted message must not
 * exceed 1024 bytes.  This will automatically call packet_write_wait.
 */

void
packet_send_debug(const char *fmt,...)
{
	char buf[1024];
	va_list args;

	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	packet_start(SSH_MSG_DEBUG);
	packet_put_string(buf, strlen(buf));
	packet_send();
	packet_write_wait();
}

/*
 * Logs the error plus constructs and sends a disconnect packet, closes the
 * connection, and exits.  This function never returns. The error message
 * should not contain a newline.  The length of the formatted message must
 * not exceed 1024 bytes.
 */

void
packet_disconnect(const char *fmt,...)
{
	char buf[1024];
	va_list args;
	static int disconnecting = 0;
	if (disconnecting)	/* Guard against recursive invocations. */
		fatal("packet_disconnect called recursively.");
	disconnecting = 1;

	/*
	 * Format the message.  Note that the caller must make sure the
	 * message is of limited size.
	 */
	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	/* Send the disconnect message to the other side, and wait for it to get sent. */
	packet_start(SSH_MSG_DISCONNECT);
	packet_put_string(buf, strlen(buf));
	packet_send();
	packet_write_wait();

	/* Stop listening for connections. */
	channel_stop_listening();

	/* Close the connection. */
	packet_close();

	/* Display the error locally and exit. */
	log("Disconnecting: %.100s", buf);
	fatal_cleanup();
}

/* Checks if there is any buffered output, and tries to write some of the output. */

void
packet_write_poll()
{
	int len = buffer_len(&output);
	if (len > 0) {
		len = write(connection_out, buffer_ptr(&output), len);
		if (len <= 0) {
			if (errno == EAGAIN)
				return;
			else
				fatal("Write failed: %.100s", strerror(errno));
		}
		buffer_consume(&output, len);
	}
}

/*
 * Calls packet_write_poll repeatedly until all pending output data has been
 * written.
 */

void
packet_write_wait()
{
	packet_write_poll();
	while (packet_have_data_to_write()) {
		fd_set set;
		FD_ZERO(&set);
		FD_SET(connection_out, &set);
		select(connection_out + 1, NULL, &set, NULL, NULL);
		packet_write_poll();
	}
}

/* Returns true if there is buffered data to write to the connection. */

int
packet_have_data_to_write()
{
	return buffer_len(&output) != 0;
}

/* Returns true if there is not too much data to write to the connection. */

int
packet_not_very_much_data_to_write()
{
	if (interactive_mode)
		return buffer_len(&output) < 16384;
	else
		return buffer_len(&output) < 128 * 1024;
}

/* Informs that the current session is interactive.  Sets IP flags for that. */

void
packet_set_interactive(int interactive, int keepalives)
{
	int on = 1;

	/* Record that we are in interactive mode. */
	interactive_mode = interactive;

	/* Only set socket options if using a socket.  */
	if (!packet_connection_is_on_socket())
		return;
	if (keepalives) {
		/* Set keepalives if requested. */
		if (setsockopt(connection_in, SOL_SOCKET, SO_KEEPALIVE, (void *) &on,
		    sizeof(on)) < 0)
			error("setsockopt SO_KEEPALIVE: %.100s", strerror(errno));
	}
	/*
	 * IPTOS_LOWDELAY, TCP_NODELAY and IPTOS_THROUGHPUT are IPv4 only
	 */
	if (!packet_connection_is_ipv4())
		return;
	if (interactive) {
		/*
		 * Set IP options for an interactive connection.  Use
		 * IPTOS_LOWDELAY and TCP_NODELAY.
		 */
		int lowdelay = IPTOS_LOWDELAY;
		if (setsockopt(connection_in, IPPROTO_IP, IP_TOS, (void *) &lowdelay,
		    sizeof(lowdelay)) < 0)
			error("setsockopt IPTOS_LOWDELAY: %.100s", strerror(errno));
		if (setsockopt(connection_in, IPPROTO_TCP, TCP_NODELAY, (void *) &on,
		    sizeof(on)) < 0)
			error("setsockopt TCP_NODELAY: %.100s", strerror(errno));
	} else {
		/*
		 * Set IP options for a non-interactive connection.  Use
		 * IPTOS_THROUGHPUT.
		 */
		int throughput = IPTOS_THROUGHPUT;
		if (setsockopt(connection_in, IPPROTO_IP, IP_TOS, (void *) &throughput,
		    sizeof(throughput)) < 0)
			error("setsockopt IPTOS_THROUGHPUT: %.100s", strerror(errno));
	}
}

/* Returns true if the current connection is interactive. */

int
packet_is_interactive()
{
	return interactive_mode;
}

int
packet_set_maxsize(int s)
{
	static int called = 0;
	if (called) {
		log("packet_set_maxsize: called twice: old %d new %d", max_packet_size, s);
		return -1;
	}
	if (s < 4 * 1024 || s > 1024 * 1024) {
		log("packet_set_maxsize: bad size %d", s);
		return -1;
	}
	log("packet_set_maxsize: setting to %d", s);
	max_packet_size = s;
	return s;
}
