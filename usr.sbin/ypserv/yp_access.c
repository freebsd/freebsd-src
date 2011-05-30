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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/fcntl.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <rpc/rpc.h>
#include <rpcsvc/yp.h>
#include <rpcsvc/yppasswd.h>
#include <rpcsvc/ypxfrd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <db.h>
#include <errno.h>
#include <limits.h>
#include <netdb.h>
#include <paths.h>
#include <stdlib.h>

#include "yp_extern.h"
#ifdef TCP_WRAPPER
#include "tcpd.h"
extern int hosts_ctl(const char *, const char *, const char *, const char *);
#endif

char securenets_path[MAXPATHLEN];
enum yp_snf_format securenets_format = YP_SNF_NATIVE;

const char *yp_procs[] = {
	/* NIS v1 */
	"ypoldproc_null",
	"ypoldproc_domain",
	"ypoldproc_domain_nonack",
	"ypoldproc_match",
	"ypoldproc_first",
	"ypoldproc_next",
	"ypoldproc_poll",
	"ypoldproc_push",
	"ypoldproc_get",
	"badproc1", /* placeholder */
	"badproc2", /* placeholder */
	"badproc3", /* placeholder */
	
	/* NIS v2 */
	"ypproc_null",
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

static SLIST_HEAD(, securenet) securenets =
	SLIST_HEAD_INITIALIZER(securenets);
struct securenet {
	struct sockaddr_storage	sn_addr;
	struct sockaddr_storage	sn_mask;
	SLIST_ENTRY(securenet)	sn_next;
};

static int
mask2prefixlen(const struct sockaddr *sap, int *prefixlen)
{
	switch (sap->sa_family) {
#ifdef AF_INET
	case AF_INET:
		return (yp_mask2prefixlen_in(sap, prefixlen));
		break;
#endif
#ifdef AF_INET6
	case AF_INET6:
		return (yp_mask2prefixlen_in6(sap, prefixlen));
		break;
#endif
	default:
		break;
	}
	return (-1);
}

static int
prefixlen2mask(struct sockaddr *sap, const int *prefixlen)
{
	switch (sap->sa_family) {
#ifdef AF_INET
	case AF_INET:
		return (yp_prefixlen2mask_in(sap, prefixlen));
		break;
#endif
#ifdef AF_INET6
	case AF_INET6:
		return (yp_prefixlen2mask_in6(sap, prefixlen));
		break;
#endif
	default:
		break;
	}
	return (-1);
}

void
yp_debug_sa(const struct sockaddr *sap)
{
	int error;
	int plen;
	char host[NI_MAXHOST + 1];
	char serv[NI_MAXSERV + 1];

	error = getnameinfo(sap, sap->sa_len, host, sizeof(host), serv,
		    sizeof(serv), NI_NUMERICHOST | NI_NUMERICSERV);
	if (error)
		yp_error("sockaddr: %s", gai_strerror(error));
	mask2prefixlen(sap, &plen);
	yp_error("sockaddr: %d:[%s]:%s(/%d)",
		 sap->sa_family, host, serv, plen);
}

void
show_securenets(void)
{
	struct securenet *snp;
	struct sockaddr *sap;
	int i = 0;

	yp_error("--- securenets dump start ---");
	SLIST_FOREACH(snp, &securenets, sn_next) {
		i++;
		yp_error("entry %d:", i);
		sap = (struct sockaddr *)&(snp->sn_addr);
		yp_debug_sa(sap);
		sap = (struct sockaddr *)&(snp->sn_mask);
		yp_debug_sa(sap);
	}
	yp_error("--- securenets dump end ---");
}

/*
 * Read /var/yp/securenets file and initialize the securenets
 * list. If the file doesn't exist, we set up a dummy entry that
 * allows all hosts to connect.
 */
#define YP_ACL_HOSTMASK_INET	"255.255.255.255"
#define YP_ACL_HOSTMASK_INET6	"ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff"
int
load_securenets(void)
{
	FILE *fp;
	char linebuf[BUFSIZ + 2];
	struct securenet *snp;
	struct sockaddr *sap;
	int error = 0;
	int line;
	struct addrinfo hints, *res, *res0;

	if (SLIST_EMPTY(&securenets))
		SLIST_INIT(&securenets);
	else {
		/*
		 * If securenets is not NULL, we are being called to reload
		 * the list; free the existing list before re-reading the
		 * securenets file.
		 */
		while (!SLIST_EMPTY(&securenets)) {
			snp = SLIST_FIRST(&securenets);
			SLIST_REMOVE_HEAD(&securenets, sn_next);
			free(snp);
		}
	}
	if (debug)
		yp_error("load_securenets(): loading %s", securenets_path);
	if ((fp = fopen(securenets_path, "r")) == NULL) {
		if (errno == ENOENT) {
			/* Create empty access list. */
			if (debug)
				yp_error("load_securenets(): ENOENT");
			memset(&hints, 0, sizeof(hints));
			hints.ai_family = PF_UNSPEC;
			hints.ai_flags = AI_PASSIVE;
			hints.ai_socktype = SOCK_STREAM;
			error = getaddrinfo(NULL, "0", &hints, &res0);
			if (error) {
				yp_error("getaddrinfo() failed: %s",
				    gai_strerror(error));
				freeaddrinfo(res0);
				return (1);
			}
			for (res = res0; res; res = res->ai_next) {
				snp = malloc(sizeof(*snp));
				if (snp == NULL) {
					yp_error("malloc failed: %s",
						 strerror(errno));
					freeaddrinfo(res0);
					return (1);
				}
				memset(snp, 0, sizeof(*snp));
				memcpy(&snp->sn_addr, res->ai_addr,
				   sizeof(res->ai_addrlen));
				memcpy(&snp->sn_mask, res->ai_addr,
				   sizeof(res->ai_addrlen));
				sap = (struct sockaddr *)&(snp->sn_mask);
				prefixlen2mask(sap, 0);
				SLIST_INSERT_HEAD(&securenets, snp, sn_next);
			}
			freeaddrinfo(res0);
			return (0);
		} else {
			yp_error("fopen(%s) failed: %s", securenets_path,
			    strerror(errno));
			return (1);
		}
	}

	line = 0;
	while (fgets(linebuf, sizeof(linebuf), fp)) {
		int nitems;
		const char *col_host;
		const char *col_mask;
		char addr1[NI_MAXHOST + 1];
		char addr2[NI_MAXHOST + 1];
		int plen;
		sa_family_t family;
		char *p;

		line++;
		if (debug)
			yp_error("load_securenets(): read line %d", line);
		if ((linebuf[0] == '#')
		    || (strspn(linebuf, " \t\r\n") == strlen(linebuf)))
			continue;
		nitems = sscanf(linebuf, "%s %s", addr1, addr2);
		snp = malloc(sizeof(*snp));
		memset(snp, 0, sizeof(*snp));

		if (debug)
			yp_error("load_securenets(): nitems = %d", nitems);
		if (nitems == 2) {
			switch (securenets_format) {
			case YP_SNF_NATIVE:
				/* ex. 127.0.0.1 255.0.0.0 */
				col_host = addr1;
				col_mask = addr2;
				break;
			case YP_SNF_SOLARIS:
				/* ex. 255.0.0.0 127.0.0.1  */
				col_host = addr2;
				col_mask = addr1;
				break;
			default:
				yp_error("line %d: internal error: %s",
					 line, linebuf);
				continue;

			}
			if (debug) {
				yp_error("load_securenets(): try mask expr");
				yp_error("load_securenets(): host = [%s]",
				    col_host);
				yp_error("load_securenets(): mask = [%s]",
				    col_mask);
			}
			memset(&hints, 0, sizeof(hints));
			hints.ai_family = PF_UNSPEC;
			hints.ai_socktype = SOCK_STREAM;
			hints.ai_flags = AI_NUMERICHOST;
			error = getaddrinfo(col_host, NULL, &hints, &res0);
			if (error) {
				yp_error("line %d: "
					 "badly formatted securenets entry: "
					 "%s: %s", line, linebuf,
					 gai_strerror(error));
				freeaddrinfo(res0);
				free(snp);
				continue;
			}
			memcpy(&snp->sn_addr, res0->ai_addr, res0->ai_addrlen);
			family = res0->ai_addr->sa_family;
			freeaddrinfo(res0);

			if ((securenets_format == YP_SNF_SOLARIS) &&
			    (strcmp(col_host, "host") == 0)) {
				switch (family) {
#ifdef AF_INET
				case AF_INET:
					col_host = YP_ACL_HOSTMASK_INET;
					break;
#endif
#ifdef AF_INET6
				case AF_INET6:
					col_host = YP_ACL_HOSTMASK_INET6;
					break;
#endif
				default:
					yp_error("line %d: host keyword for "
						 "unsupported address family: "
						 "%s: %s", line, linebuf,
						 gai_strerror(error));
					continue;
				}
			}
			memset(&hints, 0, sizeof(hints));
			hints.ai_family = PF_UNSPEC;
			hints.ai_socktype = SOCK_STREAM;
			hints.ai_flags = AI_NUMERICHOST;
			error = getaddrinfo(col_mask, NULL, &hints, &res0);
			if (error) {
				yp_error("line %d: "
				    "badly formatted securenets entry: "
				    "%s: %s", line, linebuf,
				    gai_strerror(error));
				freeaddrinfo(res0);
				free(snp);
				continue;
			}
			memcpy(&snp->sn_mask, res0->ai_addr, res0->ai_addrlen);
			freeaddrinfo(res0);
		} else if (nitems == 1) {
			/* ex. 127.0.0.1/8 */
			/* ex. fe80::/10 */
			if (debug)
				yp_error("load_securenets(): try CIDR expr");
			p = strrchr(addr1, '/');
			if (p != NULL) {
				*p = ' ';
				nitems = sscanf(addr1, "%s %d", addr2, &plen);
				if (nitems < 2) {
					yp_error("line %d: "
						 "badly formatted securenets entry:"
						 " %s", line, linebuf);
					free(snp);
					continue;
				}
			} else
				memcpy(addr2, addr1, sizeof(addr2));

			memset(&hints, 0, sizeof(hints));
			hints.ai_family = PF_UNSPEC;
			hints.ai_socktype = SOCK_STREAM;
			hints.ai_flags = AI_NUMERICHOST;
			error = getaddrinfo(addr2, NULL, &hints, &res0);
			if (error) {
				yp_error("line %d: "
				    "badly formatted securenets entry: "
				    "%s: %s", line, linebuf,
				    gai_strerror(error));
				freeaddrinfo(res0);
				free(snp);
				continue;
			}
			if (p == NULL)
				switch (res0->ai_addr->sa_family) {
#ifdef AF_INET
				case AF_INET:
					plen = 32;
					break;
#endif
#ifdef AF_INET6
				case AF_INET6:
					plen = 128;
					break;
#endif
				default:
					yp_error("line %d: "
						 "unsupported address family: "
						 "%s: %s", line, linebuf,
						 gai_strerror(error));
					continue;
				}
			if (debug) {
				yp_error("load_securenets(): addr2 = [%s]",
				    addr2);
				yp_error("load_securenets(): plen = [%d]",
				    plen);
			}
			memcpy(&snp->sn_addr, res0->ai_addr, res0->ai_addrlen);
			memcpy(&snp->sn_mask, res0->ai_addr, res0->ai_addrlen);
			freeaddrinfo(res0);

			sap = (struct sockaddr *)&(snp->sn_mask);
			prefixlen2mask(sap, &plen);
		} else {
			yp_error("line %d: "
			    "badly formatted securenets entry: "
			    "%s", line, linebuf);
			continue;
		}
		if (debug) {
			yp_error("load_securenets(): adding entry");
			yp_debug_sa((struct sockaddr*)&(snp->sn_addr));
			yp_debug_sa((struct sockaddr *)&(snp->sn_mask));
		}
		SLIST_INSERT_HEAD(&securenets, snp, sn_next);
	}
	fclose(fp);
	if (debug)
		show_securenets();
	return (0);
}

static int
compare_subnet(struct sockaddr *addr1,
	       struct sockaddr *addr2,
	       struct sockaddr *mask)
{
#ifdef AF_INET
	struct sockaddr_in *in_addr1;
	struct sockaddr_in *in_addr2;
	struct sockaddr_in *in_addrm;
#endif
#ifdef AF_INET6
	struct sockaddr_in6 *in6_addr1;
	struct sockaddr_in6 *in6_addr2;
	struct sockaddr_in6 *in6_addrm;
#endif
	u_char a1[sizeof(struct sockaddr_storage)];
	u_char a2[sizeof(struct sockaddr_storage)];
	u_char m[sizeof(struct sockaddr_storage)];
	ssize_t len;
	int i;
	int samescope;

	if (addr1->sa_family != addr2->sa_family)
		return (1);

	memset(a1, 0, sizeof(a1));
	memset(a1, 0, sizeof(a2));
	memset(m, 0, sizeof(m));

	switch (addr1->sa_family) {
#ifdef AF_INET
	case AF_INET:
		in_addr1 = (struct sockaddr_in *)(addr1);
		in_addr2 = (struct sockaddr_in *)(addr2);
		in_addrm = (struct sockaddr_in *)(mask);
		len = sizeof(in_addr1->sin_addr.s_addr);
		memcpy(a1, (u_char *)&(in_addr1->sin_addr.s_addr),
		    sizeof(in_addr1->sin_addr.s_addr));
		memcpy(a2, (u_char *)&(in_addr2->sin_addr.s_addr),
		    sizeof(in_addr2->sin_addr.s_addr));
		memcpy(m, (u_char *)&(in_addrm->sin_addr.s_addr),
		    sizeof(in_addrm->sin_addr.s_addr));
		samescope = 1;
		break;
#endif
#ifdef AF_INET6
	case AF_INET6:
		in6_addr1 = (struct sockaddr_in6 *)(addr1);
		in6_addr2 = (struct sockaddr_in6 *)(addr2);
		in6_addrm = (struct sockaddr_in6 *)(mask);
		len = sizeof(in6_addr1->sin6_addr.s6_addr);
		memcpy(a1, (u_char *)in6_addr1->sin6_addr.s6_addr,
		    sizeof(in6_addr1->sin6_addr.s6_addr));
		memcpy(a2, (u_char *)in6_addr2->sin6_addr.s6_addr,
		    sizeof(in6_addr2->sin6_addr.s6_addr));
		memcpy(m, (u_char *)in6_addrm->sin6_addr.s6_addr,
		    sizeof(in6_addrm->sin6_addr.s6_addr));
		samescope = (in6_addr1->sin6_scope_id ==
		    in6_addr2->sin6_scope_id);
		break;
#endif
	default:
		return (1);
	}
	for (i=0; i < len; i++) {
		a1[i] &= m[i];
		a2[i] &= m[i];
	}
	if (debug) {
		yp_error("compare_subnet(): addr1");
		yp_debug_sa(addr1);
		yp_error("compare_subnet(): addr2");
		yp_debug_sa(addr2);
		yp_error("compare_subnet(): mask");
		yp_debug_sa(mask);
	}
	if (!samescope)
		return (1);
	return (memcmp(a1, a2, len));
}

/*
 * Access control functions.
 *
 * yp_access() checks the mapname and client host address and watches for
 * the following things:
 *
 * - If the client is referencing one of the master.passwd.* or shadow.* maps,
 *   it must be using a privileged port to make its RPC to us. If it is, then
 *   we can assume that the caller is root and allow the RPC to succeed. If it
 *   isn't access is denied.
 *
 * - The client's IP address is checked against the securenets rules.
 *   There are two kinds of securenets support: the built-in support,
 *   which is very simple and depends on the presence of a
 *   /var/yp/securenets file, and tcp-wrapper support, which requires
 *   Wietse Venema's libwrap.a and tcpd.h. (Since the tcp-wrapper
 *   package does not ship with FreeBSD, we use the built-in support
 *   by default. Users can recompile the server with the tcp-wrapper library
 *   if they already have it installed and want to use hosts.allow and
 *   hosts.deny to control access instead of having a separate securenets
 *   file.)
 *
 *   If no /var/yp/securenets file is present, the host access checks
 *   are bypassed and all hosts are allowed to connect.
 *
 * The yp_validdomain() function checks the domain specified by the caller
 * to make sure it's actually served by this server. This is more a sanity
 * check than an a security check, but this seems to be the best place for
 * it.
 */

#ifdef DB_CACHE
int
yp_access(const char *map, const char *domain, const struct svc_req *rqstp)
#else
int
yp_access(const char *map, const struct svc_req *rqstp)
#endif
{
	int status_securenets = 0;
	int error;
#ifdef TCP_WRAPPER
	int status_tcpwrap;
#endif
	static struct sockaddr oldaddr;
	struct securenet *snp;
	struct sockaddr *sap;
	struct netbuf *rqhost;
	const char *yp_procedure = NULL;
	char procbuf[50];
	char host[NI_MAXHOST];
	char serv[NI_MAXSERV];

	memset(&oldaddr, 0, sizeof(oldaddr));

	if (rqstp->rq_prog != YPPASSWDPROG && rqstp->rq_prog != YPPROG) {
		snprintf(procbuf, sizeof(procbuf), "#%lu/#%lu",
		    (unsigned long)rqstp->rq_prog,
		    (unsigned long)rqstp->rq_proc);
		yp_procedure = (char *)&procbuf;
	} else {
		yp_procedure = rqstp->rq_prog == YPPASSWDPROG ?
		"yppasswdprog_update" :
		yp_procs[rqstp->rq_proc + (12 * (rqstp->rq_vers - 1))];
	}

	rqhost = svc_getrpccaller(rqstp->rq_xprt);
	sap = (struct sockaddr *)(rqhost->buf);
	error = getnameinfo(sap, sap->sa_len,
		    host, sizeof(host), serv, sizeof(serv),
		    NI_NUMERICHOST | NI_NUMERICSERV);
	if (error) {
		yp_error("yp_access: getnameinfo(): %s", gai_strerror(error));
		return (1);
	}
	if (debug) {
		yp_error("procedure %s called from %s:%s", yp_procedure,
		    host, serv);
		if (map != NULL)
			yp_error("client is referencing map \"%s\".", map);
	}

	/* Check the map name if one was supplied. */
	if (map != NULL) {
		if (strchr(map, '/')) {
			yp_error("embedded slash in map name \"%s\" "
			    "-- possible spoof attempt from %s:%s",
			    map, host, serv);
			return (1);
		}
#ifdef DB_CACHE
		if ((yp_testflag((const char *)map, (const char *)domain, YP_SECURE) ||
#else
		if ((strstr(map, "master.passwd.") || strstr(map, "shadow.") ||
#endif
		    (rqstp->rq_prog == YPPROG &&
		     rqstp->rq_proc == YPPROC_XFR) ||
		    (rqstp->rq_prog == YPXFRD_FREEBSD_PROG &&
		     rqstp->rq_proc == YPXFRD_GETMAP)) &&
		     atoi(serv) >= IPPORT_RESERVED) {
			yp_error("access to %s denied -- client %s:%s"
			    " not privileged", map, host, serv);
			return (1);
		}
	}

#ifdef TCP_WRAPPER
	status_tcpwrap = hosts_ctl("ypserv", STRING_UNKNOWN, host, "");
#endif
	if (debug)
		yp_error("yp_access(): compare start");
	SLIST_FOREACH (snp, &securenets, sn_next) {
		if (compare_subnet(sap,
		    (struct sockaddr *)&(snp->sn_addr),
		    (struct sockaddr *)&(snp->sn_mask)) == 0) {
			status_securenets = 1;
			if (debug)
				yp_error("yp_access(): compare success!");
			break;
		}
	}
	if (debug)
		yp_error("yp_access(): compare end");
#ifdef TCP_WRAPPER
	if (status_securenets == 0 || status_tcpwrap == 0) {
#else
	if (status_securenets == 0) {
#endif
	/*
	 * One of the following two events occured:
	 *
	 * (1) The /var/yp/securenets exists and the remote host does not
	 *     match any of the networks specified in it.
	 * (2) The hosts.allow file has denied access and TCP_WRAPPER is
	 *     defined.
	 *
	 * In either case deny access.
	 */
		if (memcmp(sap, &oldaddr, sizeof(oldaddr))) {
			yp_error("connect from %s:%s to procedure %s refused",
			     host, serv, yp_procedure);
			memcpy(&oldaddr, sap, sizeof(oldaddr));
		}
		return (1);
	}
	return (0);

}

int
yp_validdomain(const char *domain)
{
	struct stat statbuf;
	char dompath[MAXPATHLEN + 2];

	if (domain == NULL || strstr(domain, "binding") ||
	    !strcmp(domain, ".") || !strcmp(domain, "..") ||
	    strchr(domain, '/') || strlen(domain) > YPMAXDOMAIN)
		return (1);

	snprintf(dompath, sizeof(dompath), "%s/%s", yp_dir, domain);

	if (stat(dompath, &statbuf) < 0 || !S_ISDIR(statbuf.st_mode))
		return (1);

	return (0);
}
