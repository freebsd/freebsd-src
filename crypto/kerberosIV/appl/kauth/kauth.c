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

/*
 * Little program that reads an srvtab or password and
 * creates a suitable ticketfile and associated AFS tokens.
 *
 * If an optional command is given the command is executed in a
 * new PAG and when the command exits the tickets are destroyed.
 */

#include "kauth.h"

RCSID("$Id: kauth.c,v 1.75 1997/05/02 15:09:24 assar Exp $");

krb_principal princ;
static char srvtab[MaxPathLen + 1];
static int lifetime = DEFAULT_TKT_LIFE;
static char remote_tktfile[MaxPathLen + 1];
static char remoteuser[100];
static char *cell = 0;

static void
usage(void)
{
    fprintf(stderr,
	    "Usage: %s [-n <name>] [-r remoteuser] [-t remote ticketfile]"
	    "[-l lifetime (in minutes) ] [-h hosts... ]"
	    "[-f srvtab ] [-c AFS cell name ] [command ... ]\n",
	    __progname);
    fprintf(stderr, "\nA fully qualified name can be given user[.instance][@realm]\nRealm is converted to uppercase!\n");
    exit(1);
}

static void
doexec(int argc, char **argv)
{
    int status;
    pid_t ret;

    switch (fork()) {
    case -1:
	err (1, "fork");
	break;
    case 0:
	/* in child */
	execvp(argv[0], argv);
	err (1, "Can't exec program ``%s''", argv[0]);
	break;
    default:
	/* in parent */
	do {
	    ret = wait(&status);
	} while ((ret > 0 && !WIFEXITED(status)) || (ret < 0 && errno == EINTR));
	if (ret < 0)
	    perror("wait");
	dest_tkt();
	if (k_hasafs())
	    k_unlog();	 
	break;
    }
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
	    if ((code = k_afsklog(cell, NULL)) != 0 && code != KDC_PR_UNKNOWN) {
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
key_to_key(char *user, char *instance, char *realm, void *arg,
	   des_cblock *key)
{
    memcpy(key, arg, sizeof(des_cblock));
    return 0;
}

int
main(int argc, char **argv)
{
    int code, more_args;
    int ret;
    int c;
    char *file;
    int pflag = 0;
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
  
    while ((c = getopt(argc, argv, "r:t:f:hl:n:c:")) != EOF)
	switch (c) {
	case 'f':
	    strncpy(srvtab, optarg, sizeof(srvtab));
	    break;
	case 't':
	    strncpy(remote_tktfile, optarg, sizeof(remote_tktfile));
	    break;
	case 'r':
	    strncpy(remoteuser, optarg, sizeof(remoteuser));
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
	    break;
	case '?':
	default:
	    usage();
	    break;
	}
  
    /* Look for kerberos name */
    if (!pflag && optind < argc && krb_parse_name(argv[optind], &princ) == 0) {
	++optind;	 
	strupr(princ.realm);
    }

    if (princ.name[0] == '\0' && krb_get_default_principal (princ.name, 
							    princ.instance, 
							    princ.realm) < 0)
	errx (1, "Could not get default principal");
  
    if (*remoteuser == '\0')
	strcpy (remoteuser, princ.name);

    more_args = argc - optind;
  
    if (princ.realm[0] == '\0')
	if (krb_get_lrealm(princ.realm, 1) != KSUCCESS)
	    strcpy(princ.realm, KRB_REALM);
  
    if (more_args) {
	int f;
      
	do{
	    snprintf(tf, sizeof(tf),
		     TKT_ROOT "%u_%u",
		     (unsigned)getuid(),
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
	des_string_to_key (passwd, &key);
	code = krb_get_in_tkt (princ.name, princ.instance, princ.realm,
			       KRB_TICKET_GRANTING_TICKET,
			       princ.realm, lifetime,
			       key_to_key, NULL, key);
	if(code == INTK_BADPW) {
	    afs_string_to_key (passwd, princ.realm, &key);
	    code = krb_get_in_tkt (princ.name, princ.instance, princ.realm,
				   KRB_TICKET_GRANTING_TICKET,
				   princ.realm, lifetime,
				   key_to_key, NULL, key);
	}
	memset(passwd, 0, sizeof(passwd));
    }
    if (code) {
	memset (key, 0, sizeof(key));
	errx (1, "%s", krb_get_err_text(code));
    }

    if (k_hasafs()) {
	if (more_args)
	    k_setpag();
	if ((code = k_afsklog(cell, NULL)) != 0 && code != KDC_PR_UNKNOWN)
	    warnx ("%s", krb_get_err_text(code));
    }

    for(ret = 0; nhost-- > 0; host++)
	ret += rkinit(&princ, lifetime, remoteuser, remote_tktfile, &key, *host);
  
    if (ret)
	return ret;

    if (more_args)
	doexec(more_args, &argv[optind]);
    else
	zrefresh();
  
    return 0;
}
