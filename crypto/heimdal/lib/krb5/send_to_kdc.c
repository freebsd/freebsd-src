/*
 * Copyright (c) 1997 - 2000 Kungliga Tekniska Högskolan
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

RCSID("$Id: send_to_kdc.c,v 1.40 2000/11/15 01:48:23 assar Exp $");

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
	     if(nbytes == 0)
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
	    free(s);
	    return -1;
	}
	p += 4;
	rep->data = s;
	rep->length -= p - s;
	if(rep->length < 4) { /* remove length */
	    free(s);
	    return -1;
	}
	rep->length -= 4;
	_krb5_get_int(p, &rep_len, 4);
	if (rep_len != rep->length) {
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
		const char *hostname,
		const krb5_data *send,
		krb5_data *receive)
{
    char *proxy2 = strdup(context->http_proxy);
    char *proxy  = proxy2;
    char *prefix;
    char *colon;
    struct addrinfo hints;
    struct addrinfo *ai, *a;
    int ret;
    int s;
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
	return krb5_eai_to_heim_errno(ret);

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

    asprintf(&prefix, "http://%s/", hostname);
    if(prefix == NULL) {
	close(s);
	return 1;
    }
    ret = send_and_recv_http(s, context->kdc_timeout,
			     prefix, send, receive);
    close (s);
    free(prefix);
    if(ret == 0 && receive->length != 0)
	return 0;
    return 1;
}

/*
 * Send the data `send' to one KDC in `realm' and get back the reply
 * in `receive'.
 */

krb5_error_code
krb5_sendto (krb5_context context,
	     const krb5_data *send,
	     char **hostlist,
	     int port,
	     krb5_data *receive)
{
     krb5_error_code ret = 0;
     char **hp, *p;
     int fd;
     int i;

     for (i = 0; i < context->max_retries; ++i)
	 for (hp = hostlist; (p = *hp); ++hp) {
	     char *colon;
	     int http_flag = 0;
	     int tcp_flag = 0;
	     struct addrinfo *ai, *a;
	     struct addrinfo hints;
	     char portstr[NI_MAXSERV];

	     if(strncmp(p, "http://", 7) == 0){
		 p += 7;
		 http_flag = 1;
		 port = htons(80);
	     } else if(strncmp(p, "http/", 5) == 0) {
		 p += 5;
		 http_flag = 1;
		 port = htons(80);
	     }else if(strncmp(p, "tcp/", 4) == 0){
		 p += 4;
		 tcp_flag = 1;
	     } else if(strncmp(p, "udp/", 4) == 0) {
		 p += 4;
	     }
	     if(http_flag && context->http_proxy) {
		 if (send_via_proxy (context, p, send, receive))
		     continue;
		 else
		     goto out;
	     }
	     colon = strchr (p, ':');
	     if (colon)
		 *colon++ = '\0';

	     memset (&hints, 0, sizeof(hints));
	     hints.ai_family = PF_UNSPEC;
	     if (tcp_flag || http_flag)
		 hints.ai_socktype = SOCK_STREAM;
	     else
		 hints.ai_socktype = SOCK_DGRAM;
	     snprintf (portstr, sizeof(portstr), "%d",
		       ntohs(init_port (colon, port)));
	     ret = getaddrinfo (p, portstr, &hints, &ai);
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
		 break;
	     }
	     if (a == NULL) {
		 freeaddrinfo (ai);
		 continue;
	     }
	     freeaddrinfo (ai);

	     if(http_flag)
		 ret = send_and_recv_http(fd, context->kdc_timeout,
					  "", send, receive);
	     else if(tcp_flag)
		 ret = send_and_recv_tcp (fd, context->kdc_timeout,
					  send, receive);
	     else
		 ret = send_and_recv_udp (fd, context->kdc_timeout,
					  send, receive);
	     close (fd);
	     if(ret == 0 && receive->length != 0)
		 goto out;
	 }
     ret = KRB5_KDC_UNREACH;
out:
     return ret;
}

krb5_error_code
krb5_sendto_kdc2(krb5_context context,
		 const krb5_data *send,
		 const krb5_realm *realm,
		 krb5_data *receive,
		 krb5_boolean master)
{
    krb5_error_code ret;
    char **hostlist;
    int port;
    
    port = krb5_getportbyname (context, "kerberos", "udp", 88);
    
    if (master || context->use_admin_kdc)
	ret = krb5_get_krb_admin_hst (context, realm, &hostlist);
    else
	ret = krb5_get_krbhst (context, realm, &hostlist);
    if (ret)
	return ret;
    ret = krb5_sendto(context, send, hostlist, port, receive);
    krb5_free_krbhst (context, hostlist);
    return ret;
}

krb5_error_code
krb5_sendto_kdc(krb5_context context,
		const krb5_data *send,
		const krb5_realm *realm,
		krb5_data *receive)
{
    return krb5_sendto_kdc2(context, send, realm, receive, FALSE);
}
