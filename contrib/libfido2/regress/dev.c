/*
 * Copyright (c) 2019-2024 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#undef NDEBUG

#include <assert.h>
#include <string.h>
#include <time.h>

#define _FIDO_INTERNAL

#include <fido.h>

#include "../fuzz/wiredata_fido2.h"
#include "extern.h"

/* gh#56 */
static void
open_iff_ok(void)
{
	fido_dev_t	*dev = NULL;

	assert((dev = fido_dev_new()) != NULL);
	setup_dummy_io(dev);
	assert(fido_dev_open(dev, "dummy") == FIDO_ERR_RX);
	assert(fido_dev_close(dev) == FIDO_ERR_INVALID_ARGUMENT);

	fido_dev_free(&dev);
}

static void
reopen(void)
{
	const uint8_t	 cbor_info_data[] = { WIREDATA_CTAP_CBOR_INFO };
	uint8_t		*wiredata;
	fido_dev_t	*dev = NULL;

	wiredata = wiredata_setup(cbor_info_data, sizeof(cbor_info_data));
	assert((dev = fido_dev_new()) != NULL);
	setup_dummy_io(dev);
	assert(fido_dev_open(dev, "dummy") == FIDO_OK);
	assert(fido_dev_close(dev) == FIDO_OK);
	wiredata_clear(&wiredata);

	wiredata = wiredata_setup(cbor_info_data, sizeof(cbor_info_data));
	assert(fido_dev_open(dev, "dummy") == FIDO_OK);
	assert(fido_dev_close(dev) == FIDO_OK);
	fido_dev_free(&dev);
	wiredata_clear(&wiredata);
}

static void
double_open(void)
{
	const uint8_t	 cbor_info_data[] = { WIREDATA_CTAP_CBOR_INFO };
	uint8_t		*wiredata;
	fido_dev_t	*dev = NULL;

	wiredata = wiredata_setup(cbor_info_data, sizeof(cbor_info_data));
	assert((dev = fido_dev_new()) != NULL);
	setup_dummy_io(dev);
	assert(fido_dev_open(dev, "dummy") == FIDO_OK);
	assert(fido_dev_open(dev, "dummy") == FIDO_ERR_INVALID_ARGUMENT);
	assert(fido_dev_close(dev) == FIDO_OK);
	fido_dev_free(&dev);
	wiredata_clear(&wiredata);
}

static void
double_close(void)
{
	const uint8_t	 cbor_info_data[] = { WIREDATA_CTAP_CBOR_INFO };
	uint8_t		*wiredata;
	fido_dev_t	*dev = NULL;

	wiredata = wiredata_setup(cbor_info_data, sizeof(cbor_info_data));
	assert((dev = fido_dev_new()) != NULL);
	assert(fido_dev_close(dev) == FIDO_ERR_INVALID_ARGUMENT);
	setup_dummy_io(dev);
	assert(fido_dev_close(dev) == FIDO_ERR_INVALID_ARGUMENT);
	assert(fido_dev_open(dev, "dummy") == FIDO_OK);
	assert(fido_dev_close(dev) == FIDO_OK);
	assert(fido_dev_close(dev) == FIDO_ERR_INVALID_ARGUMENT);
	fido_dev_free(&dev);
	wiredata_clear(&wiredata);
}

static void
is_fido2(void)
{
	const uint8_t	 cbor_info_data[] = { WIREDATA_CTAP_CBOR_INFO };
	uint8_t		*wiredata;
	fido_dev_t	*dev = NULL;

	wiredata = wiredata_setup(cbor_info_data, sizeof(cbor_info_data));
	assert((dev = fido_dev_new()) != NULL);
	setup_dummy_io(dev);
	assert(fido_dev_open(dev, "dummy") == FIDO_OK);
	assert(fido_dev_is_fido2(dev) == true);
	assert(fido_dev_supports_pin(dev) == true);
	fido_dev_force_u2f(dev);
	assert(fido_dev_is_fido2(dev) == false);
	assert(fido_dev_supports_pin(dev) == false);
	assert(fido_dev_close(dev) == FIDO_OK);
	wiredata_clear(&wiredata);

	wiredata = wiredata_setup(NULL, 0);
	assert(fido_dev_open(dev, "dummy") == FIDO_OK);
	assert(fido_dev_is_fido2(dev) == false);
	assert(fido_dev_supports_pin(dev) == false);
	fido_dev_force_fido2(dev);
	assert(fido_dev_is_fido2(dev) == true);
	assert(fido_dev_supports_pin(dev) == false);
	assert(fido_dev_close(dev) == FIDO_OK);
	fido_dev_free(&dev);
	wiredata_clear(&wiredata);
}

static void
has_pin(void)
{
	const uint8_t	 set_pin_data[] = {
			    WIREDATA_CTAP_CBOR_INFO,
			    WIREDATA_CTAP_CBOR_AUTHKEY,
			    WIREDATA_CTAP_CBOR_STATUS,
			    WIREDATA_CTAP_CBOR_STATUS
			 };
	uint8_t		*wiredata;
	fido_dev_t	*dev = NULL;

	wiredata = wiredata_setup(set_pin_data, sizeof(set_pin_data));
	assert((dev = fido_dev_new()) != NULL);
	setup_dummy_io(dev);
	assert(fido_dev_open(dev, "dummy") == FIDO_OK);
	assert(fido_dev_has_pin(dev) == false);
	assert(fido_dev_set_pin(dev, "top secret", NULL) == FIDO_OK);
	assert(fido_dev_has_pin(dev) == true);
	assert(fido_dev_reset(dev) == FIDO_OK);
	assert(fido_dev_has_pin(dev) == false);
	assert(fido_dev_close(dev) == FIDO_OK);
	fido_dev_free(&dev);
	wiredata_clear(&wiredata);
}

static void
timeout_rx(void)
{
	const uint8_t	 timeout_rx_data[] = {
			    WIREDATA_CTAP_CBOR_INFO,
			    WIREDATA_CTAP_KEEPALIVE,
			    WIREDATA_CTAP_KEEPALIVE,
			    WIREDATA_CTAP_KEEPALIVE,
			    WIREDATA_CTAP_KEEPALIVE,
			    WIREDATA_CTAP_KEEPALIVE,
			    WIREDATA_CTAP_CBOR_STATUS
			 };
	uint8_t		*wiredata;
	fido_dev_t	*dev = NULL;

	wiredata = wiredata_setup(timeout_rx_data, sizeof(timeout_rx_data));
	assert((dev = fido_dev_new()) != NULL);
	setup_dummy_io(dev);
	assert(fido_dev_open(dev, "dummy") == FIDO_OK);
	assert(fido_dev_set_timeout(dev, 3 * 1000) == FIDO_OK);
	set_read_interval(1000);
	assert(fido_dev_reset(dev) == FIDO_ERR_RX);
	assert(fido_dev_close(dev) == FIDO_OK);
	fido_dev_free(&dev);
	wiredata_clear(&wiredata);
	set_read_interval(0);
}

static void
timeout_ok(void)
{
	const uint8_t	 timeout_ok_data[] = {
			    WIREDATA_CTAP_CBOR_INFO,
			    WIREDATA_CTAP_KEEPALIVE,
			    WIREDATA_CTAP_KEEPALIVE,
			    WIREDATA_CTAP_KEEPALIVE,
			    WIREDATA_CTAP_KEEPALIVE,
			    WIREDATA_CTAP_KEEPALIVE,
			    WIREDATA_CTAP_CBOR_STATUS
			 };
	uint8_t		*wiredata;
	fido_dev_t	*dev = NULL;

	wiredata = wiredata_setup(timeout_ok_data, sizeof(timeout_ok_data));
	assert((dev = fido_dev_new()) != NULL);
	setup_dummy_io(dev);
	assert(fido_dev_open(dev, "dummy") == FIDO_OK);
	assert(fido_dev_set_timeout(dev, 30 * 1000) == FIDO_OK);
	set_read_interval(1000);
	assert(fido_dev_reset(dev) == FIDO_OK);
	assert(fido_dev_close(dev) == FIDO_OK);
	fido_dev_free(&dev);
	wiredata_clear(&wiredata);
	set_read_interval(0);
}

static void
timeout_misc(void)
{
	fido_dev_t *dev;

	assert((dev = fido_dev_new()) != NULL);
	assert(fido_dev_set_timeout(dev, -2) == FIDO_ERR_INVALID_ARGUMENT);
	assert(fido_dev_set_timeout(dev, 3 * 1000) == FIDO_OK);
	assert(fido_dev_set_timeout(dev, -1) == FIDO_OK);
	fido_dev_free(&dev);
}

int
main(void)
{
	fido_init(0);

	open_iff_ok();
	reopen();
	double_open();
	double_close();
	is_fido2();
	has_pin();
	timeout_rx();
	timeout_ok();
	timeout_misc();

	exit(0);
}
