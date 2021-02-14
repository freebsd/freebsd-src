/* $OpenBSD: serverloop.c,v 1.223 2020/07/03 06:29:57 djm Exp $ */
/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * Server main loop for handling the interactive session.
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 *
 * SSH2 support by Markus Friedl.
 * Copyright (c) 2000, 2001 Markus Friedl.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "includes.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif

#include <netinet/in.h>

#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <limits.h>
#include <signal.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <stdarg.h>

#include "openbsd-compat/sys-queue.h"
#include "xmalloc.h"
#include "packet.h"
#include "sshbuf.h"
#include "log.h"
#include "misc.h"
#include "servconf.h"
#include "canohost.h"
#include "sshpty.h"
#include "channels.h"
#include "compat.h"
#include "ssh2.h"
#include "sshkey.h"
#include "cipher.h"
#include "kex.h"
#include "hostfile.h"
#include "auth.h"
#include "session.h"
#include "dispatch.h"
#include "auth-options.h"
#include "serverloop.h"
#include "ssherr.h"

extern ServerOptions options;

/* XXX */
extern Authctxt *the_authctxt;
extern struct sshauthopt *auth_opts;
extern int use_privsep;

static int no_more_sessions = 0; /* Disallow further sessions. */

/*
 * This SIGCHLD kludge is used to detect when the child exits.  The server
 * will exit after that, as soon as forwarded connections have terminated.
 */

static volatile sig_atomic_t child_terminated = 0;	/* The child has terminated. */

/* Cleanup on signals (!use_privsep case only) */
static volatile sig_atomic_t received_sigterm = 0;

/* prototypes */
static void server_init_dispatch(struct ssh *);

/* requested tunnel forwarding interface(s), shared with session.c */
char *tun_fwd_ifnames = NULL;

/* returns 1 if bind to specified port by specified user is permitted */
static int
bind_permitted(int port, uid_t uid)
{
	if (use_privsep)
		return 1; /* allow system to decide */
	if (port < IPPORT_RESERVED && uid != 0)
		return 0;
	return 1;
}

/*
 * we write to this pipe if a SIGCHLD is caught in order to avoid
 * the race between select() and child_terminated
 */
static int notify_pipe[2];
static void
notify_setup(void)
{
	if (pipe(notify_pipe) == -1) {
		error("pipe(notify_pipe) failed %s", strerror(errno));
	} else if ((fcntl(notify_pipe[0], F_SETFD, FD_CLOEXEC) == -1) ||
	    (fcntl(notify_pipe[1], F_SETFD, FD_CLOEXEC) == -1)) {
		error("fcntl(notify_pipe, F_SETFD) failed %s", strerror(errno));
		close(notify_pipe[0]);
		close(notify_pipe[1]);
	} else {
		set_nonblock(notify_pipe[0]);
		set_nonblock(notify_pipe[1]);
		return;
	}
	notify_pipe[0] = -1;	/* read end */
	notify_pipe[1] = -1;	/* write end */
}
static void
notify_parent(void)
{
	if (notify_pipe[1] != -1)
		(void)write(notify_pipe[1], "", 1);
}
static void
notify_prepare(fd_set *readset)
{
	if (notify_pipe[0] != -1)
		FD_SET(notify_pipe[0], readset);
}
static void
notify_done(fd_set *readset)
{
	char c;

	if (notify_pipe[0] != -1 && FD_ISSET(notify_pipe[0], readset))
		while (read(notify_pipe[0], &c, 1) != -1)
			debug2("%s: reading", __func__);
}

/*ARGSUSED*/
static void
sigchld_handler(int sig)
{
	int save_errno = errno;
	child_terminated = 1;
	notify_parent();
	errno = save_errno;
}

/*ARGSUSED*/
static void
sigterm_handler(int sig)
{
	received_sigterm = sig;
}

static void
client_alive_check(struct ssh *ssh)
{
	char remote_id[512];
	int r, channel_id;

	/* timeout, check to see how many we have had */
	if (options.client_alive_count_max > 0 &&
	    ssh_packet_inc_alive_timeouts(ssh) >
	    options.client_alive_count_max) {
		sshpkt_fmt_connection_id(ssh, remote_id, sizeof(remote_id));
		logit("Timeout, client not responding from %s", remote_id);
		cleanup_exit(255);
	}

	/*
	 * send a bogus global/channel request with "wantreply",
	 * we should get back a failure
	 */
	if ((channel_id = channel_find_open(ssh)) == -1) {
		if ((r = sshpkt_start(ssh, SSH2_MSG_GLOBAL_REQUEST)) != 0 ||
		    (r = sshpkt_put_cstring(ssh, "keepalive@openssh.com"))
		    != 0 ||
		    (r = sshpkt_put_u8(ssh, 1)) != 0) /* boolean: want reply */
			fatal("%s: %s", __func__, ssh_err(r));
	} else {
		channel_request_start(ssh, channel_id,
		    "keepalive@openssh.com", 1);
	}
	if ((r = sshpkt_send(ssh)) != 0)
		fatal("%s: %s", __func__, ssh_err(r));
}

/*
 * Sleep in select() until we can do something.  This will initialize the
 * select masks.  Upon return, the masks will indicate which descriptors
 * have data or can accept data.  Optionally, a maximum time can be specified
 * for the duration of the wait (0 = infinite).
 */
static void
wait_until_can_do_something(struct ssh *ssh,
    int connection_in, int connection_out,
    fd_set **readsetp, fd_set **writesetp, int *maxfdp,
    u_int *nallocp, u_int64_t max_time_ms)
{
	struct timeval tv, *tvp;
	int ret;
	time_t minwait_secs = 0;
	int client_alive_scheduled = 0;
	/* time we last heard from the client OR sent a keepalive */
	static time_t last_client_time;

	/* Allocate and update select() masks for channel descriptors. */
	channel_prepare_select(ssh, readsetp, writesetp, maxfdp,
	    nallocp, &minwait_secs);

	/* XXX need proper deadline system for rekey/client alive */
	if (minwait_secs != 0)
		max_time_ms = MINIMUM(max_time_ms, (u_int)minwait_secs * 1000);

	/*
	 * if using client_alive, set the max timeout accordingly,
	 * and indicate that this particular timeout was for client
	 * alive by setting the client_alive_scheduled flag.
	 *
	 * this could be randomized somewhat to make traffic
	 * analysis more difficult, but we're not doing it yet.
	 */
	if (options.client_alive_interval) {
		uint64_t keepalive_ms =
		    (uint64_t)options.client_alive_interval * 1000;

		if (max_time_ms == 0 || max_time_ms > keepalive_ms) {
			max_time_ms = keepalive_ms;
			client_alive_scheduled = 1;
		}
		if (last_client_time == 0)
			last_client_time = monotime();
	}

#if 0
	/* wrong: bad condition XXX */
	if (channel_not_very_much_buffered_data())
#endif
	FD_SET(connection_in, *readsetp);
	notify_prepare(*readsetp);

	/*
	 * If we have buffered packet data going to the client, mark that
	 * descriptor.
	 */
	if (ssh_packet_have_data_to_write(ssh))
		FD_SET(connection_out, *writesetp);

	/*
	 * If child has terminated and there is enough buffer space to read
	 * from it, then read as much as is available and exit.
	 */
	if (child_terminated && ssh_packet_not_very_much_data_to_write(ssh))
		if (max_time_ms == 0 || client_alive_scheduled)
			max_time_ms = 100;

	if (max_time_ms == 0)
		tvp = NULL;
	else {
		tv.tv_sec = max_time_ms / 1000;
		tv.tv_usec = 1000 * (max_time_ms % 1000);
		tvp = &tv;
	}

	/* Wait for something to happen, or the timeout to expire. */
	ret = select((*maxfdp)+1, *readsetp, *writesetp, NULL, tvp);

	if (ret == -1) {
		memset(*readsetp, 0, *nallocp);
		memset(*writesetp, 0, *nallocp);
		if (errno != EINTR)
			error("select: %.100s", strerror(errno));
	} else if (client_alive_scheduled) {
		time_t now = monotime();

		/*
		 * If the select timed out, or returned for some other reason
		 * but we haven't heard from the client in time, send keepalive.
		 */
		if (ret == 0 || (last_client_time != 0 && last_client_time +
		    options.client_alive_interval <= now)) {
			client_alive_check(ssh);
			last_client_time = now;
		} else if (FD_ISSET(connection_in, *readsetp)) {
			last_client_time = now;
		}
	}

	notify_done(*readsetp);
}

/*
 * Processes input from the client and the program.  Input data is stored
 * in buffers and processed later.
 */
static int
process_input(struct ssh *ssh, fd_set *readset, int connection_in)
{
	int r, len;
	char buf[16384];

	/* Read and buffer any input data from the client. */
	if (FD_ISSET(connection_in, readset)) {
		len = read(connection_in, buf, sizeof(buf));
		if (len == 0) {
			verbose("Connection closed by %.100s port %d",
			    ssh_remote_ipaddr(ssh), ssh_remote_port(ssh));
			return -1;
		} else if (len == -1) {
			if (errno != EINTR && errno != EAGAIN &&
			    errno != EWOULDBLOCK) {
				verbose("Read error from remote host "
				    "%.100s port %d: %.100s",
				    ssh_remote_ipaddr(ssh),
				    ssh_remote_port(ssh), strerror(errno));
				cleanup_exit(255);
			}
		} else {
			/* Buffer any received data. */
			if ((r = ssh_packet_process_incoming(ssh, buf, len))
			    != 0)
				fatal("%s: ssh_packet_process_incoming: %s",
				    __func__, ssh_err(r));
		}
	}
	return 0;
}

/*
 * Sends data from internal buffers to client program stdin.
 */
static void
process_output(struct ssh *ssh, fd_set *writeset, int connection_out)
{
	int r;

	/* Send any buffered packet data to the client. */
	if (FD_ISSET(connection_out, writeset)) {
		if ((r = ssh_packet_write_poll(ssh)) != 0) {
			sshpkt_fatal(ssh, r, "%s: ssh_packet_write_poll",
			    __func__);
		}
	}
}

static void
process_buffered_input_packets(struct ssh *ssh)
{
	ssh_dispatch_run_fatal(ssh, DISPATCH_NONBLOCK, NULL);
}

static void
collect_children(struct ssh *ssh)
{
	pid_t pid;
	sigset_t oset, nset;
	int status;

	/* block SIGCHLD while we check for dead children */
	sigemptyset(&nset);
	sigaddset(&nset, SIGCHLD);
	sigprocmask(SIG_BLOCK, &nset, &oset);
	if (child_terminated) {
		debug("Received SIGCHLD.");
		while ((pid = waitpid(-1, &status, WNOHANG)) > 0 ||
		    (pid == -1 && errno == EINTR))
			if (pid > 0)
				session_close_by_pid(ssh, pid, status);
		child_terminated = 0;
	}
	sigprocmask(SIG_SETMASK, &oset, NULL);
}

void
server_loop2(struct ssh *ssh, Authctxt *authctxt)
{
	fd_set *readset = NULL, *writeset = NULL;
	int max_fd;
	u_int nalloc = 0, connection_in, connection_out;
	u_int64_t rekey_timeout_ms = 0;

	debug("Entering interactive session for SSH2.");

	ssh_signal(SIGCHLD, sigchld_handler);
	child_terminated = 0;
	connection_in = ssh_packet_get_connection_in(ssh);
	connection_out = ssh_packet_get_connection_out(ssh);

	if (!use_privsep) {
		ssh_signal(SIGTERM, sigterm_handler);
		ssh_signal(SIGINT, sigterm_handler);
		ssh_signal(SIGQUIT, sigterm_handler);
	}

	notify_setup();

	max_fd = MAXIMUM(connection_in, connection_out);
	max_fd = MAXIMUM(max_fd, notify_pipe[0]);

	server_init_dispatch(ssh);

	for (;;) {
		process_buffered_input_packets(ssh);

		if (!ssh_packet_is_rekeying(ssh) &&
		    ssh_packet_not_very_much_data_to_write(ssh))
			channel_output_poll(ssh);
		if (options.rekey_interval > 0 &&
		    !ssh_packet_is_rekeying(ssh)) {
			rekey_timeout_ms = ssh_packet_get_rekey_timeout(ssh) *
			    1000;
		} else {
			rekey_timeout_ms = 0;
		}

		wait_until_can_do_something(ssh, connection_in, connection_out,
		    &readset, &writeset, &max_fd, &nalloc, rekey_timeout_ms);

		if (received_sigterm) {
			logit("Exiting on signal %d", (int)received_sigterm);
			/* Clean up sessions, utmp, etc. */
			cleanup_exit(255);
		}

		collect_children(ssh);
		if (!ssh_packet_is_rekeying(ssh))
			channel_after_select(ssh, readset, writeset);
		if (process_input(ssh, readset, connection_in) < 0)
			break;
		process_output(ssh, writeset, connection_out);
	}
	collect_children(ssh);

	free(readset);
	free(writeset);

	/* free all channels, no more reads and writes */
	channel_free_all(ssh);

	/* free remaining sessions, e.g. remove wtmp entries */
	session_destroy_all(ssh, NULL);
}

static int
server_input_keep_alive(int type, u_int32_t seq, struct ssh *ssh)
{
	debug("Got %d/%u for keepalive", type, seq);
	/*
	 * reset timeout, since we got a sane answer from the client.
	 * even if this was generated by something other than
	 * the bogus CHANNEL_REQUEST we send for keepalives.
	 */
	ssh_packet_set_alive_timeouts(ssh, 0);
	return 0;
}

static Channel *
server_request_direct_tcpip(struct ssh *ssh, int *reason, const char **errmsg)
{
	Channel *c = NULL;
	char *target = NULL, *originator = NULL;
	u_int target_port = 0, originator_port = 0;
	int r;

	if ((r = sshpkt_get_cstring(ssh, &target, NULL)) != 0 ||
	    (r = sshpkt_get_u32(ssh, &target_port)) != 0 ||
	    (r = sshpkt_get_cstring(ssh, &originator, NULL)) != 0 ||
	    (r = sshpkt_get_u32(ssh, &originator_port)) != 0 ||
	    (r = sshpkt_get_end(ssh)) != 0)
		sshpkt_fatal(ssh, r, "%s: parse packet", __func__);
	if (target_port > 0xFFFF) {
		error("%s: invalid target port", __func__);
		*reason = SSH2_OPEN_ADMINISTRATIVELY_PROHIBITED;
		goto out;
	}
	if (originator_port > 0xFFFF) {
		error("%s: invalid originator port", __func__);
		*reason = SSH2_OPEN_ADMINISTRATIVELY_PROHIBITED;
		goto out;
	}

	debug("%s: originator %s port %u, target %s port %u", __func__,
	    originator, originator_port, target, target_port);

	/* XXX fine grained permissions */
	if ((options.allow_tcp_forwarding & FORWARD_LOCAL) != 0 &&
	    auth_opts->permit_port_forwarding_flag &&
	    !options.disable_forwarding) {
		c = channel_connect_to_port(ssh, target, target_port,
		    "direct-tcpip", "direct-tcpip", reason, errmsg);
	} else {
		logit("refused local port forward: "
		    "originator %s port %d, target %s port %d",
		    originator, originator_port, target, target_port);
		if (reason != NULL)
			*reason = SSH2_OPEN_ADMINISTRATIVELY_PROHIBITED;
	}

 out:
	free(originator);
	free(target);
	return c;
}

static Channel *
server_request_direct_streamlocal(struct ssh *ssh)
{
	Channel *c = NULL;
	char *target = NULL, *originator = NULL;
	u_int originator_port = 0;
	struct passwd *pw = the_authctxt->pw;
	int r;

	if (pw == NULL || !the_authctxt->valid)
		fatal("%s: no/invalid user", __func__);

	if ((r = sshpkt_get_cstring(ssh, &target, NULL)) != 0 ||
	    (r = sshpkt_get_cstring(ssh, &originator, NULL)) != 0 ||
	    (r = sshpkt_get_u32(ssh, &originator_port)) != 0 ||
	    (r = sshpkt_get_end(ssh)) != 0)
		sshpkt_fatal(ssh, r, "%s: parse packet", __func__);
	if (originator_port > 0xFFFF) {
		error("%s: invalid originator port", __func__);
		goto out;
	}

	debug("%s: originator %s port %d, target %s", __func__,
	    originator, originator_port, target);

	/* XXX fine grained permissions */
	if ((options.allow_streamlocal_forwarding & FORWARD_LOCAL) != 0 &&
	    auth_opts->permit_port_forwarding_flag &&
	    !options.disable_forwarding && (pw->pw_uid == 0 || use_privsep)) {
		c = channel_connect_to_path(ssh, target,
		    "direct-streamlocal@openssh.com", "direct-streamlocal");
	} else {
		logit("refused streamlocal port forward: "
		    "originator %s port %d, target %s",
		    originator, originator_port, target);
	}

out:
	free(originator);
	free(target);
	return c;
}

static Channel *
server_request_tun(struct ssh *ssh)
{
	Channel *c = NULL;
	u_int mode, tun;
	int r, sock;
	char *tmp, *ifname = NULL;

	if ((r = sshpkt_get_u32(ssh, &mode)) != 0)
		sshpkt_fatal(ssh, r, "%s: parse mode", __func__);
	switch (mode) {
	case SSH_TUNMODE_POINTOPOINT:
	case SSH_TUNMODE_ETHERNET:
		break;
	default:
		ssh_packet_send_debug(ssh, "Unsupported tunnel device mode.");
		return NULL;
	}
	if ((options.permit_tun & mode) == 0) {
		ssh_packet_send_debug(ssh, "Server has rejected tunnel device "
		    "forwarding");
		return NULL;
	}

	if ((r = sshpkt_get_u32(ssh, &tun)) != 0)
		sshpkt_fatal(ssh, r, "%s: parse device", __func__);
	if (tun > INT_MAX) {
		debug("%s: invalid tun", __func__);
		goto done;
	}
	if (auth_opts->force_tun_device != -1) {
		if (tun != SSH_TUNID_ANY &&
		    auth_opts->force_tun_device != (int)tun)
			goto done;
		tun = auth_opts->force_tun_device;
	}
	sock = tun_open(tun, mode, &ifname);
	if (sock < 0)
		goto done;
	debug("Tunnel forwarding using interface %s", ifname);

	c = channel_new(ssh, "tun", SSH_CHANNEL_OPEN, sock, sock, -1,
	    CHAN_TCP_WINDOW_DEFAULT, CHAN_TCP_PACKET_DEFAULT, 0, "tun", 1);
	c->datagram = 1;
#if defined(SSH_TUN_FILTER)
	if (mode == SSH_TUNMODE_POINTOPOINT)
		channel_register_filter(ssh, c->self, sys_tun_infilter,
		    sys_tun_outfilter, NULL, NULL);
#endif

	/*
	 * Update the list of names exposed to the session
	 * XXX remove these if the tunnels are closed (won't matter
	 * much if they are already in the environment though)
	 */
	tmp = tun_fwd_ifnames;
	xasprintf(&tun_fwd_ifnames, "%s%s%s",
	    tun_fwd_ifnames == NULL ? "" : tun_fwd_ifnames,
	    tun_fwd_ifnames == NULL ? "" : ",",
	    ifname);
	free(tmp);
	free(ifname);

 done:
	if (c == NULL)
		ssh_packet_send_debug(ssh, "Failed to open the tunnel device.");
	return c;
}

static Channel *
server_request_session(struct ssh *ssh)
{
	Channel *c;
	int r;

	debug("input_session_request");
	if ((r = sshpkt_get_end(ssh)) != 0)
		sshpkt_fatal(ssh, r, "%s: parse packet", __func__);

	if (no_more_sessions) {
		ssh_packet_disconnect(ssh, "Possible attack: attempt to open a "
		    "session after additional sessions disabled");
	}

	/*
	 * A server session has no fd to read or write until a
	 * CHANNEL_REQUEST for a shell is made, so we set the type to
	 * SSH_CHANNEL_LARVAL.  Additionally, a callback for handling all
	 * CHANNEL_REQUEST messages is registered.
	 */
	c = channel_new(ssh, "session", SSH_CHANNEL_LARVAL,
	    -1, -1, -1, /*window size*/0, CHAN_SES_PACKET_DEFAULT,
	    0, "server-session", 1);
	if (session_open(the_authctxt, c->self) != 1) {
		debug("session open failed, free channel %d", c->self);
		channel_free(ssh, c);
		return NULL;
	}
	channel_register_cleanup(ssh, c->self, session_close_by_channel, 0);
	return c;
}

static int
server_input_channel_open(int type, u_int32_t seq, struct ssh *ssh)
{
	Channel *c = NULL;
	char *ctype = NULL;
	const char *errmsg = NULL;
	int r, reason = SSH2_OPEN_CONNECT_FAILED;
	u_int rchan = 0, rmaxpack = 0, rwindow = 0;

	if ((r = sshpkt_get_cstring(ssh, &ctype, NULL)) != 0 ||
	    (r = sshpkt_get_u32(ssh, &rchan)) != 0 ||
	    (r = sshpkt_get_u32(ssh, &rwindow)) != 0 ||
	    (r = sshpkt_get_u32(ssh, &rmaxpack)) != 0)
		sshpkt_fatal(ssh, r, "%s: parse packet", __func__);
	debug("%s: ctype %s rchan %u win %u max %u", __func__,
	    ctype, rchan, rwindow, rmaxpack);

	if (strcmp(ctype, "session") == 0) {
		c = server_request_session(ssh);
	} else if (strcmp(ctype, "direct-tcpip") == 0) {
		c = server_request_direct_tcpip(ssh, &reason, &errmsg);
	} else if (strcmp(ctype, "direct-streamlocal@openssh.com") == 0) {
		c = server_request_direct_streamlocal(ssh);
	} else if (strcmp(ctype, "tun@openssh.com") == 0) {
		c = server_request_tun(ssh);
	}
	if (c != NULL) {
		debug("%s: confirm %s", __func__, ctype);
		c->remote_id = rchan;
		c->have_remote_id = 1;
		c->remote_window = rwindow;
		c->remote_maxpacket = rmaxpack;
		if (c->type != SSH_CHANNEL_CONNECTING) {
			if ((r = sshpkt_start(ssh, SSH2_MSG_CHANNEL_OPEN_CONFIRMATION)) != 0 ||
			    (r = sshpkt_put_u32(ssh, c->remote_id)) != 0 ||
			    (r = sshpkt_put_u32(ssh, c->self)) != 0 ||
			    (r = sshpkt_put_u32(ssh, c->local_window)) != 0 ||
			    (r = sshpkt_put_u32(ssh, c->local_maxpacket)) != 0 ||
			    (r = sshpkt_send(ssh)) != 0) {
				sshpkt_fatal(ssh, r,
				    "%s: send open confirm", __func__);
			}
		}
	} else {
		debug("%s: failure %s", __func__, ctype);
		if ((r = sshpkt_start(ssh, SSH2_MSG_CHANNEL_OPEN_FAILURE)) != 0 ||
		    (r = sshpkt_put_u32(ssh, rchan)) != 0 ||
		    (r = sshpkt_put_u32(ssh, reason)) != 0 ||
		    (r = sshpkt_put_cstring(ssh, errmsg ? errmsg : "open failed")) != 0 ||
		    (r = sshpkt_put_cstring(ssh, "")) != 0 ||
		    (r = sshpkt_send(ssh)) != 0) {
			sshpkt_fatal(ssh, r,
			    "%s: send open failure", __func__);
		}
	}
	free(ctype);
	return 0;
}

static int
server_input_hostkeys_prove(struct ssh *ssh, struct sshbuf **respp)
{
	struct sshbuf *resp = NULL;
	struct sshbuf *sigbuf = NULL;
	struct sshkey *key = NULL, *key_pub = NULL, *key_prv = NULL;
	int r, ndx, kexsigtype, use_kexsigtype, success = 0;
	const u_char *blob;
	u_char *sig = 0;
	size_t blen, slen;

	if ((resp = sshbuf_new()) == NULL || (sigbuf = sshbuf_new()) == NULL)
		fatal("%s: sshbuf_new", __func__);

	kexsigtype = sshkey_type_plain(
	    sshkey_type_from_name(ssh->kex->hostkey_alg));
	while (ssh_packet_remaining(ssh) > 0) {
		sshkey_free(key);
		key = NULL;
		if ((r = sshpkt_get_string_direct(ssh, &blob, &blen)) != 0 ||
		    (r = sshkey_from_blob(blob, blen, &key)) != 0) {
			error("%s: couldn't parse key: %s",
			    __func__, ssh_err(r));
			goto out;
		}
		/*
		 * Better check that this is actually one of our hostkeys
		 * before attempting to sign anything with it.
		 */
		if ((ndx = ssh->kex->host_key_index(key, 1, ssh)) == -1) {
			error("%s: unknown host %s key",
			    __func__, sshkey_type(key));
			goto out;
		}
		/*
		 * XXX refactor: make kex->sign just use an index rather
		 * than passing in public and private keys
		 */
		if ((key_prv = get_hostkey_by_index(ndx)) == NULL &&
		    (key_pub = get_hostkey_public_by_index(ndx, ssh)) == NULL) {
			error("%s: can't retrieve hostkey %d", __func__, ndx);
			goto out;
		}
		sshbuf_reset(sigbuf);
		free(sig);
		sig = NULL;
		/*
		 * For RSA keys, prefer to use the signature type negotiated
		 * during KEX to the default (SHA1).
		 */
		use_kexsigtype = kexsigtype == KEY_RSA &&
		    sshkey_type_plain(key->type) == KEY_RSA;
		if ((r = sshbuf_put_cstring(sigbuf,
		    "hostkeys-prove-00@openssh.com")) != 0 ||
		    (r = sshbuf_put_string(sigbuf,
		    ssh->kex->session_id, ssh->kex->session_id_len)) != 0 ||
		    (r = sshkey_puts(key, sigbuf)) != 0 ||
		    (r = ssh->kex->sign(ssh, key_prv, key_pub, &sig, &slen,
		    sshbuf_ptr(sigbuf), sshbuf_len(sigbuf),
		    use_kexsigtype ? ssh->kex->hostkey_alg : NULL)) != 0 ||
		    (r = sshbuf_put_string(resp, sig, slen)) != 0) {
			error("%s: couldn't prepare signature: %s",
			    __func__, ssh_err(r));
			goto out;
		}
	}
	/* Success */
	*respp = resp;
	resp = NULL; /* don't free it */
	success = 1;
 out:
	free(sig);
	sshbuf_free(resp);
	sshbuf_free(sigbuf);
	sshkey_free(key);
	return success;
}

static int
server_input_global_request(int type, u_int32_t seq, struct ssh *ssh)
{
	char *rtype = NULL;
	u_char want_reply = 0;
	int r, success = 0, allocated_listen_port = 0;
	u_int port = 0;
	struct sshbuf *resp = NULL;
	struct passwd *pw = the_authctxt->pw;
	struct Forward fwd;

	memset(&fwd, 0, sizeof(fwd));
	if (pw == NULL || !the_authctxt->valid)
		fatal("%s: no/invalid user", __func__);

	if ((r = sshpkt_get_cstring(ssh, &rtype, NULL)) != 0 ||
	    (r = sshpkt_get_u8(ssh, &want_reply)) != 0)
		sshpkt_fatal(ssh, r, "%s: parse packet", __func__);
	debug("%s: rtype %s want_reply %d", __func__, rtype, want_reply);

	/* -R style forwarding */
	if (strcmp(rtype, "tcpip-forward") == 0) {
		if ((r = sshpkt_get_cstring(ssh, &fwd.listen_host, NULL)) != 0 ||
		    (r = sshpkt_get_u32(ssh, &port)) != 0)
			sshpkt_fatal(ssh, r, "%s: parse tcpip-forward", __func__);
		debug("%s: tcpip-forward listen %s port %u", __func__,
		    fwd.listen_host, port);
		if (port <= INT_MAX)
			fwd.listen_port = (int)port;
		/* check permissions */
		if (port > INT_MAX ||
		    (options.allow_tcp_forwarding & FORWARD_REMOTE) == 0 ||
		    !auth_opts->permit_port_forwarding_flag ||
		    options.disable_forwarding ||
		    (!want_reply && fwd.listen_port == 0) ||
		    (fwd.listen_port != 0 &&
		     !bind_permitted(fwd.listen_port, pw->pw_uid))) {
			success = 0;
			ssh_packet_send_debug(ssh, "Server has disabled port forwarding.");
		} else {
			/* Start listening on the port */
			success = channel_setup_remote_fwd_listener(ssh, &fwd,
			    &allocated_listen_port, &options.fwd_opts);
		}
		if ((resp = sshbuf_new()) == NULL)
			fatal("%s: sshbuf_new", __func__);
		if (allocated_listen_port != 0 &&
		    (r = sshbuf_put_u32(resp, allocated_listen_port)) != 0)
			fatal("%s: sshbuf_put_u32: %s", __func__, ssh_err(r));
	} else if (strcmp(rtype, "cancel-tcpip-forward") == 0) {
		if ((r = sshpkt_get_cstring(ssh, &fwd.listen_host, NULL)) != 0 ||
		    (r = sshpkt_get_u32(ssh, &port)) != 0)
			sshpkt_fatal(ssh, r, "%s: parse cancel-tcpip-forward", __func__);

		debug("%s: cancel-tcpip-forward addr %s port %d", __func__,
		    fwd.listen_host, port);
		if (port <= INT_MAX) {
			fwd.listen_port = (int)port;
			success = channel_cancel_rport_listener(ssh, &fwd);
		}
	} else if (strcmp(rtype, "streamlocal-forward@openssh.com") == 0) {
		if ((r = sshpkt_get_cstring(ssh, &fwd.listen_path, NULL)) != 0)
			sshpkt_fatal(ssh, r, "%s: parse streamlocal-forward@openssh.com", __func__);
		debug("%s: streamlocal-forward listen path %s", __func__,
		    fwd.listen_path);

		/* check permissions */
		if ((options.allow_streamlocal_forwarding & FORWARD_REMOTE) == 0
		    || !auth_opts->permit_port_forwarding_flag ||
		    options.disable_forwarding ||
		    (pw->pw_uid != 0 && !use_privsep)) {
			success = 0;
			ssh_packet_send_debug(ssh, "Server has disabled "
			    "streamlocal forwarding.");
		} else {
			/* Start listening on the socket */
			success = channel_setup_remote_fwd_listener(ssh,
			    &fwd, NULL, &options.fwd_opts);
		}
	} else if (strcmp(rtype, "cancel-streamlocal-forward@openssh.com") == 0) {
		if ((r = sshpkt_get_cstring(ssh, &fwd.listen_path, NULL)) != 0)
			sshpkt_fatal(ssh, r, "%s: parse cancel-streamlocal-forward@openssh.com", __func__);
		debug("%s: cancel-streamlocal-forward path %s", __func__,
		    fwd.listen_path);

		success = channel_cancel_rport_listener(ssh, &fwd);
	} else if (strcmp(rtype, "no-more-sessions@openssh.com") == 0) {
		no_more_sessions = 1;
		success = 1;
	} else if (strcmp(rtype, "hostkeys-prove-00@openssh.com") == 0) {
		success = server_input_hostkeys_prove(ssh, &resp);
	}
	/* XXX sshpkt_get_end() */
	if (want_reply) {
		if ((r = sshpkt_start(ssh, success ?
		    SSH2_MSG_REQUEST_SUCCESS : SSH2_MSG_REQUEST_FAILURE)) != 0 ||
		    (success && resp != NULL && (r = sshpkt_putb(ssh, resp)) != 0) ||
		    (r = sshpkt_send(ssh)) != 0 ||
		    (r = ssh_packet_write_wait(ssh)) != 0)
			sshpkt_fatal(ssh, r, "%s: send reply", __func__);
	}
	free(fwd.listen_host);
	free(fwd.listen_path);
	free(rtype);
	sshbuf_free(resp);
	return 0;
}

static int
server_input_channel_req(int type, u_int32_t seq, struct ssh *ssh)
{
	Channel *c;
	int r, success = 0;
	char *rtype = NULL;
	u_char want_reply = 0;
	u_int id = 0;

	if ((r = sshpkt_get_u32(ssh, &id)) != 0 ||
	    (r = sshpkt_get_cstring(ssh, &rtype, NULL)) != 0 ||
	    (r = sshpkt_get_u8(ssh, &want_reply)) != 0)
		sshpkt_fatal(ssh, r, "%s: parse packet", __func__);

	debug("server_input_channel_req: channel %u request %s reply %d",
	    id, rtype, want_reply);

	if (id >= INT_MAX || (c = channel_lookup(ssh, (int)id)) == NULL) {
		ssh_packet_disconnect(ssh, "%s: unknown channel %d",
		    __func__, id);
	}
	if (!strcmp(rtype, "eow@openssh.com")) {
		if ((r = sshpkt_get_end(ssh)) != 0)
			sshpkt_fatal(ssh, r, "%s: parse packet", __func__);
		chan_rcvd_eow(ssh, c);
	} else if ((c->type == SSH_CHANNEL_LARVAL ||
	    c->type == SSH_CHANNEL_OPEN) && strcmp(c->ctype, "session") == 0)
		success = session_input_channel_req(ssh, c, rtype);
	if (want_reply && !(c->flags & CHAN_CLOSE_SENT)) {
		if (!c->have_remote_id)
			fatal("%s: channel %d: no remote_id",
			    __func__, c->self);
		if ((r = sshpkt_start(ssh, success ?
		    SSH2_MSG_CHANNEL_SUCCESS : SSH2_MSG_CHANNEL_FAILURE)) != 0 ||
		    (r = sshpkt_put_u32(ssh, c->remote_id)) != 0 ||
		    (r = sshpkt_send(ssh)) != 0)
			sshpkt_fatal(ssh, r, "%s: send reply", __func__);
	}
	free(rtype);
	return 0;
}

static void
server_init_dispatch(struct ssh *ssh)
{
	debug("server_init_dispatch");
	ssh_dispatch_init(ssh, &dispatch_protocol_error);
	ssh_dispatch_set(ssh, SSH2_MSG_CHANNEL_CLOSE, &channel_input_oclose);
	ssh_dispatch_set(ssh, SSH2_MSG_CHANNEL_DATA, &channel_input_data);
	ssh_dispatch_set(ssh, SSH2_MSG_CHANNEL_EOF, &channel_input_ieof);
	ssh_dispatch_set(ssh, SSH2_MSG_CHANNEL_EXTENDED_DATA, &channel_input_extended_data);
	ssh_dispatch_set(ssh, SSH2_MSG_CHANNEL_OPEN, &server_input_channel_open);
	ssh_dispatch_set(ssh, SSH2_MSG_CHANNEL_OPEN_CONFIRMATION, &channel_input_open_confirmation);
	ssh_dispatch_set(ssh, SSH2_MSG_CHANNEL_OPEN_FAILURE, &channel_input_open_failure);
	ssh_dispatch_set(ssh, SSH2_MSG_CHANNEL_REQUEST, &server_input_channel_req);
	ssh_dispatch_set(ssh, SSH2_MSG_CHANNEL_WINDOW_ADJUST, &channel_input_window_adjust);
	ssh_dispatch_set(ssh, SSH2_MSG_GLOBAL_REQUEST, &server_input_global_request);
	/* client_alive */
	ssh_dispatch_set(ssh, SSH2_MSG_CHANNEL_SUCCESS, &server_input_keep_alive);
	ssh_dispatch_set(ssh, SSH2_MSG_CHANNEL_FAILURE, &server_input_keep_alive);
	ssh_dispatch_set(ssh, SSH2_MSG_REQUEST_SUCCESS, &server_input_keep_alive);
	ssh_dispatch_set(ssh, SSH2_MSG_REQUEST_FAILURE, &server_input_keep_alive);
	/* rekeying */
	ssh_dispatch_set(ssh, SSH2_MSG_KEXINIT, &kex_input_kexinit);
}
