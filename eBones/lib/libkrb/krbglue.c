/*
 * Copyright 1988 by the Massachusetts Institute of Technology.
 * For copying and distribution information, please see the file
 * <Copyright.MIT>.
 *
 *	from: krbglue.c,v 4.1 89/01/23 15:51:50 wesommer Exp $
 *	$FreeBSD$
 */

#if 0
#ifndef lint
static char rcsid[] =
$FreeBSD$";
#endif lint
#endif

#ifndef NCOMPAT
/*
 * glue together new libraries and old clients
 */

#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <des.h>
#include "krb.h"

/* These definitions should be in krb.h, no? */
/*
#if defined(__HIGHC__)
#undef __STDC__
#endif
#ifdef __STDC__
extern int krb_mk_req (KTEXT, char *, char *, char *, long);
extern int krb_rd_req (KTEXT, char *, char *, long, AUTH_DAT *, char *);
extern int krb_kntoln (AUTH_DAT *, char *);
extern int krb_set_key (char *, int);
extern int krb_get_cred (char *, char *, char *, CREDENTIALS *);
extern long krb_mk_priv (u_char *, u_char *, u_long, Key_schedule,
			 C_Block, struct sockaddr_in *,
			 struct sockaddr_in *);
extern long krb_rd_priv (u_char *, u_long, Key_schedule,
			 C_Block, struct sockaddr_in *,
			 struct sockaddr_in *, MSG_DAT *);
extern long krb_mk_safe (u_char *, u_char *, u_long, C_Block *,
			 struct sockaddr_in *, struct sockaddr_in *);
extern long krb_rd_safe (u_char *, u_long, C_Block *,
			 struct sockaddr_in *, struct sockaddr_in *,
			 MSG_DAT *);
extern long krb_mk_err (u_char *, long, char *);
extern int krb_rd_err (u_char *, u_long, long *, MSG_DAT *);
extern int krb_get_pw_in_tkt (char *, char *, char *, char *, char *, int,
			      char *);
extern int krb_get_svc_in_tkt (char *, char *, char *, char *, char *, int,
			       char *);
extern int krb_get_pw_tkt (char *, char *, char *, char *);
extern int krb_get_lrealm (char *, char *);
extern int krb_realmofhost (char *);
extern char *krb_get_phost (char *);
extern int krb_get_krbhst (char *, char *, int);
#ifdef DEBUG
extern KTEXT krb_create_death_packet (char *);
#endif
#else
extern int krb_mk_req ();
extern int krb_rd_req ();
extern int krb_kntoln ();
extern int krb_set_key ();
extern int krb_get_cred ();
extern long krb_mk_priv ();
extern long krb_rd_priv ();
extern long krb_mk_safe ();
extern long krb_rd_safe ();
extern long krb_mk_err ();
extern int krb_rd_err ();
extern int krb_get_pw_in_tkt ();
extern int krb_get_svc_in_tkt ();
extern int krb_get_pw_tkt ();
extern int krb_get_lrealm ();
extern int krb_realmofhost ();
extern char *krb_get_phost ();
extern int krb_get_krbhst ();
#ifdef DEBUG
extern KTEXT krb_create_death_packet ();
#endif
#endif
*/


int mk_ap_req(authent, service, instance, realm, checksum)
    KTEXT authent;
    char *service, *instance, *realm;
    u_long checksum;
{
    return krb_mk_req(authent,service,instance,realm,checksum);
}

int rd_ap_req(authent, service, instance, from_addr, ad, fn)
    KTEXT authent;
    char *service, *instance;
    u_long from_addr;
    AUTH_DAT *ad;
    char *fn;
{
    return krb_rd_req(authent,service,instance,from_addr,ad,fn);
}

int an_to_ln(ad, lname)
    AUTH_DAT *ad;
    char *lname;
{
    return krb_kntoln (ad,lname);
}

int set_serv_key (key, cvt)
    char *key;
    int cvt;
{
    return krb_set_key(key,cvt);
}

int get_credentials (svc,inst,rlm,cred)
    char *svc, *inst, *rlm;
    CREDENTIALS *cred;
{
    return krb_get_cred (svc, inst, rlm, cred);
}

long mk_private_msg (in,out,in_length,schedule,key,sender,receiver)
    u_char *in, *out;
    u_long in_length;
    Key_schedule schedule;
    C_Block key;
    struct sockaddr_in *sender, *receiver;
{
    return krb_mk_priv (in,out,in_length,schedule,key,sender,receiver);
}

long rd_private_msg (in,in_length,schedule,key,sender,receiver,msg_data)
    u_char *in;
    u_long in_length;
    Key_schedule schedule;
    C_Block key;
    struct sockaddr_in *sender, *receiver;
    MSG_DAT *msg_data;
{
    return krb_rd_priv (in,in_length,schedule,key,sender,receiver,msg_data);
}

long mk_safe_msg (in,out,in_length,key,sender,receiver)
    u_char *in, *out;
    u_long in_length;
    C_Block *key;
    struct sockaddr_in *sender, *receiver;
{
    return krb_mk_safe (in,out,in_length,key,sender,receiver);
}

long rd_safe_msg (in,length,key,sender,receiver,msg_data)
    u_char *in;
    u_long length;
    C_Block *key;
    struct sockaddr_in *sender, *receiver;
    MSG_DAT *msg_data;
{
    return krb_rd_safe (in,length,key,sender,receiver,msg_data);
}

long mk_appl_err_msg (out,code,string)
    u_char *out;
    long code;
    char *string;
{
    return krb_mk_err (out,code,string);
}

long rd_appl_err_msg (in,length,code,msg_data)
    u_char *in;
    u_long length;
    long *code;
    MSG_DAT *msg_data;
{
    return krb_rd_err (in,length,code,msg_data);
}

int get_in_tkt(user,instance,realm,service,sinstance,life,password)
    char *user, *instance, *realm, *service, *sinstance;
    int life;
    char *password;
{
    return krb_get_pw_in_tkt(user,instance,realm,service,sinstance,
			     life,password);
}

int get_svc_in_tkt(user, instance, realm, service, sinstance, life, srvtab)
    char *user, *instance, *realm, *service, *sinstance;
    int life;
    char *srvtab;
{
    return krb_get_svc_in_tkt(user, instance, realm, service, sinstance,
			      life, srvtab);
}

int get_pw_tkt(user,instance,realm,cpw)
    char *user;
    char *instance;
    char *realm;
    char *cpw;
{
    return krb_get_pw_tkt(user,instance,realm,cpw);
}

int
get_krbrlm (r, n)
char *r;
int n;
{
    return krb_get_lream(r,n);
}

int
krb_getrealm (host)
{
    return krb_realmofhost(host);
}

char *
get_phost (host)
char *host
{
    return krb_get_phost(host);
}

int
get_krbhst (h, r, n)
char *h;
char *r;
int n;
{
    return krb_get_krbhst(h,r,n);
}
#ifdef DEBUG
struct ktext *create_death_packet(a_name)
    char *a_name;
{
    return krb_create_death_packet(a_name);
}
#endif /* DEBUG */

#if 0
extern int krb_ck_repl ();

int check_replay ()
{
    return krb_ck_repl ();
}
#endif
#endif /* NCOMPAT */
