/*
 * Copyright (c) 1995, 1996, 1997, 1998, 1999 Kungliga Tekniska Högskolan
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
/* $FreeBSD$ */

#include "kadm_locl.h"
#include "ksrvutil.h"

RCSID("$Id: ksrvutil_get.c,v 1.43 1999/12/02 16:58:36 joda Exp $");

#define BAD_PW 1
#define GOOD_PW 0
#define FUDGE_VALUE 15		/* for ticket expiration time */
#define PE_NO 0
#define PE_YES 1
#define PE_UNSURE 2

static char tktstring[MaxPathLen];

static int
princ_exists(char *name, char *instance, char *realm)
{
    int status;

    status = krb_get_pw_in_tkt(name, instance, realm,
			       KRB_TICKET_GRANTING_TICKET,
			       realm, 1, "");

    if ((status == KSUCCESS) || (status == INTK_BADPW))
	return(PE_YES);
    else if (status == KDC_PR_UNKNOWN)
	return(PE_NO);
    else
	return(PE_UNSURE);
}

static int
get_admin_password(char *myname, char *myinst, char *myrealm)
{
  int status;
  char admin_passwd[MAX_KPW_LEN];	/* Admin's password */
  int ticket_life = 1;	/* minimum ticket lifetime */
  char buf[1024];
  CREDENTIALS c;

  if (princ_exists(myname, myinst, myrealm) != PE_NO) {
    snprintf(buf, sizeof(buf), "Password for %s: ",
	    krb_unparse_name_long (myname, myinst, myrealm));
    if (des_read_pw_string(admin_passwd, sizeof(admin_passwd)-1,
			    buf, 0)) {
      fprintf(stderr, "Error reading admin password.\n");
      goto bad;
    }
    status = krb_get_pw_in_tkt(myname, myinst, myrealm, PWSERV_NAME, 
			       KADM_SINST, ticket_life, admin_passwd);
    memset(admin_passwd, 0, sizeof(admin_passwd));
  } else
    status = KDC_PR_UNKNOWN;
  
  switch(status) {
  case GT_PW_OK:
    return(GOOD_PW);
  case KDC_PR_UNKNOWN:
    printf("Principal %s does not exist.\n",
	   krb_unparse_name_long(myname, myinst, myrealm));
    goto bad;
  case GT_PW_BADPW:
    printf("Incorrect admin password.\n");
    goto bad;
  default:
    com_err("kadmin", status+krb_err_base,
	    "while getting password tickets");
    goto bad;
  }
  
bad:
  memset(admin_passwd, 0, sizeof(admin_passwd));
  dest_tkt();
  return(BAD_PW);
}

static void
srvtab_put_key (int fd, char *filename, char *name, char *inst, char *realm,
		int8_t kvno, des_cblock key)
{
  char sname[ANAME_SZ];		/* name of service */
  char sinst[INST_SZ];		/* instance of service */
  char srealm[REALM_SZ];	/* realm of service */
  int8_t skvno;
  des_cblock skey;

  lseek(fd, 0, SEEK_SET);

  while(getst(fd, sname,  SNAME_SZ) > 0 &&
	getst(fd, sinst,  INST_SZ) > 0  &&
	getst(fd, srealm, REALM_SZ) > 0 &&
	read(fd, &skvno,  sizeof(skvno)) > 0 &&
	read(fd, skey,    sizeof(skey)) > 0) {
    if(strcmp(name,  sname)  == 0 &&
       strcmp(inst,  sinst)  == 0 &&
       strcmp(realm, srealm) == 0) {
      lseek(fd, lseek(fd,0,SEEK_CUR)-(sizeof(skvno) + sizeof(skey)), SEEK_SET);
      safe_write(filename, fd, &kvno, sizeof(kvno));
      safe_write(filename, fd, key,   sizeof(des_cblock));
      return;
    }
  }
  safe_write(filename, fd, name,  strlen(name) + 1);
  safe_write(filename, fd, inst,  strlen(inst) + 1);
  safe_write(filename, fd, realm, strlen(realm) + 1);
  safe_write(filename, fd, &kvno, sizeof(kvno));
  safe_write(filename, fd, key,   sizeof(des_cblock));
}

/* 
 * node list of services 
 */

struct srv_ent{
  char name[SNAME_SZ];
  char inst[INST_SZ];
  char realm[REALM_SZ];
  struct srv_ent *next;
};

static int
key_to_key(const char *user,
	   char *instance,
	   const char *realm,
	   const void *arg,
	   des_cblock *key)
{
  memcpy(key, arg, sizeof(des_cblock));
  return 0;
}

static void
get_srvtab_ent(int unique_filename, int fd, char *filename, 
	       char *name, char *inst, char *realm)
{
    char chname[128];
    des_cblock newkey;
    char old_tktfile[MaxPathLen], new_tktfile[MaxPathLen];
    char garbage_name[ANAME_SZ];
    char garbage_inst[ANAME_SZ];
    CREDENTIALS c;
    u_int8_t kvno;
    Kadm_vals values;
    int ret;

    strlcpy(chname, krb_get_phost(inst), sizeof(chname));
    if(strcmp(inst, chname))
	fprintf(stderr, 
		"Warning: Are you sure `%s' should not be `%s'?\n",
		inst, chname);
    
    memset(&values, 0, sizeof(values));
    strlcpy(values.name, name, sizeof(values.name));
    strlcpy(values.instance, inst, sizeof(values.instance));
    des_random_key(newkey);
    values.key_low = (newkey[0] << 24) | (newkey[1] << 16)
	| (newkey[2] << 8) | (newkey[3] << 0);
    values.key_high = (newkey[4] << 24) | (newkey[5] << 16)
	| (newkey[6] << 8) | (newkey[7] << 0);

    SET_FIELD(KADM_NAME,values.fields);
    SET_FIELD(KADM_INST,values.fields);
    SET_FIELD(KADM_DESKEY,values.fields);

    ret = kadm_mod(&values, &values);
    if(ret == KADM_NOENTRY)
	ret = kadm_add(&values);
    if (ret != KSUCCESS) {
	warnx ("Couldn't get srvtab entry for %s.%s: %s",
	       name, inst, error_message(ret));
	return;
    }
  
    values.key_low = values.key_high = 0;

    /* get the key version number */
    { 
	int old = krb_use_admin_server(1);

	strlcpy(old_tktfile, tkt_string(), sizeof(old_tktfile));
	snprintf(new_tktfile, sizeof(new_tktfile), "%s_ksrvutil-get.%u",
		 TKT_ROOT, (unsigned)getpid());
	krb_set_tkt_string(new_tktfile);
      
	ret = krb_get_in_tkt(name, inst, realm, name, inst,
			     1, key_to_key, NULL, &newkey);
	krb_use_admin_server(old);
 	if (ret) {
	    warnx ("getting tickets for %s: %s", 
		   krb_unparse_name_long(name, inst, realm),
		   krb_get_err_text(ret));
	    return;
  	}
    }
      
    if (ret == KSUCCESS &&
	(ret = tf_init(tkt_string(), R_TKT_FIL)) == KSUCCESS &&
	(ret = tf_get_pname(garbage_name)) == KSUCCESS &&
	(ret = tf_get_pinst(garbage_inst)) == KSUCCESS &&
	(ret = tf_get_cred(&c)) == KSUCCESS)
	kvno = c.kvno;
    else {
	warnx ("Could not find the cred in the ticket file: %s",
	       krb_get_err_text(ret));
	return;
    }

    tf_close();
    krb_set_tkt_string(old_tktfile);
    unlink(new_tktfile);
    
    if(ret != KSUCCESS) {
	memset(&newkey, 0, sizeof(newkey));
	warnx ("Could not get a ticket for %s: %s\n",
	       krb_unparse_name_long(name, inst, realm),
	       krb_get_err_text(ret));
	return;
    }

    /* Write the new key & c:o to the srvtab file */

    if(unique_filename){
	char *fn;
	asprintf(&fn, "%s-%s", filename, 
		 krb_unparse_name_long(name, inst, realm));
	if(fn == NULL){
	    warnx("Out of memory");
	    leave(NULL, 1);
	}
	fd = open(fn, O_RDWR | O_CREAT | O_TRUNC, 0600); /* XXX flags, mode? */
	if(fd < 0){
	    warn("%s", fn);
	    leave(NULL, 1);
	}
	srvtab_put_key (fd, fn, name, inst, realm, kvno, newkey);
	close(fd);
	fprintf (stderr, "Created %s\n", fn);
	free(fn);
    }else{
	srvtab_put_key (fd, filename, name, inst, realm, kvno, newkey);
	fprintf (stderr, "Added %s\n", 
		 krb_unparse_name_long (name, inst, realm));
    }
    memset(&newkey, 0, sizeof(newkey));
}

static void
ksrvutil_kadm(int unique_filename, int fd, char *filename, struct srv_ent *p)
{
  int ret;
  CREDENTIALS c;
  
  ret = kadm_init_link(PWSERV_NAME, KADM_SINST, u_realm);
  if (ret != KADM_SUCCESS) {
    warnx("Couldn't initialize kadmin link: %s", error_message(ret));
    leave(NULL, 1);
  }
  
  ret = krb_get_cred (PWSERV_NAME, KADM_SINST, u_realm, &c);
  if (ret != KSUCCESS) {
    umask(077);
       
    /*
     *  create ticket file and get admin tickets
     */
    snprintf(tktstring, sizeof(tktstring), "%s_ksrvutil_%d",
	     TKT_ROOT, (int)getpid());
    krb_set_tkt_string(tktstring);
    destroyp = TRUE;
       
    ret = get_admin_password(u_name, u_inst, u_realm);
    if (ret) {
      warnx("Couldn't get admin password.");
      leave(NULL, 1);
    }
  }  
  for(;p;){
    get_srvtab_ent(unique_filename, fd, filename, p->name, p->inst, p->realm);
    p=p->next;
  }
  unlink(tktstring);
}

static void
parseinput (char *result, size_t sz, char *val, char *def)
{
  char *lim;
  int inq;

  if (val[0] == '\0') {
    strlcpy (result, def, sz);
    return;
  }
  lim = result + sz - 1;
  inq = 0;
  while(*val && result < lim) {
    switch(*val) {
    case '\'' :
      inq = !inq;
      ++val;
      break;
    case '\\' :
      if(!inq)
	val++;
    default:
      *result++ = *val++;
      break;
    }
  }
  *result = '\0';
}

void
ksrvutil_get(int unique_filename, int fd, char *filename, int argc, char **argv)
{
  char sname[ANAME_SZ];		/* name of service */
  char sinst[INST_SZ];		/* instance of service */
  char srealm[REALM_SZ];	/* realm of service */
  char databuf[BUFSIZ];
  char local_hostname[100];
  char prompt[100];
  struct srv_ent *head=NULL;
  int i;

  gethostname(local_hostname, sizeof(local_hostname));
  strlcpy(local_hostname,
		  krb_get_phost(local_hostname),
		  sizeof(local_hostname));

  if (argc)
    for(i=0; i < argc; ++i) {
      struct srv_ent *p=malloc(sizeof(*p));

      if(p == NULL) {
	warnx ("out of memory in malloc");
	leave(NULL,1);
      }
      p->next = head;
      strlcpy (p->realm, u_realm, sizeof(p->realm));
      if (kname_parse (p->name, p->inst, p->realm, argv[i]) !=
	  KSUCCESS) {
	warnx ("parse error on '%s'\n", argv[i]);
	free(p);
	continue;
      }
      if (p->name[0] == '\0')
	strlcpy(p->name, "rcmd", sizeof(p->name));
      if (p->inst[0] == '\0')
	strlcpy(p->inst, local_hostname, sizeof(p->inst));
      if (p->realm[0] == '\0')
	strlcpy(p->realm, u_realm, sizeof(p->realm));
      head = p;
    }

  else
    do{
      safe_read_stdin("Name [rcmd]: ", databuf, sizeof(databuf));
      parseinput (sname, sizeof(sname), databuf, "rcmd");
    
      snprintf(prompt, sizeof(prompt), "Instance [%s]: ", local_hostname);
      safe_read_stdin(prompt, databuf, sizeof(databuf));
      parseinput (sinst, sizeof(sinst), databuf, local_hostname);
    
      snprintf(prompt, sizeof(prompt), "Realm [%s]: ", u_realm);
      safe_read_stdin(prompt, databuf, sizeof(databuf));
      parseinput (srealm, sizeof(srealm), databuf, u_realm);

      if(yn("Is this correct?")){
	struct srv_ent *p=(struct srv_ent*)malloc(sizeof(struct srv_ent));
	if (p == NULL) {
	    warnx ("out of memory in malloc");
	    leave(NULL,1);
	}
	p->next=head;
	head=p;
	strlcpy(p->name, sname, sizeof(p->name));
	strlcpy(p->inst, sinst, sizeof(p->inst));
	strlcpy(p->realm, srealm, sizeof(p->realm));
      }
    }while(ny("Add more keys?"));
  
  
  ksrvutil_kadm(unique_filename, fd, filename, head);

  {
    struct srv_ent *p=head, *q;
    while(p){
      q=p;
      p=p->next;
      free(q);
    }
  }

}
