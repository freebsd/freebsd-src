/*
 * Copyright (c) 1997-2004 Erez Zadok
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
 * $Id: nfs_start.c,v 1.5.2.7 2004/01/06 03:15:16 ezk Exp $
 *
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */
#include <am_defs.h>
#include <amd.h>

#ifndef SELECT_MAXWAIT
# define SELECT_MAXWAIT 16
#endif /* not SELECT_MAXWAIT */

SVCXPRT *nfsxprt;
u_short nfs_port;

#ifndef HAVE_SIGACTION
# define MASKED_SIGS	(sigmask(SIGINT)|sigmask(SIGTERM)|sigmask(SIGCHLD)|sigmask(SIGHUP))
#endif /* not HAVE_SIGACTION */

#ifdef DEBUG
/*
 * Check that we are not burning resources
 */
static void
checkup(void)
{

  static int max_fd = 0;
  static char *max_mem = 0;

  int next_fd = dup(0);
  caddr_t next_mem = sbrk(0);
  close(next_fd);

  if (max_fd < next_fd) {
    dlog("%d new fds allocated; total is %d",
	 next_fd - max_fd, next_fd);
    max_fd = next_fd;
  }
  if (max_mem < next_mem) {
#ifdef HAVE_GETPAGESIZE
    dlog("%#lx bytes of memory allocated; total is %#lx (%ld pages)",
	 (long) (next_mem - max_mem), (unsigned long) next_mem,
	 ((long) next_mem + getpagesize() - 1) / (long) getpagesize());
#else /* not HAVE_GETPAGESIZE */
    dlog("%#lx bytes of memory allocated; total is %#lx",
	 (long) (next_mem - max_mem), (unsigned long) next_mem);
#endif /* not HAVE_GETPAGESIZE */
    max_mem = next_mem;

  }
}
#endif /* DEBUG */


static int
#ifdef HAVE_SIGACTION
do_select(sigset_t smask, int fds, fd_set *fdp, struct timeval *tvp)
#else /* not HAVE_SIGACTION */
do_select(int smask, int fds, fd_set *fdp, struct timeval *tvp)
#endif /* not HAVE_SIGACTION */
{

  int sig;
  int nsel;

  if ((sig = setjmp(select_intr))) {
    select_intr_valid = 0;
    /* Got a signal */
    switch (sig) {
    case SIGINT:
    case SIGTERM:
      amd_state = Finishing;
      reschedule_timeout_mp();
      break;
    }
    nsel = -1;
    errno = EINTR;
  } else {
    select_intr_valid = 1;
    /*
     * Invalidate the current clock value
     */
    clock_valid = 0;
    /*
     * Allow interrupts.  If a signal
     * occurs, then it will cause a longjmp
     * up above.
     */
#ifdef HAVE_SIGACTION
    sigprocmask(SIG_SETMASK, &smask, NULL);
#else /* not HAVE_SIGACTION */
    (void) sigsetmask(smask);
#endif /* not HAVE_SIGACTION */

    /*
     * Wait for input
     */
    nsel = select(fds, fdp, (fd_set *) 0, (fd_set *) 0,
		  tvp->tv_sec ? tvp : (struct timeval *) 0);
  }

#ifdef HAVE_SIGACTION
  sigprocmask(SIG_BLOCK, &masked_sigs, NULL);
#else /* not HAVE_SIGACTION */
  (void) sigblock(MASKED_SIGS);
#endif /* not HAVE_SIGACTION */

  /*
   * Perhaps reload the cache?
   */
  if (do_mapc_reload < clocktime()) {
    mapc_reload();
    do_mapc_reload = clocktime() + ONE_HOUR;
  }
  return nsel;
}


/*
 * Determine whether anything is left in
 * the RPC input queue.
 */
static int
rpc_pending_now(void)
{
  struct timeval tvv;
  int nsel;
#ifdef FD_SET
  fd_set readfds;

  FD_ZERO(&readfds);
  FD_SET(fwd_sock, &readfds);
#else /* not FD_SET */
  int readfds = (1 << fwd_sock);
#endif /* not FD_SET */

  tvv.tv_sec = tvv.tv_usec = 0;
  nsel = select(FD_SETSIZE, &readfds, (fd_set *) 0, (fd_set *) 0, &tvv);
  if (nsel < 1)
    return (0);
#ifdef FD_SET
  if (FD_ISSET(fwd_sock, &readfds))
    return (1);
#else /* not FD_SET */
  if (readfds & (1 << fwd_sock))
    return (1);
#endif /* not FD_SET */

  return (0);
}


static serv_state
run_rpc(void)
{
#ifdef HAVE_SIGACTION
  sigset_t smask;
  sigprocmask(SIG_BLOCK, &masked_sigs, &smask);
#else /* not HAVE_SIGACTION */
  int smask = sigblock(MASKED_SIGS);
#endif /* not HAVE_SIGACTION */

  next_softclock = clocktime();

  amd_state = Run;

  /*
   * Keep on trucking while we are in Run mode.  This state
   * is switched to Quit after all the file systems have
   * been unmounted.
   */
  while ((int) amd_state <= (int) Finishing) {
    struct timeval tvv;
    int nsel;
    time_t now;
#ifdef HAVE_SVC_GETREQSET
    fd_set readfds;

    memmove(&readfds, &svc_fdset, sizeof(svc_fdset));
    FD_SET(fwd_sock, &readfds);
#else /* not HAVE_SVC_GETREQSET */
# ifdef FD_SET
    fd_set readfds;
    FD_ZERO(&readfds);
    readfds.fds_bits[0] = svc_fds;
    FD_SET(fwd_sock, &readfds);
# else /* not FD_SET */
    int readfds = svc_fds | (1 << fwd_sock);
# endif /* not FD_SET */
#endif /* not HAVE_SVC_GETREQSET */

#ifdef DEBUG
    checkup();
#endif /* DEBUG */

    /*
     * If the full timeout code is not called,
     * then recompute the time delta manually.
     */
    now = clocktime();

    if (next_softclock <= now) {
      if (amd_state == Finishing)
	umount_exported();
      tvv.tv_sec = softclock();
    } else {
      tvv.tv_sec = next_softclock - now;
    }
    tvv.tv_usec = 0;

    if (amd_state == Finishing && last_used_map < 0) {
      flush_mntfs();
      amd_state = Quit;
      break;
    }
    if (tvv.tv_sec <= 0)
      tvv.tv_sec = SELECT_MAXWAIT;
#ifdef DEBUG
    if (tvv.tv_sec) {
      dlog("Select waits for %ds", (int) tvv.tv_sec);
    } else {
      dlog("Select waits for Godot");
    }
#endif /* DEBUG */

    nsel = do_select(smask, FD_SETSIZE, &readfds, &tvv);

    switch (nsel) {
    case -1:
      if (errno == EINTR) {
#ifdef DEBUG
	dlog("select interrupted");
#endif /* DEBUG */
	continue;
      }
      plog(XLOG_ERROR, "select: %m");
      break;

    case 0:
      break;

    default:
      /*
       * Read all pending NFS responses at once to avoid having responses.
       * queue up as a consequence of retransmissions.
       */
#ifdef FD_SET
      if (FD_ISSET(fwd_sock, &readfds)) {
	FD_CLR(fwd_sock, &readfds);
#else /* not FD_SET */
      if (readfds & (1 << fwd_sock)) {
	readfds &= ~(1 << fwd_sock);
#endif /* not FD_SET */
	--nsel;
	do {
	  fwd_reply();
	} while (rpc_pending_now() > 0);
      }

      if (nsel) {
	/*
	 * Anything left must be a normal
	 * RPC request.
	 */
#ifdef HAVE_SVC_GETREQSET
	svc_getreqset(&readfds);
#else /* not HAVE_SVC_GETREQSET */
# ifdef FD_SET
	svc_getreq(readfds.fds_bits[0]);
# else /* not FD_SET */
	svc_getreq(readfds);
# endif /* not FD_SET */
#endif /* not HAVE_SVC_GETREQSET */
      }
      break;
    }
  }

#ifdef HAVE_SIGACTION
  sigprocmask(SIG_SETMASK, &smask, NULL);
#else /* not HAVE_SIGACTION */
  (void) sigsetmask(smask);
#endif /* not HAVE_SIGACTION */

  if (amd_state == Quit)
    amd_state = Done;

  return amd_state;
}


int
mount_automounter(int ppid)
{
  /*
   * Old code replaced by rpc-trash patch.
   * Erez Zadok <ezk@cs.columbia.edu>
   int so = socket(AF_INET, SOCK_DGRAM, 0);
   */
  SVCXPRT *udp_amqp = NULL, *tcp_amqp = NULL;
  int nmount, ret;
  int soNFS;
  int udp_soAMQ, tcp_soAMQ;
#ifdef HAVE_TRANSPORT_TYPE_TLI
  struct netconfig *udp_amqncp, *tcp_amqncp;
#endif /* HAVE_TRANSPORT_TYPE_TLI */

  /*
   * Create the nfs service for amd
   */
#ifdef HAVE_TRANSPORT_TYPE_TLI
  ret = create_nfs_service(&soNFS, &nfs_port, &nfsxprt, nfs_program_2);
  if (ret != 0)
    return ret;
  ret = create_amq_service(&udp_soAMQ, &udp_amqp, &udp_amqncp, &tcp_soAMQ, &tcp_amqp, &tcp_amqncp);
#else /* not HAVE_TRANSPORT_TYPE_TLI */
  ret = create_nfs_service(&soNFS, &nfs_port, &nfsxprt, nfs_program_2);
  if (ret != 0)
    return ret;
  ret = create_amq_service(&udp_soAMQ, &udp_amqp, &tcp_soAMQ, &tcp_amqp);
#endif /* not HAVE_TRANSPORT_TYPE_TLI */
  if (ret != 0)
    return ret;

  /*
   * Start RPC forwarding
   */
  if (fwd_init() != 0)
    return 3;

  /*
   * Construct the root automount node
   */
  make_root_node();

  /*
   * Pick up the pieces from a previous run
   * This is likely to (indirectly) need the rpc_fwd package
   * so it *must* come after the call to fwd_init().
   */
  if (gopt.flags & CFM_RESTART_EXISTING_MOUNTS)
    restart();

  /*
   * Mount the top-level auto-mountpoints
   */
  nmount = mount_exported();

  /*
   * Now safe to tell parent that we are up and running
   */
  if (ppid)
    kill(ppid, SIGQUIT);

  if (nmount == 0) {
    plog(XLOG_FATAL, "No work to do - quitting");
    amd_state = Done;
    return 0;
  }

#ifdef DEBUG
  amuDebug(D_AMQ) {
#endif /* DEBUG */
    /*
     * Complete registration of amq (first TCP service then UDP)
     */
    unregister_amq();

#ifdef HAVE_TRANSPORT_TYPE_TLI
    ret = svc_reg(tcp_amqp, get_amd_program_number(), AMQ_VERSION,
		  amq_program_1, tcp_amqncp);
#else /* not HAVE_TRANSPORT_TYPE_TLI */
    ret = svc_register(tcp_amqp, get_amd_program_number(), AMQ_VERSION,
		       amq_program_1, IPPROTO_TCP);
#endif /* not HAVE_TRANSPORT_TYPE_TLI */
    if (ret != 1) {
      plog(XLOG_FATAL, "unable to register (AMQ_PROGRAM=%d, AMQ_VERSION, tcp)", get_amd_program_number());
      return 3;
    }

#ifdef HAVE_TRANSPORT_TYPE_TLI
    ret = svc_reg(udp_amqp, get_amd_program_number(), AMQ_VERSION,
		  amq_program_1, udp_amqncp);
#else /* not HAVE_TRANSPORT_TYPE_TLI */
    ret = svc_register(udp_amqp, get_amd_program_number(), AMQ_VERSION,
		       amq_program_1, IPPROTO_UDP);
#endif /* not HAVE_TRANSPORT_TYPE_TLI */
    if (ret != 1) {
      plog(XLOG_FATAL, "unable to register (AMQ_PROGRAM=%d, AMQ_VERSION, udp)", get_amd_program_number());
      return 4;
    }

#ifdef DEBUG
  }
#endif /* DEBUG */

  /*
   * Start timeout_mp rolling
   */
  reschedule_timeout_mp();

  /*
   * Start the server
   */
  if (run_rpc() != Done) {
    plog(XLOG_FATAL, "run_rpc failed");
    amd_state = Done;
  }
  return 0;
}
