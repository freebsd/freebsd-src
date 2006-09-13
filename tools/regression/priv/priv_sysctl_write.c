/*-
 * Copyright (c) 2006 nCircle Network Security, Inc.
 * All rights reserved.
 *
 * This software was developed by Robert N. M. Watson for the TrustedBSD
 * Project under contract to nCircle Network Security, Inc.
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR, NCIRCLE NETWORK SECURITY,
 * INC., OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 * Test that sysctls can only be written with privilege by trying first with,
 * then without privilege.  Do this by first reading, then setting the
 * hostname as a no-op.
 */

#include <sys/types.h>
#include <sys/sysctl.h>

#include <err.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "main.h"

#define KERN_HOSTNAME_STRING	"kern.hostname"

void
priv_sysctl_write(void)
{
	char buffer[1024];
	size_t len;
	int error;

	assert_root();

	/*
	 * First query the current value.
	 */
	len = sizeof(buffer);
	error = sysctlbyname(KERN_HOSTNAME_STRING, buffer, &len, NULL, 0);
	if (error)
		err(-1, "sysctlbyname(\"%s\") query", KERN_HOSTNAME_STRING);

	/*
	 * Now try to set with privilege.
	 */
	error = sysctlbyname(KERN_HOSTNAME_STRING, NULL, NULL, buffer,
	    strlen(buffer));
	if (error)
		err(-1, "sysctlbyname(\"%s\") set as root",
		    KERN_HOSTNAME_STRING);

	/*
	 * Now without privilege.
	 */
	set_euid(UID_OTHER);

	error = sysctlbyname(KERN_HOSTNAME_STRING, NULL, NULL, buffer,
	    strlen(buffer));
	if (error == 0)
		errx(-1, "sysctlbyname(\"%s\") succeeded as !root",
		    KERN_HOSTNAME_STRING);
	if (errno != EPERM)
		err(-1, "sysctlbyname(\"%s\") wrong errno %d",
		    KERN_HOSTNAME_STRING, errno);
}
