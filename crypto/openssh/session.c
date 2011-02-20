/* $OpenBSD: session.c,v 1.256 2010/06/25 07:20:04 djm Exp $ */
/*
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
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
__RCSID("$FreeBSD$");

#include <sys/types.h>
#include <sys/param.h>
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>

#include <arpa/inet.h>

#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#ifdef HAVE_PATHS_H
#include <paths.h>
#endif
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "openbsd-compat/sys-queue.h"
#include "xmalloc.h"
#include "ssh.h"
#include "ssh1.h"
#include "ssh2.h"
#include "sshpty.h"
#include "packet.h"
#include "buffer.h"
#include "match.h"
#include "uidswap.h"
#include "compat.h"
#include "channels.h"
#include "key.h"
#include "cipher.h"
#ifdef GSSAPI
#include "ssh-gss.h"
#endif
#include "hostfile.h"
#include "auth.h"
#include "auth-options.h"
#include "pathnames.h"
#include "log.h"
#include "servconf.h"
#include "sshlogin.h"
#include "serverloop.h"
#include "canohost.h"
#include "misc.h"
#include "session.h"
#include "kex.h"
#include "monitor_wrap.h"
#include "sftp.h"

#if defined(KRB5) && defined(USE_AFS)
#include <kafs.h>
#endif

#define IS_INTERNAL_SFTP(c) \
	(!strncmp(c, INTERNAL_SFTP_NAME, sizeof(INTERNAL_SFTP_NAME) - 1) && \
	 (c[sizeof(INTERNAL_SFTP_NAME) - 1] == '\0' || \
	  c[sizeof(INTERNAL_SFTP_NAME) - 1] == ' ' || \
	  c[sizeof(INTERNAL_SFTP_NAME) - 1] == '\t'))

/* func */

Session *session_new(void);
void	session_set_fds(Session *, int, int, int, int, int);
void	session_pty_cleanup(Session *);
void	session_proctitle(Session *);
int	session_setup_x11fwd(Session *);
int	do_exec_pty(Session *, const char *);
int	do_exec_no_pty(Session *, const char *);
int	do_exec(Session *, const char *);
void	do_login(Session *, const char *);
#ifdef LOGIN_NEEDS_UTMPX
static void	do_pre_login(Session *s);
#endif
void	do_child(Session *, const char *);
void	do_motd(void);
int	check_quietlogin(Session *, const char *);

static void do_authenticated1(Authctxt *);
static void do_authenticated2(Authctxt *);

static int session_pty_req(Session *);

/* import */
extern ServerOptions options;
extern char *__progname;
extern int log_stderr;
extern int debug_flag;
extern u_int utmp_len;
extern int startup_pipe;
extern void destroy_sensitive_data(void);
extern Buffer loginmsg;

/* original command from peer. */
const char *original_command = NULL;

/* data */
static int sessions_first_unused = -1;
static int sessions_nalloc = 0;
static Session *sessions = NULL;

#define SUBSYSTEM_NONE			0
#define SUBSYSTEM_EXT			1
#define SUBSYSTEM_INT_SFTP		2
#define SUBSYSTEM_INT_SFTP_ERROR	3

#ifdef HAVE_LOGIN_CAP
login_cap_t *lc;
#endif

static int is_child = 0;

/* Name and directory of socket for authentication agent forwarding. */
static char *auth_sock_name = NULL;
static char *auth_sock_dir = NULL;

/* removes the agent forwarding socket */

static void
auth_sock_cleanup_proc(struct passwd *pw)
{
	if (auth_sock_name != NULL) {
		temporarily_use_uid(pw);
		unlink(auth_sock_name);
		rmdir(auth_sock_dir);
		auth_sock_name = NULL;
		restore_uid();
	}
}

static int
auth_input_request_forwarding(struct passwd * pw)
{
	Channel *nc;
	int sock = -1;
	struct sockaddr_un sunaddr;

	if (auth_sock_name != NULL) {
		error("authentication forwarding requested twice.");
		return 0;
	}

	/* Temporarily drop privileged uid for mkdir/bind. */
	temporarily_use_uid(pw);

	/* Allocate a buffer for the socket name, and format the name. */
	auth_sock_dir = xstrdup("/tmp/ssh-XXXXXXXXXX");

	/* Create private directory for socket */
	if (mkdtemp(auth_sock_dir) == NULL) {
		packet_send_debug("Agent forwarding disabled: "
		    "mkdtemp() failed: %.100s", strerror(errno));
		restore_uid();
		xfree(auth_sock_dir);
		auth_sock_dir = NULL;
		goto authsock_err;
	}

	xasprintf(&auth_sock_name, "%s/agent.%ld",
	    auth_sock_dir, (long) getpid());

	/* Create the socket. */
	sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock < 0) {
		error("socket: %.100s", strerror(errno));
		restore_uid();
		goto authsock_err;
	}

	/* Bind it to the name. */
	memset(&sunaddr, 0, sizeof(sunaddr));
	sunaddr.sun_family = AF_UNIX;
	strlcpy(sunaddr.sun_path, auth_sock_name, sizeof(sunaddr.sun_path));

	if (bind(sock, (struct sockaddr *)&sunaddr, sizeof(sunaddr)) < 0) {
		error("bind: %.100s", strerror(errno));
		restore_uid();
		goto authsock_err;
	}

	/* Restore the privileged uid. */
	restore_uid();

	/* Start listening on the socket. */
	if (listen(sock, SSH_LISTEN_BACKLOG) < 0) {
		error("listen: %.100s", strerror(errno));
		goto authsock_err;
	}

	/* Allocate a channel for the authentication agent socket. */
	nc = channel_new("auth socket",
	    SSH_CHANNEL_AUTH_SOCKET, sock, sock, -1,
	    CHAN_X11_WINDOW_DEFAULT, CHAN_X11_PACKET_DEFAULT,
	    0, "auth socket", 1);
	nc->path = xstrdup(auth_sock_name);
	return 1;

 authsock_err:
	if (auth_sock_name != NULL)
		xfree(auth_sock_name);
	if (auth_sock_dir != NULL) {
		rmdir(auth_sock_dir);
		xfree(auth_sock_dir);
	}
	if (sock != -1)
		close(sock);
	auth_sock_name = NULL;
	auth_sock_dir = NULL;
	return 0;
}

static void
display_loginmsg(void)
{
	if (buffer_len(&loginmsg) > 0) {
		buffer_append(&loginmsg, "\0", 1);
		printf("%s", (char *)buffer_ptr(&loginmsg));
		buffer_clear(&loginmsg);
	}
}

void
do_authenticated(Authctxt *authctxt)
{
	setproctitle("%s", authctxt->pw->pw_name);

	/* setup the channel layer */
	if (!no_port_forwarding_flag && options.allow_tcp_forwarding)
		channel_permit_all_opens();

	auth_debug_send();

	if (compat20)
		do_authenticated2(authctxt);
	else
		do_authenticated1(authctxt);

	do_cleanup(authctxt);
}

/*
 * Prepares for an interactive session.  This is called after the user has
 * been successfully authenticated.  During this message exchange, pseudo
 * terminals are allocated, X11, TCP/IP, and authentication agent forwardings
 * are requested, etc.
 */
static void
do_authenticated1(Authctxt *authctxt)
{
	Session *s;
	char *command;
	int success, type, screen_flag;
	int enable_compression_after_reply = 0;
	u_int proto_len, data_len, dlen, compression_level = 0;

	s = session_new();
	if (s == NULL) {
		error("no more sessions");
		return;
	}
	s->authctxt = authctxt;
	s->pw = authctxt->pw;

	/*
	 * We stay in this loop until the client requests to execute a shell
	 * or a command.
	 */
	for (;;) {
		success = 0;

		/* Get a packet from the client. */
		type = packet_read();

		/* Process the packet. */
		switch (type) {
		case SSH_CMSG_REQUEST_COMPRESSION:
			compression_level = packet_get_int();
			packet_check_eom();
			if (compression_level < 1 || compression_level > 9) {
				packet_send_debug("Received invalid compression level %d.",
				    compression_level);
				break;
			}
			if (options.compression == COMP_NONE) {
				debug2("compression disabled");
				break;
			}
			/* Enable compression after we have responded with SUCCESS. */
			enable_compression_after_reply = 1;
			success = 1;
			break;

		case SSH_CMSG_REQUEST_PTY:
			success = session_pty_req(s);
			break;

		case SSH_CMSG_X11_REQUEST_FORWARDING:
			s->auth_proto = packet_get_string(&proto_len);
			s->auth_data = packet_get_string(&data_len);

			screen_flag = packet_get_protocol_flags() &
			    SSH_PROTOFLAG_SCREEN_NUMBER;
			debug2("SSH_PROTOFLAG_SCREEN_NUMBER: %d", screen_flag);

			if (packet_remaining() == 4) {
				if (!screen_flag)
					debug2("Buggy client: "
					    "X11 screen flag missing");
				s->screen = packet_get_int();
			} else {
				s->screen = 0;
			}
			packet_check_eom();
			success = session_setup_x11fwd(s);
			if (!success) {
				xfree(s->auth_proto);
				xfree(s->auth_data);
				s->auth_proto = NULL;
				s->auth_data = NULL;
			}
			break;

		case SSH_CMSG_AGENT_REQUEST_FORWARDING:
			if (!options.allow_agent_forwarding ||
			    no_agent_forwarding_flag || compat13) {
				debug("Authentication agent forwarding not permitted for this authentication.");
				break;
			}
			debug("Received authentication agent forwarding request.");
			success = auth_input_request_forwarding(s->pw);
			break;

		case SSH_CMSG_PORT_FORWARD_REQUEST:
			if (no_port_forwarding_flag) {
				debug("Port forwarding not permitted for this authentication.");
				break;
			}
			if (!options.allow_tcp_forwarding) {
				debug("Port forwarding not permitted.");
				break;
			}
			debug("Received TCP/IP port forwarding request.");
			if (channel_input_port_forward_request(s->pw->pw_uid == 0,
			    options.gateway_ports) < 0) {
				debug("Port forwarding failed.");
				break;
			}
			success = 1;
			break;

		case SSH_CMSG_MAX_PACKET_SIZE:
			if (packet_set_maxsize(packet_get_int()) > 0)
				success = 1;
			break;

		case SSH_CMSG_EXEC_SHELL:
		case SSH_CMSG_EXEC_CMD:
			if (type == SSH_CMSG_EXEC_CMD) {
				command = packet_get_string(&dlen);
				debug("Exec command '%.500s'", command);
				if (do_exec(s, command) != 0)
					packet_disconnect(
					    "command execution failed");
				xfree(command);
			} else {
				if (do_exec(s, NULL) != 0)
					packet_disconnect(
					    "shell execution failed");
			}
			packet_check_eom();
			session_close(s);
			return;

		default:
			/*
			 * Any unknown messages in this phase are ignored,
			 * and a failure message is returned.
			 */
			logit("Unknown packet type received after authentication: %d", type);
		}
		packet_start(success ? SSH_SMSG_SUCCESS : SSH_SMSG_FAILURE);
		packet_send();
		packet_write_wait();

		/* Enable compression now that we have replied if appropriate. */
		if (enable_compression_after_reply) {
			enable_compression_after_reply = 0;
			packet_start_compression(compression_level);
		}
	}
}

#define USE_PIPES
/*
 * This is called to fork and execute a command when we have no tty.  This
 * will call do_child from the child, and server_loop from the parent after
 * setting up file descriptors and such.
 */
int
do_exec_no_pty(Session *s, const char *command)
{
	pid_t pid;

#ifdef USE_PIPES
	int pin[2], pout[2], perr[2];

	if (s == NULL)
		fatal("do_exec_no_pty: no session");

	/* Allocate pipes for communicating with the program. */
	if (pipe(pin) < 0) {
		error("%s: pipe in: %.100s", __func__, strerror(errno));
		return -1;
	}
	if (pipe(pout) < 0) {
		error("%s: pipe out: %.100s", __func__, strerror(errno));
		close(pin[0]);
		close(pin[1]);
		return -1;
	}
	if (pipe(perr) < 0) {
		error("%s: pipe err: %.100s", __func__,
		    strerror(errno));
		close(pin[0]);
		close(pin[1]);
		close(pout[0]);
		close(pout[1]);
		return -1;
	}
#else
	int inout[2], err[2];

	if (s == NULL)
		fatal("do_exec_no_pty: no session");

	/* Uses socket pairs to communicate with the program. */
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, inout) < 0) {
		error("%s: socketpair #1: %.100s", __func__, strerror(errno));
		return -1;
	}
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, err) < 0) {
		error("%s: socketpair #2: %.100s", __func__,
		    strerror(errno));
		close(inout[0]);
		close(inout[1]);
		return -1;
	}
#endif

	session_proctitle(s);

	/* Fork the child. */
	switch ((pid = fork())) {
	case -1:
		error("%s: fork: %.100s", __func__, strerror(errno));
#ifdef USE_PIPES
		close(pin[0]);
		close(pin[1]);
		close(pout[0]);
		close(pout[1]);
		close(perr[0]);
		close(perr[1]);
#else
		close(inout[0]);
		close(inout[1]);
		close(err[0]);
		close(err[1]);
#endif
		return -1;
	case 0:
		is_child = 1;

		/* Child.  Reinitialize the log since the pid has changed. */
		log_init(__progname, options.log_level,
		    options.log_facility, log_stderr);

		/*
		 * Create a new session and process group since the 4.4BSD
		 * setlogin() affects the entire process group.
		 */
		if (setsid() < 0)
			error("setsid failed: %.100s", strerror(errno));

#ifdef USE_PIPES
		/*
		 * Redirect stdin.  We close the parent side of the socket
		 * pair, and make the child side the standard input.
		 */
		close(pin[1]);
		if (dup2(pin[0], 0) < 0)
			perror("dup2 stdin");
		close(pin[0]);

		/* Redirect stdout. */
		close(pout[0]);
		if (dup2(pout[1], 1) < 0)
			perror("dup2 stdout");
		close(pout[1]);

		/* Redirect stderr. */
		close(perr[0]);
		if (dup2(perr[1], 2) < 0)
			perror("dup2 stderr");
		close(perr[1]);
#else
		/*
		 * Redirect stdin, stdout, and stderr.  Stdin and stdout will
		 * use the same socket, as some programs (particularly rdist)
		 * seem to depend on it.
		 */
		close(inout[1]);
		close(err[1]);
		if (dup2(inout[0], 0) < 0)	/* stdin */
			perror("dup2 stdin");
		if (dup2(inout[0], 1) < 0)	/* stdout (same as stdin) */
			perror("dup2 stdout");
		close(inout[0]);
		if (dup2(err[0], 2) < 0)	/* stderr */
			perror("dup2 stderr");
		close(err[0]);
#endif


#ifdef _UNICOS
		cray_init_job(s->pw); /* set up cray jid and tmpdir */
#endif

		/* Do processing for the child (exec command etc). */
		do_child(s, command);
		/* NOTREACHED */
	default:
		break;
	}

#ifdef _UNICOS
	signal(WJSIGNAL, cray_job_termination_handler);
#endif /* _UNICOS */
#ifdef HAVE_CYGWIN
	cygwin_set_impersonation_token(INVALID_HANDLE_VALUE);
#endif

	s->pid = pid;
	/* Set interactive/non-interactive mode. */
	packet_set_interactive(s->display != NULL);

	/*
	 * Clear loginmsg, since it's the child's responsibility to display
	 * it to the user, otherwise multiple sessions may accumulate
	 * multiple copies of the login messages.
	 */
	buffer_clear(&loginmsg);

#ifdef USE_PIPES
	/* We are the parent.  Close the child sides of the pipes. */
	close(pin[0]);
	close(pout[1]);
	close(perr[1]);

	if (compat20) {
		session_set_fds(s, pin[1], pout[0], perr[0],
		    s->is_subsystem, 0);
	} else {
		/* Enter the interactive session. */
		server_loop(pid, pin[1], pout[0], perr[0]);
		/* server_loop has closed pin[1], pout[0], and perr[0]. */
	}
#else
	/* We are the parent.  Close the child sides of the socket pairs. */
	close(inout[0]);
	close(err[0]);

	/*
	 * Enter the interactive session.  Note: server_loop must be able to
	 * handle the case that fdin and fdout are the same.
	 */
	if (compat20) {
		session_set_fds(s, inout[1], inout[1], err[1],
		    s->is_subsystem, 0);
	} else {
		server_loop(pid, inout[1], inout[1], err[1]);
		/* server_loop has closed inout[1] and err[1]. */
	}
#endif
	return 0;
}

/*
 * This is called to fork and execute a command when we have a tty.  This
 * will call do_child from the child, and server_loop from the parent after
 * setting up file descriptors, controlling tty, updating wtmp, utmp,
 * lastlog, and other such operations.
 */
int
do_exec_pty(Session *s, const char *command)
{
	int fdout, ptyfd, ttyfd, ptymaster;
	pid_t pid;

	if (s == NULL)
		fatal("do_exec_pty: no session");
	ptyfd = s->ptyfd;
	ttyfd = s->ttyfd;

	/*
	 * Create another descriptor of the pty master side for use as the
	 * standard input.  We could use the original descriptor, but this
	 * simplifies code in server_loop.  The descriptor is bidirectional.
	 * Do this before forking (and cleanup in the child) so as to
	 * detect and gracefully fail out-of-fd conditions.
	 */
	if ((fdout = dup(ptyfd)) < 0) {
		error("%s: dup #1: %s", __func__, strerror(errno));
		close(ttyfd);
		close(ptyfd);
		return -1;
	}
	/* we keep a reference to the pty master */
	if ((ptymaster = dup(ptyfd)) < 0) {
		error("%s: dup #2: %s", __func__, strerror(errno));
		close(ttyfd);
		close(ptyfd);
		close(fdout);
		return -1;
	}

	/* Fork the child. */
	switch ((pid = fork())) {
	case -1:
		error("%s: fork: %.100s", __func__, strerror(errno));
		close(fdout);
		close(ptymaster);
		close(ttyfd);
		close(ptyfd);
		return -1;
	case 0:
		is_child = 1;

		close(fdout);
		close(ptymaster);

		/* Child.  Reinitialize the log because the pid has changed. */
		log_init(__progname, options.log_level,
		    options.log_facility, log_stderr);
		/* Close the master side of the pseudo tty. */
		close(ptyfd);

		/* Make the pseudo tty our controlling tty. */
		pty_make_controlling_tty(&ttyfd, s->tty);

		/* Redirect stdin/stdout/stderr from the pseudo tty. */
		if (dup2(ttyfd, 0) < 0)
			error("dup2 stdin: %s", strerror(errno));
		if (dup2(ttyfd, 1) < 0)
			error("dup2 stdout: %s", strerror(errno));
		if (dup2(ttyfd, 2) < 0)
			error("dup2 stderr: %s", strerror(errno));

		/* Close the extra descriptor for the pseudo tty. */
		close(ttyfd);

		/* record login, etc. similar to login(1) */
#ifndef HAVE_OSF_SIA
		if (!(options.use_login && command == NULL)) {
#ifdef _UNICOS
			cray_init_job(s->pw); /* set up cray jid and tmpdir */
#endif /* _UNICOS */
			do_login(s, command);
		}
# ifdef LOGIN_NEEDS_UTMPX
		else
			do_pre_login(s);
# endif
#endif
		/*
		 * Do common processing for the child, such as execing
		 * the command.
		 */
		do_child(s, command);
		/* NOTREACHED */
	default:
		break;
	}

#ifdef _UNICOS
	signal(WJSIGNAL, cray_job_termination_handler);
#endif /* _UNICOS */
#ifdef HAVE_CYGWIN
	cygwin_set_impersonation_token(INVALID_HANDLE_VALUE);
#endif

	s->pid = pid;

	/* Parent.  Close the slave side of the pseudo tty. */
	close(ttyfd);

	/* Enter interactive session. */
	s->ptymaster = ptymaster;
	packet_set_interactive(1);
	if (compat20) {
		session_set_fds(s, ptyfd, fdout, -1, 1, 1);
	} else {
		server_loop(pid, ptyfd, fdout, -1);
		/* server_loop _has_ closed ptyfd and fdout. */
	}
	return 0;
}

#ifdef LOGIN_NEEDS_UTMPX
static void
do_pre_login(Session *s)
{
	socklen_t fromlen;
	struct sockaddr_storage from;
	pid_t pid = getpid();

	/*
	 * Get IP address of client. If the connection is not a socket, let
	 * the address be 0.0.0.0.
	 */
	memset(&from, 0, sizeof(from));
	fromlen = sizeof(from);
	if (packet_connection_is_on_socket()) {
		if (getpeername(packet_get_connection_in(),
		    (struct sockaddr *)&from, &fromlen) < 0) {
			debug("getpeername: %.100s", strerror(errno));
			cleanup_exit(255);
		}
	}

	record_utmp_only(pid, s->tty, s->pw->pw_name,
	    get_remote_name_or_ip(utmp_len, options.use_dns),
	    (struct sockaddr *)&from, fromlen);
}
#endif

/*
 * This is called to fork and execute a command.  If another command is
 * to be forced, execute that instead.
 */
int
do_exec(Session *s, const char *command)
{
	int ret;

	if (options.adm_forced_command) {
		original_command = command;
		command = options.adm_forced_command;
		if (IS_INTERNAL_SFTP(command)) {
			s->is_subsystem = s->is_subsystem ?
			    SUBSYSTEM_INT_SFTP : SUBSYSTEM_INT_SFTP_ERROR;
		} else if (s->is_subsystem)
			s->is_subsystem = SUBSYSTEM_EXT;
		debug("Forced command (config) '%.900s'", command);
	} else if (forced_command) {
		original_command = command;
		command = forced_command;
		if (IS_INTERNAL_SFTP(command)) {
			s->is_subsystem = s->is_subsystem ?
			    SUBSYSTEM_INT_SFTP : SUBSYSTEM_INT_SFTP_ERROR;
		} else if (s->is_subsystem)
			s->is_subsystem = SUBSYSTEM_EXT;
		debug("Forced command (key option) '%.900s'", command);
	}

#ifdef SSH_AUDIT_EVENTS
	if (command != NULL)
		PRIVSEP(audit_run_command(command));
	else if (s->ttyfd == -1) {
		char *shell = s->pw->pw_shell;

		if (shell[0] == '\0')	/* empty shell means /bin/sh */
			shell =_PATH_BSHELL;
		PRIVSEP(audit_run_command(shell));
	}
#endif
	if (s->ttyfd != -1)
		ret = do_exec_pty(s, command);
	else
		ret = do_exec_no_pty(s, command);

	original_command = NULL;

	/*
	 * Clear loginmsg: it's the child's responsibility to display
	 * it to the user, otherwise multiple sessions may accumulate
	 * multiple copies of the login messages.
	 */
	buffer_clear(&loginmsg);

	return ret;
}

/* administrative, login(1)-like work */
void
do_login(Session *s, const char *command)
{
	socklen_t fromlen;
	struct sockaddr_storage from;
	struct passwd * pw = s->pw;
	pid_t pid = getpid();

	/*
	 * Get IP address of client. If the connection is not a socket, let
	 * the address be 0.0.0.0.
	 */
	memset(&from, 0, sizeof(from));
	fromlen = sizeof(from);
	if (packet_connection_is_on_socket()) {
		if (getpeername(packet_get_connection_in(),
		    (struct sockaddr *)&from, &fromlen) < 0) {
			debug("getpeername: %.100s", strerror(errno));
			cleanup_exit(255);
		}
	}

	/* Record that there was a login on that tty from the remote host. */
	if (!use_privsep)
		record_login(pid, s->tty, pw->pw_name, pw->pw_uid,
		    get_remote_name_or_ip(utmp_len,
		    options.use_dns),
		    (struct sockaddr *)&from, fromlen);

#ifdef USE_PAM
	/*
	 * If password change is needed, do it now.
	 * This needs to occur before the ~/.hushlogin check.
	 */
	if (options.use_pam && !use_privsep && s->authctxt->force_pwchange) {
		display_loginmsg();
		do_pam_chauthtok();
		s->authctxt->force_pwchange = 0;
		/* XXX - signal [net] parent to enable forwardings */
	}
#endif

	if (check_quietlogin(s, command))
		return;

	display_loginmsg();

	do_motd();
}

/*
 * Display the message of the day.
 */
void
do_motd(void)
{
	FILE *f;
	char buf[256];

	if (options.print_motd) {
#ifdef HAVE_LOGIN_CAP
		f = fopen(login_getcapstr(lc, "welcome", "/etc/motd",
		    "/etc/motd"), "r");
#else
		f = fopen("/etc/motd", "r");
#endif
		if (f) {
			while (fgets(buf, sizeof(buf), f))
				fputs(buf, stdout);
			fclose(f);
		}
	}
}


/*
 * Check for quiet login, either .hushlogin or command given.
 */
int
check_quietlogin(Session *s, const char *command)
{
	char buf[256];
	struct passwd *pw = s->pw;
	struct stat st;

	/* Return 1 if .hushlogin exists or a command given. */
	if (command != NULL)
		return 1;
	snprintf(buf, sizeof(buf), "%.200s/.hushlogin", pw->pw_dir);
#ifdef HAVE_LOGIN_CAP
	if (login_getcapbool(lc, "hushlogin", 0) || stat(buf, &st) >= 0)
		return 1;
#else
	if (stat(buf, &st) >= 0)
		return 1;
#endif
	return 0;
}

/*
 * Sets the value of the given variable in the environment.  If the variable
 * already exists, its value is overridden.
 */
void
child_set_env(char ***envp, u_int *envsizep, const char *name,
	const char *value)
{
	char **env;
	u_int envsize;
	u_int i, namelen;

	/*
	 * If we're passed an uninitialized list, allocate a single null
	 * entry before continuing.
	 */
	if (*envp == NULL && *envsizep == 0) {
		*envp = xmalloc(sizeof(char *));
		*envp[0] = NULL;
		*envsizep = 1;
	}

	/*
	 * Find the slot where the value should be stored.  If the variable
	 * already exists, we reuse the slot; otherwise we append a new slot
	 * at the end of the array, expanding if necessary.
	 */
	env = *envp;
	namelen = strlen(name);
	for (i = 0; env[i]; i++)
		if (strncmp(env[i], name, namelen) == 0 && env[i][namelen] == '=')
			break;
	if (env[i]) {
		/* Reuse the slot. */
		xfree(env[i]);
	} else {
		/* New variable.  Expand if necessary. */
		envsize = *envsizep;
		if (i >= envsize - 1) {
			if (envsize >= 1000)
				fatal("child_set_env: too many env vars");
			envsize += 50;
			env = (*envp) = xrealloc(env, envsize, sizeof(char *));
			*envsizep = envsize;
		}
		/* Need to set the NULL pointer at end of array beyond the new slot. */
		env[i + 1] = NULL;
	}

	/* Allocate space and format the variable in the appropriate slot. */
	env[i] = xmalloc(strlen(name) + 1 + strlen(value) + 1);
	snprintf(env[i], strlen(name) + 1 + strlen(value) + 1, "%s=%s", name, value);
}

/*
 * Reads environment variables from the given file and adds/overrides them
 * into the environment.  If the file does not exist, this does nothing.
 * Otherwise, it must consist of empty lines, comments (line starts with '#')
 * and assignments of the form name=value.  No other forms are allowed.
 */
static void
read_environment_file(char ***env, u_int *envsize,
	const char *filename)
{
	FILE *f;
	char buf[4096];
	char *cp, *value;
	u_int lineno = 0;

	f = fopen(filename, "r");
	if (!f)
		return;

	while (fgets(buf, sizeof(buf), f)) {
		if (++lineno > 1000)
			fatal("Too many lines in environment file %s", filename);
		for (cp = buf; *cp == ' ' || *cp == '\t'; cp++)
			;
		if (!*cp || *cp == '#' || *cp == '\n')
			continue;

		cp[strcspn(cp, "\n")] = '\0';

		value = strchr(cp, '=');
		if (value == NULL) {
			fprintf(stderr, "Bad line %u in %.100s\n", lineno,
			    filename);
			continue;
		}
		/*
		 * Replace the equals sign by nul, and advance value to
		 * the value string.
		 */
		*value = '\0';
		value++;
		child_set_env(env, envsize, cp, value);
	}
	fclose(f);
}

#ifdef HAVE_ETC_DEFAULT_LOGIN
/*
 * Return named variable from specified environment, or NULL if not present.
 */
static char *
child_get_env(char **env, const char *name)
{
	int i;
	size_t len;

	len = strlen(name);
	for (i=0; env[i] != NULL; i++)
		if (strncmp(name, env[i], len) == 0 && env[i][len] == '=')
			return(env[i] + len + 1);
	return NULL;
}

/*
 * Read /etc/default/login.
 * We pick up the PATH (or SUPATH for root) and UMASK.
 */
static void
read_etc_default_login(char ***env, u_int *envsize, uid_t uid)
{
	char **tmpenv = NULL, *var;
	u_int i, tmpenvsize = 0;
	u_long mask;

	/*
	 * We don't want to copy the whole file to the child's environment,
	 * so we use a temporary environment and copy the variables we're
	 * interested in.
	 */
	read_environment_file(&tmpenv, &tmpenvsize, "/etc/default/login");

	if (tmpenv == NULL)
		return;

	if (uid == 0)
		var = child_get_env(tmpenv, "SUPATH");
	else
		var = child_get_env(tmpenv, "PATH");
	if (var != NULL)
		child_set_env(env, envsize, "PATH", var);

	if ((var = child_get_env(tmpenv, "UMASK")) != NULL)
		if (sscanf(var, "%5lo", &mask) == 1)
			umask((mode_t)mask);

	for (i = 0; tmpenv[i] != NULL; i++)
		xfree(tmpenv[i]);
	xfree(tmpenv);
}
#endif /* HAVE_ETC_DEFAULT_LOGIN */

void
copy_environment(char **source, char ***env, u_int *envsize)
{
	char *var_name, *var_val;
	int i;

	if (source == NULL)
		return;

	for(i = 0; source[i] != NULL; i++) {
		var_name = xstrdup(source[i]);
		if ((var_val = strstr(var_name, "=")) == NULL) {
			xfree(var_name);
			continue;
		}
		*var_val++ = '\0';

		debug3("Copy environment: %s=%s", var_name, var_val);
		child_set_env(env, envsize, var_name, var_val);

		xfree(var_name);
	}
}

static char **
do_setup_env(Session *s, const char *shell)
{
	char buf[256];
	u_int i, envsize;
	char **env, *laddr;
	struct passwd *pw = s->pw;
#if !defined (HAVE_LOGIN_CAP) && !defined (HAVE_CYGWIN)
	char *path = NULL;
#else
	extern char **environ;
	char **senv, **var;
#endif

	/* Initialize the environment. */
	envsize = 100;
	env = xcalloc(envsize, sizeof(char *));
	env[0] = NULL;

#ifdef HAVE_CYGWIN
	/*
	 * The Windows environment contains some setting which are
	 * important for a running system. They must not be dropped.
	 */
	{
		char **p;

		p = fetch_windows_environment();
		copy_environment(p, &env, &envsize);
		free_windows_environment(p);
	}
#endif

	if (getenv("TZ"))
		child_set_env(&env, &envsize, "TZ", getenv("TZ"));

#ifdef GSSAPI
	/* Allow any GSSAPI methods that we've used to alter
	 * the childs environment as they see fit
	 */
	ssh_gssapi_do_child(&env, &envsize);
#endif

	if (!options.use_login) {
		/* Set basic environment. */
		for (i = 0; i < s->num_env; i++)
			child_set_env(&env, &envsize, s->env[i].name,
			    s->env[i].val);

		child_set_env(&env, &envsize, "USER", pw->pw_name);
		child_set_env(&env, &envsize, "LOGNAME", pw->pw_name);
#ifdef _AIX
		child_set_env(&env, &envsize, "LOGIN", pw->pw_name);
#endif
		child_set_env(&env, &envsize, "HOME", pw->pw_dir);
		snprintf(buf, sizeof buf, "%.200s/%.50s",
			 _PATH_MAILDIR, pw->pw_name);
		child_set_env(&env, &envsize, "MAIL", buf);
#ifdef HAVE_LOGIN_CAP
		child_set_env(&env, &envsize, "PATH", _PATH_STDPATH);
		child_set_env(&env, &envsize, "TERM", "su");
		senv = environ;
		environ = xmalloc(sizeof(char *));
		*environ = NULL;
		(void) setusercontext(lc, pw, pw->pw_uid,
		    LOGIN_SETENV|LOGIN_SETPATH);
		copy_environment(environ, &env, &envsize);
		for (var = environ; *var != NULL; ++var)
			xfree(*var);
		xfree(environ);
		environ = senv;
#else /* HAVE_LOGIN_CAP */
# ifndef HAVE_CYGWIN
		/*
		 * There's no standard path on Windows. The path contains
		 * important components pointing to the system directories,
		 * needed for loading shared libraries. So the path better
		 * remains intact here.
		 */
#  ifdef HAVE_ETC_DEFAULT_LOGIN
		read_etc_default_login(&env, &envsize, pw->pw_uid);
		path = child_get_env(env, "PATH");
#  endif /* HAVE_ETC_DEFAULT_LOGIN */
		if (path == NULL || *path == '\0') {
			child_set_env(&env, &envsize, "PATH",
			    s->pw->pw_uid == 0 ?
				SUPERUSER_PATH : _PATH_STDPATH);
		}
# endif /* HAVE_CYGWIN */
#endif /* HAVE_LOGIN_CAP */

		/* Normal systems set SHELL by default. */
		child_set_env(&env, &envsize, "SHELL", shell);
	}

	/* Set custom environment options from RSA authentication. */
	if (!options.use_login) {
		while (custom_environment) {
			struct envstring *ce = custom_environment;
			char *str = ce->s;

			for (i = 0; str[i] != '=' && str[i]; i++)
				;
			if (str[i] == '=') {
				str[i] = 0;
				child_set_env(&env, &envsize, str, str + i + 1);
			}
			custom_environment = ce->next;
			xfree(ce->s);
			xfree(ce);
		}
	}

	/* SSH_CLIENT deprecated */
	snprintf(buf, sizeof buf, "%.50s %d %d",
	    get_remote_ipaddr(), get_remote_port(), get_local_port());
	child_set_env(&env, &envsize, "SSH_CLIENT", buf);

	laddr = get_local_ipaddr(packet_get_connection_in());
	snprintf(buf, sizeof buf, "%.50s %d %.50s %d",
	    get_remote_ipaddr(), get_remote_port(), laddr, get_local_port());
	xfree(laddr);
	child_set_env(&env, &envsize, "SSH_CONNECTION", buf);

	if (s->ttyfd != -1)
		child_set_env(&env, &envsize, "SSH_TTY", s->tty);
	if (s->term)
		child_set_env(&env, &envsize, "TERM", s->term);
	if (s->display)
		child_set_env(&env, &envsize, "DISPLAY", s->display);
	if (original_command)
		child_set_env(&env, &envsize, "SSH_ORIGINAL_COMMAND",
		    original_command);

#ifdef _UNICOS
	if (cray_tmpdir[0] != '\0')
		child_set_env(&env, &envsize, "TMPDIR", cray_tmpdir);
#endif /* _UNICOS */

	/*
	 * Since we clear KRB5CCNAME at startup, if it's set now then it
	 * must have been set by a native authentication method (eg AIX or
	 * SIA), so copy it to the child.
	 */
	{
		char *cp;

		if ((cp = getenv("KRB5CCNAME")) != NULL)
			child_set_env(&env, &envsize, "KRB5CCNAME", cp);
	}

#ifdef _AIX
	{
		char *cp;

		if ((cp = getenv("AUTHSTATE")) != NULL)
			child_set_env(&env, &envsize, "AUTHSTATE", cp);
		read_environment_file(&env, &envsize, "/etc/environment");
	}
#endif
#ifdef KRB5
	if (s->authctxt->krb5_ccname)
		child_set_env(&env, &envsize, "KRB5CCNAME",
		    s->authctxt->krb5_ccname);
#endif
#ifdef USE_PAM
	/*
	 * Pull in any environment variables that may have
	 * been set by PAM.
	 */
	if (options.use_pam) {
		char **p;

		p = fetch_pam_child_environment();
		copy_environment(p, &env, &envsize);
		free_pam_environment(p);

		p = fetch_pam_environment();
		copy_environment(p, &env, &envsize);
		free_pam_environment(p);
	}
#endif /* USE_PAM */

	if (auth_sock_name != NULL)
		child_set_env(&env, &envsize, SSH_AUTHSOCKET_ENV_NAME,
		    auth_sock_name);

	/* read $HOME/.ssh/environment. */
	if (options.permit_user_env && !options.use_login) {
		snprintf(buf, sizeof buf, "%.200s/.ssh/environment",
		    strcmp(pw->pw_dir, "/") ? pw->pw_dir : "");
		read_environment_file(&env, &envsize, buf);
	}
	if (debug_flag) {
		/* dump the environment */
		fprintf(stderr, "Environment:\n");
		for (i = 0; env[i]; i++)
			fprintf(stderr, "  %.200s\n", env[i]);
	}
	return env;
}

/*
 * Run $HOME/.ssh/rc, /etc/ssh/sshrc, or xauth (whichever is found
 * first in this order).
 */
static void
do_rc_files(Session *s, const char *shell)
{
	FILE *f = NULL;
	char cmd[1024];
	int do_xauth;
	struct stat st;

	do_xauth =
	    s->display != NULL && s->auth_proto != NULL && s->auth_data != NULL;

	/* ignore _PATH_SSH_USER_RC for subsystems and admin forced commands */
	if (!s->is_subsystem && options.adm_forced_command == NULL &&
	    !no_user_rc && stat(_PATH_SSH_USER_RC, &st) >= 0) {
		snprintf(cmd, sizeof cmd, "%s -c '%s %s'",
		    shell, _PATH_BSHELL, _PATH_SSH_USER_RC);
		if (debug_flag)
			fprintf(stderr, "Running %s\n", cmd);
		f = popen(cmd, "w");
		if (f) {
			if (do_xauth)
				fprintf(f, "%s %s\n", s->auth_proto,
				    s->auth_data);
			pclose(f);
		} else
			fprintf(stderr, "Could not run %s\n",
			    _PATH_SSH_USER_RC);
	} else if (stat(_PATH_SSH_SYSTEM_RC, &st) >= 0) {
		if (debug_flag)
			fprintf(stderr, "Running %s %s\n", _PATH_BSHELL,
			    _PATH_SSH_SYSTEM_RC);
		f = popen(_PATH_BSHELL " " _PATH_SSH_SYSTEM_RC, "w");
		if (f) {
			if (do_xauth)
				fprintf(f, "%s %s\n", s->auth_proto,
				    s->auth_data);
			pclose(f);
		} else
			fprintf(stderr, "Could not run %s\n",
			    _PATH_SSH_SYSTEM_RC);
	} else if (do_xauth && options.xauth_location != NULL) {
		/* Add authority data to .Xauthority if appropriate. */
		if (debug_flag) {
			fprintf(stderr,
			    "Running %.500s remove %.100s\n",
			    options.xauth_location, s->auth_display);
			fprintf(stderr,
			    "%.500s add %.100s %.100s %.100s\n",
			    options.xauth_location, s->auth_display,
			    s->auth_proto, s->auth_data);
		}
		snprintf(cmd, sizeof cmd, "%s -q -",
		    options.xauth_location);
		f = popen(cmd, "w");
		if (f) {
			fprintf(f, "remove %s\n",
			    s->auth_display);
			fprintf(f, "add %s %s %s\n",
			    s->auth_display, s->auth_proto,
			    s->auth_data);
			pclose(f);
		} else {
			fprintf(stderr, "Could not run %s\n",
			    cmd);
		}
	}
}

static void
do_nologin(struct passwd *pw)
{
	FILE *f = NULL;
	char buf[1024], *nl, *def_nl = _PATH_NOLOGIN;
	struct stat sb;

#ifdef HAVE_LOGIN_CAP
	if (login_getcapbool(lc, "ignorenologin", 0) && pw->pw_uid)
		return;
	nl = login_getcapstr(lc, "nologin", def_nl, def_nl);
#else
	if (pw->pw_uid == 0)
		return;
	nl = def_nl;
#endif
	if (stat(nl, &sb) == -1) {
		if (nl != def_nl)
			xfree(nl);
		return;
	}

	/* /etc/nologin exists.  Print its contents if we can and exit. */
	logit("User %.100s not allowed because %s exists", pw->pw_name, nl);
	if ((f = fopen(nl, "r")) != NULL) {
 		while (fgets(buf, sizeof(buf), f))
 			fputs(buf, stderr);
 		fclose(f);
 	}
	exit(254);
}

/*
 * Chroot into a directory after checking it for safety: all path components
 * must be root-owned directories with strict permissions.
 */
static void
safely_chroot(const char *path, uid_t uid)
{
	const char *cp;
	char component[MAXPATHLEN];
	struct stat st;

	if (*path != '/')
		fatal("chroot path does not begin at root");
	if (strlen(path) >= sizeof(component))
		fatal("chroot path too long");

	/*
	 * Descend the path, checking that each component is a
	 * root-owned directory with strict permissions.
	 */
	for (cp = path; cp != NULL;) {
		if ((cp = strchr(cp, '/')) == NULL)
			strlcpy(component, path, sizeof(component));
		else {
			cp++;
			memcpy(component, path, cp - path);
			component[cp - path] = '\0';
		}
	
		debug3("%s: checking '%s'", __func__, component);

		if (stat(component, &st) != 0)
			fatal("%s: stat(\"%s\"): %s", __func__,
			    component, strerror(errno));
		if (st.st_uid != 0 || (st.st_mode & 022) != 0)
			fatal("bad ownership or modes for chroot "
			    "directory %s\"%s\"", 
			    cp == NULL ? "" : "component ", component);
		if (!S_ISDIR(st.st_mode))
			fatal("chroot path %s\"%s\" is not a directory",
			    cp == NULL ? "" : "component ", component);

	}

	if (chdir(path) == -1)
		fatal("Unable to chdir to chroot path \"%s\": "
		    "%s", path, strerror(errno));
	if (chroot(path) == -1)
		fatal("chroot(\"%s\"): %s", path, strerror(errno));
	if (chdir("/") == -1)
		fatal("%s: chdir(/) after chroot: %s",
		    __func__, strerror(errno));
	verbose("Changed root directory to \"%s\"", path);
}

/* Set login name, uid, gid, and groups. */
void
do_setusercontext(struct passwd *pw)
{
	char *chroot_path, *tmp;

#ifdef WITH_SELINUX
	/* Cache selinux status for later use */
	(void)ssh_selinux_enabled();
#endif

#ifndef HAVE_CYGWIN
	if (getuid() == 0 || geteuid() == 0)
#endif /* HAVE_CYGWIN */
	{
#ifdef HAVE_LOGIN_CAP
# ifdef __bsdi__
		setpgid(0, 0);
# endif
# ifdef USE_PAM
		if (options.use_pam) {
			do_pam_setcred(use_privsep);
		}
# endif /* USE_PAM */
		if (setusercontext(lc, pw, pw->pw_uid,
		    (LOGIN_SETALL & ~(LOGIN_SETENV|LOGIN_SETPATH|LOGIN_SETUSER))) < 0) {
			perror("unable to set user context");
			exit(1);
		}
#else
# if defined(HAVE_GETLUID) && defined(HAVE_SETLUID)
		/* Sets login uid for accounting */
		if (getluid() == -1 && setluid(pw->pw_uid) == -1)
			error("setluid: %s", strerror(errno));
# endif /* defined(HAVE_GETLUID) && defined(HAVE_SETLUID) */

		if (setlogin(pw->pw_name) < 0)
			error("setlogin failed: %s", strerror(errno));
		if (setgid(pw->pw_gid) < 0) {
			perror("setgid");
			exit(1);
		}
		/* Initialize the group list. */
		if (initgroups(pw->pw_name, pw->pw_gid) < 0) {
			perror("initgroups");
			exit(1);
		}
		endgrent();
# ifdef USE_PAM
		/*
		 * PAM credentials may take the form of supplementary groups.
		 * These will have been wiped by the above initgroups() call.
		 * Reestablish them here.
		 */
		if (options.use_pam) {
			do_pam_setcred(use_privsep);
		}
# endif /* USE_PAM */
# if defined(WITH_IRIX_PROJECT) || defined(WITH_IRIX_JOBS) || defined(WITH_IRIX_ARRAY)
		irix_setusercontext(pw);
# endif /* defined(WITH_IRIX_PROJECT) || defined(WITH_IRIX_JOBS) || defined(WITH_IRIX_ARRAY) */
# ifdef _AIX
		aix_usrinfo(pw);
# endif /* _AIX */
# ifdef USE_LIBIAF
		if (set_id(pw->pw_name) != 0) {
			exit(1);
		}
# endif /* USE_LIBIAF */
#endif
#ifdef HAVE_SETPCRED
		/*
		 * If we have a chroot directory, we set all creds except real
		 * uid which we will need for chroot.  If we don't have a
		 * chroot directory, we don't override anything.
		 */
		{
			char **creds = NULL, *chroot_creds[] =
			    { "REAL_USER=root", NULL };

			if (options.chroot_directory != NULL &&
			    strcasecmp(options.chroot_directory, "none") != 0)
				creds = chroot_creds;

			if (setpcred(pw->pw_name, creds) == -1)
				fatal("Failed to set process credentials");
		}
#endif /* HAVE_SETPCRED */

#ifdef WITH_SELINUX
		ssh_selinux_setup_exec_context(pw->pw_name);
#endif

		if (options.chroot_directory != NULL &&
		    strcasecmp(options.chroot_directory, "none") != 0) {
                        tmp = tilde_expand_filename(options.chroot_directory,
			    pw->pw_uid);
			chroot_path = percent_expand(tmp, "h", pw->pw_dir,
			    "u", pw->pw_name, (char *)NULL);
			safely_chroot(chroot_path, pw->pw_uid);
			free(tmp);
			free(chroot_path);
		}

#ifdef HAVE_LOGIN_CAP
		if (setusercontext(lc, pw, pw->pw_uid, LOGIN_SETUSER) < 0) {
			perror("unable to set user context (setuser)");
			exit(1);
		}
#else
		/* Permanently switch to the desired uid. */
		permanently_set_uid(pw);
#endif
	}

	if (getuid() != pw->pw_uid || geteuid() != pw->pw_uid)
		fatal("Failed to set uids to %u.", (u_int) pw->pw_uid);
}

static void
do_pwchange(Session *s)
{
	fflush(NULL);
	fprintf(stderr, "WARNING: Your password has expired.\n");
	if (s->ttyfd != -1) {
		fprintf(stderr,
		    "You must change your password now and login again!\n");
#ifdef PASSWD_NEEDS_USERNAME
		execl(_PATH_PASSWD_PROG, "passwd", s->pw->pw_name,
		    (char *)NULL);
#else
		execl(_PATH_PASSWD_PROG, "passwd", (char *)NULL);
#endif
		perror("passwd");
	} else {
		fprintf(stderr,
		    "Password change required but no TTY available.\n");
	}
	exit(1);
}

static void
launch_login(struct passwd *pw, const char *hostname)
{
	/* Launch login(1). */

	execl(LOGIN_PROGRAM, "login", "-h", hostname,
#ifdef xxxLOGIN_NEEDS_TERM
		    (s->term ? s->term : "unknown"),
#endif /* LOGIN_NEEDS_TERM */
#ifdef LOGIN_NO_ENDOPT
	    "-p", "-f", pw->pw_name, (char *)NULL);
#else
	    "-p", "-f", "--", pw->pw_name, (char *)NULL);
#endif

	/* Login couldn't be executed, die. */

	perror("login");
	exit(1);
}

static void
child_close_fds(void)
{
	int i;

	if (packet_get_connection_in() == packet_get_connection_out())
		close(packet_get_connection_in());
	else {
		close(packet_get_connection_in());
		close(packet_get_connection_out());
	}
	/*
	 * Close all descriptors related to channels.  They will still remain
	 * open in the parent.
	 */
	/* XXX better use close-on-exec? -markus */
	channel_close_all();

	/*
	 * Close any extra file descriptors.  Note that there may still be
	 * descriptors left by system functions.  They will be closed later.
	 */
	endpwent();

	/*
	 * Close any extra open file descriptors so that we don't have them
	 * hanging around in clients.  Note that we want to do this after
	 * initgroups, because at least on Solaris 2.3 it leaves file
	 * descriptors open.
	 */
	for (i = 3; i < 64; i++)
		close(i);
}

/*
 * Performs common processing for the child, such as setting up the
 * environment, closing extra file descriptors, setting the user and group
 * ids, and executing the command or shell.
 */
#define ARGV_MAX 10
void
do_child(Session *s, const char *command)
{
	extern char **environ;
	char **env;
	char *argv[ARGV_MAX];
	const char *shell, *shell0, *hostname = NULL;
	struct passwd *pw = s->pw;
	int r = 0;

	/* remove hostkey from the child's memory */
	destroy_sensitive_data();

	/* Force a password change */
	if (s->authctxt->force_pwchange) {
		do_setusercontext(pw);
		child_close_fds();
		do_pwchange(s);
		exit(1);
	}

	/* login(1) is only called if we execute the login shell */
	if (options.use_login && command != NULL)
		options.use_login = 0;

#ifdef _UNICOS
	cray_setup(pw->pw_uid, pw->pw_name, command);
#endif /* _UNICOS */

	/*
	 * Login(1) does this as well, and it needs uid 0 for the "-h"
	 * switch, so we let login(1) to this for us.
	 */
	if (!options.use_login) {
#ifdef HAVE_OSF_SIA
		session_setup_sia(pw, s->ttyfd == -1 ? NULL : s->tty);
		if (!check_quietlogin(s, command))
			do_motd();
#else /* HAVE_OSF_SIA */
		/* When PAM is enabled we rely on it to do the nologin check */
		if (!options.use_pam)
			do_nologin(pw);
		do_setusercontext(pw);
		/*
		 * PAM session modules in do_setusercontext may have
		 * generated messages, so if this in an interactive
		 * login then display them too.
		 */
		if (!check_quietlogin(s, command))
			display_loginmsg();
#endif /* HAVE_OSF_SIA */
	}

#ifdef USE_PAM
	if (options.use_pam && !options.use_login && !is_pam_session_open()) {
		debug3("PAM session not opened, exiting");
		display_loginmsg();
		exit(254);
	}
#endif

	/*
	 * Get the shell from the password data.  An empty shell field is
	 * legal, and means /bin/sh.
	 */
	shell = (pw->pw_shell[0] == '\0') ? _PATH_BSHELL : pw->pw_shell;

	/*
	 * Make sure $SHELL points to the shell from the password file,
	 * even if shell is overridden from login.conf
	 */
	env = do_setup_env(s, shell);

#ifdef HAVE_LOGIN_CAP
	shell = login_getcapstr(lc, "shell", (char *)shell, (char *)shell);
#endif

	/* we have to stash the hostname before we close our socket. */
	if (options.use_login)
		hostname = get_remote_name_or_ip(utmp_len,
		    options.use_dns);
	/*
	 * Close the connection descriptors; note that this is the child, and
	 * the server will still have the socket open, and it is important
	 * that we do not shutdown it.  Note that the descriptors cannot be
	 * closed before building the environment, as we call
	 * get_remote_ipaddr there.
	 */
	child_close_fds();

	/*
	 * Must take new environment into use so that .ssh/rc,
	 * /etc/ssh/sshrc and xauth are run in the proper environment.
	 */
	environ = env;

#if defined(KRB5) && defined(USE_AFS)
	/*
	 * At this point, we check to see if AFS is active and if we have
	 * a valid Kerberos 5 TGT. If so, it seems like a good idea to see
	 * if we can (and need to) extend the ticket into an AFS token. If
	 * we don't do this, we run into potential problems if the user's
	 * home directory is in AFS and it's not world-readable.
	 */

	if (options.kerberos_get_afs_token && k_hasafs() &&
	    (s->authctxt->krb5_ctx != NULL)) {
		char cell[64];

		debug("Getting AFS token");

		k_setpag();

		if (k_afs_cell_of_file(pw->pw_dir, cell, sizeof(cell)) == 0)
			krb5_afslog(s->authctxt->krb5_ctx,
			    s->authctxt->krb5_fwd_ccache, cell, NULL);

		krb5_afslog_home(s->authctxt->krb5_ctx,
		    s->authctxt->krb5_fwd_ccache, NULL, NULL, pw->pw_dir);
	}
#endif

	/* Change current directory to the user's home directory. */
	if (chdir(pw->pw_dir) < 0) {
		/* Suppress missing homedir warning for chroot case */
#ifdef HAVE_LOGIN_CAP
		r = login_getcapbool(lc, "requirehome", 0);
#endif
		if (r || options.chroot_directory == NULL ||
		    strcasecmp(options.chroot_directory, "none") == 0)
			fprintf(stderr, "Could not chdir to home "
			    "directory %s: %s\n", pw->pw_dir,
			    strerror(errno));
		if (r)
			exit(1);
	}

	closefrom(STDERR_FILENO + 1);

	if (!options.use_login)
		do_rc_files(s, shell);

	/* restore SIGPIPE for child */
	signal(SIGPIPE, SIG_DFL);

	if (s->is_subsystem == SUBSYSTEM_INT_SFTP_ERROR) {
		printf("This service allows sftp connections only.\n");
		fflush(NULL);
		exit(1);
	} else if (s->is_subsystem == SUBSYSTEM_INT_SFTP) {
		extern int optind, optreset;
		int i;
		char *p, *args;

		setproctitle("%s@%s", s->pw->pw_name, INTERNAL_SFTP_NAME);
		args = xstrdup(command ? command : "sftp-server");
		for (i = 0, (p = strtok(args, " ")); p; (p = strtok(NULL, " ")))
			if (i < ARGV_MAX - 1)
				argv[i++] = p;
		argv[i] = NULL;
		optind = optreset = 1;
		__progname = argv[0];
#ifdef WITH_SELINUX
		ssh_selinux_change_context("sftpd_t");
#endif
		exit(sftp_server_main(i, argv, s->pw));
	}

	fflush(NULL);

	if (options.use_login) {
		launch_login(pw, hostname);
		/* NEVERREACHED */
	}

	/* Get the last component of the shell name. */
	if ((shell0 = strrchr(shell, '/')) != NULL)
		shell0++;
	else
		shell0 = shell;

	/*
	 * If we have no command, execute the shell.  In this case, the shell
	 * name to be passed in argv[0] is preceded by '-' to indicate that
	 * this is a login shell.
	 */
	if (!command) {
		char argv0[256];

		/* Start the shell.  Set initial character to '-'. */
		argv0[0] = '-';

		if (strlcpy(argv0 + 1, shell0, sizeof(argv0) - 1)
		    >= sizeof(argv0) - 1) {
			errno = EINVAL;
			perror(shell);
			exit(1);
		}

		/* Execute the shell. */
		argv[0] = argv0;
		argv[1] = NULL;
		execve(shell, argv, env);

		/* Executing the shell failed. */
		perror(shell);
		exit(1);
	}
	/*
	 * Execute the command using the user's shell.  This uses the -c
	 * option to execute the command.
	 */
	argv[0] = (char *) shell0;
	argv[1] = "-c";
	argv[2] = (char *) command;
	argv[3] = NULL;
	execve(shell, argv, env);
	perror(shell);
	exit(1);
}

void
session_unused(int id)
{
	debug3("%s: session id %d unused", __func__, id);
	if (id >= options.max_sessions ||
	    id >= sessions_nalloc) {
		fatal("%s: insane session id %d (max %d nalloc %d)",
		    __func__, id, options.max_sessions, sessions_nalloc);
	}
	bzero(&sessions[id], sizeof(*sessions));
	sessions[id].self = id;
	sessions[id].used = 0;
	sessions[id].chanid = -1;
	sessions[id].ptyfd = -1;
	sessions[id].ttyfd = -1;
	sessions[id].ptymaster = -1;
	sessions[id].x11_chanids = NULL;
	sessions[id].next_unused = sessions_first_unused;
	sessions_first_unused = id;
}

Session *
session_new(void)
{
	Session *s, *tmp;

	if (sessions_first_unused == -1) {
		if (sessions_nalloc >= options.max_sessions)
			return NULL;
		debug2("%s: allocate (allocated %d max %d)",
		    __func__, sessions_nalloc, options.max_sessions);
		tmp = xrealloc(sessions, sessions_nalloc + 1,
		    sizeof(*sessions));
		if (tmp == NULL) {
			error("%s: cannot allocate %d sessions",
			    __func__, sessions_nalloc + 1);
			return NULL;
		}
		sessions = tmp;
		session_unused(sessions_nalloc++);
	}

	if (sessions_first_unused >= sessions_nalloc ||
	    sessions_first_unused < 0) {
		fatal("%s: insane first_unused %d max %d nalloc %d",
		    __func__, sessions_first_unused, options.max_sessions,
		    sessions_nalloc);
	}

	s = &sessions[sessions_first_unused];
	if (s->used) {
		fatal("%s: session %d already used",
		    __func__, sessions_first_unused);
	}
	sessions_first_unused = s->next_unused;
	s->used = 1;
	s->next_unused = -1;
	debug("session_new: session %d", s->self);

	return s;
}

static void
session_dump(void)
{
	int i;
	for (i = 0; i < sessions_nalloc; i++) {
		Session *s = &sessions[i];

		debug("dump: used %d next_unused %d session %d %p "
		    "channel %d pid %ld",
		    s->used,
		    s->next_unused,
		    s->self,
		    s,
		    s->chanid,
		    (long)s->pid);
	}
}

int
session_open(Authctxt *authctxt, int chanid)
{
	Session *s = session_new();
	debug("session_open: channel %d", chanid);
	if (s == NULL) {
		error("no more sessions");
		return 0;
	}
	s->authctxt = authctxt;
	s->pw = authctxt->pw;
	if (s->pw == NULL || !authctxt->valid)
		fatal("no user for session %d", s->self);
	debug("session_open: session %d: link with channel %d", s->self, chanid);
	s->chanid = chanid;
	return 1;
}

Session *
session_by_tty(char *tty)
{
	int i;
	for (i = 0; i < sessions_nalloc; i++) {
		Session *s = &sessions[i];
		if (s->used && s->ttyfd != -1 && strcmp(s->tty, tty) == 0) {
			debug("session_by_tty: session %d tty %s", i, tty);
			return s;
		}
	}
	debug("session_by_tty: unknown tty %.100s", tty);
	session_dump();
	return NULL;
}

static Session *
session_by_channel(int id)
{
	int i;
	for (i = 0; i < sessions_nalloc; i++) {
		Session *s = &sessions[i];
		if (s->used && s->chanid == id) {
			debug("session_by_channel: session %d channel %d",
			    i, id);
			return s;
		}
	}
	debug("session_by_channel: unknown channel %d", id);
	session_dump();
	return NULL;
}

static Session *
session_by_x11_channel(int id)
{
	int i, j;

	for (i = 0; i < sessions_nalloc; i++) {
		Session *s = &sessions[i];

		if (s->x11_chanids == NULL || !s->used)
			continue;
		for (j = 0; s->x11_chanids[j] != -1; j++) {
			if (s->x11_chanids[j] == id) {
				debug("session_by_x11_channel: session %d "
				    "channel %d", s->self, id);
				return s;
			}
		}
	}
	debug("session_by_x11_channel: unknown channel %d", id);
	session_dump();
	return NULL;
}

static Session *
session_by_pid(pid_t pid)
{
	int i;
	debug("session_by_pid: pid %ld", (long)pid);
	for (i = 0; i < sessions_nalloc; i++) {
		Session *s = &sessions[i];
		if (s->used && s->pid == pid)
			return s;
	}
	error("session_by_pid: unknown pid %ld", (long)pid);
	session_dump();
	return NULL;
}

static int
session_window_change_req(Session *s)
{
	s->col = packet_get_int();
	s->row = packet_get_int();
	s->xpixel = packet_get_int();
	s->ypixel = packet_get_int();
	packet_check_eom();
	pty_change_window_size(s->ptyfd, s->row, s->col, s->xpixel, s->ypixel);
	return 1;
}

static int
session_pty_req(Session *s)
{
	u_int len;
	int n_bytes;

	if (no_pty_flag) {
		debug("Allocating a pty not permitted for this authentication.");
		return 0;
	}
	if (s->ttyfd != -1) {
		packet_disconnect("Protocol error: you already have a pty.");
		return 0;
	}

	s->term = packet_get_string(&len);

	if (compat20) {
		s->col = packet_get_int();
		s->row = packet_get_int();
	} else {
		s->row = packet_get_int();
		s->col = packet_get_int();
	}
	s->xpixel = packet_get_int();
	s->ypixel = packet_get_int();

	if (strcmp(s->term, "") == 0) {
		xfree(s->term);
		s->term = NULL;
	}

	/* Allocate a pty and open it. */
	debug("Allocating pty.");
	if (!PRIVSEP(pty_allocate(&s->ptyfd, &s->ttyfd, s->tty,
	    sizeof(s->tty)))) {
		if (s->term)
			xfree(s->term);
		s->term = NULL;
		s->ptyfd = -1;
		s->ttyfd = -1;
		error("session_pty_req: session %d alloc failed", s->self);
		return 0;
	}
	debug("session_pty_req: session %d alloc %s", s->self, s->tty);

	/* for SSH1 the tty modes length is not given */
	if (!compat20)
		n_bytes = packet_remaining();
	tty_parse_modes(s->ttyfd, &n_bytes);

	if (!use_privsep)
		pty_setowner(s->pw, s->tty);

	/* Set window size from the packet. */
	pty_change_window_size(s->ptyfd, s->row, s->col, s->xpixel, s->ypixel);

	packet_check_eom();
	session_proctitle(s);
	return 1;
}

static int
session_subsystem_req(Session *s)
{
	struct stat st;
	u_int len;
	int success = 0;
	char *prog, *cmd, *subsys = packet_get_string(&len);
	u_int i;

	packet_check_eom();
	logit("subsystem request for %.100s by user %s", subsys,
	    s->pw->pw_name);

	for (i = 0; i < options.num_subsystems; i++) {
		if (strcmp(subsys, options.subsystem_name[i]) == 0) {
			prog = options.subsystem_command[i];
			cmd = options.subsystem_args[i];
			if (strcmp(INTERNAL_SFTP_NAME, prog) == 0) {
				s->is_subsystem = SUBSYSTEM_INT_SFTP;
				debug("subsystem: %s", prog);
			} else {
				if (stat(prog, &st) < 0)
					debug("subsystem: cannot stat %s: %s",
					    prog, strerror(errno));
				s->is_subsystem = SUBSYSTEM_EXT;
				debug("subsystem: exec() %s", cmd);
			}
			success = do_exec(s, cmd) == 0;
			break;
		}
	}

	if (!success)
		logit("subsystem request for %.100s failed, subsystem not found",
		    subsys);

	xfree(subsys);
	return success;
}

static int
session_x11_req(Session *s)
{
	int success;

	if (s->auth_proto != NULL || s->auth_data != NULL) {
		error("session_x11_req: session %d: "
		    "x11 forwarding already active", s->self);
		return 0;
	}
	s->single_connection = packet_get_char();
	s->auth_proto = packet_get_string(NULL);
	s->auth_data = packet_get_string(NULL);
	s->screen = packet_get_int();
	packet_check_eom();

	success = session_setup_x11fwd(s);
	if (!success) {
		xfree(s->auth_proto);
		xfree(s->auth_data);
		s->auth_proto = NULL;
		s->auth_data = NULL;
	}
	return success;
}

static int
session_shell_req(Session *s)
{
	packet_check_eom();
	return do_exec(s, NULL) == 0;
}

static int
session_exec_req(Session *s)
{
	u_int len, success;

	char *command = packet_get_string(&len);
	packet_check_eom();
	success = do_exec(s, command) == 0;
	xfree(command);
	return success;
}

static int
session_break_req(Session *s)
{

	packet_get_int();	/* ignored */
	packet_check_eom();

	if (s->ttyfd == -1 || tcsendbreak(s->ttyfd, 0) < 0)
		return 0;
	return 1;
}

static int
session_env_req(Session *s)
{
	char *name, *val;
	u_int name_len, val_len, i;

	name = packet_get_string(&name_len);
	val = packet_get_string(&val_len);
	packet_check_eom();

	/* Don't set too many environment variables */
	if (s->num_env > 128) {
		debug2("Ignoring env request %s: too many env vars", name);
		goto fail;
	}

	for (i = 0; i < options.num_accept_env; i++) {
		if (match_pattern(name, options.accept_env[i])) {
			debug2("Setting env %d: %s=%s", s->num_env, name, val);
			s->env = xrealloc(s->env, s->num_env + 1,
			    sizeof(*s->env));
			s->env[s->num_env].name = name;
			s->env[s->num_env].val = val;
			s->num_env++;
			return (1);
		}
	}
	debug2("Ignoring env request %s: disallowed name", name);

 fail:
	xfree(name);
	xfree(val);
	return (0);
}

static int
session_auth_agent_req(Session *s)
{
	static int called = 0;
	packet_check_eom();
	if (no_agent_forwarding_flag || !options.allow_agent_forwarding) {
		debug("session_auth_agent_req: no_agent_forwarding_flag");
		return 0;
	}
	if (called) {
		return 0;
	} else {
		called = 1;
		return auth_input_request_forwarding(s->pw);
	}
}

int
session_input_channel_req(Channel *c, const char *rtype)
{
	int success = 0;
	Session *s;

	if ((s = session_by_channel(c->self)) == NULL) {
		logit("session_input_channel_req: no session %d req %.100s",
		    c->self, rtype);
		return 0;
	}
	debug("session_input_channel_req: session %d req %s", s->self, rtype);

	/*
	 * a session is in LARVAL state until a shell, a command
	 * or a subsystem is executed
	 */
	if (c->type == SSH_CHANNEL_LARVAL) {
		if (strcmp(rtype, "shell") == 0) {
			success = session_shell_req(s);
		} else if (strcmp(rtype, "exec") == 0) {
			success = session_exec_req(s);
		} else if (strcmp(rtype, "pty-req") == 0) {
			success = session_pty_req(s);
		} else if (strcmp(rtype, "x11-req") == 0) {
			success = session_x11_req(s);
		} else if (strcmp(rtype, "auth-agent-req@openssh.com") == 0) {
			success = session_auth_agent_req(s);
		} else if (strcmp(rtype, "subsystem") == 0) {
			success = session_subsystem_req(s);
		} else if (strcmp(rtype, "env") == 0) {
			success = session_env_req(s);
		}
	}
	if (strcmp(rtype, "window-change") == 0) {
		success = session_window_change_req(s);
	} else if (strcmp(rtype, "break") == 0) {
		success = session_break_req(s);
	}

	return success;
}

void
session_set_fds(Session *s, int fdin, int fdout, int fderr, int ignore_fderr,
    int is_tty)
{
	if (!compat20)
		fatal("session_set_fds: called for proto != 2.0");
	/*
	 * now that have a child and a pipe to the child,
	 * we can activate our channel and register the fd's
	 */
	if (s->chanid == -1)
		fatal("no channel for session %d", s->self);
	channel_set_fds(s->chanid,
	    fdout, fdin, fderr,
	    ignore_fderr ? CHAN_EXTENDED_IGNORE : CHAN_EXTENDED_READ,
	    1, is_tty, CHAN_SES_WINDOW_DEFAULT);
}

/*
 * Function to perform pty cleanup. Also called if we get aborted abnormally
 * (e.g., due to a dropped connection).
 */
void
session_pty_cleanup2(Session *s)
{
	if (s == NULL) {
		error("session_pty_cleanup: no session");
		return;
	}
	if (s->ttyfd == -1)
		return;

	debug("session_pty_cleanup: session %d release %s", s->self, s->tty);

	/* Record that the user has logged out. */
	if (s->pid != 0)
		record_logout(s->pid, s->tty, s->pw->pw_name);

	/* Release the pseudo-tty. */
	if (getuid() == 0)
		pty_release(s->tty);

	/*
	 * Close the server side of the socket pairs.  We must do this after
	 * the pty cleanup, so that another process doesn't get this pty
	 * while we're still cleaning up.
	 */
	if (s->ptymaster != -1 && close(s->ptymaster) < 0)
		error("close(s->ptymaster/%d): %s",
		    s->ptymaster, strerror(errno));

	/* unlink pty from session */
	s->ttyfd = -1;
}

void
session_pty_cleanup(Session *s)
{
	PRIVSEP(session_pty_cleanup2(s));
}

static char *
sig2name(int sig)
{
#define SSH_SIG(x) if (sig == SIG ## x) return #x
	SSH_SIG(ABRT);
	SSH_SIG(ALRM);
	SSH_SIG(FPE);
	SSH_SIG(HUP);
	SSH_SIG(ILL);
	SSH_SIG(INT);
	SSH_SIG(KILL);
	SSH_SIG(PIPE);
	SSH_SIG(QUIT);
	SSH_SIG(SEGV);
	SSH_SIG(TERM);
	SSH_SIG(USR1);
	SSH_SIG(USR2);
#undef	SSH_SIG
	return "SIG@openssh.com";
}

static void
session_close_x11(int id)
{
	Channel *c;

	if ((c = channel_by_id(id)) == NULL) {
		debug("session_close_x11: x11 channel %d missing", id);
	} else {
		/* Detach X11 listener */
		debug("session_close_x11: detach x11 channel %d", id);
		channel_cancel_cleanup(id);
		if (c->ostate != CHAN_OUTPUT_CLOSED)
			chan_mark_dead(c);
	}
}

static void
session_close_single_x11(int id, void *arg)
{
	Session *s;
	u_int i;

	debug3("session_close_single_x11: channel %d", id);
	channel_cancel_cleanup(id);
	if ((s = session_by_x11_channel(id)) == NULL)
		fatal("session_close_single_x11: no x11 channel %d", id);
	for (i = 0; s->x11_chanids[i] != -1; i++) {
		debug("session_close_single_x11: session %d: "
		    "closing channel %d", s->self, s->x11_chanids[i]);
		/*
		 * The channel "id" is already closing, but make sure we
		 * close all of its siblings.
		 */
		if (s->x11_chanids[i] != id)
			session_close_x11(s->x11_chanids[i]);
	}
	xfree(s->x11_chanids);
	s->x11_chanids = NULL;
	if (s->display) {
		xfree(s->display);
		s->display = NULL;
	}
	if (s->auth_proto) {
		xfree(s->auth_proto);
		s->auth_proto = NULL;
	}
	if (s->auth_data) {
		xfree(s->auth_data);
		s->auth_data = NULL;
	}
	if (s->auth_display) {
		xfree(s->auth_display);
		s->auth_display = NULL;
	}
}

static void
session_exit_message(Session *s, int status)
{
	Channel *c;

	if ((c = channel_lookup(s->chanid)) == NULL)
		fatal("session_exit_message: session %d: no channel %d",
		    s->self, s->chanid);
	debug("session_exit_message: session %d channel %d pid %ld",
	    s->self, s->chanid, (long)s->pid);

	if (WIFEXITED(status)) {
		channel_request_start(s->chanid, "exit-status", 0);
		packet_put_int(WEXITSTATUS(status));
		packet_send();
	} else if (WIFSIGNALED(status)) {
		channel_request_start(s->chanid, "exit-signal", 0);
		packet_put_cstring(sig2name(WTERMSIG(status)));
#ifdef WCOREDUMP
		packet_put_char(WCOREDUMP(status)? 1 : 0);
#else /* WCOREDUMP */
		packet_put_char(0);
#endif /* WCOREDUMP */
		packet_put_cstring("");
		packet_put_cstring("");
		packet_send();
	} else {
		/* Some weird exit cause.  Just exit. */
		packet_disconnect("wait returned status %04x.", status);
	}

	/* disconnect channel */
	debug("session_exit_message: release channel %d", s->chanid);

	/*
	 * Adjust cleanup callback attachment to send close messages when
	 * the channel gets EOF. The session will be then be closed
	 * by session_close_by_channel when the childs close their fds.
	 */
	channel_register_cleanup(c->self, session_close_by_channel, 1);

	/*
	 * emulate a write failure with 'chan_write_failed', nobody will be
	 * interested in data we write.
	 * Note that we must not call 'chan_read_failed', since there could
	 * be some more data waiting in the pipe.
	 */
	if (c->ostate != CHAN_OUTPUT_CLOSED)
		chan_write_failed(c);
}

void
session_close(Session *s)
{
	u_int i;

	debug("session_close: session %d pid %ld", s->self, (long)s->pid);
	if (s->ttyfd != -1)
		session_pty_cleanup(s);
	if (s->term)
		xfree(s->term);
	if (s->display)
		xfree(s->display);
	if (s->x11_chanids)
		xfree(s->x11_chanids);
	if (s->auth_display)
		xfree(s->auth_display);
	if (s->auth_data)
		xfree(s->auth_data);
	if (s->auth_proto)
		xfree(s->auth_proto);
	if (s->env != NULL) {
		for (i = 0; i < s->num_env; i++) {
			xfree(s->env[i].name);
			xfree(s->env[i].val);
		}
		xfree(s->env);
	}
	session_proctitle(s);
	session_unused(s->self);
}

void
session_close_by_pid(pid_t pid, int status)
{
	Session *s = session_by_pid(pid);
	if (s == NULL) {
		debug("session_close_by_pid: no session for pid %ld",
		    (long)pid);
		return;
	}
	if (s->chanid != -1)
		session_exit_message(s, status);
	if (s->ttyfd != -1)
		session_pty_cleanup(s);
	s->pid = 0;
}

/*
 * this is called when a channel dies before
 * the session 'child' itself dies
 */
void
session_close_by_channel(int id, void *arg)
{
	Session *s = session_by_channel(id);
	u_int i;

	if (s == NULL) {
		debug("session_close_by_channel: no session for id %d", id);
		return;
	}
	debug("session_close_by_channel: channel %d child %ld",
	    id, (long)s->pid);
	if (s->pid != 0) {
		debug("session_close_by_channel: channel %d: has child", id);
		/*
		 * delay detach of session, but release pty, since
		 * the fd's to the child are already closed
		 */
		if (s->ttyfd != -1)
			session_pty_cleanup(s);
		return;
	}
	/* detach by removing callback */
	channel_cancel_cleanup(s->chanid);

	/* Close any X11 listeners associated with this session */
	if (s->x11_chanids != NULL) {
		for (i = 0; s->x11_chanids[i] != -1; i++) {
			session_close_x11(s->x11_chanids[i]);
			s->x11_chanids[i] = -1;
		}
	}

	s->chanid = -1;
	session_close(s);
}

void
session_destroy_all(void (*closefunc)(Session *))
{
	int i;
	for (i = 0; i < sessions_nalloc; i++) {
		Session *s = &sessions[i];
		if (s->used) {
			if (closefunc != NULL)
				closefunc(s);
			else
				session_close(s);
		}
	}
}

static char *
session_tty_list(void)
{
	static char buf[1024];
	int i;
	char *cp;

	buf[0] = '\0';
	for (i = 0; i < sessions_nalloc; i++) {
		Session *s = &sessions[i];
		if (s->used && s->ttyfd != -1) {

			if (strncmp(s->tty, "/dev/", 5) != 0) {
				cp = strrchr(s->tty, '/');
				cp = (cp == NULL) ? s->tty : cp + 1;
			} else
				cp = s->tty + 5;

			if (buf[0] != '\0')
				strlcat(buf, ",", sizeof buf);
			strlcat(buf, cp, sizeof buf);
		}
	}
	if (buf[0] == '\0')
		strlcpy(buf, "notty", sizeof buf);
	return buf;
}

void
session_proctitle(Session *s)
{
	if (s->pw == NULL)
		error("no user for session %d", s->self);
	else
		setproctitle("%s@%s", s->pw->pw_name, session_tty_list());
}

int
session_setup_x11fwd(Session *s)
{
	struct stat st;
	char display[512], auth_display[512];
	char hostname[MAXHOSTNAMELEN];
	u_int i;

	if (no_x11_forwarding_flag) {
		packet_send_debug("X11 forwarding disabled in user configuration file.");
		return 0;
	}
	if (!options.x11_forwarding) {
		debug("X11 forwarding disabled in server configuration file.");
		return 0;
	}
	if (!options.xauth_location ||
	    (stat(options.xauth_location, &st) == -1)) {
		packet_send_debug("No xauth program; cannot forward with spoofing.");
		return 0;
	}
	if (options.use_login) {
		packet_send_debug("X11 forwarding disabled; "
		    "not compatible with UseLogin=yes.");
		return 0;
	}
	if (s->display != NULL) {
		debug("X11 display already set.");
		return 0;
	}
	if (x11_create_display_inet(options.x11_display_offset,
	    options.x11_use_localhost, s->single_connection,
	    &s->display_number, &s->x11_chanids) == -1) {
		debug("x11_create_display_inet failed.");
		return 0;
	}
	for (i = 0; s->x11_chanids[i] != -1; i++) {
		channel_register_cleanup(s->x11_chanids[i],
		    session_close_single_x11, 0);
	}

	/* Set up a suitable value for the DISPLAY variable. */
	if (gethostname(hostname, sizeof(hostname)) < 0)
		fatal("gethostname: %.100s", strerror(errno));
	/*
	 * auth_display must be used as the displayname when the
	 * authorization entry is added with xauth(1).  This will be
	 * different than the DISPLAY string for localhost displays.
	 */
	if (options.x11_use_localhost) {
		snprintf(display, sizeof display, "localhost:%u.%u",
		    s->display_number, s->screen);
		snprintf(auth_display, sizeof auth_display, "unix:%u.%u",
		    s->display_number, s->screen);
		s->display = xstrdup(display);
		s->auth_display = xstrdup(auth_display);
	} else {
#ifdef IPADDR_IN_DISPLAY
		struct hostent *he;
		struct in_addr my_addr;

		he = gethostbyname(hostname);
		if (he == NULL) {
			error("Can't get IP address for X11 DISPLAY.");
			packet_send_debug("Can't get IP address for X11 DISPLAY.");
			return 0;
		}
		memcpy(&my_addr, he->h_addr_list[0], sizeof(struct in_addr));
		snprintf(display, sizeof display, "%.50s:%u.%u", inet_ntoa(my_addr),
		    s->display_number, s->screen);
#else
		snprintf(display, sizeof display, "%.400s:%u.%u", hostname,
		    s->display_number, s->screen);
#endif
		s->display = xstrdup(display);
		s->auth_display = xstrdup(display);
	}

	return 1;
}

static void
do_authenticated2(Authctxt *authctxt)
{
	server_loop2(authctxt);
}

void
do_cleanup(Authctxt *authctxt)
{
	static int called = 0;

	debug("do_cleanup");

	/* no cleanup if we're in the child for login shell */
	if (is_child)
		return;

	/* avoid double cleanup */
	if (called)
		return;
	called = 1;

	if (authctxt == NULL)
		return;

#ifdef USE_PAM
	if (options.use_pam) {
		sshpam_cleanup();
		sshpam_thread_cleanup();
	}
#endif

	if (!authctxt->authenticated)
		return;

#ifdef KRB5
	if (options.kerberos_ticket_cleanup &&
	    authctxt->krb5_ctx)
		krb5_cleanup_proc(authctxt);
#endif

#ifdef GSSAPI
	if (compat20 && options.gss_cleanup_creds)
		ssh_gssapi_cleanup_creds();
#endif

	/* remove agent socket */
	auth_sock_cleanup_proc(authctxt->pw);

	/*
	 * Cleanup ptys/utmp only if privsep is disabled,
	 * or if running in monitor.
	 */
	if (!use_privsep || mm_is_monitor())
		session_destroy_all(session_pty_cleanup2);
}
