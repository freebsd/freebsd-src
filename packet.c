/* $OpenBSD: packet.c,v 1.182 2013/04/11 02:27:50 djm Exp $ */
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
 * Copyright (c) 2000, 2001 Markus Friedl.  All rights reserved.
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
 
#include <sys/types.h>
#include "openbsd-compat/sys-queue.h"
#include <sys/param.h>
#include <sys/socket.h>
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif

#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include "xmalloc.h"
#include "buffer.h"
#include "packet.h"
#include "crc32.h"
#include "compress.h"
#include "deattack.h"
#include "channels.h"
#include "compat.h"
#include "ssh1.h"
#include "ssh2.h"
#include "cipher.h"
#include "key.h"
#include "kex.h"
#include "mac.h"
#include "log.h"
#include "canohost.h"
#include "misc.h"
#include "ssh.h"
#include "roaming.h"

#ifdef PACKET_DEBUG
#define DBG(x) x
#else
#define DBG(x)
#endif

#define PACKET_MAX_SIZE (256 * 1024)

struct packet_state {
	u_int32_t seqnr;
	u_int32_t packets;
	u_int64_t blocks;
	u_int64_t bytes;
};

struct packet {
	TAILQ_ENTRY(packet) next;
	u_char type;
	Buffer payload;
};

struct session_state {
	/*
	 * This variable contains the file descriptors used for
	 * communicating with the other side.  connection_in is used for
	 * reading; connection_out for writing.  These can be the same
	 * descriptor, in which case it is assumed to be a socket.
	 */
	int connection_in;
	int connection_out;

	/* Protocol flags for the remote side. */
	u_int remote_protocol_flags;

	/* Encryption context for receiving data.  Only used for decryption. */
	CipherContext receive_context;

	/* Encryption context for sending data.  Only used for encryption. */
	CipherContext send_context;

	/* Buffer for raw input data from the socket. */
	Buffer input;

	/* Buffer for raw output data going to the socket. */
	Buffer output;

	/* Buffer for the partial outgoing packet being constructed. */
	Buffer outgoing_packet;

	/* Buffer for the incoming packet currently being processed. */
	Buffer incoming_packet;

	/* Scratch buffer for packet compression/decompression. */
	Buffer compression_buffer;
	int compression_buffer_ready;

	/*
	 * Flag indicating whether packet compression/decompression is
	 * enabled.
	 */
	int packet_compression;

	/* default maximum packet size */
	u_int max_packet_size;

	/* Flag indicating whether this module has been initialized. */
	int initialized;

	/* Set to true if the connection is interactive. */
	int interactive_mode;

	/* Set to true if we are the server side. */
	int server_side;

	/* Set to true if we are authenticated. */
	int after_authentication;

	int keep_alive_timeouts;

	/* The maximum time that we will wait to send or receive a packet */
	int packet_timeout_ms;

	/* Session key information for Encryption and MAC */
	Newkeys *newkeys[MODE_MAX];
	struct packet_state p_read, p_send;

	u_int64_t max_blocks_in, max_blocks_out;
	u_int32_t rekey_limit;

	/* Session key for protocol v1 */
	u_char ssh1_key[SSH_SESSION_KEY_LENGTH];
	u_int ssh1_keylen;

	/* roundup current message to extra_pad bytes */
	u_char extra_pad;

	/* XXX discard incoming data after MAC error */
	u_int packet_discard;
	Mac *packet_discard_mac;

	/* Used in packet_read_poll2() */
	u_int packlen;

	/* Used in packet_send2 */
	int rekeying;

	/* Used in packet_set_interactive */
	int set_interactive_called;

	/* Used in packet_set_maxsize */
	int set_maxsize_called;

	TAILQ_HEAD(, packet) outgoing;
};

static struct session_state *active_state, *backup_state;

static struct session_state *
alloc_session_state(void)
{
	struct session_state *s = xcalloc(1, sizeof(*s));

	s->connection_in = -1;
	s->connection_out = -1;
	s->max_packet_size = 32768;
	s->packet_timeout_ms = -1;
	return s;
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
	if (active_state == NULL)
		active_state = alloc_session_state();
	active_state->connection_in = fd_in;
	active_state->connection_out = fd_out;
	cipher_init(&active_state->send_context, none, (const u_char *)"",
	    0, NULL, 0, CIPHER_ENCRYPT);
	cipher_init(&active_state->receive_context, none, (const u_char *)"",
	    0, NULL, 0, CIPHER_DECRYPT);
	active_state->newkeys[MODE_IN] = active_state->newkeys[MODE_OUT] = NULL;
	if (!active_state->initialized) {
		active_state->initialized = 1;
		buffer_init(&active_state->input);
		buffer_init(&active_state->output);
		buffer_init(&active_state->outgoing_packet);
		buffer_init(&active_state->incoming_packet);
		TAILQ_INIT(&active_state->outgoing);
		active_state->p_send.packets = active_state->p_read.packets = 0;
	}
}

void
packet_set_timeout(int timeout, int count)
{
	if (timeout <= 0 || count <= 0) {
		active_state->packet_timeout_ms = -1;
		return;
	}
	if ((INT_MAX / 1000) / count < timeout)
		active_state->packet_timeout_ms = INT_MAX;
	else
		active_state->packet_timeout_ms = timeout * count * 1000;
}

static void
packet_stop_discard(void)
{
	if (active_state->packet_discard_mac) {
		char buf[1024];
		
		memset(buf, 'a', sizeof(buf));
		while (buffer_len(&active_state->incoming_packet) <
		    PACKET_MAX_SIZE)
			buffer_append(&active_state->incoming_packet, buf,
			    sizeof(buf));
		(void) mac_compute(active_state->packet_discard_mac,
		    active_state->p_read.seqnr,
		    buffer_ptr(&active_state->incoming_packet),
		    PACKET_MAX_SIZE);
	}
	logit("Finished discarding for %.200s", get_remote_ipaddr());
	cleanup_exit(255);
}

static void
packet_start_discard(Enc *enc, Mac *mac, u_int packet_length, u_int discard)
{
	if (enc == NULL || !cipher_is_cbc(enc->cipher) || (mac && mac->etm))
		packet_disconnect("Packet corrupt");
	if (packet_length != PACKET_MAX_SIZE && mac && mac->enabled)
		active_state->packet_discard_mac = mac;
	if (buffer_len(&active_state->input) >= discard)
		packet_stop_discard();
	active_state->packet_discard = discard -
	    buffer_len(&active_state->input);
}

/* Returns 1 if remote host is connected via socket, 0 if not. */

int
packet_connection_is_on_socket(void)
{
	struct sockaddr_storage from, to;
	socklen_t fromlen, tolen;

	/* filedescriptors in and out are the same, so it's a socket */
	if (active_state->connection_in == active_state->connection_out)
		return 1;
	fromlen = sizeof(from);
	memset(&from, 0, sizeof(from));
	if (getpeername(active_state->connection_in, (struct sockaddr *)&from,
	    &fromlen) < 0)
		return 0;
	tolen = sizeof(to);
	memset(&to, 0, sizeof(to));
	if (getpeername(active_state->connection_out, (struct sockaddr *)&to,
	    &tolen) < 0)
		return 0;
	if (fromlen != tolen || memcmp(&from, &to, fromlen) != 0)
		return 0;
	if (from.ss_family != AF_INET && from.ss_family != AF_INET6)
		return 0;
	return 1;
}

/*
 * Exports an IV from the CipherContext required to export the key
 * state back from the unprivileged child to the privileged parent
 * process.
 */

void
packet_get_keyiv(int mode, u_char *iv, u_int len)
{
	CipherContext *cc;

	if (mode == MODE_OUT)
		cc = &active_state->send_context;
	else
		cc = &active_state->receive_context;

	cipher_get_keyiv(cc, iv, len);
}

int
packet_get_keycontext(int mode, u_char *dat)
{
	CipherContext *cc;

	if (mode == MODE_OUT)
		cc = &active_state->send_context;
	else
		cc = &active_state->receive_context;

	return (cipher_get_keycontext(cc, dat));
}

void
packet_set_keycontext(int mode, u_char *dat)
{
	CipherContext *cc;

	if (mode == MODE_OUT)
		cc = &active_state->send_context;
	else
		cc = &active_state->receive_context;

	cipher_set_keycontext(cc, dat);
}

int
packet_get_keyiv_len(int mode)
{
	CipherContext *cc;

	if (mode == MODE_OUT)
		cc = &active_state->send_context;
	else
		cc = &active_state->receive_context;

	return (cipher_get_keyiv_len(cc));
}

void
packet_set_iv(int mode, u_char *dat)
{
	CipherContext *cc;

	if (mode == MODE_OUT)
		cc = &active_state->send_context;
	else
		cc = &active_state->receive_context;

	cipher_set_keyiv(cc, dat);
}

int
packet_get_ssh1_cipher(void)
{
	return (cipher_get_number(active_state->receive_context.cipher));
}

void
packet_get_state(int mode, u_int32_t *seqnr, u_int64_t *blocks,
    u_int32_t *packets, u_int64_t *bytes)
{
	struct packet_state *state;

	state = (mode == MODE_IN) ?
	    &active_state->p_read : &active_state->p_send;
	if (seqnr)
		*seqnr = state->seqnr;
	if (blocks)
		*blocks = state->blocks;
	if (packets)
		*packets = state->packets;
	if (bytes)
		*bytes = state->bytes;
}

void
packet_set_state(int mode, u_int32_t seqnr, u_int64_t blocks, u_int32_t packets,
    u_int64_t bytes)
{
	struct packet_state *state;

	state = (mode == MODE_IN) ?
	    &active_state->p_read : &active_state->p_send;
	state->seqnr = seqnr;
	state->blocks = blocks;
	state->packets = packets;
	state->bytes = bytes;
}

static int
packet_connection_af(void)
{
	struct sockaddr_storage to;
	socklen_t tolen = sizeof(to);

	memset(&to, 0, sizeof(to));
	if (getsockname(active_state->connection_out, (struct sockaddr *)&to,
	    &tolen) < 0)
		return 0;
#ifdef IPV4_IN_IPV6
	if (to.ss_family == AF_INET6 &&
	    IN6_IS_ADDR_V4MAPPED(&((struct sockaddr_in6 *)&to)->sin6_addr))
		return AF_INET;
#endif
	return to.ss_family;
}

/* Sets the connection into non-blocking mode. */

void
packet_set_nonblocking(void)
{
	/* Set the socket into non-blocking mode. */
	set_nonblock(active_state->connection_in);

	if (active_state->connection_out != active_state->connection_in)
		set_nonblock(active_state->connection_out);
}

/* Returns the socket used for reading. */

int
packet_get_connection_in(void)
{
	return active_state->connection_in;
}

/* Returns the descriptor used for writing. */

int
packet_get_connection_out(void)
{
	return active_state->connection_out;
}

/* Closes the connection and clears and frees internal data structures. */

void
packet_close(void)
{
	if (!active_state->initialized)
		return;
	active_state->initialized = 0;
	if (active_state->connection_in == active_state->connection_out) {
		shutdown(active_state->connection_out, SHUT_RDWR);
		close(active_state->connection_out);
	} else {
		close(active_state->connection_in);
		close(active_state->connection_out);
	}
	buffer_free(&active_state->input);
	buffer_free(&active_state->output);
	buffer_free(&active_state->outgoing_packet);
	buffer_free(&active_state->incoming_packet);
	if (active_state->compression_buffer_ready) {
		buffer_free(&active_state->compression_buffer);
		buffer_compress_uninit();
	}
	cipher_cleanup(&active_state->send_context);
	cipher_cleanup(&active_state->receive_context);
}

/* Sets remote side protocol flags. */

void
packet_set_protocol_flags(u_int protocol_flags)
{
	active_state->remote_protocol_flags = protocol_flags;
}

/* Returns the remote protocol flags set earlier by the above function. */

u_int
packet_get_protocol_flags(void)
{
	return active_state->remote_protocol_flags;
}

/*
 * Starts packet compression from the next packet on in both directions.
 * Level is compression level 1 (fastest) - 9 (slow, best) as in gzip.
 */

static void
packet_init_compression(void)
{
	if (active_state->compression_buffer_ready == 1)
		return;
	active_state->compression_buffer_ready = 1;
	buffer_init(&active_state->compression_buffer);
}

void
packet_start_compression(int level)
{
	if (active_state->packet_compression && !compat20)
		fatal("Compression already enabled.");
	active_state->packet_compression = 1;
	packet_init_compression();
	buffer_compress_init_send(level);
	buffer_compress_init_recv();
}

/*
 * Causes any further packets to be encrypted using the given key.  The same
 * key is used for both sending and reception.  However, both directions are
 * encrypted independently of each other.
 */

void
packet_set_encryption_key(const u_char *key, u_int keylen, int number)
{
	Cipher *cipher = cipher_by_number(number);

	if (cipher == NULL)
		fatal("packet_set_encryption_key: unknown cipher number %d", number);
	if (keylen < 20)
		fatal("packet_set_encryption_key: keylen too small: %d", keylen);
	if (keylen > SSH_SESSION_KEY_LENGTH)
		fatal("packet_set_encryption_key: keylen too big: %d", keylen);
	memcpy(active_state->ssh1_key, key, keylen);
	active_state->ssh1_keylen = keylen;
	cipher_init(&active_state->send_context, cipher, key, keylen, NULL,
	    0, CIPHER_ENCRYPT);
	cipher_init(&active_state->receive_context, cipher, key, keylen, NULL,
	    0, CIPHER_DECRYPT);
}

u_int
packet_get_encryption_key(u_char *key)
{
	if (key == NULL)
		return (active_state->ssh1_keylen);
	memcpy(key, active_state->ssh1_key, active_state->ssh1_keylen);
	return (active_state->ssh1_keylen);
}

/* Start constructing a packet to send. */
void
packet_start(u_char type)
{
	u_char buf[9];
	int len;

	DBG(debug("packet_start[%d]", type));
	len = compat20 ? 6 : 9;
	memset(buf, 0, len - 1);
	buf[len - 1] = type;
	buffer_clear(&active_state->outgoing_packet);
	buffer_append(&active_state->outgoing_packet, buf, len);
}

/* Append payload. */
void
packet_put_char(int value)
{
	char ch = value;

	buffer_append(&active_state->outgoing_packet, &ch, 1);
}

void
packet_put_int(u_int value)
{
	buffer_put_int(&active_state->outgoing_packet, value);
}

void
packet_put_int64(u_int64_t value)
{
	buffer_put_int64(&active_state->outgoing_packet, value);
}

void
packet_put_string(const void *buf, u_int len)
{
	buffer_put_string(&active_state->outgoing_packet, buf, len);
}

void
packet_put_cstring(const char *str)
{
	buffer_put_cstring(&active_state->outgoing_packet, str);
}

void
packet_put_raw(const void *buf, u_int len)
{
	buffer_append(&active_state->outgoing_packet, buf, len);
}

void
packet_put_bignum(BIGNUM * value)
{
	buffer_put_bignum(&active_state->outgoing_packet, value);
}

void
packet_put_bignum2(BIGNUM * value)
{
	buffer_put_bignum2(&active_state->outgoing_packet, value);
}

#ifdef OPENSSL_HAS_ECC
void
packet_put_ecpoint(const EC_GROUP *curve, const EC_POINT *point)
{
	buffer_put_ecpoint(&active_state->outgoing_packet, curve, point);
}
#endif

/*
 * Finalizes and sends the packet.  If the encryption key has been set,
 * encrypts the packet before sending.
 */

static void
packet_send1(void)
{
	u_char buf[8], *cp;
	int i, padding, len;
	u_int checksum;
	u_int32_t rnd = 0;

	/*
	 * If using packet compression, compress the payload of the outgoing
	 * packet.
	 */
	if (active_state->packet_compression) {
		buffer_clear(&active_state->compression_buffer);
		/* Skip padding. */
		buffer_consume(&active_state->outgoing_packet, 8);
		/* padding */
		buffer_append(&active_state->compression_buffer,
		    "\0\0\0\0\0\0\0\0", 8);
		buffer_compress(&active_state->outgoing_packet,
		    &active_state->compression_buffer);
		buffer_clear(&active_state->outgoing_packet);
		buffer_append(&active_state->outgoing_packet,
		    buffer_ptr(&active_state->compression_buffer),
		    buffer_len(&active_state->compression_buffer));
	}
	/* Compute packet length without padding (add checksum, remove padding). */
	len = buffer_len(&active_state->outgoing_packet) + 4 - 8;

	/* Insert padding. Initialized to zero in packet_start1() */
	padding = 8 - len % 8;
	if (!active_state->send_context.plaintext) {
		cp = buffer_ptr(&active_state->outgoing_packet);
		for (i = 0; i < padding; i++) {
			if (i % 4 == 0)
				rnd = arc4random();
			cp[7 - i] = rnd & 0xff;
			rnd >>= 8;
		}
	}
	buffer_consume(&active_state->outgoing_packet, 8 - padding);

	/* Add check bytes. */
	checksum = ssh_crc32(buffer_ptr(&active_state->outgoing_packet),
	    buffer_len(&active_state->outgoing_packet));
	put_u32(buf, checksum);
	buffer_append(&active_state->outgoing_packet, buf, 4);

#ifdef PACKET_DEBUG
	fprintf(stderr, "packet_send plain: ");
	buffer_dump(&active_state->outgoing_packet);
#endif

	/* Append to output. */
	put_u32(buf, len);
	buffer_append(&active_state->output, buf, 4);
	cp = buffer_append_space(&active_state->output,
	    buffer_len(&active_state->outgoing_packet));
	cipher_crypt(&active_state->send_context, cp,
	    buffer_ptr(&active_state->outgoing_packet),
	    buffer_len(&active_state->outgoing_packet), 0, 0);

#ifdef PACKET_DEBUG
	fprintf(stderr, "encrypted: ");
	buffer_dump(&active_state->output);
#endif
	active_state->p_send.packets++;
	active_state->p_send.bytes += len +
	    buffer_len(&active_state->outgoing_packet);
	buffer_clear(&active_state->outgoing_packet);

	/*
	 * Note that the packet is now only buffered in output.  It won't be
	 * actually sent until packet_write_wait or packet_write_poll is
	 * called.
	 */
}

void
set_newkeys(int mode)
{
	Enc *enc;
	Mac *mac;
	Comp *comp;
	CipherContext *cc;
	u_int64_t *max_blocks;
	int crypt_type;

	debug2("set_newkeys: mode %d", mode);

	if (mode == MODE_OUT) {
		cc = &active_state->send_context;
		crypt_type = CIPHER_ENCRYPT;
		active_state->p_send.packets = active_state->p_send.blocks = 0;
		max_blocks = &active_state->max_blocks_out;
	} else {
		cc = &active_state->receive_context;
		crypt_type = CIPHER_DECRYPT;
		active_state->p_read.packets = active_state->p_read.blocks = 0;
		max_blocks = &active_state->max_blocks_in;
	}
	if (active_state->newkeys[mode] != NULL) {
		debug("set_newkeys: rekeying");
		cipher_cleanup(cc);
		enc  = &active_state->newkeys[mode]->enc;
		mac  = &active_state->newkeys[mode]->mac;
		comp = &active_state->newkeys[mode]->comp;
		mac_clear(mac);
		memset(enc->iv,  0, enc->iv_len);
		memset(enc->key, 0, enc->key_len);
		memset(mac->key, 0, mac->key_len);
		xfree(enc->name);
		xfree(enc->iv);
		xfree(enc->key);
		xfree(mac->name);
		xfree(mac->key);
		xfree(comp->name);
		xfree(active_state->newkeys[mode]);
	}
	active_state->newkeys[mode] = kex_get_newkeys(mode);
	if (active_state->newkeys[mode] == NULL)
		fatal("newkeys: no keys for mode %d", mode);
	enc  = &active_state->newkeys[mode]->enc;
	mac  = &active_state->newkeys[mode]->mac;
	comp = &active_state->newkeys[mode]->comp;
	if (cipher_authlen(enc->cipher) == 0 && mac_init(mac) == 0)
		mac->enabled = 1;
	DBG(debug("cipher_init_context: %d", mode));
	cipher_init(cc, enc->cipher, enc->key, enc->key_len,
	    enc->iv, enc->iv_len, crypt_type);
	/* Deleting the keys does not gain extra security */
	/* memset(enc->iv,  0, enc->block_size);
	   memset(enc->key, 0, enc->key_len);
	   memset(mac->key, 0, mac->key_len); */
	if ((comp->type == COMP_ZLIB ||
	    (comp->type == COMP_DELAYED &&
	     active_state->after_authentication)) && comp->enabled == 0) {
		packet_init_compression();
		if (mode == MODE_OUT)
			buffer_compress_init_send(6);
		else
			buffer_compress_init_recv();
		comp->enabled = 1;
	}
	/*
	 * The 2^(blocksize*2) limit is too expensive for 3DES,
	 * blowfish, etc, so enforce a 1GB limit for small blocksizes.
	 */
	if (enc->block_size >= 16)
		*max_blocks = (u_int64_t)1 << (enc->block_size*2);
	else
		*max_blocks = ((u_int64_t)1 << 30) / enc->block_size;
	if (active_state->rekey_limit)
		*max_blocks = MIN(*max_blocks,
		    active_state->rekey_limit / enc->block_size);
}

/*
 * Delayed compression for SSH2 is enabled after authentication:
 * This happens on the server side after a SSH2_MSG_USERAUTH_SUCCESS is sent,
 * and on the client side after a SSH2_MSG_USERAUTH_SUCCESS is received.
 */
static void
packet_enable_delayed_compress(void)
{
	Comp *comp = NULL;
	int mode;

	/*
	 * Remember that we are past the authentication step, so rekeying
	 * with COMP_DELAYED will turn on compression immediately.
	 */
	active_state->after_authentication = 1;
	for (mode = 0; mode < MODE_MAX; mode++) {
		/* protocol error: USERAUTH_SUCCESS received before NEWKEYS */
		if (active_state->newkeys[mode] == NULL)
			continue;
		comp = &active_state->newkeys[mode]->comp;
		if (comp && !comp->enabled && comp->type == COMP_DELAYED) {
			packet_init_compression();
			if (mode == MODE_OUT)
				buffer_compress_init_send(6);
			else
				buffer_compress_init_recv();
			comp->enabled = 1;
		}
	}
}

/*
 * Finalize packet in SSH2 format (compress, mac, encrypt, enqueue)
 */
static void
packet_send2_wrapped(void)
{
	u_char type, *cp, *macbuf = NULL;
	u_char padlen, pad = 0;
	u_int i, len, authlen = 0, aadlen = 0;
	u_int32_t rnd = 0;
	Enc *enc   = NULL;
	Mac *mac   = NULL;
	Comp *comp = NULL;
	int block_size;

	if (active_state->newkeys[MODE_OUT] != NULL) {
		enc  = &active_state->newkeys[MODE_OUT]->enc;
		mac  = &active_state->newkeys[MODE_OUT]->mac;
		comp = &active_state->newkeys[MODE_OUT]->comp;
		/* disable mac for authenticated encryption */
		if ((authlen = cipher_authlen(enc->cipher)) != 0)
			mac = NULL;
	}
	block_size = enc ? enc->block_size : 8;
	aadlen = (mac && mac->enabled && mac->etm) || authlen ? 4 : 0;

	cp = buffer_ptr(&active_state->outgoing_packet);
	type = cp[5];

#ifdef PACKET_DEBUG
	fprintf(stderr, "plain:     ");
	buffer_dump(&active_state->outgoing_packet);
#endif

	if (comp && comp->enabled) {
		len = buffer_len(&active_state->outgoing_packet);
		/* skip header, compress only payload */
		buffer_consume(&active_state->outgoing_packet, 5);
		buffer_clear(&active_state->compression_buffer);
		buffer_compress(&active_state->outgoing_packet,
		    &active_state->compression_buffer);
		buffer_clear(&active_state->outgoing_packet);
		buffer_append(&active_state->outgoing_packet, "\0\0\0\0\0", 5);
		buffer_append(&active_state->outgoing_packet,
		    buffer_ptr(&active_state->compression_buffer),
		    buffer_len(&active_state->compression_buffer));
		DBG(debug("compression: raw %d compressed %d", len,
		    buffer_len(&active_state->outgoing_packet)));
	}

	/* sizeof (packet_len + pad_len + payload) */
	len = buffer_len(&active_state->outgoing_packet);

	/*
	 * calc size of padding, alloc space, get random data,
	 * minimum padding is 4 bytes
	 */
	len -= aadlen; /* packet length is not encrypted for EtM modes */
	padlen = block_size - (len % block_size);
	if (padlen < 4)
		padlen += block_size;
	if (active_state->extra_pad) {
		/* will wrap if extra_pad+padlen > 255 */
		active_state->extra_pad =
		    roundup(active_state->extra_pad, block_size);
		pad = active_state->extra_pad -
		    ((len + padlen) % active_state->extra_pad);
		debug3("packet_send2: adding %d (len %d padlen %d extra_pad %d)",
		    pad, len, padlen, active_state->extra_pad);
		padlen += pad;
		active_state->extra_pad = 0;
	}
	cp = buffer_append_space(&active_state->outgoing_packet, padlen);
	if (enc && !active_state->send_context.plaintext) {
		/* random padding */
		for (i = 0; i < padlen; i++) {
			if (i % 4 == 0)
				rnd = arc4random();
			cp[i] = rnd & 0xff;
			rnd >>= 8;
		}
	} else {
		/* clear padding */
		memset(cp, 0, padlen);
	}
	/* sizeof (packet_len + pad_len + payload + padding) */
	len = buffer_len(&active_state->outgoing_packet);
	cp = buffer_ptr(&active_state->outgoing_packet);
	/* packet_length includes payload, padding and padding length field */
	put_u32(cp, len - 4);
	cp[4] = padlen;
	DBG(debug("send: len %d (includes padlen %d, aadlen %d)",
	    len, padlen, aadlen));

	/* compute MAC over seqnr and packet(length fields, payload, padding) */
	if (mac && mac->enabled && !mac->etm) {
		macbuf = mac_compute(mac, active_state->p_send.seqnr,
		    buffer_ptr(&active_state->outgoing_packet), len);
		DBG(debug("done calc MAC out #%d", active_state->p_send.seqnr));
	}
	/* encrypt packet and append to output buffer. */
	cp = buffer_append_space(&active_state->output, len + authlen);
	cipher_crypt(&active_state->send_context, cp,
	    buffer_ptr(&active_state->outgoing_packet),
	    len - aadlen, aadlen, authlen);
	/* append unencrypted MAC */
	if (mac && mac->enabled) {
		if (mac->etm) {
			/* EtM: compute mac over aadlen + cipher text */
			macbuf = mac_compute(mac,
			    active_state->p_send.seqnr, cp, len);
			DBG(debug("done calc MAC(EtM) out #%d",
			    active_state->p_send.seqnr));
		}
		buffer_append(&active_state->output, macbuf, mac->mac_len);
	}
#ifdef PACKET_DEBUG
	fprintf(stderr, "encrypted: ");
	buffer_dump(&active_state->output);
#endif
	/* increment sequence number for outgoing packets */
	if (++active_state->p_send.seqnr == 0)
		logit("outgoing seqnr wraps around");
	if (++active_state->p_send.packets == 0)
		if (!(datafellows & SSH_BUG_NOREKEY))
			fatal("XXX too many packets with same key");
	active_state->p_send.blocks += len / block_size;
	active_state->p_send.bytes += len;
	buffer_clear(&active_state->outgoing_packet);

	if (type == SSH2_MSG_NEWKEYS)
		set_newkeys(MODE_OUT);
	else if (type == SSH2_MSG_USERAUTH_SUCCESS && active_state->server_side)
		packet_enable_delayed_compress();
}

static void
packet_send2(void)
{
	struct packet *p;
	u_char type, *cp;

	cp = buffer_ptr(&active_state->outgoing_packet);
	type = cp[5];

	/* during rekeying we can only send key exchange messages */
	if (active_state->rekeying) {
		if ((type < SSH2_MSG_TRANSPORT_MIN) ||
		    (type > SSH2_MSG_TRANSPORT_MAX) ||
		    (type == SSH2_MSG_SERVICE_REQUEST) ||
		    (type == SSH2_MSG_SERVICE_ACCEPT)) {
			debug("enqueue packet: %u", type);
			p = xmalloc(sizeof(*p));
			p->type = type;
			memcpy(&p->payload, &active_state->outgoing_packet,
			    sizeof(Buffer));
			buffer_init(&active_state->outgoing_packet);
			TAILQ_INSERT_TAIL(&active_state->outgoing, p, next);
			return;
		}
	}

	/* rekeying starts with sending KEXINIT */
	if (type == SSH2_MSG_KEXINIT)
		active_state->rekeying = 1;

	packet_send2_wrapped();

	/* after a NEWKEYS message we can send the complete queue */
	if (type == SSH2_MSG_NEWKEYS) {
		active_state->rekeying = 0;
		while ((p = TAILQ_FIRST(&active_state->outgoing))) {
			type = p->type;
			debug("dequeue packet: %u", type);
			buffer_free(&active_state->outgoing_packet);
			memcpy(&active_state->outgoing_packet, &p->payload,
			    sizeof(Buffer));
			TAILQ_REMOVE(&active_state->outgoing, p, next);
			xfree(p);
			packet_send2_wrapped();
		}
	}
}

void
packet_send(void)
{
	if (compat20)
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
packet_read_seqnr(u_int32_t *seqnr_p)
{
	int type, len, ret, ms_remain, cont;
	fd_set *setp;
	char buf[8192];
	struct timeval timeout, start, *timeoutp = NULL;

	DBG(debug("packet_read()"));

	setp = (fd_set *)xcalloc(howmany(active_state->connection_in + 1,
	    NFDBITS), sizeof(fd_mask));

	/* Since we are blocking, ensure that all written packets have been sent. */
	packet_write_wait();

	/* Stay in the loop until we have received a complete packet. */
	for (;;) {
		/* Try to read a packet from the buffer. */
		type = packet_read_poll_seqnr(seqnr_p);
		if (!compat20 && (
		    type == SSH_SMSG_SUCCESS
		    || type == SSH_SMSG_FAILURE
		    || type == SSH_CMSG_EOF
		    || type == SSH_CMSG_EXIT_CONFIRMATION))
			packet_check_eom();
		/* If we got a packet, return it. */
		if (type != SSH_MSG_NONE) {
			xfree(setp);
			return type;
		}
		/*
		 * Otherwise, wait for some data to arrive, add it to the
		 * buffer, and try again.
		 */
		memset(setp, 0, howmany(active_state->connection_in + 1,
		    NFDBITS) * sizeof(fd_mask));
		FD_SET(active_state->connection_in, setp);

		if (active_state->packet_timeout_ms > 0) {
			ms_remain = active_state->packet_timeout_ms;
			timeoutp = &timeout;
		}
		/* Wait for some data to arrive. */
		for (;;) {
			if (active_state->packet_timeout_ms != -1) {
				ms_to_timeval(&timeout, ms_remain);
				gettimeofday(&start, NULL);
			}
			if ((ret = select(active_state->connection_in + 1, setp,
			    NULL, NULL, timeoutp)) >= 0)
				break;
			if (errno != EAGAIN && errno != EINTR &&
			    errno != EWOULDBLOCK)
				break;
			if (active_state->packet_timeout_ms == -1)
				continue;
			ms_subtract_diff(&start, &ms_remain);
			if (ms_remain <= 0) {
				ret = 0;
				break;
			}
		}
		if (ret == 0) {
			logit("Connection to %.200s timed out while "
			    "waiting to read", get_remote_ipaddr());
			cleanup_exit(255);
		}
		/* Read data from the socket. */
		do {
			cont = 0;
			len = roaming_read(active_state->connection_in, buf,
			    sizeof(buf), &cont);
		} while (len == 0 && cont);
		if (len == 0) {
			logit("Connection closed by %.200s", get_remote_ipaddr());
			cleanup_exit(255);
		}
		if (len < 0)
			fatal("Read from socket failed: %.100s", strerror(errno));
		/* Append it to the buffer. */
		packet_process_incoming(buf, len);
	}
	/* NOTREACHED */
}

int
packet_read(void)
{
	return packet_read_seqnr(NULL);
}

/*
 * Waits until a packet has been received, verifies that its type matches
 * that given, and gives a fatal error and exits if there is a mismatch.
 */

void
packet_read_expect(int expected_type)
{
	int type;

	type = packet_read();
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
 */

static int
packet_read_poll1(void)
{
	u_int len, padded_len;
	u_char *cp, type;
	u_int checksum, stored_checksum;

	/* Check if input size is less than minimum packet size. */
	if (buffer_len(&active_state->input) < 4 + 8)
		return SSH_MSG_NONE;
	/* Get length of incoming packet. */
	cp = buffer_ptr(&active_state->input);
	len = get_u32(cp);
	if (len < 1 + 2 + 2 || len > 256 * 1024)
		packet_disconnect("Bad packet length %u.", len);
	padded_len = (len + 8) & ~7;

	/* Check if the packet has been entirely received. */
	if (buffer_len(&active_state->input) < 4 + padded_len)
		return SSH_MSG_NONE;

	/* The entire packet is in buffer. */

	/* Consume packet length. */
	buffer_consume(&active_state->input, 4);

	/*
	 * Cryptographic attack detector for ssh
	 * (C)1998 CORE-SDI, Buenos Aires Argentina
	 * Ariel Futoransky(futo@core-sdi.com)
	 */
	if (!active_state->receive_context.plaintext) {
		switch (detect_attack(buffer_ptr(&active_state->input),
		    padded_len)) {
		case DEATTACK_DETECTED:
			packet_disconnect("crc32 compensation attack: "
			    "network attack detected");
		case DEATTACK_DOS_DETECTED:
			packet_disconnect("deattack denial of "
			    "service detected");
		}
	}

	/* Decrypt data to incoming_packet. */
	buffer_clear(&active_state->incoming_packet);
	cp = buffer_append_space(&active_state->incoming_packet, padded_len);
	cipher_crypt(&active_state->receive_context, cp,
	    buffer_ptr(&active_state->input), padded_len, 0, 0);

	buffer_consume(&active_state->input, padded_len);

#ifdef PACKET_DEBUG
	fprintf(stderr, "read_poll plain: ");
	buffer_dump(&active_state->incoming_packet);
#endif

	/* Compute packet checksum. */
	checksum = ssh_crc32(buffer_ptr(&active_state->incoming_packet),
	    buffer_len(&active_state->incoming_packet) - 4);

	/* Skip padding. */
	buffer_consume(&active_state->incoming_packet, 8 - len % 8);

	/* Test check bytes. */
	if (len != buffer_len(&active_state->incoming_packet))
		packet_disconnect("packet_read_poll1: len %d != buffer_len %d.",
		    len, buffer_len(&active_state->incoming_packet));

	cp = (u_char *)buffer_ptr(&active_state->incoming_packet) + len - 4;
	stored_checksum = get_u32(cp);
	if (checksum != stored_checksum)
		packet_disconnect("Corrupted check bytes on input.");
	buffer_consume_end(&active_state->incoming_packet, 4);

	if (active_state->packet_compression) {
		buffer_clear(&active_state->compression_buffer);
		buffer_uncompress(&active_state->incoming_packet,
		    &active_state->compression_buffer);
		buffer_clear(&active_state->incoming_packet);
		buffer_append(&active_state->incoming_packet,
		    buffer_ptr(&active_state->compression_buffer),
		    buffer_len(&active_state->compression_buffer));
	}
	active_state->p_read.packets++;
	active_state->p_read.bytes += padded_len + 4;
	type = buffer_get_char(&active_state->incoming_packet);
	if (type < SSH_MSG_MIN || type > SSH_MSG_MAX)
		packet_disconnect("Invalid ssh1 packet type: %d", type);
	return type;
}

static int
packet_read_poll2(u_int32_t *seqnr_p)
{
	u_int padlen, need;
	u_char *macbuf = NULL, *cp, type;
	u_int maclen, authlen = 0, aadlen = 0, block_size;
	Enc *enc   = NULL;
	Mac *mac   = NULL;
	Comp *comp = NULL;

	if (active_state->packet_discard)
		return SSH_MSG_NONE;

	if (active_state->newkeys[MODE_IN] != NULL) {
		enc  = &active_state->newkeys[MODE_IN]->enc;
		mac  = &active_state->newkeys[MODE_IN]->mac;
		comp = &active_state->newkeys[MODE_IN]->comp;
		/* disable mac for authenticated encryption */
		if ((authlen = cipher_authlen(enc->cipher)) != 0)
			mac = NULL;
	}
	maclen = mac && mac->enabled ? mac->mac_len : 0;
	block_size = enc ? enc->block_size : 8;
	aadlen = (mac && mac->enabled && mac->etm) || authlen ? 4 : 0;

	if (aadlen && active_state->packlen == 0) {
		if (buffer_len(&active_state->input) < 4)
			return SSH_MSG_NONE;
		cp = buffer_ptr(&active_state->input);
		active_state->packlen = get_u32(cp);
		if (active_state->packlen < 1 + 4 ||
		    active_state->packlen > PACKET_MAX_SIZE) {
#ifdef PACKET_DEBUG
			buffer_dump(&active_state->input);
#endif
			logit("Bad packet length %u.", active_state->packlen);
			packet_disconnect("Packet corrupt");
		}
		buffer_clear(&active_state->incoming_packet);
	} else if (active_state->packlen == 0) {
		/*
		 * check if input size is less than the cipher block size,
		 * decrypt first block and extract length of incoming packet
		 */
		if (buffer_len(&active_state->input) < block_size)
			return SSH_MSG_NONE;
		buffer_clear(&active_state->incoming_packet);
		cp = buffer_append_space(&active_state->incoming_packet,
		    block_size);
		cipher_crypt(&active_state->receive_context, cp,
		    buffer_ptr(&active_state->input), block_size, 0, 0);
		cp = buffer_ptr(&active_state->incoming_packet);
		active_state->packlen = get_u32(cp);
		if (active_state->packlen < 1 + 4 ||
		    active_state->packlen > PACKET_MAX_SIZE) {
#ifdef PACKET_DEBUG
			buffer_dump(&active_state->incoming_packet);
#endif
			logit("Bad packet length %u.", active_state->packlen);
			packet_start_discard(enc, mac, active_state->packlen,
			    PACKET_MAX_SIZE);
			return SSH_MSG_NONE;
		}
		buffer_consume(&active_state->input, block_size);
	}
	DBG(debug("input: packet len %u", active_state->packlen+4));
	if (aadlen) {
		/* only the payload is encrypted */
		need = active_state->packlen;
	} else {
		/*
		 * the payload size and the payload are encrypted, but we
		 * have a partial packet of block_size bytes
		 */
		need = 4 + active_state->packlen - block_size;
	}
	DBG(debug("partial packet: block %d, need %d, maclen %d, authlen %d,"
	    " aadlen %d", block_size, need, maclen, authlen, aadlen));
	if (need % block_size != 0) {
		logit("padding error: need %d block %d mod %d",
		    need, block_size, need % block_size);
		packet_start_discard(enc, mac, active_state->packlen,
		    PACKET_MAX_SIZE - block_size);
		return SSH_MSG_NONE;
	}
	/*
	 * check if the entire packet has been received and
	 * decrypt into incoming_packet:
	 * 'aadlen' bytes are unencrypted, but authenticated.
	 * 'need' bytes are encrypted, followed by either
	 * 'authlen' bytes of authentication tag or
	 * 'maclen' bytes of message authentication code.
	 */
	if (buffer_len(&active_state->input) < aadlen + need + authlen + maclen)
		return SSH_MSG_NONE;
#ifdef PACKET_DEBUG
	fprintf(stderr, "read_poll enc/full: ");
	buffer_dump(&active_state->input);
#endif
	/* EtM: compute mac over encrypted input */
	if (mac && mac->enabled && mac->etm)
		macbuf = mac_compute(mac, active_state->p_read.seqnr,
		    buffer_ptr(&active_state->input), aadlen + need);
	cp = buffer_append_space(&active_state->incoming_packet, aadlen + need);
	cipher_crypt(&active_state->receive_context, cp,
	    buffer_ptr(&active_state->input), need, aadlen, authlen);
	buffer_consume(&active_state->input, aadlen + need + authlen);
	/*
	 * compute MAC over seqnr and packet,
	 * increment sequence number for incoming packet
	 */
	if (mac && mac->enabled) {
		if (!mac->etm)
			macbuf = mac_compute(mac, active_state->p_read.seqnr,
			    buffer_ptr(&active_state->incoming_packet),
			    buffer_len(&active_state->incoming_packet));
		if (timingsafe_bcmp(macbuf, buffer_ptr(&active_state->input),
		    mac->mac_len) != 0) {
			logit("Corrupted MAC on input.");
			if (need > PACKET_MAX_SIZE)
				fatal("internal error need %d", need);
			packet_start_discard(enc, mac, active_state->packlen,
			    PACKET_MAX_SIZE - need);
			return SSH_MSG_NONE;
		}
				
		DBG(debug("MAC #%d ok", active_state->p_read.seqnr));
		buffer_consume(&active_state->input, mac->mac_len);
	}
	/* XXX now it's safe to use fatal/packet_disconnect */
	if (seqnr_p != NULL)
		*seqnr_p = active_state->p_read.seqnr;
	if (++active_state->p_read.seqnr == 0)
		logit("incoming seqnr wraps around");
	if (++active_state->p_read.packets == 0)
		if (!(datafellows & SSH_BUG_NOREKEY))
			fatal("XXX too many packets with same key");
	active_state->p_read.blocks += (active_state->packlen + 4) / block_size;
	active_state->p_read.bytes += active_state->packlen + 4;

	/* get padlen */
	cp = buffer_ptr(&active_state->incoming_packet);
	padlen = cp[4];
	DBG(debug("input: padlen %d", padlen));
	if (padlen < 4)
		packet_disconnect("Corrupted padlen %d on input.", padlen);

	/* skip packet size + padlen, discard padding */
	buffer_consume(&active_state->incoming_packet, 4 + 1);
	buffer_consume_end(&active_state->incoming_packet, padlen);

	DBG(debug("input: len before de-compress %d",
	    buffer_len(&active_state->incoming_packet)));
	if (comp && comp->enabled) {
		buffer_clear(&active_state->compression_buffer);
		buffer_uncompress(&active_state->incoming_packet,
		    &active_state->compression_buffer);
		buffer_clear(&active_state->incoming_packet);
		buffer_append(&active_state->incoming_packet,
		    buffer_ptr(&active_state->compression_buffer),
		    buffer_len(&active_state->compression_buffer));
		DBG(debug("input: len after de-compress %d",
		    buffer_len(&active_state->incoming_packet)));
	}
	/*
	 * get packet type, implies consume.
	 * return length of payload (without type field)
	 */
	type = buffer_get_char(&active_state->incoming_packet);
	if (type < SSH2_MSG_MIN || type >= SSH2_MSG_LOCAL_MIN)
		packet_disconnect("Invalid ssh2 packet type: %d", type);
	if (type == SSH2_MSG_NEWKEYS)
		set_newkeys(MODE_IN);
	else if (type == SSH2_MSG_USERAUTH_SUCCESS &&
	    !active_state->server_side)
		packet_enable_delayed_compress();
#ifdef PACKET_DEBUG
	fprintf(stderr, "read/plain[%d]:\r\n", type);
	buffer_dump(&active_state->incoming_packet);
#endif
	/* reset for next packet */
	active_state->packlen = 0;
	return type;
}

int
packet_read_poll_seqnr(u_int32_t *seqnr_p)
{
	u_int reason, seqnr;
	u_char type;
	char *msg;

	for (;;) {
		if (compat20) {
			type = packet_read_poll2(seqnr_p);
			if (type) {
				active_state->keep_alive_timeouts = 0;
				DBG(debug("received packet type %d", type));
			}
			switch (type) {
			case SSH2_MSG_IGNORE:
				debug3("Received SSH2_MSG_IGNORE");
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
				/* Ignore normal client exit notifications */
				do_log2(active_state->server_side &&
				    reason == SSH2_DISCONNECT_BY_APPLICATION ?
				    SYSLOG_LEVEL_INFO : SYSLOG_LEVEL_ERROR,
				    "Received disconnect from %s: %u: %.400s",
				    get_remote_ipaddr(), reason, msg);
				xfree(msg);
				cleanup_exit(255);
				break;
			case SSH2_MSG_UNIMPLEMENTED:
				seqnr = packet_get_int();
				debug("Received SSH2_MSG_UNIMPLEMENTED for %u",
				    seqnr);
				break;
			default:
				return type;
			}
		} else {
			type = packet_read_poll1();
			switch (type) {
			case SSH_MSG_IGNORE:
				break;
			case SSH_MSG_DEBUG:
				msg = packet_get_string(NULL);
				debug("Remote: %.900s", msg);
				xfree(msg);
				break;
			case SSH_MSG_DISCONNECT:
				msg = packet_get_string(NULL);
				error("Received disconnect from %s: %.400s",
				    get_remote_ipaddr(), msg);
				cleanup_exit(255);
				break;
			default:
				if (type)
					DBG(debug("received packet type %d", type));
				return type;
			}
		}
	}
}

/*
 * Buffers the given amount of input characters.  This is intended to be used
 * together with packet_read_poll.
 */

void
packet_process_incoming(const char *buf, u_int len)
{
	if (active_state->packet_discard) {
		active_state->keep_alive_timeouts = 0; /* ?? */
		if (len >= active_state->packet_discard)
			packet_stop_discard();
		active_state->packet_discard -= len;
		return;
	}
	buffer_append(&active_state->input, buf, len);
}

/* Returns a character from the packet. */

u_int
packet_get_char(void)
{
	char ch;

	buffer_get(&active_state->incoming_packet, &ch, 1);
	return (u_char) ch;
}

/* Returns an integer from the packet data. */

u_int
packet_get_int(void)
{
	return buffer_get_int(&active_state->incoming_packet);
}

/* Returns an 64 bit integer from the packet data. */

u_int64_t
packet_get_int64(void)
{
	return buffer_get_int64(&active_state->incoming_packet);
}

/*
 * Returns an arbitrary precision integer from the packet data.  The integer
 * must have been initialized before this call.
 */

void
packet_get_bignum(BIGNUM * value)
{
	buffer_get_bignum(&active_state->incoming_packet, value);
}

void
packet_get_bignum2(BIGNUM * value)
{
	buffer_get_bignum2(&active_state->incoming_packet, value);
}

#ifdef OPENSSL_HAS_ECC
void
packet_get_ecpoint(const EC_GROUP *curve, EC_POINT *point)
{
	buffer_get_ecpoint(&active_state->incoming_packet, curve, point);
}
#endif

void *
packet_get_raw(u_int *length_ptr)
{
	u_int bytes = buffer_len(&active_state->incoming_packet);

	if (length_ptr != NULL)
		*length_ptr = bytes;
	return buffer_ptr(&active_state->incoming_packet);
}

int
packet_remaining(void)
{
	return buffer_len(&active_state->incoming_packet);
}

/*
 * Returns a string from the packet data.  The string is allocated using
 * xmalloc; it is the responsibility of the calling program to free it when
 * no longer needed.  The length_ptr argument may be NULL, or point to an
 * integer into which the length of the string is stored.
 */

void *
packet_get_string(u_int *length_ptr)
{
	return buffer_get_string(&active_state->incoming_packet, length_ptr);
}

void *
packet_get_string_ptr(u_int *length_ptr)
{
	return buffer_get_string_ptr(&active_state->incoming_packet, length_ptr);
}

/* Ensures the returned string has no embedded \0 characters in it. */
char *
packet_get_cstring(u_int *length_ptr)
{
	return buffer_get_cstring(&active_state->incoming_packet, length_ptr);
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

	if (compat20 && (datafellows & SSH_BUG_DEBUG))
		return;

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

	/* Display the error locally */
	logit("Disconnecting: %.100s", buf);

	/* Send the disconnect message to the other side, and wait for it to get sent. */
	if (compat20) {
		packet_start(SSH2_MSG_DISCONNECT);
		packet_put_int(SSH2_DISCONNECT_PROTOCOL_ERROR);
		packet_put_cstring(buf);
		packet_put_cstring("");
	} else {
		packet_start(SSH_MSG_DISCONNECT);
		packet_put_cstring(buf);
	}
	packet_send();
	packet_write_wait();

	/* Stop listening for connections. */
	channel_close_all();

	/* Close the connection. */
	packet_close();
	cleanup_exit(255);
}

/* Checks if there is any buffered output, and tries to write some of the output. */

void
packet_write_poll(void)
{
	int len = buffer_len(&active_state->output);
	int cont;

	if (len > 0) {
		cont = 0;
		len = roaming_write(active_state->connection_out,
		    buffer_ptr(&active_state->output), len, &cont);
		if (len == -1) {
			if (errno == EINTR || errno == EAGAIN ||
			    errno == EWOULDBLOCK)
				return;
			fatal("Write failed: %.100s", strerror(errno));
		}
		if (len == 0 && !cont)
			fatal("Write connection closed");
		buffer_consume(&active_state->output, len);
	}
}

/*
 * Calls packet_write_poll repeatedly until all pending output data has been
 * written.
 */

void
packet_write_wait(void)
{
	fd_set *setp;
	int ret, ms_remain;
	struct timeval start, timeout, *timeoutp = NULL;

	setp = (fd_set *)xcalloc(howmany(active_state->connection_out + 1,
	    NFDBITS), sizeof(fd_mask));
	packet_write_poll();
	while (packet_have_data_to_write()) {
		memset(setp, 0, howmany(active_state->connection_out + 1,
		    NFDBITS) * sizeof(fd_mask));
		FD_SET(active_state->connection_out, setp);

		if (active_state->packet_timeout_ms > 0) {
			ms_remain = active_state->packet_timeout_ms;
			timeoutp = &timeout;
		}
		for (;;) {
			if (active_state->packet_timeout_ms != -1) {
				ms_to_timeval(&timeout, ms_remain);
				gettimeofday(&start, NULL);
			}
			if ((ret = select(active_state->connection_out + 1,
			    NULL, setp, NULL, timeoutp)) >= 0)
				break;
			if (errno != EAGAIN && errno != EINTR &&
			    errno != EWOULDBLOCK)
				break;
			if (active_state->packet_timeout_ms == -1)
				continue;
			ms_subtract_diff(&start, &ms_remain);
			if (ms_remain <= 0) {
				ret = 0;
				break;
			}
		}
		if (ret == 0) {
			logit("Connection to %.200s timed out while "
			    "waiting to write", get_remote_ipaddr());
			cleanup_exit(255);
		}
		packet_write_poll();
	}
	xfree(setp);
}

/* Returns true if there is buffered data to write to the connection. */

int
packet_have_data_to_write(void)
{
	return buffer_len(&active_state->output) != 0;
}

/* Returns true if there is not too much data to write to the connection. */

int
packet_not_very_much_data_to_write(void)
{
	if (active_state->interactive_mode)
		return buffer_len(&active_state->output) < 16384;
	else
		return buffer_len(&active_state->output) < 128 * 1024;
}

static void
packet_set_tos(int tos)
{
#ifndef IP_TOS_IS_BROKEN
	if (!packet_connection_is_on_socket())
		return;
	switch (packet_connection_af()) {
# ifdef IP_TOS
	case AF_INET:
		debug3("%s: set IP_TOS 0x%02x", __func__, tos);
		if (setsockopt(active_state->connection_in,
		    IPPROTO_IP, IP_TOS, &tos, sizeof(tos)) < 0)
			error("setsockopt IP_TOS %d: %.100s:",
			    tos, strerror(errno));
		break;
# endif /* IP_TOS */
# ifdef IPV6_TCLASS
	case AF_INET6:
		debug3("%s: set IPV6_TCLASS 0x%02x", __func__, tos);
		if (setsockopt(active_state->connection_in,
		    IPPROTO_IPV6, IPV6_TCLASS, &tos, sizeof(tos)) < 0)
			error("setsockopt IPV6_TCLASS %d: %.100s:",
			    tos, strerror(errno));
		break;
# endif /* IPV6_TCLASS */
	}
#endif /* IP_TOS_IS_BROKEN */
}

/* Informs that the current session is interactive.  Sets IP flags for that. */

void
packet_set_interactive(int interactive, int qos_interactive, int qos_bulk)
{
	if (active_state->set_interactive_called)
		return;
	active_state->set_interactive_called = 1;

	/* Record that we are in interactive mode. */
	active_state->interactive_mode = interactive;

	/* Only set socket options if using a socket.  */
	if (!packet_connection_is_on_socket())
		return;
	set_nodelay(active_state->connection_in);
	packet_set_tos(interactive ? qos_interactive : qos_bulk);
}

/* Returns true if the current connection is interactive. */

int
packet_is_interactive(void)
{
	return active_state->interactive_mode;
}

int
packet_set_maxsize(u_int s)
{
	if (active_state->set_maxsize_called) {
		logit("packet_set_maxsize: called twice: old %d new %d",
		    active_state->max_packet_size, s);
		return -1;
	}
	if (s < 4 * 1024 || s > 1024 * 1024) {
		logit("packet_set_maxsize: bad size %d", s);
		return -1;
	}
	active_state->set_maxsize_called = 1;
	debug("packet_set_maxsize: setting to %d", s);
	active_state->max_packet_size = s;
	return s;
}

int
packet_inc_alive_timeouts(void)
{
	return ++active_state->keep_alive_timeouts;
}

void
packet_set_alive_timeouts(int ka)
{
	active_state->keep_alive_timeouts = ka;
}

u_int
packet_get_maxsize(void)
{
	return active_state->max_packet_size;
}

/* roundup current message to pad bytes */
void
packet_add_padding(u_char pad)
{
	active_state->extra_pad = pad;
}

/*
 * 9.2.  Ignored Data Message
 *
 *   byte      SSH_MSG_IGNORE
 *   string    data
 *
 * All implementations MUST understand (and ignore) this message at any
 * time (after receiving the protocol version). No implementation is
 * required to send them. This message can be used as an additional
 * protection measure against advanced traffic analysis techniques.
 */
void
packet_send_ignore(int nbytes)
{
	u_int32_t rnd = 0;
	int i;

	packet_start(compat20 ? SSH2_MSG_IGNORE : SSH_MSG_IGNORE);
	packet_put_int(nbytes);
	for (i = 0; i < nbytes; i++) {
		if (i % 4 == 0)
			rnd = arc4random();
		packet_put_char((u_char)rnd & 0xff);
		rnd >>= 8;
	}
}

#define MAX_PACKETS	(1U<<31)
int
packet_need_rekeying(void)
{
	if (datafellows & SSH_BUG_NOREKEY)
		return 0;
	return
	    (active_state->p_send.packets > MAX_PACKETS) ||
	    (active_state->p_read.packets > MAX_PACKETS) ||
	    (active_state->max_blocks_out &&
	        (active_state->p_send.blocks > active_state->max_blocks_out)) ||
	    (active_state->max_blocks_in &&
	        (active_state->p_read.blocks > active_state->max_blocks_in));
}

void
packet_set_rekey_limit(u_int32_t bytes)
{
	active_state->rekey_limit = bytes;
}

void
packet_set_server(void)
{
	active_state->server_side = 1;
}

void
packet_set_authenticated(void)
{
	active_state->after_authentication = 1;
}

void *
packet_get_input(void)
{
	return (void *)&active_state->input;
}

void *
packet_get_output(void)
{
	return (void *)&active_state->output;
}

void *
packet_get_newkeys(int mode)
{
	return (void *)active_state->newkeys[mode];
}

/*
 * Save the state for the real connection, and use a separate state when
 * resuming a suspended connection.
 */
void
packet_backup_state(void)
{
	struct session_state *tmp;

	close(active_state->connection_in);
	active_state->connection_in = -1;
	close(active_state->connection_out);
	active_state->connection_out = -1;
	if (backup_state)
		tmp = backup_state;
	else
		tmp = alloc_session_state();
	backup_state = active_state;
	active_state = tmp;
}

/*
 * Swap in the old state when resuming a connecion.
 */
void
packet_restore_state(void)
{
	struct session_state *tmp;
	void *buf;
	u_int len;

	tmp = backup_state;
	backup_state = active_state;
	active_state = tmp;
	active_state->connection_in = backup_state->connection_in;
	backup_state->connection_in = -1;
	active_state->connection_out = backup_state->connection_out;
	backup_state->connection_out = -1;
	len = buffer_len(&backup_state->input);
	if (len > 0) {
		buf = buffer_ptr(&backup_state->input);
		buffer_append(&active_state->input, buf, len);
		buffer_clear(&backup_state->input);
		add_recv_bytes(len);
	}
}
