/*
 * Copyright (c) 1997-1999 Erez Zadok
 * Copyright (c) 1989 Jan-Simon Pendry
 * Copyright (c) 1989 Imperial College of Science, Technology & Medicine
 * Copyright (c) 1989 The Regents of the University of California.
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
 * $Id: rpc_fwd.c,v 1.2 1999/01/10 21:53:52 ezk Exp $
 *
 */

/*
 * RPC packet forwarding
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */
#include <am_defs.h>
#include <amd.h>

/*
 * Note that the ID field in the external packet is only
 * ever treated as a 32 bit opaque data object, so there
 * is no need to convert to and from network byte ordering.
 */

#define	XID_ALLOC(struct )	(xid++)
#define	MAX_PACKET_SIZE	8192	/* Maximum UDP packet size */

/*
 * Each pending reply has an rpc_forward structure
 * associated with it.  These have a 15 second lifespan.
 * If a new structure is required, then an expired
 * one will be re-allocated if available, otherwise a fresh
 * one is allocated.  Whenever a reply is received the
 * structure is discarded.
 */
typedef struct rpc_forward rpc_forward;
struct rpc_forward {
  qelem rf_q;			/* Linked list */
  time_t rf_ttl;		/* Time to live */
  u_int rf_xid;			/* Packet id */
  u_int rf_oldid;		/* Original packet id */
  fwd_fun rf_fwd;		/* Forwarding function */
  voidp rf_ptr;
  struct sockaddr_in rf_sin;
};

/*
 * Head of list of pending replies
 */
qelem rpc_head = {&rpc_head, &rpc_head};
int fwd_sock;
static u_int xid;


/*
 * Allocate a rely structure
 */
static rpc_forward *
fwd_alloc(void)
{
  time_t now = clocktime();
  rpc_forward *p = 0, *p2;

  /*
   * First search for an existing expired one.
   */
  ITER(p2, rpc_forward, &rpc_head) {
    if (p2->rf_ttl <= now) {
      p = p2;
      break;
    }
  }

  /*
   * If one couldn't be found then allocate
   * a new structure and link it at the
   * head of the list.
   */
  if (p) {
    /*
     * Call forwarding function to say that
     * this message was junked.
     */
#ifdef DEBUG
    dlog("Re-using packet forwarding slot - id %#x", p->rf_xid);
#endif /* DEBUG */
    if (p->rf_fwd)
      (*p->rf_fwd) (0, 0, 0, &p->rf_sin, p->rf_ptr, FALSE);
    rem_que(&p->rf_q);
  } else {
    p = ALLOC(struct rpc_forward);
  }
  ins_que(&p->rf_q, &rpc_head);

  /*
   * Set the time to live field
   * Timeout in 43 seconds
   */
  p->rf_ttl = now + 43;

  return p;
}


/*
 * Free an allocated reply structure.
 * First unlink it from the list, then
 * discard it.
 */
static void
fwd_free(rpc_forward *p)
{
  rem_que(&p->rf_q);
  XFREE(p);
}


/*
 * Initialize the RPC forwarder
 */
int
fwd_init(void)
{
#ifdef FIONBIO
  int on = 1;
#endif /* FIONBIO */

#ifdef HAVE_TRANSPORT_TYPE_TLI
  /*
   * Create ping TLI socket (/dev/tcp and /dev/ticlts did not work)
   * (HPUX-11 does not like using O_NDELAY in flags)
   */
  fwd_sock = t_open("/dev/udp", O_RDWR|O_NONBLOCK, 0);
  if (fwd_sock < 0) {
    plog(XLOG_ERROR, "unable to create RPC forwarding TLI socket: %s",
	 t_errlist[t_errno]);
    return errno;
  }
#else /* not HAVE_TRANSPORT_TYPE_TLI */
  /*
   * Create ping socket
   */
  fwd_sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (fwd_sock < 0) {
    plog(XLOG_ERROR, "unable to create RPC forwarding socket: %m");
    return errno;
  }
#endif /* not HAVE_TRANSPORT_TYPE_TLI */

  /*
   * Some things we talk to require a priv port - so make one here
   */
  if (bind_resv_port(fwd_sock, (u_short *) 0) < 0)
    plog(XLOG_ERROR, "can't bind privileged port");

  if (fcntl(fwd_sock, F_SETFL, FNDELAY) < 0
#ifdef FIONBIO
      && ioctl(fwd_sock, FIONBIO, &on) < 0
#endif /* FIONBIO */
    ) {
    plog(XLOG_ERROR, "Can't set non-block on forwarding socket: %m");
    return errno;
  }

  return 0;
}


/*
 * Locate a packet in the forwarding list
 */
static rpc_forward *
fwd_locate(u_int id)
{
  rpc_forward *p;

  ITER(p, rpc_forward, &rpc_head) {
    if (p->rf_xid == id)
      return p;
  }

  return 0;
}


/*
 * This is called to forward a packet to another
 * RPC server.  The message id is changed and noted
 * so that when a reply appears we can tie it up
 * correctly.  Just matching the reply's source address
 * would not work because it might come from a
 * different address.
 */
int
fwd_packet(int type_id, voidp pkt, int len, struct sockaddr_in *fwdto, struct sockaddr_in *replyto, voidp i, fwd_fun cb)
{
  rpc_forward *p;
  u_int *pkt_int;
  int error;
#ifdef HAVE_TRANSPORT_TYPE_TLI
  struct t_unitdata ud;
#endif /* HAVE_TRANSPORT_TYPE_TLI */

  if ((int) amd_state >= (int) Finishing)
    return ENOENT;

  /*
   * See if the type_id is fully specified.
   * If so, then discard any old entries
   * for this id.
   * Otherwise make sure the type_id is
   * fully qualified by allocating an id here.
   */
#ifdef DEBUG
  switch (type_id & RPC_XID_MASK) {
  case RPC_XID_PORTMAP:
    dlog("Sending PORTMAP request");
    break;
  case RPC_XID_MOUNTD:
    dlog("Sending MOUNTD request %#x", type_id);
    break;
  case RPC_XID_NFSPING:
    dlog("Sending NFS ping");
    break;
  default:
    dlog("UNKNOWN RPC XID");
    break;
  }
#endif /* DEBUG */

  if (type_id & ~RPC_XID_MASK) {
    p = fwd_locate(type_id);
    if (p) {
#ifdef DEBUG
      dlog("Discarding earlier rpc fwd handle");
#endif /* DEBUG */
      fwd_free(p);
    }
  } else {
#ifdef DEBUG
    dlog("Allocating a new xid...");
#endif /* DEBUG */
    type_id = MK_RPC_XID(type_id, XID_ALLOC(struct ));
  }

  p = fwd_alloc();
  if (!p)
    return ENOBUFS;

  error = 0;

  pkt_int = (u_int *) pkt;

  /*
   * Get the original packet id
   */
  p->rf_oldid = *pkt_int;

  /*
   * Replace with newly allocated id
   */
  p->rf_xid = *pkt_int = type_id;

  /*
   * The sendto may fail if, for example, the route
   * to a remote host is lost because an intermediate
   * gateway has gone down.  Important to fill in the
   * rest of "p" otherwise nasty things happen later...
   */
#ifdef DEBUG
  {
    char dq[20];
    if (p && fwdto)
      dlog("Sending packet id %#x to %s.%d",
	   p->rf_xid,
	   inet_dquad(dq, fwdto->sin_addr.s_addr),
	   ntohs(fwdto->sin_port));
  }
#endif /* DEBUG */

  /* if NULL, remote server probably down */
  if (!fwdto) {
    error = AM_ERRNO_HOST_DOWN;
    goto out;
  }

#ifdef HAVE_TRANSPORT_TYPE_TLI
  ud.addr.buf = (char *) fwdto;
  if (fwdto)			/* if NULL, set sizes to zero */
    ud.addr.maxlen = ud.addr.len = sizeof(struct sockaddr_in);
  else
    ud.addr.maxlen = ud.addr.len = 0;
  ud.opt.buf = (char *) NULL;
  ud.opt.maxlen = ud.opt.len = 0;
  ud.udata.buf = pkt;
  ud.udata.maxlen = ud.udata.len = len;
  if (t_sndudata(fwd_sock, &ud) < 0) {
    plog(XLOG_ERROR,"fwd_packet failed: t_errno=%d, errno=%d",t_errno,errno);
    error = errno;
  }
#else /* not HAVE_TRANSPORT_TYPE_TLI */
  if (sendto(fwd_sock, (char *) pkt, len, 0,
	     (struct sockaddr *) fwdto, sizeof(*fwdto)) < 0)
    error = errno;
#endif /* not HAVE_TRANSPORT_TYPE_TLI */

  /*
   * Save callback function and return address
   */
out:
  p->rf_fwd = cb;
  if (replyto)
    p->rf_sin = *replyto;
  else
    memset((voidp) &p->rf_sin, 0, sizeof(p->rf_sin));
  p->rf_ptr = i;

  return error;
}


/*
 * Called when some data arrives on the forwarding socket
 */
void
fwd_reply(void)
{
  int len;
  u_int pkt[MAX_PACKET_SIZE / sizeof(u_int) + 1];
  u_int *pkt_int;
  int rc;
  rpc_forward *p;
  struct sockaddr_in src_addr;
  RECVFROM_FROMLEN_TYPE src_addr_len;
#ifdef HAVE_TRANSPORT_TYPE_TLI
  struct t_unitdata ud;
  int flags = 0;
#endif /* HAVE_TRANSPORT_TYPE_TLI */

  /*
   * Determine the length of the packet
   */
  len = MAX_PACKET_SIZE;

  /*
   * Read the packet and check for validity
   */
again:
  src_addr_len = sizeof(src_addr);
#ifdef HAVE_TRANSPORT_TYPE_TLI
  ud.addr.buf = (char *) &src_addr;
  ud.addr.maxlen = ud.addr.len = src_addr_len;
  ud.opt.buf = (char *) NULL;
  ud.opt.maxlen = ud.opt.len = 0;
  ud.udata.buf = (char *) pkt;
  ud.udata.maxlen = ud.udata.len = len;
  /* XXX: use flags accordingly such as if T_MORE set */
  rc = t_rcvudata(fwd_sock, &ud, &flags);
  if (rc == 0)			/* success, reset rc to length */
    rc = ud.udata.len;
  else {
    plog(XLOG_ERROR,"fwd_reply failed: t_errno=%d, errno=%d, flags=%d",t_errno,errno, flags);
  }
#else /* not HAVE_TRANSPORT_TYPE_TLI */
  rc = recvfrom(fwd_sock,
		(char *) pkt,
		len,
		0,
		(struct sockaddr *) &src_addr,
		&src_addr_len);
#endif /* not HAVE_TRANSPORT_TYPE_TLI */

  /*
   * XXX: in svr4, if the T_MORE bit of flags is set, what do
   * we then do?  -Erez
   */
  if (rc < 0 || src_addr_len != sizeof(src_addr) ||
      src_addr.sin_family != AF_INET) {
    if (rc < 0 && errno == EINTR)
      goto again;
    plog(XLOG_ERROR, "Error reading RPC reply: %m");
    goto out;
  }

  /*
   * Do no more work if finishing soon
   */
  if ((int) amd_state >= (int) Finishing)
    goto out;

  /*
   * Find packet reference
   */
  pkt_int = (u_int *) pkt;

#ifdef DEBUG
  switch (*pkt_int & RPC_XID_MASK) {
  case RPC_XID_PORTMAP:
    dlog("Receiving PORTMAP reply");
    break;
  case RPC_XID_MOUNTD:
    dlog("Receiving MOUNTD reply %#x", *pkt_int);
    break;
  case RPC_XID_NFSPING:
    dlog("Receiving NFS ping %#x", *pkt_int);
    break;
  default:
    dlog("UNKNOWN RPC XID");
    break;
  }
#endif /* DEBUG */

  p = fwd_locate(*pkt_int);
  if (!p) {
#ifdef DEBUG
    dlog("Can't forward reply id %#x", *pkt_int);
#endif /* DEBUG */
    goto out;
  }

  if (p->rf_fwd) {
    /*
     * Put the original message id back
     * into the packet.
     */
    *pkt_int = p->rf_oldid;

    /*
     * Call forwarding function
     */
    (*p->rf_fwd) ((voidp) pkt, rc, &src_addr, &p->rf_sin, p->rf_ptr, TRUE);
  }

  /*
   * Free forwarding info
   */
  fwd_free(p);

out:;
}
