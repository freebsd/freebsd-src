/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * MIPS64 specific IP/TCP/UDP checksumming routines
 *
 * Copyright (C) 1998, 1999 Ralf Baechle
 */
#include <net/checksum.h>
#include <linux/types.h>
#include <asm/byteorder.h>
#include <asm/string.h>
#include <asm/uaccess.h>

/*
 * copy while checksumming, otherwise like csum_partial
 */
unsigned int csum_partial_copy(const char *src, char *dst,
                               int len, unsigned int sum)
{
	/*
	 * It's 2:30 am and I don't feel like doing it real ...
	 * This is lots slower than the real thing (tm)
	 */
	sum = csum_partial(src, len, sum);
	memcpy(dst, src, len);

	return sum;
}

/*
 * Copy from userspace and compute checksum.  If we catch an exception
 * then zero the rest of the buffer.
 */
unsigned int csum_partial_copy_from_user (const char *src, char *dst,
                                          int len, unsigned int sum,
                                          int *err_ptr)
{
	int missing;

	missing = copy_from_user(dst, src, len);
	if (missing) {
		memset(dst + len - missing, 0, missing);
		*err_ptr = -EFAULT;
	}

	return csum_partial(dst, len, sum);
}
