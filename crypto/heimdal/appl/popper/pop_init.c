/*
 * Copyright (c) 1989 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#include <popper.h>
RCSID("$Id: pop_init.c,v 1.58 2001/02/20 01:44:47 assar Exp $");


#if defined(KRB4) || defined(KRB5)

static int
pop_net_read(POP *p, int fd, void *buf, size_t len)
{
#ifdef KRB5
    return krb5_net_read(p->context, &fd, buf, len);
#elif defined(KRB4)
    return krb_net_read(fd, buf, len);
#endif
}
#endif

static char *addr_log;

static void
pop_write_addr(POP *p, struct sockaddr *addr)
{
    char ts[32];
    char as[128];
    time_t t;
    FILE *f;
    if(addr_log == NULL)
	return;
    t = time(NULL);
    strftime(ts, sizeof(ts), "%Y%m%d%H%M%S", localtime(&t));
    if(inet_ntop (addr->sa_family, socket_get_address(addr), 
		  as, sizeof(as)) == NULL) {
        pop_log(p, POP_PRIORITY, "failed to print address");
	return;
    }
    
    f = fopen(addr_log, "a");
    if(f == NULL) {
        pop_log(p, POP_PRIORITY, "failed to open address log (%s)", addr_log);
	return;
    }
    fprintf(f, "%s %s\n", as, ts);
    fclose(f);
}

#ifdef KRB4
static int
krb4_authenticate (POP *p, int s, u_char *buf, struct sockaddr *addr)
{
    Key_schedule schedule;
    KTEXT_ST ticket;
    char instance[INST_SZ];  
    char version[9];
    int auth;
  
    if (memcmp (buf, KRB_SENDAUTH_VERS, 4) != 0)
	return -1;
    if (pop_net_read (p, s, buf + 4,
		      KRB_SENDAUTH_VLEN - 4) != KRB_SENDAUTH_VLEN - 4)
	return -1;
    if (memcmp (buf, KRB_SENDAUTH_VERS, KRB_SENDAUTH_VLEN) != 0)
	return -1;

    k_getsockinst (0, instance, sizeof(instance));
    auth = krb_recvauth(KOPT_IGNORE_PROTOCOL,
			s,
			&ticket,
			"pop",
			instance,
                        (struct sockaddr_in *)addr,
			(struct sockaddr_in *) NULL,
                        &p->kdata,
			"",
			schedule,
			version);
    
    if (auth != KSUCCESS) {
        pop_msg(p, POP_FAILURE, "Kerberos authentication failure: %s", 
                krb_get_err_text(auth));
        pop_log(p, POP_PRIORITY, "%s: (%s.%s@%s) %s", p->client, 
                p->kdata.pname, p->kdata.pinst, p->kdata.prealm,
		krb_get_err_text(auth));
	return -1;
    }

#ifdef DEBUG
    pop_log(p, POP_DEBUG, "%s.%s@%s (%s): ok", p->kdata.pname, 
            p->kdata.pinst, p->kdata.prealm, p->ipaddr);
#endif /* DEBUG */
    return 0;
}
#endif /* KRB4 */

#ifdef KRB5
static int
krb5_authenticate (POP *p, int s, u_char *buf, struct sockaddr *addr)
{
    krb5_error_code ret;
    krb5_auth_context auth_context = NULL;
    u_int32_t len;
    krb5_ticket *ticket;
    char *server;

    if (memcmp (buf, "\x00\x00\x00\x13", 4) != 0)
	return -1;
    len = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | (buf[3]);
	
    if (krb5_net_read(p->context, &s, buf, len) != len)
	return -1;
    if (len != sizeof(KRB5_SENDAUTH_VERSION)
	|| memcmp (buf, KRB5_SENDAUTH_VERSION, len) != 0)
	return -1;

    ret = krb5_recvauth (p->context,
			 &auth_context,
			 &s,
			 "KPOPV1.0",
			 NULL, /* let rd_req figure out what server to use */
			 KRB5_RECVAUTH_IGNORE_VERSION,
			 NULL,
			 &ticket);
    if (ret) {
	pop_log(p, POP_PRIORITY, "krb5_recvauth: %s",
		krb5_get_err_text(p->context, ret));
	return -1;
    }


    ret = krb5_unparse_name(p->context, ticket->server, &server);
    if(ret) {
	pop_log(p, POP_PRIORITY, "krb5_unparse_name: %s", 
		krb5_get_err_text(p->context, ret));
	ret = -1;
	goto out;
    }
    /* does this make sense? */
    if(strncmp(server, "pop/", 4) != 0) {
	pop_log(p, POP_PRIORITY,
		"Got ticket for service `%s'", server);
	ret = -1;
	goto out;
    } else if(p->debug)
	pop_log(p, POP_DEBUG, 
		"Accepted ticket for service `%s'", server);
    free(server);
 out:
    krb5_auth_con_free (p->context, auth_context);
    krb5_copy_principal (p->context, ticket->client, &p->principal);
    krb5_free_ticket (p->context, ticket);

    return ret;
}
#endif

static int
krb_authenticate(POP *p, struct sockaddr *addr)
{
#if defined(KRB4) || defined(KRB5)
    u_char buf[BUFSIZ];

    if (pop_net_read (p, 0, buf, 4) != 4) {
	pop_msg(p, POP_FAILURE, "Reading four bytes: %s",
		strerror(errno));
	exit (1);
    }
#ifdef KRB4
    if (krb4_authenticate (p, 0, buf, addr) == 0){
	pop_write_addr(p, addr);
	p->version = 4;
	return POP_SUCCESS;
    }
#endif
#ifdef KRB5
    if (krb5_authenticate (p, 0, buf, addr) == 0){
	pop_write_addr(p, addr);
	p->version = 5;
	return POP_SUCCESS;
    }
#endif
    exit (1);
	
#endif /* defined(KRB4) || defined(KRB5) */

    return(POP_SUCCESS);
}

static int
plain_authenticate (POP *p, struct sockaddr *addr)
{
    return(POP_SUCCESS);
}

static int kerberos_flag;
static char *auth_str;
static int debug_flag;
static int interactive_flag;
static char *port_str;
static char *trace_file;
static int timeout;
static int help_flag;
static int version_flag;

static struct getargs args[] = {
#if defined(KRB4) || defined(KRB5)
    { "kerberos", 'k', arg_flag, &kerberos_flag, "use kerberos" },
#endif
    { "auth-mode", 'a', arg_string, &auth_str, "required authentication" },
    { "debug", 'd', arg_flag, &debug_flag },
    { "interactive", 'i', arg_flag, &interactive_flag, "create new socket" },
    { "port", 'p', arg_string, &port_str, "port to listen to", "port" },
    { "trace-file", 't', arg_string, &trace_file, "trace all command to file", "file" },
    { "timeout", 'T', arg_integer, &timeout, "timeout", "seconds" },
    { "address-log", 0, arg_string, &addr_log, "enable address log", "file" },
    { "help", 'h', arg_flag, &help_flag },
    { "version", 'v', arg_flag, &version_flag }
};

static int num_args = sizeof(args) / sizeof(args[0]);

/* 
 *  init:   Start a Post Office Protocol session
 */

static int
pop_getportbyname(POP *p, const char *service, 
		  const char *proto, short def)
{
#ifdef KRB5
    return krb5_getportbyname(p->context, service, proto, def);
#elif defined(KRB4)
    return k_getportbyname(service, proto, htons(def));
#else
    return htons(default);
#endif
}

int
pop_init(POP *p,int argcount,char **argmessage)
{
    struct sockaddr_storage cs_ss;
    struct sockaddr *cs = (struct sockaddr *)&cs_ss;
    socklen_t		    len;
    char                *   trace_file_name = "/tmp/popper-trace";
    int			    portnum = 0;
    int 		    optind = 0;
    int			    error;

    /*  Initialize the POP parameter block */
    memset (p, 0, sizeof(POP));

    setprogname(argmessage[0]);

    /*  Save my name in a global variable */
    p->myname = (char*)getprogname();

    /*  Get the name of our host */
    gethostname(p->myhost,MaxHostNameLen);

#ifdef KRB5
    {
	krb5_error_code ret;

	ret = krb5_init_context (&p->context);
	if (ret)
	    errx (1, "krb5_init_context failed: %d", ret);

	krb5_openlog(p->context, p->myname, &p->logf);
	krb5_set_warn_dest(p->context, p->logf);
    }
#else
    /*  Open the log file */
    roken_openlog(p->myname,POP_LOGOPTS,POP_FACILITY);
#endif

    p->auth_level = AUTH_NONE;

    if(getarg(args, num_args, argcount, argmessage, &optind)){
	arg_printusage(args, num_args, NULL, "");
	exit(1);
    }
    if(help_flag){
	arg_printusage(args, num_args, NULL, "");
	exit(0);
    }
    if(version_flag){
	print_version(NULL);
	exit(0);
    }

    argcount -= optind;
    argmessage += optind;

    if (argcount != 0) {
	arg_printusage(args, num_args, NULL, "");
	exit(1);
    }

    if(auth_str){
	if (strcmp (auth_str, "none") == 0)
	    p->auth_level = AUTH_NONE;
	else if(strcmp(auth_str, "otp") == 0)
	    p->auth_level = AUTH_OTP;
	else
	    warnx ("bad value for -a: %s", optarg);
    }
    /*  Debugging requested */
    p->debug = debug_flag;

    if(port_str)
	portnum = htons(atoi(port_str));
    if(trace_file){
	p->debug++;
	if ((p->trace = fopen(trace_file, "a+")) == NULL) {
	    pop_log(p, POP_PRIORITY,
		    "Unable to open trace file \"%s\", err = %d",
		    optarg,errno);
	    exit (1);
	}
	trace_file_name = trace_file;
    }

#if defined(KRB4) || defined(KRB5)
    p->kerberosp = kerberos_flag;
#endif

    if(timeout)
	pop_timeout = timeout;
    
    /* Fake inetd */
    if (interactive_flag) {
	if (portnum == 0)
	    portnum = p->kerberosp ?
		pop_getportbyname(p, "kpop", "tcp", 1109) :
	    pop_getportbyname(p, "pop", "tcp", 110);
	mini_inetd (portnum);
    }

    /*  Get the address and socket of the client to whom I am speaking */
    len = sizeof(cs_ss);
    if (getpeername(STDIN_FILENO, cs, &len) < 0) {
        pop_log(p,POP_PRIORITY,
            "Unable to obtain socket and address of client, err = %d",errno);
        exit (1);
    }

    /*  Save the dotted decimal form of the client's IP address 
        in the POP parameter block */
    inet_ntop (cs->sa_family, socket_get_address (cs),
	       p->ipaddr, sizeof(p->ipaddr));

    /*  Save the client's port */
    p->ipport = ntohs(socket_get_port (cs));

    /*  Get the canonical name of the host to whom I am speaking */
    error = getnameinfo_verified (cs, len, p->client, sizeof(p->client),
				  NULL, 0, 0);
    if (error) {
	pop_log (p, POP_PRIORITY,
		 "getnameinfo: %s", gai_strerror (error));
	strlcpy (p->client, p->ipaddr, sizeof(p->client));
    }

    /*  Create input file stream for TCP/IP communication */
    if ((p->input = fdopen(STDIN_FILENO,"r")) == NULL){
        pop_log(p,POP_PRIORITY,
            "Unable to open communication stream for input, err = %d",errno);
        exit (1);
    }

    /*  Create output file stream for TCP/IP communication */
    if ((p->output = fdopen(STDOUT_FILENO,"w")) == NULL){
        pop_log(p,POP_PRIORITY,
            "Unable to open communication stream for output, err = %d",errno);
        exit (1);
    }

    pop_log(p,POP_PRIORITY,
        "(v%s) Servicing request from \"%s\" at %s\n",
            VERSION,p->client,p->ipaddr);

#ifdef DEBUG
    if (p->trace)
        pop_log(p,POP_PRIORITY,
            "Tracing session and debugging information in file \"%s\"",
                trace_file_name);
    else if (p->debug)
        pop_log(p,POP_PRIORITY,"Debugging turned on");
#endif /* DEBUG */


    return((p->kerberosp ? krb_authenticate : plain_authenticate)(p, cs));
}
