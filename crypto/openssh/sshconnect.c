/* $OpenBSD: sshconnect.c,v 1.368 2024/04/30 02:10:49 djm Exp $ */
/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * Code to connect to a remote host, and to perform the client side of the
 * login (authentication) dialog.
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 */

#include "includes.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/socket.h>
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif

#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netdb.h>
#ifdef HAVE_PATHS_H
#include <paths.h>
#endif
#include <pwd.h>
#ifdef HAVE_POLL_H
#include <poll.h>
#endif
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#ifdef HAVE_IFADDRS_H
# include <ifaddrs.h>
#endif

#include "xmalloc.h"
#include "hostfile.h"
#include "ssh.h"
#include "sshbuf.h"
#include "packet.h"
#include "sshkey.h"
#include "sshconnect.h"
#include "log.h"
#include "match.h"
#include "misc.h"
#include "readconf.h"
#include "atomicio.h"
#include "dns.h"
#include "monitor_fdpass.h"
#include "ssh2.h"
#include "version.h"
#include "authfile.h"
#include "ssherr.h"
#include "authfd.h"
#include "kex.h"

struct sshkey *previous_host_key = NULL;

static int matching_host_key_dns = 0;

static pid_t proxy_command_pid = 0;

/* import */
extern int debug_flag;
extern Options options;
extern char *__progname;

static int show_other_keys(struct hostkeys *, struct sshkey *);
static void warn_changed_key(struct sshkey *);

/* Expand a proxy command */
static char *
expand_proxy_command(const char *proxy_command, const char *user,
    const char *host, const char *host_arg, int port)
{
	char *tmp, *ret, strport[NI_MAXSERV];
	const char *keyalias = options.host_key_alias ?
	    options.host_key_alias : host_arg;

	snprintf(strport, sizeof strport, "%d", port);
	xasprintf(&tmp, "exec %s", proxy_command);
	ret = percent_expand(tmp,
	    "h", host,
	    "k", keyalias,
	    "n", host_arg,
	    "p", strport,
	    "r", options.user,
	    (char *)NULL);
	free(tmp);
	return ret;
}

/*
 * Connect to the given ssh server using a proxy command that passes a
 * a connected fd back to us.
 */
static int
ssh_proxy_fdpass_connect(struct ssh *ssh, const char *host,
    const char *host_arg, u_short port, const char *proxy_command)
{
	char *command_string;
	int sp[2], sock;
	pid_t pid;
	char *shell;

	if ((shell = getenv("SHELL")) == NULL)
		shell = _PATH_BSHELL;

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sp) == -1)
		fatal("Could not create socketpair to communicate with "
		    "proxy dialer: %.100s", strerror(errno));

	command_string = expand_proxy_command(proxy_command, options.user,
	    host, host_arg, port);
	debug("Executing proxy dialer command: %.500s", command_string);

	/* Fork and execute the proxy command. */
	if ((pid = fork()) == 0) {
		char *argv[10];

		close(sp[1]);
		/* Redirect stdin and stdout. */
		if (sp[0] != 0) {
			if (dup2(sp[0], 0) == -1)
				perror("dup2 stdin");
		}
		if (sp[0] != 1) {
			if (dup2(sp[0], 1) == -1)
				perror("dup2 stdout");
		}
		if (sp[0] >= 2)
			close(sp[0]);

		/*
		 * Stderr is left for non-ControlPersist connections is so
		 * error messages may be printed on the user's terminal.
		 */
		if (!debug_flag && options.control_path != NULL &&
		    options.control_persist && stdfd_devnull(0, 0, 1) == -1)
			error_f("stdfd_devnull failed");

		argv[0] = shell;
		argv[1] = "-c";
		argv[2] = command_string;
		argv[3] = NULL;

		/*
		 * Execute the proxy command.
		 * Note that we gave up any extra privileges above.
		 */
		execv(argv[0], argv);
		perror(argv[0]);
		exit(1);
	}
	/* Parent. */
	if (pid == -1)
		fatal("fork failed: %.100s", strerror(errno));
	close(sp[0]);
	free(command_string);

	if ((sock = mm_receive_fd(sp[1])) == -1)
		fatal("proxy dialer did not pass back a connection");
	close(sp[1]);

	while (waitpid(pid, NULL, 0) == -1)
		if (errno != EINTR)
			fatal("Couldn't wait for child: %s", strerror(errno));

	/* Set the connection file descriptors. */
	if (ssh_packet_set_connection(ssh, sock, sock) == NULL)
		return -1; /* ssh_packet_set_connection logs error */

	return 0;
}

/*
 * Connect to the given ssh server using a proxy command.
 */
static int
ssh_proxy_connect(struct ssh *ssh, const char *host, const char *host_arg,
    u_short port, const char *proxy_command)
{
	char *command_string;
	int pin[2], pout[2];
	pid_t pid;
	char *shell;

	if ((shell = getenv("SHELL")) == NULL || *shell == '\0')
		shell = _PATH_BSHELL;

	/* Create pipes for communicating with the proxy. */
	if (pipe(pin) == -1 || pipe(pout) == -1)
		fatal("Could not create pipes to communicate with the proxy: %.100s",
		    strerror(errno));

	command_string = expand_proxy_command(proxy_command, options.user,
	    host, host_arg, port);
	debug("Executing proxy command: %.500s", command_string);

	/* Fork and execute the proxy command. */
	if ((pid = fork()) == 0) {
		char *argv[10];

		/* Redirect stdin and stdout. */
		close(pin[1]);
		if (pin[0] != 0) {
			if (dup2(pin[0], 0) == -1)
				perror("dup2 stdin");
			close(pin[0]);
		}
		close(pout[0]);
		if (dup2(pout[1], 1) == -1)
			perror("dup2 stdout");
		/* Cannot be 1 because pin allocated two descriptors. */
		close(pout[1]);

		/*
		 * Stderr is left for non-ControlPersist connections is so
		 * error messages may be printed on the user's terminal.
		 */
		if (!debug_flag && options.control_path != NULL &&
		    options.control_persist && stdfd_devnull(0, 0, 1) == -1)
			error_f("stdfd_devnull failed");

		argv[0] = shell;
		argv[1] = "-c";
		argv[2] = command_string;
		argv[3] = NULL;

		/*
		 * Execute the proxy command.  Note that we gave up any
		 * extra privileges above.
		 */
		ssh_signal(SIGPIPE, SIG_DFL);
		execv(argv[0], argv);
		perror(argv[0]);
		exit(1);
	}
	/* Parent. */
	if (pid == -1)
		fatal("fork failed: %.100s", strerror(errno));
	else
		proxy_command_pid = pid; /* save pid to clean up later */

	/* Close child side of the descriptors. */
	close(pin[0]);
	close(pout[1]);

	/* Free the command name. */
	free(command_string);

	/* Set the connection file descriptors. */
	if (ssh_packet_set_connection(ssh, pout[0], pin[1]) == NULL)
		return -1; /* ssh_packet_set_connection logs error */

	return 0;
}

void
ssh_kill_proxy_command(void)
{
	/*
	 * Send SIGHUP to proxy command if used. We don't wait() in
	 * case it hangs and instead rely on init to reap the child
	 */
	if (proxy_command_pid > 1)
		kill(proxy_command_pid, SIGHUP);
}

#ifdef HAVE_IFADDRS_H
/*
 * Search a interface address list (returned from getifaddrs(3)) for an
 * address that matches the desired address family on the specified interface.
 * Returns 0 and fills in *resultp and *rlenp on success. Returns -1 on failure.
 */
static int
check_ifaddrs(const char *ifname, int af, const struct ifaddrs *ifaddrs,
    struct sockaddr_storage *resultp, socklen_t *rlenp)
{
	struct sockaddr_in6 *sa6;
	struct sockaddr_in *sa;
	struct in6_addr *v6addr;
	const struct ifaddrs *ifa;
	int allow_local;

	/*
	 * Prefer addresses that are not loopback or linklocal, but use them
	 * if nothing else matches.
	 */
	for (allow_local = 0; allow_local < 2; allow_local++) {
		for (ifa = ifaddrs; ifa != NULL; ifa = ifa->ifa_next) {
			if (ifa->ifa_addr == NULL || ifa->ifa_name == NULL ||
			    (ifa->ifa_flags & IFF_UP) == 0 ||
			    ifa->ifa_addr->sa_family != af ||
			    strcmp(ifa->ifa_name, options.bind_interface) != 0)
				continue;
			switch (ifa->ifa_addr->sa_family) {
			case AF_INET:
				sa = (struct sockaddr_in *)ifa->ifa_addr;
				if (!allow_local && sa->sin_addr.s_addr ==
				    htonl(INADDR_LOOPBACK))
					continue;
				if (*rlenp < sizeof(struct sockaddr_in)) {
					error_f("v4 addr doesn't fit");
					return -1;
				}
				*rlenp = sizeof(struct sockaddr_in);
				memcpy(resultp, sa, *rlenp);
				return 0;
			case AF_INET6:
				sa6 = (struct sockaddr_in6 *)ifa->ifa_addr;
				v6addr = &sa6->sin6_addr;
				if (!allow_local &&
				    (IN6_IS_ADDR_LINKLOCAL(v6addr) ||
				    IN6_IS_ADDR_LOOPBACK(v6addr)))
					continue;
				if (*rlenp < sizeof(struct sockaddr_in6)) {
					error_f("v6 addr doesn't fit");
					return -1;
				}
				*rlenp = sizeof(struct sockaddr_in6);
				memcpy(resultp, sa6, *rlenp);
				return 0;
			}
		}
	}
	return -1;
}
#endif

/*
 * Creates a socket for use as the ssh connection.
 */
static int
ssh_create_socket(struct addrinfo *ai)
{
	int sock, r;
	struct sockaddr_storage bindaddr;
	socklen_t bindaddrlen = 0;
	struct addrinfo hints, *res = NULL;
#ifdef HAVE_IFADDRS_H
	struct ifaddrs *ifaddrs = NULL;
#endif
	char ntop[NI_MAXHOST];

	sock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
	if (sock == -1) {
		error("socket: %s", strerror(errno));
		return -1;
	}
	(void)fcntl(sock, F_SETFD, FD_CLOEXEC);

	/* Use interactive QOS (if specified) until authentication completed */
	if (options.ip_qos_interactive != INT_MAX)
		set_sock_tos(sock, options.ip_qos_interactive);

	/* Bind the socket to an alternative local IP address */
	if (options.bind_address == NULL && options.bind_interface == NULL)
		return sock;

	if (options.bind_address != NULL) {
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = ai->ai_family;
		hints.ai_socktype = ai->ai_socktype;
		hints.ai_protocol = ai->ai_protocol;
		hints.ai_flags = AI_PASSIVE;
		if ((r = getaddrinfo(options.bind_address, NULL,
		    &hints, &res)) != 0) {
			error("getaddrinfo: %s: %s", options.bind_address,
			    ssh_gai_strerror(r));
			goto fail;
		}
		if (res == NULL) {
			error("getaddrinfo: no addrs");
			goto fail;
		}
		memcpy(&bindaddr, res->ai_addr, res->ai_addrlen);
		bindaddrlen = res->ai_addrlen;
	} else if (options.bind_interface != NULL) {
#ifdef HAVE_IFADDRS_H
		if ((r = getifaddrs(&ifaddrs)) != 0) {
			error("getifaddrs: %s: %s", options.bind_interface,
			    strerror(errno));
			goto fail;
		}
		bindaddrlen = sizeof(bindaddr);
		if (check_ifaddrs(options.bind_interface, ai->ai_family,
		    ifaddrs, &bindaddr, &bindaddrlen) != 0) {
			logit("getifaddrs: %s: no suitable addresses",
			    options.bind_interface);
			goto fail;
		}
#else
		error("BindInterface not supported on this platform.");
#endif
	}
	if ((r = getnameinfo((struct sockaddr *)&bindaddr, bindaddrlen,
	    ntop, sizeof(ntop), NULL, 0, NI_NUMERICHOST)) != 0) {
		error_f("getnameinfo failed: %s", ssh_gai_strerror(r));
		goto fail;
	}
	if (bind(sock, (struct sockaddr *)&bindaddr, bindaddrlen) != 0) {
		error("bind %s: %s", ntop, strerror(errno));
		goto fail;
	}
	debug_f("bound to %s", ntop);
	/* success */
	goto out;
fail:
	close(sock);
	sock = -1;
 out:
	if (res != NULL)
		freeaddrinfo(res);
#ifdef HAVE_IFADDRS_H
	if (ifaddrs != NULL)
		freeifaddrs(ifaddrs);
#endif
	return sock;
}

/*
 * Opens a TCP/IP connection to the remote server on the given host.
 * The address of the remote host will be returned in hostaddr.
 * If port is 0, the default port will be used.
 * Connection_attempts specifies the maximum number of tries (one per
 * second).  If proxy_command is non-NULL, it specifies the command (with %h
 * and %p substituted for host and port, respectively) to use to contact
 * the daemon.
 */
static int
ssh_connect_direct(struct ssh *ssh, const char *host, struct addrinfo *aitop,
    struct sockaddr_storage *hostaddr, u_short port, int connection_attempts,
    int *timeout_ms, int want_keepalive)
{
	int on = 1, saved_timeout_ms = *timeout_ms;
	int oerrno, sock = -1, attempt;
	char ntop[NI_MAXHOST], strport[NI_MAXSERV];
	struct addrinfo *ai;

	debug3_f("entering");
	memset(ntop, 0, sizeof(ntop));
	memset(strport, 0, sizeof(strport));

	for (attempt = 0; attempt < connection_attempts; attempt++) {
		if (attempt > 0) {
			/* Sleep a moment before retrying. */
			sleep(1);
			debug("Trying again...");
		}
		/*
		 * Loop through addresses for this host, and try each one in
		 * sequence until the connection succeeds.
		 */
		for (ai = aitop; ai; ai = ai->ai_next) {
			if (ai->ai_family != AF_INET &&
			    ai->ai_family != AF_INET6) {
				errno = EAFNOSUPPORT;
				continue;
			}
			if (getnameinfo(ai->ai_addr, ai->ai_addrlen,
			    ntop, sizeof(ntop), strport, sizeof(strport),
			    NI_NUMERICHOST|NI_NUMERICSERV) != 0) {
				oerrno = errno;
				error_f("getnameinfo failed");
				errno = oerrno;
				continue;
			}
			if (options.address_family != AF_UNSPEC &&
			    ai->ai_family != options.address_family) {
				debug2_f("skipping address [%s]:%s: "
				    "wrong address family", ntop, strport);
				errno = EAFNOSUPPORT;
				continue;
			}

			debug("Connecting to %.200s [%.100s] port %s.",
				host, ntop, strport);

			/* Create a socket for connecting. */
			sock = ssh_create_socket(ai);
			if (sock < 0) {
				/* Any error is already output */
				errno = 0;
				continue;
			}

			*timeout_ms = saved_timeout_ms;
			if (timeout_connect(sock, ai->ai_addr, ai->ai_addrlen,
			    timeout_ms) >= 0) {
				/* Successful connection. */
				memcpy(hostaddr, ai->ai_addr, ai->ai_addrlen);
				break;
			} else {
				oerrno = errno;
				debug("connect to address %s port %s: %s",
				    ntop, strport, strerror(errno));
				close(sock);
				sock = -1;
				errno = oerrno;
			}
		}
		if (sock != -1)
			break;	/* Successful connection. */
	}

	/* Return failure if we didn't get a successful connection. */
	if (sock == -1) {
		error("ssh: connect to host %s port %s: %s",
		    host, strport, errno == 0 ? "failure" : strerror(errno));
		return -1;
	}

	debug("Connection established.");

	/* Set SO_KEEPALIVE if requested. */
	if (want_keepalive &&
	    setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (void *)&on,
	    sizeof(on)) == -1)
		error("setsockopt SO_KEEPALIVE: %.100s", strerror(errno));

	/* Set the connection. */
	if (ssh_packet_set_connection(ssh, sock, sock) == NULL)
		return -1; /* ssh_packet_set_connection logs error */

	return 0;
}

int
ssh_connect(struct ssh *ssh, const char *host, const char *host_arg,
    struct addrinfo *addrs, struct sockaddr_storage *hostaddr, u_short port,
    int connection_attempts, int *timeout_ms, int want_keepalive)
{
	int in, out;

	if (options.proxy_command == NULL) {
		return ssh_connect_direct(ssh, host, addrs, hostaddr, port,
		    connection_attempts, timeout_ms, want_keepalive);
	} else if (strcmp(options.proxy_command, "-") == 0) {
		if ((in = dup(STDIN_FILENO)) == -1 ||
		    (out = dup(STDOUT_FILENO)) == -1) {
			if (in >= 0)
				close(in);
			error_f("dup() in/out failed");
			return -1; /* ssh_packet_set_connection logs error */
		}
		if ((ssh_packet_set_connection(ssh, in, out)) == NULL)
			return -1; /* ssh_packet_set_connection logs error */
		return 0;
	} else if (options.proxy_use_fdpass) {
		return ssh_proxy_fdpass_connect(ssh, host, host_arg, port,
		    options.proxy_command);
	}
	return ssh_proxy_connect(ssh, host, host_arg, port,
	    options.proxy_command);
}

/* defaults to 'no' */
static int
confirm(const char *prompt, const char *fingerprint)
{
	const char *msg, *again = "Please type 'yes' or 'no': ";
	const char *again_fp = "Please type 'yes', 'no' or the fingerprint: ";
	char *p, *cp;
	int ret = -1;

	if (options.batch_mode)
		return 0;
	for (msg = prompt;;msg = fingerprint ? again_fp : again) {
		cp = p = read_passphrase(msg, RP_ECHO);
		if (p == NULL)
			return 0;
		p += strspn(p, " \t"); /* skip leading whitespace */
		p[strcspn(p, " \t\n")] = '\0'; /* remove trailing whitespace */
		if (p[0] == '\0' || strcasecmp(p, "no") == 0)
			ret = 0;
		else if (strcasecmp(p, "yes") == 0 || (fingerprint != NULL &&
		    strcmp(p, fingerprint) == 0))
			ret = 1;
		free(cp);
		if (ret != -1)
			return ret;
	}
}

static int
sockaddr_is_local(struct sockaddr *hostaddr)
{
	switch (hostaddr->sa_family) {
	case AF_INET:
		return (ntohl(((struct sockaddr_in *)hostaddr)->
		    sin_addr.s_addr) >> 24) == IN_LOOPBACKNET;
	case AF_INET6:
		return IN6_IS_ADDR_LOOPBACK(
		    &(((struct sockaddr_in6 *)hostaddr)->sin6_addr));
	default:
		return 0;
	}
}

/*
 * Prepare the hostname and ip address strings that are used to lookup
 * host keys in known_hosts files. These may have a port number appended.
 */
void
get_hostfile_hostname_ipaddr(char *hostname, struct sockaddr *hostaddr,
    u_short port, char **hostfile_hostname, char **hostfile_ipaddr)
{
	char ntop[NI_MAXHOST];
	socklen_t addrlen;

	switch (hostaddr == NULL ? -1 : hostaddr->sa_family) {
	case -1:
		addrlen = 0;
		break;
	case AF_INET:
		addrlen = sizeof(struct sockaddr_in);
		break;
	case AF_INET6:
		addrlen = sizeof(struct sockaddr_in6);
		break;
	default:
		addrlen = sizeof(struct sockaddr);
		break;
	}

	/*
	 * We don't have the remote ip-address for connections
	 * using a proxy command
	 */
	if (hostfile_ipaddr != NULL) {
		if (options.proxy_command == NULL) {
			if (getnameinfo(hostaddr, addrlen,
			    ntop, sizeof(ntop), NULL, 0, NI_NUMERICHOST) != 0)
				fatal_f("getnameinfo failed");
			*hostfile_ipaddr = put_host_port(ntop, port);
		} else {
			*hostfile_ipaddr = xstrdup("<no hostip for proxy "
			    "command>");
		}
	}

	/*
	 * Allow the user to record the key under a different name or
	 * differentiate a non-standard port.  This is useful for ssh
	 * tunneling over forwarded connections or if you run multiple
	 * sshd's on different ports on the same machine.
	 */
	if (hostfile_hostname != NULL) {
		if (options.host_key_alias != NULL) {
			*hostfile_hostname = xstrdup(options.host_key_alias);
			debug("using hostkeyalias: %s", *hostfile_hostname);
		} else {
			*hostfile_hostname = put_host_port(hostname, port);
		}
	}
}

/* returns non-zero if path appears in hostfiles, or 0 if not. */
static int
path_in_hostfiles(const char *path, char **hostfiles, u_int num_hostfiles)
{
	u_int i;

	for (i = 0; i < num_hostfiles; i++) {
		if (strcmp(path, hostfiles[i]) == 0)
			return 1;
	}
	return 0;
}

struct find_by_key_ctx {
	const char *host, *ip;
	const struct sshkey *key;
	char **names;
	u_int nnames;
};

/* Try to replace home directory prefix (per $HOME) with a ~/ sequence */
static char *
try_tilde_unexpand(const char *path)
{
	char *home, *ret = NULL;
	size_t l;

	if (*path != '/')
		return xstrdup(path);
	if ((home = getenv("HOME")) == NULL || (l = strlen(home)) == 0)
		return xstrdup(path);
	if (strncmp(path, home, l) != 0)
		return xstrdup(path);
	/*
	 * ensure we have matched on a path boundary: either the $HOME that
	 * we just compared ends with a '/' or the next character of the path
	 * must be a '/'.
	 */
	if (home[l - 1] != '/' && path[l] != '/')
		return xstrdup(path);
	if (path[l] == '/')
		l++;
	xasprintf(&ret, "~/%s", path + l);
	return ret;
}

/*
 * Returns non-zero if the key is accepted by HostkeyAlgorithms.
 * Made slightly less trivial by the multiple RSA signature algorithm names.
 */
int
hostkey_accepted_by_hostkeyalgs(const struct sshkey *key)
{
	const char *ktype = sshkey_ssh_name(key);
	const char *hostkeyalgs = options.hostkeyalgorithms;

	if (key->type == KEY_UNSPEC)
		return 0;
	if (key->type == KEY_RSA &&
	    (match_pattern_list("rsa-sha2-256", hostkeyalgs, 0) == 1 ||
	    match_pattern_list("rsa-sha2-512", hostkeyalgs, 0) == 1))
		return 1;
	if (key->type == KEY_RSA_CERT &&
	    (match_pattern_list("rsa-sha2-512-cert-v01@openssh.com", hostkeyalgs, 0) == 1 ||
	    match_pattern_list("rsa-sha2-256-cert-v01@openssh.com", hostkeyalgs, 0) == 1))
		return 1;
	return match_pattern_list(ktype, hostkeyalgs, 0) == 1;
}

static int
hostkeys_find_by_key_cb(struct hostkey_foreach_line *l, void *_ctx)
{
	struct find_by_key_ctx *ctx = (struct find_by_key_ctx *)_ctx;
	char *path;

	/* we are looking for keys with names that *do not* match */
	if ((l->match & HKF_MATCH_HOST) != 0)
		return 0;
	/* not interested in marker lines */
	if (l->marker != MRK_NONE)
		return 0;
	/* we are only interested in exact key matches */
	if (l->key == NULL || !sshkey_equal(ctx->key, l->key))
		return 0;
	path = try_tilde_unexpand(l->path);
	debug_f("found matching key in %s:%lu", path, l->linenum);
	ctx->names = xrecallocarray(ctx->names,
	    ctx->nnames, ctx->nnames + 1, sizeof(*ctx->names));
	xasprintf(&ctx->names[ctx->nnames], "%s:%lu: %s", path, l->linenum,
	    strncmp(l->hosts, HASH_MAGIC, strlen(HASH_MAGIC)) == 0 ?
	    "[hashed name]" : l->hosts);
	ctx->nnames++;
	free(path);
	return 0;
}

static int
hostkeys_find_by_key_hostfile(const char *file, const char *which,
    struct find_by_key_ctx *ctx)
{
	int r;

	debug3_f("trying %s hostfile \"%s\"", which, file);
	if ((r = hostkeys_foreach(file, hostkeys_find_by_key_cb, ctx,
	    ctx->host, ctx->ip, HKF_WANT_PARSE_KEY, 0)) != 0) {
		if (r == SSH_ERR_SYSTEM_ERROR && errno == ENOENT) {
			debug_f("hostkeys file %s does not exist", file);
			return 0;
		}
		error_fr(r, "hostkeys_foreach failed for %s", file);
		return r;
	}
	return 0;
}

/*
 * Find 'key' in known hosts file(s) that do not match host/ip.
 * Used to display also-known-as information for previously-unseen hostkeys.
 */
static void
hostkeys_find_by_key(const char *host, const char *ip, const struct sshkey *key,
    char **user_hostfiles, u_int num_user_hostfiles,
    char **system_hostfiles, u_int num_system_hostfiles,
    char ***names, u_int *nnames)
{
	struct find_by_key_ctx ctx = {0, 0, 0, 0, 0};
	u_int i;

	*names = NULL;
	*nnames = 0;

	if (key == NULL || sshkey_is_cert(key))
		return;

	ctx.host = host;
	ctx.ip = ip;
	ctx.key = key;

	for (i = 0; i < num_user_hostfiles; i++) {
		if (hostkeys_find_by_key_hostfile(user_hostfiles[i],
		    "user", &ctx) != 0)
			goto fail;
	}
	for (i = 0; i < num_system_hostfiles; i++) {
		if (hostkeys_find_by_key_hostfile(system_hostfiles[i],
		    "system", &ctx) != 0)
			goto fail;
	}
	/* success */
	*names = ctx.names;
	*nnames = ctx.nnames;
	ctx.names = NULL;
	ctx.nnames = 0;
	return;
 fail:
	for (i = 0; i < ctx.nnames; i++)
		free(ctx.names[i]);
	free(ctx.names);
}

#define MAX_OTHER_NAMES	8 /* Maximum number of names to list */
static char *
other_hostkeys_message(const char *host, const char *ip,
    const struct sshkey *key,
    char **user_hostfiles, u_int num_user_hostfiles,
    char **system_hostfiles, u_int num_system_hostfiles)
{
	char *ret = NULL, **othernames = NULL;
	u_int i, n, num_othernames = 0;

	hostkeys_find_by_key(host, ip, key,
	    user_hostfiles, num_user_hostfiles,
	    system_hostfiles, num_system_hostfiles,
	    &othernames, &num_othernames);
	if (num_othernames == 0)
		return xstrdup("This key is not known by any other names.");

	xasprintf(&ret, "This host key is known by the following other "
	    "names/addresses:");

	n = num_othernames;
	if (n > MAX_OTHER_NAMES)
		n = MAX_OTHER_NAMES;
	for (i = 0; i < n; i++) {
		xextendf(&ret, "\n", "    %s", othernames[i]);
	}
	if (n < num_othernames) {
		xextendf(&ret, "\n", "    (%d additional names omitted)",
		    num_othernames - n);
	}
	for (i = 0; i < num_othernames; i++)
		free(othernames[i]);
	free(othernames);
	return ret;
}

void
load_hostkeys_command(struct hostkeys *hostkeys, const char *command_template,
    const char *invocation, const struct ssh_conn_info *cinfo,
    const struct sshkey *host_key, const char *hostfile_hostname)
{
	int r, i, ac = 0;
	char *key_fp = NULL, *keytext = NULL, *tmp;
	char *command = NULL, *tag = NULL, **av = NULL;
	FILE *f = NULL;
	pid_t pid;
	void (*osigchld)(int);

	xasprintf(&tag, "KnownHostsCommand-%s", invocation);

	if (host_key != NULL) {
		if ((key_fp = sshkey_fingerprint(host_key,
		    options.fingerprint_hash, SSH_FP_DEFAULT)) == NULL)
			fatal_f("sshkey_fingerprint failed");
		if ((r = sshkey_to_base64(host_key, &keytext)) != 0)
			fatal_fr(r, "sshkey_to_base64 failed");
	}
	/*
	 * NB. all returns later this function should go via "out" to
	 * ensure the original SIGCHLD handler is restored properly.
	 */
	osigchld = ssh_signal(SIGCHLD, SIG_DFL);

	/* Turn the command into an argument vector */
	if (argv_split(command_template, &ac, &av, 0) != 0) {
		error("%s \"%s\" contains invalid quotes", tag,
		    command_template);
		goto out;
	}
	if (ac == 0) {
		error("%s \"%s\" yielded no arguments", tag,
		    command_template);
		goto out;
	}
	for (i = 1; i < ac; i++) {
		tmp = percent_dollar_expand(av[i],
		    DEFAULT_CLIENT_PERCENT_EXPAND_ARGS(cinfo),
		    "H", hostfile_hostname,
		    "I", invocation,
		    "t", host_key == NULL ? "NONE" : sshkey_ssh_name(host_key),
		    "f", key_fp == NULL ? "NONE" : key_fp,
		    "K", keytext == NULL ? "NONE" : keytext,
		    (char *)NULL);
		if (tmp == NULL)
			fatal_f("percent_expand failed");
		free(av[i]);
		av[i] = tmp;
	}
	/* Prepare a printable command for logs, etc. */
	command = argv_assemble(ac, av);

	if ((pid = subprocess(tag, command, ac, av, &f,
	    SSH_SUBPROCESS_STDOUT_CAPTURE|SSH_SUBPROCESS_UNSAFE_PATH|
	    SSH_SUBPROCESS_PRESERVE_ENV, NULL, NULL, NULL)) == 0)
		goto out;

	load_hostkeys_file(hostkeys, hostfile_hostname, tag, f, 1);

	if (exited_cleanly(pid, tag, command, 0) != 0)
		fatal("KnownHostsCommand failed");

 out:
	if (f != NULL)
		fclose(f);
	ssh_signal(SIGCHLD, osigchld);
	for (i = 0; i < ac; i++)
		free(av[i]);
	free(av);
	free(tag);
	free(command);
	free(key_fp);
	free(keytext);
}

/*
 * check whether the supplied host key is valid, return -1 if the key
 * is not valid. user_hostfile[0] will not be updated if 'readonly' is true.
 */
#define RDRW	0
#define RDONLY	1
#define ROQUIET	2
static int
check_host_key(char *hostname, const struct ssh_conn_info *cinfo,
    struct sockaddr *hostaddr, u_short port,
    struct sshkey *host_key, int readonly, int clobber_port,
    char **user_hostfiles, u_int num_user_hostfiles,
    char **system_hostfiles, u_int num_system_hostfiles,
    const char *hostfile_command)
{
	HostStatus host_status = -1, ip_status = -1;
	struct sshkey *raw_key = NULL;
	char *ip = NULL, *host = NULL;
	char hostline[1000], *hostp, *fp, *ra;
	char msg[1024];
	const char *type, *fail_reason = NULL;
	const struct hostkey_entry *host_found = NULL, *ip_found = NULL;
	int len, cancelled_forwarding = 0, confirmed;
	int local = sockaddr_is_local(hostaddr);
	int r, want_cert = sshkey_is_cert(host_key), host_ip_differ = 0;
	int hostkey_trusted = 0; /* Known or explicitly accepted by user */
	struct hostkeys *host_hostkeys, *ip_hostkeys;
	u_int i;

	/*
	 * Force accepting of the host key for loopback/localhost. The
	 * problem is that if the home directory is NFS-mounted to multiple
	 * machines, localhost will refer to a different machine in each of
	 * them, and the user will get bogus HOST_CHANGED warnings.  This
	 * essentially disables host authentication for localhost; however,
	 * this is probably not a real problem.
	 */
	if (options.no_host_authentication_for_localhost == 1 && local &&
	    options.host_key_alias == NULL) {
		debug("Forcing accepting of host key for "
		    "loopback/localhost.");
		options.update_hostkeys = 0;
		return 0;
	}

	/*
	 * Don't ever try to write an invalid name to a known hosts file.
	 * Note: do this before get_hostfile_hostname_ipaddr() to catch
	 * '[' or ']' in the name before they are added.
	 */
	if (strcspn(hostname, "@?*#[]|'\'\"\\") != strlen(hostname)) {
		debug_f("invalid hostname \"%s\"; will not record: %s",
		    hostname, fail_reason);
		readonly = RDONLY;
	}

	/*
	 * Prepare the hostname and address strings used for hostkey lookup.
	 * In some cases, these will have a port number appended.
	 */
	get_hostfile_hostname_ipaddr(hostname, hostaddr,
	    clobber_port ? 0 : port, &host, &ip);

	/*
	 * Turn off check_host_ip if the connection is to localhost, via proxy
	 * command or if we don't have a hostname to compare with
	 */
	if (options.check_host_ip && (local ||
	    strcmp(hostname, ip) == 0 || options.proxy_command != NULL))
		options.check_host_ip = 0;

	host_hostkeys = init_hostkeys();
	for (i = 0; i < num_user_hostfiles; i++)
		load_hostkeys(host_hostkeys, host, user_hostfiles[i], 0);
	for (i = 0; i < num_system_hostfiles; i++)
		load_hostkeys(host_hostkeys, host, system_hostfiles[i], 0);
	if (hostfile_command != NULL && !clobber_port) {
		load_hostkeys_command(host_hostkeys, hostfile_command,
		    "HOSTNAME", cinfo, host_key, host);
	}

	ip_hostkeys = NULL;
	if (!want_cert && options.check_host_ip) {
		ip_hostkeys = init_hostkeys();
		for (i = 0; i < num_user_hostfiles; i++)
			load_hostkeys(ip_hostkeys, ip, user_hostfiles[i], 0);
		for (i = 0; i < num_system_hostfiles; i++)
			load_hostkeys(ip_hostkeys, ip, system_hostfiles[i], 0);
		if (hostfile_command != NULL && !clobber_port) {
			load_hostkeys_command(ip_hostkeys, hostfile_command,
			    "ADDRESS", cinfo, host_key, ip);
		}
	}

 retry:
	if (!hostkey_accepted_by_hostkeyalgs(host_key)) {
		error("host key %s not permitted by HostkeyAlgorithms",
		    sshkey_ssh_name(host_key));
		goto fail;
	}

	/* Reload these as they may have changed on cert->key downgrade */
	want_cert = sshkey_is_cert(host_key);
	type = sshkey_type(host_key);

	/*
	 * Check if the host key is present in the user's list of known
	 * hosts or in the systemwide list.
	 */
	host_status = check_key_in_hostkeys(host_hostkeys, host_key,
	    &host_found);

	/*
	 * If there are no hostfiles, or if the hostkey was found via
	 * KnownHostsCommand, then don't try to touch the disk.
	 */
	if (!readonly && (num_user_hostfiles == 0 ||
	    (host_found != NULL && host_found->note != 0)))
		readonly = RDONLY;

	/*
	 * Also perform check for the ip address, skip the check if we are
	 * localhost, looking for a certificate, or the hostname was an ip
	 * address to begin with.
	 */
	if (!want_cert && ip_hostkeys != NULL) {
		ip_status = check_key_in_hostkeys(ip_hostkeys, host_key,
		    &ip_found);
		if (host_status == HOST_CHANGED &&
		    (ip_status != HOST_CHANGED ||
		    (ip_found != NULL &&
		    !sshkey_equal(ip_found->key, host_found->key))))
			host_ip_differ = 1;
	} else
		ip_status = host_status;

	switch (host_status) {
	case HOST_OK:
		/* The host is known and the key matches. */
		debug("Host '%.200s' is known and matches the %s host %s.",
		    host, type, want_cert ? "certificate" : "key");
		debug("Found %s in %s:%lu", want_cert ? "CA key" : "key",
		    host_found->file, host_found->line);
		if (want_cert) {
			if (sshkey_cert_check_host(host_key,
			    options.host_key_alias == NULL ?
			    hostname : options.host_key_alias, 0,
			    options.ca_sign_algorithms, &fail_reason) != 0) {
				error("%s", fail_reason);
				goto fail;
			}
			/*
			 * Do not attempt hostkey update if a certificate was
			 * successfully matched.
			 */
			if (options.update_hostkeys != 0) {
				options.update_hostkeys = 0;
				debug3_f("certificate host key in use; "
				    "disabling UpdateHostkeys");
			}
		}
		/* Turn off UpdateHostkeys if key was in system known_hosts */
		if (options.update_hostkeys != 0 &&
		    (path_in_hostfiles(host_found->file,
		    system_hostfiles, num_system_hostfiles) ||
		    (ip_status == HOST_OK && ip_found != NULL &&
		    path_in_hostfiles(ip_found->file,
		    system_hostfiles, num_system_hostfiles)))) {
			options.update_hostkeys = 0;
			debug3_f("host key found in GlobalKnownHostsFile; "
			    "disabling UpdateHostkeys");
		}
		if (options.update_hostkeys != 0 && host_found->note) {
			options.update_hostkeys = 0;
			debug3_f("host key found via KnownHostsCommand; "
			    "disabling UpdateHostkeys");
		}
		if (options.check_host_ip && ip_status == HOST_NEW) {
			if (readonly || want_cert)
				logit("%s host key for IP address "
				    "'%.128s' not in list of known hosts.",
				    type, ip);
			else if (!add_host_to_hostfile(user_hostfiles[0], ip,
			    host_key, options.hash_known_hosts))
				logit("Failed to add the %s host key for IP "
				    "address '%.128s' to the list of known "
				    "hosts (%.500s).", type, ip,
				    user_hostfiles[0]);
			else
				logit("Warning: Permanently added the %s host "
				    "key for IP address '%.128s' to the list "
				    "of known hosts.", type, ip);
		} else if (options.visual_host_key) {
			fp = sshkey_fingerprint(host_key,
			    options.fingerprint_hash, SSH_FP_DEFAULT);
			ra = sshkey_fingerprint(host_key,
			    options.fingerprint_hash, SSH_FP_RANDOMART);
			if (fp == NULL || ra == NULL)
				fatal_f("sshkey_fingerprint failed");
			logit("Host key fingerprint is %s\n%s", fp, ra);
			free(ra);
			free(fp);
		}
		hostkey_trusted = 1;
		break;
	case HOST_NEW:
		if (options.host_key_alias == NULL && port != 0 &&
		    port != SSH_DEFAULT_PORT && !clobber_port) {
			debug("checking without port identifier");
			if (check_host_key(hostname, cinfo, hostaddr, 0,
			    host_key, ROQUIET, 1,
			    user_hostfiles, num_user_hostfiles,
			    system_hostfiles, num_system_hostfiles,
			    hostfile_command) == 0) {
				debug("found matching key w/out port");
				break;
			}
		}
		if (readonly || want_cert)
			goto fail;
		/* The host is new. */
		if (options.strict_host_key_checking ==
		    SSH_STRICT_HOSTKEY_YES) {
			/*
			 * User has requested strict host key checking.  We
			 * will not add the host key automatically.  The only
			 * alternative left is to abort.
			 */
			error("No %s host key is known for %.200s and you "
			    "have requested strict checking.", type, host);
			goto fail;
		} else if (options.strict_host_key_checking ==
		    SSH_STRICT_HOSTKEY_ASK) {
			char *msg1 = NULL, *msg2 = NULL;

			xasprintf(&msg1, "The authenticity of host "
			    "'%.200s (%s)' can't be established", host, ip);

			if (show_other_keys(host_hostkeys, host_key)) {
				xextendf(&msg1, "\n", "but keys of different "
				    "type are already known for this host.");
			} else
				xextendf(&msg1, "", ".");

			fp = sshkey_fingerprint(host_key,
			    options.fingerprint_hash, SSH_FP_DEFAULT);
			ra = sshkey_fingerprint(host_key,
			    options.fingerprint_hash, SSH_FP_RANDOMART);
			if (fp == NULL || ra == NULL)
				fatal_f("sshkey_fingerprint failed");
			xextendf(&msg1, "\n", "%s key fingerprint is %s.",
			    type, fp);
			if (options.visual_host_key)
				xextendf(&msg1, "\n", "%s", ra);
			if (options.verify_host_key_dns) {
				xextendf(&msg1, "\n",
				    "%s host key fingerprint found in DNS.",
				    matching_host_key_dns ?
				    "Matching" : "No matching");
			}
			/* msg2 informs for other names matching this key */
			if ((msg2 = other_hostkeys_message(host, ip, host_key,
			    user_hostfiles, num_user_hostfiles,
			    system_hostfiles, num_system_hostfiles)) != NULL)
				xextendf(&msg1, "\n", "%s", msg2);

			xextendf(&msg1, "\n",
			    "Are you sure you want to continue connecting "
			    "(yes/no/[fingerprint])? ");

			confirmed = confirm(msg1, fp);
			free(ra);
			free(fp);
			free(msg1);
			free(msg2);
			if (!confirmed)
				goto fail;
			hostkey_trusted = 1; /* user explicitly confirmed */
		}
		/*
		 * If in "new" or "off" strict mode, add the key automatically
		 * to the local known_hosts file.
		 */
		if (options.check_host_ip && ip_status == HOST_NEW) {
			snprintf(hostline, sizeof(hostline), "%s,%s", host, ip);
			hostp = hostline;
			if (options.hash_known_hosts) {
				/* Add hash of host and IP separately */
				r = add_host_to_hostfile(user_hostfiles[0],
				    host, host_key, options.hash_known_hosts) &&
				    add_host_to_hostfile(user_hostfiles[0], ip,
				    host_key, options.hash_known_hosts);
			} else {
				/* Add unhashed "host,ip" */
				r = add_host_to_hostfile(user_hostfiles[0],
				    hostline, host_key,
				    options.hash_known_hosts);
			}
		} else {
			r = add_host_to_hostfile(user_hostfiles[0], host,
			    host_key, options.hash_known_hosts);
			hostp = host;
		}

		if (!r)
			logit("Failed to add the host to the list of known "
			    "hosts (%.500s).", user_hostfiles[0]);
		else
			logit("Warning: Permanently added '%.200s' (%s) to the "
			    "list of known hosts.", hostp, type);
		break;
	case HOST_REVOKED:
		error("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
		error("@       WARNING: REVOKED HOST KEY DETECTED!               @");
		error("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
		error("The %s host key for %s is marked as revoked.", type, host);
		error("This could mean that a stolen key is being used to");
		error("impersonate this host.");

		/*
		 * If strict host key checking is in use, the user will have
		 * to edit the key manually and we can only abort.
		 */
		if (options.strict_host_key_checking !=
		    SSH_STRICT_HOSTKEY_OFF) {
			error("%s host key for %.200s was revoked and you have "
			    "requested strict checking.", type, host);
			goto fail;
		}
		goto continue_unsafe;

	case HOST_CHANGED:
		if (want_cert) {
			/*
			 * This is only a debug() since it is valid to have
			 * CAs with wildcard DNS matches that don't match
			 * all hosts that one might visit.
			 */
			debug("Host certificate authority does not "
			    "match %s in %s:%lu", CA_MARKER,
			    host_found->file, host_found->line);
			goto fail;
		}
		if (readonly == ROQUIET)
			goto fail;
		if (options.check_host_ip && host_ip_differ) {
			char *key_msg;
			if (ip_status == HOST_NEW)
				key_msg = "is unknown";
			else if (ip_status == HOST_OK)
				key_msg = "is unchanged";
			else
				key_msg = "has a different value";
			error("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
			error("@       WARNING: POSSIBLE DNS SPOOFING DETECTED!          @");
			error("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
			error("The %s host key for %s has changed,", type, host);
			error("and the key for the corresponding IP address %s", ip);
			error("%s. This could either mean that", key_msg);
			error("DNS SPOOFING is happening or the IP address for the host");
			error("and its host key have changed at the same time.");
			if (ip_status != HOST_NEW)
				error("Offending key for IP in %s:%lu",
				    ip_found->file, ip_found->line);
		}
		/* The host key has changed. */
		warn_changed_key(host_key);
		if (num_user_hostfiles > 0 || num_system_hostfiles > 0) {
			error("Add correct host key in %.100s to get rid "
			    "of this message.", num_user_hostfiles > 0 ?
			    user_hostfiles[0] : system_hostfiles[0]);
		}
		error("Offending %s key in %s:%lu",
		    sshkey_type(host_found->key),
		    host_found->file, host_found->line);

		/*
		 * If strict host key checking is in use, the user will have
		 * to edit the key manually and we can only abort.
		 */
		if (options.strict_host_key_checking !=
		    SSH_STRICT_HOSTKEY_OFF) {
			error("Host key for %.200s has changed and you have "
			    "requested strict checking.", host);
			goto fail;
		}

 continue_unsafe:
		/*
		 * If strict host key checking has not been requested, allow
		 * the connection but without MITM-able authentication or
		 * forwarding.
		 */
		if (options.password_authentication) {
			error("Password authentication is disabled to avoid "
			    "man-in-the-middle attacks.");
			options.password_authentication = 0;
			cancelled_forwarding = 1;
		}
		if (options.kbd_interactive_authentication) {
			error("Keyboard-interactive authentication is disabled"
			    " to avoid man-in-the-middle attacks.");
			options.kbd_interactive_authentication = 0;
			cancelled_forwarding = 1;
		}
		if (options.forward_agent) {
			error("Agent forwarding is disabled to avoid "
			    "man-in-the-middle attacks.");
			options.forward_agent = 0;
			cancelled_forwarding = 1;
		}
		if (options.forward_x11) {
			error("X11 forwarding is disabled to avoid "
			    "man-in-the-middle attacks.");
			options.forward_x11 = 0;
			cancelled_forwarding = 1;
		}
		if (options.num_local_forwards > 0 ||
		    options.num_remote_forwards > 0) {
			error("Port forwarding is disabled to avoid "
			    "man-in-the-middle attacks.");
			options.num_local_forwards =
			    options.num_remote_forwards = 0;
			cancelled_forwarding = 1;
		}
		if (options.tun_open != SSH_TUNMODE_NO) {
			error("Tunnel forwarding is disabled to avoid "
			    "man-in-the-middle attacks.");
			options.tun_open = SSH_TUNMODE_NO;
			cancelled_forwarding = 1;
		}
		if (options.update_hostkeys != 0) {
			error("UpdateHostkeys is disabled because the host "
			    "key is not trusted.");
			options.update_hostkeys = 0;
		}
		if (options.exit_on_forward_failure && cancelled_forwarding)
			fatal("Error: forwarding disabled due to host key "
			    "check failure");

		/*
		 * XXX Should permit the user to change to use the new id.
		 * This could be done by converting the host key to an
		 * identifying sentence, tell that the host identifies itself
		 * by that sentence, and ask the user if they wish to
		 * accept the authentication.
		 */
		break;
	case HOST_FOUND:
		fatal("internal error");
		break;
	}

	if (options.check_host_ip && host_status != HOST_CHANGED &&
	    ip_status == HOST_CHANGED) {
		snprintf(msg, sizeof(msg),
		    "Warning: the %s host key for '%.200s' "
		    "differs from the key for the IP address '%.128s'"
		    "\nOffending key for IP in %s:%lu",
		    type, host, ip, ip_found->file, ip_found->line);
		if (host_status == HOST_OK) {
			len = strlen(msg);
			snprintf(msg + len, sizeof(msg) - len,
			    "\nMatching host key in %s:%lu",
			    host_found->file, host_found->line);
		}
		if (options.strict_host_key_checking ==
		    SSH_STRICT_HOSTKEY_ASK) {
			strlcat(msg, "\nAre you sure you want "
			    "to continue connecting (yes/no)? ", sizeof(msg));
			if (!confirm(msg, NULL))
				goto fail;
		} else if (options.strict_host_key_checking !=
		    SSH_STRICT_HOSTKEY_OFF) {
			logit("%s", msg);
			error("Exiting, you have requested strict checking.");
			goto fail;
		} else {
			logit("%s", msg);
		}
	}

	if (!hostkey_trusted && options.update_hostkeys) {
		debug_f("hostkey not known or explicitly trusted: "
		    "disabling UpdateHostkeys");
		options.update_hostkeys = 0;
	}

	free(ip);
	free(host);
	if (host_hostkeys != NULL)
		free_hostkeys(host_hostkeys);
	if (ip_hostkeys != NULL)
		free_hostkeys(ip_hostkeys);
	return 0;

fail:
	if (want_cert && host_status != HOST_REVOKED) {
		/*
		 * No matching certificate. Downgrade cert to raw key and
		 * search normally.
		 */
		debug("No matching CA found. Retry with plain key");
		if ((r = sshkey_from_private(host_key, &raw_key)) != 0)
			fatal_fr(r, "decode key");
		if ((r = sshkey_drop_cert(raw_key)) != 0)
			fatal_r(r, "Couldn't drop certificate");
		host_key = raw_key;
		goto retry;
	}
	sshkey_free(raw_key);
	free(ip);
	free(host);
	if (host_hostkeys != NULL)
		free_hostkeys(host_hostkeys);
	if (ip_hostkeys != NULL)
		free_hostkeys(ip_hostkeys);
	return -1;
}

/* returns 0 if key verifies or -1 if key does NOT verify */
int
verify_host_key(char *host, struct sockaddr *hostaddr, struct sshkey *host_key,
    const struct ssh_conn_info *cinfo)
{
	u_int i;
	int r = -1, flags = 0;
	char valid[64], *fp = NULL, *cafp = NULL;
	struct sshkey *plain = NULL;

	if ((fp = sshkey_fingerprint(host_key,
	    options.fingerprint_hash, SSH_FP_DEFAULT)) == NULL) {
		error_fr(r, "fingerprint host key");
		r = -1;
		goto out;
	}

	if (sshkey_is_cert(host_key)) {
		if ((cafp = sshkey_fingerprint(host_key->cert->signature_key,
		    options.fingerprint_hash, SSH_FP_DEFAULT)) == NULL) {
			error_fr(r, "fingerprint CA key");
			r = -1;
			goto out;
		}
		sshkey_format_cert_validity(host_key->cert,
		    valid, sizeof(valid));
		debug("Server host certificate: %s %s, serial %llu "
		    "ID \"%s\" CA %s %s valid %s",
		    sshkey_ssh_name(host_key), fp,
		    (unsigned long long)host_key->cert->serial,
		    host_key->cert->key_id,
		    sshkey_ssh_name(host_key->cert->signature_key), cafp,
		    valid);
		for (i = 0; i < host_key->cert->nprincipals; i++) {
			debug2("Server host certificate hostname: %s",
			    host_key->cert->principals[i]);
		}
	} else {
		debug("Server host key: %s %s", sshkey_ssh_name(host_key), fp);
	}

	if (sshkey_equal(previous_host_key, host_key)) {
		debug2_f("server host key %s %s matches cached key",
		    sshkey_type(host_key), fp);
		r = 0;
		goto out;
	}

	/* Check in RevokedHostKeys file if specified */
	if (options.revoked_host_keys != NULL) {
		r = sshkey_check_revoked(host_key, options.revoked_host_keys);
		switch (r) {
		case 0:
			break; /* not revoked */
		case SSH_ERR_KEY_REVOKED:
			error("Host key %s %s revoked by file %s",
			    sshkey_type(host_key), fp,
			    options.revoked_host_keys);
			r = -1;
			goto out;
		default:
			error_r(r, "Error checking host key %s %s in "
			    "revoked keys file %s", sshkey_type(host_key),
			    fp, options.revoked_host_keys);
			r = -1;
			goto out;
		}
	}

	if (options.verify_host_key_dns) {
		/*
		 * XXX certs are not yet supported for DNS, so downgrade
		 * them and try the plain key.
		 */
		if ((r = sshkey_from_private(host_key, &plain)) != 0)
			goto out;
		if (sshkey_is_cert(plain))
			sshkey_drop_cert(plain);
		if (verify_host_key_dns(host, hostaddr, plain, &flags) == 0) {
			if (flags & DNS_VERIFY_FOUND) {
				if (options.verify_host_key_dns == 1 &&
				    flags & DNS_VERIFY_MATCH &&
				    flags & DNS_VERIFY_SECURE) {
					r = 0;
					goto out;
				}
				if (flags & DNS_VERIFY_MATCH) {
					matching_host_key_dns = 1;
				} else {
					warn_changed_key(plain);
					error("Update the SSHFP RR in DNS "
					    "with the new host key to get rid "
					    "of this message.");
				}
			}
		}
	}
	r = check_host_key(host, cinfo, hostaddr, options.port, host_key,
	    RDRW, 0, options.user_hostfiles, options.num_user_hostfiles,
	    options.system_hostfiles, options.num_system_hostfiles,
	    options.known_hosts_command);

out:
	sshkey_free(plain);
	free(fp);
	free(cafp);
	if (r == 0 && host_key != NULL) {
		sshkey_free(previous_host_key);
		r = sshkey_from_private(host_key, &previous_host_key);
	}

	return r;
}

/*
 * Starts a dialog with the server, and authenticates the current user on the
 * server.  This does not need any extra privileges.  The basic connection
 * to the server must already have been established before this is called.
 * If login fails, this function prints an error and never returns.
 * This function does not require super-user privileges.
 */
void
ssh_login(struct ssh *ssh, Sensitive *sensitive, const char *orighost,
    struct sockaddr *hostaddr, u_short port, struct passwd *pw, int timeout_ms,
    const struct ssh_conn_info *cinfo)
{
	char *host;
	char *server_user, *local_user;
	int r;

	local_user = xstrdup(pw->pw_name);
	server_user = options.user ? options.user : local_user;

	/* Convert the user-supplied hostname into all lowercase. */
	host = xstrdup(orighost);
	lowercase(host);

	/* Exchange protocol version identification strings with the server. */
	if ((r = kex_exchange_identification(ssh, timeout_ms, NULL)) != 0)
		sshpkt_fatal(ssh, r, "banner exchange");

	/* Put the connection into non-blocking mode. */
	ssh_packet_set_nonblocking(ssh);

	/* key exchange */
	/* authenticate user */
	debug("Authenticating to %s:%d as '%s'", host, port, server_user);
	ssh_kex2(ssh, host, hostaddr, port, cinfo);
	ssh_userauth2(ssh, local_user, server_user, host, sensitive);
	free(local_user);
	free(host);
}

/* print all known host keys for a given host, but skip keys of given type */
static int
show_other_keys(struct hostkeys *hostkeys, struct sshkey *key)
{
	int type[] = {
		KEY_RSA,
#ifdef WITH_DSA
		KEY_DSA,
#endif
		KEY_ECDSA,
		KEY_ED25519,
		KEY_XMSS,
		-1
	};
	int i, ret = 0;
	char *fp, *ra;
	const struct hostkey_entry *found;

	for (i = 0; type[i] != -1; i++) {
		if (type[i] == key->type)
			continue;
		if (!lookup_key_in_hostkeys_by_type(hostkeys, type[i],
		    -1, &found))
			continue;
		fp = sshkey_fingerprint(found->key,
		    options.fingerprint_hash, SSH_FP_DEFAULT);
		ra = sshkey_fingerprint(found->key,
		    options.fingerprint_hash, SSH_FP_RANDOMART);
		if (fp == NULL || ra == NULL)
			fatal_f("sshkey_fingerprint fail");
		logit("WARNING: %s key found for host %s\n"
		    "in %s:%lu\n"
		    "%s key fingerprint %s.",
		    sshkey_type(found->key),
		    found->host, found->file, found->line,
		    sshkey_type(found->key), fp);
		if (options.visual_host_key)
			logit("%s", ra);
		free(ra);
		free(fp);
		ret = 1;
	}
	return ret;
}

static void
warn_changed_key(struct sshkey *host_key)
{
	char *fp;

	fp = sshkey_fingerprint(host_key, options.fingerprint_hash,
	    SSH_FP_DEFAULT);
	if (fp == NULL)
		fatal_f("sshkey_fingerprint fail");

	error("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
	error("@    WARNING: REMOTE HOST IDENTIFICATION HAS CHANGED!     @");
	error("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
	error("IT IS POSSIBLE THAT SOMEONE IS DOING SOMETHING NASTY!");
	error("Someone could be eavesdropping on you right now (man-in-the-middle attack)!");
	error("It is also possible that a host key has just been changed.");
	error("The fingerprint for the %s key sent by the remote host is\n%s.",
	    sshkey_type(host_key), fp);
	error("Please contact your system administrator.");

	free(fp);
}

/*
 * Execute a local command
 */
int
ssh_local_cmd(const char *args)
{
	char *shell;
	pid_t pid;
	int status;
	void (*osighand)(int);

	if (!options.permit_local_command ||
	    args == NULL || !*args)
		return (1);

	if ((shell = getenv("SHELL")) == NULL || *shell == '\0')
		shell = _PATH_BSHELL;

	osighand = ssh_signal(SIGCHLD, SIG_DFL);
	pid = fork();
	if (pid == 0) {
		ssh_signal(SIGPIPE, SIG_DFL);
		debug3("Executing %s -c \"%s\"", shell, args);
		execl(shell, shell, "-c", args, (char *)NULL);
		error("Couldn't execute %s -c \"%s\": %s",
		    shell, args, strerror(errno));
		_exit(1);
	} else if (pid == -1)
		fatal("fork failed: %.100s", strerror(errno));
	while (waitpid(pid, &status, 0) == -1)
		if (errno != EINTR)
			fatal("Couldn't wait for child: %s", strerror(errno));
	ssh_signal(SIGCHLD, osighand);

	if (!WIFEXITED(status))
		return (1);

	return (WEXITSTATUS(status));
}

void
maybe_add_key_to_agent(const char *authfile, struct sshkey *private,
    const char *comment, const char *passphrase)
{
	int auth_sock = -1, r;
	const char *skprovider = NULL;

	if (options.add_keys_to_agent == 0)
		return;

	if ((r = ssh_get_authentication_socket(&auth_sock)) != 0) {
		debug3("no authentication agent, not adding key");
		return;
	}

	if (options.add_keys_to_agent == 2 &&
	    !ask_permission("Add key %s (%s) to agent?", authfile, comment)) {
		debug3("user denied adding this key");
		close(auth_sock);
		return;
	}
	if (sshkey_is_sk(private))
		skprovider = options.sk_provider;
	if ((r = ssh_add_identity_constrained(auth_sock, private,
	    comment == NULL ? authfile : comment,
	    options.add_keys_to_agent_lifespan,
	    (options.add_keys_to_agent == 3), 0, skprovider, NULL, 0)) == 0)
		debug("identity added to agent: %s", authfile);
	else
		debug("could not add identity to agent: %s (%d)", authfile, r);
	close(auth_sock);
}
