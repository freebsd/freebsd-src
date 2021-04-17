/*
 * Testing tool for X.509v3 routines
 * Copyright (c) 2006-2019, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"

#include "common.h"
#include "tls/x509v3.h"
#include "../fuzzer-common.h"


int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	struct x509_certificate *cert;

	wpa_fuzzer_set_debug_level();

	cert = x509_certificate_parse(data, size);
	x509_certificate_free(cert);
	return 0;
}
