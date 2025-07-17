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
#include <stdio.h>
#include <unistd.h>

#include <libder.h>

#include "fuzzers.h"

struct supply_data {
	const uint8_t	*data;
	volatile size_t	 datasz;
	int		 socket;
};

static void *
supply_thread(void *data)
{
	struct supply_data *sdata = data;
	size_t sz = sdata->datasz;
	ssize_t writesz;

	do {
		writesz = write(sdata->socket, sdata->data, sz);

		data += writesz;
		sz -= writesz;
	} while (sz != 0 && writesz > 0);

	sdata->datasz = sz;
	shutdown(sdata->socket, SHUT_RDWR);
	close(sdata->socket);

	return (NULL);
}

static int
fuzz_fd(const struct fuzz_params *fparams, const uint8_t *data, size_t sz)
{
	struct supply_data sdata;
	struct libder_ctx *ctx;
	struct libder_object *obj;
	size_t totalsz;
	int sockets[2];
	pid_t pid;
	int ret;

	ret = socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0,
	    &sockets[0]);
	if (ret == -1)
		return (-1);

	sdata.data = data;
	sdata.datasz = sz;
	sdata.socket = sockets[1];
	signal(SIGCHLD, SIG_IGN);
	pid = fork();
	if (pid == -1) {
		close(sockets[0]);
		close(sockets[1]);
		return (-1);
	}

	if (pid == 0) {
		close(sockets[0]);
		supply_thread(&sdata);
		_exit(0);
	} else {
		close(sockets[1]);
	}

	totalsz = 0;
	ret = 0;
	ctx = libder_open();
	libder_set_strict(ctx, !!fparams->strict);
	while (totalsz < sz) {
		size_t readsz = 0;

		obj = libder_read_fd(ctx, sockets[0], &readsz);
		libder_obj_free(obj);

		/*
		 * Even invalid reads should consume at least one byte.
		 */
		assert(readsz != 0);

		totalsz += readsz;
		if (readsz == 0)
			break;
	}

	assert(totalsz == sz);
	libder_close(ctx);
	close(sockets[0]);

	return (ret);
}

static int
fuzz_file(const struct fuzz_params *fparams, const uint8_t *data, size_t sz)
{
	FILE *fp;
	struct libder_ctx *ctx;
	struct libder_object *obj;
	size_t totalsz;
	int ret;

	if (fparams->buftype >= BUFFER_END)
		return (-1);

	fp = fmemopen(__DECONST(void *, data), sz, "rb");
	assert(fp != NULL);

	switch (fparams->buftype) {
	case BUFFER_NONE:
		setvbuf(fp, NULL, 0, _IONBF);
		break;
	case BUFFER_FULL:
		setvbuf(fp, NULL, 0, _IOFBF);
		break;
	case BUFFER_END:
		assert(0);
	}

	totalsz = 0;
	ret = 0;
	ctx = libder_open();
	libder_set_strict(ctx, !!fparams->strict);
	while (!feof(fp)) {
		size_t readsz = 0;

		obj = libder_read_file(ctx, fp, &readsz);
		libder_obj_free(obj);

		if (obj == NULL)
			assert(readsz != 0 || feof(fp));
		else
			assert(readsz != 0);

		totalsz += readsz;
	}

	assert(totalsz == sz);
	libder_close(ctx);
	fclose(fp);

	return (ret);
}

static int
fuzz_plain(const struct fuzz_params *fparams, const uint8_t *data, size_t sz)
{
	struct libder_ctx *ctx;
	struct libder_object *obj;
	int ret;

	if (sz == 0)
		return (-1);

	ret = 0;
	ctx = libder_open();
	libder_set_strict(ctx, !!fparams->strict);
	do {
		size_t readsz;

		readsz = sz;
		obj = libder_read(ctx, data, &readsz);
		libder_obj_free(obj);

		if (obj == NULL)
			assert(readsz != 0 || readsz == sz);
		else
			assert(readsz != 0);

		/*
		 * If we hit an entirely invalid segment of the buffer, we'll
		 * just skip a byte and try again.
		 */
		data += MAX(1, readsz);
		sz -= MAX(1, readsz);
	} while (sz != 0);

	libder_close(ctx);

	return (ret);
};

static bool
validate_padding(const struct fuzz_params *fparams)
{
	const uint8_t *end = (const void *)(fparams + 1);
	const uint8_t *pad = (const uint8_t *)&fparams->PARAM_PAD_START;

	while (pad < end) {
		if (*pad++ != 0)
			return (false);
	}

	return (true);
}

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t sz)
{
	const struct fuzz_params *fparams;

	if (sz <= sizeof(*fparams))
		return (-1);

	fparams = (const void *)data;
	if (fparams->type >= STREAM_END)
		return (-1);

	if (!validate_padding(fparams))
		return (-1);

	data += sizeof(*fparams);
	sz -= sizeof(*fparams);

	if (fparams->type != STREAM_FILE && fparams->buftype != BUFFER_NONE)
		return (-1);

	switch (fparams->type) {
	case STREAM_FD:
		return (fuzz_fd(fparams, data, sz));
	case STREAM_FILE:
		return (fuzz_file(fparams, data, sz));
	case STREAM_PLAIN:
		return (fuzz_plain(fparams, data, sz));
	case STREAM_END:
		assert(0);
	}

	__builtin_trap();
}
