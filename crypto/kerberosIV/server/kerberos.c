/*
 * Copyright 1985, 1986, 1987, 1988 by the Massachusetts Institute
 * of Technology.
 *
 * For copying and distribution information, please see the file
 * <mit-copyright.h>.
 */

#include "config.h"
#include "protos.h"

RCSID("$Id: kerberos.c,v 1.64 1997/05/20 18:40:46 bg Exp $");

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
#if defined(HAVE_SYS_IOCTL_H) && SunOS != 4
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

#include <des.h>
#include <krb.h>
#include <krb_db.h>
#include <prot.h>
#include <klog.h>

#include <kdc.h>

static des_key_schedule master_key_schedule;
static des_cblock master_key;

static struct timeval kerb_time;
static u_char master_key_version;
static char k_instance[INST_SZ];
static char *lt;
static int more;

static int mflag;		/* Are we invoked manually? */
static char *log_file;		/* name of alt. log file */
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
	    " [-a max_age] [-l log_file] [-r realm] [database_pathname]\n",
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

    strcpy(e_msg, "\nKerberos error -- ");
    strcat(e_msg, string);
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
    strcpy(lastrealm, r);
    return (KSUCCESS);
}


static int
kerberos(unsigned char *buf, int len, struct in_addr client, KTEXT rpkt)
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
	strcpy((char*)rpkt->dat, "Packet too short");
	return KFAILURE;
    }

    gettimeofday(&kerb_time, NULL);

    pvno = *p++;
    if(pvno != KRB_PROT_VERSION){
	msg = klog(L_KRB_PERR, "KRB protocol version mismatch (%d)", pvno);
	strcpy((char*)rpkt->dat, msg);
	return KERB_ERR_PKT_VER;
    }
    msg_type = *p++;
    lsb = msg_type & 1;
    msg_type &= ~1;
    switch(msg_type){
    case AUTH_MSG_KDC_REQUEST:
	/* XXX range check */
	p += krb_get_nir(p, name, inst, realm);
	p += krb_get_int(p, &req_time, 4, lsb);
	life = *p++;
	p += krb_get_nir(p, service, sinst, NULL);
	klog(L_INI_REQ, "AS REQ %s.%s@%s for %s.%s from %s", 
	     name, inst, realm, service, sinst, inet_ntoa(client));
	if((err = check_princ(name, inst, 0, &a_name))){
	    strcpy((char*)rpkt->dat, krb_get_err_text(err));
	    return err;
	}
	tk->length = 0;
	if((err = check_princ(service, sinst, 0, &s_name))){
	    strcpy((char*)rpkt->dat, krb_get_err_text(err));
	    return err;
	}
	life = min(life, s_name.max_life);
	life = min(life, a_name.max_life);
    
	des_new_random_key(&session);
	copy_to_key(&s_name.key_low, &s_name.key_high, key);
	unseal(&key);
	krb_create_ticket(tk, flags, a_name.name, a_name.instance, 
			  local_realm, client.s_addr, session, 
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
	strcpy(realm, (char*)buf + 3);
	if((err = set_tgtkey(realm))){
	    msg = klog(L_ERR_UNK, "Unknown realm %s from %s", 
		       realm, inet_ntoa(client));
	    strcpy((char*)rpkt->dat, msg);
	    return err;
	}
	p = buf + strlen(realm) + 4;
	p = p + p[0] + p[1] + 2;
	auth->length = p - buf;
	memcpy(auth->dat, buf, auth->length);
	err = krb_rd_req(auth, KRB_TICKET_GRANTING_TICKET,
			 realm, client.s_addr, &ad, 0);
	if(err){
	    msg = klog(L_ERR_UNK, "krb_rd_req from %s: %s", 
		       inet_ntoa(client), krb_get_err_text(err));
	    strcpy((char*)rpkt->dat, msg);
	    return err;
	}
	p += krb_get_int(p, &req_time, 4, lsb);
	life = *p++;
	p += krb_get_nir(p, service, sinst, NULL);
	klog(L_APPL_REQ, "APPL REQ %s.%s@%s for %s.%s from %s",
	     ad.pname, ad.pinst, ad.prealm, 
	     service, sinst, 
	     inet_ntoa(client));
	if(strcmp(ad.prealm, realm)){
	    msg = klog(L_ERR_UNK, "Can't hop realms: %s -> %s", 
		       realm, ad.prealm);
	    strcpy((char*)rpkt->dat, msg);
	    return KERB_ERR_PRINCIPAL_UNKNOWN;
	}

	if(!strcmp(service, "changepw")){
	    strcpy((char*)rpkt->dat, 
		   "Can't authorize password changed based on TGT");
	    return KERB_ERR_PRINCIPAL_UNKNOWN;
	}

	err = check_princ(service, sinst, life, &s_name);
	if(err){
	    strcpy((char*)rpkt->dat, krb_get_err_text(err));
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
			  client.s_addr, &session, life, kerb_time.tv_sec,
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
	msg = klog(L_KRB_PERR, "Unknown message type: %d from %s", 
		   msg_type, inet_ntoa(client));
	strcpy((char*)rpkt->dat, msg);
	return KFAILURE;
    }
}


static void
kerberos_wrap(int s, KTEXT data, struct sockaddr_in *client)
{
    KTEXT_ST pkt;
    int err = kerberos(data->dat, data->length, client->sin_addr, &pkt);
    if(err == -1)
	return;
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
};

static void
mksocket(struct descr *d, struct in_addr addr, int type, 
	 const char *service, int port)
{
    struct sockaddr_in sina;
    int     on = 1;
    int sock;

    memset(d, 0, sizeof(struct descr));
    if ((sock = socket(AF_INET, type, 0)) < 0)
	err (1, "socket");
#if defined(SO_REUSEADDR) && defined(HAVE_SETSOCKOPT)
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (void *)&on,
		   sizeof(on)) < 0)
	warn ("setsockopt (SO_REUSEADDR)");
#endif
    memset(&sina, 0, sizeof(sina));
    sina.sin_family = AF_INET;
    sina.sin_port   = port;
    sina.sin_addr   = addr;
    if (bind(sock, (struct sockaddr *)&sina, sizeof(sina)) < 0)
	err (1, "bind '%s/%s' (%d)",
	     service, (type == SOCK_DGRAM) ? "udp" : "tcp",
	     ntohs(sina.sin_port));
    
    if(type == SOCK_STREAM)
	listen(sock, SOMAXCONN);
    d->s = sock;
    d->type = type;
}


static void loop(struct descr *fds, int maxfd);

int
main(int argc, char **argv)
{
    int     child;
    int c;
    struct descr *fds;
    int nfds;
    int i;
    int n;
    int     kerror;

    umask(077);		/* Create protected files */

    set_progname (argv[0]);

    while ((c = getopt(argc, argv, "snmp:a:l:r:")) != EOF) {
	switch(c) {
	case 's':
	    /*
	     * Set parameters to slave server defaults.
	     */
	    if (max_age == -1 && !nflag)
		max_age = ONE_DAY;	/* 24 hours */
	    if (pause_int == -1)
		pause_int = FIVE_MINUTES; /* 5 minutes */
#if 0
	    if (log_file == NULL) {
		/* this is only silly */
		log_file = KRBSLAVELOG;
	    }
#endif
	    break;
	case 'n':
	    max_age = -1;	/* don't check max age. */
	    nflag++;
	    break;
	case 'm':
	    mflag++;		/* running manually; prompt for master key */
	    break;
	case 'p':
	    /* Set pause interval. */
	    if (!isdigit(optarg[0]))
		usage();
	    pause_int = atoi(optarg);
	    if ((pause_int < 5) ||  (pause_int > ONE_HOUR)) {
		fprintf(stderr, "pause_int must be between 5 and 3600 seconds.\n");
		usage();
	    }
	    break;
	case 'a':
	    /* Set max age. */
	    if (!isdigit(optarg[0])) 
		usage();
	    max_age = atoi(optarg);
	    if ((max_age < ONE_HOUR) || (max_age > THREE_DAYS)) {
		fprintf(stderr, "max_age must be between one hour and three days, in seconds\n");
		usage();
	    }
	    break;
	case 'l':
	    /* Set alternate log file */
	    log_file = optarg;
	    break;
	case 'r':
	    /* Set realm name */
	    rflag++;
	    strcpy(local_realm, optarg);
	    break;
	default:
	    usage();
	    break;
	}
    }
    
    if(log_file == NULL)
	log_file = KRBLOG;

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
    
    /* find our hostname, and use it as the instance */
    if (k_gethostname(k_instance, INST_SZ))
	err (1, "gethostname");

    /*
     * Yes this looks backwards but it has to be this way to enable a
     * smooth migration to the new port 88.
     */
    {
      int p1, p2;
      struct in_addr *a;

      p1 = k_getportbyname ("kerberos-iv", "udp", htons(750));
      p2 = k_getportbyname ("kerberos-sec", "udp", htons(88));

      if (p1 == p2)
	{
	  fprintf(stderr, "Either define kerberos-iv/udp as 750\n");
	  fprintf(stderr, "      and kerberos-sec/udp as 88\n");
	  fprintf(stderr, "or the other way around!");
	  exit(1);
	}

      nfds = k_get_all_addrs (&a);
      if (nfds < 0) {
	   struct in_addr any;

	   any.s_addr = INADDR_ANY;

	   fprintf (stderr, "Could not get local addresses, "
		    "binding to INADDR_ANY\n");
	   nfds = 1;
	   a = malloc(sizeof(*a) * nfds);
	   memcpy(a, &any, sizeof(struct in_addr));
      }
      nfds *= 4;
      fds = (struct descr*)malloc(nfds * sizeof(struct descr));
      for (i = 0; i < nfds/4; i++) {
	  mksocket(fds + 4 * i + 0, a[i], SOCK_DGRAM, "kerberos-iv", p1);
	  mksocket(fds + 4 * i + 1, a[i], SOCK_DGRAM, "kerberos-sec", p2);
	  mksocket(fds + 4 * i + 2, a[i], SOCK_STREAM, "kerberos-iv", p1);
	  mksocket(fds + 4 * i + 3, a[i], SOCK_STREAM, "kerberos-sec", p2);
      }
      free (a);
    }
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
      klog (L_KRB_PERR, "kerberos: couldn't get master key.\n");
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
loop(struct descr *fds, int nfds)
{
    for (;;) {
	int ret;
        fd_set readfds;
	struct timeval tv;
	int maxfd = 0;
	struct descr *n, *minfree;
	
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
	}
	/* add more space for sockets */
	if(minfree == NULL){
	    int i = nfds;
	    struct descr *new;
	    nfds *=2;
	    new = realloc(fds, sizeof(struct descr) * nfds);
	    if(new){
		fds = new;
		minfree = fds + i;
		for(; i < nfds; i++) fds[i].s = -1;
	    }
	}
	ret = select(maxfd + 1, &readfds, 0, 0, 0);
	for (n = fds; n < fds + nfds; n++){
	    if(n->s < 0) continue;
	    if (FD_ISSET(n->s, &readfds)){
		if(n->type == SOCK_STREAM && n->timeout == 0){
		    /* add accepted socket to list of sockets we are
                       selecting on */
		    int s = accept(n->s, NULL, 0);
		    if(minfree == NULL){
			kerb_err_reply(s, NULL, KFAILURE, "Out of memory");
			close(s);
		    }else{
			minfree->s = s;
			minfree->type = SOCK_STREAM;
			gettimeofday(&tv, NULL);
			minfree->timeout = tv.tv_sec + 4; /* XXX */
		    }
		}else{
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
			continue;
		    }
		    n->buf.length += b;
		    if(n->type == SOCK_STREAM){
			if(n->buf.length >= 4 && n->buf.dat[0] == 0){
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
			    kerberos_wrap(n->s, &n->buf, &from);
			    n->buf.length = 0;
			    close(n->s);
			    n->s = -1;
			}
		    }else{
			/* udp packets are atomic */
			kerberos_wrap(n->s, &n->buf, &from);
			n->buf.length = 0;
		    }
		}
	    }
	}
    }
}
