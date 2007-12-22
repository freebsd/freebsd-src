/*-
 * Copyright (c) 2003 Mike Barcroft <mike@FreeBSD.org>
 * All rights reserved.
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
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/jail.h>
#include <sys/sysctl.h>

#include <arpa/inet.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>


int
main(void)
{ 
	struct xprison *sxp, *xp;
	struct in_addr in;
	size_t i, len;

	if (sysctlbyname("security.jail.list", NULL, &len, NULL, 0) == -1)
		err(1, "sysctlbyname(): security.jail.list");

	for (i = 0; i < 4; i++) {
		if (len <= 0)
			exit(0);	
		sxp = xp = malloc(len);
		if (sxp == NULL)
			err(1, "malloc()");

		if (sysctlbyname("security.jail.list", xp, &len, NULL, 0) == -1) {
			if (errno == ENOMEM) {
				free(sxp);
				sxp = NULL;
				continue;
			}
			err(1, "sysctlbyname(): security.jail.list");
		}
		break;
	}
	if (sxp == NULL)
		err(1, "sysctlbyname(): security.jail.list");
	if (len < sizeof(*xp) || len % sizeof(*xp) ||
	    xp->pr_version != XPRISON_VERSION)
		errx(1, "Kernel and userland out of sync");

	printf("   JID  IP Address      Hostname                      Path\n");
	for (i = 0; i < len / sizeof(*xp); i++) {
		in.s_addr = ntohl(xp->pr_ip);
		printf("%6d  %-15.15s %-29.29s %.74s\n",
		    xp->pr_id, inet_ntoa(in), xp->pr_host, xp->pr_path);
		xp++;
	}
	free(sxp);
	exit(0);
}
