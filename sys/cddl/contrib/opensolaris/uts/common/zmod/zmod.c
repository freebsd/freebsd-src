/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/systm.h>
#include <sys/kobj.h>
#include <sys/zmod.h>

#include <contrib/zlib/zlib.h>
#include <contrib/zlib/zutil.h>

struct zchdr {
	uint_t zch_magic;
	uint_t zch_size;
};

#define	ZCH_MAGIC	0x3cc13cc1

/*ARGSUSED*/
static void *
zfs_zcalloc(void *opaque, uint_t items, uint_t size)
{
	size_t nbytes = sizeof (struct zchdr) + items * size;
	struct zchdr *z = kobj_zalloc(nbytes, KM_NOWAIT|KM_TMP);

	if (z == NULL)
		return (NULL);

	z->zch_magic = ZCH_MAGIC;
	z->zch_size = nbytes;

	return (z + 1);
}

/*ARGSUSED*/
static void
zfs_zcfree(void *opaque, void *ptr)
{
	struct zchdr *z = ((struct zchdr *)ptr) - 1;

	if (z->zch_magic != ZCH_MAGIC)
		panic("zcfree region corrupt: hdr=%p ptr=%p", (void *)z, ptr);

	kobj_free(z, z->zch_size);
}

/*
 * Uncompress the buffer 'src' into the buffer 'dst'.  The caller must store
 * the expected decompressed data size externally so it can be passed in.
 * The resulting decompressed size is then returned through dstlen.  This
 * function return Z_OK on success, or another error code on failure.
 */
int
z_uncompress(void *dst, size_t *dstlen, const void *src, size_t srclen)
{
	z_stream zs;
	int err;

	bzero(&zs, sizeof (zs));
	zs.next_in = (uchar_t *)src;
	zs.avail_in = srclen;
	zs.next_out = dst;
	zs.avail_out = *dstlen;
	zs.zalloc = zfs_zcalloc;
	zs.zfree = zfs_zcfree;

	/*
	 * Call inflateInit2() specifying a window size of DEF_WBITS
	 * with the 6th bit set to indicate that the compression format
	 * type (zlib or gzip) should be automatically detected.
	 */
	if ((err = inflateInit2(&zs, DEF_WBITS | 0x20)) != Z_OK)
		return (err);

	if ((err = inflate(&zs, Z_FINISH)) != Z_STREAM_END) {
		(void) inflateEnd(&zs);
		return (err == Z_OK ? Z_BUF_ERROR : err);
	}

	*dstlen = zs.total_out;
	return (inflateEnd(&zs));
}

int
z_compress_level(void *dst, size_t *dstlen, const void *src, size_t srclen,
    int level)
{

	z_stream zs;
	int err;

	bzero(&zs, sizeof (zs));
	zs.next_in = (uchar_t *)src;
	zs.avail_in = srclen;
	zs.next_out = dst;
	zs.avail_out = *dstlen;
	zs.zalloc = zfs_zcalloc;
	zs.zfree = zfs_zcfree;

	if ((err = deflateInit(&zs, level)) != Z_OK)
		return (err);

	if ((err = deflate(&zs, Z_FINISH)) != Z_STREAM_END) {
		(void) deflateEnd(&zs);
		return (err == Z_OK ? Z_BUF_ERROR : err);
	}

	*dstlen = zs.total_out;
	return (deflateEnd(&zs));
}

int
z_compress(void *dst, size_t *dstlen, const void *src, size_t srclen)
{
	return (z_compress_level(dst, dstlen, src, srclen,
	    Z_DEFAULT_COMPRESSION));
}

/*
 * Convert a zlib error code into a string error message.
 */
const char *
z_strerror(int err)
{
	int i = Z_NEED_DICT - err;

	if (i < 0 || i > Z_NEED_DICT - Z_VERSION_ERROR)
		return ("unknown error");

	return (zError(err));
}
