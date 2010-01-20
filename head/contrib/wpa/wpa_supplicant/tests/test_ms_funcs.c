/*
 * Test program for ms_funcs
 * Copyright (c) 2003-2006, Jouni Malinen <j@w1.fi>
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

#include "ms_funcs.c"


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
