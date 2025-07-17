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
 * Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Portions Copyright 2022 Mikhail Zakharov <zmey20000@yahoo.com>
 */

#include <contrib/zlib/zlib.h>
#include <contrib/zlib/zutil.h>

static void *
zfs_zcalloc(void *opaque __unused, uint_t items, uint_t size)
{

	return (calloc(items, size));
}

static void
zfs_zcfree(void *opaque __unused, void *ptr)
{
	free(ptr);
}

/*
 * Uncompress the buffer 'src' into the buffer 'dst'.  The caller must store
 * the expected decompressed data size externally so it can be passed in.
 * The resulting decompressed size is then returned through dstlen.  This
 * function return Z_OK on success, or another error code on failure.
 */
static inline int
z_uncompress(void *dst, size_t *dstlen, const void *src, size_t srclen)
{
	z_stream zs;
	int err;

	bzero(&zs, sizeof (zs));
	zs.next_in = (unsigned char *)src;
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

static int
gzip_decompress(void *s_start, void *d_start, size_t s_len, size_t d_len,
    int n __unused)
{
	size_t dstlen = d_len;

	ASSERT(d_len >= s_len);

	if (z_uncompress(d_start, &dstlen, s_start, s_len) != Z_OK)
		return (-1);

	return (0);
}
