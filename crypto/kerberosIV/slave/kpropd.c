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

#include "slav_locl.h"

#include "kprop.h"

RCSID("$Id: kpropd.c,v 2.21 1997/05/02 17:52:13 assar Exp $");

#ifndef SBINDIR
#define SBINDIR "/usr/athena/sbin"
#endif

struct sockaddr_in master, slave;

char *database = DBM_FILE;

char *lockfile = DB_DIR "/slave_propagation";

char *logfile = K_LOGFIL;

char *kdb_util = SBINDIR "/kdb_util";

char *kdb_util_command = "load";

char *srvtab = "";

char realm[REALM_SZ];

static
int
copy_data(int from, int to, des_cblock *session, des_key_schedule schedule)
{
    unsigned char tmp[4];
    char buf[KPROP_BUFSIZ + 26];
    u_int32_t length;
    int n;
    
    int kerr;
    MSG_DAT m;

    while(1){
	n = krb_net_read(from, tmp, 4);
	if(n == 0)
	    break;
	if(n < 0){
	    klog(L_KRB_PERR, "krb_net_read: %s", strerror(errno));
	    return -1;
	}
	if(n != 4){
	    klog(L_KRB_PERR, "Premature end of data");
	    return -1;
	}
	length = (tmp[0] << 24) | (tmp[1] << 16) | (tmp[2] << 8) | tmp[3];
	if(length > sizeof(buf)){
	    klog(L_KRB_PERR, "Giant packet received: %d", length);
	    return -1;
	}
	if(krb_net_read(from, buf, length) != length){
	    klog(L_KRB_PERR, "Premature end of data");
	    return -1;
	}
	kerr = krb_rd_priv (buf, length, schedule, session, &master, &slave, &m);
	if(kerr != KSUCCESS){
	    klog(L_KRB_PERR, "Kerberos error: %s", krb_get_err_text(kerr));
	    return -1;
	}
	write(to, m.app_data, m.app_length);
    }
    return 0;
}


static
int
kprop(int s)
{
    char buf[128];
    int n;
    KTEXT_ST ticket;
    AUTH_DAT ad;
    char sinst[INST_SZ];
    char command[1024];
    des_key_schedule schedule;
    int mode;
    int kerr;
    int lock;
    
    n = sizeof(master);
    if(getpeername(s, (struct sockaddr*)&master, &n) < 0){
	klog(L_KRB_PERR, "getpeername: %s", strerror(errno));
	return 1;
    }
    
    n = sizeof(slave);
    if(getsockname(s, (struct sockaddr*)&slave, &n) < 0){
	klog(L_KRB_PERR, "getsockname: %s", strerror(errno));
	return 1;
    }

    klog(L_KRB_PERR, "Connection from %s", inet_ntoa(master.sin_addr));

    n = krb_net_read(s, buf, KPROP_PROT_VERSION_LEN + 2);
    if(n < KPROP_PROT_VERSION_LEN + 2){
	klog(L_KRB_PERR, "Premature end of data");
	return 1;
    }
    if(memcmp(buf, KPROP_PROT_VERSION, KPROP_PROT_VERSION_LEN) != 0){
	klog(L_KRB_PERR, "Bad protocol version string received");
	return 1;
    }
    mode = (buf[n-2] << 8) | buf[n-1];
    if(mode != KPROP_TRANSFER_PRIVATE){
	klog(L_KRB_PERR, "Bad transfer mode received: %d", mode);
	return 1;
    }
    k_getsockinst(s, sinst, sizeof(sinst));
    kerr = krb_recvauth(KOPT_DO_MUTUAL, s, &ticket,
			KPROP_SERVICE_NAME, sinst,
			&master, &slave,
			&ad, srvtab, schedule, 
			buf);
    if(kerr != KSUCCESS){
	klog(L_KRB_PERR, "Kerberos error: %s", krb_get_err_text(kerr));
	return 1;
    }
    des_set_key(&ad.session, schedule);
    
    lock = open(lockfile, O_WRONLY|O_CREAT, 0600);
    if(lock < 0){
	klog(L_KRB_PERR, "Failed to open file: %s", strerror(errno));
	return 1;
    }
    if(k_flock(lock, K_LOCK_EX | K_LOCK_NB)){
	close(lock);
	klog(L_KRB_PERR, "Failed to lock file: %s", strerror(errno));
	return 1;
    }
    
    if(ftruncate(lock, 0) < 0){
	close(lock);
	klog(L_KRB_PERR, "Failed to lock file: %s", strerror(errno));
	return 1;
    }

    if(copy_data(s, lock, &ad.session, schedule)){
	close(lock);
	return 1;
    }
    close(lock);
    snprintf(command, sizeof(command),
	     "%s %s %s %s", kdb_util, kdb_util_command, 
	    lockfile, database);
    if(system(command) == 0){
	klog(L_KRB_PERR, "Propagation finished successfully");
	return 0;
    }
    klog(L_KRB_PERR, "*** Propagation failed ***");
    return 1;
}

static int
doit(void)
{
    return kprop(0);
}

static int
doit_interactive(void)
{
    struct sockaddr_in sa;
    int salen;
    int s, s2;
    int ret;

    s = socket(AF_INET, SOCK_STREAM, 0);
    if(s < 0){
      klog(L_KRB_PERR, "socket: %s", strerror(errno));
      return 1;
    }
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = k_getportbyname ("krb_prop", "tcp", htons(KPROP_PORT));
    ret = bind(s, (struct sockaddr*)&sa, sizeof(sa));
    if (ret < 0) {
      klog(L_KRB_PERR, "bind: %s", strerror(errno));
      return 1;
    }
    ret = listen(s, SOMAXCONN);
    if (ret < 0) {
      klog(L_KRB_PERR, "listen: %s", strerror(errno));
      return 1;
    }
    for(;;) {
      salen = sizeof(sa);
      s2 = accept(s, (struct sockaddr*)&sa, &salen);
      switch(fork()){
      case -1:
	klog(L_KRB_PERR, "fork: %s", strerror(errno));
	return 1;
      case 0:
	close(s);
	kprop(s2);
	return 1;
      default: {
	  int status;
	  close(s2);
	  wait(&status);
	}
      }
    }
}

static void
usage (void)
{
     fprintf (stderr, 
	      "Usage: kpropd [-i] [-d database] [-l log] [-m] [-[p|P] program]"
	      " [-r realm] [-s srvtab]\n");
     exit (1);
}

int
main(int argc, char **argv)
{
    int opt;
    int interactive = 0;

    krb_get_lrealm(realm, 1);
    
    while((opt = getopt(argc, argv, ":d:l:mp:P:r:s:i")) >= 0){
	switch(opt){
	case 'd':
	    database = optarg;
	    break;
	case 'l':
	    logfile = optarg;
	    break;
	case 'm':
	    kdb_util_command = "merge";
	    break;
	case 'p':
	case 'P':
	    kdb_util = optarg;
	    break;
	case 'r':
	    strcpy(realm, optarg);
	    break;
	case 's':
	    srvtab = optarg;
	    break;
	case 'i':
	    interactive = 1;
	    break;
	default:
	    klog(L_KRB_PERR, "Bad option: -%c", optopt);
	    usage ();
	    exit(1);
	}
    }
    kset_logfile(logfile);
    if (interactive)
      return doit_interactive ();
    else
      return doit ();
}
