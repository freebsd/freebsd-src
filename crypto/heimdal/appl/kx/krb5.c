/*
 * Copyright (c) 1995 - 2000, 2002 Kungliga Tekniska Högskolan
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

#include "kx.h"

RCSID("$Id: krb5.c,v 1.9 2002/05/24 15:13:52 joda Exp $");

#ifdef KRB5

struct krb5_kx_context {
    krb5_context context;
    krb5_keyblock *keyblock;
    krb5_crypto crypto;
    krb5_principal client;
};

typedef struct krb5_kx_context krb5_kx_context;

/*
 * Destroy the krb5 context in `c'.
 */

static void
krb5_destroy (kx_context *c)
{
    krb5_kx_context *kc = (krb5_kx_context *)c->data;

    if (kc->keyblock)
	krb5_free_keyblock (kc->context, kc->keyblock);
    if (kc->crypto)
	krb5_crypto_destroy (kc->context, kc->crypto);
    if (kc->client)
	krb5_free_principal (kc->context, kc->client);
    if (kc->context)
	krb5_free_context (kc->context);
    free (kc);
}

/*
 * Read the authentication information from `s' and return 0 if
 * succesful, else -1.
 */

static int
krb5_authenticate (kx_context *kc, int s)
{
    krb5_kx_context *c = (krb5_kx_context *)kc->data;
    krb5_context context = c->context;
    krb5_auth_context auth_context = NULL;
    krb5_error_code ret;
    krb5_principal server;
    const char *host = kc->host;

    ret = krb5_sname_to_principal (context,
				   host, "host", KRB5_NT_SRV_HST, &server);
    if (ret) {
	krb5_warn (context, ret, "krb5_sname_to_principal: %s", host);
	return 1;
    }

    ret = krb5_sendauth (context,
			 &auth_context,
			 &s,
			 KX_VERSION,
			 NULL,
			 server,
			 AP_OPTS_MUTUAL_REQUIRED,
			 NULL,
			 NULL,
			 NULL,
			 NULL,
			 NULL,
			 NULL);
    if (ret) {
	if(ret != KRB5_SENDAUTH_BADRESPONSE)
	    krb5_warn (context, ret, "krb5_sendauth: %s", host);
	return 1;
    }

    ret = krb5_auth_con_getkey (context, auth_context, &c->keyblock);
    if (ret) {
	krb5_warn (context, ret, "krb5_auth_con_getkey: %s", host);
	krb5_auth_con_free (context, auth_context);
	return 1;
    }
    
    ret = krb5_crypto_init (context, c->keyblock, 0, &c->crypto);
    if (ret) {
	krb5_warn (context, ret, "krb5_crypto_init");
	krb5_auth_con_free (context, auth_context);
	return 1;
    }
    return 0;
}

/*
 * Read an encapsulated krb5 packet from `fd' into `buf' (of size
 * `len').  Return the number of bytes read or 0 on EOF or -1 on
 * error.
 */

static ssize_t
krb5_read (kx_context *kc,
	   int fd, void *buf, size_t len)
{
    krb5_kx_context *c = (krb5_kx_context *)kc->data;
    krb5_context context = c->context;
    size_t data_len, outer_len;
    krb5_error_code ret;
    unsigned char tmp[4];
    krb5_data data;
    int l;

    l = krb5_net_read (context, &fd, tmp, 4);
    if (l == 0)
	return l;
    if (l != 4)
	return -1;
    data_len  = (tmp[0] << 24) | (tmp[1] << 16) | (tmp[2] << 8) | tmp[3];
    outer_len = krb5_get_wrapped_length (context, c->crypto, data_len);
    if (outer_len > len)
	return -1;
    if (krb5_net_read (context, &fd, buf, outer_len) != outer_len)
	return -1;

    ret = krb5_decrypt (context, c->crypto, KRB5_KU_OTHER_ENCRYPTED,
			buf, outer_len, &data);
    if (ret) {
	krb5_warn (context, ret, "krb5_decrypt");
	return -1;
    }
    if (data_len > data.length) {
	krb5_data_free (&data);
	return -1;
    }
    memmove (buf, data.data, data_len);
    krb5_data_free (&data);
    return data_len;
}

/*
 * Write an encapsulated krb5 packet on `fd' with the data in `buf,
 * len'.  Return len or -1 on error.
 */

static ssize_t
krb5_write(kx_context *kc,
	   int fd, const void *buf, size_t len)
{
    krb5_kx_context *c = (krb5_kx_context *)kc->data;
    krb5_context context = c->context;
    krb5_data data;
    krb5_error_code ret;
    unsigned char tmp[4];
    size_t outlen;

    ret = krb5_encrypt (context, c->crypto, KRB5_KU_OTHER_ENCRYPTED,
			(void *)buf, len, &data);
    if (ret){
	krb5_warn (context, ret, "krb5_write");
	return -1;
    }

    outlen = data.length;
    tmp[0] = (len >> 24) & 0xFF;
    tmp[1] = (len >> 16) & 0xFF;
    tmp[2] = (len >>  8) & 0xFF;
    tmp[3] = (len >>  0) & 0xFF;

    if (krb5_net_write (context, &fd, tmp, 4) != 4 ||
	krb5_net_write (context, &fd, data.data, outlen) != outlen) {
	krb5_data_free (&data);
	return -1;
    }
    krb5_data_free (&data);
    return len;
}

/*
 * Copy from the unix socket `from_fd' encrypting to `to_fd'.
 * Return 0, -1 or len.
 */

static int
copy_out (kx_context *kc, int from_fd, int to_fd)
{
    char buf[32768];
    ssize_t len;

    len = read (from_fd, buf, sizeof(buf));
    if (len == 0)
	return 0;
    if (len < 0) {
	warn ("read");
	return len;
    }
    return krb5_write (kc, to_fd, buf, len);
}
	
/*
 * Copy from the socket `from_fd' decrypting to `to_fd'.
 * Return 0, -1 or len.
 */

static int
copy_in (kx_context *kc, int from_fd, int to_fd)
{
    krb5_kx_context *c = (krb5_kx_context *)kc->data;
    char buf[33000];		/* XXX */

    ssize_t len;

    len = krb5_read (kc, from_fd, buf, sizeof(buf));
    if (len == 0)
	return 0;
    if (len < 0) {
	warn ("krb5_read");
	return len;
    }

    return krb5_net_write (c->context, &to_fd, buf, len);
}

/*
 * Copy data between `fd1' and `fd2', encrypting in one direction and
 * decrypting in the other.
 */

static int
krb5_copy_encrypted (kx_context *kc, int fd1, int fd2)
{
    for (;;) {
	fd_set fdset;
	int ret;

	if (fd1 >= FD_SETSIZE || fd2 >= FD_SETSIZE) {
	    warnx ("fd too large");
	    return 1;
	}

	FD_ZERO(&fdset);
	FD_SET(fd1, &fdset);
	FD_SET(fd2, &fdset);

	ret = select (max(fd1, fd2)+1, &fdset, NULL, NULL, NULL);
	if (ret < 0 && errno != EINTR) {
	    warn ("select");
	    return 1;
	}
	if (FD_ISSET(fd1, &fdset)) {
	    ret = copy_out (kc, fd1, fd2);
	    if (ret <= 0)
		return ret;
	}
	if (FD_ISSET(fd2, &fdset)) {
	    ret = copy_in (kc, fd2, fd1);
	    if (ret <= 0)
		return ret;
	}
    }
}

/*
 * Return 0 if the user authenticated on `kc' is allowed to login as
 * `user'.
 */

static int
krb5_userok (kx_context *kc, char *user)
{
    krb5_kx_context *c = (krb5_kx_context *)kc->data;
    krb5_context context = c->context;
    krb5_error_code ret;
    char *tmp;

    ret = krb5_unparse_name (context, c->client, &tmp);
    if (ret)
	krb5_err (context, 1, ret, "krb5_unparse_name");
    kc->user = tmp;

    return !krb5_kuserok (context, c->client, user);
}

/*
 * Create an instance of an krb5 context.
 */

void
krb5_make_context (kx_context *kc)
{
    krb5_kx_context *c;
    krb5_error_code ret;

    kc->authenticate	= krb5_authenticate;
    kc->userok		= krb5_userok;
    kc->read		= krb5_read;
    kc->write		= krb5_write;
    kc->copy_encrypted	= krb5_copy_encrypted;
    kc->destroy		= krb5_destroy;
    kc->user		= NULL;
    kc->data		= malloc(sizeof(krb5_kx_context));

    if (kc->data == NULL)
	err (1, "malloc");
    memset (kc->data, 0, sizeof(krb5_kx_context));
    c = (krb5_kx_context *)kc->data;
    ret = krb5_init_context (&c->context);
    if (ret)
	errx (1, "krb5_init_context failed: %d", ret);
}

/*
 * Receive authentication information on `sock' (first four bytes
 * in `buf').
 */

int
recv_v5_auth (kx_context *kc, int sock, u_char *buf)
{
    u_int32_t len;
    krb5_error_code ret;
    krb5_kx_context *c;
    krb5_context context;
    krb5_principal server;
    krb5_auth_context auth_context = NULL;
    krb5_ticket *ticket;

    if (memcmp (buf, "\x00\x00\x00\x13", 4) != 0)
	return 1;
    len = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | (buf[3]);
    if (net_read(sock, buf, len) != len) {
	syslog (LOG_ERR, "read: %m");
	exit (1);
    }
    if (len != sizeof(KRB5_SENDAUTH_VERSION)
	|| memcmp (buf, KRB5_SENDAUTH_VERSION, len) != 0) {
	syslog (LOG_ERR, "bad sendauth version: %.8s", buf);
	exit (1);
    }

    krb5_make_context (kc);
    c = (krb5_kx_context *)kc->data;
    context = c->context;

    ret = krb5_sock_to_principal (context, sock, "host",
				  KRB5_NT_SRV_HST, &server);
    if (ret) {
	syslog (LOG_ERR, "krb5_sock_to_principal: %s",
		krb5_get_err_text (context, ret));
	exit (1);
    }

    ret = krb5_recvauth (context,
			 &auth_context,
			 &sock,
			 KX_VERSION,
			 server,
			 KRB5_RECVAUTH_IGNORE_VERSION,
			 NULL,
			 &ticket);
    krb5_free_principal (context, server);
    if (ret) {
	syslog (LOG_ERR, "krb5_sock_to_principal: %s",
		krb5_get_err_text (context, ret));
	exit (1);
    }

    ret = krb5_auth_con_getkey (context, auth_context, &c->keyblock);
    if (ret) {
	syslog (LOG_ERR, "krb5_auth_con_getkey: %s",
		krb5_get_err_text (context, ret));
	exit (1);
    }

    ret = krb5_crypto_init (context, c->keyblock, 0, &c->crypto);
    if (ret) {
	syslog (LOG_ERR, "krb5_crypto_init: %s",
		krb5_get_err_text (context, ret));
	exit (1);
    }

    c->client = ticket->client;
    ticket->client = NULL;
    krb5_free_ticket (context, ticket);

    return 0;
}

#endif /* KRB5 */
