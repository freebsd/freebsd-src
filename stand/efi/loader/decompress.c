/*
 * Copyright (c) 2026 Netflix, Inc. Written by Warner Losh
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "stand.h"
#include "loader_efi.h"
#include <efilib.h>
#include "decompress.h"

#include <zlib.h>
#include <bzlib.h>
#ifdef LOADER_ZFS_SUPPORT	/* ZSTD only available with ZFS */
#include <zstd.h>
#endif
#include <sys/_param.h>

#define ULL(x) ((unsigned long long)(x))

static EFI_MEMORY_TYPE mem_type = EfiReservedMemoryType;

struct decomp_state
{
	enum compression type;
	size_t size;		/* Best guess at final size */
	size_t alloc_size;	/* Current size of `buf` */
	size_t pages;		/* Alloc_size in pages */
	uint8_t *buf_cur;	/* Current output buffer */
	uint8_t *buf_end;	/* Current end of allocated buffer */
	EFI_PHYSICAL_ADDRESS buf; /* Decompression buffer */
	union {
		z_stream zstrm;
		bz_stream bzstrm;
#ifdef LOADER_ZFS_SUPPORT
		ZSTD_DStream *zstdstrm;
#endif
	};
	EFI_STATUS (*init)(decomp_state *dctx, uint8_t *first_buf, size_t buflen,
	    size_t size_hint);
	enum step_return (*step)(decomp_state *dctx, uint8_t *buf, size_t len, size_t offset);
	void (*fini)(decomp_state *dctx, bool flush);
};

static enum compression
what_compressed(uint8_t *buf, size_t len)
{
	/*
	 * Failsafe
	 */
	if (len < 32)
		return (none);
	if (memcmp(buf, "\x1f\x8b", 2) == 0) {
		printf("GZIP\n");
		return (zlib);
	}
	if (memcmp(buf, "BZh", 3) == 0) {
		printf("BZIP2\n");
		return (bzip2);
	}
	if (memcmp(buf, "\x28\xb5\x2f\xfd", 4) == 0) {
		printf("zstd\n");
		return (zstd);
	}
	if (memcmp(buf, "\xfd""7zXZ\x00", 6) == 0) {
		printf("xz -- unsupproted\n");
	} else {
		printf("Not compressed\n");
	}
	return (none);
}

static EFI_STATUS
alloc_buffer(decomp_state *dctx, size_t size)
{
	dctx->alloc_size = roundup2(size, EFI_PAGE_SIZE);
	dctx->pages = dctx->alloc_size / EFI_PAGE_SIZE;
	EFI_STATUS status = BS->AllocatePages(AllocateAnyPages, mem_type, dctx->pages, &dctx->buf);
	if (EFI_ERROR(status)) {
		printf("Failed to allocate memory for %llu bytes\n", ULL(dctx->alloc_size));
		return (status);
	}
	BS->SetMem((void *)dctx->buf, dctx->alloc_size, 0);
	dctx->buf_cur = (uint8_t *)dctx->buf;
	dctx->buf_end = (uint8_t *)dctx->buf + dctx->alloc_size;
	return (EFI_SUCCESS);
}

static EFI_STATUS
grow_buffer(decomp_state *dctx)
{
	/*
	 * a 1.5 exp growth trades a few more copies for a little less waste.
	 */
	size_t newsz = roundup2(dctx->alloc_size * 3 / 2, EFI_PAGE_SIZE);
	size_t newpages = newsz / EFI_PAGE_SIZE;
	EFI_PHYSICAL_ADDRESS newbuf;
	EFI_STATUS status = BS->AllocatePages(AllocateAnyPages, mem_type, newpages, &newbuf);
	if (EFI_ERROR(status)) {
		printf("Failed to allocate memory for %llu bytes\n", ULL(newsz));
		return (status);
	}
	memcpy((void *)newbuf, (void *)dctx->buf, dctx->alloc_size);
	BS->FreePages(dctx->buf, dctx->pages);
	dctx->buf = newbuf;
	dctx->pages = newpages;
	dctx->buf_cur = (uint8_t *)dctx->buf + dctx->alloc_size;
	dctx->buf_end = (uint8_t *)dctx->buf + newsz;
	BS->SetMem(dctx->buf_cur, newsz - dctx->alloc_size, 0);
	dctx->alloc_size = newsz;
	return (EFI_SUCCESS);
}

static void
free_buffer(decomp_state *dctx)
{
	if (dctx->buf)
		BS->FreePages(dctx->buf, dctx->pages);
	dctx->buf = 0;
}

/*
 * zlib supprot
 */
static EFI_STATUS
zlib_init(decomp_state *dctx, uint8_t *first_buf, size_t buflen, size_t size_hint)
{
	z_stream *strm = &dctx->zstrm;

	/*
	 * Assume 4x compression, but start at 64MB
	 */
	dctx->size = max(size_hint * 4, M(64));
	EFI_STATUS status = alloc_buffer(dctx, dctx->size);
	if (EFI_ERROR(status))
		return (status);
        memset(strm, 0, sizeof(*strm));
        strm->next_in = first_buf;
        strm->avail_in = buflen;
        return (inflateInit2(strm, 15 + 16) == Z_OK ? EFI_SUCCESS : EFI_VOLUME_CORRUPTED);
}

static enum step_return
zlib_step(decomp_state *dctx, uint8_t *buf, size_t len, size_t offset)
{
	z_stream *strm = &dctx->zstrm;
	size_t outlen = dctx->buf_end - dctx->buf_cur;

        strm->next_in = buf;
        strm->avail_in = len;
        strm->next_out = dctx->buf_cur;
        strm->avail_out = outlen;
	
        int ret = inflate(strm, Z_NO_FLUSH);
	dctx->buf_cur += outlen - strm->avail_out;

        if (ret == Z_STREAM_END)
                return (done);
        if (ret != Z_OK)
                return (err);
        if (dctx->buf_cur < dctx->buf_end) /* Have output space */
		return (ok);

	/*
	 * We're out of space, grow the buffer and try again if there's buffer
	 * space. We try again recursively since we know that will usually go
	 * only 1 deep.
	 */
	if (EFI_ERROR(grow_buffer(dctx)))
		return (err);
	if (strm->avail_in == 0)
		return (ok);
	size_t consumed = len - strm->avail_in;
	return (zlib_step(dctx, buf + consumed, strm->avail_in, offset + consumed));
}

static void
zlib_fini(decomp_state *dctx, bool flush)
{
	inflateEnd(&dctx->zstrm);
	if (!flush)
		return;
	free_buffer(dctx);
}

/*
 * Bzip2 supprot
 */
static EFI_STATUS
bzip2_init(decomp_state *dctx, uint8_t *first_buf, size_t buflen, size_t size_hint)
{
	bz_stream *strm = &dctx->bzstrm;

	/*
	 * Assume 4x compression, but start at 64MB
	 */
	dctx->size = max(size_hint * 4, M(64));
	EFI_STATUS status = alloc_buffer(dctx, dctx->size);
	if (EFI_ERROR(status))
		return (status);
        memset(strm, 0, sizeof(*strm));
        strm->next_in = first_buf;
        strm->avail_in = buflen;
        return (BZ2_bzDecompressInit(strm, 0, 0) == BZ_OK ? EFI_SUCCESS : EFI_VOLUME_CORRUPTED);
}

static enum step_return
bzip2_step(decomp_state *dctx, uint8_t *buf, size_t len, size_t offset)
{
	bz_stream *strm = &dctx->bzstrm;
	size_t outlen = dctx->buf_end - dctx->buf_cur;

        strm->next_in = buf;
        strm->avail_in = len;
        strm->next_out = dctx->buf_cur;
        strm->avail_out = outlen;
	
        int ret = BZ2_bzDecompress(strm);
	dctx->buf_cur += outlen - strm->avail_out;

        if (ret == BZ_STREAM_END)
                return (done);
        if (ret != Z_OK)
                return (err);
        if (dctx->buf_cur < dctx->buf_end) /* Have output space */
		return (ok);

	/*
	 * We're out of space, grow the buffer and try again if there's buffer
	 * space. We try again recursively since we know that will usually go
	 * only 1 deep.
	 */
	if (EFI_ERROR(grow_buffer(dctx)))
		return (err);
	if (strm->avail_in == 0)
		return (ok);
	size_t consumed = len - strm->avail_in;
	return (bzip2_step(dctx, buf + consumed, strm->avail_in, offset + consumed));
}

static void
bzip2_fini(decomp_state *dctx, bool flush)
{
	BZ2_bzDecompressEnd(&dctx->bzstrm);
	if (!flush)
		return;
	free_buffer(dctx);
}

/*
 * ZSTD supprot
 */
#ifdef LOADER_ZFS_SUPPORT
static EFI_STATUS
zstd_init(decomp_state *dctx, uint8_t *first_buf, size_t buflen, size_t size_hint)
{
	unsigned long long size = ZSTD_getFrameContentSize(first_buf, buflen);
	if (size == ZSTD_CONTENTSIZE_ERROR)
		return (EFI_VOLUME_CORRUPTED);
	if (size == ZSTD_CONTENTSIZE_UNKNOWN)
		dctx->size = max(size_hint * 4, M(64));		/* Guess 4x compression or 64M */
	else
		dctx->size = size;				/* We know the size */
	EFI_STATUS status = alloc_buffer(dctx, dctx->size);
	if (EFI_ERROR(status))
		return (status);

        dctx->zstdstrm = ZSTD_createDStream();
        if (dctx->zstdstrm == NULL)
                return (EFI_OUT_OF_RESOURCES);
        if (ZSTD_isError(ZSTD_initDStream(dctx->zstdstrm))) {
		ZSTD_freeDStream(dctx->zstdstrm);
		dctx->zstdstrm = NULL;
                return (EFI_OUT_OF_RESOURCES);
	}
	return (EFI_SUCCESS);
}
	
static enum step_return
zstd_step(decomp_state *dctx, uint8_t *buf, size_t len, size_t offset)
{
	size_t outlen = dctx->buf_end - dctx->buf_cur;
	ZSTD_inBuffer inbuf = { buf, len, 0 };
	ZSTD_outBuffer outbuf = { dctx->buf_cur, outlen, 0 };
	size_t ret;

	ret = ZSTD_decompressStream(dctx->zstdstrm, &outbuf, &inbuf);
	dctx->buf_cur += outbuf.pos;

        if (ZSTD_isError(ret))
                return (err);
        if (ret == 0)
                return (done);
        if (dctx->buf_cur < dctx->buf_end) /* Have output space */
		return (ok);

	/*
	 * We're out of space, grow the buffer and try again if there's buffer
	 * space. We try again recursively since we know that will usually go
	 * only 1 deep.
	 */
	if (EFI_ERROR(grow_buffer(dctx)))
		return (err);
	if (inbuf.size == inbuf.pos)
		return (ok);
	return (zstd_step(dctx, buf + inbuf.pos, inbuf.size - inbuf.pos, offset + inbuf.pos));
}

static void
zstd_fini(decomp_state *dctx, bool flush)
{
	ZSTD_freeDStream(dctx->zstdstrm);
	if (!flush)
		return;
	free_buffer(dctx);
}
#endif

/*
 * No / Unknown decompression fallback
 */
static EFI_STATUS
null_init(decomp_state *dctx, uint8_t *first_buf, size_t buflen, size_t size_hint)
{
	dctx->size = size_hint;
	return (alloc_buffer(dctx, size_hint));
}

static enum step_return
null_step(decomp_state *dctx, uint8_t *buf, size_t len, size_t offset)
{
	size_t end = offset + len;
	
	if (end > dctx->size) {
		printf("Too much data recieved!");
		return (err);
	}

	CHAR8 *src = buf;
	CHAR8 *dst = (void*)(dctx->buf + offset);
	BS->CopyMem(dst, src, len);
	return (end == dctx->size ? done : ok);
}

static void
null_fini(decomp_state *dctx, bool flush)
{
	if (!flush)
		return;
	free_buffer(dctx);
}

decomp_state *
decomp_init(uint8_t *buf, size_t buflen, size_t size_hint)
{
	decomp_state *dctx;
	
	dctx = malloc(sizeof(*dctx));
	memset(dctx, 0, sizeof(*dctx));
	dctx->type = what_compressed(buf, buflen);
	switch (dctx->type) {
	case zlib:
		dctx->init = zlib_init;
		dctx->step = zlib_step;
		dctx->fini = zlib_fini;
		break;
	case bzip2:
		dctx->init = bzip2_init;
		dctx->step = bzip2_step;
		dctx->fini = bzip2_fini;
		break;
#ifdef LOADER_ZFS_SUPPORT
	case zstd:
		dctx->init = zstd_init;
		dctx->step = zstd_step;
		dctx->fini = zstd_fini;
		break;
#endif
	case none:
		dctx->init = null_init;
		dctx->step = null_step;
		dctx->fini = null_fini;
		break;
	default:
		return (NULL);
	}

	if (EFI_ERROR(dctx->init(dctx, buf, buflen, size_hint))) {
		free(dctx);
		dctx = NULL;
	}
	return (dctx);
}

enum step_return
decomp_step(decomp_state *dctx, uint8_t *buf, size_t len, size_t offset)
{
	return (dctx->step(dctx, buf, len, offset));
}

void
decomp_fini(decomp_state *dctx, bool flush)
{
	return (dctx->fini(dctx, flush));
}

EFI_PHYSICAL_ADDRESS
decomp_buffer(decomp_state *dctx)
{
	if (dctx == NULL)
		return (0);
	return (dctx->buf);
}

size_t
decomp_buffer_length(decomp_state *dctx)
{
	if (dctx == NULL)
		return (0);
	return ((void *)dctx->buf_cur - (void *)dctx->buf);
}
