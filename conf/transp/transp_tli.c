/*
 * Copyright (c) 1997-2014 Erez Zadok
 * Copyright (c) 1990 Jan-Simon Pendry
 * Copyright (c) 1990 Imperial College of Science, Technology & Medicine
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry at Imperial College, London.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *
 * File: am-utils/conf/transp/transp_tli.c
 *
 * TLI specific utilities.
 *      -Erez Zadok <ezk@cs.columbia.edu>
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */
#include <am_defs.h>
#include <amu.h>

struct netconfig *nfsncp;


/*
 * find the IP address that can be used to connect to the local host
 */
void
amu_get_myaddress(struct in_addr *iap, const char *preferred_localhost)
{
  int ret;
  voidp handlep;
  struct netconfig *ncp;
  struct nd_addrlist *addrs = (struct nd_addrlist *) NULL;
  struct nd_hostserv service;

  handlep = setnetconfig();
  ncp = getnetconfig(handlep);
  service.h_host = (preferred_localhost ? (char *) preferred_localhost : HOST_SELF_CONNECT);
  service.h_serv = (char *) NULL;

  ret = netdir_getbyname(ncp, &service, &addrs);

  if (ret || !addrs || addrs->n_cnt < 1) {
    plog(XLOG_FATAL, "cannot get local host address. using 127.0.0.1");
    iap->s_addr = htonl(INADDR_LOOPBACK);
  } else {
    /*
     * XXX: there may be more more than one address for this local
     * host.  Maybe something can be done with those.
     */
    struct sockaddr_in *sinp = (struct sockaddr_in *) addrs->n_addrs[0].buf;
    char dq[20];
    if (preferred_localhost)
      plog(XLOG_INFO, "localhost_address \"%s\" requested, using %s",
	   preferred_localhost, inet_dquad(dq, sizeof(dq), iap->s_addr));
    iap->s_addr = sinp->sin_addr.s_addr; /* XXX: used to be htonl() */
  }

  endnetconfig(handlep);	/* free's up internal resources too */
  netdir_free((voidp) addrs, ND_ADDRLIST);
}


/*
 * How to bind to reserved ports.
 * TLI handle (socket) and port version.
 */
int
bind_resv_port(int td, u_short *pp)
{
  int rc = -1, port;
  struct t_bind *treq, *tret;
  struct sockaddr_in *sin;

  treq = (struct t_bind *) t_alloc(td, T_BIND, T_ADDR);
  if (!treq) {
    plog(XLOG_ERROR, "t_alloc req");
    return -1;
  }
  tret = (struct t_bind *) t_alloc(td, T_BIND, T_ADDR);
  if (!tret) {
    t_free((char *) treq, T_BIND);
    plog(XLOG_ERROR, "t_alloc ret");
    return -1;
  }
  memset((char *) treq->addr.buf, 0, treq->addr.len);
  sin = (struct sockaddr_in *) treq->addr.buf;
  sin->sin_family = AF_INET;
  treq->qlen = 64; /* 0 is ok for udp, for tcp you need qlen>0 */
  treq->addr.len = treq->addr.maxlen;
  errno = EADDRINUSE;
  port = IPPORT_RESERVED;

  do {
    --port;
    sin->sin_port = htons(port);
    rc = t_bind(td, treq, tret);
    if (rc < 0) {
      plog(XLOG_ERROR, "t_bind");
    } else {
      if (memcmp(treq->addr.buf, tret->addr.buf, tret->addr.len) == 0)
	break;
      else
	t_unbind(td);
    }
  } while ((rc < 0 || errno == EADDRINUSE) && (int) port > IPPORT_RESERVED / 2);

  if (pp) {
    if (rc == 0)
      *pp = port;
    else
      plog(XLOG_ERROR, "could not t_bind to any reserved port");
  }
  t_free((char *) tret, T_BIND);
  t_free((char *) treq, T_BIND);
  return rc;
}




/*
 * close a descriptor, TLI style
 */
int
amu_close(int fd)
{
  return t_close(fd);
}


/*
 * Create an rpc client attached to the mount daemon.
 */
CLIENT *
get_mount_client(char *host, struct sockaddr_in *unused_sin, struct timeval *tv, int *sock, u_long mnt_version)
{
  CLIENT *client;
  struct netbuf nb;
  struct netconfig *nc = NULL;
  struct sockaddr_in sin;

  nb.maxlen = sizeof(sin);
  nb.buf = (char *) &sin;

  /*
   * First try a TCP handler
   */

  /*
   * Find mountd address on TCP
   */
  if ((nc = getnetconfigent(NC_TCP)) == NULL) {
    plog(XLOG_ERROR, "getnetconfig for tcp failed: %s", nc_sperror());
    goto tryudp;
  }
  if (!rpcb_getaddr(MOUNTPROG, mnt_version, nc, &nb, host)) {
    /*
     * don't print error messages here, since mountd might legitimately
     * serve udp only
     */
    goto tryudp;
  }
  /*
   * Create privileged TCP socket
   */
  *sock = t_open(nc->nc_device, O_RDWR, 0);

  if (*sock < 0) {
    plog(XLOG_ERROR, "t_open %s: %m", nc->nc_device);
    goto tryudp;
  }
  if (bind_resv_port(*sock, (u_short *) NULL) < 0)
    plog(XLOG_ERROR, "couldn't bind mountd socket to privileged port");

  if ((client = clnt_vc_create(*sock, &nb, MOUNTPROG, mnt_version, 0, 0))
      == (CLIENT *) NULL) {
    plog(XLOG_ERROR, "clnt_vc_create failed");
    t_close(*sock);
    goto tryudp;
  }
  /* tcp succeeded */
  dlog("get_mount_client: using tcp, port %d", sin.sin_port);
  if (nc)
    freenetconfigent(nc);
  return client;

tryudp:
  /* first free possibly previously allocated netconfig entry */
  if (nc)
    freenetconfigent(nc);

  /*
   * TCP failed so try UDP
   */

  /*
   * Find mountd address on UDP
   */
  if ((nc = getnetconfigent(NC_UDP)) == NULL) {
    plog(XLOG_ERROR, "getnetconfig for udp failed: %s", nc_sperror());
    goto badout;
  }
  if (!rpcb_getaddr(MOUNTPROG, mnt_version, nc, &nb, host)) {
    plog(XLOG_ERROR, "%s",
	 clnt_spcreateerror("couldn't get mountd address on udp"));
    goto badout;
  }
  /*
   * Create privileged UDP socket
   */
  *sock = t_open(nc->nc_device, O_RDWR, 0);

  if (*sock < 0) {
    plog(XLOG_ERROR, "t_open %s: %m", nc->nc_device);
    goto badout;		/* neither tcp not udp succeeded */
  }
  if (bind_resv_port(*sock, (u_short *) NULL) < 0)
    plog(XLOG_ERROR, "couldn't bind mountd socket to privileged port");

  if ((client = clnt_dg_create(*sock, &nb, MOUNTPROG, mnt_version, 0, 0))
      == (CLIENT *) NULL) {
    plog(XLOG_ERROR, "clnt_dg_create failed");
    t_close(*sock);
    goto badout;		/* neither tcp not udp succeeded */
  }
  if (clnt_control(client, CLSET_RETRY_TIMEOUT, (char *) tv) == FALSE) {
    plog(XLOG_ERROR, "clnt_control CLSET_RETRY_TIMEOUT for udp failed");
    clnt_destroy(client);
    goto badout;		/* neither tcp not udp succeeded */
  }
  /* udp succeeded */
  dlog("get_mount_client: using udp, port %d", sin.sin_port);
  return client;

badout:
  /* failed */
  if (nc)
    freenetconfigent(nc);
  return NULL;
}


#ifdef NOT_NEEDED_ON_TLI_SYSTEMS
/*
 * find the address of the caller of an RPC procedure.
 */
struct sockaddr_in *
amu_svc_getcaller(SVCXPRT *xprt)
{
  /*
   * On TLI systems we don't use an INET network type, but a "ticlts" (see
   * /etc/netconfig).  This means that packets could only come from the
   * loopback interface, and we don't need to check them and filter possibly
   * spoofed packets.  Therefore we simply return NULL here, and the caller
   * will ignore the result.
   */
  return NULL;			/* tell called to ignore check */
}
#endif /* NOT_NEEDED_ON_TLI_SYSTEMS */


/*
 * Register an RPC server:
 * return 1 on success, 0 otherwise.
 */
int
amu_svc_register(SVCXPRT *xprt, u_long prognum, u_long versnum,
		 void (*dispatch)(struct svc_req *rqstp, SVCXPRT *xprt),
		 u_long protocol, struct netconfig *ncp)
{
  /* on TLI: svc_reg returns 1 on success, 0 otherwise */
  return svc_reg(xprt, prognum, versnum, dispatch, ncp);
}


/*
 * Bind to reserved UDP port, for NFS service only.
 * Port-only version.
 */
static int
bind_resv_port_only_udp(u_short *pp)
{
  int td, rc = -1, port;
  struct t_bind *treq, *tret;
  struct sockaddr_in *sin;
  extern char *t_errlist[];
  extern int t_errno;
  struct netconfig *nc = (struct netconfig *) NULL;
  voidp nc_handle;

  if ((nc_handle = setnetconfig()) == (voidp) NULL) {
    plog(XLOG_ERROR, "Cannot rewind netconfig: %s", nc_sperror());
    return -1;
  }
  /*
   * Search the netconfig table for INET/UDP.
   * This loop will terminate if there was an error in the /etc/netconfig
   * file or if you reached the end of the file without finding the udp
   * device.  Either way your machine has probably far more problems (for
   * example, you cannot have nfs v2 w/o UDP).
   */
  while (1) {
    if ((nc = getnetconfig(nc_handle)) == (struct netconfig *) NULL) {
      plog(XLOG_ERROR, "Error accessing getnetconfig: %s", nc_sperror());
      endnetconfig(nc_handle);
      return -1;
    }
    if (STREQ(nc->nc_protofmly, NC_INET) &&
	STREQ(nc->nc_proto, NC_UDP))
      break;
  }

  /*
   * This is the primary reason for the getnetconfig code above: to get the
   * correct device name to udp, and t_open a descriptor to be used in
   * t_bind below.
   */
  td = t_open(nc->nc_device, O_RDWR, (struct t_info *) NULL);
  endnetconfig(nc_handle);

  if (td < 0) {
    plog(XLOG_ERROR, "t_open failed: %d: %s", t_errno, t_errlist[t_errno]);
    return -1;
  }
  treq = (struct t_bind *) t_alloc(td, T_BIND, T_ADDR);
  if (!treq) {
    plog(XLOG_ERROR, "t_alloc req");
    return -1;
  }
  tret = (struct t_bind *) t_alloc(td, T_BIND, T_ADDR);
  if (!tret) {
    t_free((char *) treq, T_BIND);
    plog(XLOG_ERROR, "t_alloc ret");
    return -1;
  }
  memset((char *) treq->addr.buf, 0, treq->addr.len);
  sin = (struct sockaddr_in *) treq->addr.buf;
  sin->sin_family = AF_INET;
  treq->qlen = 64; /* 0 is ok for udp, for tcp you need qlen>0 */
  treq->addr.len = treq->addr.maxlen;
  errno = EADDRINUSE;

  if (pp && *pp > 0) {
    sin->sin_port = htons(*pp);
    rc = t_bind(td, treq, tret);
  } else {
    port = IPPORT_RESERVED;

    do {
      --port;
      sin->sin_port = htons(port);
      rc = t_bind(td, treq, tret);
      if (rc < 0) {
	plog(XLOG_ERROR, "t_bind for port %d: %s", port, t_errlist[t_errno]);
      } else {
	if (memcmp(treq->addr.buf, tret->addr.buf, tret->addr.len) == 0)
	  break;
	else
	  t_unbind(td);
      }
    } while ((rc < 0 || errno == EADDRINUSE) && (int) port > IPPORT_RESERVED / 2);

    if (pp && rc == 0)
      *pp = port;
  }

  t_free((char *) tret, T_BIND);
  t_free((char *) treq, T_BIND);
  return rc;
}


/*
 * Bind NFS to a reserved port.
 */
static int
bind_nfs_port(int unused_so, u_short *nfs_portp)
{
  u_short port = 0;
  int error = bind_resv_port_only_udp(&port);

  if (error == 0)
    *nfs_portp = port;
  return error;
}


/*
 * Create the nfs service for amd
 * return 0 (TRUE) if OK, 1 (FALSE) if failed.
 */
int
create_nfs_service(int *soNFSp, u_short *nfs_portp, SVCXPRT **nfs_xprtp, void (*dispatch_fxn)(struct svc_req *rqstp, SVCXPRT *transp), u_long nfs_version)
{
  char *nettype = "ticlts";

  nfsncp = getnetconfigent(nettype);
  if (nfsncp == NULL) {
    plog(XLOG_ERROR, "cannot getnetconfigent for %s", nettype);
    /* failed with ticlts, try plain udp (hpux11) */
    nettype = "udp";
    nfsncp = getnetconfigent(nettype);
    if (nfsncp == NULL) {
      plog(XLOG_ERROR, "cannot getnetconfigent for %s", nettype);
      return 1;
    }
  }
  *nfs_xprtp = svc_tli_create(RPC_ANYFD, nfsncp, NULL, 0, 0);
  if (*nfs_xprtp == NULL) {
    plog(XLOG_ERROR, "cannot create nfs tli service for amd");
    return 1;
  }

  /*
   * Get the service file descriptor and check its number to see if
   * the t_open failed.  If it succeeded, then go on to binding to a
   * reserved nfs port.
   */
  *soNFSp = (*nfs_xprtp)->xp_fd;
  if (*soNFSp < 0 || bind_nfs_port(*soNFSp, nfs_portp) < 0) {
    plog(XLOG_ERROR, "Can't create privileged nfs port (TLI)");
    svc_destroy(*nfs_xprtp);
    return 1;
  }
  if (svc_reg(*nfs_xprtp, NFS_PROGRAM, nfs_version, dispatch_fxn, NULL) != 1) {
    plog(XLOG_ERROR, "could not register amd NFS service");
    svc_destroy(*nfs_xprtp);
    return 1;
  }

  return 0;			/* all is well */
}


/*
 * Bind to preferred AMQ port.
 */
static int
bind_preferred_amq_port(u_short pref_port,
			const struct netconfig *ncp,
			struct t_bind **tretpp)
{
  int td = -1, rc = -1;
  struct t_bind *treq;
  struct sockaddr_in *sin, *sin2;
  extern char *t_errlist[];
  extern int t_errno;

  if (!ncp) {
    plog(XLOG_ERROR, "null ncp");
    return -1;
  }

  td = t_open(ncp->nc_device, O_RDWR, (struct t_info *) NULL);
  if (td < 0) {
    plog(XLOG_ERROR, "t_open failed: %d: %s", t_errno, t_errlist[t_errno]);
    return -1;
  }
  treq = (struct t_bind *) t_alloc(td, T_BIND, T_ADDR);
  if (!treq) {
    plog(XLOG_ERROR, "t_alloc req");
    return -1;
  }
  *tretpp = (struct t_bind *) t_alloc(td, T_BIND, T_ADDR);
  if (!*tretpp) {
    t_free((char *) treq, T_BIND);
    plog(XLOG_ERROR, "t_alloc tretpp");
    return -1;
  }
  memset((char *) treq->addr.buf, 0, treq->addr.len);
  sin = (struct sockaddr_in *) treq->addr.buf;
  sin->sin_family = AF_INET;
  treq->qlen = 64; /* must be greater than 0 to work for TCP connections */
  treq->addr.len = treq->addr.maxlen;

  if (pref_port > 0) {
    sin->sin_port = htons(pref_port);
    sin->sin_addr.s_addr = htonl(INADDR_LOOPBACK); /* XXX: may not be needed */
    rc = t_bind(td, treq, *tretpp);
    if (rc < 0) {
      plog(XLOG_ERROR, "t_bind return err %d", rc);
      goto out;
    }
    /* check if we got the port we asked for */
    sin2 = (struct sockaddr_in *) (*tretpp)->addr.buf;
    if (sin->sin_port != sin2->sin_port) {
      plog(XLOG_ERROR, "asked for port %d, got different one (%d)",
	   ntohs(sin->sin_port), ntohs(sin2->sin_port));
      t_errno = TNOADDR; /* XXX: is this correct? */
      rc = -1;
      goto out;
    }
    if (sin->sin_addr.s_addr != sin2->sin_addr.s_addr) {
      plog(XLOG_ERROR, "asked for address %x, got different one (%x)",
	   (int) ntohl(sin->sin_addr.s_addr), (int) ntohl(sin2->sin_addr.s_addr));
      t_errno = TNOADDR; /* XXX: is this correct? */
      rc = -1;
      goto out;
    }
  }
out:
  t_free((char *) treq, T_BIND);
  return (rc < 0 ? rc : td);
}


/*
 * Create the amq service for amd (both TCP and UDP)
 */
int
create_amq_service(int *udp_soAMQp,
		   SVCXPRT **udp_amqpp,
		   struct netconfig **udp_amqncpp,
		   int *tcp_soAMQp,
		   SVCXPRT **tcp_amqpp,
		   struct netconfig **tcp_amqncpp,
		   u_short preferred_amq_port)
{
  /*
   * (partially) create the amq service for amd
   * to be completed further in by caller.
   * XXX: is this "partially" still true?!  See amd/nfs_start.c. -Erez
   */

  /* first create the TCP service */
  if (tcp_amqncpp)
    if ((*tcp_amqncpp = getnetconfigent(NC_TCP)) == NULL) {
      plog(XLOG_ERROR, "cannot getnetconfigent for %s", NC_TCP);
      return 1;
    }

  if (tcp_amqpp) {
    if (preferred_amq_port > 0) {
      struct t_bind *tbp = NULL;
      int sock;

      plog(XLOG_INFO, "requesting preferred amq TCP port %d", preferred_amq_port);
      sock = bind_preferred_amq_port(preferred_amq_port, *tcp_amqncpp, &tbp);
      if (sock < 0) {
	plog(XLOG_ERROR, "bind_preferred_amq_port failed for TCP port %d: %s",
	     preferred_amq_port, t_errlist[t_errno]);
	return 1;
      }
      *tcp_amqpp = svc_tli_create(sock, *tcp_amqncpp, tbp, 0, 0);
      if (*tcp_amqpp != NULL)
	plog(XLOG_INFO, "amq service bound to TCP port %d", preferred_amq_port);
      t_free((char *) tbp, T_BIND);
    } else {
      /* select any port */
      *tcp_amqpp = svc_tli_create(RPC_ANYFD, *tcp_amqncpp, NULL, 0, 0);
    }
    if (*tcp_amqpp == NULL) {
      plog(XLOG_ERROR, "cannot create (tcp) tli service for amq");
      return 1;
    }
  }
  if (tcp_soAMQp && tcp_amqpp)
    *tcp_soAMQp = (*tcp_amqpp)->xp_fd;

  /* next create the UDP service */
  if (udp_amqncpp)
    if ((*udp_amqncpp = getnetconfigent(NC_UDP)) == NULL) {
      plog(XLOG_ERROR, "cannot getnetconfigent for %s", NC_UDP);
      return 1;
    }
  if (udp_amqpp) {
    if (preferred_amq_port > 0) {
      struct t_bind *tbp = NULL;
      int sock;

      plog(XLOG_INFO, "requesting preferred amq UDP port %d", preferred_amq_port);
      sock = bind_preferred_amq_port(preferred_amq_port, *udp_amqncpp, &tbp);
      if (sock < 0) {
	plog(XLOG_ERROR, "bind_preferred_amq_port failed for UDP port %d: %s",
	     preferred_amq_port, t_errlist[t_errno]);
	return 1;
      }
      *udp_amqpp = svc_tli_create(sock, *udp_amqncpp, tbp, 0, 0);
      if (*udp_amqpp != NULL)
	plog(XLOG_INFO, "amq service bound to UDP port %d", preferred_amq_port);
      t_free((char *) tbp, T_BIND);
    } else {
      /* select any port */
      *udp_amqpp = svc_tli_create(RPC_ANYFD, *udp_amqncpp, NULL, 0, 0);
    }
    if (*udp_amqpp == NULL) {
      plog(XLOG_ERROR, "cannot create (udp) tli service for amq");
      return 1;
    }
  }
  if (udp_soAMQp && udp_amqpp)
    *udp_soAMQp = (*udp_amqpp)->xp_fd;

  return 0;			/* all is well */
}


/*
 * Find netconfig info for TCP/UDP device, and fill in the knetconfig
 * structure.  If in_ncp is not NULL, use that instead of defaulting
 * to a TCP/UDP service.  If in_ncp is NULL, then use the service type
 * specified in nc_protoname (which may be either "tcp" or "udp").  If
 * nc_protoname is NULL, default to UDP.
 */
int
get_knetconfig(struct knetconfig **kncpp, struct netconfig *in_ncp, char *nc_protoname)
{
  struct netconfig *ncp = NULL;
  struct stat statbuf;

  if (in_ncp)
    ncp = in_ncp;
  else {
    if (nc_protoname)
      ncp = getnetconfigent(nc_protoname);
    else
      ncp = getnetconfigent(NC_UDP);
  }
  if (!ncp)
    return -2;

  *kncpp = (struct knetconfig *) xzalloc(sizeof(struct knetconfig));
  if (*kncpp == (struct knetconfig *) NULL) {
    if (!in_ncp)
      freenetconfigent(ncp);
    return -3;
  }
  (*kncpp)->knc_semantics = ncp->nc_semantics;
  (*kncpp)->knc_protofmly = xstrdup(ncp->nc_protofmly);
  (*kncpp)->knc_proto = xstrdup(ncp->nc_proto);

  if (stat(ncp->nc_device, &statbuf) < 0) {
    plog(XLOG_ERROR, "could not stat() %s: %m", ncp->nc_device);
    XFREE(*kncpp);
    *kncpp = NULL;
    if (!in_ncp)
      freenetconfigent(ncp);
    return -3;			/* amd will end (free not needed) */
  }
  (*kncpp)->knc_rdev = (dev_t) statbuf.st_rdev;
  if (!in_ncp) {		/* free only if argument not passed */
    freenetconfigent(ncp);
    ncp = NULL;
  }
  return 0;
}


/*
 * Free a previously allocated knetconfig structure.
 */
void
free_knetconfig(struct knetconfig *kncp)
{
  if (kncp) {
    if (kncp->knc_protofmly)
      XFREE(kncp->knc_protofmly);
    if (kncp->knc_proto)
      XFREE(kncp->knc_proto);
    XFREE(kncp);
    kncp = (struct knetconfig *) NULL;
  }
}


/*
 * Check if the portmapper is running and reachable: 0==down, 1==up
 */
int check_pmap_up(char *host, struct sockaddr_in* sin)
{
  CLIENT *client;
  enum clnt_stat clnt_stat = RPC_TIMEDOUT; /* assume failure */
  int socket = RPC_ANYSOCK;
  struct timeval timeout;

  timeout.tv_sec = 2;
  timeout.tv_usec = 0;
  sin->sin_port = htons(PMAPPORT);
  client = clntudp_create(sin, PMAPPROG, PMAPVERS, timeout, &socket);
  if (client == (CLIENT *) NULL) {
    plog(XLOG_ERROR,
	 "check_pmap_up: cannot create connection to contact portmapper on host \"%s\"%s",
	 host, clnt_spcreateerror(""));
    return 0;
  }

  timeout.tv_sec = 6;
  /* Ping the portmapper on a remote system by calling the nullproc */
  clnt_stat = clnt_call(client,
			PMAPPROC_NULL,
			(XDRPROC_T_TYPE) xdr_void,
			NULL,
			(XDRPROC_T_TYPE) xdr_void,
			NULL,
			timeout);
  clnt_destroy(client);
  close(socket);
  sin->sin_port = 0;

  if (clnt_stat == RPC_TIMEDOUT) {
    plog(XLOG_ERROR,
	 "check_pmap_up: failed to contact portmapper on host \"%s\": %s",
	 host, clnt_sperrno(clnt_stat));
    return 0;
  }
  return 1;
}


/*
 * Find the best NFS version for a host.
 */
u_long
get_nfs_version(char *host, struct sockaddr_in *sin, u_long nfs_version, const char *proto, u_long def)
{
  CLIENT *clnt = NULL;
  rpcvers_t versout;
  struct timeval tv;

  /*
   * If not set or set wrong, then try from NFS_VERS_MAX on down. If
   * set, then try from nfs_version on down.
   */
  if (!nfs_valid_version(nfs_version))
    if (nfs_valid_version(def))
      nfs_version = def;
    else
      nfs_version = NFS_VERS_MAX;
  }

  if (nfs_version == NFS_VERSION) {
    dlog("get_nfs_version trying NFS(%d,%s) for %s",
	 (int) nfs_version, proto, host);
  } else {
    dlog("get_nfs_version trying NFS(%d-%d,%s) for %s",
	 (int) NFS_VERSION, (int) nfs_version, proto, host);
  }

  /* 3 seconds is more than enough for a LAN */
  memset(&tv, 0, sizeof(tv));
  tv.tv_sec = 3;
  tv.tv_usec = 0;

#ifdef HAVE_CLNT_CREATE_VERS_TIMED
  clnt = clnt_create_vers_timed(host, NFS_PROGRAM, &versout, NFS_VERSION, nfs_version, proto, &tv);
#else /* not HAVE_CLNT_CREATE_VERS_TIMED */
  clnt = clnt_create_vers(host, NFS_PROGRAM, &versout, NFS_VERSION, nfs_version, proto);
#endif	/* not HAVE_CLNT_CREATE_VERS_TIMED */

  if (clnt == NULL) {
    if (nfs_version == NFS_VERSION)
      plog(XLOG_INFO, "get_nfs_version NFS(%d,%s) failed for %s: %s",
	   (int) nfs_version, proto, host, clnt_spcreateerror(""));
    else
      plog(XLOG_INFO, "get_nfs_version NFS(%d-%d,%s) failed for %s: %s",
	   (int) NFS_VERSION, (int) nfs_version, proto, host, clnt_spcreateerror(""));
    return 0;
  }
  clnt_destroy(clnt);

  return versout;
}


#if defined(HAVE_FS_AUTOFS) && defined(AUTOFS_PROG)
/*
 * find the IP address that can be used to connect autofs service to.
 */
static int
get_autofs_address(struct netconfig *ncp, struct t_bind *tbp)
{
  int ret;
  struct nd_addrlist *addrs = (struct nd_addrlist *) NULL;
  struct nd_hostserv service;

  service.h_host = HOST_SELF_CONNECT;
  service.h_serv = "autofs";

  ret = netdir_getbyname(ncp, &service, &addrs);

  if (ret) {
    plog(XLOG_FATAL, "get_autofs_address: cannot get local host address: %s", netdir_sperror());
    goto out;
  }

  /*
   * XXX: there may be more more than one address for this local
   * host.  Maybe something can be done with those.
   */
  tbp->addr.len = addrs->n_addrs->len;
  tbp->addr.maxlen = addrs->n_addrs->len;
  memcpy(tbp->addr.buf, addrs->n_addrs->buf, addrs->n_addrs->len);
 /*
  * qlen should not be zero for TCP connections.  It's not clear what it
  * should be for UDP connections, but setting it to something like 64 seems
  * to be the safe value that works.
  */
  tbp->qlen = 64;

  /* all OK */
  netdir_free((voidp) addrs, ND_ADDRLIST);

out:
  return ret;
}


/*
 * Register the autofs service for amd
 */
int
register_autofs_service(char *autofs_conftype,
			void (*autofs_dispatch)(struct svc_req *rqstp, SVCXPRT *xprt))
{
  struct t_bind *tbp = NULL;
  struct netconfig *autofs_ncp;
  SVCXPRT *autofs_xprt = NULL;
  int fd = -1, err = 1;		/* assume failed */

  plog(XLOG_INFO, "registering autofs service: %s", autofs_conftype);
  autofs_ncp = getnetconfigent(autofs_conftype);
  if (autofs_ncp == NULL) {
    plog(XLOG_ERROR, "register_autofs_service: cannot getnetconfigent for %s", autofs_conftype);
    goto out;
  }

  fd = t_open(autofs_ncp->nc_device, O_RDWR, NULL);
  if (fd < 0) {
    plog(XLOG_ERROR, "register_autofs_service: t_open failed (%s)",
	 t_errlist[t_errno]);
    goto out;
  }

  tbp = (struct t_bind *) t_alloc(fd, T_BIND, T_ADDR);
  if (!tbp) {
    plog(XLOG_ERROR, "register_autofs_service: t_alloc failed");
    goto out;
  }

  if (get_autofs_address(autofs_ncp, tbp) != 0) {
    plog(XLOG_ERROR, "register_autofs_service: get_autofs_address failed");
    goto out;
  }

  autofs_xprt = svc_tli_create(fd, autofs_ncp, tbp, 0, 0);
  if (autofs_xprt == NULL) {
    plog(XLOG_ERROR, "cannot create autofs tli service for amd");
    goto out;
  }

  rpcb_unset(AUTOFS_PROG, AUTOFS_VERS, autofs_ncp);
  if (svc_reg(autofs_xprt, AUTOFS_PROG, AUTOFS_VERS, autofs_dispatch, autofs_ncp) == FALSE) {
    plog(XLOG_ERROR, "could not register amd AUTOFS service");
    goto out;
  }
  err = 0;
  goto really_out;

out:
  if (autofs_ncp)
    freenetconfigent(autofs_ncp);
  if (autofs_xprt)
    SVC_DESTROY(autofs_xprt);
  else {
    if (fd > 0)
      t_close(fd);
  }

really_out:
  if (tbp)
    t_free((char *) tbp, T_BIND);

  dlog("register_autofs_service: returning %d\n", err);
  return err;
}


int
unregister_autofs_service(char *autofs_conftype)
{
  struct netconfig *autofs_ncp;
  int err = 1;

  plog(XLOG_INFO, "unregistering autofs service listener: %s", autofs_conftype);

  autofs_ncp = getnetconfigent(autofs_conftype);
  if (autofs_ncp == NULL) {
    plog(XLOG_ERROR, "destroy_autofs_service: cannot getnetconfigent for %s", autofs_conftype);
    goto out;
  }

out:
  rpcb_unset(AUTOFS_PROG, AUTOFS_VERS, autofs_ncp);
  return err;
}
#endif /* HAVE_FS_AUTOFS && AUTOFS_PROG */
