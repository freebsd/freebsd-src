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

#include "yp_extern.h"
#include "yp.h"
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifndef lint
static char rcsid[] = "$Id: yp_server.c,v 1.18 1995/12/16 04:01:55 wpaul Exp $";
#endif /* not lint */

int forked = 0;
int children = 0;
DB *spec_dbp = NULL;	/* Special global DB handle for ypproc_all. */

void *
ypproc_null_2_svc(void *argp, struct svc_req *rqstp)
{
	static char * result;
	static char rval = 0;

	if (yp_access(NULL, (struct svc_req *)rqstp))
		return(NULL);

	result = &rval;

	return((void *) &result);
}

bool_t *
ypproc_domain_2_svc(domainname *argp, struct svc_req *rqstp)
{
	static bool_t  result;

	if (yp_access(NULL, (struct svc_req *)rqstp)) {
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

	if (yp_access(NULL, (struct svc_req *)rqstp))
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
	DBT key, data;

	if (yp_access(argp->map, (struct svc_req *)rqstp)) {
		result.stat = YP_YPERR;
		return (&result);
	}

	if (argp->domain == NULL || argp->map == NULL) {
		result.stat = YP_BADARGS;
		return (&result);
	}

	if (yp_validdomain(argp->domain)) {
		result.stat = YP_NODOM;
		return(&result);
	}

	key.size = argp->key.keydat_len;
	key.data = argp->key.keydat_val;

	result.stat = yp_get_record(argp->domain, argp->map, &key, &data, 0);

	if (result.stat == YP_TRUE) {
		result.val.valdat_len = data.size;
		result.val.valdat_val = data.data;
	}

	/*
	 * Do DNS lookups for hosts maps if database lookup failed.
	 */

	if (do_dns && result.stat != YP_TRUE && strstr(argp->map, "hosts")) {
		char *rval;

	/* DNS lookups can take time -- do them in a subprocess */

		if (!debug && children < MAX_CHILDREN && fork()) {
			children++;
			forked = 0;
			/*
			 * Returning NULL here prevents svc_sendreply()
			 * from being called by the parent. This is vital
			 * since having both the parent and the child process
			 * call it would confuse the client.
			 */
			return (NULL);
		} else {
			forked++;
		}

		if (debug)
			yp_error("Doing DNS lookup of %.*s",
			 	  argp->key.keydat_len,
				  argp->key.keydat_val);

		/* NUL terminate! NUL terminate!! NUL TERMINATE!!! */
		argp->key.keydat_val[argp->key.keydat_len] = '\0';

		if (!strcmp(argp->map, "hosts.byname"))
			rval = yp_dnsname((char *)argp->key.keydat_val);
		else if (!strcmp(argp->map, "hosts.byaddr"))
			rval = yp_dnsaddr((const char *)argp->key.keydat_val);


		if (rval) {
			if (debug)
				yp_error("DNS lookup successful. Result: %s", rval);
			result.val.valdat_len = strlen(rval);
			result.val.valdat_val = rval;
			result.stat = YP_TRUE;
		} else {
			if (debug)
				yp_error("DNS lookup failed.");
			result.stat = YP_NOKEY;
		}
	}

	return (&result);
}

ypresp_key_val *
ypproc_first_2_svc(ypreq_nokey *argp, struct svc_req *rqstp)
{
	static ypresp_key_val  result;
	DBT key, data;
	DB *dbp;

	if (yp_access(argp->map, (struct svc_req *)rqstp)) {
		result.stat = YP_YPERR;
		return (&result);
	}

	if (argp->domain == NULL) {
		result.stat = YP_BADARGS;
		return (&result);
	}

	if (yp_validdomain(argp->domain)) {
		result.stat = YP_NODOM;
		return(&result);
	}

	if ((dbp = yp_open_db(argp->domain, argp->map)) == NULL) {
		result.stat = yp_errno;
		return(&result);
	}

	key.data = NULL;
	key.size = 0;
	result.stat = yp_first_record(dbp, &key, &data);
	(void)(dbp->close)(dbp);

	if (result.stat == YP_TRUE) {
		result.key.keydat_len = key.size;
		result.key.keydat_val = key.data;
		result.val.valdat_len = data.size;
		result.val.valdat_val = data.data;
	}

	return (&result);
}

ypresp_key_val *
ypproc_next_2_svc(ypreq_key *argp, struct svc_req *rqstp)
{
	static ypresp_key_val  result;
	DBT key, data;
	DB *dbp;

	if (yp_access(argp->map, (struct svc_req *)rqstp)) {
		result.stat = YP_YPERR;
		return (&result);
	}

	if (argp->domain == NULL || argp->map == NULL) {
		result.stat = YP_BADARGS;
		return (&result);
	}

	if (yp_validdomain(argp->domain)) {
		result.stat = YP_NODOM;
		return(&result);
	}

	if ((dbp = yp_open_db(argp->domain, argp->map)) == NULL) {
		result.stat = yp_errno;
		return(&result);
	}

	key.size = argp->key.keydat_len;
	key.data = argp->key.keydat_val;

	result.stat = yp_next_record(dbp, &key, &data, 0);
	(void)(dbp->close)(dbp);

	if (result.stat == YP_TRUE) {
		result.key.keydat_len = key.size;
		result.key.keydat_val = key.data;
		result.val.valdat_len = data.size;
		result.val.valdat_val = data.data;
	}

	return (&result);
}

ypresp_xfr *
ypproc_xfr_2_svc(ypreq_xfr *argp, struct svc_req *rqstp)
{
	static ypresp_xfr  result;

	if (yp_access(argp->map_parms.map, (struct svc_req *)rqstp)) {
		result.xfrstat = YPXFR_REFUSED;
		return(&result);
	}

	if (argp->map_parms.domain == NULL) {
		result.xfrstat = YPXFR_BADARGS;
		return (&result);
	}

	if (yp_validdomain(argp->map_parms.domain)) {
		result.xfrstat = YPXFR_NODOM;
		return(&result);
	}

	switch(fork()) {
	case 0:
	{
		char g[11], t[11], p[11];
		struct sockaddr_in *rqhost;
		char ypxfr_command[MAXPATHLEN + 2];

		rqhost = svc_getcaller(rqstp->rq_xprt);
		sprintf (ypxfr_command, "%sypxfr", _PATH_LIBEXEC);
		sprintf (t, "%u", argp->transid);
		sprintf (g, "%u", argp->prog);
		sprintf (p, "%u", argp->port);
		children++;
		forked = 0;
		execl(ypxfr_command, "ypxfr", "-d", argp->map_parms.domain,
		      "-h", argp->map_parms.peer, "-f", "-C", t, g,
		      inet_ntoa(rqhost->sin_addr), p, argp->map_parms.map,
		      NULL);
		yp_error("ypxfr execl(): %s", strerror(errno));
		return(NULL);
	}
	case -1:
		yp_error("ypxfr fork(): %s", strerror(errno));
		result.xfrstat = YPXFR_XFRERR;
		break;
	default:
		result.xfrstat = YPXFR_SUCC;
		forked++;
		break;
	}

	result.transid = argp->transid;
	return (&result);
}

void *
ypproc_clear_2_svc(void *argp, struct svc_req *rqstp)
{
	static char * result;
	static char rval = 0;

	/*
	 * We don't have to do anything for ypproc_clear. Unlike
	 * the SunOS ypserv, we don't hold out database descriptors
	 * open forever.
	 */
	if (yp_access(NULL, (struct svc_req *)rqstp))
		return (NULL);

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
	DBT key, data;

	while (1) {
		/* Get a record. */
		key.size = objp->ypresp_all_u.val.key.keydat_len;
		key.data = objp->ypresp_all_u.val.key.keydat_val;

		if ((objp->ypresp_all_u.val.stat =
		    yp_next_record(spec_dbp,&key,&data,1)) == YP_TRUE) {
			objp->ypresp_all_u.val.val.valdat_len = data.size;
			objp->ypresp_all_u.val.val.valdat_val = data.data;
			objp->ypresp_all_u.val.key.keydat_len = key.size;
			objp->ypresp_all_u.val.key.keydat_val = key.data;
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

	if (yp_access(argp->map, (struct svc_req *)rqstp)) {
		result.ypresp_all_u.val.stat = YP_YPERR;
		return (&result);
	}

	if (argp->domain == NULL || argp->map == NULL) {
		result.ypresp_all_u.val.stat = YP_BADARGS;
		return (&result);
	}

	if (yp_validdomain(argp->domain)) {
		result.ypresp_all_u.val.stat = YP_NODOM;
		return(&result);
	}

	/*
	 * The ypproc_all procedure can take a while to complete.
	 * Best to handle it in a subprocess so the parent doesn't
	 * block. We fork() here so we don't end up sharing a
	 * DB file handle with the parent.
	 */

	if (!debug && children < MAX_CHILDREN && fork()) {
		children++;
		forked = 0;
		return (NULL);
	} else {
		forked++;
	}

	if ((spec_dbp = yp_open_db(argp->domain, argp->map)) == NULL) {
		result.ypresp_all_u.val.stat = yp_errno;
		return(&result);
	}

	/* Kick off the actual data transfer. */
	svc_sendreply(rqstp->rq_xprt, xdr_my_ypresp_all, (char *)&result);

	/* Close database when done. */
	(void)(spec_dbp->close)(spec_dbp);

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
	DBT key,data;

	if (yp_access(NULL, (struct svc_req *)rqstp)) {
		result.stat = YP_YPERR;
		return(&result);
	}

	if (argp->domain == NULL) {
		result.stat = YP_BADARGS;
		return (&result);
	}
		
	if (yp_validdomain(argp->domain)) {
		result.stat = YP_NODOM;
		return (&result);
	}

	key.data = "YP_MASTER_NAME";
	key.size = sizeof("YP_MASTER_NAME") - 1;

	result.stat = yp_get_record(argp->domain, argp->map, &key, &data, 1);

	if (result.stat == YP_TRUE) {
		result.peer = (char *)data.data;
		result.peer[data.size] = '\0';
	} else
		result.peer = "";

	return (&result);
}

ypresp_order *
ypproc_order_2_svc(ypreq_nokey *argp, struct svc_req *rqstp)
{
	static ypresp_order  result;
	DBT key,data;

	if (yp_access(NULL, (struct svc_req *)rqstp)) {
		result.stat = YP_YPERR;
		return(&result);
	}

	if (argp->domain == NULL) {
		result.stat = YP_BADARGS;
		return (&result);
	}
		
	if (yp_validdomain(argp->domain)) {
		result.stat = YP_NODOM;
		return (&result);
	}

	/*
	 * We could just check the timestamp on the map file,
	 * but that's a hack: we'll only know the last time the file
	 * was touched, not the last time the database contents were
	 * updated.
	 */
	key.data = "YP_LAST_MODIFIED";
	key.size = sizeof("YP_LAST_MODIFIED") - 1;

	result.stat = yp_get_record(argp->domain, argp->map, &key, &data, 1);

	if (result.stat == YP_TRUE)
		result.ordernum = atoi((char *)data.data);
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
		yp_error("opendir(%s) failed: %s", strerror(errno));
		return(NULL);
	}

	while ((dirp = readdir(dird)) != NULL) {
		if (strcmp(dirp->d_name, ".") && strcmp(dirp->d_name, "..")) {
			snprintf(yp_mapname, sizeof(yp_mapname), "%s/%s",yp_mapdir,dirp->d_name);
			if (stat(yp_mapname, &statbuf) < 0 || !S_ISREG(statbuf.st_mode))
				continue;
			if ((cur = (struct ypmaplist *)malloc(sizeof(struct ypmaplist))) < 0) {
				yp_error("malloc() failed: %s", strerror(errno));
				closedir(dird);
				yp_maplist_free(yp_maplist);
				return(NULL);
			}
			if ((cur->map = (char *)strdup(dirp->d_name)) == NULL) {
				yp_error("strdup() failed: %s", strerror(errno));
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
	static ypresp_maplist  result;

	if (yp_access(NULL, (struct svc_req *)rqstp)) {
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
