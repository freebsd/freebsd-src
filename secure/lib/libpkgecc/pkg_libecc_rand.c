/* SPDX-License-Identifier: Unlicense */
#include <sys/types.h>
#include <stdlib.h>

#include <libecc/external_deps/rand.h>

int
get_random(unsigned char *buf, uint16_t len)
{

	/*
	 * We need random numbers even in a sandbox, so we can't use
	 * /dev/urandom as the external_deps version of get_random() does on
	 * FreeBSD.  arc4random_buf() is a better choice because it uses the
	 * underlying getrandom(2) instead of needing to open a device handle.
	 *
	 * We don't have any guarantees that this won't open a device on other
	 * platforms, but we also don't do any sandboxing on those platforms.
	 */
	arc4random_buf(buf, len);
	return 0;
}
