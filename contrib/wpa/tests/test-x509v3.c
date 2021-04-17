/*
 * Testing tool for X.509v3 routines
 * Copyright (c) 2006-2007, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"

#include "common.h"
#include "tls/asn1.h"
#include "tls/x509v3.h"


int main(int argc, char *argv[])
{
	char *buf;
	size_t len;
	struct x509_certificate *certs = NULL, *last = NULL, *cert;
	int i, reason;

	wpa_debug_level = 0;
	wpa_debug_show_keys = 1;

	if (argc < 3 || strcmp(argv[1], "-v") != 0) {
		printf("usage: test_x509v3 -v <cert1.der> <cert2.der> ..\n");
		return -1;
	}

	for (i = 2; i < argc; i++) {
		printf("Reading: %s\n", argv[i]);
		buf = os_readfile(argv[i], &len);
		if (buf == NULL) {
			printf("Failed to read '%s'\n", argv[i]);
			return -1;
		}

		cert = x509_certificate_parse((u8 *) buf, len);
		if (cert == NULL) {
			printf("Failed to parse X.509 certificate\n");
			return -1;
		}

		free(buf);

		if (certs == NULL)
			certs = cert;
		else
			last->next = cert;
		last = cert;
	}

	printf("\n\nValidating certificate chain\n");
	if (x509_certificate_chain_validate(last, certs, &reason, 0) < 0) {
		printf("\nCertificate chain validation failed: %d\n", reason);
		return -1;
	}
	printf("\nCertificate chain is valid\n");

	return 0;
}
