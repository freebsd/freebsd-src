/*
 * Copyright (c) 1997-2003 Erez Zadok
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgment:
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *      %W% (Berkeley) %G%
 *
 * $Id: srvr_nfs.c,v 1.7.2.10 2002/12/29 01:55:43 ib42 Exp $
 * $FreeBSD$
 *
 */

/*
 * NFS server modeling
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */
#include <am_defs.h>
#include <amd.h>

/*
 * Number of pings allowed to fail before host is declared down
 * - three-fifths of the allowed mount time...
 */
#define	MAX_ALLOWED_PINGS	(3 + /* for luck ... */ 1)

/*
 * How often to ping when starting a new server
 */
#define	FAST_NFS_PING		3

#if (FAST_NFS_PING * MAX_ALLOWED_PINGS) >= ALLOWED_MOUNT_TIME
# error: sanity check failed in srvr_nfs.c
/*
 * you cannot do things this way...
 * sufficient fast pings must be given the chance to fail
 * within the allowed mount time
 */
#endif /* (FAST_NFS_PING * MAX_ALLOWED_PINGS) >= ALLOWED_MOUNT_TIME */

#define	NPXID_ALLOC(struct )	(++np_xid)

/* structures and typedefs */
typedef struct nfs_private {
  u_short np_mountd;		/* Mount daemon port number */
  char np_mountd_inval;		/* Port *may* be invalid */
  int np_ping;			/* Number of failed ping attempts */
  time_t np_ttl;		/* Time when server is thought dead */
  int np_xid;			/* RPC transaction id for pings */
  int np_error;			/* Error during portmap request */
} nfs_private;

/* globals */
qelem nfs_srvr_list = {&nfs_srvr_list, &nfs_srvr_list};

/* statics */
static int np_xid;		/* For NFS pings */
static int ping_len;
static char ping_buf[sizeof(struct rpc_msg) + 32];

#if defined(MNTTAB_OPT_PROTO) || defined(HAVE_FS_NFS3)
/*
 * Protocols we know about, in order of preference.
 *
 * Note that Solaris 8 and newer NetBSD systems are switching to UDP first,
 * so this order may have to be adjusted for Amd in the future once more
 * vendors make that change. -Erez 11/24/2000
 */
static char *protocols[] = { "tcp", "udp", NULL };
#endif /* defined(MNTTAB_OPT_PROTO) || defined(HAVE_FS_NFS3) */

/* forward definitions */
static void nfs_keepalive(voidp);



/*
 * Flush any cached data
 */
void
flush_srvr_nfs_cache(void)
{
  fserver *fs = 0;

  ITER(fs, fserver, &nfs_srvr_list) {
    nfs_private *np = (nfs_private *) fs->fs_private;
    if (np) {
      np->np_mountd_inval = TRUE;
      np->np_error = -1;
    }
  }
}


/*
 * Startup the NFS ping for a particular version.
 */
static void
start_ping(u_long nfs_version)
{
  XDR ping_xdr;
  struct rpc_msg ping_msg;

  /*
   * Non nfs mounts like /afs/glue.umd.edu have ended up here.
   */
  if (nfs_version == 0) {
    nfs_version = NFS_VERSION;
    plog(XLOG_WARNING, "start_ping: nfs_version = 0 fixed");
  }
  plog(XLOG_INFO, "start_ping: nfs_version: %d", (int) nfs_version);

  rpc_msg_init(&ping_msg, NFS_PROGRAM, nfs_version, NFSPROC_NULL);

  /*
   * Create an XDR endpoint
   */
  xdrmem_create(&ping_xdr, ping_buf, sizeof(ping_buf), XDR_ENCODE);

  /*
   * Create the NFS ping message
   */
  if (!xdr_callmsg(&ping_xdr, &ping_msg)) {
    plog(XLOG_ERROR, "Couldn't create ping RPC message");
    going_down(3);
  }
  /*
   * Find out how long it is
   */
  ping_len = xdr_getpos(&ping_xdr);

  /*
   * Destroy the XDR endpoint - we don't need it anymore
   */
  xdr_destroy(&ping_xdr);
}


/*
 * Called when a portmap reply arrives
 */
static void
got_portmap(voidp pkt, int len, struct sockaddr_in *sa, struct sockaddr_in *ia, voidp idv, int done)
{
  fserver *fs2 = (fserver *) idv;
  fserver *fs = 0;

  /*
   * Find which fileserver we are talking about
   */
  ITER(fs, fserver, &nfs_srvr_list)
  if (fs == fs2)
      break;

  if (fs == fs2) {
    u_long port = 0;	/* XXX - should be short but protocol is naff */
    int error = done ? pickup_rpc_reply(pkt, len, (voidp) &port, (XDRPROC_T_TYPE) xdr_u_long) : -1;
    nfs_private *np = (nfs_private *) fs->fs_private;

    if (!error && port) {
#ifdef DEBUG
      dlog("got port (%d) for mountd on %s", (int) port, fs->fs_host);
#endif /* DEBUG */
      /*
       * Grab the port number.  Portmap sends back
       * an u_long in native ordering, so it
       * needs converting to a u_short in
       * network ordering.
       */
      np->np_mountd = htons((u_short) port);
      np->np_mountd_inval = FALSE;
      np->np_error = 0;
    } else {
#ifdef DEBUG
      dlog("Error fetching port for mountd on %s", fs->fs_host);
      dlog("\t error=%d, port=%d", error, (int) port);
#endif /* DEBUG */
      /*
       * Almost certainly no mountd running on remote host
       */
      np->np_error = error ? error : ETIMEDOUT;
    }

    if (fs->fs_flags & FSF_WANT)
      wakeup_srvr(fs);
  } else if (done) {
#ifdef DEBUG
    dlog("Got portmap for old port request");
#endif /* DEBUG */
  } else {
#ifdef DEBUG
    dlog("portmap request timed out");
#endif /* DEBUG */
  }
}


/*
 * Obtain portmap information
 */
static int
call_portmap(fserver *fs, AUTH *auth, u_long prog, u_long vers, u_long prot)
{
  struct rpc_msg pmap_msg;
  int len;
  char iobuf[UDPMSGSIZE];
  int error;
  struct pmap pmap;

  rpc_msg_init(&pmap_msg, PMAPPROG, PMAPVERS, PMAPPROC_NULL);
  pmap.pm_prog = prog;
  pmap.pm_vers = vers;
  pmap.pm_prot = prot;
  pmap.pm_port = 0;
  len = make_rpc_packet(iobuf,
			sizeof(iobuf),
			PMAPPROC_GETPORT,
			&pmap_msg,
			(voidp) &pmap,
			(XDRPROC_T_TYPE) xdr_pmap,
			auth);
  if (len > 0) {
    struct sockaddr_in sin;
    memset((voidp) &sin, 0, sizeof(sin));
    sin = *fs->fs_ip;
    sin.sin_port = htons(PMAPPORT);
    error = fwd_packet(RPC_XID_PORTMAP, (voidp) iobuf, len,
		       &sin, &sin, (voidp) fs, got_portmap);
  } else {
    error = -len;
  }

  return error;
}


static void
recompute_portmap(fserver *fs)
{
  int error;
  u_long mnt_version;

  if (nfs_auth)
    error = 0;
  else
    error = make_nfs_auth();

  if (error) {
    nfs_private *np = (nfs_private *) fs->fs_private;
    np->np_error = error;
    return;
  }

  if (fs->fs_version == 0)
    plog(XLOG_WARNING, "recompute_portmap: nfs_version = 0 fixed");

  plog(XLOG_INFO, "recompute_portmap: NFS version %d", (int) fs->fs_version);
#ifdef HAVE_FS_NFS3
  if (fs->fs_version == NFS_VERSION3)
    mnt_version = MOUNTVERS3;
  else
#endif /* HAVE_FS_NFS3 */
    mnt_version = MOUNTVERS;

  plog(XLOG_INFO, "Using MOUNT version: %d", (int) mnt_version);
  call_portmap(fs, nfs_auth, MOUNTPROG, mnt_version, (u_long) IPPROTO_UDP);
}


/*
 * This is called when we get a reply to an RPC ping.
 * The value of id was taken from the nfs_private
 * structure when the ping was transmitted.
 */
static void
nfs_pinged(voidp pkt, int len, struct sockaddr_in *sp, struct sockaddr_in *tsp, voidp idv, int done)
{
  int xid = (long) idv;		/* for 64-bit archs */
  fserver *fs;
#ifdef DEBUG
  int found_map = 0;
#endif /* DEBUG */

  if (!done)
    return;

  /*
   * For each node...
   */
  ITER(fs, fserver, &nfs_srvr_list) {
    nfs_private *np = (nfs_private *) fs->fs_private;
    if (np->np_xid == xid && (fs->fs_flags & FSF_PINGING)) {
      /*
       * Reset the ping counter.
       * Update the keepalive timer.
       * Log what happened.
       */
      if (fs->fs_flags & FSF_DOWN) {
	fs->fs_flags &= ~FSF_DOWN;
	if (fs->fs_flags & FSF_VALID) {
	  srvrlog(fs, "is up");
	} else {
	  if (np->np_ping > 1)
	    srvrlog(fs, "ok");
#ifdef DEBUG
	  else
	    srvrlog(fs, "starts up");
#endif /* DEBUG */
	  fs->fs_flags |= FSF_VALID;
	}

	map_flush_srvr(fs);
      } else {
	if (fs->fs_flags & FSF_VALID) {
#ifdef DEBUG
	  dlog("file server %s type nfs is still up", fs->fs_host);
#endif /* DEBUG */
	} else {
	  if (np->np_ping > 1)
	    srvrlog(fs, "ok");
	  fs->fs_flags |= FSF_VALID;
	}
      }

      /*
       * Adjust ping interval
       */
      untimeout(fs->fs_cid);
      fs->fs_cid = timeout(fs->fs_pinger, nfs_keepalive, (voidp) fs);

      /*
       * Update ttl for this server
       */
      np->np_ttl = clocktime() +
	(MAX_ALLOWED_PINGS - 1) * FAST_NFS_PING + fs->fs_pinger - 1;

      /*
       * New RPC xid...
       */
      np->np_xid = NPXID_ALLOC(struct );

      /*
       * Failed pings is zero...
       */
      np->np_ping = 0;

      /*
       * Recompute portmap information if not known
       */
      if (np->np_mountd_inval)
	recompute_portmap(fs);

#ifdef DEBUG
      found_map++;
#endif /* DEBUG */
      break;
    }
  }

#ifdef DEBUG
  if (found_map == 0)
    dlog("Spurious ping packet");
#endif /* DEBUG */
}


/*
 * Called when no ping-reply received
 */
static void
nfs_timed_out(voidp v)
{
  fserver *fs = v;
  nfs_private *np = (nfs_private *) fs->fs_private;

  /*
   * Another ping has failed
   */
  np->np_ping++;
  if (np->np_ping > 1)
    srvrlog(fs, "not responding");

  /*
   * Not known to be up any longer
   */
  if (FSRV_ISUP(fs))
    fs->fs_flags &= ~FSF_VALID;

  /*
   * If ttl has expired then guess that it is dead
   */
  if (np->np_ttl < clocktime()) {
    int oflags = fs->fs_flags;
#ifdef DEBUG
    dlog("ttl has expired");
#endif /* DEBUG */
    if ((fs->fs_flags & FSF_DOWN) == 0) {
      /*
       * Server was up, but is now down.
       */
      srvrlog(fs, "is down");
      fs->fs_flags |= FSF_DOWN | FSF_VALID;
      /*
       * Since the server is down, the portmap
       * information may now be wrong, so it
       * must be flushed from the local cache
       */
      flush_nfs_fhandle_cache(fs);
      np->np_error = -1;
    } else {
      /*
       * Known to be down
       */
#ifdef DEBUG
      if ((fs->fs_flags & FSF_VALID) == 0)
	srvrlog(fs, "starts down");
#endif /* DEBUG */
      fs->fs_flags |= FSF_VALID;
    }
    if (oflags != fs->fs_flags && (fs->fs_flags & FSF_WANT))
      wakeup_srvr(fs);
    /*
     * Reset failed ping count
     */
    np->np_ping = 0;
  } else {
#ifdef DEBUG
    if (np->np_ping > 1)
      dlog("%d pings to %s failed - at most %d allowed", np->np_ping, fs->fs_host, MAX_ALLOWED_PINGS);
#endif /* DEBUG */
  }

  /*
   * New RPC xid, so any late responses to the previous ping
   * get ignored...
   */
  np->np_xid = NPXID_ALLOC(struct );

  /*
   * Run keepalive again
   */
  nfs_keepalive(fs);
}


/*
 * Keep track of whether a server is alive
 */
static void
nfs_keepalive(voidp v)
{
  fserver *fs = v;
  int error;
  nfs_private *np = (nfs_private *) fs->fs_private;
  int fstimeo = -1;

  /*
   * Send an NFS ping to this node
   */

  if (ping_len == 0)
    start_ping(fs->fs_version);

  /*
   * Queue the packet...
   */
  error = fwd_packet(MK_RPC_XID(RPC_XID_NFSPING, np->np_xid),
		     (voidp) ping_buf,
		     ping_len,
		     fs->fs_ip,
		     (struct sockaddr_in *) 0,
		     (voidp) ((long) np->np_xid), /* for 64-bit archs */
		     nfs_pinged);

  /*
   * See if a hard error occurred
   */
  switch (error) {
  case ENETDOWN:
  case ENETUNREACH:
  case EHOSTDOWN:
  case EHOSTUNREACH:
    np->np_ping = MAX_ALLOWED_PINGS;	/* immediately down */
    np->np_ttl = (time_t) 0;
    /*
     * This causes an immediate call to nfs_timed_out
     * whenever the server was thought to be up.
     * See +++ below.
     */
    fstimeo = 0;
    break;

  case 0:
#ifdef DEBUG
    dlog("Sent NFS ping to %s", fs->fs_host);
#endif /* DEBUG */
    break;
  }

  /*
   * Back off the ping interval if we are not getting replies and
   * the remote system is know to be down.
   */
  switch (fs->fs_flags & (FSF_DOWN | FSF_VALID)) {
  case FSF_VALID:		/* Up */
    if (fstimeo < 0)		/* +++ see above */
      fstimeo = FAST_NFS_PING;
    break;

  case FSF_VALID | FSF_DOWN:	/* Down */
    fstimeo = fs->fs_pinger;
    break;

  default:			/* Unknown */
    fstimeo = FAST_NFS_PING;
    break;
  }

#ifdef DEBUG
  dlog("NFS timeout in %d seconds", fstimeo);
#endif /* DEBUG */

  fs->fs_cid = timeout(fstimeo, nfs_timed_out, (voidp) fs);
}


int
nfs_srvr_port(fserver *fs, u_short *port, voidp wchan)
{
  int error = -1;
  if ((fs->fs_flags & FSF_VALID) == FSF_VALID) {
    if ((fs->fs_flags & FSF_DOWN) == 0) {
      nfs_private *np = (nfs_private *) fs->fs_private;
      if (np->np_error == 0) {
	*port = np->np_mountd;
	error = 0;
      } else {
	error = np->np_error;
      }
      /*
       * Now go get the port mapping again in case it changed.
       * Note that it is used even if (np_mountd_inval)
       * is True.  The flag is used simply as an
       * indication that the mountd may be invalid, not
       * that it is known to be invalid.
       */
      if (np->np_mountd_inval)
	recompute_portmap(fs);
      else
	np->np_mountd_inval = TRUE;
    } else {
      error = EWOULDBLOCK;
    }
  }
  if (error < 0 && wchan && !(fs->fs_flags & FSF_WANT)) {
    /*
     * If a wait channel is supplied, and no
     * error has yet occurred, then arrange
     * that a wakeup is done on the wait channel,
     * whenever a wakeup is done on this fs node.
     * Wakeup's are done on the fs node whenever
     * it changes state - thus causing control to
     * come back here and new, better things to happen.
     */
    fs->fs_flags |= FSF_WANT;
    sched_task(wakeup_task, wchan, (voidp) fs);
  }
  return error;
}


static void
start_nfs_pings(fserver *fs, int pingval)
{
  if (fs->fs_flags & FSF_PINGING) {
#ifdef DEBUG
    dlog("Already running pings to %s", fs->fs_host);
#endif /* DEBUG */
    return;
  }

  if (fs->fs_cid)
    untimeout(fs->fs_cid);
  if (pingval < 0) {
    srvrlog(fs, "wired up");
    fs->fs_flags |= FSF_VALID;
    fs->fs_flags &= ~FSF_DOWN;
  } else {
    fs->fs_flags |= FSF_PINGING;
    nfs_keepalive(fs);
  }
}


/*
 * Find an nfs server for a host.
 */
fserver *
find_nfs_srvr(mntfs *mf)
{
  char *host = mf->mf_fo->opt_rhost;
  char *nfs_proto = NULL;
  fserver *fs;
  int pingval;
  mntent_t mnt;
  nfs_private *np;
  struct hostent *hp = 0;
  struct sockaddr_in *ip;
  u_long nfs_version = 0;	/* default is no version specified */
#ifdef MNTTAB_OPT_PROTO
  char *rfsname = mf->mf_fo->opt_rfs;
#endif /* MNTTAB_OPT_PROTO */

  /*
   * Get ping interval from mount options.
   * Current only used to decide whether pings
   * are required or not.  < 0 = no pings.
   */
  mnt.mnt_opts = mf->mf_mopts;
  pingval = hasmntval(&mnt, "ping");

  /*
   * Get the NFS version from the mount options. This is used
   * to decide the highest NFS version to try.
   */
#ifdef MNTTAB_OPT_VERS
  nfs_version = hasmntval(&mnt, MNTTAB_OPT_VERS);
#endif /* MNTTAB_OPT_VERS */

#ifdef MNTTAB_OPT_PROTO
  {
    char *proto_opt = hasmnteq(&mnt, MNTTAB_OPT_PROTO);
    if (proto_opt) {
      char **p;
      for (p = protocols; *p; p ++)
	if (NSTREQ(proto_opt, *p, strlen(*p))) {
	  nfs_proto = *p;
	  break;
	}
      if (*p == NULL)
	plog(XLOG_WARNING, "ignoring unknown protocol option for %s:%s",
	     host, rfsname);
    }
  }
#endif /* MNTTAB_OPT_PROTO */

#ifdef HAVE_NFS_NFSV2_H
  /* allow overriding if nfsv2 option is specified in mount options */
  if (hasmntopt(&mnt, "nfsv2")) {
    nfs_version = (u_long) 2;	/* nullify any ``vers=X'' statements */
    nfs_proto = "udp";		/* nullify any ``proto=tcp'' statements */
    plog(XLOG_WARNING, "found compatiblity option \"nfsv2\": set options vers=2,proto=udp for host %s", host);
  }
#endif /* HAVE_NFS_NFSV2_H */

  /* check if we globally overridden the NFS version/protocol */
  if (gopt.nfs_vers) {
    nfs_version = gopt.nfs_vers;
    plog(XLOG_INFO, "find_nfs_srvr: force NFS version to %d",
	 (int) nfs_version);
  }
  if (gopt.nfs_proto) {
    nfs_proto = gopt.nfs_proto;
    plog(XLOG_INFO, "find_nfs_srvr: force NFS protocol transport to %s", nfs_proto);
  }

  /*
   * lookup host address and canonical name
   */
  hp = gethostbyname(host);

  /*
   * New code from Bob Harris <harris@basil-rathbone.mit.edu>
   * Use canonical name to keep track of file server
   * information.  This way aliases do not generate
   * multiple NFS pingers.  (Except when we're normalizing
   * hosts.)
   */
  if (hp && !(gopt.flags & CFM_NORMALIZE_HOSTNAMES))
    host = (char *) hp->h_name;

  if (hp) {
    switch (hp->h_addrtype) {
    case AF_INET:
      ip = ALLOC(struct sockaddr_in);
      memset((voidp) ip, 0, sizeof(*ip));
      ip->sin_family = AF_INET;
      memmove((voidp) &ip->sin_addr, (voidp) hp->h_addr, sizeof(ip->sin_addr));

      ip->sin_port = htons(NFS_PORT);
      break;

    default:
      ip = 0;
      break;
    }
  } else {
    plog(XLOG_USER, "Unknown host: %s", host);
    ip = 0;
  }

  /*
   * Get the NFS Version, and verify server is up. Probably no
   * longer need to start server down below.
   */
  if (ip) {
#ifdef HAVE_FS_NFS3
    /*
     * Find the best combination of NFS version and protocol.
     * When given a choice, use the highest available version,
     * and use TCP over UDP if available.
     */
    if (nfs_proto)
      nfs_version = get_nfs_version(host, ip, nfs_version, nfs_proto);
    else {
      int best_nfs_version = 0;
      int proto_nfs_version;
      char **p;

      for (p = protocols; *p; p++) {
	proto_nfs_version = get_nfs_version(host, ip, nfs_version, *p);

	if (proto_nfs_version > best_nfs_version) {
	  best_nfs_version = proto_nfs_version;
	  nfs_proto = *p;
	}
      }
      nfs_version = best_nfs_version;
    }

    if (!nfs_version) {
      /*
       * If the NFS server is down or does not support the portmapper call
       * (such as certain Novell NFS servers) we mark it as version 2 and we
       * let the nfs code deal with the case that is down.  If when the
       * server comes back up, it can support NFS V.3 and/or TCP, it will
       * use those.
       */
      nfs_version = NFS_VERSION;
      nfs_proto = "udp";
    }
#else /* not HAVE_FS_NFS3 */
    nfs_version = NFS_VERSION;
#endif /* not HAVE_FS_NFS3 */
  }

  if (!nfs_proto)
    nfs_proto = "udp";

  plog(XLOG_INFO, "Using NFS version %d, protocol %s on host %s",
       (int) nfs_version, nfs_proto, host);

  /*
   * Try to find an existing fs server structure for this host.
   * Note that differing versions or protocols have their own structures.
   * XXX: Need to fix the ping mechanism to actually use the NFS protocol
   * chosen here (right now it always uses datagram sockets).
   */
  ITER(fs, fserver, &nfs_srvr_list) {
    if (STREQ(host, fs->fs_host) &&
 	nfs_version == fs->fs_version &&
	STREQ(nfs_proto, fs->fs_proto)) {
      /*
       * following if statement from Mike Mitchell
       * <mcm@unx.sas.com>
       * Initialize the ping data if we aren't pinging
       * now.  The np_ttl and np_ping fields are
       * especially important.
       */
      if (!(fs->fs_flags & FSF_PINGING)) {
	np = (nfs_private *) fs->fs_private;
	np->np_mountd_inval = TRUE;
	np->np_xid = NPXID_ALLOC(struct );
	np->np_error = -1;
	np->np_ping = 0;
	/*
	 * Initially the server will be deemed dead
	 * after MAX_ALLOWED_PINGS of the fast variety
	 * have failed.
	 */
	np->np_ttl = MAX_ALLOWED_PINGS * FAST_NFS_PING + clocktime() - 1;
      }
      /*
       * fill in the IP address -- this is only needed
       * if there is a chance an IP address will change
       * between mounts.
       * Mike Mitchell, mcm@unx.sas.com, 09/08/93
       */
      if (hp && fs->fs_ip)
	memmove((voidp) &fs->fs_ip->sin_addr, (voidp) hp->h_addr, sizeof(fs->fs_ip->sin_addr));

      start_nfs_pings(fs, pingval);
      fs->fs_refc++;
      if (ip)
	XFREE(ip);
      return fs;
    }
  }

  /*
   * Get here if we can't find an entry
   */

  /*
   * Allocate a new server
   */
  fs = ALLOC(struct fserver);
  fs->fs_refc = 1;
  fs->fs_host = strdup(hp ? hp->h_name : "unknown_hostname");
  if (gopt.flags & CFM_NORMALIZE_HOSTNAMES)
    host_normalize(&fs->fs_host);
  fs->fs_ip = ip;
  fs->fs_cid = 0;
  if (ip) {
    fs->fs_flags = FSF_DOWN;	/* Starts off down */
  } else {
    fs->fs_flags = FSF_ERROR | FSF_VALID;
    mf->mf_flags |= MFF_ERROR;
    mf->mf_error = ENOENT;
  }
  fs->fs_version = nfs_version;
  fs->fs_proto = nfs_proto;
  fs->fs_type = MNTTAB_TYPE_NFS;
  fs->fs_pinger = AM_PINGER;
  np = ALLOC(struct nfs_private);
  memset((voidp) np, 0, sizeof(*np));
  np->np_mountd_inval = TRUE;
  np->np_xid = NPXID_ALLOC(struct );
  np->np_error = -1;

  /*
   * Initially the server will be deemed dead after
   * MAX_ALLOWED_PINGS of the fast variety have failed.
   */
  np->np_ttl = clocktime() + MAX_ALLOWED_PINGS * FAST_NFS_PING - 1;
  fs->fs_private = (voidp) np;
  fs->fs_prfree = (void (*)(voidp)) free;

  if (!(fs->fs_flags & FSF_ERROR)) {
    /*
     * Start of keepalive timer
     */
    start_nfs_pings(fs, pingval);
  }

  /*
   * Add to list of servers
   */
  ins_que(&fs->fs_q, &nfs_srvr_list);

  return fs;
}
