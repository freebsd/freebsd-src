/* $OpenBSD: mux.c,v 1.7 2008/06/13 17:21:20 dtucker Exp $ */
/*
 * Copyright (c) 2002-2008 Damien Miller <djm@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* ssh session multiplexing support */

#include "includes.h"

/*
 * TODO:
 *   1. partial reads in muxserver_accept_control (maybe make channels
 *      from accepted connections)
 *   2. Better signalling from master to slave, especially passing of
 *      error messages
 *   3. Better fall-back from mux slave error to new connection.
 *   3. Add/delete forwardings via slave
 *   4. ExitOnForwardingFailure (after #3 obviously)
 *   5. Maybe extension mechanisms for multi-X11/multi-agent forwarding
 *   6. Document the mux mini-protocol somewhere.
 *   7. Support ~^Z in mux slaves.
 *   8. Inspect or control sessions in master.
 *   9. If we ever support the "signal" channel request, send signals on
 *      sessions in master.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#ifdef HAVE_PATHS_H
#include <paths.h>
#endif

#ifdef HAVE_UTIL_H
# include <util.h>
#endif

#ifdef HAVE_LIBUTIL_H
# include <libutil.h>
#endif

#include "openbsd-compat/sys-queue.h"
#include "xmalloc.h"
#include "log.h"
#include "ssh.h"
#include "pathnames.h"
#include "misc.h"
#include "match.h"
#include "buffer.h"
#include "channels.h"
#include "msg.h"
#include "packet.h"
#include "monitor_fdpass.h"
#include "sshpty.h"
#include "key.h"
#include "readconf.h"
#include "clientloop.h"

/* from ssh.c */
extern int tty_flag;
extern Options options;
extern int stdin_null_flag;
extern char *host;
int subsystem_flag;
extern Buffer command;

/* Context for session open confirmation callback */
struct mux_session_confirm_ctx {
	int want_tty;
	int want_subsys;
	int want_x_fwd;
	int want_agent_fwd;
	Buffer cmd;
	char *term;
	struct termios tio;
	char **env;
};

/* fd to control socket */
int muxserver_sock = -1;

/* Multiplexing control command */
u_int muxclient_command = 0;

/* Set when signalled. */
static volatile sig_atomic_t muxclient_terminate = 0;

/* PID of multiplex server */
static u_int muxserver_pid = 0;


/* ** Multiplexing master support */

/* Prepare a mux master to listen on a Unix domain socket. */
void
muxserver_listen(void)
{
	struct sockaddr_un addr;
	mode_t old_umask;
	int addr_len;

	if (options.control_path == NULL ||
	    options.control_master == SSHCTL_MASTER_NO)
		return;

	debug("setting up multiplex master socket");

	memset(&addr, '\0', sizeof(addr));
	addr.sun_family = AF_UNIX;
	addr_len = offsetof(struct sockaddr_un, sun_path) +
	    strlen(options.control_path) + 1;

	if (strlcpy(addr.sun_path, options.control_path,
	    sizeof(addr.sun_path)) >= sizeof(addr.sun_path))
		fatal("ControlPath too long");

	if ((muxserver_sock = socket(PF_UNIX, SOCK_STREAM, 0)) < 0)
		fatal("%s socket(): %s", __func__, strerror(errno));

	old_umask = umask(0177);
	if (bind(muxserver_sock, (struct sockaddr *)&addr, addr_len) == -1) {
		muxserver_sock = -1;
		if (errno == EINVAL || errno == EADDRINUSE) {
			error("ControlSocket %s already exists, "
			    "disabling multiplexing", options.control_path);
			close(muxserver_sock);
			muxserver_sock = -1;
			xfree(options.control_path);
			options.control_path = NULL;
			options.control_master = SSHCTL_MASTER_NO;
			return;
		} else
			fatal("%s bind(): %s", __func__, strerror(errno));
	}
	umask(old_umask);

	if (listen(muxserver_sock, 64) == -1)
		fatal("%s listen(): %s", __func__, strerror(errno));

	set_nonblock(muxserver_sock);
}

/* Callback on open confirmation in mux master for a mux client session. */
static void
mux_session_confirm(int id, void *arg)
{
	struct mux_session_confirm_ctx *cctx = arg;
	const char *display;
	Channel *c;
	int i;

	if (cctx == NULL)
		fatal("%s: cctx == NULL", __func__);
	if ((c = channel_lookup(id)) == NULL)
		fatal("%s: no channel for id %d", __func__, id);

	display = getenv("DISPLAY");
	if (cctx->want_x_fwd && options.forward_x11 && display != NULL) {
		char *proto, *data;
		/* Get reasonable local authentication information. */
		client_x11_get_proto(display, options.xauth_location,
		    options.forward_x11_trusted, &proto, &data);
		/* Request forwarding with authentication spoofing. */
		debug("Requesting X11 forwarding with authentication spoofing.");
		x11_request_forwarding_with_spoofing(id, display, proto, data);
		/* XXX wait for reply */
	}

	if (cctx->want_agent_fwd && options.forward_agent) {
		debug("Requesting authentication agent forwarding.");
		channel_request_start(id, "auth-agent-req@openssh.com", 0);
		packet_send();
	}

	client_session2_setup(id, cctx->want_tty, cctx->want_subsys,
	    cctx->term, &cctx->tio, c->rfd, &cctx->cmd, cctx->env);

	c->open_confirm_ctx = NULL;
	buffer_free(&cctx->cmd);
	xfree(cctx->term);
	if (cctx->env != NULL) {
		for (i = 0; cctx->env[i] != NULL; i++)
			xfree(cctx->env[i]);
		xfree(cctx->env);
	}
	xfree(cctx);
}

/*
 * Accept a connection on the mux master socket and process the
 * client's request. Returns flag indicating whether mux master should
 * begin graceful close.
 */
int
muxserver_accept_control(void)
{
	Buffer m;
	Channel *c;
	int client_fd, new_fd[3], ver, allowed, window, packetmax;
	socklen_t addrlen;
	struct sockaddr_storage addr;
	struct mux_session_confirm_ctx *cctx;
	char *cmd;
	u_int i, j, len, env_len, mux_command, flags, escape_char;
	uid_t euid;
	gid_t egid;
	int start_close = 0;

	/*
	 * Accept connection on control socket
	 */
	memset(&addr, 0, sizeof(addr));
	addrlen = sizeof(addr);
	if ((client_fd = accept(muxserver_sock,
	    (struct sockaddr*)&addr, &addrlen)) == -1) {
		error("%s accept: %s", __func__, strerror(errno));
		return 0;
	}

	if (getpeereid(client_fd, &euid, &egid) < 0) {
		error("%s getpeereid failed: %s", __func__, strerror(errno));
		close(client_fd);
		return 0;
	}
	if ((euid != 0) && (getuid() != euid)) {
		error("control mode uid mismatch: peer euid %u != uid %u",
		    (u_int) euid, (u_int) getuid());
		close(client_fd);
		return 0;
	}

	/* XXX handle asynchronously */
	unset_nonblock(client_fd);

	/* Read command */
	buffer_init(&m);
	if (ssh_msg_recv(client_fd, &m) == -1) {
		error("%s: client msg_recv failed", __func__);
		close(client_fd);
		buffer_free(&m);
		return 0;
	}
	if ((ver = buffer_get_char(&m)) != SSHMUX_VER) {
		error("%s: wrong client version %d", __func__, ver);
		buffer_free(&m);
		close(client_fd);
		return 0;
	}

	allowed = 1;
	mux_command = buffer_get_int(&m);
	flags = buffer_get_int(&m);

	buffer_clear(&m);

	switch (mux_command) {
	case SSHMUX_COMMAND_OPEN:
		if (options.control_master == SSHCTL_MASTER_ASK ||
		    options.control_master == SSHCTL_MASTER_AUTO_ASK)
			allowed = ask_permission("Allow shared connection "
			    "to %s? ", host);
		/* continue below */
		break;
	case SSHMUX_COMMAND_TERMINATE:
		if (options.control_master == SSHCTL_MASTER_ASK ||
		    options.control_master == SSHCTL_MASTER_AUTO_ASK)
			allowed = ask_permission("Terminate shared connection "
			    "to %s? ", host);
		if (allowed)
			start_close = 1;
		/* FALLTHROUGH */
	case SSHMUX_COMMAND_ALIVE_CHECK:
		/* Reply for SSHMUX_COMMAND_TERMINATE and ALIVE_CHECK */
		buffer_clear(&m);
		buffer_put_int(&m, allowed);
		buffer_put_int(&m, getpid());
		if (ssh_msg_send(client_fd, SSHMUX_VER, &m) == -1) {
			error("%s: client msg_send failed", __func__);
			close(client_fd);
			buffer_free(&m);
			return start_close;
		}
		buffer_free(&m);
		close(client_fd);
		return start_close;
	default:
		error("Unsupported command %d", mux_command);
		buffer_free(&m);
		close(client_fd);
		return 0;
	}

	/* Reply for SSHMUX_COMMAND_OPEN */
	buffer_clear(&m);
	buffer_put_int(&m, allowed);
	buffer_put_int(&m, getpid());
	if (ssh_msg_send(client_fd, SSHMUX_VER, &m) == -1) {
		error("%s: client msg_send failed", __func__);
		close(client_fd);
		buffer_free(&m);
		return 0;
	}

	if (!allowed) {
		error("Refused control connection");
		close(client_fd);
		buffer_free(&m);
		return 0;
	}

	buffer_clear(&m);
	if (ssh_msg_recv(client_fd, &m) == -1) {
		error("%s: client msg_recv failed", __func__);
		close(client_fd);
		buffer_free(&m);
		return 0;
	}
	if ((ver = buffer_get_char(&m)) != SSHMUX_VER) {
		error("%s: wrong client version %d", __func__, ver);
		buffer_free(&m);
		close(client_fd);
		return 0;
	}

	cctx = xcalloc(1, sizeof(*cctx));
	cctx->want_tty = (flags & SSHMUX_FLAG_TTY) != 0;
	cctx->want_subsys = (flags & SSHMUX_FLAG_SUBSYS) != 0;
	cctx->want_x_fwd = (flags & SSHMUX_FLAG_X11_FWD) != 0;
	cctx->want_agent_fwd = (flags & SSHMUX_FLAG_AGENT_FWD) != 0;
	cctx->term = buffer_get_string(&m, &len);
	escape_char = buffer_get_int(&m);

	cmd = buffer_get_string(&m, &len);
	buffer_init(&cctx->cmd);
	buffer_append(&cctx->cmd, cmd, strlen(cmd));

	env_len = buffer_get_int(&m);
	env_len = MIN(env_len, 4096);
	debug3("%s: receiving %d env vars", __func__, env_len);
	if (env_len != 0) {
		cctx->env = xcalloc(env_len + 1, sizeof(*cctx->env));
		for (i = 0; i < env_len; i++)
			cctx->env[i] = buffer_get_string(&m, &len);
		cctx->env[i] = NULL;
	}

	debug2("%s: accepted tty %d, subsys %d, cmd %s", __func__,
	    cctx->want_tty, cctx->want_subsys, cmd);
	xfree(cmd);

	/* Gather fds from client */
	for(i = 0; i < 3; i++) {
		if ((new_fd[i] = mm_receive_fd(client_fd)) == -1) {
			error("%s: failed to receive fd %d from slave",
			    __func__, i);
			for (j = 0; j < i; j++)
				close(new_fd[j]);
			for (j = 0; j < env_len; j++)
				xfree(cctx->env[j]);
			if (env_len > 0)
				xfree(cctx->env);
			xfree(cctx->term);
			buffer_free(&cctx->cmd);
			close(client_fd);
			xfree(cctx);
			return 0;
		}
	}

	debug2("%s: got fds stdin %d, stdout %d, stderr %d", __func__,
	    new_fd[0], new_fd[1], new_fd[2]);

	/* Try to pick up ttymodes from client before it goes raw */
	if (cctx->want_tty && tcgetattr(new_fd[0], &cctx->tio) == -1)
		error("%s: tcgetattr: %s", __func__, strerror(errno));

	/* This roundtrip is just for synchronisation of ttymodes */
	buffer_clear(&m);
	if (ssh_msg_send(client_fd, SSHMUX_VER, &m) == -1) {
		error("%s: client msg_send failed", __func__);
		close(client_fd);
		close(new_fd[0]);
		close(new_fd[1]);
		close(new_fd[2]);
		buffer_free(&m);
		xfree(cctx->term);
		if (env_len != 0) {
			for (i = 0; i < env_len; i++)
				xfree(cctx->env[i]);
			xfree(cctx->env);
		}
		return 0;
	}
	buffer_free(&m);

	/* enable nonblocking unless tty */
	if (!isatty(new_fd[0]))
		set_nonblock(new_fd[0]);
	if (!isatty(new_fd[1]))
		set_nonblock(new_fd[1]);
	if (!isatty(new_fd[2]))
		set_nonblock(new_fd[2]);

	set_nonblock(client_fd);

	window = CHAN_SES_WINDOW_DEFAULT;
	packetmax = CHAN_SES_PACKET_DEFAULT;
	if (cctx->want_tty) {
		window >>= 1;
		packetmax >>= 1;
	}
	
	c = channel_new("session", SSH_CHANNEL_OPENING,
	    new_fd[0], new_fd[1], new_fd[2], window, packetmax,
	    CHAN_EXTENDED_WRITE, "client-session", /*nonblock*/0);

	c->ctl_fd = client_fd;
	if (cctx->want_tty && escape_char != 0xffffffff) {
		channel_register_filter(c->self,
		    client_simple_escape_filter, NULL,
		    client_filter_cleanup,
		    client_new_escape_filter_ctx((int)escape_char));
	}

	debug3("%s: channel_new: %d", __func__, c->self);

	channel_send_open(c->self);
	channel_register_open_confirm(c->self, mux_session_confirm, cctx);
	return 0;
}

/* ** Multiplexing client support */

/* Exit signal handler */
static void
control_client_sighandler(int signo)
{
	muxclient_terminate = signo;
}

/*
 * Relay signal handler - used to pass some signals from mux client to
 * mux master.
 */
static void
control_client_sigrelay(int signo)
{
	int save_errno = errno;

	if (muxserver_pid > 1)
		kill(muxserver_pid, signo);

	errno = save_errno;
}

/* Check mux client environment variables before passing them to mux master. */
static int
env_permitted(char *env)
{
	int i, ret;
	char name[1024], *cp;

	if ((cp = strchr(env, '=')) == NULL || cp == env)
		return (0);
	ret = snprintf(name, sizeof(name), "%.*s", (int)(cp - env), env);
	if (ret <= 0 || (size_t)ret >= sizeof(name))
		fatal("env_permitted: name '%.100s...' too long", env);

	for (i = 0; i < options.num_send_env; i++)
		if (match_pattern(name, options.send_env[i]))
			return (1);

	return (0);
}

/* Multiplex client main loop. */
void
muxclient(const char *path)
{
	struct sockaddr_un addr;
	int i, r, fd, sock, exitval[2], num_env, addr_len;
	Buffer m;
	char *term;
	extern char **environ;
	u_int allowed, flags;

	if (muxclient_command == 0)
		muxclient_command = SSHMUX_COMMAND_OPEN;

	switch (options.control_master) {
	case SSHCTL_MASTER_AUTO:
	case SSHCTL_MASTER_AUTO_ASK:
		debug("auto-mux: Trying existing master");
		/* FALLTHROUGH */
	case SSHCTL_MASTER_NO:
		break;
	default:
		return;
	}

	memset(&addr, '\0', sizeof(addr));
	addr.sun_family = AF_UNIX;
	addr_len = offsetof(struct sockaddr_un, sun_path) +
	    strlen(path) + 1;

	if (strlcpy(addr.sun_path, path,
	    sizeof(addr.sun_path)) >= sizeof(addr.sun_path))
		fatal("ControlPath too long");

	if ((sock = socket(PF_UNIX, SOCK_STREAM, 0)) < 0)
		fatal("%s socket(): %s", __func__, strerror(errno));

	if (connect(sock, (struct sockaddr *)&addr, addr_len) == -1) {
		if (muxclient_command != SSHMUX_COMMAND_OPEN) {
			fatal("Control socket connect(%.100s): %s", path,
			    strerror(errno));
		}
		if (errno == ENOENT)
			debug("Control socket \"%.100s\" does not exist", path);
		else {
			error("Control socket connect(%.100s): %s", path,
			    strerror(errno));
		}
		close(sock);
		return;
	}

	if (stdin_null_flag) {
		if ((fd = open(_PATH_DEVNULL, O_RDONLY)) == -1)
			fatal("open(/dev/null): %s", strerror(errno));
		if (dup2(fd, STDIN_FILENO) == -1)
			fatal("dup2: %s", strerror(errno));
		if (fd > STDERR_FILENO)
			close(fd);
	}

	term = getenv("TERM");

	flags = 0;
	if (tty_flag)
		flags |= SSHMUX_FLAG_TTY;
	if (subsystem_flag)
		flags |= SSHMUX_FLAG_SUBSYS;
	if (options.forward_x11)
		flags |= SSHMUX_FLAG_X11_FWD;
	if (options.forward_agent)
		flags |= SSHMUX_FLAG_AGENT_FWD;

	signal(SIGPIPE, SIG_IGN);

	buffer_init(&m);

	/* Send our command to server */
	buffer_put_int(&m, muxclient_command);
	buffer_put_int(&m, flags);
	if (ssh_msg_send(sock, SSHMUX_VER, &m) == -1) {
		error("%s: msg_send", __func__);
 muxerr:
		close(sock);
		buffer_free(&m);
		if (muxclient_command != SSHMUX_COMMAND_OPEN)
			cleanup_exit(255);
		logit("Falling back to non-multiplexed connection");
		xfree(options.control_path);
		options.control_path = NULL;
		options.control_master = SSHCTL_MASTER_NO;
		return;
	}
	buffer_clear(&m);

	/* Get authorisation status and PID of controlee */
	if (ssh_msg_recv(sock, &m) == -1) {
		error("%s: Did not receive reply from master", __func__);
		goto muxerr;
	}
	if (buffer_get_char(&m) != SSHMUX_VER) {
		error("%s: Master replied with wrong version", __func__);
		goto muxerr;
	}
	if (buffer_get_int_ret(&allowed, &m) != 0) {
		error("%s: bad server reply", __func__);
		goto muxerr;
	}
	if (allowed != 1) {
		error("Connection to master denied");
		goto muxerr;
	}
	muxserver_pid = buffer_get_int(&m);

	buffer_clear(&m);

	switch (muxclient_command) {
	case SSHMUX_COMMAND_ALIVE_CHECK:
		fprintf(stderr, "Master running (pid=%d)\r\n",
		    muxserver_pid);
		exit(0);
	case SSHMUX_COMMAND_TERMINATE:
		fprintf(stderr, "Exit request sent.\r\n");
		exit(0);
	case SSHMUX_COMMAND_OPEN:
		buffer_put_cstring(&m, term ? term : "");
		if (options.escape_char == SSH_ESCAPECHAR_NONE)
			buffer_put_int(&m, 0xffffffff);
		else
			buffer_put_int(&m, options.escape_char);
		buffer_append(&command, "\0", 1);
		buffer_put_cstring(&m, buffer_ptr(&command));

		if (options.num_send_env == 0 || environ == NULL) {
			buffer_put_int(&m, 0);
		} else {
			/* Pass environment */
			num_env = 0;
			for (i = 0; environ[i] != NULL; i++) {
				if (env_permitted(environ[i]))
					num_env++; /* Count */
			}
			buffer_put_int(&m, num_env);
		for (i = 0; environ[i] != NULL && num_env >= 0; i++) {
				if (env_permitted(environ[i])) {
					num_env--;
					buffer_put_cstring(&m, environ[i]);
				}
			}
		}
		break;
	default:
		fatal("unrecognised muxclient_command %d", muxclient_command);
	}

	if (ssh_msg_send(sock, SSHMUX_VER, &m) == -1) {
		error("%s: msg_send", __func__);
		goto muxerr;
	}

	if (mm_send_fd(sock, STDIN_FILENO) == -1 ||
	    mm_send_fd(sock, STDOUT_FILENO) == -1 ||
	    mm_send_fd(sock, STDERR_FILENO) == -1) {
		error("%s: send fds failed", __func__);
		goto muxerr;
	}

	/*
	 * Mux errors are non-recoverable from this point as the master
	 * has ownership of the session now.
	 */

	/* Wait for reply, so master has a chance to gather ttymodes */
	buffer_clear(&m);
	if (ssh_msg_recv(sock, &m) == -1)
		fatal("%s: msg_recv", __func__);
	if (buffer_get_char(&m) != SSHMUX_VER)
		fatal("%s: wrong version", __func__);
	buffer_free(&m);

	signal(SIGHUP, control_client_sighandler);
	signal(SIGINT, control_client_sighandler);
	signal(SIGTERM, control_client_sighandler);
	signal(SIGWINCH, control_client_sigrelay);

	if (tty_flag)
		enter_raw_mode();

	/*
	 * Stick around until the controlee closes the client_fd.
	 * Before it does, it is expected to write this process' exit
	 * value (one int). This process must read the value and wait for
	 * the closure of the client_fd; if this one closes early, the 
	 * multiplex master will terminate early too (possibly losing data).
	 */
	exitval[0] = 0;
	for (i = 0; !muxclient_terminate && i < (int)sizeof(exitval);) {
		r = read(sock, (char *)exitval + i, sizeof(exitval) - i);
		if (r == 0) {
			debug2("Received EOF from master");
			break;
		}
		if (r == -1) {
			if (errno == EINTR)
				continue;
			fatal("%s: read %s", __func__, strerror(errno));
		}
		i += r;
	}

	close(sock);
	leave_raw_mode();
	if (i > (int)sizeof(int))
		fatal("%s: master returned too much data (%d > %lu)",
		    __func__, i, (u_long)sizeof(int));
	if (muxclient_terminate) {
		debug2("Exiting on signal %d", muxclient_terminate);
		exitval[0] = 255;
	} else if (i < (int)sizeof(int)) {
		debug2("Control master terminated unexpectedly");
		exitval[0] = 255;
	} else
		debug2("Received exit status from master %d", exitval[0]);

	if (tty_flag && options.log_level != SYSLOG_LEVEL_QUIET)
		fprintf(stderr, "Shared connection to %s closed.\r\n", host);

	exit(exitval[0]);
}
