/*
 * WPA Supplicant / shared MSCHAPV2 helper functions / RFC 2433 / RFC 2759
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
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "sha1.h"
#include "ms_funcs.h"
#include "crypto.h"
#include "rc4.h"


/**
 * challenge_hash - ChallengeHash() - RFC 2759, Sect. 8.2
 * @peer_challenge: 16-octet PeerChallenge (IN)
 * @auth_challenge: 16-octet AuthChallenge (IN)
 * @username: 0-to-256-char UserName (IN)
 * @username_len: Length of username
 * @challenge: 8-octet Challenge (OUT)
 */
static void challenge_hash(const u8 *peer_challenge, const u8 *auth_challenge,
			   const u8 *username, size_t username_len,
			   u8 *challenge)
{
	u8 hash[SHA1_MAC_LEN];
	const unsigned char *addr[3];
	size_t len[3];

	addr[0] = peer_challenge;
	len[0] = 16;
	addr[1] = auth_challenge;
	len[1] = 16;
	addr[2] = username;
	len[2] = username_len;

	sha1_vector(3, addr, len, hash);
	memcpy(challenge, hash, 8);
}


/**
 * nt_password_hash - NtPasswordHash() - RFC 2759, Sect. 8.3
 * @password: 0-to-256-unicode-char Password (IN)
 * @password_len: Length of password
 * @password_hash: 16-octet PasswordHash (OUT)
 */
void nt_password_hash(const u8 *password, size_t password_len,
		      u8 *password_hash)
{
	u8 *buf;
	int i;
	size_t len;

	/* Convert password into unicode */
	buf = malloc(password_len * 2);
	if (buf == NULL)
		return;
	memset(buf, 0, password_len * 2);
	for (i = 0; i < password_len; i++)
		buf[2 * i] = password[i];

	len = password_len * 2;
	md4_vector(1, (const u8 **) &buf, &len, password_hash);
	free(buf);
}


/**
 * hash_nt_password_hash - HashNtPasswordHash() - RFC 2759, Sect. 8.4
 * @password_hash: 16-octet PasswordHash (IN)
 * @password_hash_hash: 16-octet PaswordHashHash (OUT)
 */
void hash_nt_password_hash(const u8 *password_hash, u8 *password_hash_hash)
{
	size_t len = 16;
	md4_vector(1, &password_hash, &len, password_hash_hash);
}


/**
 * challenge_response - ChallengeResponse() - RFC 2759, Sect. 8.5
 * @challenge: 8-octet Challenge (IN)
 * @password_hash: 16-octet PasswordHash (IN)
 * @response: 24-octet Response (OUT)
 */
void challenge_response(const u8 *challenge, const u8 *password_hash,
			u8 *response)
{
	u8 zpwd[7];
	des_encrypt(challenge, password_hash, response);
	des_encrypt(challenge, password_hash + 7, response + 8);
	zpwd[0] = password_hash[14];
	zpwd[1] = password_hash[15];
	memset(zpwd + 2, 0, 5);
	des_encrypt(challenge, zpwd, response + 16);
}


/**
 * generate_nt_response - GenerateNTResponse() - RFC 2759, Sect. 8.1
 * @auth_challenge: 16-octet AuthenticatorChallenge (IN)
 * @peer_hallenge: 16-octet PeerChallenge (IN)
 * @username: 0-to-256-char UserName (IN)
 * @username_len: Length of username
 * @password: 0-to-256-unicode-char Password (IN)
 * @password_len: Length of password
 * @response: 24-octet Response (OUT)
 */
void generate_nt_response(const u8 *auth_challenge, const u8 *peer_challenge,
			  const u8 *username, size_t username_len,
			  const u8 *password, size_t password_len,
			  u8 *response)
{
	u8 challenge[8];
	u8 password_hash[16];

	challenge_hash(peer_challenge, auth_challenge, username, username_len,
		       challenge);
	nt_password_hash(password, password_len, password_hash);
	challenge_response(challenge, password_hash, response);
}


/**
 * generate_authenticator_response - GenerateAuthenticatorResponse() - RFC 2759, Sect. 8.7
 * @password: 0-to-256-unicode-char Password (IN)
 * @password_len: Length of password
 * @nt_response: 24-octet NT-Response (IN)
 * @peer_challenge: 16-octet PeerChallenge (IN)
 * @auth_challenge: 16-octet AuthenticatorChallenge (IN)
 * @username: 0-to-256-char UserName (IN)
 * @username_len: Length of username
 * @response: 42-octet AuthenticatorResponse (OUT)
 */
void generate_authenticator_response(const u8 *password, size_t password_len,
				     const u8 *peer_challenge,
				     const u8 *auth_challenge,
				     const u8 *username, size_t username_len,
				     const u8 *nt_response, u8 *response)
{
	static const u8 magic1[39] = {
		0x4D, 0x61, 0x67, 0x69, 0x63, 0x20, 0x73, 0x65, 0x72, 0x76,
		0x65, 0x72, 0x20, 0x74, 0x6F, 0x20, 0x63, 0x6C, 0x69, 0x65,
		0x6E, 0x74, 0x20, 0x73, 0x69, 0x67, 0x6E, 0x69, 0x6E, 0x67,
		0x20, 0x63, 0x6F, 0x6E, 0x73, 0x74, 0x61, 0x6E, 0x74
	};
	static const u8 magic2[41] = {
		0x50, 0x61, 0x64, 0x20, 0x74, 0x6F, 0x20, 0x6D, 0x61, 0x6B,
		0x65, 0x20, 0x69, 0x74, 0x20, 0x64, 0x6F, 0x20, 0x6D, 0x6F,
		0x72, 0x65, 0x20, 0x74, 0x68, 0x61, 0x6E, 0x20, 0x6F, 0x6E,
		0x65, 0x20, 0x69, 0x74, 0x65, 0x72, 0x61, 0x74, 0x69, 0x6F,
		0x6E
	};

	u8 password_hash[16], password_hash_hash[16], challenge[8];
	const unsigned char *addr1[3];
	const size_t len1[3] = { 16, 24, sizeof(magic1) };
	const unsigned char *addr2[3];
	const size_t len2[3] = { SHA1_MAC_LEN, 8, sizeof(magic2) };

	addr1[0] = password_hash_hash;
	addr1[1] = nt_response;
	addr1[2] = magic1;

	addr2[0] = response;
	addr2[1] = challenge;
	addr2[2] = magic2;

	nt_password_hash(password, password_len, password_hash);
	hash_nt_password_hash(password_hash, password_hash_hash);
	sha1_vector(3, addr1, len1, response);

	challenge_hash(peer_challenge, auth_challenge, username, username_len,
		       challenge);
	sha1_vector(3, addr2, len2, response);
}


/**
 * nt_challenge_response - NtChallengeResponse() - RFC 2433, Sect. A.5
 * @challenge: 8-octet Challenge (IN)
 * @password: 0-to-256-unicode-char Password (IN)
 * @password_len: Length of password
 * @response: 24-octet Response (OUT)
 */
void nt_challenge_response(const u8 *challenge, const u8 *password,
			   size_t password_len, u8 *response)
{
	u8 password_hash[16];
	nt_password_hash(password, password_len, password_hash);
	challenge_response(challenge, password_hash, response);
}


/**
 * get_master_key - GetMasterKey() - RFC 3079, Sect. 3.4
 * @password_hash_hash: 16-octet PasswordHashHash (IN)
 * @nt_response: 24-octet NTResponse (IN)
 * @master_key: 16-octet MasterKey (OUT)
 */
void get_master_key(const u8 *password_hash_hash, const u8 *nt_response,
		    u8 *master_key)
{
	static const u8 magic1[27] = {
		0x54, 0x68, 0x69, 0x73, 0x20, 0x69, 0x73, 0x20, 0x74,
		0x68, 0x65, 0x20, 0x4d, 0x50, 0x50, 0x45, 0x20, 0x4d,
		0x61, 0x73, 0x74, 0x65, 0x72, 0x20, 0x4b, 0x65, 0x79
	};
	const unsigned char *addr[3];
	const size_t len[3] = { 16, 24, sizeof(magic1) };
	u8 hash[SHA1_MAC_LEN];

	addr[0] = password_hash_hash;
	addr[1] = nt_response;
	addr[2] = magic1;

	sha1_vector(3, addr, len, hash);
	memcpy(master_key, hash, 16);
}


/**
 * get_asymetric_start_key - GetAsymetricStartKey() - RFC 3079, Sect. 3.4
 * @master_key: 16-octet MasterKey (IN)
 * @session_key: 8-to-16 octet SessionKey (OUT)
 * @session_key_len: SessionKeyLength (Length of session_key)
 * @is_send: IsSend (IN, BOOLEAN)
 * @is_server: IsServer (IN, BOOLEAN)
 */
void get_asymetric_start_key(const u8 *master_key, u8 *session_key,
			     size_t session_key_len, int is_send,
			     int is_server)
{
	static const u8 magic2[84] = {
		0x4f, 0x6e, 0x20, 0x74, 0x68, 0x65, 0x20, 0x63, 0x6c, 0x69,
		0x65, 0x6e, 0x74, 0x20, 0x73, 0x69, 0x64, 0x65, 0x2c, 0x20,
		0x74, 0x68, 0x69, 0x73, 0x20, 0x69, 0x73, 0x20, 0x74, 0x68,
		0x65, 0x20, 0x73, 0x65, 0x6e, 0x64, 0x20, 0x6b, 0x65, 0x79,
		0x3b, 0x20, 0x6f, 0x6e, 0x20, 0x74, 0x68, 0x65, 0x20, 0x73,
		0x65, 0x72, 0x76, 0x65, 0x72, 0x20, 0x73, 0x69, 0x64, 0x65,
		0x2c, 0x20, 0x69, 0x74, 0x20, 0x69, 0x73, 0x20, 0x74, 0x68,
		0x65, 0x20, 0x72, 0x65, 0x63, 0x65, 0x69, 0x76, 0x65, 0x20,
		0x6b, 0x65, 0x79, 0x2e
	};
	static const u8 magic3[84] = {
		0x4f, 0x6e, 0x20, 0x74, 0x68, 0x65, 0x20, 0x63, 0x6c, 0x69,
		0x65, 0x6e, 0x74, 0x20, 0x73, 0x69, 0x64, 0x65, 0x2c, 0x20,
		0x74, 0x68, 0x69, 0x73, 0x20, 0x69, 0x73, 0x20, 0x74, 0x68,
		0x65, 0x20, 0x72, 0x65, 0x63, 0x65, 0x69, 0x76, 0x65, 0x20,
		0x6b, 0x65, 0x79, 0x3b, 0x20, 0x6f, 0x6e, 0x20, 0x74, 0x68,
		0x65, 0x20, 0x73, 0x65, 0x72, 0x76, 0x65, 0x72, 0x20, 0x73,
		0x69, 0x64, 0x65, 0x2c, 0x20, 0x69, 0x74, 0x20, 0x69, 0x73,
		0x20, 0x74, 0x68, 0x65, 0x20, 0x73, 0x65, 0x6e, 0x64, 0x20,
		0x6b, 0x65, 0x79, 0x2e
	};
	static const u8 shs_pad1[40] = {
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};

	static const u8 shs_pad2[40] = {
		0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2,
		0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2,
		0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2,
		0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2
	};
	u8 digest[SHA1_MAC_LEN];
	const unsigned char *addr[4];
	const size_t len[4] = { 16, 40, 84, 40 };

	addr[0] = master_key;
	addr[1] = shs_pad1;
	if (is_send) {
		addr[2] = is_server ? magic3 : magic2;
	} else {
		addr[2] = is_server ? magic2 : magic3;
	}
	addr[3] = shs_pad2;

	sha1_vector(4, addr, len, digest);

	if (session_key_len > SHA1_MAC_LEN)
		session_key_len = SHA1_MAC_LEN;
	memcpy(session_key, digest, session_key_len);
}


#define PWBLOCK_LEN 516

/**
 * encrypt_pw_block_with_password_hash - EncryptPwBlobkWithPasswordHash() - RFC 2759, Sect. 8.10
 * @password: 0-to-256-unicode-char Password (IN)
 * @password_len: Length of password
 * @password_hash: 16-octet PasswordHash (IN)
 * @pw_block: 516-byte PwBlock (OUT)
 */
static void encrypt_pw_block_with_password_hash(
	const u8 *password, size_t password_len,
	const u8 *password_hash, u8 *pw_block)
{
	size_t i, offset;
	u8 *pos;

	if (password_len > 256)
		return;

	memset(pw_block, 0, PWBLOCK_LEN);
	offset = (256 - password_len) * 2;
	for (i = 0; i < password_len; i++)
		pw_block[offset + i * 2] = password[i];
	pos = &pw_block[2 * 256];
	WPA_PUT_LE16(pos, password_len * 2);
	rc4(pw_block, PWBLOCK_LEN, password_hash, 16);
}


/**
 * new_password_encrypted_with_old_nt_password_hash - NewPasswordEncryptedWithOldNtPasswordHash() - RFC 2759, Sect. 8.9
 * @new_password: 0-to-256-unicode-char NewPassword (IN)
 * @new_password_len: Length of new_password
 * @old_password: 0-to-256-unicode-char OldPassword (IN)
 * @old_password_len: Length of old_password
 * @encrypted_pw_block: 516-octet EncryptedPwBlock (OUT)
 */
void new_password_encrypted_with_old_nt_password_hash(
	const u8 *new_password, size_t new_password_len,
	const u8 *old_password, size_t old_password_len,
	u8 *encrypted_pw_block)
{
	u8 password_hash[16];

	nt_password_hash(old_password, old_password_len, password_hash);
	encrypt_pw_block_with_password_hash(new_password, new_password_len,
					    password_hash, encrypted_pw_block);
}


/**
 * nt_password_hash_encrypted_with_block - NtPasswordHashEncryptedWithBlock() - RFC 2759, Sect 8.13
 * @password_hash: 16-octer PasswordHash (IN)
 * @block: 16-octet Block (IN)
 * @cypher: 16-octer Cypher (OUT)
 */
static void nt_password_hash_encrypted_with_block(const u8 *password_hash,
						  const u8 *block,
						  u8 *cypher)
{
	des_encrypt(password_hash, block, cypher);
	des_encrypt(password_hash + 8, block + 7, cypher + 8);
}


/**
 * old_nt_password_hash_encrypted_with_new_nt_password_hash - OldNtPasswordHashEncryptedWithNewNtPasswordHash() - RFC 2759, Sect. 8.12
 * @new_password: 0-to-256-unicode-char NewPassword (IN)
 * @new_password_len: Length of new_password
 * @old_password: 0-to-256-unicode-char OldPassword (IN)
 * @old_password_len: Length of old_password
 * @encrypted_password_ash: 16-octet EncryptedPasswordHash (OUT)
 */
void old_nt_password_hash_encrypted_with_new_nt_password_hash(
	const u8 *new_password, size_t new_password_len,
	const u8 *old_password, size_t old_password_len,
	u8 *encrypted_password_hash)
{
	u8 old_password_hash[16], new_password_hash[16];

	nt_password_hash(old_password, old_password_len, old_password_hash);
	nt_password_hash(new_password, new_password_len, new_password_hash);
	nt_password_hash_encrypted_with_block(old_password_hash,
					      new_password_hash,
					      encrypted_password_hash);
}


#ifdef TEST_MAIN_MS_FUNCS

#include "rc4.c"

int main(int argc, char *argv[])
{
	/* Test vector from RFC2759 example */
	u8 *username = "User";
	u8 *password = "clientPass";
	u8 auth_challenge[] = {
		0x5B, 0x5D, 0x7C, 0x7D, 0x7B, 0x3F, 0x2F, 0x3E,
		0x3C, 0x2C, 0x60, 0x21, 0x32, 0x26, 0x26, 0x28
	};
	u8 peer_challenge[] = {
		0x21, 0x40, 0x23, 0x24, 0x25, 0x5E, 0x26, 0x2A,
		0x28, 0x29, 0x5F, 0x2B, 0x3A, 0x33, 0x7C, 0x7E
	};
	u8 challenge[] = { 0xD0, 0x2E, 0x43, 0x86, 0xBC, 0xE9, 0x12, 0x26 };
	u8 password_hash[] = {
		0x44, 0xEB, 0xBA, 0x8D, 0x53, 0x12, 0xB8, 0xD6,
		0x11, 0x47, 0x44, 0x11, 0xF5, 0x69, 0x89, 0xAE
	};
	u8 nt_response[] = {
		0x82, 0x30, 0x9E, 0xCD, 0x8D, 0x70, 0x8B, 0x5E,
		0xA0, 0x8F, 0xAA, 0x39, 0x81, 0xCD, 0x83, 0x54,
		0x42, 0x33, 0x11, 0x4A, 0x3D, 0x85, 0xD6, 0xDF
	};
	u8 password_hash_hash[] = {
		0x41, 0xC0, 0x0C, 0x58, 0x4B, 0xD2, 0xD9, 0x1C,
		0x40, 0x17, 0xA2, 0xA1, 0x2F, 0xA5, 0x9F, 0x3F
	};
	u8 authenticator_response[] = {
		0x40, 0x7A, 0x55, 0x89, 0x11, 0x5F, 0xD0, 0xD6,
		0x20, 0x9F, 0x51, 0x0F, 0xE9, 0xC0, 0x45, 0x66,
		0x93, 0x2C, 0xDA, 0x56
	};
	u8 master_key[] = {
		0xFD, 0xEC, 0xE3, 0x71, 0x7A, 0x8C, 0x83, 0x8C,
		0xB3, 0x88, 0xE5, 0x27, 0xAE, 0x3C, 0xDD, 0x31
	};
	u8 send_start_key[] = {
		0x8B, 0x7C, 0xDC, 0x14, 0x9B, 0x99, 0x3A, 0x1B,
		0xA1, 0x18, 0xCB, 0x15, 0x3F, 0x56, 0xDC, 0xCB
	};
	u8 buf[32];

	int errors = 0;

	printf("Testing ms_funcs.c\n");

	challenge_hash(peer_challenge, auth_challenge,
		       username, strlen(username),
		       buf);
	if (memcmp(challenge, buf, sizeof(challenge)) != 0) {
		printf("challenge_hash failed\n");
		errors++;
	}

	nt_password_hash(password, strlen(password), buf);
	if (memcmp(password_hash, buf, sizeof(password_hash)) != 0) {
		printf("nt_password_hash failed\n");
		errors++;
	}

	generate_nt_response(auth_challenge, peer_challenge,
			     username, strlen(username),
			     password, strlen(password),
			     buf);
	if (memcmp(nt_response, buf, sizeof(nt_response)) != 0) {
		printf("generate_nt_response failed\n");
		errors++;
	}

	hash_nt_password_hash(password_hash, buf);
	if (memcmp(password_hash_hash, buf, sizeof(password_hash_hash)) != 0) {
		printf("hash_nt_password_hash failed\n");
		errors++;
	}

	generate_authenticator_response(password, strlen(password),
					peer_challenge, auth_challenge,
					username, strlen(username),
					nt_response, buf);
	if (memcmp(authenticator_response, buf, sizeof(authenticator_response))
	    != 0) {
		printf("generate_authenticator_response failed\n");
		errors++;
	}

	get_master_key(password_hash_hash, nt_response, buf);
	if (memcmp(master_key, buf, sizeof(master_key)) != 0) {
		printf("get_master_key failed\n");
		errors++;
	}

	get_asymetric_start_key(master_key, buf, sizeof(send_start_key), 1, 1);
	if (memcmp(send_start_key, buf, sizeof(send_start_key)) != 0) {
		printf("get_asymetric_start_key failed\n");
		errors++;
	}

	if (errors)
		printf("FAILED! %d errors\n", errors);

	return errors;
}
#endif /* TEST_MAIN_MS_FUNCS */
