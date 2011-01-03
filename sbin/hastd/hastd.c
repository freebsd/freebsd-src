/*-
 * Copyright (c) 2009-2010 The FreeBSD Foundation
 * Copyright (c) 2010 Pawel Jakub Dawidek <pjd@FreeBSD.org>
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
#include "event.h"
#include "hast.h"
#include "hast_proto.h"
#include "hastd.h"
#include "hooks.h"
#include "subr.h"

/* Path to configuration file. */
const char *cfgpath = HAST_CONFIG;
/* Hastd configuration. */
static struct hastd_config *cfg;
/* Was SIGINT or SIGTERM signal received? */
bool sigexit_received = false;
/* PID file handle. */
struct pidfh *pfh;

/* How often check for hooks running for too long. */
#define	REPORT_INTERVAL	5

static void
usage(void)
{

	errx(EX_USAGE, "[-dFh] [-c config] [-P pidfile]");
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
child_exit_log(unsigned int pid, int status)
{

	if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
		pjdlog_debug(1, "Worker process exited gracefully (pid=%u).",
		    pid);
	} else if (WIFSIGNALED(status)) {
		pjdlog_error("Worker process killed (pid=%u, signal=%d).",
		    pid, WTERMSIG(status));
	} else {
		pjdlog_error("Worker process exited ungracefully (pid=%u, exitcode=%d).",
		    pid, WIFEXITED(status) ? WEXITSTATUS(status) : -1);
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
			 * cancel child responsible for the old one or if this
			 * was hook which we executed.
			 */
			hook_check_one(pid, status);
			continue;
		}
		pjdlog_prefix_set("[%s] (%s) ", res->hr_name,
		    role2str(res->hr_role));
		child_exit_log(pid, status);
		child_cleanup(res);
		if (res->hr_role == HAST_ROLE_PRIMARY) {
			/*
			 * Restart child process if it was killed by signal
			 * or exited because of temporary problem.
			 */
			if (WIFSIGNALED(status) ||
			    (WIFEXITED(status) &&
			     WEXITSTATUS(status) == EX_TEMPFAIL)) {
				sleep(1);
				pjdlog_info("Restarting worker process.");
				hastd_primary(res);
			} else {
				res->hr_role = HAST_ROLE_INIT;
				pjdlog_info("Changing resource role back to %s.",
				    role2str(res->hr_role));
			}
		}
		pjdlog_prefix_set("%s", "");
	}
}

static bool
resource_needs_restart(const struct hast_resource *res0,
    const struct hast_resource *res1)
{

	assert(strcmp(res0->hr_name, res1->hr_name) == 0);

	if (strcmp(res0->hr_provname, res1->hr_provname) != 0)
		return (true);
	if (strcmp(res0->hr_localpath, res1->hr_localpath) != 0)
		return (true);
	if (res0->hr_role == HAST_ROLE_INIT ||
	    res0->hr_role == HAST_ROLE_SECONDARY) {
		if (strcmp(res0->hr_remoteaddr, res1->hr_remoteaddr) != 0)
			return (true);
		if (res0->hr_replication != res1->hr_replication)
			return (true);
		if (res0->hr_timeout != res1->hr_timeout)
			return (true);
		if (strcmp(res0->hr_exec, res1->hr_exec) != 0)
			return (true);
	}
	return (false);
}

static bool
resource_needs_reload(const struct hast_resource *res0,
    const struct hast_resource *res1)
{

	assert(strcmp(res0->hr_name, res1->hr_name) == 0);
	assert(strcmp(res0->hr_provname, res1->hr_provname) == 0);
	assert(strcmp(res0->hr_localpath, res1->hr_localpath) == 0);

	if (res0->hr_role != HAST_ROLE_PRIMARY)
		return (false);

	if (strcmp(res0->hr_remoteaddr, res1->hr_remoteaddr) != 0)
		return (true);
	if (res0->hr_replication != res1->hr_replication)
		return (true);
	if (res0->hr_timeout != res1->hr_timeout)
		return (true);
	if (strcmp(res0->hr_exec, res1->hr_exec) != 0)
		return (true);
	return (false);
}

static void
hastd_reload(void)
{
	struct hastd_config *newcfg;
	struct hast_resource *nres, *cres, *tres;
	uint8_t role;

	pjdlog_info("Reloading configuration...");

	newcfg = yy_config_parse(cfgpath, false);
	if (newcfg == NULL)
		goto failed;

	/*
	 * Check if control address has changed.
	 */
	if (strcmp(cfg->hc_controladdr, newcfg->hc_controladdr) != 0) {
		if (proto_server(newcfg->hc_controladdr,
		    &newcfg->hc_controlconn) < 0) {
			pjdlog_errno(LOG_ERR,
			    "Unable to listen on control address %s",
			    newcfg->hc_controladdr);
			goto failed;
		}
	}
	/*
	 * Check if listen address has changed.
	 */
	if (strcmp(cfg->hc_listenaddr, newcfg->hc_listenaddr) != 0) {
		if (proto_server(newcfg->hc_listenaddr,
		    &newcfg->hc_listenconn) < 0) {
			pjdlog_errno(LOG_ERR, "Unable to listen on address %s",
			    newcfg->hc_listenaddr);
			goto failed;
		}
	}
	/*
	 * Only when both control and listen sockets are successfully
	 * initialized switch them to new configuration.
	 */
	if (newcfg->hc_controlconn != NULL) {
		pjdlog_info("Control socket changed from %s to %s.",
		    cfg->hc_controladdr, newcfg->hc_controladdr);
		proto_close(cfg->hc_controlconn);
		cfg->hc_controlconn = newcfg->hc_controlconn;
		newcfg->hc_controlconn = NULL;
		strlcpy(cfg->hc_controladdr, newcfg->hc_controladdr,
		    sizeof(cfg->hc_controladdr));
	}
	if (newcfg->hc_listenconn != NULL) {
		pjdlog_info("Listen socket changed from %s to %s.",
		    cfg->hc_listenaddr, newcfg->hc_listenaddr);
		proto_close(cfg->hc_listenconn);
		cfg->hc_listenconn = newcfg->hc_listenconn;
		newcfg->hc_listenconn = NULL;
		strlcpy(cfg->hc_listenaddr, newcfg->hc_listenaddr,
		    sizeof(cfg->hc_listenaddr));
	}

	/*
	 * Stop and remove resources that were removed from the configuration.
	 */
	TAILQ_FOREACH_SAFE(cres, &cfg->hc_resources, hr_next, tres) {
		TAILQ_FOREACH(nres, &newcfg->hc_resources, hr_next) {
			if (strcmp(cres->hr_name, nres->hr_name) == 0)
				break;
		}
		if (nres == NULL) {
			control_set_role(cres, HAST_ROLE_INIT);
			TAILQ_REMOVE(&cfg->hc_resources, cres, hr_next);
			pjdlog_info("Resource %s removed.", cres->hr_name);
			free(cres);
		}
	}
	/*
	 * Move new resources to the current configuration.
	 */
	TAILQ_FOREACH_SAFE(nres, &newcfg->hc_resources, hr_next, tres) {
		TAILQ_FOREACH(cres, &cfg->hc_resources, hr_next) {
			if (strcmp(cres->hr_name, nres->hr_name) == 0)
				break;
		}
		if (cres == NULL) {
			TAILQ_REMOVE(&newcfg->hc_resources, nres, hr_next);
			TAILQ_INSERT_TAIL(&cfg->hc_resources, nres, hr_next);
			pjdlog_info("Resource %s added.", nres->hr_name);
		}
	}
	/*
	 * Deal with modified resources.
	 * Depending on what has changed exactly we might want to perform
	 * different actions.
	 *
	 * We do full resource restart in the following situations:
	 * Resource role is INIT or SECONDARY.
	 * Resource role is PRIMARY and path to local component or provider
	 * name has changed.
	 * In case of PRIMARY, the worker process will be killed and restarted,
	 * which also means removing /dev/hast/<name> provider and
	 * recreating it.
	 *
	 * We do just reload (send SIGHUP to worker process) if we act as
	 * PRIMARY, but only remote address, replication mode and timeout
	 * has changed. For those, there is no need to restart worker process.
	 * If PRIMARY receives SIGHUP, it will reconnect if remote address or
	 * replication mode has changed or simply set new timeout if only
	 * timeout has changed.
	 */
	TAILQ_FOREACH_SAFE(nres, &newcfg->hc_resources, hr_next, tres) {
		TAILQ_FOREACH(cres, &cfg->hc_resources, hr_next) {
			if (strcmp(cres->hr_name, nres->hr_name) == 0)
				break;
		}
		assert(cres != NULL);
		if (resource_needs_restart(cres, nres)) {
			pjdlog_info("Resource %s configuration was modified, restarting it.",
			    cres->hr_name);
			role = cres->hr_role;
			control_set_role(cres, HAST_ROLE_INIT);
			TAILQ_REMOVE(&cfg->hc_resources, cres, hr_next);
			free(cres);
			TAILQ_REMOVE(&newcfg->hc_resources, nres, hr_next);
			TAILQ_INSERT_TAIL(&cfg->hc_resources, nres, hr_next);
			control_set_role(nres, role);
		} else if (resource_needs_reload(cres, nres)) {
			pjdlog_info("Resource %s configuration was modified, reloading it.",
			    cres->hr_name);
			strlcpy(cres->hr_remoteaddr, nres->hr_remoteaddr,
			    sizeof(cres->hr_remoteaddr));
			cres->hr_replication = nres->hr_replication;
			cres->hr_timeout = nres->hr_timeout;
			if (cres->hr_workerpid != 0) {
				if (kill(cres->hr_workerpid, SIGHUP) < 0) {
					pjdlog_errno(LOG_WARNING,
					    "Unable to send SIGHUP to worker process %u",
					    (unsigned int)cres->hr_workerpid);
				}
			}
		}
	}

	yy_config_free(newcfg);
	pjdlog_info("Configuration reloaded successfully.");
	return;
failed:
	if (newcfg != NULL) {
		if (newcfg->hc_controlconn != NULL)
			proto_close(newcfg->hc_controlconn);
		if (newcfg->hc_listenconn != NULL)
			proto_close(newcfg->hc_listenconn);
		yy_config_free(newcfg);
	}
	pjdlog_warning("Configuration not reloaded.");
}

static void
terminate_workers(void)
{
	struct hast_resource *res;

	pjdlog_info("Termination signal received, exiting.");
	TAILQ_FOREACH(res, &cfg->hc_resources, hr_next) {
		if (res->hr_workerpid == 0)
			continue;
		pjdlog_info("Terminating worker process (resource=%s, role=%s, pid=%u).",
		    res->hr_name, role2str(res->hr_role), res->hr_workerpid);
		if (kill(res->hr_workerpid, SIGTERM) == 0)
			continue;
		pjdlog_errno(LOG_WARNING,
		    "Unable to send signal to worker process (resource=%s, role=%s, pid=%u).",
		    res->hr_name, role2str(res->hr_role), res->hr_workerpid);
	}
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
	pjdlog_info("Connection from %s to %s.", raddr, laddr);

	/* Error in setting timeout is not critical, but why should it fail? */
	if (proto_timeout(conn, HAST_TIMEOUT) < 0)
		pjdlog_errno(LOG_WARNING, "Unable to set connection timeout");

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
		nv_add_stringf(nverr, "errmsg", "Token doesn't match.");
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
				/* We can only log the problem. */
				pjdlog_errno(LOG_ERR,
				    "Waiting for worker process (pid=%u) failed",
				    (unsigned int)res->hr_workerpid);
			} else {
				child_exit_log(res->hr_workerpid, status);
			}
			child_cleanup(res);
		} else if (res->hr_remotein != NULL) {
			char oaddr[256];

			proto_remote_address(res->hr_remotein, oaddr,
			    sizeof(oaddr));
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
	struct hast_resource *res;
	struct timeval seltimeout;
	struct timespec sigtimeout;
	int fd, maxfd, ret, signo;
	sigset_t mask;
	fd_set rfds;

	seltimeout.tv_sec = REPORT_INTERVAL;
	seltimeout.tv_usec = 0;
	sigtimeout.tv_sec = 0;
	sigtimeout.tv_nsec = 0;

	PJDLOG_VERIFY(sigemptyset(&mask) == 0);
	PJDLOG_VERIFY(sigaddset(&mask, SIGHUP) == 0);
	PJDLOG_VERIFY(sigaddset(&mask, SIGINT) == 0);
	PJDLOG_VERIFY(sigaddset(&mask, SIGTERM) == 0);
	PJDLOG_VERIFY(sigaddset(&mask, SIGCHLD) == 0);

	pjdlog_info("Started successfully, running protocol version %d.",
	    HAST_PROTO_VERSION);

	for (;;) {
		while ((signo = sigtimedwait(&mask, NULL, &sigtimeout)) != -1) {
			switch (signo) {
			case SIGINT:
			case SIGTERM:
				sigexit_received = true;
				terminate_workers();
				exit(EX_OK);
				break;
			case SIGCHLD:
				child_exit();
				break;
			case SIGHUP:
				hastd_reload();
				break;
			default:
				assert(!"invalid condition");
			}
		}

		/* Setup descriptors for select(2). */
		FD_ZERO(&rfds);
		maxfd = fd = proto_descriptor(cfg->hc_controlconn);
		assert(fd >= 0);
		FD_SET(fd, &rfds);
		fd = proto_descriptor(cfg->hc_listenconn);
		assert(fd >= 0);
		FD_SET(fd, &rfds);
		maxfd = fd > maxfd ? fd : maxfd;
		TAILQ_FOREACH(res, &cfg->hc_resources, hr_next) {
			if (res->hr_event == NULL)
				continue;
			fd = proto_descriptor(res->hr_event);
			assert(fd >= 0);
			FD_SET(fd, &rfds);
			maxfd = fd > maxfd ? fd : maxfd;
		}

		assert(maxfd + 1 <= (int)FD_SETSIZE);
		ret = select(maxfd + 1, &rfds, NULL, NULL, &seltimeout);
		if (ret == 0)
			hook_check();
		else if (ret == -1) {
			if (errno == EINTR)
				continue;
			KEEP_ERRNO((void)pidfile_remove(pfh));
			pjdlog_exit(EX_OSERR, "select() failed");
		}

		if (FD_ISSET(proto_descriptor(cfg->hc_controlconn), &rfds))
			control_handle(cfg);
		if (FD_ISSET(proto_descriptor(cfg->hc_listenconn), &rfds))
			listen_accept();
		TAILQ_FOREACH(res, &cfg->hc_resources, hr_next) {
			if (res->hr_event == NULL)
				continue;
			if (FD_ISSET(proto_descriptor(res->hr_event), &rfds)) {
				if (event_recv(res) == 0)
					continue;
				/* The worker process exited? */
				proto_close(res->hr_event);
				res->hr_event = NULL;
			}
		}
	}
}

static void
dummy_sighandler(int sig __unused)
{
	/* Nothing to do. */
}

int
main(int argc, char *argv[])
{
	const char *pidfile;
	pid_t otherpid;
	bool foreground;
	int debuglevel;
	sigset_t mask;

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

	g_gate_load();

	pfh = pidfile_open(pidfile, 0600, &otherpid);
	if (pfh == NULL) {
		if (errno == EEXIST) {
			pjdlog_exitx(EX_TEMPFAIL,
			    "Another hastd is already running, pid: %jd.",
			    (intmax_t)otherpid);
		}
		/* If we cannot create pidfile from other reasons, only warn. */
		pjdlog_errno(LOG_WARNING, "Unable to open or create pidfile");
	}

	cfg = yy_config_parse(cfgpath, true);
	assert(cfg != NULL);

	/*
	 * Because SIGCHLD is ignored by default, setup dummy handler for it,
	 * so we can mask it.
	 */
	PJDLOG_VERIFY(signal(SIGCHLD, dummy_sighandler) != SIG_ERR);
	PJDLOG_VERIFY(sigemptyset(&mask) == 0);
	PJDLOG_VERIFY(sigaddset(&mask, SIGHUP) == 0);
	PJDLOG_VERIFY(sigaddset(&mask, SIGINT) == 0);
	PJDLOG_VERIFY(sigaddset(&mask, SIGTERM) == 0);
	PJDLOG_VERIFY(sigaddset(&mask, SIGCHLD) == 0);
	PJDLOG_VERIFY(sigprocmask(SIG_SETMASK, &mask, NULL) == 0);

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

	hook_init();

	main_loop();

	exit(0);
}
