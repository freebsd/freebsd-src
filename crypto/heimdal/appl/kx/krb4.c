/*
 * Copyright (c) 1995 - 2000 Kungliga Tekniska Högskolan
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

RCSID("$Id: krb4.c,v 1.8 2000/10/08 13:19:22 assar Exp $");

#ifdef KRB4

struct krb4_kx_context {
    des_cblock key;
    des_key_schedule schedule;
    AUTH_DAT auth;
};

typedef struct krb4_kx_context krb4_kx_context;

/*
 * Destroy the krb4 context in `c'.
 */

static void
krb4_destroy (kx_context *c)
{
    memset (c->data, 0, sizeof(krb4_kx_context));
    free (c->data);
}

/*
 * Read the authentication information from `s' and return 0 if
 * succesful, else -1.
 */

static int
krb4_authenticate (kx_context *kc, int s)
{
    CREDENTIALS cred;
    KTEXT_ST text;
    MSG_DAT msg;
    int status;
    krb4_kx_context *c = (krb4_kx_context *)kc->data;
    const char *host = kc->host;

#ifdef HAVE_KRB_GET_OUR_IP_FOR_REALM
    if (krb_get_config_bool("nat_in_use")) {
	struct in_addr natAddr;

	if (krb_get_our_ip_for_realm(krb_realmofhost(kc->host),
				     &natAddr) == KSUCCESS
	    || krb_get_our_ip_for_realm (NULL, &natAddr) == KSUCCESS)
	    kc->thisaddr.sin_addr = natAddr;
    }
#endif

    status = krb_sendauth (KOPT_DO_MUTUAL, s, &text, "rcmd",
			   (char *)host, krb_realmofhost (host),
			   getpid(), &msg, &cred, c->schedule,
			   &kc->thisaddr, &kc->thataddr, KX_VERSION);
    if (status != KSUCCESS) {
	warnx ("%s: %s\n", host, krb_get_err_text(status));
	return -1;
    }
    memcpy (c->key, cred.session, sizeof(des_cblock));
    return 0;
}

/*
 * Read a krb4 priv packet from `fd' into `buf' (of size `len').
 * Return the number of bytes read or 0 on EOF or -1 on error.
 */

static ssize_t
krb4_read (kx_context *kc,
	   int fd, void *buf, size_t len)
{
    unsigned char tmp[4];
    ssize_t ret;
    size_t l;
    int status;
    krb4_kx_context *c = (krb4_kx_context *)kc->data;
    MSG_DAT msg;

    ret = krb_net_read (fd, tmp, 4);
    if (ret == 0)
	return ret;
    if (ret != 4)
	return -1;
    l = (tmp[0] << 24) | (tmp[1] << 16) | (tmp[2] << 8) | tmp[3];
    if (l > len)
	return -1;
    if (krb_net_read (fd, buf, l) != l)
	return -1;
    status = krb_rd_priv (buf, l, c->schedule, &c->key,
			  &kc->thataddr, &kc->thisaddr, &msg);
    if (status != RD_AP_OK) {
	warnx ("krb4_read: %s", krb_get_err_text(status));
	return -1;
    }
    memmove (buf, msg.app_data, msg.app_length);
    return msg.app_length;
}

/*
 * Write a krb4 priv packet on `fd' with the data in `buf, len'.
 * Return len or -1 on error
 */

static ssize_t
krb4_write(kx_context *kc,
	   int fd, const void *buf, size_t len)
{
    void *outbuf;
    krb4_kx_context *c = (krb4_kx_context *)kc->data;
    int outlen;
    unsigned char tmp[4];

    outbuf = malloc (len + 30);
    if (outbuf == NULL)
	return -1;
    outlen = krb_mk_priv ((void *)buf, outbuf, len, c->schedule, &c->key,
			  &kc->thisaddr, &kc->thataddr);
    if (outlen < 0) {
	free (outbuf);
	return -1;
    }
    tmp[0] = (outlen >> 24) & 0xFF;
    tmp[1] = (outlen >> 16) & 0xFF;
    tmp[2] = (outlen >>  8) & 0xFF;
    tmp[3] = (outlen >>  0) & 0xFF;

    if (krb_net_write (fd, tmp, 4) != 4 ||
	krb_net_write (fd, outbuf, outlen) != outlen) {
	free (outbuf);
	return -1;
    }
    free (outbuf);
    return len;
}

/*
 * Copy data from `fd1' to `fd2', {en,de}crypting with cfb64
 * with `mode' and state stored in `iv', `schedule', and `num'.
 * Return -1 if error, 0 if eof, else 1
 */

static int
do_enccopy (int fd1, int fd2, int mode, des_cblock *iv,
	    des_key_schedule schedule, int *num)
{
     int ret;
     u_char buf[BUFSIZ];

     ret = read (fd1, buf, sizeof(buf));
     if (ret == 0)
	  return 0;
     if (ret < 0) {
	 warn ("read");
	 return ret;
     }
#ifndef NOENCRYPTION
     des_cfb64_encrypt (buf, buf, ret, schedule, iv,
			num, mode);
#endif
     ret = krb_net_write (fd2, buf, ret);
     if (ret < 0) {
	 warn ("write");
	 return ret;
     }
     return 1;
}

/*
 * Copy data between fd1 and fd2, encrypting one way and decrypting
 * the other.
 */

static int
krb4_copy_encrypted (kx_context *kc,
		     int fd1, int fd2)
{
    krb4_kx_context *c = (krb4_kx_context *)kc->data;
    des_cblock iv1, iv2;
    int num1 = 0, num2 = 0;

    memcpy (iv1, c->key, sizeof(iv1));
    memcpy (iv2, c->key, sizeof(iv2));
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
	    ret = do_enccopy (fd1, fd2, DES_ENCRYPT, &iv1, c->schedule, &num1);
	    if (ret <= 0)
		return ret;
	}
	if (FD_ISSET(fd2, &fdset)) {
	    ret = do_enccopy (fd2, fd1, DES_DECRYPT, &iv2, c->schedule, &num2);
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
krb4_userok (kx_context *kc, char *user)
{
    krb4_kx_context *c = (krb4_kx_context *)kc->data;
    char *tmp;

    tmp = krb_unparse_name_long (c->auth.pname,
				 c->auth.pinst,
				 c->auth.prealm);
    kc->user = strdup (tmp);
    if (kc->user == NULL)
	err (1, "malloc");


    return kuserok (&c->auth, user);
}

/*
 * Create an instance of an krb4 context.
 */

void
krb4_make_context (kx_context *kc)
{
    kc->authenticate	= krb4_authenticate;
    kc->userok		= krb4_userok;
    kc->read		= krb4_read;
    kc->write		= krb4_write;
    kc->copy_encrypted	= krb4_copy_encrypted;
    kc->destroy		= krb4_destroy;
    kc->user		= NULL;
    kc->data		= malloc(sizeof(krb4_kx_context));

    if (kc->data == NULL)
	err (1, "malloc");
}

/*
 * Receive authentication information on `sock' (first four bytes
 * in `buf').
 */

int
recv_v4_auth (kx_context *kc, int sock, u_char *buf)
{
    int status;
    KTEXT_ST ticket;
    char instance[INST_SZ + 1];
    char version[KRB_SENDAUTH_VLEN + 1];
    krb4_kx_context *c;
    AUTH_DAT auth;
    des_key_schedule schedule;

    if (memcmp (buf, KRB_SENDAUTH_VERS, 4) != 0)
	return -1;
    if (net_read (sock, buf + 4, KRB_SENDAUTH_VLEN - 4) !=
	KRB_SENDAUTH_VLEN - 4) {
	syslog (LOG_ERR, "read: %m");
	exit (1);
    }
    if (memcmp (buf, KRB_SENDAUTH_VERS, KRB_SENDAUTH_VLEN) != 0) {
	syslog (LOG_ERR, "unrecognized auth protocol: %.8s", buf);
	exit (1);
    }

    k_getsockinst (sock, instance, sizeof(instance));
    status = krb_recvauth (KOPT_IGNORE_PROTOCOL | KOPT_DO_MUTUAL,
			   sock,
			   &ticket,
			   "rcmd",
			   instance,
			   &kc->thataddr,
			   &kc->thisaddr,
			   &auth,
			   "",
			   schedule,
			   version);
    if (status != KSUCCESS) {
	syslog (LOG_ERR, "krb_recvauth: %s", krb_get_err_text(status));
	exit (1);
    }
    if (strncmp (version, KX_VERSION, KRB_SENDAUTH_VLEN) != 0) {
	 /* Try to be nice to old kx's */
	 if (strncmp (version, KX_OLD_VERSION, KRB_SENDAUTH_VLEN) == 0) {
	     char *old_errmsg = "\001Old version of kx. Please upgrade.";
	     char user[64];

	     syslog (LOG_ERR, "Old version client (%s)", version);

	     krb_net_read (sock, user, sizeof(user));
	     krb_net_write (sock, old_errmsg, strlen(old_errmsg) + 1);
	     exit (1);
	 } else {
	     syslog (LOG_ERR, "bad version: %s", version);
	     exit (1);
	 }
    }

    krb4_make_context (kc);
    c = (krb4_kx_context *)kc->data;

    c->auth     = auth;
    memcpy (c->key, &auth.session, sizeof(des_cblock));
    memcpy (c->schedule, schedule, sizeof(schedule));

    return 0;
}

#endif /* KRB4 */
