/*
 * Copyright (c) 2024 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#undef NDEBUG

#include <assert.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

#define _FIDO_INTERNAL
#include <fido.h>

#include "extern.h"
#include "../fuzz/wiredata_fido2.h"

#define REPORT_LEN	(64 + 1)

static uint8_t	 ctap_nonce[8];
static uint8_t	*wiredata_ptr;
static size_t	 wiredata_len;
static int	 fake_dev_handle;
static int	 initialised;
static long	 interval_ms;

#if defined(_MSC_VER)
static int
nanosleep(const struct timespec *rqtp, struct timespec *rmtp)
{
	if (rmtp != NULL) {
		errno = EINVAL;
		return (-1);
	}

	Sleep((DWORD)(rqtp->tv_sec * 1000) + (DWORD)(rqtp->tv_nsec / 1000000));

	return (0);
}
#endif

static void *
dummy_open(const char *path)
{
	(void)path;

	return (&fake_dev_handle);
}

static void
dummy_close(void *handle)
{
	assert(handle == &fake_dev_handle);
}

static int
dummy_read(void *handle, unsigned char *ptr, size_t len, int ms)
{
	struct timespec tv;
	size_t		n;
	long		d;

	assert(handle == &fake_dev_handle);
	assert(ptr != NULL);
	assert(len == REPORT_LEN - 1);

	if (wiredata_ptr == NULL)
		return (-1);

	if (!initialised) {
		assert(wiredata_len >= REPORT_LEN - 1);
		memcpy(&wiredata_ptr[7], &ctap_nonce, sizeof(ctap_nonce));
		initialised = 1;
	}

	if (ms >= 0 && ms < interval_ms)
		d = ms;
	else
		d = interval_ms;

	if (d) {
		tv.tv_sec = d / 1000;
		tv.tv_nsec = (d % 1000) * 1000000;
		if (nanosleep(&tv, NULL) == -1)
			err(1, "nanosleep");
	}

	if (d != interval_ms)
		return (-1); /* timeout */

	if (wiredata_len < len)
		n = wiredata_len;
	else
		n = len;

	memcpy(ptr, wiredata_ptr, n);
	wiredata_ptr += n;
	wiredata_len -= n;

	return ((int)n);
}

static int
dummy_write(void *handle, const unsigned char *ptr, size_t len)
{
	struct timespec tv;

	assert(handle == &fake_dev_handle);
	assert(ptr != NULL);
	assert(len == REPORT_LEN);

	if (!initialised)
		memcpy(&ctap_nonce, &ptr[8], sizeof(ctap_nonce));

	if (interval_ms) {
		tv.tv_sec = interval_ms / 1000;
		tv.tv_nsec = (interval_ms % 1000) * 1000000;
		if (nanosleep(&tv, NULL) == -1)
			err(1, "nanosleep");
	}

	return ((int)len);
}

uint8_t *
wiredata_setup(const uint8_t *data, size_t len)
{
	const uint8_t ctap_init_data[] = { WIREDATA_CTAP_INIT };

	assert(wiredata_ptr == NULL);
	assert(SIZE_MAX - len > sizeof(ctap_init_data));
	assert((wiredata_ptr = malloc(sizeof(ctap_init_data) + len)) != NULL);

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable:6386)
#endif
	memcpy(wiredata_ptr, ctap_init_data, sizeof(ctap_init_data));
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

	if (len)
		memcpy(wiredata_ptr + sizeof(ctap_init_data), data, len);

	wiredata_len = sizeof(ctap_init_data) + len;

	return (wiredata_ptr);
}

void
wiredata_clear(uint8_t **wiredata)
{
	free(*wiredata);
	*wiredata = NULL;
	wiredata_ptr = NULL;
	wiredata_len = 0;
	initialised = 0;
}

void
setup_dummy_io(fido_dev_t *dev)
{
	fido_dev_io_t io;

	memset(&io, 0, sizeof(io));
	io.open = dummy_open;
	io.close = dummy_close;
	io.read = dummy_read;
	io.write = dummy_write;

	assert(fido_dev_set_io_functions(dev, &io) == FIDO_OK);
}

void
set_read_interval(long ms)
{
	interval_ms = ms;
}
