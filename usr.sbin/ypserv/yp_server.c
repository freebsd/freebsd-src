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

#include "yp.h"
#include "yp_extern.h"
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <rpc/rpc.h>

#ifndef lint
static const char rcsid[] = "$Id: yp_server.c,v 1.5 1997/03/15 06:06:06 wpaul Exp wpaul $";
#endif /* not lint */

int forked = 0;
int children = 0;
static char *master_string = "YP_MASTER_NAME";
static char *order_string = "YP_LAST_MODIFIED";
static int master_sz = sizeof("YP_MASTER_NAME") - 1;
static int order_sz = sizeof("YP_LAST_MODIFIED") - 1;

/*
 * NIS v2 support. This is where most of the action happens.
 */

void *
ypproc_null_2_svc(void *argp, struct svc_req *rqstp)
{
	static char * result;
	static char rval = 0;

#ifdef DB_CACHE
	if (yp_access(NULL, NULL, (struct svc_req *)rqstp))
#else
	if (yp_access(NULL, (struct svc_req *)rqstp))
#endif
		return(NULL);

	result = &rval;

	return((void *) &result);
}

bool_t *
ypproc_domain_2_svc(domainname *argp, struct svc_req *rqstp)
{
	static bool_t  result;

#ifdef DB_CACHE
	if (yp_access(NULL, NULL, (struct svc_req *)rqstp)) {
#else
	if (yp_access(NULL, (struct svc_req *)rqstp)) {
#endif
		result = FALSE;
		return (&result);
	}

	if (argp == NULL || yp_validdomain(*argp))
		result = FALSE;
	else
		result = TRUE;

	return (&result);
}

bool_t *
ypproc_domain_nonack_2_svc(domainname *argp, struct svc_req *rqstp)
{
	static bool_t  result;

#ifdef DB_CACHE
	if (yp_access(NULL, NULL, (struct svc_req *)rqstp))
#else
	if (yp_access(NULL, (struct svc_req *)rqstp))
#endif
		return (NULL);

	if (argp == NULL || yp_validdomain(*argp))
		return (NULL);
	else
		result = TRUE;

	return (&result);
}

ypresp_val *
ypproc_match_2_svc(ypreq_key *argp, struct svc_req *rqstp)
{
	static ypresp_val  result;

	result.val.valdat_val = "";
	result.val.valdat_len = 0;

#ifdef DB_CACHE
	if (yp_access(argp->map, argp->domain, (struct svc_req *)rqstp)) {
#else
	if (yp_access(argp->map, (struct svc_req *)rqstp)) {
#endif
		result.stat = YP_YPERR;
		return (&result);
	}

	if (argp->domain == NULL || argp->map == NULL) {
		result.stat = YP_BADARGS;
		return (&result);
	}

	if (yp_select_map(argp->map, argp->domain, &argp->key, 1) != YP_TRUE) {
		result.stat = yp_errno;
		return(&result);
	}

	result.stat = yp_getbykey(&argp->key, &result.val);

	/*
	 * Do DNS lookups for hosts maps if database lookup failed.
	 */

#ifdef DB_CACHE
	if (result.stat != YP_TRUE &&
	    (yp_testflag(argp->map, argp->domain, YP_INTERDOMAIN) ||
	    (strstr(argp->map, "hosts") && do_dns))) {
#else
	if (do_dns && result.stat != YP_TRUE && strstr(argp->map, "hosts")) {
#endif
	
		/* NUL terminate! NUL terminate!! NUL TERMINATE!!! */
		argp->key.keydat_val[argp->key.keydat_len] = '\0';

		if (debug)
			yp_error("Doing DNS lookup of %.*s",
			 	  argp->key.keydat_len,
				  argp->key.keydat_val);

		if (!strcmp(argp->map, "hosts.byname"))
			result.stat = yp_async_lookup_name(rqstp,
					(char *)argp->key.keydat_val);
		else if (!strcmp(argp->map, "hosts.byaddr"))
			result.stat = yp_async_lookup_addr(rqstp,
					(char *)argp->key.keydat_val);

		if (result.stat == YP_TRUE)
			return(NULL);
	}

	return (&result);
}

ypresp_key_val *
ypproc_first_2_svc(ypreq_nokey *argp, struct svc_req *rqstp)
{
	static ypresp_key_val  result;

	result.val.valdat_val = result.key.keydat_val = "";
	result.val.valdat_len = result.key.keydat_len = 0;

#ifdef DB_CACHE
	if (yp_access(argp->map, argp->domain, (struct svc_req *)rqstp)) {
#else
	if (yp_access(argp->map, (struct svc_req *)rqstp)) {
#endif
		result.stat = YP_YPERR;
		return (&result);
	}

	if (argp->domain == NULL) {
		result.stat = YP_BADARGS;
		return (&result);
	}

	if (yp_select_map(argp->map, argp->domain, &result.key, 0) != YP_TRUE) {
		result.stat = yp_errno;
		return(&result);
	}

	result.stat = yp_firstbykey(&result.key, &result.val);

	return (&result);
}

ypresp_key_val *
ypproc_next_2_svc(ypreq_key *argp, struct svc_req *rqstp)
{
	static ypresp_key_val  result;

	result.val.valdat_val = result.key.keydat_val = "";
	result.val.valdat_len = result.key.keydat_len = 0;

#ifdef DB_CACHE
	if (yp_access(argp->map, argp->domain, (struct svc_req *)rqstp)) {
#else
	if (yp_access(argp->map, (struct svc_req *)rqstp)) {
#endif
		result.stat = YP_YPERR;
		return (&result);
	}

	if (argp->domain == NULL || argp->map == NULL) {
		result.stat = YP_BADARGS;
		return (&result);
	}

	if (yp_select_map(argp->map, argp->domain, &argp->key, 0) != YP_TRUE) {
		result.stat = yp_errno;
		return(&result);
	}

	result.key.keydat_len = argp->key.keydat_len;
	result.key.keydat_val = argp->key.keydat_val;

	result.stat = yp_nextbykey(&result.key, &result.val);

	return (&result);
}

static void ypxfr_callback(rval,addr,transid,prognum,port)
	ypxfrstat rval;
	struct sockaddr_in *addr;
	unsigned int transid;
	unsigned int prognum;
	unsigned long port;
{
	CLIENT *clnt;
	int sock = RPC_ANYSOCK;
	struct timeval timeout;
	yppushresp_xfr ypxfr_resp;
	struct rpc_err err;

	timeout.tv_sec = 5;
	timeout.tv_usec = 0;
	addr->sin_port = htons(port);

	if ((clnt = clntudp_create(addr,prognum,1,timeout,&sock)) == NULL) {
		yp_error("%s: %s", inet_ntoa(addr->sin_addr),
		  clnt_spcreateerror("failed to establish callback handle"));
		return;
	}

	ypxfr_resp.status = rval;
	ypxfr_resp.transid = transid;

	/* Turn the timeout off -- we don't want to block. */
	timeout.tv_sec = 0;
	if (clnt_control(clnt, CLSET_TIMEOUT, (char *)&timeout) == FALSE)
		yp_error("failed to set timeout on ypproc_xfr callback");

	if (yppushproc_xfrresp_1(&ypxfr_resp, clnt) == NULL) {
		clnt_geterr(clnt, &err);
		if (err.re_status != RPC_SUCCESS &&
		    err.re_status != RPC_TIMEDOUT)
			yp_error("%s", clnt_sperror(clnt,
				"ypxfr callback failed"));
	}

	clnt_destroy(clnt);
	return;
}

#define YPXFR_RETURN(CODE) 						\
	/* Order is important: send regular RPC reply, then callback */	\
	result.xfrstat = CODE; 						\
	svc_sendreply(rqstp->rq_xprt, xdr_ypresp_xfr, (char *)&result); \
	ypxfr_callback(CODE,rqhost,argp->transid, 			\
					argp->prog,argp->port); 	\
	return(NULL);

ypresp_xfr *
ypproc_xfr_2_svc(ypreq_xfr *argp, struct svc_req *rqstp)
{
	static ypresp_xfr  result;
	struct sockaddr_in *rqhost;
	ypresp_master *mres;
	ypreq_nokey mreq;

	result.transid = argp->transid;
	rqhost = svc_getcaller(rqstp->rq_xprt);

	/*
	 * The FreeBSD ypxfr(8) program will not talk to a ypserv(8)
	 * or rpc.ypxfrd(8) unless it's using a reserved port, but we
 	 * may as well check that the process calling this procedure
	 * is also using one.
	 *
	 * yp_access() may also do a reserved port test, but only
	 * if the YP_SECURE flag is set in the map. For this procedure,
	 * the check should be unconditional.
	 */
	if (ntohs(rqhost->sin_port) >= IPPORT_RESERVED) {
		yp_error("ypxfr request from non-reserved port (%s:%d) -- \
rejecting", inet_ntoa(rqhost->sin_addr), ntohs(rqhost->sin_port));
		YPXFR_RETURN(YPXFR_REFUSED)
	}

#ifdef DB_CACHE
	if (yp_access(argp->map_parms.map,
			argp->map_parms.domain, (struct svc_req *)rqstp)) {
#else
	if (yp_access(argp->map_parms.map, (struct svc_req *)rqstp)) {
#endif
		YPXFR_RETURN(YPXFR_REFUSED)
	}


	if (argp->map_parms.domain == NULL) {
		YPXFR_RETURN(YPXFR_BADARGS)
	}

	if (yp_validdomain(argp->map_parms.domain)) {
		YPXFR_RETURN(YPXFR_NODOM)
	}

	/*
	 * Determine the master host ourselves. The caller may
	 * be up to no good. This has the side effect of verifying
	 * that the requested map and domain actually exist.
	 */

	mreq.domain = argp->map_parms.domain;
	mreq.map = argp->map_parms.map;

	mres = ypproc_master_2_svc(&mreq, rqstp);

	if (mres->stat != YP_TRUE) {
		yp_error("couldn't find master for map %s@%s",
						argp->map_parms.map,
						argp->map_parms.domain);
		yp_error("host at %s (%s) may be pulling my leg",
						argp->map_parms.peer,
						inet_ntoa(rqhost->sin_addr));
		YPXFR_RETURN(YPXFR_REFUSED)
	}

	switch(fork()) {
	case 0:
	{
		char g[11], t[11], p[11];
		char ypxfr_command[MAXPATHLEN + 2];

		sprintf (ypxfr_command, "%sypxfr", _PATH_LIBEXEC);
		sprintf (t, "%u", argp->transid);
		sprintf (g, "%u", argp->prog);
		sprintf (p, "%u", argp->port);
		if (debug) {
			close(0); close(1); close(2);
		}
		if (strcmp(yp_dir, _PATH_YP)) {
			execl(ypxfr_command, "ypxfr",
			"-d", argp->map_parms.domain,
		      	"-h", mres->peer,
			"-p", yp_dir, "-C", t,
		      	g, inet_ntoa(rqhost->sin_addr),
			p, argp->map_parms.map,
		      	NULL);
		} else {
			execl(ypxfr_command, "ypxfr",
			"-d", argp->map_parms.domain,
		      	"-h", mres->peer,
			"-C", t,
		      	g, inet_ntoa(rqhost->sin_addr),
			p, argp->map_parms.map,
		      	NULL);
		}
		forked++;
		yp_error("ypxfr execl(%s): %s", ypxfr_command, strerror(errno));
		YPXFR_RETURN(YPXFR_XFRERR)
		break;
	}
	case -1:
		yp_error("ypxfr fork(): %s", strerror(errno));
		YPXFR_RETURN(YPXFR_XFRERR)
		break;
	default:
		result.xfrstat = YPXFR_SUCC;
		children++;
		forked = 0;
		break;
	}

	return (&result);
}
#undef YPXFR_RETURN

void *
ypproc_clear_2_svc(void *argp, struct svc_req *rqstp)
{
	static char * result;
	static char rval = 0;

#ifdef DB_CACHE
	if (yp_access(NULL, NULL, (struct svc_req *)rqstp))
#else
	if (yp_access(NULL, (struct svc_req *)rqstp))
#endif
		return (NULL);
#ifdef DB_CACHE
	/* clear out the database cache */
	yp_flush_all();
#endif
	/* Re-read the securenets database for the hell of it. */
	load_securenets();

	result = &rval;
	return((void *) &result);
}

/*
 * For ypproc_all, we have to send a stream of ypresp_all structures
 * via TCP, but the XDR filter generated from the yp.x protocol
 * definition file only serializes one such structure. This means that
 * to send the whole stream, you need a wrapper which feeds all the
 * records into the underlying XDR routine until it hits an 'EOF.'
 * But to use the wrapper, you have to violate the boundaries between
 * RPC layers by calling svc_sendreply() directly from the ypproc_all
 * service routine instead of letting the RPC dispatcher do it.
 *
 * Bleah.
 */

/*
 * Custom XDR routine for serialzing results of ypproc_all: keep
 * reading from the database and spew until we run out of records
 * or encounter an error.
 */
static bool_t
xdr_my_ypresp_all(register XDR *xdrs, ypresp_all *objp)
{
	while (1) {
		/* Get a record. */
		if ((objp->ypresp_all_u.val.stat =
			yp_nextbykey(&objp->ypresp_all_u.val.key,
				     &objp->ypresp_all_u.val.val)) == YP_TRUE) {
			objp->more = TRUE;
		} else {
			objp->more = FALSE;
		}

		/* Serialize. */
		if (!xdr_ypresp_all(xdrs, objp))
			return(FALSE);
		if (objp->more == FALSE)
			return(TRUE);
	}
}

ypresp_all *
ypproc_all_2_svc(ypreq_nokey *argp, struct svc_req *rqstp)
{
	static ypresp_all  result;

	/*
	 * Set this here so that the client will be forced to make
	 * at least one attempt to read from us even if all we're
	 * doing is returning an error.
	 */
	result.more = TRUE;
	result.ypresp_all_u.val.key.keydat_len = 0;
	result.ypresp_all_u.val.key.keydat_val = "";

#ifdef DB_CACHE
	if (yp_access(argp->map, argp->domain, (struct svc_req *)rqstp)) {
#else
	if (yp_access(argp->map, (struct svc_req *)rqstp)) {
#endif
		result.ypresp_all_u.val.stat = YP_YPERR;
		return (&result);
	}

	if (argp->domain == NULL || argp->map == NULL) {
		result.ypresp_all_u.val.stat = YP_BADARGS;
		return (&result);
	}

	/*
	 * XXX If we hit the child limit, fail the request.
	 * If we don't, and the map is large, we could block for
	 * a long time in the parent.
	 */
	if (children >= MAX_CHILDREN) {
		result.ypresp_all_u.val.stat = YP_YPERR;
		return(&result);
	}

	/*
	 * The ypproc_all procedure can take a while to complete.
	 * Best to handle it in a subprocess so the parent doesn't
	 * block. (Is there a better way to do this? Maybe with
	 * async socket I/O?)
	 */
	if (!debug && children < MAX_CHILDREN && fork()) {
		children++;
		forked = 0;
		return (NULL);
	} else {
		forked++;
	}

	if (yp_select_map(argp->map, argp->domain,
				&result.ypresp_all_u.val.key, 0) != YP_TRUE) {
		result.ypresp_all_u.val.stat = yp_errno;
		return(&result);
	}

	/* Kick off the actual data transfer. */
	svc_sendreply(rqstp->rq_xprt, xdr_my_ypresp_all, (char *)&result);

	/*
	 * Returning NULL prevents the dispatcher from calling
	 * svc_sendreply() since we already did it.
	 */
	return (NULL);
}

ypresp_master *
ypproc_master_2_svc(ypreq_nokey *argp, struct svc_req *rqstp)
{
	static ypresp_master  result;
	static char ypvalbuf[YPMAXRECORD];
	keydat key = { master_sz, master_string };
	valdat val;

	result.peer = "";

#ifdef DB_CACHE
	if (yp_access(argp->map, argp->domain, (struct svc_req *)rqstp)) {
#else
	if (yp_access(argp->map, (struct svc_req *)rqstp)) {
#endif
		result.stat = YP_YPERR;
		return(&result);
	}

	if (argp->domain == NULL) {
		result.stat = YP_BADARGS;
		return (&result);
	}

	if (yp_select_map(argp->map, argp->domain, &key, 1) != YP_TRUE) {
		result.stat = yp_errno;
		return(&result);
	}

	/*
	 * Note that we copy the data retrieved from the database to
	 * a private buffer and NUL terminate the buffer rather than
	 * terminating the data in place. We do this because by stuffing
	 * a '\0' into data.data, we will actually be corrupting memory
	 * allocated by the DB package. This is a bad thing now that we
	 * cache DB handles rather than closing the database immediately.
	 */
	result.stat = yp_getbykey(&key, &val);
	if (result.stat == YP_TRUE) {
		bcopy((char *)val.valdat_val, (char *)&ypvalbuf,
						val.valdat_len);
		ypvalbuf[val.valdat_len] = '\0';
		result.peer = (char *)&ypvalbuf;
	} else
		result.peer = "";

	return (&result);
}

ypresp_order *
ypproc_order_2_svc(ypreq_nokey *argp, struct svc_req *rqstp)
{
	static ypresp_order  result;
	keydat key = { order_sz, order_string };
	valdat val;

	result.ordernum = 0;

#ifdef DB_CACHE
	if (yp_access(argp->map, argp->domain, (struct svc_req *)rqstp)) {
#else
	if (yp_access(argp->map, (struct svc_req *)rqstp)) {
#endif
		result.stat = YP_YPERR;
		return(&result);
	}

	if (argp->domain == NULL) {
		result.stat = YP_BADARGS;
		return (&result);
	}
		
	/*
	 * We could just check the timestamp on the map file,
	 * but that's a hack: we'll only know the last time the file
	 * was touched, not the last time the database contents were
	 * updated.
	 */

	if (yp_select_map(argp->map, argp->domain, &key, 1) != YP_TRUE) {
		result.stat = yp_errno;
		return(&result);
	}

	result.stat = yp_getbykey(&key, &val);

	if (result.stat == YP_TRUE)
		result.ordernum = atoi((char *)val.valdat_val);
	else
		result.ordernum = 0;

	return (&result);
}

static void yp_maplist_free(yp_maplist)
	struct ypmaplist *yp_maplist;
{
	register struct ypmaplist *next;

	while(yp_maplist) {
		next = yp_maplist->next;
		free(yp_maplist->map);
		free(yp_maplist);
		yp_maplist = next;
	}
	return;
}

static struct ypmaplist *yp_maplist_create(domain)
	const char *domain;
{
	char yp_mapdir[MAXPATHLEN + 2];
	char yp_mapname[MAXPATHLEN + 2];
	struct ypmaplist *cur = NULL;
	struct ypmaplist *yp_maplist = NULL;
	DIR *dird;
	struct dirent *dirp;
	struct stat statbuf;

	snprintf(yp_mapdir, sizeof(yp_mapdir), "%s/%s", yp_dir, domain);

	if ((dird = opendir(yp_mapdir)) == NULL) {
		yp_error("opendir(%s) failed: %s", yp_mapdir, strerror(errno));
		return(NULL);
	}

	while ((dirp = readdir(dird)) != NULL) {
		if (strcmp(dirp->d_name, ".") && strcmp(dirp->d_name, "..")) {
			snprintf(yp_mapname, sizeof(yp_mapname), "%s/%s",
							yp_mapdir,dirp->d_name);
			if (stat(yp_mapname, &statbuf) < 0 ||
						!S_ISREG(statbuf.st_mode))
				continue;
			if ((cur = (struct ypmaplist *)
				malloc(sizeof(struct ypmaplist))) == NULL) {
				yp_error("malloc() failed: %s",strerror(errno));
				closedir(dird);
				yp_maplist_free(yp_maplist);
				return(NULL);
			}
			if ((cur->map = (char *)strdup(dirp->d_name)) == NULL) {
				yp_error("strdup() failed: %s",strerror(errno));
				closedir(dird);
				yp_maplist_free(yp_maplist);
				return(NULL);
			}
			cur->next = yp_maplist;
			yp_maplist = cur;
			if (debug)
				yp_error("map: %s", yp_maplist->map);
		}

	}
	closedir(dird);
	return(yp_maplist);
}

ypresp_maplist *
ypproc_maplist_2_svc(domainname *argp, struct svc_req *rqstp)
{
	static ypresp_maplist  result = { 0, NULL };

#ifdef DB_CACHE
	if (yp_access(NULL, NULL, (struct svc_req *)rqstp)) {
#else
	if (yp_access(NULL, (struct svc_req *)rqstp)) {
#endif
		result.stat = YP_YPERR;
		return(&result);
	}

	if (argp == NULL) {
		result.stat = YP_BADARGS;
		return (&result);
	}
		
	if (yp_validdomain(*argp)) {
		result.stat = YP_NODOM;
		return (&result);
	}

	/*
	 * We have to construct a linked list for the ypproc_maplist
	 * procedure using dynamically allocated memory. Since the XDR
	 * layer won't free this list for us, we have to deal with it
	 * ourselves. We call yp_maplist_free() first to free any
	 * previously allocated data we may have accumulated to insure
	 * that we have only one linked list in memory at any given
	 * time.
	 */

	yp_maplist_free(result.maps);

	if ((result.maps = yp_maplist_create(*argp)) == NULL) {
		yp_error("yp_maplist_create failed");
		result.stat = YP_YPERR;
		return(&result);
	} else
		result.stat = YP_TRUE;

	return (&result);
}

/*
 * NIS v1 support. The nullproc, domain and domain_nonack
 * functions from v1 are identical to those in v2, so all
 * we have to do is hand off to them.
 *
 * The other functions are mostly just wrappers around their v2
 * counterparts. For example, for the v1 'match' procedure, we
 * crack open the argument structure, make a request to the v2
 * 'match' function, repackage the data into a v1 response and
 * then send it on its way.
 *
 * Note that we don't support the pull, push and get procedures.
 * There's little documentation available to show what they
 * do, and I suspect they're meant largely for map transfers
 * between master and slave servers.
 */

void *
ypoldproc_null_1_svc(void *argp, struct svc_req *rqstp)
{
	return(ypproc_null_2_svc(argp, rqstp));
}

bool_t *
ypoldproc_domain_1_svc(domainname *argp, struct svc_req *rqstp)
{
	return(ypproc_domain_2_svc(argp, rqstp));
}

bool_t *
ypoldproc_domain_nonack_1_svc(domainname *argp, struct svc_req *rqstp)
{
	return (ypproc_domain_nonack_2_svc(argp, rqstp));
}

/*
 * the 'match' procedure sends a response of type YPRESP_VAL
 */
ypresponse *
ypoldproc_match_1_svc(yprequest *argp, struct svc_req *rqstp)
{
	static ypresponse  result;
	ypresp_val *v2_result;

	result.yp_resptype = YPRESP_VAL;
	result.ypresponse_u.yp_resp_valtype.val.valdat_val = "";
	result.ypresponse_u.yp_resp_valtype.val.valdat_len = 0;

	if (argp->yp_reqtype != YPREQ_KEY) {
		result.ypresponse_u.yp_resp_valtype.stat = YP_BADARGS;
		return(&result);
	}

	v2_result = ypproc_match_2_svc(&argp->yprequest_u.yp_req_keytype,rqstp);
	if (v2_result == NULL)
		return(NULL);

	bcopy((char *)v2_result,
	      (char *)&result.ypresponse_u.yp_resp_valtype,
	      sizeof(ypresp_val));

	return (&result);
}

/*
 * the 'first' procedure sends a response of type YPRESP_KEY_VAL
 */
ypresponse *
ypoldproc_first_1_svc(yprequest *argp, struct svc_req *rqstp)
{
	static ypresponse  result;
	ypresp_key_val *v2_result;

	result.yp_resptype = YPRESP_KEY_VAL;
	result.ypresponse_u.yp_resp_key_valtype.val.valdat_val =
	result.ypresponse_u.yp_resp_key_valtype.key.keydat_val = "";
	result.ypresponse_u.yp_resp_key_valtype.val.valdat_len =
	result.ypresponse_u.yp_resp_key_valtype.key.keydat_len = 0;

	if (argp->yp_reqtype != YPREQ_NOKEY) {
		result.ypresponse_u.yp_resp_key_valtype.stat = YP_BADARGS;
		return(&result);
	}

	v2_result = ypproc_first_2_svc(&argp->yprequest_u.yp_req_nokeytype,
									rqstp);
	if (v2_result == NULL)
		return(NULL);

	bcopy((char *)v2_result,
	      (char *)&result.ypresponse_u.yp_resp_key_valtype,
	      sizeof(ypresp_key_val));

	return (&result);
}

/*
 * the 'next' procedure sends a response of type YPRESP_KEY_VAL
 */
ypresponse *
ypoldproc_next_1_svc(yprequest *argp, struct svc_req *rqstp)
{
	static ypresponse  result;
	ypresp_key_val *v2_result;

	result.yp_resptype = YPRESP_KEY_VAL;
	result.ypresponse_u.yp_resp_key_valtype.val.valdat_val =
	result.ypresponse_u.yp_resp_key_valtype.key.keydat_val = "";
	result.ypresponse_u.yp_resp_key_valtype.val.valdat_len =
	result.ypresponse_u.yp_resp_key_valtype.key.keydat_len = 0;

	if (argp->yp_reqtype != YPREQ_KEY) {
		result.ypresponse_u.yp_resp_key_valtype.stat = YP_BADARGS;
		return(&result);
	}

	v2_result = ypproc_next_2_svc(&argp->yprequest_u.yp_req_keytype,rqstp);
	if (v2_result == NULL)
		return(NULL);

	bcopy((char *)v2_result,
	      (char *)&result.ypresponse_u.yp_resp_key_valtype,
	      sizeof(ypresp_key_val));

	return (&result);
}

/*
 * the 'poll' procedure sends a response of type YPRESP_MAP_PARMS
 */
ypresponse *
ypoldproc_poll_1_svc(yprequest *argp, struct svc_req *rqstp)
{
	static ypresponse  result;
	ypresp_master *v2_result1;
	ypresp_order *v2_result2;

	result.yp_resptype = YPRESP_MAP_PARMS;
	result.ypresponse_u.yp_resp_map_parmstype.domain =
		argp->yprequest_u.yp_req_nokeytype.domain;
	result.ypresponse_u.yp_resp_map_parmstype.map =
		argp->yprequest_u.yp_req_nokeytype.map;
	/*
	 * Hmm... there is no 'status' value in the
	 * yp_resp_map_parmstype structure, so I have to
	 * guess at what to do to indicate a failure.
	 * I hope this is right.
	 */
	result.ypresponse_u.yp_resp_map_parmstype.ordernum = 0;
	result.ypresponse_u.yp_resp_map_parmstype.peer = "";

	if (argp->yp_reqtype != YPREQ_MAP_PARMS) {
		return(&result);
	}

	v2_result1 = ypproc_master_2_svc(&argp->yprequest_u.yp_req_nokeytype,
									rqstp);
	if (v2_result1 == NULL)
		return(NULL);

	if (v2_result1->stat != YP_TRUE) {
		return(&result);
	}

	v2_result2 = ypproc_order_2_svc(&argp->yprequest_u.yp_req_nokeytype,
									rqstp);
	if (v2_result2 == NULL)
		return(NULL);

	if (v2_result2->stat != YP_TRUE) {
		return(&result);
	}

	result.ypresponse_u.yp_resp_map_parmstype.peer =
		v2_result1->peer;
	result.ypresponse_u.yp_resp_map_parmstype.ordernum =
		v2_result2->ordernum;

	return (&result);
}

ypresponse *
ypoldproc_push_1_svc(yprequest *argp, struct svc_req *rqstp)
{
	static ypresponse  result;

	/*
	 * Not implemented.
	 */

	return (&result);
}

ypresponse *
ypoldproc_pull_1_svc(yprequest *argp, struct svc_req *rqstp)
{
	static ypresponse  result;

	/*
	 * Not implemented.
	 */

	return (&result);
}

ypresponse *
ypoldproc_get_1_svc(yprequest *argp, struct svc_req *rqstp)
{
	static ypresponse  result;

	/*
	 * Not implemented.
	 */

	return (&result);
}
