/*
 * Copyright 1985, 1986, 1987, 1988 by the Massachusetts Institute
 * of Technology.
 *
 * For copying and distribution information, please see the file
 * <mit-copyright.h>.
 */
/* $FreeBSD$ */

#include "config.h"
#include "protos.h"

RCSID("$Id: kerberos.c,v 1.87.2.3 2000/10/18 20:24:13 assar Exp $");

/*
 * If support for really large numbers of network interfaces is
 * desired, define FD_SETSIZE to some suitable value.
 */
#define FD_SETSIZE (4*1024)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef TIME_WITH_SYS_TIME
#include <sys/time.h>
#include <time.h>
#elif defined(HAVE_SYS_TIME_H)
#include <sys/time.h>
#else
#include <time.h>
#endif

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#include <errno.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#if defined(HAVE_SYS_IOCTL_H) && SunOS != 40
#include <sys/ioctl.h>
#endif
#ifdef HAVE_SYS_FILIO_H
#include <sys/filio.h>
#endif /* HAVE_SYS_FILIO_H */

#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#include <err.h>

#ifdef SOCKS
#include <socks.h>
#endif

#include <roken.h>
#include <base64.h>

#define OPENSSL_DES_LIBDES_COMPATIBILITY
#include <openssl/des.h>
#include <krb.h>
#include <krb_db.h>
#include <prot.h>
#include <klog.h>

#include <krb_log.h>

#include <kdc.h>

static des_key_schedule master_key_schedule;
static des_cblock master_key;

static struct timeval kerb_time;
static u_char master_key_version;
static char *lt;
static int more;

static int mflag;		/* Are we invoked manually? */
static char *log_file = KRBLOG;	/* name of alt. log file */
static int nflag;		/* don't check max age */
static int rflag;		/* alternate realm specified */

/* fields within the received request packet */
static char *req_name_ptr;
static char *req_inst_ptr;
static char *req_realm_ptr;
static u_int32_t req_time_ws;

static char local_realm[REALM_SZ];

/* options */
static int max_age = -1;
static int pause_int = -1;

/*
 * Print usage message and exit.
 */
static void
usage(void)
{
    fprintf(stderr, "Usage: %s [-s] [-m] [-n] [-p pause_seconds]"
	    " [-a max_age] [-l log_file] [-i address_to_listen_on]"
	    " [-r realm] [database_pathname]\n",
	    __progname);
    exit(1);
}

/*
 * kerb_err_reply creates an error reply packet and sends it to the
 * client. 
 */

static void
kerb_err_reply(int f, struct sockaddr_in *client, int err, char *string)
{
    static KTEXT_ST e_pkt_st;
    KTEXT   e_pkt = &e_pkt_st;
    static char e_msg[128];

    snprintf (e_msg, sizeof(e_msg),
	      "\nKerberos error -- %s", string);
    cr_err_reply(e_pkt, req_name_ptr, req_inst_ptr, req_realm_ptr,
		 req_time_ws, err, e_msg);
    sendto(f, (char*)e_pkt->dat, e_pkt->length, 0, (struct sockaddr *)client,
	   sizeof(*client));
}

static void
hang(void)
{
    if (pause_int == -1) {
	klog(L_KRB_PERR, "Kerberos will pause so as not to loop init");
	for (;;)
	    pause();
    } else {
	char buf[256];
	snprintf(buf, sizeof(buf),
		 "Kerberos will wait %d seconds before dying so as not to loop init",
		 pause_int);
	klog(L_KRB_PERR, buf);
	sleep(pause_int);
	klog(L_KRB_PERR, "Do svedania....\n");
	exit(1);
    }
}

static int
check_princ(char *p_name, char *instance, unsigned int lifetime, Principal *p)
{
    static int n;
    static int more;

    n = kerb_get_principal(p_name, instance, p, 1, &more);
    
    if (n < 0) {
	lt = klog(L_KRB_PERR, "Database unavailable!");
	hang();
    }
    
    /*
     * if more than one p_name, pick one, randomly create a session key,
     * compute maximum lifetime, lookup authorizations if applicable,
     * and stuff into cipher. 
     */
    if (n == 0) {
	/* service unknown, log error, skip to next request */
	lt = klog(L_ERR_UNK, "UNKNOWN %s.%s", p_name, instance);
	return KERB_ERR_PRINCIPAL_UNKNOWN;
    }
    if (more) {
	/* not unique, log error */
	lt = klog(L_ERR_NUN, "Principal not unique %s.%s", p_name, instance);
	return KERB_ERR_PRINCIPAL_NOT_UNIQUE;
    }
    /* If the user's key is null, we want to return an error */
    if ((p->key_low == 0) && (p->key_high == 0)) {
	/* User has a null key */
	lt = klog(L_ERR_NKY, "Null key %s.%s", p_name, instance);
	return KERB_ERR_NULL_KEY;
    }
    if (master_key_version != p->kdc_key_ver) {
	/* log error reply */
	lt = klog(L_ERR_MKV,
		  "Incorrect master key version for %s.%s: %d (should be %d)",
		  p->name, p->instance, p->kdc_key_ver, master_key_version);
	return KERB_ERR_NAME_MAST_KEY_VER;
    }
    /* make sure the service hasn't expired */
    if ((u_int32_t) p->exp_date < (u_int32_t) kerb_time.tv_sec) {
	/* service did expire, log it */
	time_t t = p->exp_date;
	lt = klog(L_ERR_SEXP,
		  "Principal %s.%s expired at %s", p->name, p->instance,
		  krb_stime(&t));
	return KERB_ERR_NAME_EXP;
    }
    /* ok is zero */
    return 0;
}

static void
unseal(des_cblock *key)
{
    kdb_encrypt_key(key, key, &master_key, master_key_schedule, DES_DECRYPT);
}


/* Set the key for krb_rd_req so we can check tgt */
static int
set_tgtkey(char *r)
              			/* Realm for desired key */
{
    int     n;
    static char lastrealm[REALM_SZ];
    Principal p_st;
    Principal *p = &p_st;
    des_cblock key;

    if (!strcmp(lastrealm, r))
	return (KSUCCESS);

    klog(L_ALL_REQ, "Getting key for %s", r);

    n = kerb_get_principal(KRB_TICKET_GRANTING_TICKET, r, p, 1, &more);
    if (n == 0)
	return (KFAILURE);

    /* unseal tgt key from master key */
    copy_to_key(&p->key_low, &p->key_high, key);
    unseal(&key);
    krb_set_key(key, 0);
    strlcpy (lastrealm, r, REALM_SZ);
    return (KSUCCESS);
}


static int
kerberos(unsigned char *buf, int len,
	 char *proto, struct sockaddr_in *client,
	 struct sockaddr_in *server,
	 KTEXT rpkt)
{
    int pvno;
    int msg_type;
    int lsb;
    int life;
    int flags = 0;
    char name[ANAME_SZ], inst[INST_SZ], realm[REALM_SZ];
    char service[SNAME_SZ], sinst[INST_SZ];
    u_int32_t req_time;
    static KTEXT_ST ticket, cipher, adat;
    KTEXT tk = &ticket, ciph = &cipher, auth = &adat;
    AUTH_DAT ad;
    des_cblock session, key;
    int err;
    Principal a_name, s_name;
    
    char *msg;
    
    
    unsigned char *p = buf;
    if(len < 2){
	strlcpy((char*)rpkt->dat,
			"Packet too short",
			sizeof(rpkt->dat));
	return KFAILURE;
    }

    gettimeofday(&kerb_time, NULL);

    pvno = *p++;
    if(pvno != KRB_PROT_VERSION){
	msg = klog(L_KRB_PERR, "KRB protocol version mismatch (%d)", pvno);
	strlcpy((char*)rpkt->dat,
			msg,
			sizeof(rpkt->dat));
	return KERB_ERR_PKT_VER;
    }
    msg_type = *p++;
    lsb = msg_type & 1;
    msg_type &= ~1;
    switch(msg_type){
    case AUTH_MSG_KDC_REQUEST:
	/* XXX range check */
	p += krb_get_nir(p, name, sizeof(name),
			 inst, sizeof(inst),
			 realm, sizeof(realm));
	p += krb_get_int(p, &req_time, 4, lsb);
	life = *p++;
	p += krb_get_nir(p, service, sizeof(service),
			 sinst, sizeof(sinst), NULL, 0);
	klog(L_INI_REQ,
	     "AS REQ %s.%s@%s for %s.%s from %s (%s/%u)", 
	     name, inst, realm, service, sinst,
	     inet_ntoa(client->sin_addr),
	     proto, ntohs(server->sin_port));
	if((err = check_princ(name, inst, 0, &a_name))){
	    strlcpy((char*)rpkt->dat,
			    krb_get_err_text(err),
			    sizeof(rpkt->dat));
	    return err;
	}
	tk->length = 0;
	if((err = check_princ(service, sinst, 0, &s_name))){
	    strlcpy((char*)rpkt->dat,
			    krb_get_err_text(err),
			    sizeof(rpkt->dat));
	    return err;
	}
	life = min(life, s_name.max_life);
	life = min(life, a_name.max_life);
    
	des_new_random_key(&session);
	copy_to_key(&s_name.key_low, &s_name.key_high, key);
	unseal(&key);
	krb_create_ticket(tk, flags, a_name.name, a_name.instance, 
			  local_realm, client->sin_addr.s_addr,
			  session, 
			  life, kerb_time.tv_sec, 
			  s_name.name, s_name.instance, &key);
	copy_to_key(&a_name.key_low, &a_name.key_high, key);
	unseal(&key);
	create_ciph(ciph, session, s_name.name, s_name.instance,
		    local_realm, life, s_name.key_version, tk, 
		    kerb_time.tv_sec, &key);
	memset(&session, 0, sizeof(session));
	memset(&key, 0, sizeof(key));
	{
	    KTEXT r;
	    r = create_auth_reply(name, inst, realm, req_time, 0, 
				  a_name.exp_date, a_name.key_version, ciph);
	    memcpy(rpkt, r, sizeof(*rpkt));
	}
	return 0;
    case AUTH_MSG_APPL_REQUEST:
	strlcpy(realm, (char*)buf + 3, REALM_SZ);
	if((err = set_tgtkey(realm))){
	    msg = klog(L_ERR_UNK,
		       "Unknown realm %s from %s (%s/%u)", 
		       realm, inet_ntoa(client->sin_addr),
		       proto, ntohs(server->sin_port));
	    strlcpy((char*)rpkt->dat,
			    msg,
			    sizeof(rpkt->dat));
	    return err;
	}
	p = buf + strlen(realm) + 4;
	p = p + p[0] + p[1] + 2;
	auth->length = p - buf;
	memcpy(auth->dat, buf, auth->length);
	err = krb_rd_req(auth, KRB_TICKET_GRANTING_TICKET,
			 realm, client->sin_addr.s_addr, &ad, 0);
	if(err){
	    msg = klog(L_ERR_UNK,
		       "krb_rd_req from %s (%s/%u): %s", 
		       inet_ntoa(client->sin_addr),
		       proto,
		       ntohs(server->sin_port),
		       krb_get_err_text(err));
	    strlcpy((char*)rpkt->dat,
			    msg,
			    sizeof(rpkt->dat));
	    return err;
	}
	p += krb_get_int(p, &req_time, 4, lsb);
	life = *p++;
	p += krb_get_nir(p, service, sizeof(service),
			 sinst, sizeof(sinst), NULL, 0);
	klog(L_APPL_REQ,
	     "APPL REQ %s.%s@%s for %s.%s from %s (%s/%u)",
	     ad.pname, ad.pinst, ad.prealm,
	     service, sinst,
	     inet_ntoa(client->sin_addr),
	     proto,
	     ntohs(server->sin_port));

	if(strcmp(ad.prealm, realm)){
	    msg = klog(L_ERR_UNK, "Can't hop realms: %s -> %s", 
		       realm, ad.prealm);
	    strlcpy((char*)rpkt->dat,
			    msg,
			    sizeof(rpkt->dat));
	    return KERB_ERR_PRINCIPAL_UNKNOWN;
	}

	if(!strcmp(service, "changepw")){
	    strlcpy((char*)rpkt->dat, 
			    "Can't authorize password changed based on TGT",
			    sizeof(rpkt->dat));
	    return KERB_ERR_PRINCIPAL_UNKNOWN;
	}

	err = check_princ(service, sinst, life, &s_name);
	if(err){
	    strlcpy((char*)rpkt->dat,
			    krb_get_err_text(err),
			    sizeof(rpkt->dat));
	    return err;
	}
	life = min(life, 
		   krb_time_to_life(kerb_time.tv_sec, 
				    krb_life_to_time(ad.time_sec, 
						     ad.life)));
	life = min(life, s_name.max_life);
	copy_to_key(&s_name.key_low, &s_name.key_high, key);
	unseal(&key);
	des_new_random_key(&session);
	krb_create_ticket(tk, flags, ad.pname, ad.pinst, ad.prealm,
			  client->sin_addr.s_addr, &session,
			  life, kerb_time.tv_sec,
			  s_name.name, s_name.instance,
			  &key);
	
	memset(&key, 0, sizeof(key));

	create_ciph(ciph, session, service, sinst, local_realm,
		    life, s_name.key_version, tk,
		    kerb_time.tv_sec, &ad.session);

	memset(&session, 0, sizeof(session));
	memset(ad.session, 0, sizeof(ad.session));
	{
	    KTEXT r;
	    r =create_auth_reply(ad.pname, ad.pinst, ad.prealm, 
				 req_time, 0, 0, 0, ciph);
	    memcpy(rpkt, r, sizeof(*rpkt));
	}
	memset(&s_name, 0, sizeof(s_name));
	return 0;
	
    case AUTH_MSG_ERR_REPLY:
	return -1;
    default:
	msg = klog(L_KRB_PERR,
		   "Unknown message type: %d from %s (%s/%u)", 
		   msg_type,
		   inet_ntoa(client->sin_addr),
		   proto,
		   ntohs(server->sin_port));
	strlcpy((char*)rpkt->dat,
			msg,
			sizeof(rpkt->dat));
	return KFAILURE;
    }
}


static void
kerberos_wrap(int s, KTEXT data, char *proto, struct sockaddr_in *client, 
	      struct sockaddr_in *server)
{
    KTEXT_ST pkt;
    int http_flag = strcmp(proto, "http") == 0;
    int err = kerberos(data->dat, data->length, proto, client, server, &pkt);
    if(err == -1)
	return;
    if(http_flag){
	const char *msg = 
	    "HTTP/1.1 200 OK\r\n"
	    "Server: KTH-KRB/1\r\n"
	    "Content-type: application/octet-stream\r\n"
	    "Content-transfer-encoding: binary\r\n\r\n";
	sendto(s, msg, strlen(msg), 0, (struct sockaddr *)client,
	       sizeof(*client));
    }
    if(err){
	kerb_err_reply(s, client, err, (char*)pkt.dat);
	return;
    }
    sendto(s, pkt.dat, pkt.length, 0, (struct sockaddr *)client,
	   sizeof(*client));
}


/*
 * setup_disc 
 *
 * disconnect all descriptors, remove ourself from the process
 * group that spawned us. 
 */

static void
setup_disc(void)
{
    int     s;

    for (s = 0; s < 3; s++) {
	close(s);
    }

    open("/dev/null", 0);
    dup2(0, 1);
    dup2(0, 2);

    setsid();

    chdir("/tmp");
    return;
}

/*
 * Make sure that database isn't stale.
 *
 * Exit if it is; we don't want to tell lies.
 */

static void
check_db_age(void)
{
    long age;
    
    if (max_age != -1) {
	/* Requires existance of kerb_get_db_age() */
	gettimeofday(&kerb_time, 0);
	age = kerb_get_db_age();
	if (age == 0) {
	    klog(L_KRB_PERR, "Database currently being updated!");
	    hang();
	}
	if ((age + max_age) < kerb_time.tv_sec) {
	    klog(L_KRB_PERR, "Database out of date!");
	    hang();
	    /* NOTREACHED */
	}
    }
}

struct descr{
    int s;
    KTEXT_ST buf;
    int type;
    int timeout;
    struct sockaddr_in addr;
};

static void
mksocket(struct descr *d, struct in_addr addr, int type, 
	 const char *service, int port)
{
    int     on = 1;
    int sock;

    memset(d, 0, sizeof(struct descr));
    if ((sock = socket(AF_INET, type, 0)) < 0)
	err (1, "socket");
    if (sock >= FD_SETSIZE) {
        errno = EMFILE;
	errx(1, "Aborting: too many descriptors");
    }
#if defined(SO_REUSEADDR) && defined(HAVE_SETSOCKOPT)
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (void *)&on,
		   sizeof(on)) < 0)
	warn ("setsockopt (SO_REUSEADDR)");
#endif
    memset(&d->addr, 0, sizeof(d->addr));
    d->addr.sin_family = AF_INET;
    d->addr.sin_port   = port;
    d->addr.sin_addr   = addr;
    if (bind(sock, (struct sockaddr *)&d->addr, sizeof(d->addr)) < 0)
	err (1, "bind '%s/%s' (%d)",
	     service, (type == SOCK_DGRAM) ? "udp" : "tcp",
	     ntohs(d->addr.sin_port));
    
    if(type == SOCK_STREAM)
	listen(sock, SOMAXCONN);
    d->s = sock;
    d->type = type;
}


static void loop(struct descr *fds, int maxfd);

struct port_spec {
    int port;
    int type;
};

static int
add_port(struct port_spec **ports, int *num_ports, int port, int type)
{
    struct port_spec *tmp;
    tmp = realloc(*ports, (*num_ports + 1) * sizeof(*tmp));
    if(tmp == NULL)
	return ENOMEM;
    *ports = tmp;
    tmp[*num_ports].port = port;
    tmp[*num_ports].type = type;
    (*num_ports)++;
    return 0;
}

static void
make_sockets(const char *port_spec, struct in_addr *i_addr, 
	     struct descr **fds, int *nfds)
{
    int tp;
    struct in_addr *a;
    char *p, *q, *pos = NULL;
    struct servent *sp;
    struct port_spec *ports = NULL;
    int num_ports = 0;
    int i, j;
    char *port_spec_copy = strdup (port_spec);

    if (port_spec_copy == NULL)
	err (1, "strdup");

    for(p = strtok_r(port_spec_copy, ", \t", &pos); 
	p; 
	p = strtok_r(NULL, ", \t", &pos)){
	if(strcmp(p, "+") == 0){
	    add_port(&ports, &num_ports, 88, SOCK_DGRAM);
	    add_port(&ports, &num_ports, 88, SOCK_STREAM);
	    add_port(&ports, &num_ports, 750, SOCK_DGRAM);
	    add_port(&ports, &num_ports, 750, SOCK_STREAM);
	}else{
	    q = strchr(p, '/');
	    if(q){
		*q = 0;
		q++;
	    }
	    sp = getservbyname(p, q);
	    if(sp)
		tp = ntohs(sp->s_port);
	    else if(sscanf(p, "%d", &tp) != 1) {
		warnx("Unknown port: %s%s%s", p, q ? "/" : "", q ? q : "");
		continue;
	    }
	    if(q){
		if(strcasecmp(q, "tcp") == 0)
		    add_port(&ports, &num_ports, tp, SOCK_STREAM);
		else if(strcasecmp(q, "udp") == 0)
		    add_port(&ports, &num_ports, tp, SOCK_DGRAM);
		else
		    warnx("Unknown protocol type: %s", q);
	    }else{
		add_port(&ports, &num_ports, tp, SOCK_DGRAM);
		add_port(&ports, &num_ports, tp, SOCK_STREAM);
	    }
	}
    }
    free (port_spec_copy);

    if(num_ports == 0)
	errx(1, "No valid ports specified!");
    
    if (i_addr) {
	*nfds = 1;
	a = malloc(sizeof(*a) * *nfds);
	if (a == NULL)
	    errx (1, "Failed to allocate %lu bytes",
		  (unsigned long)(sizeof(*a) * *nfds));
	memcpy(a, i_addr, sizeof(struct in_addr));
    } else
	*nfds = k_get_all_addrs (&a);
    if (*nfds < 0) {
	struct in_addr any;

	any.s_addr = INADDR_ANY;

	warnx ("Could not get local addresses, binding to INADDR_ANY");
	*nfds = 1;
	a = malloc(sizeof(*a) * *nfds);
	if (a == NULL)
	    errx (1, "Failed to allocate %lu bytes",
		  (unsigned long)(sizeof(*a) * *nfds));
	memcpy(a, &any, sizeof(struct in_addr));
    }
    *fds = malloc(*nfds * num_ports * sizeof(**fds));
    if (*fds == NULL)
	errx (1, "Failed to allocate %lu bytes",
	      (unsigned long)(*nfds * num_ports * sizeof(**fds)));
    for (i = 0; i < *nfds; i++) {
	for(j = 0; j < num_ports; j++) {
	    mksocket(*fds + num_ports * i + j, a[i], 
		     ports[j].type, "", htons(ports[j].port));
	}
    }
    *nfds *= num_ports;
    free(ports);
    free (a);
}


int
main(int argc, char **argv)
{
    int     child;
    int c;
    struct descr *fds;
    int nfds;
    int n;
    int     kerror;
    int i_flag = 0;
    struct in_addr i_addr;
    char *port_spec = "+";

    umask(077);		/* Create protected files */

    set_progname (argv[0]);

    while ((c = getopt(argc, argv, "snmp:P:a:l:r:i:")) != -1) {
	switch(c) {
	case 's':
	    /*
	     * Set parameters to slave server defaults.
	     */
	    if (max_age == -1 && !nflag)
		max_age = THREE_DAYS;	/* Survive weekend */
	    if (pause_int == -1)
		pause_int = FIVE_MINUTES; /* 5 minutes */
	    break;
	case 'n':
	    max_age = -1;	/* don't check max age. */
	    nflag++;
	    break;
	case 'm':
	    mflag++;		/* running manually; prompt for master key */
	    break;
	case 'p': {
	    /* Set pause interval. */
	    char *tmp;

	    pause_int = strtol (optarg, &tmp, 0);
	    if (pause_int == 0 && tmp == optarg) {
		fprintf(stderr, "pause_int `%s' not a number\n", optarg);
		usage ();
	    }

	    if ((pause_int < 5) ||  (pause_int > ONE_HOUR)) {
		fprintf(stderr, "pause_int must be between 5 and 3600 seconds.\n");
		usage();
	    }
	    break;
	}
	case 'P':
	    port_spec = optarg;
	    break;
	case 'a': {
	    /* Set max age. */
	    char *tmp;

	    max_age = strtol (optarg, &tmp, 0);
	    if (max_age == 0 && tmp == optarg) {
		fprintf (stderr, "max_age `%s' not a number\n", optarg);
		usage ();
	    }
	    if ((max_age < ONE_HOUR) || (max_age > THREE_DAYS)) {
		fprintf(stderr, "max_age must be between one hour and "
			"three days, in seconds\n");
		usage();
	    }
	    break;
	}
	case 'l':
	    /* Set alternate log file */
	    log_file = optarg;
	    break;
	case 'r':
	    /* Set realm name */
	    rflag++;
	    strlcpy(local_realm, optarg, sizeof(local_realm));
	    break;
	case 'i':
	    /* Only listen on this address */
	    if(inet_aton (optarg, &i_addr) == 0) {
		fprintf (stderr, "Bad address: %s\n", optarg);
		exit (1);
	    }
	    ++i_flag;
	    break;
	default:
	    usage();
	    break;
	}
    }
    
    if (optind == (argc-1)) {
	if (kerb_db_set_name(argv[optind]) != 0) {
	    fprintf(stderr, "Could not set alternate database name\n");
	    exit(1);
	}
	optind++;
    }

    if (optind != argc)
	usage();
	
    printf("Kerberos server starting\n");
    
    if ((!nflag) && (max_age != -1))
	printf("\tMaximum database age: %d seconds\n", max_age);
    if (pause_int != -1)
	printf("\tSleep for %d seconds on error\n", pause_int);
    else
	printf("\tSleep forever on error\n");
    if (mflag)
	printf("\tMaster key will be entered manually\n");
    
    printf("\tLog file is %s\n", log_file);

    kset_logfile(log_file);
    
    make_sockets(port_spec, i_flag ? &i_addr : NULL, &fds, &nfds);

    /* do all the database and cache inits */
    if ((n = kerb_init())) {
	if (mflag) {
	    printf("Kerberos db and cache init ");
	    printf("failed = %d ...exiting\n", n);
	    exit (1);
	} else {
	    klog(L_KRB_PERR,
	    "Kerberos db and cache init failed = %d ...exiting", n);
	    hang();
	}
    }

    /* Make sure database isn't stale */
    check_db_age();
    
    /* setup master key */
    if (kdb_get_master_key (mflag, &master_key, master_key_schedule) != 0) {
      klog (L_KRB_PERR, "kerberos: couldn't get master key.");
      exit (1);
    }
    kerror = kdb_verify_master_key (&master_key, master_key_schedule, stdout);
    if (kerror < 0) {
      klog (L_KRB_PERR, "Can't verify master key.");
      memset(master_key, 0, sizeof (master_key));
      memset (master_key_schedule, 0, sizeof (master_key_schedule));
      exit (1);
    }

    master_key_version = (u_char) kerror;

    fprintf(stdout, "\nCurrent Kerberos master key version is %d\n",
	    master_key_version);
    des_init_random_number_generator(&master_key);

    if (!rflag) {
	/* Look up our local realm */
	krb_get_lrealm(local_realm, 1);
    }
    fprintf(stdout, "Local realm: %s\n", local_realm);
    fflush(stdout);

    if (set_tgtkey(local_realm)) {
	/* Ticket granting service unknown */
	klog(L_KRB_PERR, "Ticket granting ticket service unknown");
	fprintf(stderr, "Ticket granting ticket service unknown\n");
	exit(1);
    }
    if (mflag) {
	if ((child = fork()) != 0) {
	    printf("Kerberos started, PID=%d\n", child);
	    exit(0);
	}
	setup_disc();
    }
    
    klog(L_ALL_REQ, "Starting Kerberos for %s (kvno %d)", 
	 local_realm, master_key_version);
    
    /* receive loop */
    loop(fds, nfds);
    exit(1);
}


static void
read_socket(struct descr *n)
{
    int b;
    struct sockaddr_in from;
    int fromlen = sizeof(from);
    b = recvfrom(n->s, n->buf.dat + n->buf.length, 
		 MAX_PKT_LEN - n->buf.length, 0, 
		 (struct sockaddr *)&from, &fromlen);
    if(b < 0){
	if(n->type == SOCK_STREAM){
	    close(n->s);
	    n->s = -1;
	}
	n->buf.length = 0;
	return;
    }
    n->buf.length += b;
    if(n->type == SOCK_STREAM){
	char *proto = "tcp";
	if(n->buf.length > 4 && 
	   strncmp((char *)n->buf.dat, "GET ", 4) == 0 &&
	   strncmp((char *)n->buf.dat + n->buf.length - 4, 
		   "\r\n\r\n", 4) == 0){
	    char *p;
	    char *save = NULL;

	    n->buf.dat[n->buf.length - 1] = 0;
	    strtok_r((char *)n->buf.dat, " \t\r\n", &save);
	    p = strtok_r(NULL, " \t\r\n", &save);
	    if(p == NULL)
		p = "";
	    if(*p == '/') p++;
	    n->buf.length = base64_decode(p, n->buf.dat);
	    if(n->buf.length <= 0){
		const char *msg = 
		    "HTTP/1.1 404 Not found\r\n"
		    "Server: KTH-KRB/1\r\n"
		    "Content-type: text/html\r\n"
		    "Content-transfer-encoding: 8bit\r\n\r\n"
		    "<TITLE>404 Not found</TITLE>\r\n"
		    "<H1>404 Not found</H1>\r\n"
		    "That page does not exist. Information about "
		    "<A HREF=\"http://www.pdc.kth.se/kth-krb\">KTH-KRB</A> "
		    "is available elsewhere.\r\n";
		fromlen = sizeof(from);
		if(getpeername(n->s,(struct sockaddr*)&from, &fromlen) == 0)
		    klog(L_KRB_PERR, "Unknown HTTP request from %s", 
			 inet_ntoa(from.sin_addr));
		else
		    klog(L_KRB_PERR, "Unknown HTTP request from <unknown>");
		write(n->s, msg, strlen(msg));
		close(n->s);
		n->s = -1;
		n->buf.length = 0;
		return;
	    }
	    proto = "http";
	    b = 0;
	}
	else if(n->buf.length >= 4 && n->buf.dat[0] == 0){
	    /* if this is a new type of packet (with
	       the length attached to the head of the
	       packet), and there is no more data to
	       be read, fake an old packet, so the
	       code below will work */
	    u_int32_t len;
	    krb_get_int(n->buf.dat, &len, 4, 0);
	    if(n->buf.length == len + 4){
		memmove(n->buf.dat, n->buf.dat + 4, len);
		b = 0;
	    }
	}
	if(b == 0){
	    /* handle request if there are 
	       no more bytes to read */
	    fromlen = sizeof(from);
	    getpeername(n->s,(struct sockaddr*)&from, &fromlen);
	    kerberos_wrap(n->s, &n->buf, proto, &from,
			  &n->addr);
	    n->buf.length = 0;
	    close(n->s);
	    n->s = -1;
	}
    }else{
	/* udp packets are atomic */
	kerberos_wrap(n->s, &n->buf, "udp", &from,
		      &n->addr);
	n->buf.length = 0;
    }
}

static fd_set readfds;

static void
loop(struct descr *fds, int base_nfds)
{
    int nfds = base_nfds;
    int max_tcp = min(FD_SETSIZE, getdtablesize()) - fds[base_nfds - 1].s;
    if (max_tcp <= 10) {
        errno = EMFILE;
 	errx(1, "Aborting: too many descriptors");
    }
    max_tcp -= 10;		/* We need a few extra for DB, logs, etc. */
    if (max_tcp > 100) max_tcp = 100; /* Keep to some sane limit. */

    for (;;) {
	int ret;
	struct timeval tv;
	int next_timeout = 10;	/* In seconds */
	int maxfd = 0;
	struct descr *n, *minfree;
	int accepted; /* accept at most one socket per `round' */
	
	FD_ZERO(&readfds);
	gettimeofday(&tv, NULL);
	maxfd = 0;
	minfree = NULL;
	/* Remove expired TCP sockets, and add all other 
	   to the set we are selecting on */
	for(n = fds; n < fds + nfds; n++){
	    if(n->s >= 0 && n->timeout && tv.tv_sec > n->timeout){
		kerb_err_reply(n->s, NULL, KERB_ERR_TIMEOUT, "Timeout");
		close(n->s);
		n->s = -1;
	    }
	    if(n->s < 0){
		if(minfree == NULL) minfree = n;
		continue;
	    }
	    FD_SET(n->s, &readfds);
	    maxfd = max(maxfd, n->s);
	    next_timeout = min(next_timeout, tv.tv_sec - n->timeout);
	}
	/* add more space for sockets */
	if (minfree == NULL && nfds < base_nfds + max_tcp) {
	    int i = nfds;
	    struct descr *new;
	    nfds *=2;
	    if (nfds > base_nfds + max_tcp)
	        nfds = base_nfds + max_tcp;
	    new = realloc(fds, sizeof(struct descr) * nfds);
	    if(new){
		fds = new;
		minfree = fds + i;
		for(; i < nfds; i++) fds[i].s = -1;
	    }
	}
	if (minfree == NULL) {
	    /*
	     * We are possibly the subject of a DOS attack, pick a TCP
	     * connection at random and drop it.
	     */
	    int r = rand() % (nfds - base_nfds);
	    r = r + base_nfds;
	    FD_CLR(fds[r].s, &readfds);
	    close(fds[r].s);
	    fds[r].s = -1;
	    minfree = &fds[r];
	}
	if (next_timeout < 0) next_timeout = 0;
	tv.tv_sec = next_timeout;
	tv.tv_usec = 0;
	ret = select(maxfd + 1, &readfds, 0, 0, &tv);
	if (ret < 0) {
	    if (errno != EINTR)
	        klog(L_KRB_PERR, "select: %s", strerror(errno));
	  continue;
	}
	accepted = 0;
	for (n = fds; n < fds + nfds; n++){
	    if(n->s < 0) continue;
	    if (FD_ISSET(n->s, &readfds)){
		if(n->type == SOCK_STREAM && n->timeout == 0){
		    /* add accepted socket to list of sockets we are
                       selecting on */
		    int s;
		    if(accepted) continue;
		    accepted = 1;
		    s = accept(n->s, NULL, 0);
		    if (minfree == NULL || s >= FD_SETSIZE) {
			close(s);
		    }else{
			minfree->s = s;
			minfree->type = SOCK_STREAM;
			gettimeofday(&tv, NULL);
			minfree->timeout = tv.tv_sec + 4; /* XXX */
			minfree->buf.length = 0;
			memcpy(&minfree->addr, &n->addr, sizeof(minfree->addr));
		    }
		}else
		    read_socket(n);
	    }
	}
    }
}
