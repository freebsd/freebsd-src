/*-
 * Copyright (c) 2024 Kyle Evans <kevans@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/param.h>
#include <sys/socket.h>

#include <assert.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <libder.h>

#include "fuzzers.h"

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t sz)
{
	struct libder_ctx *ctx;
	struct libder_object *obj;
	size_t readsz;
	int ret;
	bool strict;

	if (sz < 2)
		return (-1);

	/*
	 * I worked this in originally by just using the high bit of the first
	 * byte, but then I realized that encoding it that way would make it
	 * impossible to get strict validation of universal and application
	 * tags.  The former is a bit more important than the latter.
	 */
	strict = !!data[0];
	data++;
	sz--;

	ctx = libder_open();
	libder_set_strict(ctx, strict);
	ret = -1;
	readsz = sz;
	obj = libder_read(ctx, data, &readsz);
	if (obj == NULL || readsz != sz)
		goto out;

	if (obj != NULL) {
		uint8_t *buf = NULL;
		size_t bufsz = 0;

		/*
		 * If we successfully read it, then it shouldn't
		 * overflow.  We're letting libder allocate the buffer,
		 * so we shouldn't be able to hit the 'too small' bit.
		 *
		 * I can't imagine what other errors might happen, so
		 * we'll just assert on it.
		 */
		buf = libder_write(ctx, obj, buf, &bufsz);
		if (buf == NULL)
			goto out;

		assert(bufsz != 0);

		free(buf);
	}

	ret = 0;

out:
	libder_obj_free(obj);
	libder_close(ctx);

	return (ret);
}
