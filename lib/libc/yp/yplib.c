/*
 * Copyright (c) 1992/3 Theo de Raadt <deraadt@fsa.ca>
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
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef LINT
static char *rcsid = "$Id: yplib.c,v 1.20 1996/06/01 05:08:31 wpaul Exp $";
#endif

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <rpc/rpc.h>
#include <rpc/xdr.h>
#include <rpcsvc/yp.h>


/*
 * We have to define these here due to clashes between yp_prot.h and
 * yp.h.
 */

struct dom_binding {
	struct dom_binding *dom_pnext;
	char dom_domain[YPMAXDOMAIN + 1];
	struct sockaddr_in dom_server_addr;
	u_short dom_server_port;
	int dom_socket;
	CLIENT *dom_client;
	u_short dom_local_port;	/* now I finally know what this is for. */
	long dom_vers;
};

#include <rpcsvc/ypclnt.h>

#ifndef BINDINGDIR
#define BINDINGDIR "/var/yp/binding"
#endif
#define YPMATCHCACHE
#define MAX_RETRIES 20

extern bool_t xdr_domainname(), xdr_ypbind_resp();
extern bool_t xdr_ypreq_key(), xdr_ypresp_val();
extern bool_t xdr_ypreq_nokey(), xdr_ypresp_key_val();
extern bool_t xdr_ypresp_all(), xdr_ypresp_all_seq();
extern bool_t xdr_ypresp_master();

int (*ypresp_allfn)();
void *ypresp_data;

struct dom_binding *_ypbindlist;
static char _yp_domain[MAXHOSTNAMELEN];
int _yplib_timeout = 10;

#ifdef YPMATCHCACHE
int _yplib_cache = 5;

static struct ypmatch_ent {
	struct ypmatch_ent *next;
	char *map, *key, *val;
	int keylen, vallen;
	time_t expire_t;
} *ypmc;

static void
ypmatch_add(map, key, keylen, val, vallen)
	char *map;
	char *key;
	int keylen;
	char *val;
	int vallen;
{
	struct ypmatch_ent *ep;
	time_t t;

	time(&t);

	for(ep=ypmc; ep; ep=ep->next)
		if(ep->expire_t < t)
			break;
	if(ep==NULL) {
		ep = (struct ypmatch_ent *)malloc(sizeof *ep);
		bzero((char *)ep, sizeof *ep);
		if(ypmc)
			ep->next = ypmc;
		ypmc = ep;
	}

	if(ep->key)
		free(ep->key);
	if(ep->val)
		free(ep->val);

	ep->key = NULL;
	ep->val = NULL;

	ep->key = (char *)malloc(keylen);
	if(ep->key==NULL)
		return;

	ep->val = (char *)malloc(vallen);
	if(ep->key==NULL) {
		free(ep->key);
		ep->key = NULL;
		return;
	}
	ep->keylen = keylen;
	ep->vallen = vallen;

	bcopy(key, ep->key, ep->keylen);
	bcopy(val, ep->val, ep->vallen);

	if(ep->map) {
		if( strcmp(ep->map, map) ) {
			free(ep->map);
			ep->map = strdup(map);
		}
	} else {
		ep->map = strdup(map);
	}

	ep->expire_t = t + _yplib_cache;
}

static bool_t
ypmatch_find(map, key, keylen, val, vallen)
	char *map;
	char *key;
	int keylen;
	char **val;
	int *vallen;
{
	struct ypmatch_ent *ep;
	time_t t;

	if(ypmc==NULL)
		return 0;

	time(&t);

	for(ep=ypmc; ep; ep=ep->next) {
		if(ep->keylen != keylen)
			continue;
		if(strcmp(ep->map, map))
			continue;
		if(bcmp(ep->key, key, keylen))
			continue;
		if(t > ep->expire_t)
			continue;

		*val = ep->val;
		*vallen = ep->vallen;
		return 1;
	}
	return 0;
}
#endif

char *
ypbinderr_string(incode)
	int incode;
{
	static char err[80];
	switch(incode) {
	case 0:
		return "Success";
	case YPBIND_ERR_ERR:
		return "Internal ypbind error";
	case YPBIND_ERR_NOSERV:
		return "Domain not bound";
	case YPBIND_ERR_RESC:
		return "System resource allocation failure";
	}
	sprintf(err, "Unknown ypbind error: #%d\n", incode);
	return err;
}

int
_yp_dobind(dom, ypdb)
	char *dom;
	struct dom_binding **ypdb;
{
	static pid_t pid = -1;
	char path[MAXPATHLEN];
	struct dom_binding *ysd, *ysd2;
	struct ypbind_resp ypbr;
	struct timeval tv;
	struct sockaddr_in clnt_sin;
	int clnt_sock, lfd, fd, gpid;
	CLIENT *client;
	int new = 0, r;
	int retries = 0;
	struct sockaddr_in check;
	int checklen = sizeof(struct sockaddr_in);

	gpid = getpid();
	if( !(pid==-1 || pid==gpid) ) {
		ysd = _ypbindlist;
		while(ysd) {
			if(ysd->dom_client)
				clnt_destroy(ysd->dom_client);
			ysd2 = ysd->dom_pnext;
			free(ysd);
			ysd = ysd2;
		}
		_ypbindlist = NULL;
	}
	pid = gpid;

	if(ypdb!=NULL)
		*ypdb = NULL;

	if(dom==NULL || strlen(dom)==0)
		return YPERR_BADARGS;

	for(ysd = _ypbindlist; ysd; ysd = ysd->dom_pnext)
		if( strcmp(dom, ysd->dom_domain) == 0)
			break;


	if(ysd==NULL) {
		ysd = (struct dom_binding *)malloc(sizeof *ysd);
		bzero((char *)ysd, sizeof *ysd);
		ysd->dom_socket = -1;
		ysd->dom_vers = 0;
		new = 1;
	} else {
	/* Check the socket -- may have been hosed by the caller. */
		if (getsockname(ysd->dom_socket, (struct sockaddr *)&check,
		    &checklen) == -1 || check.sin_family != AF_INET ||
		    check.sin_port != ysd->dom_local_port) {
		/* Socket became bogus somehow... need to rebind. */
			int save, sock;

			sock = ysd->dom_socket;
			save = dup(ysd->dom_socket);
			clnt_destroy(ysd->dom_client);
			ysd->dom_vers = 0;
			ysd->dom_client = NULL;
			sock = dup2(save, sock);
			close(save);
		}
	}

again:
	retries++;
	if (retries > MAX_RETRIES) {
		if (new)
			free(ysd);
		return(YPERR_YPBIND);
	}
#ifdef BINDINGDIR
	if(ysd->dom_vers==0) {
		/*
		 * We're trying to make a new binding: zorch the
		 * existing handle now (if any).
		 */
		if(ysd->dom_client) {
			clnt_destroy(ysd->dom_client);
			ysd->dom_client = NULL;
			ysd->dom_socket = -1;
		}
		sprintf(path, "%s/%s.%d", BINDINGDIR, dom, 2);
		if( (fd=open(path, O_RDONLY)) == -1) {
			/* no binding file, YP is dead. */
			/* Try to bring it back to life. */
			close(fd);
			goto skipit;
		}
		if( flock(fd, LOCK_EX|LOCK_NB) == -1 && errno==EWOULDBLOCK) {
			struct iovec iov[2];
			struct ypbind_resp ybr;
			u_short	ypb_port;

			iov[0].iov_base = (caddr_t)&ypb_port;
			iov[0].iov_len = sizeof ypb_port;
			iov[1].iov_base = (caddr_t)&ybr;
			iov[1].iov_len = sizeof ybr;

			r = readv(fd, iov, 2);
			if(r != iov[0].iov_len + iov[1].iov_len) {
				close(fd);
				ysd->dom_vers = -1;
				goto again;
			}

			bzero(&ysd->dom_server_addr, sizeof ysd->dom_server_addr);
			ysd->dom_server_addr.sin_family = AF_INET;
			ysd->dom_server_addr.sin_len = sizeof(struct sockaddr_in);
			ysd->dom_server_addr.sin_addr.s_addr =
			    *(u_long *)&ybr.ypbind_resp_u.ypbind_bindinfo.ypbind_binding_addr;
			ysd->dom_server_addr.sin_port =
			    *(u_short *)&ybr.ypbind_resp_u.ypbind_bindinfo.ypbind_binding_port;

			ysd->dom_server_port = ysd->dom_server_addr.sin_port;
			close(fd);
			goto gotit;
		} else {
			/* no lock on binding file, YP is dead. */
			/* Try to bring it back to life. */
			close(fd);
			goto skipit;
		}
	}
skipit:
#endif
	if(ysd->dom_vers==-1 || ysd->dom_vers==0) {
		/*
		 * We're trying to make a new binding: zorch the
		 * existing handle now (if any).
		 */
		if(ysd->dom_client) {
			clnt_destroy(ysd->dom_client);
			ysd->dom_client = NULL;
			ysd->dom_socket = -1;
		}
		bzero((char *)&clnt_sin, sizeof clnt_sin);
		clnt_sin.sin_family = AF_INET;
		clnt_sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

		clnt_sock = RPC_ANYSOCK;
		client = clnttcp_create(&clnt_sin, YPBINDPROG, YPBINDVERS, &clnt_sock,
			0, 0);
		if(client==NULL) {
			/*
			 * These conditions indicate ypbind just isn't
			 * alive -- we probably don't want to shoot our
			 * mouth off in this case; instead generate error
			 * messages only for really exotic problems.
			 */
			if (rpc_createerr.cf_stat != RPC_PROGNOTREGISTERED &&
			   (rpc_createerr.cf_stat != RPC_SYSTEMERROR &&
			   rpc_createerr.cf_error.re_errno == ECONNREFUSED))
				clnt_pcreateerror("clnttcp_create");
			if(new)
				free(ysd);
			return (YPERR_YPBIND);
		}

		tv.tv_sec = _yplib_timeout/2;
		tv.tv_usec = 0;
		r = clnt_call(client, YPBINDPROC_DOMAIN,
			xdr_domainname, (char *)&dom, xdr_ypbind_resp, &ypbr, tv);
		if(r != RPC_SUCCESS) {
			clnt_destroy(client);
			ysd->dom_vers = -1;
			if (r == RPC_PROGUNAVAIL || r == RPC_PROCUNAVAIL) {
				if (new)
					free(ysd);
				return(YPERR_YPBIND);
			}
			fprintf(stderr,
			"YP: server for domain %s not responding, retrying\n", dom);
			goto again;
		} else {
			if (ypbr.ypbind_status != YPBIND_SUCC_VAL) {
				clnt_destroy(client);
				ysd->dom_vers = -1;
				sleep(_yplib_timeout/2);
				goto again;
			}
		}
		clnt_destroy(client);

		bzero((char *)&ysd->dom_server_addr, sizeof ysd->dom_server_addr);
		ysd->dom_server_addr.sin_family = AF_INET;
		ysd->dom_server_addr.sin_port =
			*(u_short *)&ypbr.ypbind_resp_u.ypbind_bindinfo.ypbind_binding_port;
		ysd->dom_server_addr.sin_addr.s_addr =
			*(u_long *)&ypbr.ypbind_resp_u.ypbind_bindinfo.ypbind_binding_addr;
		ysd->dom_server_port =
			*(u_short *)&ypbr.ypbind_resp_u.ypbind_bindinfo.ypbind_binding_port;
gotit:
		ysd->dom_vers = YPVERS;
		strcpy(ysd->dom_domain, dom);
	}

	/* Don't rebuild the connection to the server unless we have to. */
	if (ysd->dom_client == NULL) {
		tv.tv_sec = _yplib_timeout/2;
		tv.tv_usec = 0;
		ysd->dom_socket = RPC_ANYSOCK;
		ysd->dom_client = clntudp_create(&ysd->dom_server_addr,
			YPPROG, YPVERS, tv, &ysd->dom_socket);
		if(ysd->dom_client==NULL) {
			clnt_pcreateerror("clntudp_create");
			ysd->dom_vers = -1;
			goto again;
		}
		if( fcntl(ysd->dom_socket, F_SETFD, 1) == -1)
			perror("fcntl: F_SETFD");
		/*
		 * We want a port number associated with this socket
		 * so that we can check its authenticity later.
		 */
		checklen = sizeof(struct sockaddr_in);
		bzero((char *)&check, checklen);
		bind(ysd->dom_socket, (struct sockaddr *)&check, checklen);
		check.sin_family = AF_INET;
		if (!getsockname(ysd->dom_socket,
		    (struct sockaddr *)&check, &checklen)) {
			ysd->dom_local_port = check.sin_port;
		} else {
			clnt_destroy(ysd->dom_client);
			if (new)
				free(ysd);
			return(YPERR_YPBIND);
		}
	}

	if(new) {
		ysd->dom_pnext = _ypbindlist;
		_ypbindlist = ysd;
	}

	if(ypdb!=NULL)
		*ypdb = ysd;
	return 0;
}

static void
_yp_unbind(ypb)
	struct dom_binding *ypb;
{
	if (ypb->dom_client)
		clnt_destroy(ypb->dom_client);
	ypb->dom_client = NULL;
	ypb->dom_socket = -1;
	ypb->dom_vers = -1;
}

int
yp_bind(dom)
	char *dom;
{
	return _yp_dobind(dom, NULL);
}

void
yp_unbind(dom)
	char *dom;
{
	struct dom_binding *ypb, *ypbp;

	ypbp = NULL;
	for(ypb=_ypbindlist; ypb; ypb=ypb->dom_pnext) {
		if( strcmp(dom, ypb->dom_domain) == 0) {
			_yp_unbind(ypb);
			if(ypbp)
				ypbp->dom_pnext = ypb->dom_pnext;
			else
				_ypbindlist = ypb->dom_pnext;
			free(ypb);
			return;
		}
		ypbp = ypb;
	}
	return;
}

int
yp_match(indomain, inmap, inkey, inkeylen, outval, outvallen)
	char *indomain;
	char *inmap;
	const char *inkey;
	int inkeylen;
	char **outval;
	int *outvallen;
{
	struct dom_binding *ysd;
	struct ypresp_val yprv;
	struct timeval tv;
	struct ypreq_key yprk;
	int r;

	*outval = NULL;
	*outvallen = 0;

	/* Sanity check */

	if (inkey == NULL || !strlen(inkey) || inkeylen <= 0 ||
	    inmap == NULL || !strlen(inmap) ||
	    indomain == NULL || !strlen(indomain))
		return YPERR_BADARGS;

#ifdef YPMATCHCACHE
	if( !strcmp(_yp_domain, indomain) && ypmatch_find(inmap, inkey,
	    inkeylen, &yprv.val.valdat_val, &yprv.val.valdat_len)) {
		*outvallen = yprv.val.valdat_len;
		*outval = (char *)malloc(*outvallen+1);
		bcopy(yprv.val.valdat_val, *outval, *outvallen);
		(*outval)[*outvallen] = '\0';
		return 0;
	}
#endif

again:
	if( _yp_dobind(indomain, &ysd) != 0)
		return YPERR_DOMAIN;

	tv.tv_sec = _yplib_timeout;
	tv.tv_usec = 0;

	yprk.domain = indomain;
	yprk.map = inmap;
	yprk.key.keydat_val = (char *)inkey;
	yprk.key.keydat_len = inkeylen;

	bzero((char *)&yprv, sizeof yprv);

	r = clnt_call(ysd->dom_client, YPPROC_MATCH,
		xdr_ypreq_key, &yprk, xdr_ypresp_val, &yprv, tv);
	if(r != RPC_SUCCESS) {
		clnt_perror(ysd->dom_client, "yp_match: clnt_call");
		_yp_unbind(ysd);
		goto again;
	}

	if( !(r=ypprot_err(yprv.stat)) ) {
		*outvallen = yprv.val.valdat_len;
		*outval = (char *)malloc(*outvallen+1);
		bcopy(yprv.val.valdat_val, *outval, *outvallen);
		(*outval)[*outvallen] = '\0';
#ifdef YPMATCHCACHE
		if( strcmp(_yp_domain, indomain)==0 )
			 ypmatch_add(inmap, inkey, inkeylen, *outval, *outvallen);
#endif
	}

	xdr_free(xdr_ypresp_val, (char *)&yprv);
	return r;
}

int
yp_get_default_domain(domp)
char **domp;
{
	*domp = NULL;
	if(_yp_domain[0] == '\0')
		if( getdomainname(_yp_domain, sizeof _yp_domain))
			return YPERR_NODOM;
	*domp = _yp_domain;
	return 0;
}

int
yp_first(indomain, inmap, outkey, outkeylen, outval, outvallen)
	char *indomain;
	char *inmap;
	char **outkey;
	int *outkeylen;
	char **outval;
	int *outvallen;
{
	struct ypresp_key_val yprkv;
	struct ypreq_nokey yprnk;
	struct dom_binding *ysd;
	struct timeval tv;
	int r;

	/* Sanity check */

	if (indomain == NULL || !strlen(indomain) ||
	    inmap == NULL || !strlen(inmap))
		return YPERR_BADARGS;

	*outkey = *outval = NULL;
	*outkeylen = *outvallen = 0;

again:
	if( _yp_dobind(indomain, &ysd) != 0)
		return YPERR_DOMAIN;

	tv.tv_sec = _yplib_timeout;
	tv.tv_usec = 0;

	yprnk.domain = indomain;
	yprnk.map = inmap;
	bzero((char *)&yprkv, sizeof yprkv);

	r = clnt_call(ysd->dom_client, YPPROC_FIRST,
		xdr_ypreq_nokey, &yprnk, xdr_ypresp_key_val, &yprkv, tv);
	if(r != RPC_SUCCESS) {
		clnt_perror(ysd->dom_client, "yp_first: clnt_call");
		_yp_unbind(ysd);
		goto again;
	}
	if( !(r=ypprot_err(yprkv.stat)) ) {
		*outkeylen = yprkv.key.keydat_len;
		*outkey = (char *)malloc(*outkeylen+1);
		bcopy(yprkv.key.keydat_val, *outkey, *outkeylen);
		(*outkey)[*outkeylen] = '\0';
		*outvallen = yprkv.val.valdat_len;
		*outval = (char *)malloc(*outvallen+1);
		bcopy(yprkv.val.valdat_val, *outval, *outvallen);
		(*outval)[*outvallen] = '\0';
	}

	xdr_free(xdr_ypresp_key_val, (char *)&yprkv);
	return r;
}

int
yp_next(indomain, inmap, inkey, inkeylen, outkey, outkeylen, outval, outvallen)
	char *indomain;
	char *inmap;
	char *inkey;
	int inkeylen;
	char **outkey;
	int *outkeylen;
	char **outval;
	int *outvallen;
{
	struct ypresp_key_val yprkv;
	struct ypreq_key yprk;
	struct dom_binding *ysd;
	struct timeval tv;
	int r;

	/* Sanity check */

	if (inkey == NULL || !strlen(inkey) || inkeylen <= 0 ||
	    inmap == NULL || !strlen(inmap) ||
	    indomain == NULL || !strlen(indomain))
		return YPERR_BADARGS;

	*outkey = *outval = NULL;
	*outkeylen = *outvallen = 0;

again:
	if( _yp_dobind(indomain, &ysd) != 0)
		return YPERR_DOMAIN;

	tv.tv_sec = _yplib_timeout;
	tv.tv_usec = 0;

	yprk.domain = indomain;
	yprk.map = inmap;
	yprk.key.keydat_val = inkey;
	yprk.key.keydat_len = inkeylen;
	bzero((char *)&yprkv, sizeof yprkv);

	r = clnt_call(ysd->dom_client, YPPROC_NEXT,
		xdr_ypreq_key, &yprk, xdr_ypresp_key_val, &yprkv, tv);
	if(r != RPC_SUCCESS) {
		clnt_perror(ysd->dom_client, "yp_next: clnt_call");
		_yp_unbind(ysd);
		goto again;
	}
	if( !(r=ypprot_err(yprkv.stat)) ) {
		*outkeylen = yprkv.key.keydat_len;
		*outkey = (char *)malloc(*outkeylen+1);
		bcopy(yprkv.key.keydat_val, *outkey, *outkeylen);
		(*outkey)[*outkeylen] = '\0';
		*outvallen = yprkv.val.valdat_len;
		*outval = (char *)malloc(*outvallen+1);
		bcopy(yprkv.val.valdat_val, *outval, *outvallen);
		(*outval)[*outvallen] = '\0';
	}

	xdr_free(xdr_ypresp_key_val, (char *)&yprkv);
	return r;
}

int
yp_all(indomain, inmap, incallback)
	char *indomain;
	char *inmap;
	struct ypall_callback *incallback;
{
	struct ypreq_nokey yprnk;
	struct dom_binding *ysd;
	struct timeval tv;
	struct sockaddr_in clnt_sin;
	CLIENT *clnt;
	u_long status, savstat;
	int clnt_sock;

	/* Sanity check */

	if (indomain == NULL || !strlen(indomain) ||
	    inmap == NULL || !strlen(inmap))
		return YPERR_BADARGS;

again:

	if( _yp_dobind(indomain, &ysd) != 0)
		return YPERR_DOMAIN;

	tv.tv_sec = _yplib_timeout;
	tv.tv_usec = 0;

	/* YPPROC_ALL manufactures its own channel to ypserv using TCP */

	clnt_sock = RPC_ANYSOCK;
	clnt_sin = ysd->dom_server_addr;
	clnt_sin.sin_port = 0;
	clnt = clnttcp_create(&clnt_sin, YPPROG, YPVERS, &clnt_sock, 0, 0);
	if(clnt==NULL) {
		printf("clnttcp_create failed\n");
		return YPERR_PMAP;
	}

	yprnk.domain = indomain;
	yprnk.map = inmap;
	ypresp_allfn = incallback->foreach;
	ypresp_data = (void *)incallback->data;

	if (clnt_call(clnt, YPPROC_ALL,
		xdr_ypreq_nokey, &yprnk,
		xdr_ypresp_all_seq, &status, tv) != RPC_SUCCESS) {
			clnt_perror(ysd->dom_client, "yp_next: clnt_call");
			clnt_destroy(clnt);
			_yp_unbind(ysd);
			goto again;
	}

	clnt_destroy(clnt);
	savstat = status;
	xdr_free(xdr_ypresp_all_seq, (char *)&status);	/* not really needed... */
	if(savstat != YP_NOMORE)
		return ypprot_err(savstat);
	return 0;
}

int
yp_order(indomain, inmap, outorder)
	char *indomain;
	char *inmap;
	int *outorder;
{
 	struct dom_binding *ysd;
	struct ypresp_order ypro;
	struct ypreq_nokey yprnk;
	struct timeval tv;
	int r;

	/* Sanity check */

	if (indomain == NULL || !strlen(indomain) ||
	    inmap == NULL || !strlen(inmap))
		return YPERR_BADARGS;

again:
	if( _yp_dobind(indomain, &ysd) != 0)
		return YPERR_DOMAIN;

	tv.tv_sec = _yplib_timeout;
	tv.tv_usec = 0;

	yprnk.domain = indomain;
	yprnk.map = inmap;

	bzero((char *)(char *)&ypro, sizeof ypro);

	r = clnt_call(ysd->dom_client, YPPROC_ORDER,
		xdr_ypreq_nokey, &yprnk, xdr_ypresp_order, &ypro, tv);
	if(r != RPC_SUCCESS) {
		clnt_perror(ysd->dom_client, "yp_order: clnt_call");
		_yp_unbind(ysd);
		goto again;
	}

	if( !(r=ypprot_err(ypro.stat)) ) {
		*outorder = ypro.ordernum;
	}

	xdr_free(xdr_ypresp_order, (char *)&ypro);
	return (r);
}

int
yp_master(indomain, inmap, outname)
	char *indomain;
	char *inmap;
	char **outname;
{
	struct dom_binding *ysd;
	struct ypresp_master yprm;
	struct ypreq_nokey yprnk;
	struct timeval tv;
	int r;

	/* Sanity check */

	if (indomain == NULL || !strlen(indomain) ||
	    inmap == NULL || !strlen(inmap))
		return YPERR_BADARGS;
again:
	if( _yp_dobind(indomain, &ysd) != 0)
		return YPERR_DOMAIN;

	tv.tv_sec = _yplib_timeout;
	tv.tv_usec = 0;

	yprnk.domain = indomain;
	yprnk.map = inmap;

	bzero((char *)&yprm, sizeof yprm);

	r = clnt_call(ysd->dom_client, YPPROC_MASTER,
		xdr_ypreq_nokey, &yprnk, xdr_ypresp_master, &yprm, tv);
	if(r != RPC_SUCCESS) {
		clnt_perror(ysd->dom_client, "yp_master: clnt_call");
		_yp_unbind(ysd);
		goto again;
	}

	if( !(r=ypprot_err(yprm.stat)) ) {
		*outname = (char *)strdup(yprm.peer);
	}

	xdr_free(xdr_ypresp_master, (char *)&yprm);
	return (r);
}
int
yp_maplist(indomain, outmaplist)
	char *indomain;
	struct ypmaplist **outmaplist;
{
	struct dom_binding *ysd;
	struct ypresp_maplist ypml;
	struct timeval tv;
	int r;

	/* Sanity check */

	if (indomain == NULL || !strlen(indomain))
		return YPERR_BADARGS;

again:
	if( _yp_dobind(indomain, &ysd) != 0)
		return YPERR_DOMAIN;

	tv.tv_sec = _yplib_timeout;
	tv.tv_usec = 0;

	bzero((char *)&ypml, sizeof ypml);

	r = clnt_call(ysd->dom_client, YPPROC_MAPLIST,
		xdr_domainname,(char *)&indomain,xdr_ypresp_maplist,&ypml,tv);
	if (r != RPC_SUCCESS) {
		clnt_perror(ysd->dom_client, "yp_maplist: clnt_call");
		_yp_unbind(ysd);
		goto again;
	}
	if( !(r=ypprot_err(ypml.stat)) ) {
		*outmaplist = ypml.maps;
	}

	/* NO: xdr_free(xdr_ypresp_maplist, &ypml);*/
	return (r);
}

char *
yperr_string(incode)
	int incode;
{
	static char err[80];

	switch(incode) {
	case 0:
		return "Success";
	case YPERR_BADARGS:
		return "Request arguments bad";
	case YPERR_RPC:
		return "RPC failure";
	case YPERR_DOMAIN:
		return "Can't bind to server which serves this domain";
	case YPERR_MAP:
		return "No such map in server's domain";
	case YPERR_KEY:
		return "No such key in map";
	case YPERR_YPERR:
		return "YP server error";
	case YPERR_RESRC:
		return "Local resource allocation failure";
	case YPERR_NOMORE:
		return "No more records in map database";
	case YPERR_PMAP:
		return "Can't communicate with portmapper";
	case YPERR_YPBIND:
		return "Can't communicate with ypbind";
	case YPERR_YPSERV:
		return "Can't communicate with ypserv";
	case YPERR_NODOM:
		return "Local domain name not set";
	case YPERR_BADDB:
		return "Server data base is bad";
	case YPERR_VERS:
		return "YP server version mismatch - server can't supply service.";
	case YPERR_ACCESS:
		return "Access violation";
	case YPERR_BUSY:
		return "Database is busy";
	}
	sprintf(err, "YP unknown error %d\n", incode);
	return err;
}

int
ypprot_err(incode)
	unsigned int incode;
{
	switch(incode) {
	case YP_TRUE:
		return 0;
	case YP_FALSE:
		return YPERR_YPBIND;
	case YP_NOMORE:
		return YPERR_NOMORE;
	case YP_NOMAP:
		return YPERR_MAP;
	case YP_NODOM:
		return YPERR_DOMAIN;
	case YP_NOKEY:
		return YPERR_KEY;
	case YP_BADOP:
		return YPERR_YPERR;
	case YP_BADDB:
		return YPERR_BADDB;
	case YP_YPERR:
		return YPERR_YPERR;
	case YP_BADARGS:
		return YPERR_BADARGS;
	case YP_VERS:
		return YPERR_VERS;
	}
	return YPERR_YPERR;
}

int
_yp_check(dom)
	char **dom;
{
	char *unused;

	if( _yp_domain[0]=='\0' )
		if( yp_get_default_domain(&unused) )
			return 0;

	if(dom)
		*dom = _yp_domain;

	if( yp_bind(_yp_domain)==0 ) {
		yp_unbind(_yp_domain);
		return 1;
	}
	return 0;
}
