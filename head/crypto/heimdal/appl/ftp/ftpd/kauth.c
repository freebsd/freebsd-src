/*
 * Copyright (c) 1995 - 1999, 2003 Kungliga Tekniska Högskolan
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

#include "ftpd_locl.h"

RCSID("$Id: kauth.c 15666 2005-07-19 17:08:11Z lha $");

#if defined(KRB4) || defined(KRB5)

int do_destroy_tickets = 1;
char *k5ccname;

#endif

#ifdef KRB4

static KTEXT_ST cip;
static unsigned int lifetime;
static time_t local_time;

static krb_principal pr;

static int
save_tkt(const char *user,
	 const char *instance,
	 const char *realm,
	 const void *arg, 
	 key_proc_t key_proc,
	 KTEXT *cipp)
{
    local_time = time(0);
    memmove(&cip, *cipp, sizeof(cip));
    return -1;
}

static int
store_ticket(KTEXT cip)
{
    char *ptr;
    des_cblock session;
    krb_principal sp;
    unsigned char kvno;
    KTEXT_ST tkt;
    int left = cip->length;
    int len;
    int kerror;
    
    ptr = (char *) cip->dat;

    /* extract session key */
    memmove(session, ptr, 8);
    ptr += 8;
    left -= 8;

    len = strnlen(ptr, left);
    if (len == left)
	return(INTK_BADPW);
    
    /* extract server's name */
    strlcpy(sp.name, ptr, sizeof(sp.name));
    ptr += len + 1;
    left -= len + 1;

    len = strnlen(ptr, left);
    if (len == left)
	return(INTK_BADPW);
    
    /* extract server's instance */
    strlcpy(sp.instance, ptr, sizeof(sp.instance));
    ptr += len + 1;
    left -= len + 1;

    len = strnlen(ptr, left);
    if (len == left)
	return(INTK_BADPW);
    
    /* extract server's realm */
    strlcpy(sp.realm, ptr, sizeof(sp.realm));
    ptr += len + 1;
    left -= len + 1;

    if(left < 3)
	return INTK_BADPW;
    /* extract ticket lifetime, server key version, ticket length */
    /* be sure to avoid sign extension on lifetime! */
    lifetime = (unsigned char) ptr[0];
    kvno = (unsigned char) ptr[1];
    tkt.length = (unsigned char) ptr[2];
    ptr += 3;
    left -= 3;
    
    if (tkt.length > left)
	return(INTK_BADPW);

    /* extract ticket itself */
    memmove(tkt.dat, ptr, tkt.length);
    ptr += tkt.length;
    left -= tkt.length;

    /* Here is where the time should be verified against the KDC.
     * Unfortunately everything is sent in host byte order (receiver
     * makes wrong) , and at this stage there is no way for us to know
     * which byteorder the KDC has. So we simply ignore the time,
     * there are no security risks with this, the only thing that can
     * happen is that we might receive a replayed ticket, which could
     * at most be useless.
     */
    
#if 0
    /* check KDC time stamp */
    {
	time_t kdc_time;

	memmove(&kdc_time, ptr, sizeof(kdc_time));
	if (swap_bytes) swap_u_long(kdc_time);

	ptr += 4;
    
	if (abs((int)(local_time - kdc_time)) > CLOCK_SKEW) {
	    return(RD_AP_TIME);		/* XXX should probably be better
					   code */
	}
    }
#endif

    /* initialize ticket cache */

    if (tf_create(TKT_FILE) != KSUCCESS)
	return(INTK_ERR);

    if (tf_put_pname(pr.name) != KSUCCESS ||
	tf_put_pinst(pr.instance) != KSUCCESS) {
	tf_close();
	return(INTK_ERR);
    }

    
    kerror = tf_save_cred(sp.name, sp.instance, sp.realm, session, 
			  lifetime, kvno, &tkt, local_time);
    tf_close();

    return(kerror);
}

void
kauth(char *principal, char *ticket)
{
    char *p;
    int ret;
  
    if(get_command_prot() != prot_private) {
	reply(500, "Request denied (bad protection level)");
	return;
    }
    ret = krb_parse_name(principal, &pr);
    if(ret){
	reply(500, "Bad principal: %s.", krb_get_err_text(ret));
	return;
    }
    if(pr.realm[0] == 0)
	krb_get_lrealm(pr.realm, 1);

    if(ticket){
	cip.length = base64_decode(ticket, &cip.dat);
	if(cip.length == -1){
	    reply(500, "Failed to decode data.");
	    return;
	}
	ret = store_ticket(&cip);
	if(ret){
	    reply(500, "Kerberos error: %s.", krb_get_err_text(ret));
	    memset(&cip, 0, sizeof(cip));
	    return;
	}
	do_destroy_tickets = 1;

	if(k_hasafs())
	    krb_afslog(0, 0);
	reply(200, "Tickets will be destroyed on exit.");
	return;
    }
    
    ret = krb_get_in_tkt (pr.name,
			  pr.instance,
			  pr.realm,
			  KRB_TICKET_GRANTING_TICKET,
			  pr.realm,
			  DEFAULT_TKT_LIFE,
			  NULL, save_tkt, NULL);
    if(ret != INTK_BADPW){
	reply(500, "Kerberos error: %s.", krb_get_err_text(ret));
	return;
    }
    if(base64_encode(cip.dat, cip.length, &p) < 0) {
	reply(500, "Out of memory while base64-encoding.");
	return;
    }
    reply(300, "P=%s T=%s", krb_unparse_name(&pr), p);
    free(p);
    memset(&cip, 0, sizeof(cip));
}


static char *
short_date(int32_t dp)
{
    char *cp;
    time_t t = (time_t)dp;

    if (t == (time_t)(-1L)) return "***  Never  *** ";
    cp = ctime(&t) + 4;
    cp[15] = '\0';
    return (cp);
}

void
krbtkfile(const char *tkfile)
{
    do_destroy_tickets = 0;
    krb_set_tkt_string(tkfile);
    reply(200, "Using ticket file %s", tkfile);
}

#endif /* KRB4 */

#ifdef KRB5

static void
dest_cc(void)
{
    krb5_context context;
    krb5_error_code ret;
    krb5_ccache id;
    
    ret = krb5_init_context(&context);
    if (ret == 0) {
	if (k5ccname)
	    ret = krb5_cc_resolve(context, k5ccname, &id);
	else
	    ret = krb5_cc_default (context, &id);
	if (ret)
	    krb5_free_context(context);
    }
    if (ret == 0) {
	krb5_cc_destroy(context, id);
	krb5_free_context (context);
    }
}
#endif

#if defined(KRB4) || defined(KRB5)

/*
 * Only destroy if we created the tickets
 */

void
cond_kdestroy(void)
{
    if (do_destroy_tickets) {
#if KRB4
	dest_tkt();
#endif
#if KRB5
	dest_cc();
#endif
	do_destroy_tickets = 0;
    }
    afsunlog();
}

void
kdestroy(void)
{
#if KRB4
    dest_tkt();
#endif
#if KRB5
    dest_cc();
#endif
    afsunlog();
    reply(200, "Tickets destroyed");
}


void
afslog(const char *cell, int quiet)
{
    if(k_hasafs()) {
#ifdef KRB5
	krb5_context context;
	krb5_error_code ret;
	krb5_ccache id;

	ret = krb5_init_context(&context);
	if (ret == 0) {
	    if (k5ccname)
		ret = krb5_cc_resolve(context, k5ccname, &id);
	    else
		ret = krb5_cc_default(context, &id);
	    if (ret)
		krb5_free_context(context);
	}
	if (ret == 0) {
	    krb5_afslog(context, id, cell, 0);
	    krb5_cc_close (context, id);
	    krb5_free_context (context);
	}
#endif
#ifdef KRB4
	krb_afslog(cell, 0);
#endif
	if (!quiet)
	    reply(200, "afslog done");
    } else {
	if (!quiet)
	    reply(200, "no AFS present");
    }
}

void
afsunlog(void)
{
    if(k_hasafs())
	k_unlog();
}

#else
int ftpd_afslog_placeholder;
#endif /* KRB4 || KRB5 */
