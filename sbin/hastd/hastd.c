/*-
 * Copyright (c) 2009-2010 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Pawel Jakub Dawidek under sponsorship from
 * the FreeBSD Foundation.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/linker.h>
#include <sys/module.h>
#include <sys/wait.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <libutil.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include <activemap.h>
#include <pjdlog.h>

#include "control.h"
#include "hast.h"
#include "hast_proto.h"
#include "hastd.h"
#include "subr.h"

/* Path to configuration file. */
static const char *cfgpath = HAST_CONFIG;
/* Hastd configuration. */
static struct hastd_config *cfg;
/* Was SIGCHLD signal received? */
static bool sigchld_received = false;
/* Was SIGHUP signal received? */
static bool sighup_received = false;
/* Was SIGINT or SIGTERM signal received? */
bool sigexit_received = false;
/* PID file handle. */
struct pidfh *pfh;

static void
usage(void)
{

	errx(EX_USAGE, "[-dFh] [-c config] [-P pidfile]");
}

static void
sighandler(int sig)
{

	switch (sig) {
	case SIGCHLD:
		sigchld_received = true;
		break;
	case SIGHUP:
		sighup_received = true;
		break;
	default:
		assert(!"invalid condition");
	}
}

static void
g_gate_load(void)
{

	if (modfind("g_gate") == -1) {
		/* Not present in kernel, try loading it. */
		if (kldload("geom_gate") == -1 || modfind("g_gate") == -1) {
			if (errno != EEXIST) {
				pjdlog_exit(EX_OSERR,
				    "Unable to load geom_gate module");
			}
		}
	}
}

static void
child_exit(void)
{
	struct hast_resource *res;
	int status;
	pid_t pid;

	while ((pid = wait3(&status, WNOHANG, NULL)) > 0) {
		/* Find resource related to the process that just exited. */
		TAILQ_FOREACH(res, &cfg->hc_resources, hr_next) {
			if (pid == res->hr_workerpid)
				break;
		}
		if (res == NULL) {
			/*
			 * This can happen when new connection arrives and we
			 * cancel child responsible for the old one.
			 */
			continue;
		}
		pjdlog_prefix_set("[%s] (%s) ", res->hr_name,
		    role2str(res->hr_role));
		if (WEXITSTATUS(status) == 0) {
			pjdlog_debug(1,
			    "Worker process exited gracefully (pid=%u).",
			    (unsigned int)pid);
		} else {
			pjdlog_error("Worker process failed (pid=%u, status=%d).",
			    (unsigned int)pid, WEXITSTATUS(status));
		}
		proto_close(res->hr_ctrl);
		res->hr_workerpid = 0;
		if (res->hr_role == HAST_ROLE_PRIMARY) {
			sleep(1);
			pjdlog_info("Restarting worker process.");
			hastd_primary(res);
		}
		pjdlog_prefix_set("%s", "");
	}
}

static void
hastd_reload(void)
{

	/* TODO */
	pjdlog_warning("Configuration reload is not implemented.");
}

static void
listen_accept(void)
{
	struct hast_resource *res;
	struct proto_conn *conn;
	struct nv *nvin, *nvout, *nverr;
	const char *resname;
	const unsigned char *token;
	char laddr[256], raddr[256];
	size_t size;
	pid_t pid;
	int status;

	proto_local_address(cfg->hc_listenconn, laddr, sizeof(laddr));
	pjdlog_debug(1, "Accepting connection to %s.", laddr);

	if (proto_accept(cfg->hc_listenconn, &conn) < 0) {
		pjdlog_errno(LOG_ERR, "Unable to accept connection %s", laddr);
		return;
	}

	proto_local_address(conn, laddr, sizeof(laddr));
	proto_remote_address(conn, raddr, sizeof(raddr));
	pjdlog_info("Connection from %s to %s.", laddr, raddr);

	nvin = nvout = nverr = NULL;

	/*
	 * Before receiving any data see if remote host have access to any
	 * resource.
	 */
	TAILQ_FOREACH(res, &cfg->hc_resources, hr_next) {
		if (proto_address_match(conn, res->hr_remoteaddr))
			break;
	}
	if (res == NULL) {
		pjdlog_error("Client %s isn't known.", raddr);
		goto close;
	}
	/* Ok, remote host can access at least one resource. */

	if (hast_proto_recv_hdr(conn, &nvin) < 0) {
		pjdlog_errno(LOG_ERR, "Unable to receive header from %s",
		    raddr);
		goto close;
	}

	resname = nv_get_string(nvin, "resource");
	if (resname == NULL) {
		pjdlog_error("No 'resource' field in the header received from %s.",
		    raddr);
		goto close;
	}
	pjdlog_debug(2, "%s: resource=%s", raddr, resname);
	token = nv_get_uint8_array(nvin, &size, "token");
	/*
	 * NULL token means that this is first conection.
	 */
	if (token != NULL && size != sizeof(res->hr_token)) {
		pjdlog_error("Received token of invalid size from %s (expected %zu, got %zu).",
		    raddr, sizeof(res->hr_token), size);
		goto close;
	}

	/*
	 * From now on we want to send errors to the remote node.
	 */
	nverr = nv_alloc();

	/* Find resource related to this connection. */
	TAILQ_FOREACH(res, &cfg->hc_resources, hr_next) {
		if (strcmp(resname, res->hr_name) == 0)
			break;
	}
	/* Have we found the resource? */
	if (res == NULL) {
		pjdlog_error("No resource '%s' as requested by %s.",
		    resname, raddr);
		nv_add_stringf(nverr, "errmsg", "Resource not configured.");
		goto fail;
	}

	/* Now that we know resource name setup log prefix. */
	pjdlog_prefix_set("[%s] (%s) ", res->hr_name, role2str(res->hr_role));

	/* Does the remote host have access to this resource? */
	if (!proto_address_match(conn, res->hr_remoteaddr)) {
		pjdlog_error("Client %s has no access to the resource.", raddr);
		nv_add_stringf(nverr, "errmsg", "No access to the resource.");
		goto fail;
	}
	/* Is the resource marked as secondary? */
	if (res->hr_role != HAST_ROLE_SECONDARY) {
		pjdlog_error("We act as %s for the resource and not as %s as requested by %s.",
		    role2str(res->hr_role), role2str(HAST_ROLE_SECONDARY),
		    raddr);
		nv_add_stringf(nverr, "errmsg",
		    "Remote node acts as %s for the resource and not as %s.",
		    role2str(res->hr_role), role2str(HAST_ROLE_SECONDARY));
		goto fail;
	}
	/* Does token (if exists) match? */
	if (token != NULL && memcmp(token, res->hr_token,
	    sizeof(res->hr_token)) != 0) {
		pjdlog_error("Token received from %s doesn't match.", raddr);
		nv_add_stringf(nverr, "errmsg", "Toke doesn't match.");
		goto fail;
	}
	/*
	 * If there is no token, but we have half-open connection
	 * (only remotein) or full connection (worker process is running)
	 * we have to cancel those and accept the new connection.
	 */
	if (token == NULL) {
		assert(res->hr_remoteout == NULL);
		pjdlog_debug(1, "Initial connection from %s.", raddr);
		if (res->hr_workerpid != 0) {
			assert(res->hr_remotein == NULL);
			pjdlog_debug(1,
			    "Worker process exists (pid=%u), stopping it.",
			    (unsigned int)res->hr_workerpid);
			/* Stop child process. */
			if (kill(res->hr_workerpid, SIGINT) < 0) {
				pjdlog_errno(LOG_ERR,
				    "Unable to stop worker process (pid=%u)",
				    (unsigned int)res->hr_workerpid);
				/*
				 * Other than logging the problem we
				 * ignore it - nothing smart to do.
				 */
			}
			/* Wait for it to exit. */
			else if ((pid = waitpid(res->hr_workerpid,
			    &status, 0)) != res->hr_workerpid) {
				pjdlog_errno(LOG_ERR,
				    "Waiting for worker process (pid=%u) failed",
				    (unsigned int)res->hr_workerpid);
				/* See above. */
			} else if (status != 0) {
				pjdlog_error("Worker process (pid=%u) exited ungracefully: status=%d.",
				    (unsigned int)res->hr_workerpid, status);
				/* See above. */
			} else {
				pjdlog_debug(1,
				    "Worker process (pid=%u) exited gracefully.",
				    (unsigned int)res->hr_workerpid);
			}
			res->hr_workerpid = 0;
		} else if (res->hr_remotein != NULL) {
			char oaddr[256];

			proto_remote_address(conn, oaddr, sizeof(oaddr));
			pjdlog_debug(1,
			    "Canceling half-open connection from %s on connection from %s.",
			    oaddr, raddr);
			proto_close(res->hr_remotein);
			res->hr_remotein = NULL;
		}
	}

	/*
	 * Checks and cleanups are done.
	 */

	if (token == NULL) {
		arc4random_buf(res->hr_token, sizeof(res->hr_token));
		nvout = nv_alloc();
		nv_add_uint8_array(nvout, res->hr_token,
		    sizeof(res->hr_token), "token");
		if (nv_error(nvout) != 0) {
			pjdlog_common(LOG_ERR, 0, nv_error(nvout),
			    "Unable to prepare return header for %s", raddr);
			nv_add_stringf(nverr, "errmsg",
			    "Remote node was unable to prepare return header: %s.",
			    strerror(nv_error(nvout)));
			goto fail;
		}
		if (hast_proto_send(NULL, conn, nvout, NULL, 0) < 0) {
			int error = errno;

			pjdlog_errno(LOG_ERR, "Unable to send response to %s",
			    raddr);
			nv_add_stringf(nverr, "errmsg",
			    "Remote node was unable to send response: %s.",
			    strerror(error));
			goto fail;
		}
		res->hr_remotein = conn;
		pjdlog_debug(1, "Incoming connection from %s configured.",
		    raddr);
	} else {
		res->hr_remoteout = conn;
		pjdlog_debug(1, "Outgoing connection to %s configured.", raddr);
		hastd_secondary(res, nvin);
	}
	nv_free(nvin);
	nv_free(nvout);
	nv_free(nverr);
	pjdlog_prefix_set("%s", "");
	return;
fail:
	if (nv_error(nverr) != 0) {
		pjdlog_common(LOG_ERR, 0, nv_error(nverr),
		    "Unable to prepare error header for %s", raddr);
		goto close;
	}
	if (hast_proto_send(NULL, conn, nverr, NULL, 0) < 0) {
		pjdlog_errno(LOG_ERR, "Unable to send error to %s", raddr);
		goto close;
	}
close:
	if (nvin != NULL)
		nv_free(nvin);
	if (nvout != NULL)
		nv_free(nvout);
	if (nverr != NULL)
		nv_free(nverr);
	proto_close(conn);
	pjdlog_prefix_set("%s", "");
}

static void
main_loop(void)
{
	fd_set rfds, wfds;
	int fd, maxfd, ret;

	for (;;) {
		if (sigchld_received) {
			sigchld_received = false;
			child_exit();
		}
		if (sighup_received) {
			sighup_received = false;
			hastd_reload();
		}

		maxfd = 0;
		FD_ZERO(&rfds);
		FD_ZERO(&wfds);

		/* Setup descriptors for select(2). */
#define	SETUP_FD(conn)	do {						\
	fd = proto_descriptor(conn);					\
	if (fd >= 0) {							\
		maxfd = fd > maxfd ? fd : maxfd;			\
		FD_SET(fd, &rfds);					\
		FD_SET(fd, &wfds);					\
	}								\
} while (0)
		SETUP_FD(cfg->hc_controlconn);
		SETUP_FD(cfg->hc_listenconn);
#undef	SETUP_FD

		ret = select(maxfd + 1, &rfds, &wfds, NULL, NULL);
		if (ret == -1) {
			if (errno == EINTR)
				continue;
			KEEP_ERRNO((void)pidfile_remove(pfh));
			pjdlog_exit(EX_OSERR, "select() failed");
		}

#define	ISSET_FD(conn)	\
	(FD_ISSET((fd = proto_descriptor(conn)), &rfds) || FD_ISSET(fd, &wfds))
		if (ISSET_FD(cfg->hc_controlconn))
			control_handle(cfg);
		if (ISSET_FD(cfg->hc_listenconn))
			listen_accept();
#undef	ISSET_FD
	}
}

int
main(int argc, char *argv[])
{
	const char *pidfile;
	pid_t otherpid;
	bool foreground;
	int debuglevel;

	g_gate_load();

	foreground = false;
	debuglevel = 0;
	pidfile = HASTD_PIDFILE;

	for (;;) {
		int ch;

		ch = getopt(argc, argv, "c:dFhP:");
		if (ch == -1)
			break;
		switch (ch) {
		case 'c':
			cfgpath = optarg;
			break;
		case 'd':
			debuglevel++;
			break;
		case 'F':
			foreground = true;
			break;
		case 'P':
			pidfile = optarg;
			break;
		case 'h':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	pjdlog_debug_set(debuglevel);

	pfh = pidfile_open(pidfile, 0600, &otherpid);
	if (pfh == NULL) {
		if (errno == EEXIST) {
			pjdlog_exitx(EX_TEMPFAIL,
			    "Another hastd is already running, pid: %jd.",
			    (intmax_t)otherpid);
		}
		/* If we cannot create pidfile from other reasons, only warn. */
		pjdlog_errno(LOG_WARNING, "Cannot open or create pidfile");
	}

	cfg = yy_config_parse(cfgpath);
	assert(cfg != NULL);

	signal(SIGHUP, sighandler);
	signal(SIGCHLD, sighandler);

	/* Listen on control address. */
	if (proto_server(cfg->hc_controladdr, &cfg->hc_controlconn) < 0) {
		KEEP_ERRNO((void)pidfile_remove(pfh));
		pjdlog_exit(EX_OSERR, "Unable to listen on control address %s",
		    cfg->hc_controladdr);
	}
	/* Listen for remote connections. */
	if (proto_server(cfg->hc_listenaddr, &cfg->hc_listenconn) < 0) {
		KEEP_ERRNO((void)pidfile_remove(pfh));
		pjdlog_exit(EX_OSERR, "Unable to listen on address %s",
		    cfg->hc_listenaddr);
	}

	if (!foreground) {
		if (daemon(0, 0) < 0) {
			KEEP_ERRNO((void)pidfile_remove(pfh));
			pjdlog_exit(EX_OSERR, "Unable to daemonize");
		}

		/* Start logging to syslog. */
		pjdlog_mode_set(PJDLOG_MODE_SYSLOG);

		/* Write PID to a file. */
		if (pidfile_write(pfh) < 0) {
			pjdlog_errno(LOG_WARNING,
			    "Unable to write PID to a file");
		}
	}

	main_loop();

	exit(0);
}
