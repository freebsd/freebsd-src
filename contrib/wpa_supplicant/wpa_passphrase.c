/*
 * WPA Supplicant - ASCII passphrase to WPA PSK tool
 * Copyright (c) 2003-2004, Jouni Malinen <jkmaline@cc.hut.fi>
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

#include <stdio.h>
#include <string.h>

#include "common.h"
#include "sha1.h"


int main(int argc, char *argv[])
{
	unsigned char psk[32];
	int i;
	char *ssid, *passphrase;

	if (argc != 3) {
		printf("usage: wpa_passphrase <ssid> <passphrase>\n");
		return 1;
	}

	ssid = argv[1];
	passphrase = argv[2];

	if (strlen(passphrase) < 8 || strlen(passphrase) > 63) {
		printf("Passphrase must be 8..63 characters\n");
		return 1;
	}

	pbkdf2_sha1(passphrase, ssid, strlen(ssid), 4096, psk, 32);

	printf("network={\n");
	printf("\tssid=\"%s\"\n", ssid);
	printf("\t#psk=\"%s\"\n", passphrase);
	printf("\tpsk=");
	for (i = 0; i < 32; i++)
		printf("%02x", psk[i]);
	printf("\n");
	printf("}\n");

	return 0;
}
