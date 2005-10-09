/*
 * Copyright (c) 1995, 1996, 1997, 1998 Kungliga Tekniska Högskolan
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

RCSID("$Id: kauth.c,v 1.25 1999/12/02 16:58:31 joda Exp $");

static KTEXT_ST cip;
static unsigned int lifetime;
static time_t local_time;

static krb_principal pr;

static int do_destroy_tickets = 1;

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
klist(void)
{
    int err;

    char *file = tkt_string();

    krb_principal pr;
    
    char buf1[128], buf2[128];
    int header = 1;
    CREDENTIALS c;

    

    err = tf_init(file, R_TKT_FIL);
    if(err != KSUCCESS){
	reply(500, "%s", krb_get_err_text(err));
	return;
    }
    tf_close();

    /* 
     * We must find the realm of the ticket file here before calling
     * tf_init because since the realm of the ticket file is not
     * really stored in the principal section of the file, the
     * routine we use must itself call tf_init and tf_close.
     */
    err = krb_get_tf_realm(file, pr.realm);
    if(err != KSUCCESS){
	reply(500, "%s", krb_get_err_text(err));
	return;
    }

    err = tf_init(file, R_TKT_FIL);
    if(err != KSUCCESS){
	reply(500, "%s", krb_get_err_text(err));
	return;
    }

    err = tf_get_pname(pr.name);
    if(err != KSUCCESS){
	reply(500, "%s", krb_get_err_text(err));
	return;
    }
    err = tf_get_pinst(pr.instance);
    if(err != KSUCCESS){
	reply(500, "%s", krb_get_err_text(err));
	return;
    }

    /* 
     * You may think that this is the obvious place to get the
     * realm of the ticket file, but it can't be done here as the
     * routine to do this must open the ticket file.  This is why 
     * it was done before tf_init.
     */
       
    lreply(200, "Ticket file: %s", tkt_string());

    lreply(200, "Principal: %s", krb_unparse_name(&pr));
    while ((err = tf_get_cred(&c)) == KSUCCESS) {
	if (header) {
	    lreply(200, "%-15s  %-15s  %s",
		   "  Issued", "  Expires", "  Principal (kvno)");
	    header = 0;
	}
	strlcpy(buf1, short_date(c.issue_date), sizeof(buf1));
	c.issue_date = krb_life_to_time(c.issue_date, c.lifetime);
	if (time(0) < (unsigned long) c.issue_date)
	    strlcpy(buf2, short_date(c.issue_date), sizeof(buf2));
	else
	    strlcpy(buf2, ">>> Expired <<< ", sizeof(buf2));
	lreply(200, "%s  %s  %s (%d)", buf1, buf2,
	       krb_unparse_name_long(c.service, c.instance, c.realm), c.kvno); 
    }
    if (header && err == EOF) {
	lreply(200, "No tickets in file.");
    }
    reply(200, " ");
}

/*
 * Only destroy if we created the tickets
 */

void
cond_kdestroy(void)
{
    if (do_destroy_tickets)
	dest_tkt();
    afsunlog();
}

void
kdestroy(void)
{
    dest_tkt();
    afsunlog();
    reply(200, "Tickets destroyed");
}

void
krbtkfile(const char *tkfile)
{
    do_destroy_tickets = 0;
    krb_set_tkt_string(tkfile);
    reply(200, "Using ticket file %s", tkfile);
}

void
afslog(const char *cell)
{
    if(k_hasafs()) {
	krb_afslog(cell, 0);
	reply(200, "afslog done");
    } else {
	reply(200, "no AFS present");
    }
}

void
afsunlog(void)
{
    if(k_hasafs())
	k_unlog();
}
