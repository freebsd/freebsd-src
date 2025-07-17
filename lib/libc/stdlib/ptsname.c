/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2008 Ed Schouten <ed@FreeBSD.org>
 * All rights reserved.
 *
 * Portions of this software were developed under sponsorship from Snow
 * B.V., the Netherlands.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "namespace.h"
#include <sys/param.h>
#include <sys/ioctl.h>

#include <errno.h>
#include <paths.h>
#include <stdlib.h>
#include <string.h>
#include "un-namespace.h"

/*
 * __isptmaster():  return whether the file descriptor refers to a
 *                  pseudo-terminal master device.
 */
static int
__isptmaster(int fildes)
{

	if (_ioctl(fildes, TIOCPTMASTER) == 0)
		return (0);

	if (errno != EBADF)
		errno = EINVAL;

	return (-1);
}

/*
 * In our implementation, grantpt() and unlockpt() don't actually have
 * any use, because PTY's are created on the fly and already have proper
 * permissions upon creation.
 *
 * Just make sure `fildes' actually points to a real PTY master device.
 */
__strong_reference(__isptmaster, grantpt);
__strong_reference(__isptmaster, unlockpt);

/*
 * ptsname_r(): return the pathname of the slave pseudo-terminal device
 *              associated with the specified master.
 */
int
__ptsname_r(int fildes, char *buffer, size_t buflen)
{

	if (buflen <= sizeof(_PATH_DEV)) {
		errno = ERANGE;
		return (-1);
	}

	/* Make sure fildes points to a master device. */
	if (__isptmaster(fildes) != 0)
		return (-1);

	memcpy(buffer, _PATH_DEV, sizeof(_PATH_DEV));
	buffer += sizeof(_PATH_DEV) - 1;
	buflen -= sizeof(_PATH_DEV) - 1;

	if (fdevname_r(fildes, buffer, buflen) == NULL) {
		if (errno == EINVAL)
			errno = ERANGE;
		return (-1);
	}

	return (0);
}

__strong_reference(__ptsname_r, ptsname_r);

/*
 * ptsname():  return the pathname of the slave pseudo-terminal device
 *             associated with the specified master.
 */
char *
ptsname(int fildes)
{
	static char pt_slave[sizeof(_PATH_DEV) + SPECNAMELEN];

	if (__ptsname_r(fildes, pt_slave, sizeof(pt_slave)) == 0)
		return (pt_slave);

	return (NULL);
}
