/*
 * Temporal C versions of the checksum functions until optimized assembler versions
 * can go in.
 */

#include <net/checksum.h>

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

unsigned int csum_partial_copy_nocheck(const char *src, char *dst, int len, unsigned int sum)
{
	memcpy(dst,src,len);
	return csum_partial(dst,len,sum);
}

/* Fallback for csum_and_copy_to_user is currently in include/net/checksum.h */
