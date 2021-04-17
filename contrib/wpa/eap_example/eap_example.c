/*
 * Example application showing how EAP peer and server code from
 * wpa_supplicant/hostapd can be used as a library. This example program
 * initializes both an EAP server and an EAP peer entities and then runs
 * through an EAP-PEAP/MSCHAPv2 authentication.
 * Copyright (c) 2007, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"

#include "common.h"


int eap_example_peer_init(void);
void eap_example_peer_deinit(void);
int eap_example_peer_step(void);

int eap_example_server_init(void);
void eap_example_server_deinit(void);
int eap_example_server_step(void);


int main(int argc, char *argv[])
{
	int res_s, res_p;

	wpa_debug_level = 0;

	if (eap_example_peer_init() < 0 ||
	    eap_example_server_init() < 0)
		return -1;

	do {
		printf("---[ server ]--------------------------------\n");
		res_s = eap_example_server_step();
		printf("---[ peer ]----------------------------------\n");
		res_p = eap_example_peer_step();
	} while (res_s || res_p);

	eap_example_peer_deinit();
	eap_example_server_deinit();

	return 0;
}
