/*
 * Copyright (c) 1995, 1996, 1997 Kungliga Tekniska Högskolan
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Kungliga Tekniska
 *      Högskolan and its contributors.
 * 
 * 4. Neither the name of the Institute nor the names of its contributors
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

#include "kauth.h"

RCSID("$Id: rkinit.c,v 1.19 1997/04/01 08:17:33 joda Exp $");

static struct in_addr *
getalladdrs (char *hostname, unsigned *count)
{
    struct hostent *hostent;
    struct in_addr **h;
    struct in_addr *addr;
    unsigned naddr;
    unsigned maxaddr;

    hostent = gethostbyname (hostname);
    if (hostent == NULL) {
	warnx ("gethostbyname '%s' failed: %s\n",
	       hostname,
#ifdef HAVE_H_ERRNO
	       hstrerror(h_errno)
#else
	       "unknown error"
#endif
	       );
	return NULL;
    }
    maxaddr = 1;
    naddr = 0;
    addr = malloc(sizeof(*addr) * maxaddr);
    if (addr == NULL) {
	warnx ("out of memory");
	return NULL;
    }
    for (h = (struct in_addr **)(hostent->h_addr_list);
	 *h != NULL;
	 h++) {
	if (naddr >= maxaddr) {
	    maxaddr *= 2;
	    addr = realloc (addr, sizeof(*addr) * maxaddr);
	    if (addr == NULL) {
		warnx ("out of memory");
		return NULL;
	    }
	}
	addr[naddr++] = **h;
    }
    addr = realloc (addr, sizeof(*addr) * naddr);
    if (addr == NULL) {
	warnx ("out of memory");
	return NULL;
    }
    *count = naddr;
    return addr;
}

static int
doit_host (krb_principal *princ, int lifetime, char *locuser, 
	   char *tktfile, des_cblock *key, int s, char *hostname)
{
    char buf[BUFSIZ];
    int inlen;
    KTEXT_ST text;
    CREDENTIALS cred;
    MSG_DAT msg;
    int status;
    des_key_schedule schedule;
    struct sockaddr_in thisaddr, thataddr;
    int addrlen;
    void *ret;

    addrlen = sizeof(thisaddr);
    if (getsockname (s, (struct sockaddr *)&thisaddr, &addrlen) < 0 ||
	addrlen != sizeof(thisaddr)) {
	warn ("getsockname(%s)", hostname);
	return 1;
    }
    addrlen = sizeof(thataddr);
    if (getpeername (s, (struct sockaddr *)&thataddr, &addrlen) < 0 ||
	addrlen != sizeof(thataddr)) {
	warn ("getpeername(%s)", hostname);
	return 1;
    }

    status = krb_sendauth (KOPT_DO_MUTUAL, s, &text, "rcmd",
			   hostname, krb_realmofhost (hostname),
			   getpid(), &msg, &cred, schedule,
			   &thisaddr, &thataddr, KAUTH_VERSION);
    if (status != KSUCCESS) {
	warnx ("%s: %s\n", hostname, krb_get_err_text(status));
	return 1;
    }
    inlen = pack_args (buf, princ, lifetime, locuser, tktfile);

    if (write_encrypted(s, buf, inlen, schedule, &cred.session,
			&thisaddr, &thataddr) < 0) {
	warn ("write to %s", hostname);
	return 1;
    }

    inlen = read_encrypted (s, buf, sizeof(buf), &ret, schedule,
			    &cred.session, &thataddr, &thisaddr);
    if (inlen < 0) {
	warn ("read from %s failed", hostname);
	return 1;
    }

    if (strncmp(ret, "ok", inlen) != 0) {
	warnx ("error from %s: %.*s\n",
	       hostname, inlen, (char *)ret);
	return 1;
    }

    inlen = read_encrypted (s, buf, sizeof(buf), &ret, schedule,
			    &cred.session, &thataddr, &thisaddr);
    if (inlen < 0) {
	warn ("read from %s", hostname);
	return 1;
    }
     
    {
	des_key_schedule key_s;

	des_key_sched(key, key_s);
	des_pcbc_encrypt(ret, ret, inlen, key_s, key, DES_DECRYPT);
	memset(key_s, 0, sizeof(key_s));
    }
    write_encrypted (s, ret, inlen, schedule, &cred.session,
		     &thisaddr, &thataddr);

    inlen = read_encrypted (s, buf, sizeof(buf), &ret, schedule,
			    &cred.session, &thataddr, &thisaddr);
    if (inlen < 0) {
	warn ("read from %s", hostname);
	return 1;
    }

    if (strncmp(ret, "ok", inlen) != 0) {
	warnx ("error from %s: %.*s\n",
	       hostname, inlen, (char *)ret);
	return 1;
    }
    return 0;
}

int
rkinit (krb_principal *princ, int lifetime, char *locuser, 
	char *tktfile, des_cblock *key, char *hostname)
{
    struct in_addr *addr;
    unsigned naddr;
    unsigned i;
    int port;
    int success;

    addr = getalladdrs (hostname, &naddr);
    if (addr == NULL)
	return 1;
    port = k_getportbyname ("kauth", "tcp", htons(KAUTH_PORT));
    success = 0;
    for (i = 0; !success && i < naddr; ++i) {
	struct sockaddr_in a;
	int s;

	memset(&a, 0, sizeof(a));
	a.sin_family = AF_INET;
	a.sin_port   = port;
	a.sin_addr   = addr[i];

	s = socket (AF_INET, SOCK_STREAM, 0);
	if (s < 0) {
	    warn("socket");
	    return 1;
	}
	if (connect(s, (struct sockaddr *)&a, sizeof(a)) < 0) {
	    warn("connect(%s)", hostname);
	    continue;
	}

	success = success || !doit_host (princ, lifetime,
					 locuser, tktfile, key,
					 s, hostname);
	close (s);
    }
    return !success;
}
