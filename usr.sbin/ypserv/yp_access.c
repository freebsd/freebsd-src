/*
 * Copyright (c) 1995
 *	Bill Paul <wpaul@ctr.columbia.edu>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <rpc/rpc.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <paths.h>
#include <sys/param.h>
#include "yp_extern.h"
#ifdef TCP_WRAPPER
#include "tcpd.h"
#endif

extern int debug;

char *yp_procs[] = {	"ypproc_null" ,
			"ypproc_domain",
			"ypproc_domain_nonack",
			"ypproc_match",
			"ypproc_first",
			"ypproc_next",
			"ypproc_xfr",
			"ypproc_clear",
			"ypproc_all",
			"ypproc_master",
			"ypproc_order",
			"ypproc_maplist"
		   };

/*
 * Access control functions.
 *
 * yp_access() checks the mapname and client host address and watches for
 * the following things:
 *
 * - If the client is referencing one of the master.passwd.* maps, it must
 *   be using a privileged port to make its RPC to us. If it is, then we can
 *   assume that the caller is root and allow the RPC to succeed. If it
 *   isn't access is denied.
 *
 * - If we are compiled with the tcpwrapper package, we also check to see
 *   if the host makes it past the libwrap checks and deny access if it
 *   doesn't. Host address checks are disabled if not compiled with the
 *   tcp_wrapper package.
 *
 * The yp_validdomain() functions checks the domain specified by the caller
 * to make sure it's actually served by this server. This is more a sanity
 * check than an a security check, but this seems to be the best place for
 * it.
 */

int yp_access(map, rqstp)
	const char *map;
	const struct svc_req *rqstp;
{
	struct sockaddr_in *rqhost;
#ifdef TCP_WRAPPER
	int status = 0;
	unsigned long oldaddr;
#endif

	rqhost = svc_getcaller(rqstp->rq_xprt);

	if (debug) {
		yp_error("Procedure %s called from %s:%d",
			yp_procs[rqstp->rq_proc], inet_ntoa(rqhost->sin_addr),
			ntohs(rqhost->sin_port));
		if (map != NULL)
			yp_error("Client is referencing map \"%s\".", map);
	}

	/* Check the map name if one was supplied. */
	if (map != NULL) {
		if (strstr(map, "master.passwd.") && ntohs(rqhost->sin_port) > 1023) {
			yp_error("Access to %s denied -- client not privileged", map);
			return(1);
		}
	}

#ifdef TCP_WRAPPER
	/* Check client address if TCP_WRAPPER is enalbled. */
	status = hosts_ctl(progname, STRING_UNKNOWN,
			   inet_ntoa(rqhost->sin_addr, "");

	if (!status && rqhost->sin_addr.s_addr != oldaddr) {
		yp_error("connect from %s:%d refused",
			  inet_ntoa(rqhost->sin_addr), ntohs(rqhost->sin_port));
		oldaddr = rqhost->sin_addr.s_addr;
		return(1);
	}
#endif
	return(0);

}

int yp_validdomain(domain)
	const char *domain;
{
	struct stat statbuf;
	char dompath[MAXPATHLEN + 2];

	if (domain == NULL || strstr(domain, "binding") ||
	    !strcmp(domain, ".") || !strcmp(domain, "..") ||
	    strchr(domain, '/'))
		return(1);

	snprintf(dompath, sizeof(dompath), "%s/%s", yp_dir, domain);

	if (stat(dompath, &statbuf) < 0 || !S_ISDIR(statbuf.st_mode))
		return(1);

	return(0);
}
