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

/*
 * Little program that reads an srvtab or password and
 * creates a suitable ticketfile and associated AFS tokens.
 *
 * If an optional command is given the command is executed in a
 * new PAG and when the command exits the tickets are destroyed.
 */

#include "kauth.h"

RCSID("$Id: kauth.c,v 1.97 1999/12/02 16:58:31 joda Exp $");

krb_principal princ;
static char srvtab[MaxPathLen];
static int lifetime = DEFAULT_TKT_LIFE;
static char remote_tktfile[MaxPathLen];
static char remoteuser[100];
static char *cell = 0;

static void
usage(void)
{
    fprintf(stderr,
	    "Usage:\n"
	    "  %s [name]\n"
	    "or\n"
	    "  %s [-ad] [-n name] [-r remoteuser] [-t remote ticketfile]\n"
	    "        [-l lifetime (in minutes) ] [-f srvtab ] [-c AFS cell name ]\n"
	    "        [-h hosts... [--]] [command ... ]\n\n",
	    __progname, __progname);
    fprintf(stderr, 
	    "A fully qualified name can be given: user[.instance][@realm]\n"
	    "Realm is converted to uppercase!\n");
    exit(1);
}

#define EX_NOEXEC	126
#define EX_NOTFOUND	127

static int
doexec(int argc, char **argv)
{
    int ret = simple_execvp(argv[0], argv);
    if(ret == -2)
	warn ("fork");
    if(ret == -3)
	warn("waitpid");
    if(ret < 0)
	return EX_NOEXEC;
    if(ret == EX_NOEXEC || ret == EX_NOTFOUND)
	warnx("Can't exec program ``%s''", argv[0]);
	
    return ret;
}

static RETSIGTYPE
renew(int sig)
{
    int code;

    signal(SIGALRM, renew);

    code = krb_get_svc_in_tkt(princ.name, princ.instance, princ.realm,
			      KRB_TICKET_GRANTING_TICKET,
			      princ.realm, lifetime, srvtab);
    if (code)
	warnx ("%s", krb_get_err_text(code));
    else if (k_hasafs())
	{
	    if ((code = krb_afslog(cell, NULL)) != 0 && code != KDC_PR_UNKNOWN) {
		warnx ("%s", krb_get_err_text(code));
	    }
	}

    alarm(krb_life_to_time(0, lifetime)/2 - 60);
    SIGRETURN(0);
}

static int
zrefresh(void)
{
    switch (fork()) {
    case -1:
	err (1, "Warning: Failed to fork zrefresh");
	return -1;
    case 0:
	/* Child */
	execlp("zrefresh", "zrefresh", 0);
	execl(BINDIR "/zrefresh", "zrefresh", 0);
	exit(1);
    default:
	/* Parent */
	break;
    }
    return 0;
}

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

static int
get_ticket_address(krb_principal *princ, des_cblock *key)
{
    int code;
    unsigned char flags;
    krb_principal service;
    u_int32_t addr;
    struct in_addr addr2;
    des_cblock session;
    int life;
    u_int32_t time_sec;
    des_key_schedule schedule;
    CREDENTIALS c;
	
    code = get_ad_tkt(princ->name, princ->instance, princ->realm, 0);
    if(code) {
	warnx("get_ad_tkt: %s\n", krb_get_err_text(code));
	return code;
    }
    code = krb_get_cred(princ->name, princ->instance, princ->realm, &c);
    if(code) {
	warnx("krb_get_cred: %s\n", krb_get_err_text(code));
	return code;
    }

    des_set_key(key, schedule);
    code = decomp_ticket(&c.ticket_st, 
			 &flags,
			 princ->name,
			 princ->instance,
			 princ->realm,
			 &addr,
			 session,
			 &life,
			 &time_sec,
			 service.name,
			 service.instance,
			 key,
			 schedule);
    if(code) {
	warnx("decomp_ticket: %s\n", krb_get_err_text(code));
	return code;
    }
    memset(&session, 0, sizeof(session));
    memset(schedule, 0, sizeof(schedule));
    addr2.s_addr = addr;
    fprintf(stdout, "ticket address = %s\n", inet_ntoa(addr2));
} 


int
main(int argc, char **argv)
{
    int code, more_args;
    int ret;
    int c;
    char *file;
    int pflag = 0;
    int aflag = 0;
    int version_flag = 0;
    char passwd[100];
    des_cblock key;
    char **host;
    int nhost;
    char tf[MaxPathLen];

    set_progname (argv[0]);

    if ((file =  getenv("KRBTKFILE")) == 0)
	file = TKT_FILE;  

    memset(&princ, 0, sizeof(princ));
    memset(srvtab, 0, sizeof(srvtab));
    *remoteuser = '\0';
    nhost = 0;
    host = NULL;
  
    /* Look for kerberos name */
    if (argc > 1 &&
	argv[1][0] != '-' &&
	krb_parse_name(argv[1], &princ) == 0)
      {
	argc--; argv++;
	strupr(princ.realm);
      }

    while ((c = getopt(argc, argv, "ar:t:f:hdl:n:c:v")) != -1)
	switch (c) {
	case 'a':
	    aflag++;
	    break;
	case 'd':
	    krb_enable_debug();
	    _kafs_debug = 1;
	    aflag++;
	    break;
	case 'f':
	    strlcpy(srvtab, optarg, sizeof(srvtab));
	    break;
	case 't':
	    strlcpy(remote_tktfile, optarg, sizeof(remote_tktfile));
	    break;
	case 'r':
	    strlcpy(remoteuser, optarg, sizeof(remoteuser));
	    break;
	case 'l':
	    lifetime = atoi(optarg);
	    if (lifetime == -1)
		lifetime = 255;
	    else if (lifetime < 5)
		lifetime = 1;
	    else
		lifetime = krb_time_to_life(0, lifetime*60);
	    if (lifetime > 255)
		lifetime = 255;
	    break;
	case 'n':
	    if ((code = krb_parse_name(optarg, &princ)) != 0) {
		warnx ("%s", krb_get_err_text(code));
		usage();
	    }
	    strupr(princ.realm);
	    pflag = 1;
	    break;
	case 'c':
	    cell = optarg;
	    break;
	case 'h':
	    host = argv + optind;
	    for(nhost = 0; optind < argc && *argv[optind] != '-'; ++optind)
		++nhost;
	    if(nhost == 0)
		usage();
	    break;
	case 'v':
	    version_flag++;
	    print_version(NULL);
	    break;
	case '?':
	default:
	    usage();
	    break;
	}
  
    if(version_flag) {
	print_version(NULL);
	exit(0);
    }
    if (princ.name[0] == '\0' && krb_get_default_principal (princ.name, 
							    princ.instance, 
							    princ.realm) < 0)
	errx (1, "Could not get default principal");
  
    /* With root tickets assume remote user is root */
    if (*remoteuser == '\0') {
      if (strcmp(princ.instance, "root") == 0)
	strlcpy(remoteuser, princ.instance, sizeof(remoteuser));
      else
	strlcpy(remoteuser, princ.name, sizeof(remoteuser));
    }

    more_args = argc - optind;
  
    if (princ.realm[0] == '\0')
	if (krb_get_lrealm(princ.realm, 1) != KSUCCESS)
	    strlcpy(princ.realm, KRB_REALM, REALM_SZ);
  
    if (more_args) {
	int f;
      
	do{
	    snprintf(tf, sizeof(tf), "%s%u_%u", TKT_ROOT, (unsigned)getuid(),
		     (unsigned)(getpid()*time(0)));
	    f = open(tf, O_CREAT|O_EXCL|O_RDWR);
	}while(f < 0);
	close(f);
	unlink(tf);
	setenv("KRBTKFILE", tf, 1);
	krb_set_tkt_string (tf);
    }
    
    if (srvtab[0])
	{
	    signal(SIGALRM, renew);

	    code = read_service_key (princ.name, princ.instance, princ.realm, 0, 
				     srvtab, (char *)&key);
	    if (code == KSUCCESS)
		code = krb_get_in_tkt(princ.name, princ.instance, princ.realm,
				      KRB_TICKET_GRANTING_TICKET,
				      princ.realm, lifetime,
				      key_to_key, NULL, key);
	    alarm(krb_life_to_time(0, lifetime)/2 - 60);
	}
    else {
	char prompt[128];
	  
	snprintf(prompt, sizeof(prompt), "%s's Password: ", krb_unparse_name(&princ));
	if (des_read_pw_string(passwd, sizeof(passwd)-1, prompt, 0)){
	    memset(passwd, 0, sizeof(passwd));
	    exit(1);
	}
	code = krb_get_pw_in_tkt2(princ.name, princ.instance, princ.realm, 
				  KRB_TICKET_GRANTING_TICKET, princ.realm, 
				  lifetime, passwd, &key);
	
	memset(passwd, 0, sizeof(passwd));
    }
    if (code) {
	memset (key, 0, sizeof(key));
	errx (1, "%s", krb_get_err_text(code));
    }

    if(aflag)
	get_ticket_address(&princ, &key);

    if (k_hasafs()) {
	if (more_args)
	    k_setpag();
	if ((code = krb_afslog(cell, NULL)) != 0 && code != KDC_PR_UNKNOWN) {
	    if(code > 0)
		warnx ("%s", krb_get_err_text(code));
	    else
		warnx ("failed to store AFS token");
	}
    }

    for(ret = 0; nhost-- > 0; host++)
	ret += rkinit(&princ, lifetime, remoteuser, remote_tktfile, &key, *host);
  
    if (ret)
	return ret;

    if (more_args) {
	ret = doexec(more_args, &argv[optind]);
	dest_tkt();
	if (k_hasafs())
	    k_unlog();	 
    }
    else
	zrefresh();
  
    return ret;
}
