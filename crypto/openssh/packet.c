/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * This file contains code implementing the packet protocol and communication
 * with the other side.  This same code is used both on client and server side.
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 *
 *
 * SSH2 packet format added by Markus Friedl.
 * Copyright (c) 2000 Markus Friedl.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "includes.h"
RCSID("$OpenBSD: packet.c,v 1.38 2000/10/12 14:21:12 markus Exp $");

#include "xmalloc.h"
#include "buffer.h"
#include "packet.h"
#include "bufaux.h"
#include "ssh.h"
#include "crc32.h"
#include "getput.h"

#include "compress.h"
#include "deattack.h"
#include "channels.h"

#include "compat.h"
#include "ssh2.h"

#include <openssl/bn.h>
#include <openssl/dh.h>
#include <openssl/hmac.h>
#include "buffer.h"
#include "cipher.h"
#include "kex.h"
#include "hmac.h"

#ifdef PACKET_DEBUG
#define DBG(x) x
#else
#define DBG(x)
#endif

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

/* True if SSH2 packet format is used */
int use_ssh2_packet_format = 0;

/* Session key information for Encryption and MAC */
Kex	*kex = NULL;

void
packet_set_kex(Kex *k)
{
	if( k->mac[MODE_IN ].key == NULL ||
	    k->enc[MODE_IN ].key == NULL ||
	    k->enc[MODE_IN ].iv  == NULL ||
	    k->mac[MODE_OUT].key == NULL ||
	    k->enc[MODE_OUT].key == NULL ||
	    k->enc[MODE_OUT].iv  == NULL)
		fatal("bad KEX");
	kex = k;
}
void
clear_enc_keys(Enc *enc, int len)
{
	memset(enc->iv,  0, len);
	memset(enc->key, 0, len);
	xfree(enc->iv);
	xfree(enc->key);
	enc->iv = NULL;
	enc->key = NULL;
}
void
packet_set_ssh2_format(void)
{
	DBG(debug("use_ssh2_packet_format"));
	use_ssh2_packet_format = 1;
}

/*
 * Sets the descriptors used for communication.  Disables encryption until
 * packet_set_encryption_key is called.
 */
void
packet_set_connection(int fd_in, int fd_out)
{
	Cipher *none = cipher_by_name("none");
	if (none == NULL)
		fatal("packet_set_connection: cannot load cipher 'none'");
	connection_in = fd_in;
	connection_out = fd_out;
	cipher_type = SSH_CIPHER_NONE;
	cipher_init(&send_context, none, (unsigned char *) "", 0, NULL, 0);
	cipher_init(&receive_context, none, (unsigned char *) "", 0, NULL, 0);
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

/*** XXXXX todo: kex means re-init */
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
packet_decrypt(CipherContext *context, void *dest, void *src, unsigned int bytes)
{
	/*
	 * Cryptographic attack detector for ssh - Modifications for packet.c
	 * (C)1998 CORE-SDI, Buenos Aires Argentina Ariel Futoransky(futo@core-sdi.com)
	 */
	if (!compat20 &&
	    context->cipher->number != SSH_CIPHER_NONE &&
	    detect_attack(src, bytes, NULL) == DEATTACK_DETECTED)
		packet_disconnect("crc32 compensation attack: network attack detected");

	cipher_decrypt(context, dest, src, bytes);
}

/*
 * Causes any further packets to be encrypted using the given key.  The same
 * key is used for both sending and reception.  However, both directions are
 * encrypted independently of each other.
 */

void
packet_set_encryption_key(const unsigned char *key, unsigned int keylen,
    int number)
{
	Cipher *cipher = cipher_by_number(number);
	if (cipher == NULL)
		fatal("packet_set_encryption_key: unknown cipher number %d", number);
	if (keylen < 20)
		fatal("packet_set_encryption_key: keylen too small: %d", keylen);
	cipher_init(&receive_context, cipher, key, keylen, NULL, 0);
	cipher_init(&send_context, cipher, key, keylen, NULL, 0);
}

/* Starts constructing a packet to send. */

void
packet_start1(int type)
{
	char buf[9];

	buffer_clear(&outgoing_packet);
	memset(buf, 0, 8);
	buf[8] = type;
	buffer_append(&outgoing_packet, buf, 9);
}

void
packet_start2(int type)
{
	char buf[4+1+1];

	buffer_clear(&outgoing_packet);
	memset(buf, 0, sizeof buf);
	/* buf[0..3] = payload_len; */
	/* buf[4] =    pad_len; */
	buf[5] = type & 0xff;
	buffer_append(&outgoing_packet, buf, sizeof buf);
}

void
packet_start(int type)
{
	DBG(debug("packet_start[%d]",type));
	if (use_ssh2_packet_format)
		packet_start2(type);
	else
		packet_start1(type);
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
void
packet_put_cstring(const char *str)
{
	buffer_put_string(&outgoing_packet, str, strlen(str));
}

void
packet_put_raw(const char *buf, unsigned int len)
{
	buffer_append(&outgoing_packet, buf, len);
}


/* Appends an arbitrary precision integer to packet data. */

void
packet_put_bignum(BIGNUM * value)
{
	buffer_put_bignum(&outgoing_packet, value);
}
void
packet_put_bignum2(BIGNUM * value)
{
	buffer_put_bignum2(&outgoing_packet, value);
}

/*
 * Finalizes and sends the packet.  If the encryption key has been set,
 * encrypts the packet before sending.
 */

void
packet_send1()
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

	/* Insert padding. Initialized to zero in packet_start1() */
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
	checksum = ssh_crc32((unsigned char *) buffer_ptr(&outgoing_packet),
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
 * Finalize packet in SSH2 format (compress, mac, encrypt, enqueue)
 */
void
packet_send2()
{
	unsigned char *macbuf = NULL;
	char *cp;
	unsigned int packet_length = 0;
	unsigned int i, padlen, len;
	u_int32_t rand = 0;
	static unsigned int seqnr = 0;
	int type;
	Enc *enc   = NULL;
	Mac *mac   = NULL;
	Comp *comp = NULL;
	int block_size;

	if (kex != NULL) {
		enc  = &kex->enc[MODE_OUT];
		mac  = &kex->mac[MODE_OUT];
		comp = &kex->comp[MODE_OUT];
	}
	block_size = enc ? enc->cipher->block_size : 8;

	cp = buffer_ptr(&outgoing_packet);
	type = cp[5] & 0xff;

#ifdef PACKET_DEBUG
	fprintf(stderr, "plain:     ");
	buffer_dump(&outgoing_packet);
#endif

	if (comp && comp->enabled) {
		len = buffer_len(&outgoing_packet);
		/* skip header, compress only payload */
		buffer_consume(&outgoing_packet, 5);
		buffer_clear(&compression_buffer);
		buffer_compress(&outgoing_packet, &compression_buffer);
		buffer_clear(&outgoing_packet);
		buffer_append(&outgoing_packet, "\0\0\0\0\0", 5);
		buffer_append(&outgoing_packet, buffer_ptr(&compression_buffer),
		    buffer_len(&compression_buffer));
		DBG(debug("compression: raw %d compressed %d", len,
		    buffer_len(&outgoing_packet)));
	}

	/* sizeof (packet_len + pad_len + payload) */
	len = buffer_len(&outgoing_packet);

	/*
	 * calc size of padding, alloc space, get random data,
	 * minimum padding is 4 bytes
	 */
	padlen = block_size - (len % block_size);
	if (padlen < 4)
		padlen += block_size;
	buffer_append_space(&outgoing_packet, &cp, padlen);
	if (enc && enc->cipher->number != SSH_CIPHER_NONE) {
		/* random padding */
		for (i = 0; i < padlen; i++) {
			if (i % 4 == 0)
				rand = arc4random();
			cp[i] = rand & 0xff;
			rand <<= 8;
		}
	} else {
		/* clear padding */
		memset(cp, 0, padlen);
	}
	/* packet_length includes payload, padding and padding length field */
	packet_length = buffer_len(&outgoing_packet) - 4;
	cp = buffer_ptr(&outgoing_packet);
	PUT_32BIT(cp, packet_length);
	cp[4] = padlen & 0xff;
	DBG(debug("send: len %d (includes padlen %d)", packet_length+4, padlen));

	/* compute MAC over seqnr and packet(length fields, payload, padding) */
	if (mac && mac->enabled) {
		macbuf = hmac( mac->md, seqnr,
		    (unsigned char *) buffer_ptr(&outgoing_packet),
		    buffer_len(&outgoing_packet),
		    mac->key, mac->key_len
		);
		DBG(debug("done calc MAC out #%d", seqnr));
	}
	/* encrypt packet and append to output buffer. */
	buffer_append_space(&output, &cp, buffer_len(&outgoing_packet));
	packet_encrypt(&send_context, cp, buffer_ptr(&outgoing_packet),
	    buffer_len(&outgoing_packet));
	/* append unencrypted MAC */
	if (mac && mac->enabled)
		buffer_append(&output, (char *)macbuf, mac->mac_len);
#ifdef PACKET_DEBUG
	fprintf(stderr, "encrypted: ");
	buffer_dump(&output);
#endif
	/* increment sequence number for outgoing packets */
	if (++seqnr == 0)
		log("outgoing seqnr wraps around");
	buffer_clear(&outgoing_packet);

	if (type == SSH2_MSG_NEWKEYS) {
		if (kex==NULL || mac==NULL || enc==NULL || comp==NULL)
			fatal("packet_send2: no KEX");
		if (mac->md != NULL)
			mac->enabled = 1;
		DBG(debug("cipher_init send_context"));
		cipher_init(&send_context, enc->cipher,
		    enc->key, enc->cipher->key_len,
		    enc->iv, enc->cipher->block_size);
		clear_enc_keys(enc, kex->we_need);
		if (comp->type != 0 && comp->enabled == 0) {
			comp->enabled = 1;
			if (! packet_compression)
				packet_start_compression(6);
		}
	}
}

void
packet_send()
{
	if (use_ssh2_packet_format)
		packet_send2();
	else
		packet_send1();
	DBG(debug("packet_send done"));
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
	DBG(debug("packet_read()"));

	/* Since we are blocking, ensure that all written packets have been sent. */
	packet_write_wait();

	/* Stay in the loop until we have received a complete packet. */
	for (;;) {
		/* Try to read a packet from the buffer. */
		type = packet_read_poll(payload_len_ptr);
		if (!use_ssh2_packet_format && (
		    type == SSH_SMSG_SUCCESS
		    || type == SSH_SMSG_FAILURE
		    || type == SSH_CMSG_EOF
		    || type == SSH_CMSG_EXIT_CONFIRMATION))
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
packet_read_poll1(int *payload_len_ptr)
{
	unsigned int len, padded_len;
	unsigned char *ucp;
	char buf[8], *cp;
	unsigned int checksum, stored_checksum;

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
	checksum = ssh_crc32((unsigned char *) buffer_ptr(&incoming_packet),
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

	/* Return type. */
	return (unsigned char) buf[0];
}

int
packet_read_poll2(int *payload_len_ptr)
{
	unsigned int padlen, need;
	unsigned char buf[8], *macbuf;
	unsigned char *ucp;
	char *cp;
	static unsigned int packet_length = 0;
	static unsigned int seqnr = 0;
	int type;
	int maclen, block_size;
	Enc *enc   = NULL;
	Mac *mac   = NULL;
	Comp *comp = NULL;

	if (kex != NULL) {
		enc  = &kex->enc[MODE_IN];
		mac  = &kex->mac[MODE_IN];
		comp = &kex->comp[MODE_IN];
	}
	maclen = mac && mac->enabled ? mac->mac_len : 0;
	block_size = enc ? enc->cipher->block_size : 8;

	if (packet_length == 0) {
		/*
		 * check if input size is less than the cipher block size,
		 * decrypt first block and extract length of incoming packet
		 */
		if (buffer_len(&input) < block_size)
			return SSH_MSG_NONE;
		buffer_clear(&incoming_packet);
		buffer_append_space(&incoming_packet, &cp, block_size);
		packet_decrypt(&receive_context, cp, buffer_ptr(&input),
		    block_size);
		ucp = (unsigned char *) buffer_ptr(&incoming_packet);
		packet_length = GET_32BIT(ucp);
		if (packet_length < 1 + 4 || packet_length > 256 * 1024) {
			buffer_dump(&incoming_packet);
			packet_disconnect("Bad packet length %d.", packet_length);
		}
		DBG(debug("input: packet len %d", packet_length+4));
		buffer_consume(&input, block_size);
	}
	/* we have a partial packet of block_size bytes */
	need = 4 + packet_length - block_size;
	DBG(debug("partial packet %d, need %d, maclen %d", block_size,
	    need, maclen));
	if (need % block_size != 0)
		fatal("padding error: need %d block %d mod %d",
		    need, block_size, need % block_size);
	/*
	 * check if the entire packet has been received and
	 * decrypt into incoming_packet
	 */
	if (buffer_len(&input) < need + maclen)
		return SSH_MSG_NONE;
#ifdef PACKET_DEBUG
	fprintf(stderr, "read_poll enc/full: ");
	buffer_dump(&input);
#endif
	buffer_append_space(&incoming_packet, &cp, need);
	packet_decrypt(&receive_context, cp, buffer_ptr(&input), need);
	buffer_consume(&input, need);
	/*
	 * compute MAC over seqnr and packet,
	 * increment sequence number for incoming packet
	 */
	if (mac && mac->enabled) {
		macbuf = hmac( mac->md, seqnr,
		    (unsigned char *) buffer_ptr(&incoming_packet),
		    buffer_len(&incoming_packet),
		    mac->key, mac->key_len
		);
		if (memcmp(macbuf, buffer_ptr(&input), mac->mac_len) != 0)
			packet_disconnect("Corrupted MAC on input.");
		DBG(debug("MAC #%d ok", seqnr));
		buffer_consume(&input, mac->mac_len);
	}
	if (++seqnr == 0)
		log("incoming seqnr wraps around");

	/* get padlen */
	cp = buffer_ptr(&incoming_packet) + 4;
	padlen = *cp & 0xff;
	DBG(debug("input: padlen %d", padlen));
	if (padlen < 4)
		packet_disconnect("Corrupted padlen %d on input.", padlen);

	/* skip packet size + padlen, discard padding */
	buffer_consume(&incoming_packet, 4 + 1);
	buffer_consume_end(&incoming_packet, padlen);

	DBG(debug("input: len before de-compress %d", buffer_len(&incoming_packet)));
	if (comp && comp->enabled) {
		buffer_clear(&compression_buffer);
		buffer_uncompress(&incoming_packet, &compression_buffer);
		buffer_clear(&incoming_packet);
		buffer_append(&incoming_packet, buffer_ptr(&compression_buffer),
		    buffer_len(&compression_buffer));
		DBG(debug("input: len after de-compress %d", buffer_len(&incoming_packet)));
	}
	/*
	 * get packet type, implies consume.
	 * return length of payload (without type field)
	 */
	buffer_get(&incoming_packet, (char *)&buf[0], 1);
	*payload_len_ptr = buffer_len(&incoming_packet);

	/* reset for next packet */
	packet_length = 0;

	/* extract packet type */
	type = (unsigned char)buf[0];

	if (type == SSH2_MSG_NEWKEYS) {
		if (kex==NULL || mac==NULL || enc==NULL || comp==NULL)
			fatal("packet_read_poll2: no KEX");
		if (mac->md != NULL)
			mac->enabled = 1;
		DBG(debug("cipher_init receive_context"));
		cipher_init(&receive_context, enc->cipher,
		    enc->key, enc->cipher->key_len,
		    enc->iv, enc->cipher->block_size);
		clear_enc_keys(enc, kex->we_need);
		if (comp->type != 0 && comp->enabled == 0) {
			comp->enabled = 1;
			if (! packet_compression)
				packet_start_compression(6);
		}
	}

#ifdef PACKET_DEBUG
	fprintf(stderr, "read/plain[%d]:\r\n",type);
	buffer_dump(&incoming_packet);
#endif
	return (unsigned char)type;
}

int
packet_read_poll(int *payload_len_ptr)
{
	char *msg;
	for (;;) {
		int type = use_ssh2_packet_format ?
		    packet_read_poll2(payload_len_ptr):
		    packet_read_poll1(payload_len_ptr);

		if(compat20) {
			int reason;
			if (type != 0)
				DBG(debug("received packet type %d", type));
			switch(type) {
			case SSH2_MSG_IGNORE:
				break;
			case SSH2_MSG_DEBUG:
				packet_get_char();
				msg = packet_get_string(NULL);
				debug("Remote: %.900s", msg);
				xfree(msg);
				msg = packet_get_string(NULL);
				xfree(msg);
				break;
			case SSH2_MSG_DISCONNECT:
				reason = packet_get_int();
				msg = packet_get_string(NULL);
				log("Received disconnect: %d: %.900s", reason, msg);
				xfree(msg);
				fatal_cleanup();
				break;
			default:
				return type;
				break;
			}	
		} else {
			switch(type) {
			case SSH_MSG_IGNORE:
				break;
			case SSH_MSG_DEBUG:
				msg = packet_get_string(NULL);
				debug("Remote: %.900s", msg);
				xfree(msg);
				break;
			case SSH_MSG_DISCONNECT:
				msg = packet_get_string(NULL);
				log("Received disconnect: %.900s", msg);
				fatal_cleanup();
				xfree(msg);
				break;
			default:
				if (type != 0)
					DBG(debug("received packet type %d", type));
				return type;
				break;
			}	
		}
	}
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

void
packet_get_bignum2(BIGNUM * value, int *length_ptr)
{
	*length_ptr = buffer_get_bignum2(&incoming_packet, value);
}

char *
packet_get_raw(int *length_ptr)
{
	int bytes = buffer_len(&incoming_packet);
	if (length_ptr != NULL)
		*length_ptr = bytes;
	return buffer_ptr(&incoming_packet);
}

int
packet_remaining(void)
{
	return buffer_len(&incoming_packet);
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

	if (compat20) {
		packet_start(SSH2_MSG_DEBUG);
		packet_put_char(0);	/* bool: always display */
		packet_put_cstring(buf);
		packet_put_cstring("");
	} else {
		packet_start(SSH_MSG_DEBUG);
		packet_put_cstring(buf);
	}
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
	if (compat20) {
		packet_start(SSH2_MSG_DISCONNECT);
		packet_put_int(SSH2_DISCONNECT_PROTOCOL_ERROR);
		packet_put_cstring(buf);
		packet_put_cstring("");
	} else {
		packet_start(SSH_MSG_DISCONNECT);
		packet_put_string(buf, strlen(buf));
	}
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
		log("packet_set_maxsize: called twice: old %d new %d",
		    max_packet_size, s);
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
