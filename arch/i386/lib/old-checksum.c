/*
 * FIXME: old compatibility stuff, will be removed soon.
 */

#include <net/checksum.h>

unsigned int csum_partial_copy( const char *src, char *dst, int len, int sum)
{
	int src_err=0, dst_err=0;

	sum = csum_partial_copy_generic ( src, dst, len, sum, &src_err, &dst_err);

	if (src_err || dst_err)
		printk("old csum_partial_copy_fromuser(), tell mingo to convert me.\n");

	return sum;
}


