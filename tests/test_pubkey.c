/*-
 * Copyright (c) 2024 Kyle Evans <kevans@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/stat.h>

#include <assert.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libder.h>

#include "test_common.h"

static const uint8_t oid_ecpubkey[] =
    { 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01 };
static const uint8_t oid_secp256k1[] =
    { 0x2b, 0x81, 0x04, 0x00, 0x0a };

static const uint8_t pubdata[] = { 0x00, 0x04, 0xd1, 0x76, 0x20, 0x39, 0xe5, 0x3e,
    0x67, 0x7d, 0x8d, 0xfd, 0xc4, 0x21, 0x20, 0xcd, 0xb0, 0xbf, 0x47, 0x87, 0x6a,
    0xf8, 0x07, 0x73, 0xbe, 0xbe, 0xd5, 0xbb, 0x3c, 0xbc, 0x32, 0x93, 0xd9, 0xdf,
    0x96, 0x25, 0xb7, 0x0e, 0x3c, 0x55, 0x12, 0xee, 0x7a, 0x02, 0x39, 0x0f, 0xee,
    0x7b, 0xfe, 0x1a, 0x93, 0x76, 0xf7, 0xc2, 0xac, 0x05, 0xba, 0x9a, 0x83, 0x37,
    0xf5, 0xcd, 0x55, 0x57, 0x39, 0x6f };

static void
test_interface(struct libder_object *root)
{
	const uint8_t *data;
	size_t datasz;
	struct libder_object *keystring;

	keystring = libder_obj_child(root, 1);
	assert(keystring != NULL);
	assert(libder_obj_type_simple(keystring) == BT_BITSTRING);

	data = libder_obj_data(keystring, &datasz);
	assert(datasz == sizeof(pubdata));
	assert(memcmp(pubdata, data, datasz) == 0);
}

/* buf and bufszs are just our reference */
static void
test_construction(struct libder_ctx*ctx, const uint8_t *buf, size_t bufsz)
{
	uint8_t *out;
	struct libder_object *obj, *params, *root;
	struct libder_object *keystring;
	size_t outsz;

	root = libder_obj_alloc_simple(ctx, BT_SEQUENCE, NULL, 0);
	assert(root != NULL);

	params = libder_obj_alloc_simple(ctx, BT_SEQUENCE, NULL, 0);
	assert(params != NULL);
	assert(libder_obj_append(root, params));

	keystring = libder_obj_alloc_simple(ctx, BT_BITSTRING, pubdata, sizeof(pubdata));
	assert(keystring != NULL);
	assert(libder_obj_append(root, keystring));

	/* Now go back and build the two params, id and curve */
	obj = libder_obj_alloc_simple(ctx, BT_OID, oid_ecpubkey, sizeof(oid_ecpubkey));
	assert(obj != NULL);
	assert(libder_obj_append(params, obj));

	obj = libder_obj_alloc_simple(ctx, BT_OID, oid_secp256k1, sizeof(oid_secp256k1));
	assert(obj != NULL);
	assert(libder_obj_append(params, obj));

	out = NULL;
	outsz = 0;
	out = libder_write(ctx, root, out, &outsz);
	assert(out != NULL);
	assert(outsz == bufsz);
	assert(memcmp(out, buf, bufsz) == 0);

	libder_obj_free(root);
	free(out);
}

int
main(int argc, char *argv[])
{
	struct stat sb;
	struct libder_ctx *ctx;
	struct libder_object *root;
	uint8_t *buf, *out;
	size_t bufsz, outsz, rootsz;
	ssize_t readsz;
	int dfd, error, fd;

	dfd = open_progdir(argv[0]);

	fd = openat(dfd, "repo.pub", O_RDONLY);
	assert(fd >= 0);

	close(dfd);
	dfd = -1;

	error = fstat(fd, &sb);
	assert(error == 0);

	bufsz = sb.st_size;
	buf = malloc(bufsz);
	assert(buf != NULL);

	readsz = read(fd, buf, bufsz);
	close(fd);

	assert(readsz == bufsz);

	ctx = libder_open();
	rootsz = bufsz;
	libder_set_verbose(ctx, 2);
	root = libder_read(ctx, buf, &rootsz);

	assert(root != NULL);
	assert(rootsz == bufsz);

	test_interface(root);
	test_construction(ctx, buf, bufsz);

	outsz = 0;
	out = NULL;
	out = libder_write(ctx, root, out, &outsz);
	assert(out != NULL);
	assert(outsz == bufsz);

	assert(memcmp(buf, out, outsz) == 0);

	free(out);
	free(buf);
	libder_obj_free(root);
	libder_close(ctx);
}
