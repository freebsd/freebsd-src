/*
 * WPA Supplicant / wrapper functions for crypto libraries
 * Copyright (c) 2004-2005, Jouni Malinen <jkmaline@cc.hut.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 *
 * This file defines the cryptographic functions that need to be implemented
 * for wpa_supplicant and hostapd. When TLS is not used, internal
 * implementation of MD5, SHA1, and AES is used and no external libraries are
 * required. When TLS is enabled (e.g., by enabling EAP-TLS or EAP-PEAP), the
 * crypto library used by the TLS implementation is expected to be used for
 * non-TLS needs, too, in order to save space by not implementing these
 * functions twice.
 *
 * Wrapper code for using each crypto library is in its own file (crypto*.c)
 * and one of these files is build and linked in to provide the functions
 * defined here.
 */

#ifndef CRYPTO_H
#define CRYPTO_H

/**
 * md4_vector - MD4 hash for data vector
 * @num_elem: Number of elements in the data vector
 * @addr: Pointers to the data areas
 * @len: Lengths of the data blocks
 * @mac: Buffer for the hash
 */
void md4_vector(size_t num_elem, const u8 *addr[], const size_t *len, u8 *mac);

/**
 * md5_vector - MD5 hash for data vector
 * @num_elem: Number of elements in the data vector
 * @addr: Pointers to the data areas
 * @len: Lengths of the data blocks
 * @mac: Buffer for the hash
 */
void md5_vector(size_t num_elem, const u8 *addr[], const size_t *len, u8 *mac);

/**
 * sha1_vector - SHA-1 hash for data vector
 * @num_elem: Number of elements in the data vector
 * @addr: Pointers to the data areas
 * @len: Lengths of the data blocks
 * @mac: Buffer for the hash
 */
void sha1_vector(size_t num_elem, const u8 *addr[], const size_t *len,
		 u8 *mac);

/**
 * sha1_transform - Perform one SHA-1 transform step
 * @state: SHA-1 state
 * @data: Input data for the SHA-1 transform
 *
 * This function is used to implement random number generation specified in
 * NIST FIPS Publication 186-2 for EAP-SIM. This PRF uses a function that is
 * similar to SHA-1, but has different message padding and as such, access to
 * just part of the SHA-1 is needed.
 */
void sha1_transform(u8 *state, const u8 data[64]);

/**
 * des_encrypt - Encrypt one block with DES
 * @clear: 8 octets (in)
 * @key: 7 octets (in) (no parity bits included)
 * @cypher: 8 octets (out)
 */
void des_encrypt(const u8 *clear, const u8 *key, u8 *cypher);

/**
 * aes_encrypt_init - Initialize AES for encryption
 * @key: Encryption key
 * @len: Key length in bytes (usually 16, i.e., 128 bits)
 * Returns: Pointer to context data or %NULL on failure
 */
void * aes_encrypt_init(const u8 *key, size_t len);

/**
 * aes_encrypt - Encrypt one AES block
 * @ctx: Context pointer from aes_encrypt_init()
 * @plain: Plaintext data to be encrypted (16 bytes)
 * @crypt: Buffer for the encrypted data (16 bytes)
 */
void aes_encrypt(void *ctx, const u8 *plain, u8 *crypt);

/**
 * aes_encrypt_deinit - Deinitialize AES encryption
 * @ctx: Context pointer from aes_encrypt_init()
 */
void aes_encrypt_deinit(void *ctx);

/**
 * aes_decrypt_init - Initialize AES for decryption
 * @key: Decryption key
 * @len: Key length in bytes (usually 16, i.e., 128 bits)
 * Returns: Pointer to context data or %NULL on failure
 */
void * aes_decrypt_init(const u8 *key, size_t len);

/**
 * aes_decrypt - Decrypt one AES block
 * @ctx: Context pointer from aes_encrypt_init()
 * @crypt: Encrypted data (16 bytes)
 * @plain: Buffer for the decrypted data (16 bytes)
 */
void aes_decrypt(void *ctx, const u8 *crypt, u8 *plain);

/**
 * aes_decrypt_deinit - Deinitialize AES decryption
 * @ctx: Context pointer from aes_encrypt_init()
 */
void aes_decrypt_deinit(void *ctx);


#endif /* CRYPTO_H */
