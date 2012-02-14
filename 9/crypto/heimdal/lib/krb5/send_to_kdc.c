/*
 * Copyright (c) 1997 - 2002 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden). 
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution. 
 *
 * 3. Neither the name of the Institute nor the names of its contributors 
 *    may be used to endorse or promote products derived from this software 
 *    without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. 
 */

#include "krb5_locl.h"

RCSID("$Id: send_to_kdc.c 21934 2007-08-27 14:21:04Z lha $");

struct send_to_kdc {
    krb5_send_to_kdc_func func;
    void *data;
};

/*
 * send the data in `req' on the socket `fd' (which is datagram iff udp)
 * waiting `tmout' for a reply and returning the reply in `rep'.
 * iff limit read up to this many bytes
 * returns 0 and data in `rep' if succesful, otherwise -1
 */

static int
recv_loop (int fd,
	   time_t tmout,
	   int udp,
	   size_t limit,
	   krb5_data *rep)
{
     fd_set fdset;
     struct timeval timeout;
     int ret;
     int nbytes;

     if (fd >= FD_SETSIZE) {
	 return -1;
     }

     krb5_data_zero(rep);
     do {
	 FD_ZERO(&fdset);
	 FD_SET(fd, &fdset);
	 timeout.tv_sec  = tmout;
	 timeout.tv_usec = 0;
	 ret = select (fd + 1, &fdset, NULL, NULL, &timeout);
	 if (ret < 0) {
	     if (errno == EINTR)
		 continue;
	     return -1;
	 } else if (ret == 0) {
	     return 0;
	 } else {
	     void *tmp;

	     if (ioctl (fd, FIONREAD, &nbytes) < 0) {
		 krb5_data_free (rep);
		 return -1;
	     }
	     if(nbytes <= 0)
		 return 0;

	     if (limit)
		 nbytes = min(nbytes, limit - rep->length);

	     tmp = realloc (rep->data, rep->length + nbytes);
	     if (tmp == NULL) {
		 krb5_data_free (rep);
		 return -1;
	     }
	     rep->data = tmp;
	     ret = recv (fd, (char*)tmp + rep->length, nbytes, 0);
	     if (ret < 0) {
		 krb5_data_free (rep);
		 return -1;
	     }
	     rep->length += ret;
	 }
     } while(!udp && (limit == 0 || rep->length < limit));
     return 0;
}

/*
 * Send kerberos requests and receive a reply on a udp or any other kind
 * of a datagram socket.  See `recv_loop'.
 */

static int
send_and_recv_udp(int fd, 
		  time_t tmout,
		  const krb5_data *req,
		  krb5_data *rep)
{
    if (send (fd, req->data, req->length, 0) < 0)
	return -1;

    return recv_loop(fd, tmout, 1, 0, rep);
}

/*
 * `send_and_recv' for a TCP (or any other stream) socket.
 * Since there are no record limits on a stream socket the protocol here
 * is to prepend the request with 4 bytes of its length and the reply
 * is similarly encoded.
 */

static int
send_and_recv_tcp(int fd, 
		  time_t tmout,
		  const krb5_data *req,
		  krb5_data *rep)
{
    unsigned char len[4];
    unsigned long rep_len;
    krb5_data len_data;

    _krb5_put_int(len, req->length, 4);
    if(net_write(fd, len, sizeof(len)) < 0)
	return -1;
    if(net_write(fd, req->data, req->length) < 0)
	return -1;
    if (recv_loop (fd, tmout, 0, 4, &len_data) < 0)
	return -1;
    if (len_data.length != 4) {
	krb5_data_free (&len_data);
	return -1;
    }
    _krb5_get_int(len_data.data, &rep_len, 4);
    krb5_data_free (&len_data);
    if (recv_loop (fd, tmout, 0, rep_len, rep) < 0)
	return -1;
    if(rep->length != rep_len) {
	krb5_data_free (rep);
	return -1;
    }
    return 0;
}

int
_krb5_send_and_recv_tcp(int fd,
			time_t tmout,
			const krb5_data *req,
			krb5_data *rep)
{
    return send_and_recv_tcp(fd, tmout, req, rep);
}

/*
 * `send_and_recv' tailored for the HTTP protocol.
 */

static int
send_and_recv_http(int fd, 
		   time_t tmout,
		   const char *prefix,
		   const krb5_data *req,
		   krb5_data *rep)
{
    char *request;
    char *str;
    int ret;
    int len = base64_encode(req->data, req->length, &str);

    if(len < 0)
	return -1;
    asprintf(&request, "GET %s%s HTTP/1.0\r\n\r\n", prefix, str);
    free(str);
    if (request == NULL)
	return -1;
    ret = net_write (fd, request, strlen(request));
    free (request);
    if (ret < 0)
	return ret;
    ret = recv_loop(fd, tmout, 0, 0, rep);
    if(ret)
	return ret;
    {
	unsigned long rep_len;
	char *s, *p;

	s = realloc(rep->data, rep->length + 1);
	if (s == NULL) {
	    krb5_data_free (rep);
	    return -1;
	}
	s[rep->length] = 0;
	p = strstr(s, "\r\n\r\n");
	if(p == NULL) {
	    krb5_data_zero(rep);
	    free(s);
	    return -1;
	}
	p += 4;
	rep->data = s;
	rep->length -= p - s;
	if(rep->length < 4) { /* remove length */
	    krb5_data_zero(rep);
	    free(s);
	    return -1;
	}
	rep->length -= 4;
	_krb5_get_int(p, &rep_len, 4);
	if (rep_len != rep->length) {
	    krb5_data_zero(rep);
	    free(s);
	    return -1;
	}
	memmove(rep->data, p + 4, rep->length);
    }
    return 0;
}

static int
init_port(const char *s, int fallback)
{
    if (s) {
	int tmp;

	sscanf (s, "%d", &tmp);
	return htons(tmp);
    } else
	return fallback;
}

/*
 * Return 0 if succesful, otherwise 1
 */

static int
send_via_proxy (krb5_context context,
		const krb5_krbhst_info *hi,
		const krb5_data *send_data,
		krb5_data *receive)
{
    char *proxy2 = strdup(context->http_proxy);
    char *proxy  = proxy2;
    char *prefix;
    char *colon;
    struct addrinfo hints;
    struct addrinfo *ai, *a;
    int ret;
    int s = -1;
    char portstr[NI_MAXSERV];
		 
    if (proxy == NULL)
	return ENOMEM;
    if (strncmp (proxy, "http://", 7) == 0)
	proxy += 7;

    colon = strchr(proxy, ':');
    if(colon != NULL)
	*colon++ = '\0';
    memset (&hints, 0, sizeof(hints));
    hints.ai_family   = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    snprintf (portstr, sizeof(portstr), "%d",
	      ntohs(init_port (colon, htons(80))));
    ret = getaddrinfo (proxy, portstr, &hints, &ai);
    free (proxy2);
    if (ret)
	return krb5_eai_to_heim_errno(ret, errno);

    for (a = ai; a != NULL; a = a->ai_next) {
	s = socket (a->ai_family, a->ai_socktype, a->ai_protocol);
	if (s < 0)
	    continue;
	if (connect (s, a->ai_addr, a->ai_addrlen) < 0) {
	    close (s);
	    continue;
	}
	break;
    }
    if (a == NULL) {
	freeaddrinfo (ai);
	return 1;
    }
    freeaddrinfo (ai);

    asprintf(&prefix, "http://%s/", hi->hostname);
    if(prefix == NULL) {
	close(s);
	return 1;
    }
    ret = send_and_recv_http(s, context->kdc_timeout,
			     prefix, send_data, receive);
    close (s);
    free(prefix);
    if(ret == 0 && receive->length != 0)
	return 0;
    return 1;
}

/*
 * Send the data `send' to one host from `handle` and get back the reply
 * in `receive'.
 */

krb5_error_code KRB5_LIB_FUNCTION
krb5_sendto (krb5_context context,
	     const krb5_data *send_data,
	     krb5_krbhst_handle handle,	     
	     krb5_data *receive)
{
     krb5_error_code ret;
     int fd;
     int i;

     krb5_data_zero(receive);

     for (i = 0; i < context->max_retries; ++i) {
	 krb5_krbhst_info *hi;

	 while (krb5_krbhst_next(context, handle, &hi) == 0) {
	     struct addrinfo *ai, *a;

	     if (context->send_to_kdc) {
		 struct send_to_kdc *s = context->send_to_kdc;

		 ret = (*s->func)(context, s->data, 
				  hi, send_data, receive);
		 if (ret == 0 && receive->length != 0)
		     goto out;
		 continue;
	     }

	     if(hi->proto == KRB5_KRBHST_HTTP && context->http_proxy) {
		 if (send_via_proxy (context, hi, send_data, receive) == 0) {
		     ret = 0;
		     goto out;
		 }
		 continue;
	     }

	     ret = krb5_krbhst_get_addrinfo(context, hi, &ai);
	     if (ret)
		 continue;

	     for (a = ai; a != NULL; a = a->ai_next) {
		 fd = socket (a->ai_family, a->ai_socktype, a->ai_protocol);
		 if (fd < 0)
		     continue;
		 if (connect (fd, a->ai_addr, a->ai_addrlen) < 0) {
		     close (fd);
		     continue;
		 }
		 switch (hi->proto) {
		 case KRB5_KRBHST_HTTP :
		     ret = send_and_recv_http(fd, context->kdc_timeout,
					      "", send_data, receive);
		     break;
		 case KRB5_KRBHST_TCP :
		     ret = send_and_recv_tcp (fd, context->kdc_timeout,
					      send_data, receive);
		     break;
		 case KRB5_KRBHST_UDP :
		     ret = send_and_recv_udp (fd, context->kdc_timeout,
					      send_data, receive);
		     break;
		 }
		 close (fd);
		 if(ret == 0 && receive->length != 0)
		     goto out;
	     }
	 }
	 krb5_krbhst_reset(context, handle);
     }
     krb5_clear_error_string (context);
     ret = KRB5_KDC_UNREACH;
out:
     return ret;
}

krb5_error_code KRB5_LIB_FUNCTION
krb5_sendto_kdc(krb5_context context,
		const krb5_data *send_data,
		const krb5_realm *realm,
		krb5_data *receive)
{
    return krb5_sendto_kdc_flags(context, send_data, realm, receive, 0);
}

krb5_error_code KRB5_LIB_FUNCTION
krb5_sendto_kdc_flags(krb5_context context,
		      const krb5_data *send_data,
		      const krb5_realm *realm,
		      krb5_data *receive,
		      int flags)
{
    krb5_error_code ret;
    krb5_sendto_ctx ctx;

    ret = krb5_sendto_ctx_alloc(context, &ctx);
    if (ret)
	return ret;
    krb5_sendto_ctx_add_flags(ctx, flags);
    krb5_sendto_ctx_set_func(ctx, _krb5_kdc_retry, NULL);

    ret = krb5_sendto_context(context, ctx, send_data, *realm, receive);
    krb5_sendto_ctx_free(context, ctx);
    return ret;
}

krb5_error_code KRB5_LIB_FUNCTION
krb5_set_send_to_kdc_func(krb5_context context, 
			  krb5_send_to_kdc_func func,
			  void *data)
{
    free(context->send_to_kdc);
    if (func == NULL) {
	context->send_to_kdc = NULL;
	return 0;
    }

    context->send_to_kdc = malloc(sizeof(*context->send_to_kdc));
    if (context->send_to_kdc == NULL) {
	krb5_set_error_string(context, "Out of memory");
	return ENOMEM;
    }

    context->send_to_kdc->func = func;
    context->send_to_kdc->data = data;
    return 0;
}

struct krb5_sendto_ctx_data {
    int flags;
    int type;
    krb5_sendto_ctx_func func;
    void *data;
};

krb5_error_code KRB5_LIB_FUNCTION
krb5_sendto_ctx_alloc(krb5_context context, krb5_sendto_ctx *ctx)
{
    *ctx = calloc(1, sizeof(**ctx));
    if (*ctx == NULL) {
	krb5_set_error_string(context, "out of memory");
	return ENOMEM;
    }
    return 0;
}

void KRB5_LIB_FUNCTION
krb5_sendto_ctx_add_flags(krb5_sendto_ctx ctx, int flags)
{
    ctx->flags |= flags;
}

int KRB5_LIB_FUNCTION
krb5_sendto_ctx_get_flags(krb5_sendto_ctx ctx)
{
    return ctx->flags;
}

void KRB5_LIB_FUNCTION
krb5_sendto_ctx_set_type(krb5_sendto_ctx ctx, int type)
{
    ctx->type = type;
}


void KRB5_LIB_FUNCTION
krb5_sendto_ctx_set_func(krb5_sendto_ctx ctx,
			 krb5_sendto_ctx_func func,
			 void *data)
{
    ctx->func = func;
    ctx->data = data;
}

void KRB5_LIB_FUNCTION
krb5_sendto_ctx_free(krb5_context context, krb5_sendto_ctx ctx)
{
    memset(ctx, 0, sizeof(*ctx));
    free(ctx);
}

krb5_error_code KRB5_LIB_FUNCTION
krb5_sendto_context(krb5_context context,
		    krb5_sendto_ctx ctx,
		    const krb5_data *send_data,
		    const krb5_realm realm,
		    krb5_data *receive)
{
    krb5_error_code ret;
    krb5_krbhst_handle handle = NULL;
    int type, freectx = 0;
    int action;

    krb5_data_zero(receive);

    if (ctx == NULL) {
	freectx = 1;
	ret = krb5_sendto_ctx_alloc(context, &ctx);
	if (ret)
	    return ret;
    }

    type = ctx->type;
    if (type == 0) {
	if ((ctx->flags & KRB5_KRBHST_FLAGS_MASTER) || context->use_admin_kdc)
	    type = KRB5_KRBHST_ADMIN;
	else
	    type = KRB5_KRBHST_KDC;
    }

    if (send_data->length > context->large_msg_size)
	ctx->flags |= KRB5_KRBHST_FLAGS_LARGE_MSG;

    /* loop until we get back a appropriate response */

    do {
	action = KRB5_SENDTO_DONE;

	krb5_data_free(receive);

	if (handle == NULL) {
	    ret = krb5_krbhst_init_flags(context, realm, type, 
					 ctx->flags, &handle);
	    if (ret) {
		if (freectx)
		    krb5_sendto_ctx_free(context, ctx);
		return ret;
	    }
	}
    
	ret = krb5_sendto(context, send_data, handle, receive);
	if (ret)
	    break;
	if (ctx->func) {
	    ret = (*ctx->func)(context, ctx, ctx->data, receive, &action);
	    if (ret)
		break;
	}
	if (action != KRB5_SENDTO_CONTINUE) {
	    krb5_krbhst_free(context, handle);
	    handle = NULL;
	}
    } while (action != KRB5_SENDTO_DONE);
    if (handle)
	krb5_krbhst_free(context, handle);
    if (ret == KRB5_KDC_UNREACH)
	krb5_set_error_string(context, 
			      "unable to reach any KDC in realm %s", realm);
    if (ret)
	krb5_data_free(receive);
    if (freectx)
	krb5_sendto_ctx_free(context, ctx);
    return ret;
}

krb5_error_code
_krb5_kdc_retry(krb5_context context, krb5_sendto_ctx ctx, void *data,
		const krb5_data *reply, int *action)
{
    krb5_error_code ret;
    KRB_ERROR error;

    if(krb5_rd_error(context, reply, &error))
	return 0;

    ret = krb5_error_from_rd_error(context, &error, NULL);
    krb5_free_error_contents(context, &error);

    switch(ret) {
    case KRB5KRB_ERR_RESPONSE_TOO_BIG: {
	if (krb5_sendto_ctx_get_flags(ctx) & KRB5_KRBHST_FLAGS_LARGE_MSG)
	    break;
	krb5_sendto_ctx_add_flags(ctx, KRB5_KRBHST_FLAGS_LARGE_MSG);
	*action = KRB5_SENDTO_RESTART;
	break;
    }
    case KRB5KDC_ERR_SVC_UNAVAILABLE:
	*action = KRB5_SENDTO_CONTINUE;
	break;
    }
    return 0;
}
