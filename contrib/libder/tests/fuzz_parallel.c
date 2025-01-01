/*-
 * Copyright (c) 2024 Kyle Evans <kevans@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/param.h>
#include <sys/socket.h>

#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <libder.h>

#include "fuzzers.h"

struct fuzz_frame {
	uint8_t		frame_threads;
};

struct thread_input {
	const uint8_t	*data;
	size_t		 datasz;
};

static void *
thread_main(void *cookie)
{
	const struct thread_input *input = cookie;
	struct libder_ctx *ctx;
	struct libder_object *obj;
	const uint8_t *data = input->data;
	size_t readsz, sz = input->datasz;

	ctx = libder_open();
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

out:
	libder_obj_free(obj);
	libder_close(ctx);
	return (NULL);
}

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t sz)
{
	const struct fuzz_frame *frame;
	pthread_t *threads;
	struct thread_input inp;
	size_t nthreads;

	if (sz <= sizeof(*frame))
		return (-1);

	frame = (const void *)data;
	data += sizeof(*frame);
	sz -= sizeof(*frame);

	if (frame->frame_threads < 2)
		return (-1);

	threads = malloc(sizeof(*threads) * frame->frame_threads);
	if (threads == NULL)
		return (-1);

	inp.data = data;
	inp.datasz = sz;

	for (nthreads = 0; nthreads < frame->frame_threads; nthreads++) {
		if (pthread_create(&threads[nthreads], NULL, thread_main,
		    &inp) != 0)
			break;
	}

	for (uint8_t i = 0; i < nthreads; i++)
		pthread_join(threads[i], NULL);

	free(threads);

	return (0);
}
