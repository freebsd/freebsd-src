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

#include <popper.h>
#include <base64.h>
RCSID("$Id: pop_auth.c,v 1.2 2000/04/12 15:37:45 assar Exp $");

#ifdef KRB4

enum {
    NO_PROT   = 1,
    INT_PROT  = 2,
    PRIV_PROT = 4
};

static int
auth_krb4(POP *p)
{
    int ret;
    des_cblock key;
    u_int32_t nonce, nonce_reply;
    u_int32_t max_client_packet;
    int protocols = NO_PROT | INT_PROT | PRIV_PROT;
    char data[8];
    int len;
    char *s;
    char instance[INST_SZ];  
    KTEXT_ST authent;
    des_key_schedule schedule;
    struct passwd *pw;

    /* S -> C: 32 bit nonce in MSB base64 */

    des_new_random_key(&key);
    nonce = (key[0] | (key[1] << 8) | (key[2] << 16) | (key[3] << 24)
	     | key[4] | (key[5] << 8) | (key[6] << 16) | (key[7] << 24));
    krb_put_int(nonce, data, 4, 8);
    len = base64_encode(data, 4, &s);

    pop_msg(p, POP_CONTINUE, "%s", s);
    free(s);

    /* C -> S: ticket and authenticator */

    ret = sch_readline(p->input, &s);
    if (ret <= 0 || strcmp (s, "*") == 0)
	return pop_msg(p, POP_FAILURE,
		       "authentication aborted by client");
    len = strlen(s);
    if (len > sizeof(authent.dat)) {
	return pop_msg(p, POP_FAILURE, "data packet too long");
    }

    authent.length = base64_decode(s, authent.dat);

    k_getsockinst (0, instance, sizeof(instance));
    ret = krb_rd_req(&authent, "pop", instance,
		     p->in_addr.sin_addr.s_addr,
		     &p->kdata, NULL);
    if (ret != 0) {
	return pop_msg(p, POP_FAILURE, "rd_req: %s",
		       krb_get_err_text(ret));
    }
    if (p->kdata.checksum != nonce) {
	return pop_msg(p, POP_FAILURE, "data stream modified");
    }

    /* S -> C: nonce + 1 | bit | max segment */

    krb_put_int(nonce + 1, data, 4, 7);
    data[4] = protocols;
    krb_put_int(1024, data + 5, 3, 3); /* XXX */
    des_key_sched(&p->kdata.session, schedule);
    des_pcbc_encrypt((des_cblock*)data,
		     (des_cblock*)data, 8,
		     schedule,
		     &p->kdata.session,
		     DES_ENCRYPT);
    len = base64_encode(data, 8, &s);
    pop_msg(p, POP_CONTINUE, "%s", s);

    free(s);

    /* C -> S: nonce | bit | max segment | username */

    ret = sch_readline(p->input, &s);
    if (ret <= 0 || strcmp (s, "*") == 0)
	return pop_msg(p, POP_FAILURE,
		       "authentication aborted");
    len = strlen(s);
    if (len > sizeof(authent.dat)) {
	return pop_msg(p, POP_FAILURE, "data packet too long");
    }

    authent.length = base64_decode(s, authent.dat);
    
    if (authent.length % 8 != 0) {
	return pop_msg(p, POP_FAILURE, "reply is not a multiple of 8 bytes");
    }

    des_key_sched(&p->kdata.session, schedule);
    des_pcbc_encrypt((des_cblock*)authent.dat,
		     (des_cblock*)authent.dat,
		     authent.length,
		     schedule,
		     &p->kdata.session,
		     DES_DECRYPT);

    krb_get_int(authent.dat, &nonce_reply, 4, 0);
    if (nonce_reply != nonce) {
	return pop_msg(p, POP_FAILURE, "data stream modified");
    }
    protocols &= authent.dat[4];
    krb_get_int(authent.dat + 5, &max_client_packet, 3, 0);
    if(authent.dat[authent.length - 1] != '\0') {
	return pop_msg(p, POP_FAILURE, "bad format of username");
    }
    strncpy (p->user, authent.dat + 8, sizeof(p->user));
    pw = k_getpwnam(p->user);
    if (pw == NULL) {
	return (pop_msg(p,POP_FAILURE,
			"Password supplied for \"%s\" is incorrect.",
			p->user));
    }

    if (kuserok(&p->kdata, p->user)) {
	pop_log(p, POP_PRIORITY,
		"%s: (%s.%s@%s) tried to retrieve mail for %s.",
		p->client, p->kdata.pname, p->kdata.pinst,
		p->kdata.prealm, p->user);
	return(pop_msg(p,POP_FAILURE,
		       "Popping not authorized"));
    }
    pop_log(p, POP_INFO, "%s: %s.%s@%s -> %s",
	    p->ipaddr,
	    p->kdata.pname, p->kdata.pinst, p->kdata.prealm,
	    p->user);
    ret = pop_login(p, pw);
    if (protocols & PRIV_PROT)
	;
    else if (protocols & INT_PROT)
	;
    else
	;
    
    return ret;
}
#endif /* KRB4 */

#ifdef KRB5
static int
auth_gssapi(POP *p)
{
    
}
#endif /* KRB5 */

/* 
 *  auth: RFC1734
 */

static struct {
    const char *name;
    int (*func)(POP *);
} methods[] = {
#ifdef KRB4
    {"KERBEROS_V4",	auth_krb4},
#endif
#ifdef KRB5
    {"GSSAPI",		auth_gssapi},
#endif
    {NULL,		NULL}
};

int
pop_auth (POP *p)
{
    int i;

    for (i = 0; methods[i].name != NULL; ++i)
	if (strcasecmp(p->pop_parm[1], methods[i].name) == 0)
	    return (*methods[i].func)(p);
    return pop_msg(p, POP_FAILURE,
		   "Authentication method %s unknown", p->pop_parm[1]);
}
