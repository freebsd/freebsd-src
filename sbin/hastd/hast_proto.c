/*-
 * Copyright (c) 2009-2010 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Pawel Jakub Dawidek under sponsorship from
 * the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/endian.h>

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <strings.h>

#ifdef HAVE_CRYPTO
#include <openssl/sha.h>
#endif

#include <hast.h>
#include <ebuf.h>
#include <nv.h>
#include <pjdlog.h>
#include <proto.h>

#include "hast_proto.h"

struct hast_main_header {
	/* Protocol version. */
	uint8_t		version;
	/* Size of nv headers. */
	uint32_t	size;
} __packed;

typedef int hps_send_t(struct hast_resource *, struct nv *nv, void **, size_t *, bool *);
typedef int hps_recv_t(struct hast_resource *, struct nv *nv, void **, size_t *, bool *);

struct hast_pipe_stage {
	const char	*hps_name;
	hps_send_t	*hps_send;
	hps_recv_t	*hps_recv;
};

static int compression_send(struct hast_resource *res, struct nv *nv,
    void **datap, size_t *sizep, bool *freedatap);
static int compression_recv(struct hast_resource *res, struct nv *nv,
    void **datap, size_t *sizep, bool *freedatap);
#ifdef HAVE_CRYPTO
static int checksum_send(struct hast_resource *res, struct nv *nv,
    void **datap, size_t *sizep, bool *freedatap);
static int checksum_recv(struct hast_resource *res, struct nv *nv,
    void **datap, size_t *sizep, bool *freedatap);
#endif

static struct hast_pipe_stage pipeline[] = {
	{ "compression", compression_send, compression_recv },
#ifdef HAVE_CRYPTO
	{ "checksum", checksum_send, checksum_recv }
#endif
};

static int
compression_send(struct hast_resource *res, struct nv *nv, void **datap,
    size_t *sizep, bool *freedatap)
{
	unsigned char *newbuf;

	res = res;	/* TODO */

	/*
	 * TODO: For now we emulate compression.
	 * At 80% probability we succeed to compress data, which means we
	 * allocate new buffer, copy the data over set *freedatap to true.
	 */

	if (arc4random_uniform(100) < 80) {
		uint32_t *origsize;

		/*
		 * Compression succeeded (but we will grow by 4 bytes, not
		 * shrink for now).
		 */
		newbuf = malloc(sizeof(uint32_t) + *sizep);
		if (newbuf == NULL)
			return (-1);
		origsize = (void *)newbuf;
		*origsize = htole32((uint32_t)*sizep);
		nv_add_string(nv, "null", "compression");
		if (nv_error(nv) != 0) {
			free(newbuf);
			errno = nv_error(nv);
			return (-1);
		}
		bcopy(*datap, newbuf + sizeof(uint32_t), *sizep);
		if (*freedatap)
			free(*datap);
		*freedatap = true;
		*datap = newbuf;
		*sizep = sizeof(uint32_t) + *sizep;
	} else {
		/*
		 * Compression failed, so we leave everything as it was.
		 * It is not critical for compression to succeed.
		 */
	}

	return (0);
}

static int
compression_recv(struct hast_resource *res, struct nv *nv, void **datap,
    size_t *sizep, bool *freedatap)
{
	unsigned char *newbuf;
	const char *algo;
	size_t origsize;

	res = res;	/* TODO */

	/*
	 * TODO: For now we emulate compression.
	 */

	algo = nv_get_string(nv, "compression");
	if (algo == NULL)
		return (0);	/* No compression. */
	if (strcmp(algo, "null") != 0) {
		pjdlog_error("Unknown compression algorithm '%s'.", algo);
		return (-1);	/* Unknown compression algorithm. */
	}

	origsize = le32toh(*(uint32_t *)*datap);
	newbuf = malloc(origsize);
	if (newbuf == NULL)
		return (-1);
	bcopy((unsigned char *)*datap + sizeof(uint32_t), newbuf, origsize);
	if (*freedatap)
		free(*datap);
	*freedatap = true;
	*datap = newbuf;
	*sizep = origsize;

	return (0);
}

#ifdef HAVE_CRYPTO
static int
checksum_send(struct hast_resource *res, struct nv *nv, void **datap,
    size_t *sizep, bool *freedatap __unused)
{
	unsigned char hash[SHA256_DIGEST_LENGTH];
	SHA256_CTX ctx;

	res = res;	/* TODO */

	SHA256_Init(&ctx);
	SHA256_Update(&ctx, *datap, *sizep);
	SHA256_Final(hash, &ctx);

	nv_add_string(nv, "sha256", "checksum");
	nv_add_uint8_array(nv, hash, sizeof(hash), "hash");

	return (0);
}

static int
checksum_recv(struct hast_resource *res, struct nv *nv, void **datap,
    size_t *sizep, bool *freedatap __unused)
{
	unsigned char chash[SHA256_DIGEST_LENGTH];
	const unsigned char *rhash;
	SHA256_CTX ctx;
	const char *algo;
	size_t size;

	res = res;	/* TODO */

	algo = nv_get_string(nv, "checksum");
	if (algo == NULL)
		return (0);	/* No checksum. */
	if (strcmp(algo, "sha256") != 0) {
		pjdlog_error("Unknown checksum algorithm '%s'.", algo);
		return (-1);	/* Unknown checksum algorithm. */
	}
	rhash = nv_get_uint8_array(nv, &size, "hash");
	if (rhash == NULL) {
		pjdlog_error("Checksum algorithm is present, but hash is missing.");
		return (-1);	/* Hash not found. */
	}
	if (size != sizeof(chash)) {
		pjdlog_error("Invalid hash size (%zu) for %s, should be %zu.",
		    size, algo, sizeof(chash));
		return (-1);	/* Different hash size. */
	}

	SHA256_Init(&ctx);
	SHA256_Update(&ctx, *datap, *sizep);
	SHA256_Final(chash, &ctx);

	if (bcmp(rhash, chash, sizeof(chash)) != 0) {
		pjdlog_error("Hash mismatch.");
		return (-1);	/* Hash mismatch. */
	}

	return (0);
}
#endif	/* HAVE_CRYPTO */

/*
 * Send the given nv structure via conn.
 * We keep headers in nv structure and pass data in separate argument.
 * There can be no data at all (data is NULL then).
 */
int
hast_proto_send(struct hast_resource *res, struct proto_conn *conn,
    struct nv *nv, const void *data, size_t size)
{
	struct hast_main_header hdr;
	struct ebuf *eb;
	bool freedata;
	void *dptr, *hptr;
	size_t hsize;
	int ret;

	dptr = (void *)(uintptr_t)data;
	freedata = false;
	ret = -1;

	if (data != NULL) {
if (false) {
		unsigned int ii;

		for (ii = 0; ii < sizeof(pipeline) / sizeof(pipeline[0]);
		    ii++) {
			ret = pipeline[ii].hps_send(res, nv, &dptr, &size,
			    &freedata);
			if (ret == -1)
				goto end;
		}
		ret = -1;
}
		nv_add_uint32(nv, size, "size");
		if (nv_error(nv) != 0) {
			errno = nv_error(nv);
			goto end;
		}
	}

	eb = nv_hton(nv);
	if (eb == NULL)
		goto end;

	hdr.version = HAST_PROTO_VERSION;
	hdr.size = htole32((uint32_t)ebuf_size(eb));
	if (ebuf_add_head(eb, &hdr, sizeof(hdr)) < 0)
		goto end;

	hptr = ebuf_data(eb, &hsize);
	if (proto_send(conn, hptr, hsize) < 0)
		goto end;
	if (data != NULL && proto_send(conn, dptr, size) < 0)
		goto end;

	ret = 0;
end:
	if (freedata)
		free(dptr);
	return (ret);
}

int
hast_proto_recv_hdr(struct proto_conn *conn, struct nv **nvp)
{
	struct hast_main_header hdr;
	struct nv *nv;
	struct ebuf *eb;
	void *hptr;

	eb = NULL;
	nv = NULL;

	if (proto_recv(conn, &hdr, sizeof(hdr)) < 0)
		goto fail;

	if (hdr.version != HAST_PROTO_VERSION) {
		errno = ERPCMISMATCH;
		goto fail;
	}

	hdr.size = le32toh(hdr.size);

	eb = ebuf_alloc(hdr.size);
	if (eb == NULL)
		goto fail;
	if (ebuf_add_tail(eb, NULL, hdr.size) < 0)
		goto fail;
	hptr = ebuf_data(eb, NULL);
	assert(hptr != NULL);
	if (proto_recv(conn, hptr, hdr.size) < 0)
		goto fail;
	nv = nv_ntoh(eb);
	if (nv == NULL)
		goto fail;

	*nvp = nv;
	return (0);
fail:
	if (nv != NULL)
		nv_free(nv);
	else if (eb != NULL)
		ebuf_free(eb);
	return (-1);
}

int
hast_proto_recv_data(struct hast_resource *res, struct proto_conn *conn,
    struct nv *nv, void *data, size_t size)
{
	unsigned int ii;
	bool freedata;
	size_t dsize;
	void *dptr;
	int ret;

	assert(data != NULL);
	assert(size > 0);

	ret = -1;
	freedata = false;
	dptr = data;

	dsize = nv_get_uint32(nv, "size");
	if (dsize == 0)
		(void)nv_set_error(nv, 0);
	else {
		if (proto_recv(conn, data, dsize) < 0)
			goto end;
if (false) {
		for (ii = sizeof(pipeline) / sizeof(pipeline[0]); ii > 0;
		    ii--) {
			assert(!"to be verified");
			ret = pipeline[ii - 1].hps_recv(res, nv, &dptr,
			    &dsize, &freedata);
			if (ret == -1)
				goto end;
		}
		ret = -1;
		if (dsize < size)
			goto end;
		/* TODO: 'size' doesn't seem right here. It is maximum data size. */
		if (dptr != data)
			bcopy(dptr, data, dsize);
}
	}

	ret = 0;
end:
if (ret < 0) printf("%s:%u %s\n", __func__, __LINE__, strerror(errno));
	if (freedata)
		free(dptr);
	return (ret);
}

int
hast_proto_recv(struct hast_resource *res, struct proto_conn *conn,
    struct nv **nvp, void *data, size_t size)
{
	struct nv *nv;
	size_t dsize;
	int ret;

	ret = hast_proto_recv_hdr(conn, &nv);
	if (ret < 0)
		return (ret);
	dsize = nv_get_uint32(nv, "size");
	if (dsize == 0)
		(void)nv_set_error(nv, 0);
	else
		ret = hast_proto_recv_data(res, conn, nv, data, size);
	if (ret < 0)
		nv_free(nv);
	else
		*nvp = nv;
	return (ret);
}
