/* $OpenBSD: ssh.c,v 1.569 2021/09/20 04:02:13 dtucker Exp $ */
/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * Ssh client program.  This program can be used to log into a remote machine.
 * The software supports strong authentication, encryption, and forwarding
 * of X11, TCP/IP, and authentication connections.
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 *
 * Copyright (c) 1999 Niels Provos.  All rights reserved.
 * Copyright (c) 2000, 2001, 2002, 2003 Markus Friedl.  All rights reserved.
 *
 * Modified to work with SSLeay by Niels Provos <provos@citi.umich.edu>
 * in Canada (German citizen).
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
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#include <sys/resource.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#ifdef HAVE_PATHS_H
#include <paths.h>
#endif
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <limits.h>
#include <locale.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#ifdef WITH_OPENSSL
#include <openssl/evp.h>
#include <openssl/err.h>
#endif
#include "openbsd-compat/openssl-compat.h"
#include "openbsd-compat/sys-queue.h"

#include "xmalloc.h"
#include "ssh.h"
#include "ssh2.h"
#include "canohost.h"
#include "compat.h"
#include "cipher.h"
#include "packet.h"
#include "sshbuf.h"
#include "channels.h"
#include "sshkey.h"
#include "authfd.h"
#include "authfile.h"
#include "pathnames.h"
#include "dispatch.h"
#include "clientloop.h"
#include "log.h"
#include "misc.h"
#include "readconf.h"
#include "sshconnect.h"
#include "kex.h"
#include "mac.h"
#include "sshpty.h"
#include "match.h"
#include "msg.h"
#include "version.h"
#include "ssherr.h"
#include "myproposal.h"
#include "utf8.h"

#ifdef ENABLE_PKCS11
#include "ssh-pkcs11.h"
#endif

extern char *__progname;

/* Saves a copy of argv for setproctitle emulation */
#ifndef HAVE_SETPROCTITLE
static char **saved_av;
#endif

/* Flag indicating whether debug mode is on.  May be set on the command line. */
int debug_flag = 0;

/* Flag indicating whether a tty should be requested */
int tty_flag = 0;

/*
 * Flag indicating that the current process should be backgrounded and
 * a new mux-client launched in the foreground for ControlPersist.
 */
int need_controlpersist_detach = 0;

/* Copies of flags for ControlPersist foreground mux-client */
int ostdin_null_flag, osession_type, otty_flag, orequest_tty;

/*
 * General data structure for command line options and options configurable
 * in configuration files.  See readconf.h.
 */
Options options;

/* optional user configfile */
char *config = NULL;

/*
 * Name of the host we are connecting to.  This is the name given on the
 * command line, or the Hostname specified for the user-supplied name in a
 * configuration file.
 */
char *host;

/*
 * A config can specify a path to forward, overriding SSH_AUTH_SOCK. If this is
 * not NULL, forward the socket at this path instead.
 */
char *forward_agent_sock_path = NULL;

/* socket address the host resolves to */
struct sockaddr_storage hostaddr;

/* Private host keys. */
Sensitive sensitive_data;

/* command to be executed */
struct sshbuf *command;

/* # of replies received for global requests */
static int forward_confirms_pending = -1;

/* mux.c */
extern int muxserver_sock;
extern u_int muxclient_command;

/* Prints a help message to the user.  This function never returns. */

static void
usage(void)
{
	fprintf(stderr,
"usage: ssh [-46AaCfGgKkMNnqsTtVvXxYy] [-B bind_interface]\n"
"           [-b bind_address] [-c cipher_spec] [-D [bind_address:]port]\n"
"           [-E log_file] [-e escape_char] [-F configfile] [-I pkcs11]\n"
"           [-i identity_file] [-J [user@]host[:port]] [-L address]\n"
"           [-l login_name] [-m mac_spec] [-O ctl_cmd] [-o option] [-p port]\n"
"           [-Q query_option] [-R address] [-S ctl_path] [-W host:port]\n"
"           [-w local_tun[:remote_tun]] destination [command [argument ...]]\n"
	);
	exit(255);
}

static int ssh_session2(struct ssh *, const struct ssh_conn_info *);
static void load_public_identity_files(const struct ssh_conn_info *);
static void main_sigchld_handler(int);

/* ~/ expand a list of paths. NB. assumes path[n] is heap-allocated. */
static void
tilde_expand_paths(char **paths, u_int num_paths)
{
	u_int i;
	char *cp;

	for (i = 0; i < num_paths; i++) {
		cp = tilde_expand_filename(paths[i], getuid());
		free(paths[i]);
		paths[i] = cp;
	}
}

/*
 * Expands the set of percent_expand options used by the majority of keywords
 * in the client that support percent expansion.
 * Caller must free returned string.
 */
static char *
default_client_percent_expand(const char *str,
    const struct ssh_conn_info *cinfo)
{
	return percent_expand(str,
	    DEFAULT_CLIENT_PERCENT_EXPAND_ARGS(cinfo),
	    (char *)NULL);
}

/*
 * Expands the set of percent_expand options used by the majority of keywords
 * AND perform environment variable substitution.
 * Caller must free returned string.
 */
static char *
default_client_percent_dollar_expand(const char *str,
    const struct ssh_conn_info *cinfo)
{
	char *ret;

	ret = percent_dollar_expand(str,
	    DEFAULT_CLIENT_PERCENT_EXPAND_ARGS(cinfo),
	    (char *)NULL);
	if (ret == NULL)
		fatal("invalid environment variable expansion");
	return ret;
}

/*
 * Attempt to resolve a host name / port to a set of addresses and
 * optionally return any CNAMEs encountered along the way.
 * Returns NULL on failure.
 * NB. this function must operate with a options having undefined members.
 */
static struct addrinfo *
resolve_host(const char *name, int port, int logerr, char *cname, size_t clen)
{
	char strport[NI_MAXSERV];
	struct addrinfo hints, *res;
	int gaierr;
	LogLevel loglevel = SYSLOG_LEVEL_DEBUG1;

	if (port <= 0)
		port = default_ssh_port();
	if (cname != NULL)
		*cname = '\0';
	debug3_f("lookup %s:%d", name, port);

	snprintf(strport, sizeof strport, "%d", port);
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = options.address_family == -1 ?
	    AF_UNSPEC : options.address_family;
	hints.ai_socktype = SOCK_STREAM;
	if (cname != NULL)
		hints.ai_flags = AI_CANONNAME;
	if ((gaierr = getaddrinfo(name, strport, &hints, &res)) != 0) {
		if (logerr || (gaierr != EAI_NONAME && gaierr != EAI_NODATA))
			loglevel = SYSLOG_LEVEL_ERROR;
		do_log2(loglevel, "%s: Could not resolve hostname %.100s: %s",
		    __progname, name, ssh_gai_strerror(gaierr));
		return NULL;
	}
	if (cname != NULL && res->ai_canonname != NULL) {
		if (strlcpy(cname, res->ai_canonname, clen) >= clen) {
			error_f("host \"%s\" cname \"%s\" too long (max %lu)",
			    name,  res->ai_canonname, (u_long)clen);
			if (clen > 0)
				*cname = '\0';
		}
	}
	return res;
}

/* Returns non-zero if name can only be an address and not a hostname */
static int
is_addr_fast(const char *name)
{
	return (strchr(name, '%') != NULL || strchr(name, ':') != NULL ||
	    strspn(name, "0123456789.") == strlen(name));
}

/* Returns non-zero if name represents a valid, single address */
static int
is_addr(const char *name)
{
	char strport[NI_MAXSERV];
	struct addrinfo hints, *res;

	if (is_addr_fast(name))
		return 1;

	snprintf(strport, sizeof strport, "%u", default_ssh_port());
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = options.address_family == -1 ?
	    AF_UNSPEC : options.address_family;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_NUMERICHOST|AI_NUMERICSERV;
	if (getaddrinfo(name, strport, &hints, &res) != 0)
		return 0;
	if (res == NULL || res->ai_next != NULL) {
		freeaddrinfo(res);
		return 0;
	}
	freeaddrinfo(res);
	return 1;
}

/*
 * Attempt to resolve a numeric host address / port to a single address.
 * Returns a canonical address string.
 * Returns NULL on failure.
 * NB. this function must operate with a options having undefined members.
 */
static struct addrinfo *
resolve_addr(const char *name, int port, char *caddr, size_t clen)
{
	char addr[NI_MAXHOST], strport[NI_MAXSERV];
	struct addrinfo hints, *res;
	int gaierr;

	if (port <= 0)
		port = default_ssh_port();
	snprintf(strport, sizeof strport, "%u", port);
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = options.address_family == -1 ?
	    AF_UNSPEC : options.address_family;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_NUMERICHOST|AI_NUMERICSERV;
	if ((gaierr = getaddrinfo(name, strport, &hints, &res)) != 0) {
		debug2_f("could not resolve name %.100s as address: %s",
		    name, ssh_gai_strerror(gaierr));
		return NULL;
	}
	if (res == NULL) {
		debug_f("getaddrinfo %.100s returned no addresses", name);
		return NULL;
	}
	if (res->ai_next != NULL) {
		debug_f("getaddrinfo %.100s returned multiple addresses", name);
		goto fail;
	}
	if ((gaierr = getnameinfo(res->ai_addr, res->ai_addrlen,
	    addr, sizeof(addr), NULL, 0, NI_NUMERICHOST)) != 0) {
		debug_f("Could not format address for name %.100s: %s",
		    name, ssh_gai_strerror(gaierr));
		goto fail;
	}
	if (strlcpy(caddr, addr, clen) >= clen) {
		error_f("host \"%s\" addr \"%s\" too long (max %lu)",
		    name,  addr, (u_long)clen);
		if (clen > 0)
			*caddr = '\0';
 fail:
		freeaddrinfo(res);
		return NULL;
	}
	return res;
}

/*
 * Check whether the cname is a permitted replacement for the hostname
 * and perform the replacement if it is.
 * NB. this function must operate with a options having undefined members.
 */
static int
check_follow_cname(int direct, char **namep, const char *cname)
{
	int i;
	struct allowed_cname *rule;

	if (*cname == '\0' || !config_has_permitted_cnames(&options) ||
	    strcmp(*namep, cname) == 0)
		return 0;
	if (options.canonicalize_hostname == SSH_CANONICALISE_NO)
		return 0;
	/*
	 * Don't attempt to canonicalize names that will be interpreted by
	 * a proxy or jump host unless the user specifically requests so.
	 */
	if (!direct &&
	    options.canonicalize_hostname != SSH_CANONICALISE_ALWAYS)
		return 0;
	debug3_f("check \"%s\" CNAME \"%s\"", *namep, cname);
	for (i = 0; i < options.num_permitted_cnames; i++) {
		rule = options.permitted_cnames + i;
		if (match_pattern_list(*namep, rule->source_list, 1) != 1 ||
		    match_pattern_list(cname, rule->target_list, 1) != 1)
			continue;
		verbose("Canonicalized DNS aliased hostname "
		    "\"%s\" => \"%s\"", *namep, cname);
		free(*namep);
		*namep = xstrdup(cname);
		return 1;
	}
	return 0;
}

/*
 * Attempt to resolve the supplied hostname after applying the user's
 * canonicalization rules. Returns the address list for the host or NULL
 * if no name was found after canonicalization.
 * NB. this function must operate with a options having undefined members.
 */
static struct addrinfo *
resolve_canonicalize(char **hostp, int port)
{
	int i, direct, ndots;
	char *cp, *fullhost, newname[NI_MAXHOST];
	struct addrinfo *addrs;

	/*
	 * Attempt to canonicalise addresses, regardless of
	 * whether hostname canonicalisation was requested
	 */
	if ((addrs = resolve_addr(*hostp, port,
	    newname, sizeof(newname))) != NULL) {
		debug2_f("hostname %.100s is address", *hostp);
		if (strcasecmp(*hostp, newname) != 0) {
			debug2_f("canonicalised address \"%s\" => \"%s\"",
			    *hostp, newname);
			free(*hostp);
			*hostp = xstrdup(newname);
		}
		return addrs;
	}

	/*
	 * If this looks like an address but didn't parse as one, it might
	 * be an address with an invalid interface scope. Skip further
	 * attempts at canonicalisation.
	 */
	if (is_addr_fast(*hostp)) {
		debug_f("hostname %.100s is an unrecognised address", *hostp);
		return NULL;
	}

	if (options.canonicalize_hostname == SSH_CANONICALISE_NO)
		return NULL;

	/*
	 * Don't attempt to canonicalize names that will be interpreted by
	 * a proxy unless the user specifically requests so.
	 */
	direct = option_clear_or_none(options.proxy_command) &&
	    options.jump_host == NULL;
	if (!direct &&
	    options.canonicalize_hostname != SSH_CANONICALISE_ALWAYS)
		return NULL;

	/* If domain name is anchored, then resolve it now */
	if ((*hostp)[strlen(*hostp) - 1] == '.') {
		debug3_f("name is fully qualified");
		fullhost = xstrdup(*hostp);
		if ((addrs = resolve_host(fullhost, port, 0,
		    newname, sizeof(newname))) != NULL)
			goto found;
		free(fullhost);
		goto notfound;
	}

	/* Don't apply canonicalization to sufficiently-qualified hostnames */
	ndots = 0;
	for (cp = *hostp; *cp != '\0'; cp++) {
		if (*cp == '.')
			ndots++;
	}
	if (ndots > options.canonicalize_max_dots) {
		debug3_f("not canonicalizing hostname \"%s\" (max dots %d)",
		    *hostp, options.canonicalize_max_dots);
		return NULL;
	}
	/* Attempt each supplied suffix */
	for (i = 0; i < options.num_canonical_domains; i++) {
		if (strcasecmp(options.canonical_domains[i], "none") == 0)
			break;
		xasprintf(&fullhost, "%s.%s.", *hostp,
		    options.canonical_domains[i]);
		debug3_f("attempting \"%s\" => \"%s\"", *hostp, fullhost);
		if ((addrs = resolve_host(fullhost, port, 0,
		    newname, sizeof(newname))) == NULL) {
			free(fullhost);
			continue;
		}
 found:
		/* Remove trailing '.' */
		fullhost[strlen(fullhost) - 1] = '\0';
		/* Follow CNAME if requested */
		if (!check_follow_cname(direct, &fullhost, newname)) {
			debug("Canonicalized hostname \"%s\" => \"%s\"",
			    *hostp, fullhost);
		}
		free(*hostp);
		*hostp = fullhost;
		return addrs;
	}
 notfound:
	if (!options.canonicalize_fallback_local)
		fatal("%s: Could not resolve host \"%s\"", __progname, *hostp);
	debug2_f("host %s not found in any suffix", *hostp);
	return NULL;
}

/*
 * Check the result of hostkey loading, ignoring some errors and
 * fatal()ing for others.
 */
static void
check_load(int r, const char *path, const char *message)
{
	switch (r) {
	case 0:
		break;
	case SSH_ERR_INTERNAL_ERROR:
	case SSH_ERR_ALLOC_FAIL:
		fatal_r(r, "load %s \"%s\"", message, path);
	case SSH_ERR_SYSTEM_ERROR:
		/* Ignore missing files */
		if (errno == ENOENT)
			break;
		/* FALLTHROUGH */
	default:
		error_r(r, "load %s \"%s\"", message, path);
		break;
	}
}

/*
 * Read per-user configuration file.  Ignore the system wide config
 * file if the user specifies a config file on the command line.
 */
static void
process_config_files(const char *host_name, struct passwd *pw, int final_pass,
    int *want_final_pass)
{
	char buf[PATH_MAX];
	int r;

	if (config != NULL) {
		if (strcasecmp(config, "none") != 0 &&
		    !read_config_file(config, pw, host, host_name, &options,
		    SSHCONF_USERCONF | (final_pass ? SSHCONF_FINAL : 0),
		    want_final_pass))
			fatal("Can't open user config file %.100s: "
			    "%.100s", config, strerror(errno));
	} else {
		r = snprintf(buf, sizeof buf, "%s/%s", pw->pw_dir,
		    _PATH_SSH_USER_CONFFILE);
		if (r > 0 && (size_t)r < sizeof(buf))
			(void)read_config_file(buf, pw, host, host_name,
			    &options, SSHCONF_CHECKPERM | SSHCONF_USERCONF |
			    (final_pass ? SSHCONF_FINAL : 0), want_final_pass);

		/* Read systemwide configuration file after user config. */
		(void)read_config_file(_PATH_HOST_CONFIG_FILE, pw,
		    host, host_name, &options,
		    final_pass ? SSHCONF_FINAL : 0, want_final_pass);
	}
}

/* Rewrite the port number in an addrinfo list of addresses */
static void
set_addrinfo_port(struct addrinfo *addrs, int port)
{
	struct addrinfo *addr;

	for (addr = addrs; addr != NULL; addr = addr->ai_next) {
		switch (addr->ai_family) {
		case AF_INET:
			((struct sockaddr_in *)addr->ai_addr)->
			    sin_port = htons(port);
			break;
		case AF_INET6:
			((struct sockaddr_in6 *)addr->ai_addr)->
			    sin6_port = htons(port);
			break;
		}
	}
}

static void
ssh_conn_info_free(struct ssh_conn_info *cinfo)
{
	if (cinfo == NULL)
		return;
	free(cinfo->conn_hash_hex);
	free(cinfo->shorthost);
	free(cinfo->uidstr);
	free(cinfo->keyalias);
	free(cinfo->thishost);
	free(cinfo->host_arg);
	free(cinfo->portstr);
	free(cinfo->remhost);
	free(cinfo->remuser);
	free(cinfo->homedir);
	free(cinfo->locuser);
	free(cinfo);
}

/*
 * Main program for the ssh client.
 */
int
main(int ac, char **av)
{
	struct ssh *ssh = NULL;
	int i, r, opt, exit_status, use_syslog, direct, timeout_ms;
	int was_addr, config_test = 0, opt_terminated = 0, want_final_pass = 0;
	char *p, *cp, *line, *argv0, *logfile, *host_arg;
	char cname[NI_MAXHOST], thishost[NI_MAXHOST];
	struct stat st;
	struct passwd *pw;
	extern int optind, optreset;
	extern char *optarg;
	struct Forward fwd;
	struct addrinfo *addrs = NULL;
	size_t n, len;
	u_int j;
	struct ssh_conn_info *cinfo = NULL;

	/* Ensure that fds 0, 1 and 2 are open or directed to /dev/null */
	sanitise_stdfd();

	/*
	 * Discard other fds that are hanging around. These can cause problem
	 * with backgrounded ssh processes started by ControlPersist.
	 */
	closefrom(STDERR_FILENO + 1);

	__progname = ssh_get_progname(av[0]);

#ifndef HAVE_SETPROCTITLE
	/* Prepare for later setproctitle emulation */
	/* Save argv so it isn't clobbered by setproctitle() emulation */
	saved_av = xcalloc(ac + 1, sizeof(*saved_av));
	for (i = 0; i < ac; i++)
		saved_av[i] = xstrdup(av[i]);
	saved_av[i] = NULL;
	compat_init_setproctitle(ac, av);
	av = saved_av;
#endif

	seed_rng();

	/* Get user data. */
	pw = getpwuid(getuid());
	if (!pw) {
		logit("No user exists for uid %lu", (u_long)getuid());
		exit(255);
	}
	/* Take a copy of the returned structure. */
	pw = pwcopy(pw);

	/*
	 * Set our umask to something reasonable, as some files are created
	 * with the default umask.  This will make them world-readable but
	 * writable only by the owner, which is ok for all files for which we
	 * don't set the modes explicitly.
	 */
	umask(022);

	msetlocale();

	/*
	 * Initialize option structure to indicate that no values have been
	 * set.
	 */
	initialize_options(&options);

	/*
	 * Prepare main ssh transport/connection structures
	 */
	if ((ssh = ssh_alloc_session_state()) == NULL)
		fatal("Couldn't allocate session state");
	channel_init_channels(ssh);

	/* Parse command-line arguments. */
	host = NULL;
	use_syslog = 0;
	logfile = NULL;
	argv0 = av[0];

 again:
	while ((opt = getopt(ac, av, "1246ab:c:e:fgi:kl:m:no:p:qstvx"
	    "AB:CD:E:F:GI:J:KL:MNO:PQ:R:S:TVw:W:XYy")) != -1) {
		switch (opt) {
		case '1':
			fatal("SSH protocol v.1 is no longer supported");
			break;
		case '2':
			/* Ignored */
			break;
		case '4':
			options.address_family = AF_INET;
			break;
		case '6':
			options.address_family = AF_INET6;
			break;
		case 'n':
			options.stdin_null = 1;
			break;
		case 'f':
			options.fork_after_authentication = 1;
			options.stdin_null = 1;
			break;
		case 'x':
			options.forward_x11 = 0;
			break;
		case 'X':
			options.forward_x11 = 1;
			break;
		case 'y':
			use_syslog = 1;
			break;
		case 'E':
			logfile = optarg;
			break;
		case 'G':
			config_test = 1;
			break;
		case 'Y':
			options.forward_x11 = 1;
			options.forward_x11_trusted = 1;
			break;
		case 'g':
			options.fwd_opts.gateway_ports = 1;
			break;
		case 'O':
			if (options.stdio_forward_host != NULL)
				fatal("Cannot specify multiplexing "
				    "command with -W");
			else if (muxclient_command != 0)
				fatal("Multiplexing command already specified");
			if (strcmp(optarg, "check") == 0)
				muxclient_command = SSHMUX_COMMAND_ALIVE_CHECK;
			else if (strcmp(optarg, "forward") == 0)
				muxclient_command = SSHMUX_COMMAND_FORWARD;
			else if (strcmp(optarg, "exit") == 0)
				muxclient_command = SSHMUX_COMMAND_TERMINATE;
			else if (strcmp(optarg, "stop") == 0)
				muxclient_command = SSHMUX_COMMAND_STOP;
			else if (strcmp(optarg, "cancel") == 0)
				muxclient_command = SSHMUX_COMMAND_CANCEL_FWD;
			else if (strcmp(optarg, "proxy") == 0)
				muxclient_command = SSHMUX_COMMAND_PROXY;
			else
				fatal("Invalid multiplex command.");
			break;
		case 'P':	/* deprecated */
			break;
		case 'Q':
			cp = NULL;
			if (strcmp(optarg, "cipher") == 0 ||
			    strcasecmp(optarg, "Ciphers") == 0)
				cp = cipher_alg_list('\n', 0);
			else if (strcmp(optarg, "cipher-auth") == 0)
				cp = cipher_alg_list('\n', 1);
			else if (strcmp(optarg, "mac") == 0 ||
			    strcasecmp(optarg, "MACs") == 0)
				cp = mac_alg_list('\n');
			else if (strcmp(optarg, "kex") == 0 ||
			    strcasecmp(optarg, "KexAlgorithms") == 0)
				cp = kex_alg_list('\n');
			else if (strcmp(optarg, "key") == 0)
				cp = sshkey_alg_list(0, 0, 0, '\n');
			else if (strcmp(optarg, "key-cert") == 0)
				cp = sshkey_alg_list(1, 0, 0, '\n');
			else if (strcmp(optarg, "key-plain") == 0)
				cp = sshkey_alg_list(0, 1, 0, '\n');
			else if (strcmp(optarg, "key-sig") == 0 ||
			    strcasecmp(optarg, "PubkeyAcceptedKeyTypes") == 0 || /* deprecated name */
			    strcasecmp(optarg, "PubkeyAcceptedAlgorithms") == 0 ||
			    strcasecmp(optarg, "HostKeyAlgorithms") == 0 ||
			    strcasecmp(optarg, "HostbasedKeyTypes") == 0 || /* deprecated name */
			    strcasecmp(optarg, "HostbasedAcceptedKeyTypes") == 0 || /* deprecated name */
			    strcasecmp(optarg, "HostbasedAcceptedAlgorithms") == 0)
				cp = sshkey_alg_list(0, 0, 1, '\n');
			else if (strcmp(optarg, "sig") == 0)
				cp = sshkey_alg_list(0, 1, 1, '\n');
			else if (strcmp(optarg, "protocol-version") == 0)
				cp = xstrdup("2");
			else if (strcmp(optarg, "compression") == 0) {
				cp = xstrdup(compression_alg_list(0));
				len = strlen(cp);
				for (n = 0; n < len; n++)
					if (cp[n] == ',')
						cp[n] = '\n';
			} else if (strcmp(optarg, "help") == 0) {
				cp = xstrdup(
				    "cipher\ncipher-auth\ncompression\nkex\n"
				    "key\nkey-cert\nkey-plain\nkey-sig\nmac\n"
				    "protocol-version\nsig");
			}
			if (cp == NULL)
				fatal("Unsupported query \"%s\"", optarg);
			printf("%s\n", cp);
			free(cp);
			exit(0);
			break;
		case 'a':
			options.forward_agent = 0;
			break;
		case 'A':
			options.forward_agent = 1;
			break;
		case 'k':
			options.gss_deleg_creds = 0;
			break;
		case 'K':
			options.gss_authentication = 1;
			options.gss_deleg_creds = 1;
			break;
		case 'i':
			p = tilde_expand_filename(optarg, getuid());
			if (stat(p, &st) == -1)
				fprintf(stderr, "Warning: Identity file %s "
				    "not accessible: %s.\n", p,
				    strerror(errno));
			else
				add_identity_file(&options, NULL, p, 1);
			free(p);
			break;
		case 'I':
#ifdef ENABLE_PKCS11
			free(options.pkcs11_provider);
			options.pkcs11_provider = xstrdup(optarg);
#else
			fprintf(stderr, "no support for PKCS#11.\n");
#endif
			break;
		case 'J':
			if (options.jump_host != NULL) {
				fatal("Only a single -J option is permitted "
				    "(use commas to separate multiple "
				    "jump hops)");
			}
			if (options.proxy_command != NULL)
				fatal("Cannot specify -J with ProxyCommand");
			if (parse_jump(optarg, &options, 1) == -1)
				fatal("Invalid -J argument");
			options.proxy_command = xstrdup("none");
			break;
		case 't':
			if (options.request_tty == REQUEST_TTY_YES)
				options.request_tty = REQUEST_TTY_FORCE;
			else
				options.request_tty = REQUEST_TTY_YES;
			break;
		case 'v':
			if (debug_flag == 0) {
				debug_flag = 1;
				options.log_level = SYSLOG_LEVEL_DEBUG1;
			} else {
				if (options.log_level < SYSLOG_LEVEL_DEBUG3) {
					debug_flag++;
					options.log_level++;
				}
			}
			break;
		case 'V':
			if (options.version_addendum &&
			    *options.version_addendum != '\0')
				fprintf(stderr, "%s %s, %s\n", SSH_RELEASE,
				    options.version_addendum,
				    OPENSSL_VERSION_STRING);
			else
				fprintf(stderr, "%s, %s\n", SSH_RELEASE,
				    OPENSSL_VERSION_STRING);
			if (opt == 'V')
				exit(0);
			break;
		case 'w':
			if (options.tun_open == -1)
				options.tun_open = SSH_TUNMODE_DEFAULT;
			options.tun_local = a2tun(optarg, &options.tun_remote);
			if (options.tun_local == SSH_TUNID_ERR) {
				fprintf(stderr,
				    "Bad tun device '%s'\n", optarg);
				exit(255);
			}
			break;
		case 'W':
			if (options.stdio_forward_host != NULL)
				fatal("stdio forward already specified");
			if (muxclient_command != 0)
				fatal("Cannot specify stdio forward with -O");
			if (parse_forward(&fwd, optarg, 1, 0)) {
				options.stdio_forward_host = fwd.listen_host;
				options.stdio_forward_port = fwd.listen_port;
				free(fwd.connect_host);
			} else {
				fprintf(stderr,
				    "Bad stdio forwarding specification '%s'\n",
				    optarg);
				exit(255);
			}
			options.request_tty = REQUEST_TTY_NO;
			options.session_type = SESSION_TYPE_NONE;
			break;
		case 'q':
			options.log_level = SYSLOG_LEVEL_QUIET;
			break;
		case 'e':
			if (optarg[0] == '^' && optarg[2] == 0 &&
			    (u_char) optarg[1] >= 64 &&
			    (u_char) optarg[1] < 128)
				options.escape_char = (u_char) optarg[1] & 31;
			else if (strlen(optarg) == 1)
				options.escape_char = (u_char) optarg[0];
			else if (strcmp(optarg, "none") == 0)
				options.escape_char = SSH_ESCAPECHAR_NONE;
			else {
				fprintf(stderr, "Bad escape character '%s'.\n",
				    optarg);
				exit(255);
			}
			break;
		case 'c':
			if (!ciphers_valid(*optarg == '+' || *optarg == '^' ?
			    optarg + 1 : optarg)) {
				fprintf(stderr, "Unknown cipher type '%s'\n",
				    optarg);
				exit(255);
			}
			free(options.ciphers);
			options.ciphers = xstrdup(optarg);
			break;
		case 'm':
			if (mac_valid(optarg)) {
				free(options.macs);
				options.macs = xstrdup(optarg);
			} else {
				fprintf(stderr, "Unknown mac type '%s'\n",
				    optarg);
				exit(255);
			}
			break;
		case 'M':
			if (options.control_master == SSHCTL_MASTER_YES)
				options.control_master = SSHCTL_MASTER_ASK;
			else
				options.control_master = SSHCTL_MASTER_YES;
			break;
		case 'p':
			if (options.port == -1) {
				options.port = a2port(optarg);
				if (options.port <= 0) {
					fprintf(stderr, "Bad port '%s'\n",
					    optarg);
					exit(255);
				}
			}
			break;
		case 'l':
			if (options.user == NULL)
				options.user = optarg;
			break;

		case 'L':
			if (parse_forward(&fwd, optarg, 0, 0))
				add_local_forward(&options, &fwd);
			else {
				fprintf(stderr,
				    "Bad local forwarding specification '%s'\n",
				    optarg);
				exit(255);
			}
			break;

		case 'R':
			if (parse_forward(&fwd, optarg, 0, 1) ||
			    parse_forward(&fwd, optarg, 1, 1)) {
				add_remote_forward(&options, &fwd);
			} else {
				fprintf(stderr,
				    "Bad remote forwarding specification "
				    "'%s'\n", optarg);
				exit(255);
			}
			break;

		case 'D':
			if (parse_forward(&fwd, optarg, 1, 0)) {
				add_local_forward(&options, &fwd);
			} else {
				fprintf(stderr,
				    "Bad dynamic forwarding specification "
				    "'%s'\n", optarg);
				exit(255);
			}
			break;

		case 'C':
#ifdef WITH_ZLIB
			options.compression = 1;
#else
			error("Compression not supported, disabling.");
#endif
			break;
		case 'N':
			if (options.session_type != -1 &&
			    options.session_type != SESSION_TYPE_NONE)
				fatal("Cannot specify -N with -s/SessionType");
			options.session_type = SESSION_TYPE_NONE;
			options.request_tty = REQUEST_TTY_NO;
			break;
		case 'T':
			options.request_tty = REQUEST_TTY_NO;
			break;
		case 'o':
			line = xstrdup(optarg);
			if (process_config_line(&options, pw,
			    host ? host : "", host ? host : "", line,
			    "command-line", 0, NULL, SSHCONF_USERCONF) != 0)
				exit(255);
			free(line);
			break;
		case 's':
			if (options.session_type != -1 &&
			    options.session_type != SESSION_TYPE_SUBSYSTEM)
				fatal("Cannot specify -s with -N/SessionType");
			options.session_type = SESSION_TYPE_SUBSYSTEM;
			break;
		case 'S':
			free(options.control_path);
			options.control_path = xstrdup(optarg);
			break;
		case 'b':
			options.bind_address = optarg;
			break;
		case 'B':
			options.bind_interface = optarg;
			break;
		case 'F':
			config = optarg;
			break;
		default:
			usage();
		}
	}

	if (optind > 1 && strcmp(av[optind - 1], "--") == 0)
		opt_terminated = 1;

	ac -= optind;
	av += optind;

	if (ac > 0 && !host) {
		int tport;
		char *tuser;
		switch (parse_ssh_uri(*av, &tuser, &host, &tport)) {
		case -1:
			usage();
			break;
		case 0:
			if (options.user == NULL) {
				options.user = tuser;
				tuser = NULL;
			}
			free(tuser);
			if (options.port == -1 && tport != -1)
				options.port = tport;
			break;
		default:
			p = xstrdup(*av);
			cp = strrchr(p, '@');
			if (cp != NULL) {
				if (cp == p)
					usage();
				if (options.user == NULL) {
					options.user = p;
					p = NULL;
				}
				*cp++ = '\0';
				host = xstrdup(cp);
				free(p);
			} else
				host = p;
			break;
		}
		if (ac > 1 && !opt_terminated) {
			optind = optreset = 1;
			goto again;
		}
		ac--, av++;
	}

	/* Check that we got a host name. */
	if (!host)
		usage();

	host_arg = xstrdup(host);

	/* Initialize the command to execute on remote host. */
	if ((command = sshbuf_new()) == NULL)
		fatal("sshbuf_new failed");

	/*
	 * Save the command to execute on the remote host in a buffer. There
	 * is no limit on the length of the command, except by the maximum
	 * packet size.  Also sets the tty flag if there is no command.
	 */
	if (!ac) {
		/* No command specified - execute shell on a tty. */
		if (options.session_type == SESSION_TYPE_SUBSYSTEM) {
			fprintf(stderr,
			    "You must specify a subsystem to invoke.\n");
			usage();
		}
	} else {
		/* A command has been specified.  Store it into the buffer. */
		for (i = 0; i < ac; i++) {
			if ((r = sshbuf_putf(command, "%s%s",
			    i ? " " : "", av[i])) != 0)
				fatal_fr(r, "buffer error");
		}
	}

	/*
	 * Initialize "log" output.  Since we are the client all output
	 * goes to stderr unless otherwise specified by -y or -E.
	 */
	if (use_syslog && logfile != NULL)
		fatal("Can't specify both -y and -E");
	if (logfile != NULL)
		log_redirect_stderr_to(logfile);
	log_init(argv0,
	    options.log_level == SYSLOG_LEVEL_NOT_SET ?
	    SYSLOG_LEVEL_INFO : options.log_level,
	    options.log_facility == SYSLOG_FACILITY_NOT_SET ?
	    SYSLOG_FACILITY_USER : options.log_facility,
	    !use_syslog);

	if (debug_flag)
		/* version_addendum is always NULL at this point */
		logit("%s, %s", SSH_RELEASE, OPENSSL_VERSION_STRING);

	/* Parse the configuration files */
	process_config_files(host_arg, pw, 0, &want_final_pass);
	if (want_final_pass)
		debug("configuration requests final Match pass");

	/* Hostname canonicalisation needs a few options filled. */
	fill_default_options_for_canonicalization(&options);

	/* If the user has replaced the hostname then take it into use now */
	if (options.hostname != NULL) {
		/* NB. Please keep in sync with readconf.c:match_cfg_line() */
		cp = percent_expand(options.hostname,
		    "h", host, (char *)NULL);
		free(host);
		host = cp;
		free(options.hostname);
		options.hostname = xstrdup(host);
	}

	/* Don't lowercase addresses, they will be explicitly canonicalised */
	if ((was_addr = is_addr(host)) == 0)
		lowercase(host);

	/*
	 * Try to canonicalize if requested by configuration or the
	 * hostname is an address.
	 */
	if (options.canonicalize_hostname != SSH_CANONICALISE_NO || was_addr)
		addrs = resolve_canonicalize(&host, options.port);

	/*
	 * If CanonicalizePermittedCNAMEs have been specified but
	 * other canonicalization did not happen (by not being requested
	 * or by failing with fallback) then the hostname may still be changed
	 * as a result of CNAME following.
	 *
	 * Try to resolve the bare hostname name using the system resolver's
	 * usual search rules and then apply the CNAME follow rules.
	 *
	 * Skip the lookup if a ProxyCommand is being used unless the user
	 * has specifically requested canonicalisation for this case via
	 * CanonicalizeHostname=always
	 */
	direct = option_clear_or_none(options.proxy_command) &&
	    options.jump_host == NULL;
	if (addrs == NULL && config_has_permitted_cnames(&options) && (direct ||
	    options.canonicalize_hostname == SSH_CANONICALISE_ALWAYS)) {
		if ((addrs = resolve_host(host, options.port,
		    direct, cname, sizeof(cname))) == NULL) {
			/* Don't fatal proxied host names not in the DNS */
			if (direct)
				cleanup_exit(255); /* logged in resolve_host */
		} else
			check_follow_cname(direct, &host, cname);
	}

	/*
	 * If canonicalisation is enabled then re-parse the configuration
	 * files as new stanzas may match.
	 */
	if (options.canonicalize_hostname != 0 && !want_final_pass) {
		debug("hostname canonicalisation enabled, "
		    "will re-parse configuration");
		want_final_pass = 1;
	}

	if (want_final_pass) {
		debug("re-parsing configuration");
		free(options.hostname);
		options.hostname = xstrdup(host);
		process_config_files(host_arg, pw, 1, NULL);
		/*
		 * Address resolution happens early with canonicalisation
		 * enabled and the port number may have changed since, so
		 * reset it in address list
		 */
		if (addrs != NULL && options.port > 0)
			set_addrinfo_port(addrs, options.port);
	}

	/* Fill configuration defaults. */
	if (fill_default_options(&options) != 0)
		cleanup_exit(255);

	if (options.user == NULL)
		options.user = xstrdup(pw->pw_name);

	/*
	 * If ProxyJump option specified, then construct a ProxyCommand now.
	 */
	if (options.jump_host != NULL) {
		char port_s[8];
		const char *jumpuser = options.jump_user, *sshbin = argv0;
		int port = options.port, jumpport = options.jump_port;

		if (port <= 0)
			port = default_ssh_port();
		if (jumpport <= 0)
			jumpport = default_ssh_port();
		if (jumpuser == NULL)
			jumpuser = options.user;
		if (strcmp(options.jump_host, host) == 0 && port == jumpport &&
		    strcmp(options.user, jumpuser) == 0)
			fatal("jumphost loop via %s", options.jump_host);

		/*
		 * Try to use SSH indicated by argv[0], but fall back to
		 * "ssh" if it appears unavailable.
		 */
		if (strchr(argv0, '/') != NULL && access(argv0, X_OK) != 0)
			sshbin = "ssh";

		/* Consistency check */
		if (options.proxy_command != NULL)
			fatal("inconsistent options: ProxyCommand+ProxyJump");
		/* Never use FD passing for ProxyJump */
		options.proxy_use_fdpass = 0;
		snprintf(port_s, sizeof(port_s), "%d", options.jump_port);
		xasprintf(&options.proxy_command,
		    "%s%s%s%s%s%s%s%s%s%s%.*s -W '[%%h]:%%p' %s",
		    sshbin,
		    /* Optional "-l user" argument if jump_user set */
		    options.jump_user == NULL ? "" : " -l ",
		    options.jump_user == NULL ? "" : options.jump_user,
		    /* Optional "-p port" argument if jump_port set */
		    options.jump_port <= 0 ? "" : " -p ",
		    options.jump_port <= 0 ? "" : port_s,
		    /* Optional additional jump hosts ",..." */
		    options.jump_extra == NULL ? "" : " -J ",
		    options.jump_extra == NULL ? "" : options.jump_extra,
		    /* Optional "-F" argumment if -F specified */
		    config == NULL ? "" : " -F ",
		    config == NULL ? "" : config,
		    /* Optional "-v" arguments if -v set */
		    debug_flag ? " -" : "",
		    debug_flag, "vvv",
		    /* Mandatory hostname */
		    options.jump_host);
		debug("Setting implicit ProxyCommand from ProxyJump: %s",
		    options.proxy_command);
	}

	if (options.port == 0)
		options.port = default_ssh_port();
	channel_set_af(ssh, options.address_family);

	/* Tidy and check options */
	if (options.host_key_alias != NULL)
		lowercase(options.host_key_alias);
	if (options.proxy_command != NULL &&
	    strcmp(options.proxy_command, "-") == 0 &&
	    options.proxy_use_fdpass)
		fatal("ProxyCommand=- and ProxyUseFDPass are incompatible");
	if (options.update_hostkeys == SSH_UPDATE_HOSTKEYS_ASK) {
		if (options.control_persist && options.control_path != NULL) {
			debug("UpdateHostKeys=ask is incompatible with "
			    "ControlPersist; disabling");
			options.update_hostkeys = 0;
		} else if (sshbuf_len(command) != 0 ||
		    options.remote_command != NULL ||
		    options.request_tty == REQUEST_TTY_NO) {
			debug("UpdateHostKeys=ask is incompatible with "
			    "remote command execution; disabling");
			options.update_hostkeys = 0;
		} else if (options.log_level < SYSLOG_LEVEL_INFO) {
			/* no point logging anything; user won't see it */
			options.update_hostkeys = 0;
		}
	}
	if (options.connection_attempts <= 0)
		fatal("Invalid number of ConnectionAttempts");

	if (sshbuf_len(command) != 0 && options.remote_command != NULL)
		fatal("Cannot execute command-line and remote command.");

	/* Cannot fork to background if no command. */
	if (options.fork_after_authentication && sshbuf_len(command) == 0 &&
	    options.remote_command == NULL &&
	    options.session_type != SESSION_TYPE_NONE)
		fatal("Cannot fork into background without a command "
		    "to execute.");

	/* reinit */
	log_init(argv0, options.log_level, options.log_facility, !use_syslog);
	for (j = 0; j < options.num_log_verbose; j++) {
		if (strcasecmp(options.log_verbose[j], "none") == 0)
			break;
		log_verbose_add(options.log_verbose[j]);
	}

	if (options.request_tty == REQUEST_TTY_YES ||
	    options.request_tty == REQUEST_TTY_FORCE)
		tty_flag = 1;

	/* Allocate a tty by default if no command specified. */
	if (sshbuf_len(command) == 0 && options.remote_command == NULL)
		tty_flag = options.request_tty != REQUEST_TTY_NO;

	/* Force no tty */
	if (options.request_tty == REQUEST_TTY_NO ||
	    (muxclient_command && muxclient_command != SSHMUX_COMMAND_PROXY))
		tty_flag = 0;
	/* Do not allocate a tty if stdin is not a tty. */
	if ((!isatty(fileno(stdin)) || options.stdin_null) &&
	    options.request_tty != REQUEST_TTY_FORCE) {
		if (tty_flag)
			logit("Pseudo-terminal will not be allocated because "
			    "stdin is not a terminal.");
		tty_flag = 0;
	}

	/* Set up strings used to percent_expand() arguments */
	cinfo = xcalloc(1, sizeof(*cinfo));
	if (gethostname(thishost, sizeof(thishost)) == -1)
		fatal("gethostname: %s", strerror(errno));
	cinfo->thishost = xstrdup(thishost);
	thishost[strcspn(thishost, ".")] = '\0';
	cinfo->shorthost = xstrdup(thishost);
	xasprintf(&cinfo->portstr, "%d", options.port);
	xasprintf(&cinfo->uidstr, "%llu",
	    (unsigned long long)pw->pw_uid);
	cinfo->keyalias = xstrdup(options.host_key_alias ?
	    options.host_key_alias : host_arg);
	cinfo->conn_hash_hex = ssh_connection_hash(cinfo->thishost, host,
	    cinfo->portstr, options.user);
	cinfo->host_arg = xstrdup(host_arg);
	cinfo->remhost = xstrdup(host);
	cinfo->remuser = xstrdup(options.user);
	cinfo->homedir = xstrdup(pw->pw_dir);
	cinfo->locuser = xstrdup(pw->pw_name);

	/* Find canonic host name. */
	if (strchr(host, '.') == 0) {
		struct addrinfo hints;
		struct addrinfo *ai = NULL;
		int errgai;
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = options.address_family;
		hints.ai_flags = AI_CANONNAME;
		hints.ai_socktype = SOCK_STREAM;
		errgai = getaddrinfo(host, NULL, &hints, &ai);
		if (errgai == 0) {
			if (ai->ai_canonname != NULL)
				host = xstrdup(ai->ai_canonname);
			freeaddrinfo(ai);
		}
	}

	/*
	 * Expand tokens in arguments. NB. LocalCommand is expanded later,
	 * after port-forwarding is set up, so it may pick up any local
	 * tunnel interface name allocated.
	 */
	if (options.remote_command != NULL) {
		debug3("expanding RemoteCommand: %s", options.remote_command);
		cp = options.remote_command;
		options.remote_command = default_client_percent_expand(cp,
		    cinfo);
		debug3("expanded RemoteCommand: %s", options.remote_command);
		free(cp);
		if ((r = sshbuf_put(command, options.remote_command,
		    strlen(options.remote_command))) != 0)
			fatal_fr(r, "buffer error");
	}

	if (options.control_path != NULL) {
		cp = tilde_expand_filename(options.control_path, getuid());
		free(options.control_path);
		options.control_path = default_client_percent_dollar_expand(cp,
		    cinfo);
		free(cp);
	}

	if (options.identity_agent != NULL) {
		p = tilde_expand_filename(options.identity_agent, getuid());
		cp = default_client_percent_dollar_expand(p, cinfo);
		free(p);
		free(options.identity_agent);
		options.identity_agent = cp;
	}

	if (options.forward_agent_sock_path != NULL) {
		p = tilde_expand_filename(options.forward_agent_sock_path,
		    getuid());
		cp = default_client_percent_dollar_expand(p, cinfo);
		free(p);
		free(options.forward_agent_sock_path);
		options.forward_agent_sock_path = cp;
		if (stat(options.forward_agent_sock_path, &st) != 0) {
			error("Cannot forward agent socket path \"%s\": %s",
			    options.forward_agent_sock_path, strerror(errno));
			if (options.exit_on_forward_failure)
				cleanup_exit(255);
		}
	}

	if (options.num_system_hostfiles > 0 &&
	    strcasecmp(options.system_hostfiles[0], "none") == 0) {
		if (options.num_system_hostfiles > 1)
			fatal("Invalid GlobalKnownHostsFiles: \"none\" "
			    "appears with other entries");
		free(options.system_hostfiles[0]);
		options.system_hostfiles[0] = NULL;
		options.num_system_hostfiles = 0;
	}

	if (options.num_user_hostfiles > 0 &&
	    strcasecmp(options.user_hostfiles[0], "none") == 0) {
		if (options.num_user_hostfiles > 1)
			fatal("Invalid UserKnownHostsFiles: \"none\" "
			    "appears with other entries");
		free(options.user_hostfiles[0]);
		options.user_hostfiles[0] = NULL;
		options.num_user_hostfiles = 0;
	}
	for (j = 0; j < options.num_user_hostfiles; j++) {
		if (options.user_hostfiles[j] == NULL)
			continue;
		cp = tilde_expand_filename(options.user_hostfiles[j], getuid());
		p = default_client_percent_dollar_expand(cp, cinfo);
		if (strcmp(options.user_hostfiles[j], p) != 0)
			debug3("expanded UserKnownHostsFile '%s' -> "
			    "'%s'", options.user_hostfiles[j], p);
		free(options.user_hostfiles[j]);
		free(cp);
		options.user_hostfiles[j] = p;
	}

	for (i = 0; i < options.num_local_forwards; i++) {
		if (options.local_forwards[i].listen_path != NULL) {
			cp = options.local_forwards[i].listen_path;
			p = options.local_forwards[i].listen_path =
			    default_client_percent_expand(cp, cinfo);
			if (strcmp(cp, p) != 0)
				debug3("expanded LocalForward listen path "
				    "'%s' -> '%s'", cp, p);
			free(cp);
		}
		if (options.local_forwards[i].connect_path != NULL) {
			cp = options.local_forwards[i].connect_path;
			p = options.local_forwards[i].connect_path =
			    default_client_percent_expand(cp, cinfo);
			if (strcmp(cp, p) != 0)
				debug3("expanded LocalForward connect path "
				    "'%s' -> '%s'", cp, p);
			free(cp);
		}
	}

	for (i = 0; i < options.num_remote_forwards; i++) {
		if (options.remote_forwards[i].listen_path != NULL) {
			cp = options.remote_forwards[i].listen_path;
			p = options.remote_forwards[i].listen_path =
			    default_client_percent_expand(cp, cinfo);
			if (strcmp(cp, p) != 0)
				debug3("expanded RemoteForward listen path "
				    "'%s' -> '%s'", cp, p);
			free(cp);
		}
		if (options.remote_forwards[i].connect_path != NULL) {
			cp = options.remote_forwards[i].connect_path;
			p = options.remote_forwards[i].connect_path =
			    default_client_percent_expand(cp, cinfo);
			if (strcmp(cp, p) != 0)
				debug3("expanded RemoteForward connect path "
				    "'%s' -> '%s'", cp, p);
			free(cp);
		}
	}

	if (config_test) {
		dump_client_config(&options, host);
		exit(0);
	}

	/* Expand SecurityKeyProvider if it refers to an environment variable */
	if (options.sk_provider != NULL && *options.sk_provider == '$' &&
	    strlen(options.sk_provider) > 1) {
		if ((cp = getenv(options.sk_provider + 1)) == NULL) {
			debug("Authenticator provider %s did not resolve; "
			    "disabling", options.sk_provider);
			free(options.sk_provider);
			options.sk_provider = NULL;
		} else {
			debug2("resolved SecurityKeyProvider %s => %s",
			    options.sk_provider, cp);
			free(options.sk_provider);
			options.sk_provider = xstrdup(cp);
		}
	}

	if (muxclient_command != 0 && options.control_path == NULL)
		fatal("No ControlPath specified for \"-O\" command");
	if (options.control_path != NULL) {
		int sock;
		if ((sock = muxclient(options.control_path)) >= 0) {
			ssh_packet_set_connection(ssh, sock, sock);
			ssh_packet_set_mux(ssh);
			goto skip_connect;
		}
	}

	/*
	 * If hostname canonicalisation was not enabled, then we may not
	 * have yet resolved the hostname. Do so now.
	 */
	if (addrs == NULL && options.proxy_command == NULL) {
		debug2("resolving \"%s\" port %d", host, options.port);
		if ((addrs = resolve_host(host, options.port, 1,
		    cname, sizeof(cname))) == NULL)
			cleanup_exit(255); /* resolve_host logs the error */
	}

	if (options.connection_timeout >= INT_MAX/1000)
		timeout_ms = INT_MAX;
	else
		timeout_ms = options.connection_timeout * 1000;

	/* Open a connection to the remote host. */
	if (ssh_connect(ssh, host, host_arg, addrs, &hostaddr, options.port,
	    options.connection_attempts,
	    &timeout_ms, options.tcp_keep_alive) != 0)
		exit(255);

	if (addrs != NULL)
		freeaddrinfo(addrs);

	ssh_packet_set_timeout(ssh, options.server_alive_interval,
	    options.server_alive_count_max);

	if (timeout_ms > 0)
		debug3("timeout: %d ms remain after connect", timeout_ms);

	/*
	 * If we successfully made the connection and we have hostbased auth
	 * enabled, load the public keys so we can later use the ssh-keysign
	 * helper to sign challenges.
	 */
	sensitive_data.nkeys = 0;
	sensitive_data.keys = NULL;
	if (options.hostbased_authentication) {
		sensitive_data.nkeys = 10;
		sensitive_data.keys = xcalloc(sensitive_data.nkeys,
		    sizeof(struct sshkey));

		/* XXX check errors? */
#define L_PUBKEY(p,o) do { \
	if ((o) >= sensitive_data.nkeys) \
		fatal_f("pubkey out of array bounds"); \
	check_load(sshkey_load_public(p, &(sensitive_data.keys[o]), NULL), \
	    p, "pubkey"); \
} while (0)
#define L_CERT(p,o) do { \
	if ((o) >= sensitive_data.nkeys) \
		fatal_f("cert out of array bounds"); \
	check_load(sshkey_load_cert(p, &(sensitive_data.keys[o])), p, "cert"); \
} while (0)

		if (options.hostbased_authentication == 1) {
			L_CERT(_PATH_HOST_ECDSA_KEY_FILE, 0);
			L_CERT(_PATH_HOST_ED25519_KEY_FILE, 1);
			L_CERT(_PATH_HOST_RSA_KEY_FILE, 2);
			L_CERT(_PATH_HOST_DSA_KEY_FILE, 3);
			L_PUBKEY(_PATH_HOST_ECDSA_KEY_FILE, 4);
			L_PUBKEY(_PATH_HOST_ED25519_KEY_FILE, 5);
			L_PUBKEY(_PATH_HOST_RSA_KEY_FILE, 6);
			L_PUBKEY(_PATH_HOST_DSA_KEY_FILE, 7);
			L_CERT(_PATH_HOST_XMSS_KEY_FILE, 8);
			L_PUBKEY(_PATH_HOST_XMSS_KEY_FILE, 9);
		}
	}

	/* load options.identity_files */
	load_public_identity_files(cinfo);

	/* optionally set the SSH_AUTHSOCKET_ENV_NAME variable */
	if (options.identity_agent &&
	    strcmp(options.identity_agent, SSH_AUTHSOCKET_ENV_NAME) != 0) {
		if (strcmp(options.identity_agent, "none") == 0) {
			unsetenv(SSH_AUTHSOCKET_ENV_NAME);
		} else {
			cp = options.identity_agent;
			/* legacy (limited) format */
			if (cp[0] == '$' && cp[1] != '{') {
				if (!valid_env_name(cp + 1)) {
					fatal("Invalid IdentityAgent "
					    "environment variable name %s", cp);
				}
				if ((p = getenv(cp + 1)) == NULL)
					unsetenv(SSH_AUTHSOCKET_ENV_NAME);
				else
					setenv(SSH_AUTHSOCKET_ENV_NAME, p, 1);
			} else {
				/* identity_agent specifies a path directly */
				setenv(SSH_AUTHSOCKET_ENV_NAME, cp, 1);
			}
		}
	}

	if (options.forward_agent && options.forward_agent_sock_path != NULL) {
		cp = options.forward_agent_sock_path;
		if (cp[0] == '$') {
			if (!valid_env_name(cp + 1)) {
				fatal("Invalid ForwardAgent environment variable name %s", cp);
			}
			if ((p = getenv(cp + 1)) != NULL)
				forward_agent_sock_path = xstrdup(p);
			else
				options.forward_agent = 0;
			free(cp);
		} else {
			forward_agent_sock_path = cp;
		}
	}

	/* Expand ~ in known host file names. */
	tilde_expand_paths(options.system_hostfiles,
	    options.num_system_hostfiles);
	tilde_expand_paths(options.user_hostfiles, options.num_user_hostfiles);

	ssh_signal(SIGPIPE, SIG_IGN); /* ignore SIGPIPE early */
	ssh_signal(SIGCHLD, main_sigchld_handler);

	/* Log into the remote system.  Never returns if the login fails. */
	ssh_login(ssh, &sensitive_data, host, (struct sockaddr *)&hostaddr,
	    options.port, pw, timeout_ms, cinfo);

	/* We no longer need the private host keys.  Clear them now. */
	if (sensitive_data.nkeys != 0) {
		for (i = 0; i < sensitive_data.nkeys; i++) {
			if (sensitive_data.keys[i] != NULL) {
				/* Destroys contents safely */
				debug3("clear hostkey %d", i);
				sshkey_free(sensitive_data.keys[i]);
				sensitive_data.keys[i] = NULL;
			}
		}
		free(sensitive_data.keys);
	}
	for (i = 0; i < options.num_identity_files; i++) {
		free(options.identity_files[i]);
		options.identity_files[i] = NULL;
		if (options.identity_keys[i]) {
			sshkey_free(options.identity_keys[i]);
			options.identity_keys[i] = NULL;
		}
	}
	for (i = 0; i < options.num_certificate_files; i++) {
		free(options.certificate_files[i]);
		options.certificate_files[i] = NULL;
	}

#ifdef ENABLE_PKCS11
	(void)pkcs11_del_provider(options.pkcs11_provider);
#endif

 skip_connect:
	exit_status = ssh_session2(ssh, cinfo);
	ssh_conn_info_free(cinfo);
	ssh_packet_close(ssh);

	if (options.control_path != NULL && muxserver_sock != -1)
		unlink(options.control_path);

	/* Kill ProxyCommand if it is running. */
	ssh_kill_proxy_command();

	return exit_status;
}

static void
control_persist_detach(void)
{
	pid_t pid;

	debug_f("backgrounding master process");

	/*
	 * master (current process) into the background, and make the
	 * foreground process a client of the backgrounded master.
	 */
	switch ((pid = fork())) {
	case -1:
		fatal_f("fork: %s", strerror(errno));
	case 0:
		/* Child: master process continues mainloop */
		break;
	default:
		/* Parent: set up mux client to connect to backgrounded master */
		debug2_f("background process is %ld", (long)pid);
		options.stdin_null = ostdin_null_flag;
		options.request_tty = orequest_tty;
		tty_flag = otty_flag;
		options.session_type = osession_type;
		close(muxserver_sock);
		muxserver_sock = -1;
		options.control_master = SSHCTL_MASTER_NO;
		muxclient(options.control_path);
		/* muxclient() doesn't return on success. */
		fatal("Failed to connect to new control master");
	}
	if (stdfd_devnull(1, 1, !(log_is_on_stderr() && debug_flag)) == -1)
		error_f("stdfd_devnull failed");
	daemon(1, 1);
	setproctitle("%s [mux]", options.control_path);
}

/* Do fork() after authentication. Used by "ssh -f" */
static void
fork_postauth(void)
{
	if (need_controlpersist_detach)
		control_persist_detach();
	debug("forking to background");
	options.fork_after_authentication = 0;
	if (daemon(1, 1) == -1)
		fatal("daemon() failed: %.200s", strerror(errno));
	if (stdfd_devnull(1, 1, !(log_is_on_stderr() && debug_flag)) == -1)
		error_f("stdfd_devnull failed");
}

static void
forwarding_success(void)
{
	if (forward_confirms_pending == -1)
		return;
	if (--forward_confirms_pending == 0) {
		debug_f("all expected forwarding replies received");
		if (options.fork_after_authentication)
			fork_postauth();
	} else {
		debug2_f("%d expected forwarding replies remaining",
		    forward_confirms_pending);
	}
}

/* Callback for remote forward global requests */
static void
ssh_confirm_remote_forward(struct ssh *ssh, int type, u_int32_t seq, void *ctxt)
{
	struct Forward *rfwd = (struct Forward *)ctxt;
	u_int port;
	int r;

	/* XXX verbose() on failure? */
	debug("remote forward %s for: listen %s%s%d, connect %s:%d",
	    type == SSH2_MSG_REQUEST_SUCCESS ? "success" : "failure",
	    rfwd->listen_path ? rfwd->listen_path :
	    rfwd->listen_host ? rfwd->listen_host : "",
	    (rfwd->listen_path || rfwd->listen_host) ? ":" : "",
	    rfwd->listen_port, rfwd->connect_path ? rfwd->connect_path :
	    rfwd->connect_host, rfwd->connect_port);
	if (rfwd->listen_path == NULL && rfwd->listen_port == 0) {
		if (type == SSH2_MSG_REQUEST_SUCCESS) {
			if ((r = sshpkt_get_u32(ssh, &port)) != 0)
				fatal_fr(r, "parse packet");
			if (port > 65535) {
				error("Invalid allocated port %u for remote "
				    "forward to %s:%d", port,
				    rfwd->connect_host, rfwd->connect_port);
				/* Ensure failure processing runs below */
				type = SSH2_MSG_REQUEST_FAILURE;
				channel_update_permission(ssh,
				    rfwd->handle, -1);
			} else {
				rfwd->allocated_port = (int)port;
				logit("Allocated port %u for remote "
				    "forward to %s:%d",
				    rfwd->allocated_port, rfwd->connect_path ?
				    rfwd->connect_path : rfwd->connect_host,
				    rfwd->connect_port);
				channel_update_permission(ssh,
				    rfwd->handle, rfwd->allocated_port);
			}
		} else {
			channel_update_permission(ssh, rfwd->handle, -1);
		}
	}

	if (type == SSH2_MSG_REQUEST_FAILURE) {
		if (options.exit_on_forward_failure) {
			if (rfwd->listen_path != NULL)
				fatal("Error: remote port forwarding failed "
				    "for listen path %s", rfwd->listen_path);
			else
				fatal("Error: remote port forwarding failed "
				    "for listen port %d", rfwd->listen_port);
		} else {
			if (rfwd->listen_path != NULL)
				logit("Warning: remote port forwarding failed "
				    "for listen path %s", rfwd->listen_path);
			else
				logit("Warning: remote port forwarding failed "
				    "for listen port %d", rfwd->listen_port);
		}
	}
	forwarding_success();
}

static void
client_cleanup_stdio_fwd(struct ssh *ssh, int id, void *arg)
{
	debug("stdio forwarding: done");
	cleanup_exit(0);
}

static void
ssh_stdio_confirm(struct ssh *ssh, int id, int success, void *arg)
{
	if (!success)
		fatal("stdio forwarding failed");
}

static void
ssh_tun_confirm(struct ssh *ssh, int id, int success, void *arg)
{
	if (!success) {
		error("Tunnel forwarding failed");
		if (options.exit_on_forward_failure)
			cleanup_exit(255);
	}

	debug_f("tunnel forward established, id=%d", id);
	forwarding_success();
}

static void
ssh_init_stdio_forwarding(struct ssh *ssh)
{
	Channel *c;
	int in, out;

	if (options.stdio_forward_host == NULL)
		return;

	debug3_f("%s:%d", options.stdio_forward_host,
	    options.stdio_forward_port);

	if ((in = dup(STDIN_FILENO)) == -1 ||
	    (out = dup(STDOUT_FILENO)) == -1)
		fatal_f("dup() in/out failed");
	if ((c = channel_connect_stdio_fwd(ssh, options.stdio_forward_host,
	    options.stdio_forward_port, in, out,
	    CHANNEL_NONBLOCK_STDIO)) == NULL)
		fatal_f("channel_connect_stdio_fwd failed");
	channel_register_cleanup(ssh, c->self, client_cleanup_stdio_fwd, 0);
	channel_register_open_confirm(ssh, c->self, ssh_stdio_confirm, NULL);
}

static void
ssh_init_forward_permissions(struct ssh *ssh, const char *what, char **opens,
    u_int num_opens)
{
	u_int i;
	int port;
	char *addr, *arg, *oarg, ch;
	int where = FORWARD_LOCAL;

	channel_clear_permission(ssh, FORWARD_ADM, where);
	if (num_opens == 0)
		return; /* permit any */

	/* handle keywords: "any" / "none" */
	if (num_opens == 1 && strcmp(opens[0], "any") == 0)
		return;
	if (num_opens == 1 && strcmp(opens[0], "none") == 0) {
		channel_disable_admin(ssh, where);
		return;
	}
	/* Otherwise treat it as a list of permitted host:port */
	for (i = 0; i < num_opens; i++) {
		oarg = arg = xstrdup(opens[i]);
		ch = '\0';
		addr = hpdelim2(&arg, &ch);
		if (addr == NULL || ch == '/')
			fatal_f("missing host in %s", what);
		addr = cleanhostname(addr);
		if (arg == NULL || ((port = permitopen_port(arg)) < 0))
			fatal_f("bad port number in %s", what);
		/* Send it to channels layer */
		channel_add_permission(ssh, FORWARD_ADM,
		    where, addr, port);
		free(oarg);
	}
}

static void
ssh_init_forwarding(struct ssh *ssh, char **ifname)
{
	int success = 0;
	int i;

	ssh_init_forward_permissions(ssh, "permitremoteopen",
	    options.permitted_remote_opens,
	    options.num_permitted_remote_opens);

	if (options.exit_on_forward_failure)
		forward_confirms_pending = 0; /* track pending requests */
	/* Initiate local TCP/IP port forwardings. */
	for (i = 0; i < options.num_local_forwards; i++) {
		debug("Local connections to %.200s:%d forwarded to remote "
		    "address %.200s:%d",
		    (options.local_forwards[i].listen_path != NULL) ?
		    options.local_forwards[i].listen_path :
		    (options.local_forwards[i].listen_host == NULL) ?
		    (options.fwd_opts.gateway_ports ? "*" : "LOCALHOST") :
		    options.local_forwards[i].listen_host,
		    options.local_forwards[i].listen_port,
		    (options.local_forwards[i].connect_path != NULL) ?
		    options.local_forwards[i].connect_path :
		    options.local_forwards[i].connect_host,
		    options.local_forwards[i].connect_port);
		success += channel_setup_local_fwd_listener(ssh,
		    &options.local_forwards[i], &options.fwd_opts);
	}
	if (i > 0 && success != i && options.exit_on_forward_failure)
		fatal("Could not request local forwarding.");
	if (i > 0 && success == 0)
		error("Could not request local forwarding.");

	/* Initiate remote TCP/IP port forwardings. */
	for (i = 0; i < options.num_remote_forwards; i++) {
		debug("Remote connections from %.200s:%d forwarded to "
		    "local address %.200s:%d",
		    (options.remote_forwards[i].listen_path != NULL) ?
		    options.remote_forwards[i].listen_path :
		    (options.remote_forwards[i].listen_host == NULL) ?
		    "LOCALHOST" : options.remote_forwards[i].listen_host,
		    options.remote_forwards[i].listen_port,
		    (options.remote_forwards[i].connect_path != NULL) ?
		    options.remote_forwards[i].connect_path :
		    options.remote_forwards[i].connect_host,
		    options.remote_forwards[i].connect_port);
		if ((options.remote_forwards[i].handle =
		    channel_request_remote_forwarding(ssh,
		    &options.remote_forwards[i])) >= 0) {
			client_register_global_confirm(
			    ssh_confirm_remote_forward,
			    &options.remote_forwards[i]);
			forward_confirms_pending++;
		} else if (options.exit_on_forward_failure)
			fatal("Could not request remote forwarding.");
		else
			logit("Warning: Could not request remote forwarding.");
	}

	/* Initiate tunnel forwarding. */
	if (options.tun_open != SSH_TUNMODE_NO) {
		if ((*ifname = client_request_tun_fwd(ssh,
		    options.tun_open, options.tun_local,
		    options.tun_remote, ssh_tun_confirm, NULL)) != NULL)
			forward_confirms_pending++;
		else if (options.exit_on_forward_failure)
			fatal("Could not request tunnel forwarding.");
		else
			error("Could not request tunnel forwarding.");
	}
	if (forward_confirms_pending > 0) {
		debug_f("expecting replies for %d forwards",
		    forward_confirms_pending);
	}
}

static void
check_agent_present(void)
{
	int r;

	if (options.forward_agent) {
		/* Clear agent forwarding if we don't have an agent. */
		if ((r = ssh_get_authentication_socket(NULL)) != 0) {
			options.forward_agent = 0;
			if (r != SSH_ERR_AGENT_NOT_PRESENT)
				debug_r(r, "ssh_get_authentication_socket");
		}
	}
}

static void
ssh_session2_setup(struct ssh *ssh, int id, int success, void *arg)
{
	extern char **environ;
	const char *display, *term;
	int r, interactive = tty_flag;
	char *proto = NULL, *data = NULL;

	if (!success)
		return; /* No need for error message, channels code sens one */

	display = getenv("DISPLAY");
	if (display == NULL && options.forward_x11)
		debug("X11 forwarding requested but DISPLAY not set");
	if (options.forward_x11 && client_x11_get_proto(ssh, display,
	    options.xauth_location, options.forward_x11_trusted,
	    options.forward_x11_timeout, &proto, &data) == 0) {
		/* Request forwarding with authentication spoofing. */
		debug("Requesting X11 forwarding with authentication "
		    "spoofing.");
		x11_request_forwarding_with_spoofing(ssh, id, display, proto,
		    data, 1);
		client_expect_confirm(ssh, id, "X11 forwarding", CONFIRM_WARN);
		/* XXX exit_on_forward_failure */
		interactive = 1;
	}

	check_agent_present();
	if (options.forward_agent) {
		debug("Requesting authentication agent forwarding.");
		channel_request_start(ssh, id, "auth-agent-req@openssh.com", 0);
		if ((r = sshpkt_send(ssh)) != 0)
			fatal_fr(r, "send packet");
	}

	/* Tell the packet module whether this is an interactive session. */
	ssh_packet_set_interactive(ssh, interactive,
	    options.ip_qos_interactive, options.ip_qos_bulk);

	if ((term = lookup_env_in_list("TERM", options.setenv,
	    options.num_setenv)) == NULL || *term == '\0')
		term = getenv("TERM");
	client_session2_setup(ssh, id, tty_flag,
	    options.session_type == SESSION_TYPE_SUBSYSTEM, term,
	    NULL, fileno(stdin), command, environ);
}

/* open new channel for a session */
static int
ssh_session2_open(struct ssh *ssh)
{
	Channel *c;
	int window, packetmax, in, out, err;

	if (options.stdin_null) {
		in = open(_PATH_DEVNULL, O_RDONLY);
	} else {
		in = dup(STDIN_FILENO);
	}
	out = dup(STDOUT_FILENO);
	err = dup(STDERR_FILENO);

	if (in == -1 || out == -1 || err == -1)
		fatal("dup() in/out/err failed");

	window = CHAN_SES_WINDOW_DEFAULT;
	packetmax = CHAN_SES_PACKET_DEFAULT;
	if (tty_flag) {
		window >>= 1;
		packetmax >>= 1;
	}
	c = channel_new(ssh,
	    "session", SSH_CHANNEL_OPENING, in, out, err,
	    window, packetmax, CHAN_EXTENDED_WRITE,
	    "client-session", CHANNEL_NONBLOCK_STDIO);

	debug3_f("channel_new: %d", c->self);

	channel_send_open(ssh, c->self);
	if (options.session_type != SESSION_TYPE_NONE)
		channel_register_open_confirm(ssh, c->self,
		    ssh_session2_setup, NULL);

	return c->self;
}

static int
ssh_session2(struct ssh *ssh, const struct ssh_conn_info *cinfo)
{
	int r, id = -1;
	char *cp, *tun_fwd_ifname = NULL;

	/* XXX should be pre-session */
	if (!options.control_persist)
		ssh_init_stdio_forwarding(ssh);

	ssh_init_forwarding(ssh, &tun_fwd_ifname);

	if (options.local_command != NULL) {
		debug3("expanding LocalCommand: %s", options.local_command);
		cp = options.local_command;
		options.local_command = percent_expand(cp,
		    DEFAULT_CLIENT_PERCENT_EXPAND_ARGS(cinfo),
		    "T", tun_fwd_ifname == NULL ? "NONE" : tun_fwd_ifname,
		    (char *)NULL);
		debug3("expanded LocalCommand: %s", options.local_command);
		free(cp);
	}

	/* Start listening for multiplex clients */
	if (!ssh_packet_get_mux(ssh))
		muxserver_listen(ssh);

	/*
	 * If we are in control persist mode and have a working mux listen
	 * socket, then prepare to background ourselves and have a foreground
	 * client attach as a control client.
	 * NB. we must save copies of the flags that we override for
	 * the backgrounding, since we defer attachment of the client until
	 * after the connection is fully established (in particular,
	 * async rfwd replies have been received for ExitOnForwardFailure).
	 */
	if (options.control_persist && muxserver_sock != -1) {
		ostdin_null_flag = options.stdin_null;
		osession_type = options.session_type;
		orequest_tty = options.request_tty;
		otty_flag = tty_flag;
		options.stdin_null = 1;
		options.session_type = SESSION_TYPE_NONE;
		tty_flag = 0;
		if (!options.fork_after_authentication &&
		    (osession_type != SESSION_TYPE_NONE ||
		    options.stdio_forward_host != NULL))
			need_controlpersist_detach = 1;
		options.fork_after_authentication = 1;
	}
	/*
	 * ControlPersist mux listen socket setup failed, attempt the
	 * stdio forward setup that we skipped earlier.
	 */
	if (options.control_persist && muxserver_sock == -1)
		ssh_init_stdio_forwarding(ssh);

	if (options.session_type != SESSION_TYPE_NONE)
		id = ssh_session2_open(ssh);
	else {
		ssh_packet_set_interactive(ssh,
		    options.control_master == SSHCTL_MASTER_NO,
		    options.ip_qos_interactive, options.ip_qos_bulk);
	}

	/* If we don't expect to open a new session, then disallow it */
	if (options.control_master == SSHCTL_MASTER_NO &&
	    (ssh->compat & SSH_NEW_OPENSSH)) {
		debug("Requesting no-more-sessions@openssh.com");
		if ((r = sshpkt_start(ssh, SSH2_MSG_GLOBAL_REQUEST)) != 0 ||
		    (r = sshpkt_put_cstring(ssh,
		    "no-more-sessions@openssh.com")) != 0 ||
		    (r = sshpkt_put_u8(ssh, 0)) != 0 ||
		    (r = sshpkt_send(ssh)) != 0)
			fatal_fr(r, "send packet");
	}

	/* Execute a local command */
	if (options.local_command != NULL &&
	    options.permit_local_command)
		ssh_local_cmd(options.local_command);

	/*
	 * stdout is now owned by the session channel; clobber it here
	 * so future channel closes are propagated to the local fd.
	 * NB. this can only happen after LocalCommand has completed,
	 * as it may want to write to stdout.
	 */
	if (!need_controlpersist_detach && stdfd_devnull(0, 1, 0) == -1)
		error_f("stdfd_devnull failed");

	/*
	 * If requested and we are not interested in replies to remote
	 * forwarding requests, then let ssh continue in the background.
	 */
	if (options.fork_after_authentication) {
		if (options.exit_on_forward_failure &&
		    options.num_remote_forwards > 0) {
			debug("deferring postauth fork until remote forward "
			    "confirmation received");
		} else
			fork_postauth();
	}

	return client_loop(ssh, tty_flag, tty_flag ?
	    options.escape_char : SSH_ESCAPECHAR_NONE, id);
}

/* Loads all IdentityFile and CertificateFile keys */
static void
load_public_identity_files(const struct ssh_conn_info *cinfo)
{
	char *filename, *cp;
	struct sshkey *public;
	int i;
	u_int n_ids, n_certs;
	char *identity_files[SSH_MAX_IDENTITY_FILES];
	struct sshkey *identity_keys[SSH_MAX_IDENTITY_FILES];
	int identity_file_userprovided[SSH_MAX_IDENTITY_FILES];
	char *certificate_files[SSH_MAX_CERTIFICATE_FILES];
	struct sshkey *certificates[SSH_MAX_CERTIFICATE_FILES];
	int certificate_file_userprovided[SSH_MAX_CERTIFICATE_FILES];
#ifdef ENABLE_PKCS11
	struct sshkey **keys = NULL;
	char **comments = NULL;
	int nkeys;
#endif /* PKCS11 */

	n_ids = n_certs = 0;
	memset(identity_files, 0, sizeof(identity_files));
	memset(identity_keys, 0, sizeof(identity_keys));
	memset(identity_file_userprovided, 0,
	    sizeof(identity_file_userprovided));
	memset(certificate_files, 0, sizeof(certificate_files));
	memset(certificates, 0, sizeof(certificates));
	memset(certificate_file_userprovided, 0,
	    sizeof(certificate_file_userprovided));

#ifdef ENABLE_PKCS11
	if (options.pkcs11_provider != NULL &&
	    options.num_identity_files < SSH_MAX_IDENTITY_FILES &&
	    (pkcs11_init(!options.batch_mode) == 0) &&
	    (nkeys = pkcs11_add_provider(options.pkcs11_provider, NULL,
	    &keys, &comments)) > 0) {
		for (i = 0; i < nkeys; i++) {
			if (n_ids >= SSH_MAX_IDENTITY_FILES) {
				sshkey_free(keys[i]);
				free(comments[i]);
				continue;
			}
			identity_keys[n_ids] = keys[i];
			identity_files[n_ids] = comments[i]; /* transferred */
			n_ids++;
		}
		free(keys);
		free(comments);
	}
#endif /* ENABLE_PKCS11 */
	for (i = 0; i < options.num_identity_files; i++) {
		if (n_ids >= SSH_MAX_IDENTITY_FILES ||
		    strcasecmp(options.identity_files[i], "none") == 0) {
			free(options.identity_files[i]);
			options.identity_files[i] = NULL;
			continue;
		}
		cp = tilde_expand_filename(options.identity_files[i], getuid());
		filename = default_client_percent_dollar_expand(cp, cinfo);
		free(cp);
		check_load(sshkey_load_public(filename, &public, NULL),
		    filename, "pubkey");
		debug("identity file %s type %d", filename,
		    public ? public->type : -1);
		free(options.identity_files[i]);
		identity_files[n_ids] = filename;
		identity_keys[n_ids] = public;
		identity_file_userprovided[n_ids] =
		    options.identity_file_userprovided[i];
		if (++n_ids >= SSH_MAX_IDENTITY_FILES)
			continue;

		/*
		 * If no certificates have been explicitly listed then try
		 * to add the default certificate variant too.
		 */
		if (options.num_certificate_files != 0)
			continue;
		xasprintf(&cp, "%s-cert", filename);
		check_load(sshkey_load_public(cp, &public, NULL),
		    filename, "pubkey");
		debug("identity file %s type %d", cp,
		    public ? public->type : -1);
		if (public == NULL) {
			free(cp);
			continue;
		}
		if (!sshkey_is_cert(public)) {
			debug_f("key %s type %s is not a certificate",
			    cp, sshkey_type(public));
			sshkey_free(public);
			free(cp);
			continue;
		}
		/* NB. leave filename pointing to private key */
		identity_files[n_ids] = xstrdup(filename);
		identity_keys[n_ids] = public;
		identity_file_userprovided[n_ids] =
		    options.identity_file_userprovided[i];
		n_ids++;
	}

	if (options.num_certificate_files > SSH_MAX_CERTIFICATE_FILES)
		fatal_f("too many certificates");
	for (i = 0; i < options.num_certificate_files; i++) {
		cp = tilde_expand_filename(options.certificate_files[i],
		    getuid());
		filename = default_client_percent_dollar_expand(cp, cinfo);
		free(cp);

		check_load(sshkey_load_public(filename, &public, NULL),
		    filename, "certificate");
		debug("certificate file %s type %d", filename,
		    public ? public->type : -1);
		free(options.certificate_files[i]);
		options.certificate_files[i] = NULL;
		if (public == NULL) {
			free(filename);
			continue;
		}
		if (!sshkey_is_cert(public)) {
			debug_f("key %s type %s is not a certificate",
			    filename, sshkey_type(public));
			sshkey_free(public);
			free(filename);
			continue;
		}
		certificate_files[n_certs] = filename;
		certificates[n_certs] = public;
		certificate_file_userprovided[n_certs] =
		    options.certificate_file_userprovided[i];
		++n_certs;
	}

	options.num_identity_files = n_ids;
	memcpy(options.identity_files, identity_files, sizeof(identity_files));
	memcpy(options.identity_keys, identity_keys, sizeof(identity_keys));
	memcpy(options.identity_file_userprovided,
	    identity_file_userprovided, sizeof(identity_file_userprovided));

	options.num_certificate_files = n_certs;
	memcpy(options.certificate_files,
	    certificate_files, sizeof(certificate_files));
	memcpy(options.certificates, certificates, sizeof(certificates));
	memcpy(options.certificate_file_userprovided,
	    certificate_file_userprovided,
	    sizeof(certificate_file_userprovided));
}

static void
main_sigchld_handler(int sig)
{
	int save_errno = errno;
	pid_t pid;
	int status;

	while ((pid = waitpid(-1, &status, WNOHANG)) > 0 ||
	    (pid == -1 && errno == EINTR))
		;
	errno = save_errno;
}
