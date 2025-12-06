/*-
 * Copyright (c) 2024 Kyle Evans <kevans@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * This test simply configures the tunnel as transient and exits.  By the time
 * we return, the tunnel should be gone because the last reference disappears.
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <net/if_tun.h>
#include <net/if_tap.h>

#include <assert.h>
#include <err.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

int
main(int argc, char *argv[])
{
	unsigned long tunreq;
	const char *tundev;
	int one = 1, tunfd;

	assert(argc > 1);
	tundev = argv[1];

	tunfd = open(tundev, O_RDWR);
	assert(tunfd >= 0);

	/*
	 * These are technically the same request, but we'll use the technically
	 * correct one just in case.
	 */
	if (strstr(tundev, "tun") != NULL) {
		tunreq = TUNSTRANSIENT;
	} else {
		assert(strstr(tundev, "tap") != NULL);
		tunreq = TAPSTRANSIENT;
	}

	if (ioctl(tunfd, tunreq, &one) == -1)
		err(1, "ioctl");

	/* Final close should destroy the tunnel automagically. */
	close(tunfd);

	return (0);
}
