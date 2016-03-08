/* $OpenBSD: readconf.c,v 1.239 2015/07/30 00:01:34 djm Exp $ */
/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * Functions for reading the configuration files.
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 */

#include "includes.h"
__RCSID("$FreeBSD$");

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/wait.h>
#include <sys/un.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netdb.h>
#ifdef HAVE_PATHS_H
# include <paths.h>
#endif
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#ifdef HAVE_UTIL_H
#include <util.h>
#endif
#if defined(HAVE_STRNVIS) && defined(HAVE_VIS_H) && !defined(BROKEN_STRNVIS)
# include <vis.h>
#endif

#include "xmalloc.h"
#include "ssh.h"
#include "compat.h"
#include "cipher.h"
#include "pathnames.h"
#include "log.h"
#include "sshkey.h"
#include "misc.h"
#include "readconf.h"
#include "match.h"
#include "kex.h"
#include "mac.h"
#include "uidswap.h"
#include "myproposal.h"
#include "digest.h"
#include "version.h"

/* Format of the configuration file:

   # Configuration data is parsed as follows:
   #  1. command line options
   #  2. user-specific file
   #  3. system-wide file
   # Any configuration value is only changed the first time it is set.
   # Thus, host-specific definitions should be at the beginning of the
   # configuration file, and defaults at the end.

   # Host-specific declarations.  These may override anything above.  A single
   # host may match multiple declarations; these are processed in the order
   # that they are given in.

   Host *.ngs.fi ngs.fi
     User foo

   Host fake.com
     HostName another.host.name.real.org
     User blaah
     Port 34289
     ForwardX11 no
     ForwardAgent no

   Host books.com
     RemoteForward 9999 shadows.cs.hut.fi:9999
     Cipher 3des

   Host fascist.blob.com
     Port 23123
     User tylonen
     PasswordAuthentication no

   Host puukko.hut.fi
     User t35124p
     ProxyCommand ssh-proxy %h %p

   Host *.fr
     PublicKeyAuthentication no

   Host *.su
     Cipher none
     PasswordAuthentication no

   Host vpn.fake.com
     Tunnel yes
     TunnelDevice 3

   # Defaults for various options
   Host *
     ForwardAgent no
     ForwardX11 no
     PasswordAuthentication yes
     RSAAuthentication yes
     RhostsRSAAuthentication yes
     StrictHostKeyChecking yes
     TcpKeepAlive no
     IdentityFile ~/.ssh/identity
     Port 22
     EscapeChar ~

*/

/* Keyword tokens. */

typedef enum {
	oBadOption,
	oVersionAddendum,
	oHost, oMatch,
	oForwardAgent, oForwardX11, oForwardX11Trusted, oForwardX11Timeout,
	oGatewayPorts, oExitOnForwardFailure,
	oPasswordAuthentication, oRSAAuthentication,
	oChallengeResponseAuthentication, oXAuthLocation,
	oIdentityFile, oHostName, oPort, oCipher, oRemoteForward, oLocalForward,
	oUser, oEscapeChar, oRhostsRSAAuthentication, oProxyCommand,
	oGlobalKnownHostsFile, oUserKnownHostsFile, oConnectionAttempts,
	oBatchMode, oCheckHostIP, oStrictHostKeyChecking, oCompression,
	oCompressionLevel, oTCPKeepAlive, oNumberOfPasswordPrompts,
	oUsePrivilegedPort, oLogLevel, oCiphers, oProtocol, oMacs,
	oPubkeyAuthentication,
	oKbdInteractiveAuthentication, oKbdInteractiveDevices, oHostKeyAlias,
	oDynamicForward, oPreferredAuthentications, oHostbasedAuthentication,
	oHostKeyAlgorithms, oBindAddress, oPKCS11Provider,
	oClearAllForwardings, oNoHostAuthenticationForLocalhost,
	oEnableSSHKeysign, oRekeyLimit, oVerifyHostKeyDNS, oConnectTimeout,
	oAddressFamily, oGssAuthentication, oGssDelegateCreds,
	oServerAliveInterval, oServerAliveCountMax, oIdentitiesOnly,
	oSendEnv, oControlPath, oControlMaster, oControlPersist,
	oHashKnownHosts,
	oTunnel, oTunnelDevice, oLocalCommand, oPermitLocalCommand,
	oVisualHostKey, oUseRoaming,
	oKexAlgorithms, oIPQoS, oRequestTTY, oIgnoreUnknown, oProxyUseFdpass,
	oCanonicalDomains, oCanonicalizeHostname, oCanonicalizeMaxDots,
	oCanonicalizeFallbackLocal, oCanonicalizePermittedCNAMEs,
	oStreamLocalBindMask, oStreamLocalBindUnlink, oRevokedHostKeys,
	oFingerprintHash, oUpdateHostkeys, oHostbasedKeyTypes,
	oPubkeyAcceptedKeyTypes,
	oIgnoredUnknownOption, oDeprecated, oUnsupported
} OpCodes;

/* Textual representations of the tokens. */

static struct {
	const char *name;
	OpCodes opcode;
} keywords[] = {
	{ "forwardagent", oForwardAgent },
	{ "forwardx11", oForwardX11 },
	{ "forwardx11trusted", oForwardX11Trusted },
	{ "forwardx11timeout", oForwardX11Timeout },
	{ "exitonforwardfailure", oExitOnForwardFailure },
	{ "xauthlocation", oXAuthLocation },
	{ "gatewayports", oGatewayPorts },
	{ "useprivilegedport", oUsePrivilegedPort },
	{ "rhostsauthentication", oDeprecated },
	{ "passwordauthentication", oPasswordAuthentication },
	{ "kbdinteractiveauthentication", oKbdInteractiveAuthentication },
	{ "kbdinteractivedevices", oKbdInteractiveDevices },
	{ "rsaauthentication", oRSAAuthentication },
	{ "pubkeyauthentication", oPubkeyAuthentication },
	{ "dsaauthentication", oPubkeyAuthentication },		    /* alias */
	{ "rhostsrsaauthentication", oRhostsRSAAuthentication },
	{ "hostbasedauthentication", oHostbasedAuthentication },
	{ "challengeresponseauthentication", oChallengeResponseAuthentication },
	{ "skeyauthentication", oChallengeResponseAuthentication }, /* alias */
	{ "tisauthentication", oChallengeResponseAuthentication },  /* alias */
	{ "kerberosauthentication", oUnsupported },
	{ "kerberostgtpassing", oUnsupported },
	{ "afstokenpassing", oUnsupported },
#if defined(GSSAPI)
	{ "gssapiauthentication", oGssAuthentication },
	{ "gssapidelegatecredentials", oGssDelegateCreds },
#else
	{ "gssapiauthentication", oUnsupported },
	{ "gssapidelegatecredentials", oUnsupported },
#endif
	{ "fallbacktorsh", oDeprecated },
	{ "usersh", oDeprecated },
	{ "identityfile", oIdentityFile },
	{ "identityfile2", oIdentityFile },			/* obsolete */
	{ "identitiesonly", oIdentitiesOnly },
	{ "hostname", oHostName },
	{ "hostkeyalias", oHostKeyAlias },
	{ "proxycommand", oProxyCommand },
	{ "port", oPort },
	{ "cipher", oCipher },
	{ "ciphers", oCiphers },
	{ "macs", oMacs },
	{ "protocol", oProtocol },
	{ "remoteforward", oRemoteForward },
	{ "localforward", oLocalForward },
	{ "user", oUser },
	{ "host", oHost },
	{ "match", oMatch },
	{ "escapechar", oEscapeChar },
	{ "globalknownhostsfile", oGlobalKnownHostsFile },
	{ "globalknownhostsfile2", oDeprecated },
	{ "userknownhostsfile", oUserKnownHostsFile },
	{ "userknownhostsfile2", oDeprecated },
	{ "connectionattempts", oConnectionAttempts },
	{ "batchmode", oBatchMode },
	{ "checkhostip", oCheckHostIP },
	{ "stricthostkeychecking", oStrictHostKeyChecking },
	{ "compression", oCompression },
	{ "compressionlevel", oCompressionLevel },
	{ "tcpkeepalive", oTCPKeepAlive },
	{ "keepalive", oTCPKeepAlive },				/* obsolete */
	{ "numberofpasswordprompts", oNumberOfPasswordPrompts },
	{ "loglevel", oLogLevel },
	{ "dynamicforward", oDynamicForward },
	{ "preferredauthentications", oPreferredAuthentications },
	{ "hostkeyalgorithms", oHostKeyAlgorithms },
	{ "bindaddress", oBindAddress },
#ifdef ENABLE_PKCS11
	{ "smartcarddevice", oPKCS11Provider },
	{ "pkcs11provider", oPKCS11Provider },
#else
	{ "smartcarddevice", oUnsupported },
	{ "pkcs11provider", oUnsupported },
#endif
	{ "clearallforwardings", oClearAllForwardings },
	{ "enablesshkeysign", oEnableSSHKeysign },
	{ "verifyhostkeydns", oVerifyHostKeyDNS },
	{ "nohostauthenticationforlocalhost", oNoHostAuthenticationForLocalhost },
	{ "rekeylimit", oRekeyLimit },
	{ "connecttimeout", oConnectTimeout },
	{ "addressfamily", oAddressFamily },
	{ "serveraliveinterval", oServerAliveInterval },
	{ "serveralivecountmax", oServerAliveCountMax },
	{ "sendenv", oSendEnv },
	{ "controlpath", oControlPath },
	{ "controlmaster", oControlMaster },
	{ "controlpersist", oControlPersist },
	{ "hashknownhosts", oHashKnownHosts },
	{ "tunnel", oTunnel },
	{ "tunneldevice", oTunnelDevice },
	{ "localcommand", oLocalCommand },
	{ "permitlocalcommand", oPermitLocalCommand },
	{ "visualhostkey", oVisualHostKey },
	{ "useroaming", oUseRoaming },
	{ "kexalgorithms", oKexAlgorithms },
	{ "ipqos", oIPQoS },
	{ "requesttty", oRequestTTY },
	{ "proxyusefdpass", oProxyUseFdpass },
	{ "canonicaldomains", oCanonicalDomains },
	{ "canonicalizefallbacklocal", oCanonicalizeFallbackLocal },
	{ "canonicalizehostname", oCanonicalizeHostname },
	{ "canonicalizemaxdots", oCanonicalizeMaxDots },
	{ "canonicalizepermittedcnames", oCanonicalizePermittedCNAMEs },
	{ "streamlocalbindmask", oStreamLocalBindMask },
	{ "streamlocalbindunlink", oStreamLocalBindUnlink },
	{ "revokedhostkeys", oRevokedHostKeys },
	{ "fingerprinthash", oFingerprintHash },
	{ "updatehostkeys", oUpdateHostkeys },
	{ "hostbasedkeytypes", oHostbasedKeyTypes },
	{ "pubkeyacceptedkeytypes", oPubkeyAcceptedKeyTypes },
	{ "ignoreunknown", oIgnoreUnknown },
	{ "hpndisabled", oDeprecated },
	{ "hpnbuffersize", oDeprecated },
	{ "tcprcvbufpoll", oDeprecated },
	{ "tcprcvbuf", oDeprecated },
	{ "noneenabled", oUnsupported },
	{ "noneswitch", oUnsupported },
	{ "versionaddendum", oVersionAddendum },

	{ NULL, oBadOption }
};

/*
 * Adds a local TCP/IP port forward to options.  Never returns if there is an
 * error.
 */

void
add_local_forward(Options *options, const struct Forward *newfwd)
{
	struct Forward *fwd;
#ifndef NO_IPPORT_RESERVED_CONCEPT
	extern uid_t original_real_uid;
	int ipport_reserved;
#ifdef __FreeBSD__
	size_t len_ipport_reserved = sizeof(ipport_reserved);

	if (sysctlbyname("net.inet.ip.portrange.reservedhigh",
	    &ipport_reserved, &len_ipport_reserved, NULL, 0) != 0)
		ipport_reserved = IPPORT_RESERVED;
	else
		ipport_reserved++;
#else
	ipport_reserved = IPPORT_RESERVED;
#endif
	if (newfwd->listen_port < ipport_reserved && original_real_uid != 0)
	if (newfwd->listen_port < ipport_reserved && original_real_uid != 0 &&
	    newfwd->listen_path == NULL)
		fatal("Privileged ports can only be forwarded by root.");
#endif
	options->local_forwards = xreallocarray(options->local_forwards,
	    options->num_local_forwards + 1,
	    sizeof(*options->local_forwards));
	fwd = &options->local_forwards[options->num_local_forwards++];

	fwd->listen_host = newfwd->listen_host;
	fwd->listen_port = newfwd->listen_port;
	fwd->listen_path = newfwd->listen_path;
	fwd->connect_host = newfwd->connect_host;
	fwd->connect_port = newfwd->connect_port;
	fwd->connect_path = newfwd->connect_path;
}

/*
 * Adds a remote TCP/IP port forward to options.  Never returns if there is
 * an error.
 */

void
add_remote_forward(Options *options, const struct Forward *newfwd)
{
	struct Forward *fwd;

	options->remote_forwards = xreallocarray(options->remote_forwards,
	    options->num_remote_forwards + 1,
	    sizeof(*options->remote_forwards));
	fwd = &options->remote_forwards[options->num_remote_forwards++];

	fwd->listen_host = newfwd->listen_host;
	fwd->listen_port = newfwd->listen_port;
	fwd->listen_path = newfwd->listen_path;
	fwd->connect_host = newfwd->connect_host;
	fwd->connect_port = newfwd->connect_port;
	fwd->connect_path = newfwd->connect_path;
	fwd->handle = newfwd->handle;
	fwd->allocated_port = 0;
}

static void
clear_forwardings(Options *options)
{
	int i;

	for (i = 0; i < options->num_local_forwards; i++) {
		free(options->local_forwards[i].listen_host);
		free(options->local_forwards[i].listen_path);
		free(options->local_forwards[i].connect_host);
		free(options->local_forwards[i].connect_path);
	}
	if (options->num_local_forwards > 0) {
		free(options->local_forwards);
		options->local_forwards = NULL;
	}
	options->num_local_forwards = 0;
	for (i = 0; i < options->num_remote_forwards; i++) {
		free(options->remote_forwards[i].listen_host);
		free(options->remote_forwards[i].listen_path);
		free(options->remote_forwards[i].connect_host);
		free(options->remote_forwards[i].connect_path);
	}
	if (options->num_remote_forwards > 0) {
		free(options->remote_forwards);
		options->remote_forwards = NULL;
	}
	options->num_remote_forwards = 0;
	options->tun_open = SSH_TUNMODE_NO;
}

void
add_identity_file(Options *options, const char *dir, const char *filename,
    int userprovided)
{
	char *path;
	int i;

	if (options->num_identity_files >= SSH_MAX_IDENTITY_FILES)
		fatal("Too many identity files specified (max %d)",
		    SSH_MAX_IDENTITY_FILES);

	if (dir == NULL) /* no dir, filename is absolute */
		path = xstrdup(filename);
	else
		(void)xasprintf(&path, "%.100s%.100s", dir, filename);

	/* Avoid registering duplicates */
	for (i = 0; i < options->num_identity_files; i++) {
		if (options->identity_file_userprovided[i] == userprovided &&
		    strcmp(options->identity_files[i], path) == 0) {
			debug2("%s: ignoring duplicate key %s", __func__, path);
			free(path);
			return;
		}
	}

	options->identity_file_userprovided[options->num_identity_files] =
	    userprovided;
	options->identity_files[options->num_identity_files++] = path;
}

int
default_ssh_port(void)
{
	static int port;
	struct servent *sp;

	if (port == 0) {
		sp = getservbyname(SSH_SERVICE_NAME, "tcp");
		port = sp ? ntohs(sp->s_port) : SSH_DEFAULT_PORT;
	}
	return port;
}

/*
 * Execute a command in a shell.
 * Return its exit status or -1 on abnormal exit.
 */
static int
execute_in_shell(const char *cmd)
{
	char *shell, *command_string;
	pid_t pid;
	int devnull, status;
	extern uid_t original_real_uid;

	if ((shell = getenv("SHELL")) == NULL)
		shell = _PATH_BSHELL;

	/*
	 * Use "exec" to avoid "sh -c" processes on some platforms
	 * (e.g. Solaris)
	 */
	xasprintf(&command_string, "exec %s", cmd);

	/* Need this to redirect subprocess stdin/out */
	if ((devnull = open(_PATH_DEVNULL, O_RDWR)) == -1)
		fatal("open(/dev/null): %s", strerror(errno));

	debug("Executing command: '%.500s'", cmd);

	/* Fork and execute the command. */
	if ((pid = fork()) == 0) {
		char *argv[4];

		/* Child.  Permanently give up superuser privileges. */
		permanently_drop_suid(original_real_uid);

		/* Redirect child stdin and stdout. Leave stderr */
		if (dup2(devnull, STDIN_FILENO) == -1)
			fatal("dup2: %s", strerror(errno));
		if (dup2(devnull, STDOUT_FILENO) == -1)
			fatal("dup2: %s", strerror(errno));
		if (devnull > STDERR_FILENO)
			close(devnull);
		closefrom(STDERR_FILENO + 1);

		argv[0] = shell;
		argv[1] = "-c";
		argv[2] = command_string;
		argv[3] = NULL;

		execv(argv[0], argv);
		error("Unable to execute '%.100s': %s", cmd, strerror(errno));
		/* Die with signal to make this error apparent to parent. */
		signal(SIGTERM, SIG_DFL);
		kill(getpid(), SIGTERM);
		_exit(1);
	}
	/* Parent. */
	if (pid < 0)
		fatal("%s: fork: %.100s", __func__, strerror(errno));

	close(devnull);
	free(command_string);

	while (waitpid(pid, &status, 0) == -1) {
		if (errno != EINTR && errno != EAGAIN)
			fatal("%s: waitpid: %s", __func__, strerror(errno));
	}
	if (!WIFEXITED(status)) {
		error("command '%.100s' exited abnormally", cmd);
		return -1;
	}
	debug3("command returned status %d", WEXITSTATUS(status));
	return WEXITSTATUS(status);
}

/*
 * Parse and execute a Match directive.
 */
static int
match_cfg_line(Options *options, char **condition, struct passwd *pw,
    const char *host_arg, const char *original_host, int post_canon,
    const char *filename, int linenum)
{
	char *arg, *oattrib, *attrib, *cmd, *cp = *condition, *host, *criteria;
	const char *ruser;
	int r, port, this_result, result = 1, attributes = 0, negate;
	char thishost[NI_MAXHOST], shorthost[NI_MAXHOST], portstr[NI_MAXSERV];

	/*
	 * Configuration is likely to be incomplete at this point so we
	 * must be prepared to use default values.
	 */
	port = options->port <= 0 ? default_ssh_port() : options->port;
	ruser = options->user == NULL ? pw->pw_name : options->user;
	if (options->hostname != NULL) {
		/* NB. Please keep in sync with ssh.c:main() */
		host = percent_expand(options->hostname,
		    "h", host_arg, (char *)NULL);
	} else
		host = xstrdup(host_arg);

	debug2("checking match for '%s' host %s originally %s",
	    cp, host, original_host);
	while ((oattrib = attrib = strdelim(&cp)) && *attrib != '\0') {
		criteria = NULL;
		this_result = 1;
		if ((negate = attrib[0] == '!'))
			attrib++;
		/* criteria "all" and "canonical" have no argument */
		if (strcasecmp(attrib, "all") == 0) {
			if (attributes > 1 ||
			    ((arg = strdelim(&cp)) != NULL && *arg != '\0')) {
				error("%.200s line %d: '%s' cannot be combined "
				    "with other Match attributes",
				    filename, linenum, oattrib);
				result = -1;
				goto out;
			}
			if (result)
				result = negate ? 0 : 1;
			goto out;
		}
		attributes++;
		if (strcasecmp(attrib, "canonical") == 0) {
			r = !!post_canon;  /* force bitmask member to boolean */
			if (r == (negate ? 1 : 0))
				this_result = result = 0;
			debug3("%.200s line %d: %smatched '%s'",
			    filename, linenum,
			    this_result ? "" : "not ", oattrib);
			continue;
		}
		/* All other criteria require an argument */
		if ((arg = strdelim(&cp)) == NULL || *arg == '\0') {
			error("Missing Match criteria for %s", attrib);
			result = -1;
			goto out;
		}
		if (strcasecmp(attrib, "host") == 0) {
			criteria = xstrdup(host);
			r = match_hostname(host, arg) == 1;
			if (r == (negate ? 1 : 0))
				this_result = result = 0;
		} else if (strcasecmp(attrib, "originalhost") == 0) {
			criteria = xstrdup(original_host);
			r = match_hostname(original_host, arg) == 1;
			if (r == (negate ? 1 : 0))
				this_result = result = 0;
		} else if (strcasecmp(attrib, "user") == 0) {
			criteria = xstrdup(ruser);
			r = match_pattern_list(ruser, arg, 0) == 1;
			if (r == (negate ? 1 : 0))
				this_result = result = 0;
		} else if (strcasecmp(attrib, "localuser") == 0) {
			criteria = xstrdup(pw->pw_name);
			r = match_pattern_list(pw->pw_name, arg, 0) == 1;
			if (r == (negate ? 1 : 0))
				this_result = result = 0;
		} else if (strcasecmp(attrib, "exec") == 0) {
			if (gethostname(thishost, sizeof(thishost)) == -1)
				fatal("gethostname: %s", strerror(errno));
			strlcpy(shorthost, thishost, sizeof(shorthost));
			shorthost[strcspn(thishost, ".")] = '\0';
			snprintf(portstr, sizeof(portstr), "%d", port);

			cmd = percent_expand(arg,
			    "L", shorthost,
			    "d", pw->pw_dir,
			    "h", host,
			    "l", thishost,
			    "n", original_host,
			    "p", portstr,
			    "r", ruser,
			    "u", pw->pw_name,
			    (char *)NULL);
			if (result != 1) {
				/* skip execution if prior predicate failed */
				debug3("%.200s line %d: skipped exec "
				    "\"%.100s\"", filename, linenum, cmd);
				free(cmd);
				continue;
			}
			r = execute_in_shell(cmd);
			if (r == -1) {
				fatal("%.200s line %d: match exec "
				    "'%.100s' error", filename,
				    linenum, cmd);
			}
			criteria = xstrdup(cmd);
			free(cmd);
			/* Force exit status to boolean */
			r = r == 0;
			if (r == (negate ? 1 : 0))
				this_result = result = 0;
		} else {
			error("Unsupported Match attribute %s", attrib);
			result = -1;
			goto out;
		}
		debug3("%.200s line %d: %smatched '%s \"%.100s\"' ",
		    filename, linenum, this_result ? "": "not ",
		    oattrib, criteria);
		free(criteria);
	}
	if (attributes == 0) {
		error("One or more attributes required for Match");
		result = -1;
		goto out;
	}
 out:
	if (result != -1)
		debug2("match %sfound", result ? "" : "not ");
	*condition = cp;
	free(host);
	return result;
}

/* Check and prepare a domain name: removes trailing '.' and lowercases */
static void
valid_domain(char *name, const char *filename, int linenum)
{
	size_t i, l = strlen(name);
	u_char c, last = '\0';

	if (l == 0)
		fatal("%s line %d: empty hostname suffix", filename, linenum);
	if (!isalpha((u_char)name[0]) && !isdigit((u_char)name[0]))
		fatal("%s line %d: hostname suffix \"%.100s\" "
		    "starts with invalid character", filename, linenum, name);
	for (i = 0; i < l; i++) {
		c = tolower((u_char)name[i]);
		name[i] = (char)c;
		if (last == '.' && c == '.')
			fatal("%s line %d: hostname suffix \"%.100s\" contains "
			    "consecutive separators", filename, linenum, name);
		if (c != '.' && c != '-' && !isalnum(c) &&
		    c != '_') /* technically invalid, but common */
			fatal("%s line %d: hostname suffix \"%.100s\" contains "
			    "invalid characters", filename, linenum, name);
		last = c;
	}
	if (name[l - 1] == '.')
		name[l - 1] = '\0';
}

/*
 * Returns the number of the token pointed to by cp or oBadOption.
 */
static OpCodes
parse_token(const char *cp, const char *filename, int linenum,
    const char *ignored_unknown)
{
	int i;

	for (i = 0; keywords[i].name; i++)
		if (strcmp(cp, keywords[i].name) == 0)
			return keywords[i].opcode;
	if (ignored_unknown != NULL &&
	    match_pattern_list(cp, ignored_unknown, 1) == 1)
		return oIgnoredUnknownOption;
	error("%s: line %d: Bad configuration option: %s",
	    filename, linenum, cp);
	return oBadOption;
}

/* Multistate option parsing */
struct multistate {
	char *key;
	int value;
};
static const struct multistate multistate_flag[] = {
	{ "true",			1 },
	{ "false",			0 },
	{ "yes",			1 },
	{ "no",				0 },
	{ NULL, -1 }
};
static const struct multistate multistate_yesnoask[] = {
	{ "true",			1 },
	{ "false",			0 },
	{ "yes",			1 },
	{ "no",				0 },
	{ "ask",			2 },
	{ NULL, -1 }
};
static const struct multistate multistate_addressfamily[] = {
	{ "inet",			AF_INET },
	{ "inet6",			AF_INET6 },
	{ "any",			AF_UNSPEC },
	{ NULL, -1 }
};
static const struct multistate multistate_controlmaster[] = {
	{ "true",			SSHCTL_MASTER_YES },
	{ "yes",			SSHCTL_MASTER_YES },
	{ "false",			SSHCTL_MASTER_NO },
	{ "no",				SSHCTL_MASTER_NO },
	{ "auto",			SSHCTL_MASTER_AUTO },
	{ "ask",			SSHCTL_MASTER_ASK },
	{ "autoask",			SSHCTL_MASTER_AUTO_ASK },
	{ NULL, -1 }
};
static const struct multistate multistate_tunnel[] = {
	{ "ethernet",			SSH_TUNMODE_ETHERNET },
	{ "point-to-point",		SSH_TUNMODE_POINTOPOINT },
	{ "true",			SSH_TUNMODE_DEFAULT },
	{ "yes",			SSH_TUNMODE_DEFAULT },
	{ "false",			SSH_TUNMODE_NO },
	{ "no",				SSH_TUNMODE_NO },
	{ NULL, -1 }
};
static const struct multistate multistate_requesttty[] = {
	{ "true",			REQUEST_TTY_YES },
	{ "yes",			REQUEST_TTY_YES },
	{ "false",			REQUEST_TTY_NO },
	{ "no",				REQUEST_TTY_NO },
	{ "force",			REQUEST_TTY_FORCE },
	{ "auto",			REQUEST_TTY_AUTO },
	{ NULL, -1 }
};
static const struct multistate multistate_canonicalizehostname[] = {
	{ "true",			SSH_CANONICALISE_YES },
	{ "false",			SSH_CANONICALISE_NO },
	{ "yes",			SSH_CANONICALISE_YES },
	{ "no",				SSH_CANONICALISE_NO },
	{ "always",			SSH_CANONICALISE_ALWAYS },
	{ NULL, -1 }
};

/*
 * Processes a single option line as used in the configuration files. This
 * only sets those values that have not already been set.
 */
#define WHITESPACE " \t\r\n"
int
process_config_line(Options *options, struct passwd *pw, const char *host,
    const char *original_host, char *line, const char *filename,
    int linenum, int *activep, int flags)
{
	char *s, **charptr, *endofnumber, *keyword, *arg, *arg2;
	char **cpptr, fwdarg[256];
	u_int i, *uintptr, max_entries = 0;
	int negated, opcode, *intptr, value, value2, cmdline = 0;
	LogLevel *log_level_ptr;
	long long val64;
	size_t len;
	struct Forward fwd;
	const struct multistate *multistate_ptr;
	struct allowed_cname *cname;

	if (activep == NULL) { /* We are processing a command line directive */
		cmdline = 1;
		activep = &cmdline;
	}

	/* Strip trailing whitespace */
	if ((len = strlen(line)) == 0)
		return 0;
	for (len--; len > 0; len--) {
		if (strchr(WHITESPACE, line[len]) == NULL)
			break;
		line[len] = '\0';
	}

	s = line;
	/* Get the keyword. (Each line is supposed to begin with a keyword). */
	if ((keyword = strdelim(&s)) == NULL)
		return 0;
	/* Ignore leading whitespace. */
	if (*keyword == '\0')
		keyword = strdelim(&s);
	if (keyword == NULL || !*keyword || *keyword == '\n' || *keyword == '#')
		return 0;
	/* Match lowercase keyword */
	lowercase(keyword);

	opcode = parse_token(keyword, filename, linenum,
	    options->ignored_unknown);

	switch (opcode) {
	case oBadOption:
		/* don't panic, but count bad options */
		return -1;
		/* NOTREACHED */
	case oIgnoredUnknownOption:
		debug("%s line %d: Ignored unknown option \"%s\"",
		    filename, linenum, keyword);
		return 0;
	case oConnectTimeout:
		intptr = &options->connection_timeout;
parse_time:
		arg = strdelim(&s);
		if (!arg || *arg == '\0')
			fatal("%s line %d: missing time value.",
			    filename, linenum);
		if (strcmp(arg, "none") == 0)
			value = -1;
		else if ((value = convtime(arg)) == -1)
			fatal("%s line %d: invalid time value.",
			    filename, linenum);
		if (*activep && *intptr == -1)
			*intptr = value;
		break;

	case oForwardAgent:
		intptr = &options->forward_agent;
 parse_flag:
		multistate_ptr = multistate_flag;
 parse_multistate:
		arg = strdelim(&s);
		if (!arg || *arg == '\0')
			fatal("%s line %d: missing argument.",
			    filename, linenum);
		value = -1;
		for (i = 0; multistate_ptr[i].key != NULL; i++) {
			if (strcasecmp(arg, multistate_ptr[i].key) == 0) {
				value = multistate_ptr[i].value;
				break;
			}
		}
		if (value == -1)
			fatal("%s line %d: unsupported option \"%s\".",
			    filename, linenum, arg);
		if (*activep && *intptr == -1)
			*intptr = value;
		break;

	case oForwardX11:
		intptr = &options->forward_x11;
		goto parse_flag;

	case oForwardX11Trusted:
		intptr = &options->forward_x11_trusted;
		goto parse_flag;

	case oForwardX11Timeout:
		intptr = &options->forward_x11_timeout;
		goto parse_time;

	case oGatewayPorts:
		intptr = &options->fwd_opts.gateway_ports;
		goto parse_flag;

	case oExitOnForwardFailure:
		intptr = &options->exit_on_forward_failure;
		goto parse_flag;

	case oUsePrivilegedPort:
		intptr = &options->use_privileged_port;
		goto parse_flag;

	case oPasswordAuthentication:
		intptr = &options->password_authentication;
		goto parse_flag;

	case oKbdInteractiveAuthentication:
		intptr = &options->kbd_interactive_authentication;
		goto parse_flag;

	case oKbdInteractiveDevices:
		charptr = &options->kbd_interactive_devices;
		goto parse_string;

	case oPubkeyAuthentication:
		intptr = &options->pubkey_authentication;
		goto parse_flag;

	case oRSAAuthentication:
		intptr = &options->rsa_authentication;
		goto parse_flag;

	case oRhostsRSAAuthentication:
		intptr = &options->rhosts_rsa_authentication;
		goto parse_flag;

	case oHostbasedAuthentication:
		intptr = &options->hostbased_authentication;
		goto parse_flag;

	case oChallengeResponseAuthentication:
		intptr = &options->challenge_response_authentication;
		goto parse_flag;

	case oGssAuthentication:
		intptr = &options->gss_authentication;
		goto parse_flag;

	case oGssDelegateCreds:
		intptr = &options->gss_deleg_creds;
		goto parse_flag;

	case oBatchMode:
		intptr = &options->batch_mode;
		goto parse_flag;

	case oCheckHostIP:
		intptr = &options->check_host_ip;
		goto parse_flag;

	case oVerifyHostKeyDNS:
		intptr = &options->verify_host_key_dns;
		multistate_ptr = multistate_yesnoask;
		goto parse_multistate;

	case oStrictHostKeyChecking:
		intptr = &options->strict_host_key_checking;
		multistate_ptr = multistate_yesnoask;
		goto parse_multistate;

	case oCompression:
		intptr = &options->compression;
		goto parse_flag;

	case oTCPKeepAlive:
		intptr = &options->tcp_keep_alive;
		goto parse_flag;

	case oNoHostAuthenticationForLocalhost:
		intptr = &options->no_host_authentication_for_localhost;
		goto parse_flag;

	case oNumberOfPasswordPrompts:
		intptr = &options->number_of_password_prompts;
		goto parse_int;

	case oCompressionLevel:
		intptr = &options->compression_level;
		goto parse_int;

	case oRekeyLimit:
		arg = strdelim(&s);
		if (!arg || *arg == '\0')
			fatal("%.200s line %d: Missing argument.", filename,
			    linenum);
		if (strcmp(arg, "default") == 0) {
			val64 = 0;
		} else {
			if (scan_scaled(arg, &val64) == -1)
				fatal("%.200s line %d: Bad number '%s': %s",
				    filename, linenum, arg, strerror(errno));
			/* check for too-large or too-small limits */
			if (val64 > UINT_MAX)
				fatal("%.200s line %d: RekeyLimit too large",
				    filename, linenum);
			if (val64 != 0 && val64 < 16)
				fatal("%.200s line %d: RekeyLimit too small",
				    filename, linenum);
		}
		if (*activep && options->rekey_limit == -1)
			options->rekey_limit = (u_int32_t)val64;
		if (s != NULL) { /* optional rekey interval present */
			if (strcmp(s, "none") == 0) {
				(void)strdelim(&s);	/* discard */
				break;
			}
			intptr = &options->rekey_interval;
			goto parse_time;
		}
		break;

	case oIdentityFile:
		arg = strdelim(&s);
		if (!arg || *arg == '\0')
			fatal("%.200s line %d: Missing argument.", filename, linenum);
		if (*activep) {
			intptr = &options->num_identity_files;
			if (*intptr >= SSH_MAX_IDENTITY_FILES)
				fatal("%.200s line %d: Too many identity files specified (max %d).",
				    filename, linenum, SSH_MAX_IDENTITY_FILES);
			add_identity_file(options, NULL,
			    arg, flags & SSHCONF_USERCONF);
		}
		break;

	case oXAuthLocation:
		charptr=&options->xauth_location;
		goto parse_string;

	case oUser:
		charptr = &options->user;
parse_string:
		arg = strdelim(&s);
		if (!arg || *arg == '\0')
			fatal("%.200s line %d: Missing argument.",
			    filename, linenum);
		if (*activep && *charptr == NULL)
			*charptr = xstrdup(arg);
		break;

	case oGlobalKnownHostsFile:
		cpptr = (char **)&options->system_hostfiles;
		uintptr = &options->num_system_hostfiles;
		max_entries = SSH_MAX_HOSTS_FILES;
parse_char_array:
		if (*activep && *uintptr == 0) {
			while ((arg = strdelim(&s)) != NULL && *arg != '\0') {
				if ((*uintptr) >= max_entries)
					fatal("%s line %d: "
					    "too many authorized keys files.",
					    filename, linenum);
				cpptr[(*uintptr)++] = xstrdup(arg);
			}
		}
		return 0;

	case oUserKnownHostsFile:
		cpptr = (char **)&options->user_hostfiles;
		uintptr = &options->num_user_hostfiles;
		max_entries = SSH_MAX_HOSTS_FILES;
		goto parse_char_array;

	case oHostName:
		charptr = &options->hostname;
		goto parse_string;

	case oHostKeyAlias:
		charptr = &options->host_key_alias;
		goto parse_string;

	case oPreferredAuthentications:
		charptr = &options->preferred_authentications;
		goto parse_string;

	case oBindAddress:
		charptr = &options->bind_address;
		goto parse_string;

	case oPKCS11Provider:
		charptr = &options->pkcs11_provider;
		goto parse_string;

	case oProxyCommand:
		charptr = &options->proxy_command;
parse_command:
		if (s == NULL)
			fatal("%.200s line %d: Missing argument.", filename, linenum);
		len = strspn(s, WHITESPACE "=");
		if (*activep && *charptr == NULL)
			*charptr = xstrdup(s + len);
		return 0;

	case oPort:
		intptr = &options->port;
parse_int:
		arg = strdelim(&s);
		if (!arg || *arg == '\0')
			fatal("%.200s line %d: Missing argument.", filename, linenum);
		if (arg[0] < '0' || arg[0] > '9')
			fatal("%.200s line %d: Bad number.", filename, linenum);

		/* Octal, decimal, or hex format? */
		value = strtol(arg, &endofnumber, 0);
		if (arg == endofnumber)
			fatal("%.200s line %d: Bad number.", filename, linenum);
		if (*activep && *intptr == -1)
			*intptr = value;
		break;

	case oConnectionAttempts:
		intptr = &options->connection_attempts;
		goto parse_int;

	case oCipher:
		intptr = &options->cipher;
		arg = strdelim(&s);
		if (!arg || *arg == '\0')
			fatal("%.200s line %d: Missing argument.", filename, linenum);
		value = cipher_number(arg);
		if (value == -1)
			fatal("%.200s line %d: Bad cipher '%s'.",
			    filename, linenum, arg ? arg : "<NONE>");
		if (*activep && *intptr == -1)
			*intptr = value;
		break;

	case oCiphers:
		arg = strdelim(&s);
		if (!arg || *arg == '\0')
			fatal("%.200s line %d: Missing argument.", filename, linenum);
		if (!ciphers_valid(*arg == '+' ? arg + 1 : arg))
			fatal("%.200s line %d: Bad SSH2 cipher spec '%s'.",
			    filename, linenum, arg ? arg : "<NONE>");
		if (*activep && options->ciphers == NULL)
			options->ciphers = xstrdup(arg);
		break;

	case oMacs:
		arg = strdelim(&s);
		if (!arg || *arg == '\0')
			fatal("%.200s line %d: Missing argument.", filename, linenum);
		if (!mac_valid(*arg == '+' ? arg + 1 : arg))
			fatal("%.200s line %d: Bad SSH2 Mac spec '%s'.",
			    filename, linenum, arg ? arg : "<NONE>");
		if (*activep && options->macs == NULL)
			options->macs = xstrdup(arg);
		break;

	case oKexAlgorithms:
		arg = strdelim(&s);
		if (!arg || *arg == '\0')
			fatal("%.200s line %d: Missing argument.",
			    filename, linenum);
		if (!kex_names_valid(*arg == '+' ? arg + 1 : arg))
			fatal("%.200s line %d: Bad SSH2 KexAlgorithms '%s'.",
			    filename, linenum, arg ? arg : "<NONE>");
		if (*activep && options->kex_algorithms == NULL)
			options->kex_algorithms = xstrdup(arg);
		break;

	case oHostKeyAlgorithms:
		charptr = &options->hostkeyalgorithms;
parse_keytypes:
		arg = strdelim(&s);
		if (!arg || *arg == '\0')
			fatal("%.200s line %d: Missing argument.",
			    filename, linenum);
		if (!sshkey_names_valid2(*arg == '+' ? arg + 1 : arg, 1))
			fatal("%s line %d: Bad key types '%s'.",
				filename, linenum, arg ? arg : "<NONE>");
		if (*activep && *charptr == NULL)
			*charptr = xstrdup(arg);
		break;

	case oProtocol:
		intptr = &options->protocol;
		arg = strdelim(&s);
		if (!arg || *arg == '\0')
			fatal("%.200s line %d: Missing argument.", filename, linenum);
		value = proto_spec(arg);
		if (value == SSH_PROTO_UNKNOWN)
			fatal("%.200s line %d: Bad protocol spec '%s'.",
			    filename, linenum, arg ? arg : "<NONE>");
		if (*activep && *intptr == SSH_PROTO_UNKNOWN)
			*intptr = value;
		break;

	case oLogLevel:
		log_level_ptr = &options->log_level;
		arg = strdelim(&s);
		value = log_level_number(arg);
		if (value == SYSLOG_LEVEL_NOT_SET)
			fatal("%.200s line %d: unsupported log level '%s'",
			    filename, linenum, arg ? arg : "<NONE>");
		if (*activep && *log_level_ptr == SYSLOG_LEVEL_NOT_SET)
			*log_level_ptr = (LogLevel) value;
		break;

	case oLocalForward:
	case oRemoteForward:
	case oDynamicForward:
		arg = strdelim(&s);
		if (arg == NULL || *arg == '\0')
			fatal("%.200s line %d: Missing port argument.",
			    filename, linenum);

		if (opcode == oLocalForward ||
		    opcode == oRemoteForward) {
			arg2 = strdelim(&s);
			if (arg2 == NULL || *arg2 == '\0')
				fatal("%.200s line %d: Missing target argument.",
				    filename, linenum);

			/* construct a string for parse_forward */
			snprintf(fwdarg, sizeof(fwdarg), "%s:%s", arg, arg2);
		} else if (opcode == oDynamicForward) {
			strlcpy(fwdarg, arg, sizeof(fwdarg));
		}

		if (parse_forward(&fwd, fwdarg,
		    opcode == oDynamicForward ? 1 : 0,
		    opcode == oRemoteForward ? 1 : 0) == 0)
			fatal("%.200s line %d: Bad forwarding specification.",
			    filename, linenum);

		if (*activep) {
			if (opcode == oLocalForward ||
			    opcode == oDynamicForward)
				add_local_forward(options, &fwd);
			else if (opcode == oRemoteForward)
				add_remote_forward(options, &fwd);
		}
		break;

	case oClearAllForwardings:
		intptr = &options->clear_forwardings;
		goto parse_flag;

	case oHost:
		if (cmdline)
			fatal("Host directive not supported as a command-line "
			    "option");
		*activep = 0;
		arg2 = NULL;
		while ((arg = strdelim(&s)) != NULL && *arg != '\0') {
			negated = *arg == '!';
			if (negated)
				arg++;
			if (match_pattern(host, arg)) {
				if (negated) {
					debug("%.200s line %d: Skipping Host "
					    "block because of negated match "
					    "for %.100s", filename, linenum,
					    arg);
					*activep = 0;
					break;
				}
				if (!*activep)
					arg2 = arg; /* logged below */
				*activep = 1;
			}
		}
		if (*activep)
			debug("%.200s line %d: Applying options for %.100s",
			    filename, linenum, arg2);
		/* Avoid garbage check below, as strdelim is done. */
		return 0;

	case oMatch:
		if (cmdline)
			fatal("Host directive not supported as a command-line "
			    "option");
		value = match_cfg_line(options, &s, pw, host, original_host,
		    flags & SSHCONF_POSTCANON, filename, linenum);
		if (value < 0)
			fatal("%.200s line %d: Bad Match condition", filename,
			    linenum);
		*activep = value;
		break;

	case oEscapeChar:
		intptr = &options->escape_char;
		arg = strdelim(&s);
		if (!arg || *arg == '\0')
			fatal("%.200s line %d: Missing argument.", filename, linenum);
		if (strcmp(arg, "none") == 0)
			value = SSH_ESCAPECHAR_NONE;
		else if (arg[1] == '\0')
			value = (u_char) arg[0];
		else if (arg[0] == '^' && arg[2] == 0 &&
		    (u_char) arg[1] >= 64 && (u_char) arg[1] < 128)
			value = (u_char) arg[1] & 31;
		else {
			fatal("%.200s line %d: Bad escape character.",
			    filename, linenum);
			/* NOTREACHED */
			value = 0;	/* Avoid compiler warning. */
		}
		if (*activep && *intptr == -1)
			*intptr = value;
		break;

	case oAddressFamily:
		intptr = &options->address_family;
		multistate_ptr = multistate_addressfamily;
		goto parse_multistate;

	case oEnableSSHKeysign:
		intptr = &options->enable_ssh_keysign;
		goto parse_flag;

	case oIdentitiesOnly:
		intptr = &options->identities_only;
		goto parse_flag;

	case oServerAliveInterval:
		intptr = &options->server_alive_interval;
		goto parse_time;

	case oServerAliveCountMax:
		intptr = &options->server_alive_count_max;
		goto parse_int;

	case oSendEnv:
		while ((arg = strdelim(&s)) != NULL && *arg != '\0') {
			if (strchr(arg, '=') != NULL)
				fatal("%s line %d: Invalid environment name.",
				    filename, linenum);
			if (!*activep)
				continue;
			if (options->num_send_env >= MAX_SEND_ENV)
				fatal("%s line %d: too many send env.",
				    filename, linenum);
			options->send_env[options->num_send_env++] =
			    xstrdup(arg);
		}
		break;

	case oControlPath:
		charptr = &options->control_path;
		goto parse_string;

	case oControlMaster:
		intptr = &options->control_master;
		multistate_ptr = multistate_controlmaster;
		goto parse_multistate;

	case oControlPersist:
		/* no/false/yes/true, or a time spec */
		intptr = &options->control_persist;
		arg = strdelim(&s);
		if (!arg || *arg == '\0')
			fatal("%.200s line %d: Missing ControlPersist"
			    " argument.", filename, linenum);
		value = 0;
		value2 = 0;	/* timeout */
		if (strcmp(arg, "no") == 0 || strcmp(arg, "false") == 0)
			value = 0;
		else if (strcmp(arg, "yes") == 0 || strcmp(arg, "true") == 0)
			value = 1;
		else if ((value2 = convtime(arg)) >= 0)
			value = 1;
		else
			fatal("%.200s line %d: Bad ControlPersist argument.",
			    filename, linenum);
		if (*activep && *intptr == -1) {
			*intptr = value;
			options->control_persist_timeout = value2;
		}
		break;

	case oHashKnownHosts:
		intptr = &options->hash_known_hosts;
		goto parse_flag;

	case oTunnel:
		intptr = &options->tun_open;
		multistate_ptr = multistate_tunnel;
		goto parse_multistate;

	case oTunnelDevice:
		arg = strdelim(&s);
		if (!arg || *arg == '\0')
			fatal("%.200s line %d: Missing argument.", filename, linenum);
		value = a2tun(arg, &value2);
		if (value == SSH_TUNID_ERR)
			fatal("%.200s line %d: Bad tun device.", filename, linenum);
		if (*activep) {
			options->tun_local = value;
			options->tun_remote = value2;
		}
		break;

	case oLocalCommand:
		charptr = &options->local_command;
		goto parse_command;

	case oPermitLocalCommand:
		intptr = &options->permit_local_command;
		goto parse_flag;

	case oVisualHostKey:
		intptr = &options->visual_host_key;
		goto parse_flag;

	case oIPQoS:
		arg = strdelim(&s);
		if ((value = parse_ipqos(arg)) == -1)
			fatal("%s line %d: Bad IPQoS value: %s",
			    filename, linenum, arg);
		arg = strdelim(&s);
		if (arg == NULL)
			value2 = value;
		else if ((value2 = parse_ipqos(arg)) == -1)
			fatal("%s line %d: Bad IPQoS value: %s",
			    filename, linenum, arg);
		if (*activep) {
			options->ip_qos_interactive = value;
			options->ip_qos_bulk = value2;
		}
		break;

	case oUseRoaming:
		intptr = &options->use_roaming;
		goto parse_flag;

	case oRequestTTY:
		intptr = &options->request_tty;
		multistate_ptr = multistate_requesttty;
		goto parse_multistate;

	case oVersionAddendum:
		if (s == NULL)
			fatal("%.200s line %d: Missing argument.", filename,
			    linenum);
		len = strspn(s, WHITESPACE);
		if (*activep && options->version_addendum == NULL) {
			if (strcasecmp(s + len, "none") == 0)
				options->version_addendum = xstrdup("");
			else if (strchr(s + len, '\r') != NULL)
				fatal("%.200s line %d: Invalid argument",
				    filename, linenum);
			else
				options->version_addendum = xstrdup(s + len);
		}
		return 0;

	case oIgnoreUnknown:
		charptr = &options->ignored_unknown;
		goto parse_string;

	case oProxyUseFdpass:
		intptr = &options->proxy_use_fdpass;
		goto parse_flag;

	case oCanonicalDomains:
		value = options->num_canonical_domains != 0;
		while ((arg = strdelim(&s)) != NULL && *arg != '\0') {
			valid_domain(arg, filename, linenum);
			if (!*activep || value)
				continue;
			if (options->num_canonical_domains >= MAX_CANON_DOMAINS)
				fatal("%s line %d: too many hostname suffixes.",
				    filename, linenum);
			options->canonical_domains[
			    options->num_canonical_domains++] = xstrdup(arg);
		}
		break;

	case oCanonicalizePermittedCNAMEs:
		value = options->num_permitted_cnames != 0;
		while ((arg = strdelim(&s)) != NULL && *arg != '\0') {
			/* Either '*' for everything or 'list:list' */
			if (strcmp(arg, "*") == 0)
				arg2 = arg;
			else {
				lowercase(arg);
				if ((arg2 = strchr(arg, ':')) == NULL ||
				    arg2[1] == '\0') {
					fatal("%s line %d: "
					    "Invalid permitted CNAME \"%s\"",
					    filename, linenum, arg);
				}
				*arg2 = '\0';
				arg2++;
			}
			if (!*activep || value)
				continue;
			if (options->num_permitted_cnames >= MAX_CANON_DOMAINS)
				fatal("%s line %d: too many permitted CNAMEs.",
				    filename, linenum);
			cname = options->permitted_cnames +
			    options->num_permitted_cnames++;
			cname->source_list = xstrdup(arg);
			cname->target_list = xstrdup(arg2);
		}
		break;

	case oCanonicalizeHostname:
		intptr = &options->canonicalize_hostname;
		multistate_ptr = multistate_canonicalizehostname;
		goto parse_multistate;

	case oCanonicalizeMaxDots:
		intptr = &options->canonicalize_max_dots;
		goto parse_int;

	case oCanonicalizeFallbackLocal:
		intptr = &options->canonicalize_fallback_local;
		goto parse_flag;

	case oStreamLocalBindMask:
		arg = strdelim(&s);
		if (!arg || *arg == '\0')
			fatal("%.200s line %d: Missing StreamLocalBindMask argument.", filename, linenum);
		/* Parse mode in octal format */
		value = strtol(arg, &endofnumber, 8);
		if (arg == endofnumber || value < 0 || value > 0777)
			fatal("%.200s line %d: Bad mask.", filename, linenum);
		options->fwd_opts.streamlocal_bind_mask = (mode_t)value;
		break;

	case oStreamLocalBindUnlink:
		intptr = &options->fwd_opts.streamlocal_bind_unlink;
		goto parse_flag;

	case oRevokedHostKeys:
		charptr = &options->revoked_host_keys;
		goto parse_string;

	case oFingerprintHash:
		intptr = &options->fingerprint_hash;
		arg = strdelim(&s);
		if (!arg || *arg == '\0')
			fatal("%.200s line %d: Missing argument.",
			    filename, linenum);
		if ((value = ssh_digest_alg_by_name(arg)) == -1)
			fatal("%.200s line %d: Invalid hash algorithm \"%s\".",
			    filename, linenum, arg);
		if (*activep && *intptr == -1)
			*intptr = value;
		break;

	case oUpdateHostkeys:
		intptr = &options->update_hostkeys;
		multistate_ptr = multistate_yesnoask;
		goto parse_multistate;

	case oHostbasedKeyTypes:
		charptr = &options->hostbased_key_types;
		goto parse_keytypes;

	case oPubkeyAcceptedKeyTypes:
		charptr = &options->pubkey_key_types;
		goto parse_keytypes;

	case oDeprecated:
		debug("%s line %d: Deprecated option \"%s\"",
		    filename, linenum, keyword);
		return 0;

	case oUnsupported:
		error("%s line %d: Unsupported option \"%s\"",
		    filename, linenum, keyword);
		return 0;

	default:
		fatal("%s: Unimplemented opcode %d", __func__, opcode);
	}

	/* Check that there is no garbage at end of line. */
	if ((arg = strdelim(&s)) != NULL && *arg != '\0') {
		fatal("%.200s line %d: garbage at end of line; \"%.200s\".",
		    filename, linenum, arg);
	}
	return 0;
}


/*
 * Reads the config file and modifies the options accordingly.  Options
 * should already be initialized before this call.  This never returns if
 * there is an error.  If the file does not exist, this returns 0.
 */

int
read_config_file(const char *filename, struct passwd *pw, const char *host,
    const char *original_host, Options *options, int flags)
{
	FILE *f;
	char line[1024];
	int active, linenum;
	int bad_options = 0;

	if ((f = fopen(filename, "r")) == NULL)
		return 0;

	if (flags & SSHCONF_CHECKPERM) {
		struct stat sb;

		if (fstat(fileno(f), &sb) == -1)
			fatal("fstat %s: %s", filename, strerror(errno));
		if (((sb.st_uid != 0 && sb.st_uid != getuid()) ||
		    (sb.st_mode & 022) != 0))
			fatal("Bad owner or permissions on %s", filename);
	}

	debug("Reading configuration data %.200s", filename);

	/*
	 * Mark that we are now processing the options.  This flag is turned
	 * on/off by Host specifications.
	 */
	active = 1;
	linenum = 0;
	while (fgets(line, sizeof(line), f)) {
		/* Update line number counter. */
		linenum++;
		if (process_config_line(options, pw, host, original_host,
		    line, filename, linenum, &active, flags) != 0)
			bad_options++;
	}
	fclose(f);
	if (bad_options > 0)
		fatal("%s: terminating, %d bad configuration options",
		    filename, bad_options);
	return 1;
}

/* Returns 1 if a string option is unset or set to "none" or 0 otherwise. */
int
option_clear_or_none(const char *o)
{
	return o == NULL || strcasecmp(o, "none") == 0;
}

/*
 * Initializes options to special values that indicate that they have not yet
 * been set.  Read_config_file will only set options with this value. Options
 * are processed in the following order: command line, user config file,
 * system config file.  Last, fill_default_options is called.
 */

void
initialize_options(Options * options)
{
	memset(options, 'X', sizeof(*options));
	options->version_addendum = NULL;
	options->forward_agent = -1;
	options->forward_x11 = -1;
	options->forward_x11_trusted = -1;
	options->forward_x11_timeout = -1;
	options->exit_on_forward_failure = -1;
	options->xauth_location = NULL;
	options->fwd_opts.gateway_ports = -1;
	options->fwd_opts.streamlocal_bind_mask = (mode_t)-1;
	options->fwd_opts.streamlocal_bind_unlink = -1;
	options->use_privileged_port = -1;
	options->rsa_authentication = -1;
	options->pubkey_authentication = -1;
	options->challenge_response_authentication = -1;
	options->gss_authentication = -1;
	options->gss_deleg_creds = -1;
	options->password_authentication = -1;
	options->kbd_interactive_authentication = -1;
	options->kbd_interactive_devices = NULL;
	options->rhosts_rsa_authentication = -1;
	options->hostbased_authentication = -1;
	options->batch_mode = -1;
	options->check_host_ip = -1;
	options->strict_host_key_checking = -1;
	options->compression = -1;
	options->tcp_keep_alive = -1;
	options->compression_level = -1;
	options->port = -1;
	options->address_family = -1;
	options->connection_attempts = -1;
	options->connection_timeout = -1;
	options->number_of_password_prompts = -1;
	options->cipher = -1;
	options->ciphers = NULL;
	options->macs = NULL;
	options->kex_algorithms = NULL;
	options->hostkeyalgorithms = NULL;
	options->protocol = SSH_PROTO_UNKNOWN;
	options->num_identity_files = 0;
	options->hostname = NULL;
	options->host_key_alias = NULL;
	options->proxy_command = NULL;
	options->user = NULL;
	options->escape_char = -1;
	options->num_system_hostfiles = 0;
	options->num_user_hostfiles = 0;
	options->local_forwards = NULL;
	options->num_local_forwards = 0;
	options->remote_forwards = NULL;
	options->num_remote_forwards = 0;
	options->clear_forwardings = -1;
	options->log_level = SYSLOG_LEVEL_NOT_SET;
	options->preferred_authentications = NULL;
	options->bind_address = NULL;
	options->pkcs11_provider = NULL;
	options->enable_ssh_keysign = - 1;
	options->no_host_authentication_for_localhost = - 1;
	options->identities_only = - 1;
	options->rekey_limit = - 1;
	options->rekey_interval = -1;
	options->verify_host_key_dns = -1;
	options->server_alive_interval = -1;
	options->server_alive_count_max = -1;
	options->num_send_env = 0;
	options->control_path = NULL;
	options->control_master = -1;
	options->control_persist = -1;
	options->control_persist_timeout = 0;
	options->hash_known_hosts = -1;
	options->tun_open = -1;
	options->tun_local = -1;
	options->tun_remote = -1;
	options->local_command = NULL;
	options->permit_local_command = -1;
	options->use_roaming = 0;
	options->visual_host_key = -1;
	options->ip_qos_interactive = -1;
	options->ip_qos_bulk = -1;
	options->request_tty = -1;
	options->proxy_use_fdpass = -1;
	options->ignored_unknown = NULL;
	options->num_canonical_domains = 0;
	options->num_permitted_cnames = 0;
	options->canonicalize_max_dots = -1;
	options->canonicalize_fallback_local = -1;
	options->canonicalize_hostname = -1;
	options->revoked_host_keys = NULL;
	options->fingerprint_hash = -1;
	options->update_hostkeys = -1;
	options->hostbased_key_types = NULL;
	options->pubkey_key_types = NULL;
}

/*
 * A petite version of fill_default_options() that just fills the options
 * needed for hostname canonicalization to proceed.
 */
void
fill_default_options_for_canonicalization(Options *options)
{
	if (options->canonicalize_max_dots == -1)
		options->canonicalize_max_dots = 1;
	if (options->canonicalize_fallback_local == -1)
		options->canonicalize_fallback_local = 1;
	if (options->canonicalize_hostname == -1)
		options->canonicalize_hostname = SSH_CANONICALISE_NO;
}

/*
 * Called after processing other sources of option data, this fills those
 * options for which no value has been specified with their default values.
 */
void
fill_default_options(Options * options)
{
	if (options->forward_agent == -1)
		options->forward_agent = 0;
	if (options->forward_x11 == -1)
		options->forward_x11 = 0;
	if (options->forward_x11_trusted == -1)
		options->forward_x11_trusted = 0;
	if (options->forward_x11_timeout == -1)
		options->forward_x11_timeout = 1200;
	if (options->exit_on_forward_failure == -1)
		options->exit_on_forward_failure = 0;
	if (options->xauth_location == NULL)
		options->xauth_location = _PATH_XAUTH;
	if (options->fwd_opts.gateway_ports == -1)
		options->fwd_opts.gateway_ports = 0;
	if (options->fwd_opts.streamlocal_bind_mask == (mode_t)-1)
		options->fwd_opts.streamlocal_bind_mask = 0177;
	if (options->fwd_opts.streamlocal_bind_unlink == -1)
		options->fwd_opts.streamlocal_bind_unlink = 0;
	if (options->use_privileged_port == -1)
		options->use_privileged_port = 0;
	if (options->rsa_authentication == -1)
		options->rsa_authentication = 1;
	if (options->pubkey_authentication == -1)
		options->pubkey_authentication = 1;
	if (options->challenge_response_authentication == -1)
		options->challenge_response_authentication = 1;
	if (options->gss_authentication == -1)
		options->gss_authentication = 0;
	if (options->gss_deleg_creds == -1)
		options->gss_deleg_creds = 0;
	if (options->password_authentication == -1)
		options->password_authentication = 1;
	if (options->kbd_interactive_authentication == -1)
		options->kbd_interactive_authentication = 1;
	if (options->rhosts_rsa_authentication == -1)
		options->rhosts_rsa_authentication = 0;
	if (options->hostbased_authentication == -1)
		options->hostbased_authentication = 0;
	if (options->batch_mode == -1)
		options->batch_mode = 0;
	if (options->check_host_ip == -1)
		options->check_host_ip = 0;
	if (options->strict_host_key_checking == -1)
		options->strict_host_key_checking = 2;	/* 2 is default */
	if (options->compression == -1)
		options->compression = 0;
	if (options->tcp_keep_alive == -1)
		options->tcp_keep_alive = 1;
	if (options->compression_level == -1)
		options->compression_level = 6;
	if (options->port == -1)
		options->port = 0;	/* Filled in ssh_connect. */
	if (options->address_family == -1)
		options->address_family = AF_UNSPEC;
	if (options->connection_attempts == -1)
		options->connection_attempts = 1;
	if (options->number_of_password_prompts == -1)
		options->number_of_password_prompts = 3;
	/* Selected in ssh_login(). */
	if (options->cipher == -1)
		options->cipher = SSH_CIPHER_NOT_SET;
	/* options->hostkeyalgorithms, default set in myproposals.h */
	if (options->protocol == SSH_PROTO_UNKNOWN)
		options->protocol = SSH_PROTO_2;
	if (options->num_identity_files == 0) {
		if (options->protocol & SSH_PROTO_1) {
			add_identity_file(options, "~/",
			    _PATH_SSH_CLIENT_IDENTITY, 0);
		}
		if (options->protocol & SSH_PROTO_2) {
			add_identity_file(options, "~/",
			    _PATH_SSH_CLIENT_ID_RSA, 0);
			add_identity_file(options, "~/",
			    _PATH_SSH_CLIENT_ID_DSA, 0);
#ifdef OPENSSL_HAS_ECC
			add_identity_file(options, "~/",
			    _PATH_SSH_CLIENT_ID_ECDSA, 0);
#endif
			add_identity_file(options, "~/",
			    _PATH_SSH_CLIENT_ID_ED25519, 0);
		}
	}
	if (options->escape_char == -1)
		options->escape_char = '~';
	if (options->num_system_hostfiles == 0) {
		options->system_hostfiles[options->num_system_hostfiles++] =
		    xstrdup(_PATH_SSH_SYSTEM_HOSTFILE);
		options->system_hostfiles[options->num_system_hostfiles++] =
		    xstrdup(_PATH_SSH_SYSTEM_HOSTFILE2);
	}
	if (options->num_user_hostfiles == 0) {
		options->user_hostfiles[options->num_user_hostfiles++] =
		    xstrdup(_PATH_SSH_USER_HOSTFILE);
		options->user_hostfiles[options->num_user_hostfiles++] =
		    xstrdup(_PATH_SSH_USER_HOSTFILE2);
	}
	if (options->log_level == SYSLOG_LEVEL_NOT_SET)
		options->log_level = SYSLOG_LEVEL_INFO;
	if (options->clear_forwardings == 1)
		clear_forwardings(options);
	if (options->no_host_authentication_for_localhost == - 1)
		options->no_host_authentication_for_localhost = 0;
	if (options->identities_only == -1)
		options->identities_only = 0;
	if (options->enable_ssh_keysign == -1)
		options->enable_ssh_keysign = 0;
	if (options->rekey_limit == -1)
		options->rekey_limit = 0;
	if (options->rekey_interval == -1)
		options->rekey_interval = 0;
#if HAVE_LDNS
	if (options->verify_host_key_dns == -1)
		/* automatically trust a verified SSHFP record */
		options->verify_host_key_dns = 1;
#else
	if (options->verify_host_key_dns == -1)
		options->verify_host_key_dns = 0;
#endif
	if (options->server_alive_interval == -1)
		options->server_alive_interval = 0;
	if (options->server_alive_count_max == -1)
		options->server_alive_count_max = 3;
	if (options->control_master == -1)
		options->control_master = 0;
	if (options->control_persist == -1) {
		options->control_persist = 0;
		options->control_persist_timeout = 0;
	}
	if (options->hash_known_hosts == -1)
		options->hash_known_hosts = 0;
	if (options->tun_open == -1)
		options->tun_open = SSH_TUNMODE_NO;
	if (options->tun_local == -1)
		options->tun_local = SSH_TUNID_ANY;
	if (options->tun_remote == -1)
		options->tun_remote = SSH_TUNID_ANY;
	if (options->permit_local_command == -1)
		options->permit_local_command = 0;
	options->use_roaming = 0;
	if (options->visual_host_key == -1)
		options->visual_host_key = 0;
	if (options->ip_qos_interactive == -1)
		options->ip_qos_interactive = IPTOS_LOWDELAY;
	if (options->ip_qos_bulk == -1)
		options->ip_qos_bulk = IPTOS_THROUGHPUT;
	if (options->request_tty == -1)
		options->request_tty = REQUEST_TTY_AUTO;
	if (options->proxy_use_fdpass == -1)
		options->proxy_use_fdpass = 0;
	if (options->canonicalize_max_dots == -1)
		options->canonicalize_max_dots = 1;
	if (options->canonicalize_fallback_local == -1)
		options->canonicalize_fallback_local = 1;
	if (options->canonicalize_hostname == -1)
		options->canonicalize_hostname = SSH_CANONICALISE_NO;
	if (options->fingerprint_hash == -1)
		options->fingerprint_hash = SSH_FP_HASH_DEFAULT;
	if (options->update_hostkeys == -1)
		options->update_hostkeys = 0;
	if (kex_assemble_names(KEX_CLIENT_ENCRYPT, &options->ciphers) != 0 ||
	    kex_assemble_names(KEX_CLIENT_MAC, &options->macs) != 0 ||
	    kex_assemble_names(KEX_CLIENT_KEX, &options->kex_algorithms) != 0 ||
	    kex_assemble_names(KEX_DEFAULT_PK_ALG,
	    &options->hostbased_key_types) != 0 ||
	    kex_assemble_names(KEX_DEFAULT_PK_ALG,
	    &options->pubkey_key_types) != 0)
		fatal("%s: kex_assemble_names failed", __func__);

#define CLEAR_ON_NONE(v) \
	do { \
		if (option_clear_or_none(v)) { \
			free(v); \
			v = NULL; \
		} \
	} while(0)
	CLEAR_ON_NONE(options->local_command);
	CLEAR_ON_NONE(options->proxy_command);
	CLEAR_ON_NONE(options->control_path);
	CLEAR_ON_NONE(options->revoked_host_keys);
	/* options->user will be set in the main program if appropriate */
	/* options->hostname will be set in the main program if appropriate */
	/* options->host_key_alias should not be set by default */
	/* options->preferred_authentications will be set in ssh */
	if (options->version_addendum == NULL)
		options->version_addendum = xstrdup(SSH_VERSION_FREEBSD);
}

struct fwdarg {
	char *arg;
	int ispath;
};

/*
 * parse_fwd_field
 * parses the next field in a port forwarding specification.
 * sets fwd to the parsed field and advances p past the colon
 * or sets it to NULL at end of string.
 * returns 0 on success, else non-zero.
 */
static int
parse_fwd_field(char **p, struct fwdarg *fwd)
{
	char *ep, *cp = *p;
	int ispath = 0;

	if (*cp == '\0') {
		*p = NULL;
		return -1;	/* end of string */
	}

	/*
	 * A field escaped with square brackets is used literally.
	 * XXX - allow ']' to be escaped via backslash?
	 */
	if (*cp == '[') {
		/* find matching ']' */
		for (ep = cp + 1; *ep != ']' && *ep != '\0'; ep++) {
			if (*ep == '/')
				ispath = 1;
		}
		/* no matching ']' or not at end of field. */
		if (ep[0] != ']' || (ep[1] != ':' && ep[1] != '\0'))
			return -1;
		/* NUL terminate the field and advance p past the colon */
		*ep++ = '\0';
		if (*ep != '\0')
			*ep++ = '\0';
		fwd->arg = cp + 1;
		fwd->ispath = ispath;
		*p = ep;
		return 0;
	}

	for (cp = *p; *cp != '\0'; cp++) {
		switch (*cp) {
		case '\\':
			memmove(cp, cp + 1, strlen(cp + 1) + 1);
			if (*cp == '\0')
				return -1;
			break;
		case '/':
			ispath = 1;
			break;
		case ':':
			*cp++ = '\0';
			goto done;
		}
	}
done:
	fwd->arg = *p;
	fwd->ispath = ispath;
	*p = cp;
	return 0;
}

/*
 * parse_forward
 * parses a string containing a port forwarding specification of the form:
 *   dynamicfwd == 0
 *	[listenhost:]listenport|listenpath:connecthost:connectport|connectpath
 *	listenpath:connectpath
 *   dynamicfwd == 1
 *	[listenhost:]listenport
 * returns number of arguments parsed or zero on error
 */
int
parse_forward(struct Forward *fwd, const char *fwdspec, int dynamicfwd, int remotefwd)
{
	struct fwdarg fwdargs[4];
	char *p, *cp;
	int i;

	memset(fwd, 0, sizeof(*fwd));
	memset(fwdargs, 0, sizeof(fwdargs));

	cp = p = xstrdup(fwdspec);

	/* skip leading spaces */
	while (isspace((u_char)*cp))
		cp++;

	for (i = 0; i < 4; ++i) {
		if (parse_fwd_field(&cp, &fwdargs[i]) != 0)
			break;
	}

	/* Check for trailing garbage */
	if (cp != NULL && *cp != '\0') {
		i = 0;	/* failure */
	}

	switch (i) {
	case 1:
		if (fwdargs[0].ispath) {
			fwd->listen_path = xstrdup(fwdargs[0].arg);
			fwd->listen_port = PORT_STREAMLOCAL;
		} else {
			fwd->listen_host = NULL;
			fwd->listen_port = a2port(fwdargs[0].arg);
		}
		fwd->connect_host = xstrdup("socks");
		break;

	case 2:
		if (fwdargs[0].ispath && fwdargs[1].ispath) {
			fwd->listen_path = xstrdup(fwdargs[0].arg);
			fwd->listen_port = PORT_STREAMLOCAL;
			fwd->connect_path = xstrdup(fwdargs[1].arg);
			fwd->connect_port = PORT_STREAMLOCAL;
		} else if (fwdargs[1].ispath) {
			fwd->listen_host = NULL;
			fwd->listen_port = a2port(fwdargs[0].arg);
			fwd->connect_path = xstrdup(fwdargs[1].arg);
			fwd->connect_port = PORT_STREAMLOCAL;
		} else {
			fwd->listen_host = xstrdup(fwdargs[0].arg);
			fwd->listen_port = a2port(fwdargs[1].arg);
			fwd->connect_host = xstrdup("socks");
		}
		break;

	case 3:
		if (fwdargs[0].ispath) {
			fwd->listen_path = xstrdup(fwdargs[0].arg);
			fwd->listen_port = PORT_STREAMLOCAL;
			fwd->connect_host = xstrdup(fwdargs[1].arg);
			fwd->connect_port = a2port(fwdargs[2].arg);
		} else if (fwdargs[2].ispath) {
			fwd->listen_host = xstrdup(fwdargs[0].arg);
			fwd->listen_port = a2port(fwdargs[1].arg);
			fwd->connect_path = xstrdup(fwdargs[2].arg);
			fwd->connect_port = PORT_STREAMLOCAL;
		} else {
			fwd->listen_host = NULL;
			fwd->listen_port = a2port(fwdargs[0].arg);
			fwd->connect_host = xstrdup(fwdargs[1].arg);
			fwd->connect_port = a2port(fwdargs[2].arg);
		}
		break;

	case 4:
		fwd->listen_host = xstrdup(fwdargs[0].arg);
		fwd->listen_port = a2port(fwdargs[1].arg);
		fwd->connect_host = xstrdup(fwdargs[2].arg);
		fwd->connect_port = a2port(fwdargs[3].arg);
		break;
	default:
		i = 0; /* failure */
	}

	free(p);

	if (dynamicfwd) {
		if (!(i == 1 || i == 2))
			goto fail_free;
	} else {
		if (!(i == 3 || i == 4)) {
			if (fwd->connect_path == NULL &&
			    fwd->listen_path == NULL)
				goto fail_free;
		}
		if (fwd->connect_port <= 0 && fwd->connect_path == NULL)
			goto fail_free;
	}

	if ((fwd->listen_port < 0 && fwd->listen_path == NULL) ||
	    (!remotefwd && fwd->listen_port == 0))
		goto fail_free;
	if (fwd->connect_host != NULL &&
	    strlen(fwd->connect_host) >= NI_MAXHOST)
		goto fail_free;
	/* XXX - if connecting to a remote socket, max sun len may not match this host */
	if (fwd->connect_path != NULL &&
	    strlen(fwd->connect_path) >= PATH_MAX_SUN)
		goto fail_free;
	if (fwd->listen_host != NULL &&
	    strlen(fwd->listen_host) >= NI_MAXHOST)
		goto fail_free;
	if (fwd->listen_path != NULL &&
	    strlen(fwd->listen_path) >= PATH_MAX_SUN)
		goto fail_free;

	return (i);

 fail_free:
	free(fwd->connect_host);
	fwd->connect_host = NULL;
	free(fwd->connect_path);
	fwd->connect_path = NULL;
	free(fwd->listen_host);
	fwd->listen_host = NULL;
	free(fwd->listen_path);
	fwd->listen_path = NULL;
	return (0);
}

/* XXX the following is a near-vebatim copy from servconf.c; refactor */
static const char *
fmt_multistate_int(int val, const struct multistate *m)
{
	u_int i;

	for (i = 0; m[i].key != NULL; i++) {
		if (m[i].value == val)
			return m[i].key;
	}
	return "UNKNOWN";
}

static const char *
fmt_intarg(OpCodes code, int val)
{
	if (val == -1)
		return "unset";
	switch (code) {
	case oAddressFamily:
		return fmt_multistate_int(val, multistate_addressfamily);
	case oVerifyHostKeyDNS:
	case oStrictHostKeyChecking:
	case oUpdateHostkeys:
		return fmt_multistate_int(val, multistate_yesnoask);
	case oControlMaster:
		return fmt_multistate_int(val, multistate_controlmaster);
	case oTunnel:
		return fmt_multistate_int(val, multistate_tunnel);
	case oRequestTTY:
		return fmt_multistate_int(val, multistate_requesttty);
	case oCanonicalizeHostname:
		return fmt_multistate_int(val, multistate_canonicalizehostname);
	case oFingerprintHash:
		return ssh_digest_alg_name(val);
	case oProtocol:
		switch (val) {
		case SSH_PROTO_1:
			return "1";
		case SSH_PROTO_2:
			return "2";
		case (SSH_PROTO_1|SSH_PROTO_2):
			return "2,1";
		default:
			return "UNKNOWN";
		}
	default:
		switch (val) {
		case 0:
			return "no";
		case 1:
			return "yes";
		default:
			return "UNKNOWN";
		}
	}
}

static const char *
lookup_opcode_name(OpCodes code)
{
	u_int i;

	for (i = 0; keywords[i].name != NULL; i++)
		if (keywords[i].opcode == code)
			return(keywords[i].name);
	return "UNKNOWN";
}

static void
dump_cfg_int(OpCodes code, int val)
{
	printf("%s %d\n", lookup_opcode_name(code), val);
}

static void
dump_cfg_fmtint(OpCodes code, int val)
{
	printf("%s %s\n", lookup_opcode_name(code), fmt_intarg(code, val));
}

static void
dump_cfg_string(OpCodes code, const char *val)
{
	if (val == NULL)
		return;
	printf("%s %s\n", lookup_opcode_name(code), val);
}

static void
dump_cfg_strarray(OpCodes code, u_int count, char **vals)
{
	u_int i;

	for (i = 0; i < count; i++)
		printf("%s %s\n", lookup_opcode_name(code), vals[i]);
}

static void
dump_cfg_strarray_oneline(OpCodes code, u_int count, char **vals)
{
	u_int i;

	printf("%s", lookup_opcode_name(code));
	for (i = 0; i < count; i++)
		printf(" %s",  vals[i]);
	printf("\n");
}

static void
dump_cfg_forwards(OpCodes code, u_int count, const struct Forward *fwds)
{
	const struct Forward *fwd;
	u_int i;

	/* oDynamicForward */
	for (i = 0; i < count; i++) {
		fwd = &fwds[i];
		if (code == oDynamicForward &&
		    strcmp(fwd->connect_host, "socks") != 0)
			continue;
		if (code == oLocalForward &&
		    strcmp(fwd->connect_host, "socks") == 0)
			continue;
		printf("%s", lookup_opcode_name(code));
		if (fwd->listen_port == PORT_STREAMLOCAL)
			printf(" %s", fwd->listen_path);
		else if (fwd->listen_host == NULL)
			printf(" %d", fwd->listen_port);
		else {
			printf(" [%s]:%d",
			    fwd->listen_host, fwd->listen_port);
		}
		if (code != oDynamicForward) {
			if (fwd->connect_port == PORT_STREAMLOCAL)
				printf(" %s", fwd->connect_path);
			else if (fwd->connect_host == NULL)
				printf(" %d", fwd->connect_port);
			else {
				printf(" [%s]:%d",
				    fwd->connect_host, fwd->connect_port);
			}
		}
		printf("\n");
	}
}

void
dump_client_config(Options *o, const char *host)
{
	int i;
	char vbuf[5];

	/* Most interesting options first: user, host, port */
	dump_cfg_string(oUser, o->user);
	dump_cfg_string(oHostName, host);
	dump_cfg_int(oPort, o->port);

	/* Flag options */
	dump_cfg_fmtint(oAddressFamily, o->address_family);
	dump_cfg_fmtint(oBatchMode, o->batch_mode);
	dump_cfg_fmtint(oCanonicalizeFallbackLocal, o->canonicalize_fallback_local);
	dump_cfg_fmtint(oCanonicalizeHostname, o->canonicalize_hostname);
	dump_cfg_fmtint(oChallengeResponseAuthentication, o->challenge_response_authentication);
	dump_cfg_fmtint(oCheckHostIP, o->check_host_ip);
	dump_cfg_fmtint(oCompression, o->compression);
	dump_cfg_fmtint(oControlMaster, o->control_master);
	dump_cfg_fmtint(oEnableSSHKeysign, o->enable_ssh_keysign);
	dump_cfg_fmtint(oExitOnForwardFailure, o->exit_on_forward_failure);
	dump_cfg_fmtint(oFingerprintHash, o->fingerprint_hash);
	dump_cfg_fmtint(oForwardAgent, o->forward_agent);
	dump_cfg_fmtint(oForwardX11, o->forward_x11);
	dump_cfg_fmtint(oForwardX11Trusted, o->forward_x11_trusted);
	dump_cfg_fmtint(oGatewayPorts, o->fwd_opts.gateway_ports);
#ifdef GSSAPI
	dump_cfg_fmtint(oGssAuthentication, o->gss_authentication);
	dump_cfg_fmtint(oGssDelegateCreds, o->gss_deleg_creds);
#endif /* GSSAPI */
	dump_cfg_fmtint(oHashKnownHosts, o->hash_known_hosts);
	dump_cfg_fmtint(oHostbasedAuthentication, o->hostbased_authentication);
	dump_cfg_fmtint(oIdentitiesOnly, o->identities_only);
	dump_cfg_fmtint(oKbdInteractiveAuthentication, o->kbd_interactive_authentication);
	dump_cfg_fmtint(oNoHostAuthenticationForLocalhost, o->no_host_authentication_for_localhost);
	dump_cfg_fmtint(oPasswordAuthentication, o->password_authentication);
	dump_cfg_fmtint(oPermitLocalCommand, o->permit_local_command);
	dump_cfg_fmtint(oProtocol, o->protocol);
	dump_cfg_fmtint(oProxyUseFdpass, o->proxy_use_fdpass);
	dump_cfg_fmtint(oPubkeyAuthentication, o->pubkey_authentication);
	dump_cfg_fmtint(oRequestTTY, o->request_tty);
	dump_cfg_fmtint(oRhostsRSAAuthentication, o->rhosts_rsa_authentication);
	dump_cfg_fmtint(oRSAAuthentication, o->rsa_authentication);
	dump_cfg_fmtint(oStreamLocalBindUnlink, o->fwd_opts.streamlocal_bind_unlink);
	dump_cfg_fmtint(oStrictHostKeyChecking, o->strict_host_key_checking);
	dump_cfg_fmtint(oTCPKeepAlive, o->tcp_keep_alive);
	dump_cfg_fmtint(oTunnel, o->tun_open);
	dump_cfg_fmtint(oUsePrivilegedPort, o->use_privileged_port);
	dump_cfg_fmtint(oVerifyHostKeyDNS, o->verify_host_key_dns);
	dump_cfg_fmtint(oVisualHostKey, o->visual_host_key);
	dump_cfg_fmtint(oUpdateHostkeys, o->update_hostkeys);

	/* Integer options */
	dump_cfg_int(oCanonicalizeMaxDots, o->canonicalize_max_dots);
	dump_cfg_int(oCompressionLevel, o->compression_level);
	dump_cfg_int(oConnectionAttempts, o->connection_attempts);
	dump_cfg_int(oForwardX11Timeout, o->forward_x11_timeout);
	dump_cfg_int(oNumberOfPasswordPrompts, o->number_of_password_prompts);
	dump_cfg_int(oServerAliveCountMax, o->server_alive_count_max);
	dump_cfg_int(oServerAliveInterval, o->server_alive_interval);

	/* String options */
	dump_cfg_string(oBindAddress, o->bind_address);
	dump_cfg_string(oCiphers, o->ciphers ? o->ciphers : KEX_CLIENT_ENCRYPT);
	dump_cfg_string(oControlPath, o->control_path);
	dump_cfg_string(oHostKeyAlgorithms, o->hostkeyalgorithms ? o->hostkeyalgorithms : KEX_DEFAULT_PK_ALG);
	dump_cfg_string(oHostKeyAlias, o->host_key_alias);
	dump_cfg_string(oHostbasedKeyTypes, o->hostbased_key_types);
	dump_cfg_string(oKbdInteractiveDevices, o->kbd_interactive_devices);
	dump_cfg_string(oKexAlgorithms, o->kex_algorithms ? o->kex_algorithms : KEX_CLIENT_KEX);
	dump_cfg_string(oLocalCommand, o->local_command);
	dump_cfg_string(oLogLevel, log_level_name(o->log_level));
	dump_cfg_string(oMacs, o->macs ? o->macs : KEX_CLIENT_MAC);
	dump_cfg_string(oPKCS11Provider, o->pkcs11_provider);
	dump_cfg_string(oPreferredAuthentications, o->preferred_authentications);
	dump_cfg_string(oProxyCommand, o->proxy_command);
	dump_cfg_string(oRevokedHostKeys, o->revoked_host_keys);
	dump_cfg_string(oXAuthLocation, o->xauth_location);

	/* Forwards */
	dump_cfg_forwards(oDynamicForward, o->num_local_forwards, o->local_forwards);
	dump_cfg_forwards(oLocalForward, o->num_local_forwards, o->local_forwards);
	dump_cfg_forwards(oRemoteForward, o->num_remote_forwards, o->remote_forwards);

	/* String array options */
	dump_cfg_strarray(oIdentityFile, o->num_identity_files, o->identity_files);
	dump_cfg_strarray_oneline(oCanonicalDomains, o->num_canonical_domains, o->canonical_domains);
	dump_cfg_strarray_oneline(oGlobalKnownHostsFile, o->num_system_hostfiles, o->system_hostfiles);
	dump_cfg_strarray_oneline(oUserKnownHostsFile, o->num_user_hostfiles, o->user_hostfiles);
	dump_cfg_strarray(oSendEnv, o->num_send_env, o->send_env);

	/* Special cases */

	/* oConnectTimeout */
	if (o->connection_timeout == -1)
		printf("connecttimeout none\n");
	else
		dump_cfg_int(oConnectTimeout, o->connection_timeout);

	/* oTunnelDevice */
	printf("tunneldevice");
	if (o->tun_local == SSH_TUNID_ANY)
		printf(" any");
	else
		printf(" %d", o->tun_local);
	if (o->tun_remote == SSH_TUNID_ANY)
		printf(":any");
	else
		printf(":%d", o->tun_remote);
	printf("\n");

	/* oCanonicalizePermittedCNAMEs */
	if ( o->num_permitted_cnames > 0) {
		printf("canonicalizePermittedcnames");
		for (i = 0; i < o->num_permitted_cnames; i++) {
			printf(" %s:%s", o->permitted_cnames[i].source_list,
			    o->permitted_cnames[i].target_list);
		}
		printf("\n");
	}

	/* oCipher */
	if (o->cipher != SSH_CIPHER_NOT_SET)
		printf("Cipher %s\n", cipher_name(o->cipher));

	/* oControlPersist */
	if (o->control_persist == 0 || o->control_persist_timeout == 0)
		dump_cfg_fmtint(oControlPersist, o->control_persist);
	else
		dump_cfg_int(oControlPersist, o->control_persist_timeout);

	/* oEscapeChar */
	if (o->escape_char == SSH_ESCAPECHAR_NONE)
		printf("escapechar none\n");
	else {
		vis(vbuf, o->escape_char, VIS_WHITE, 0);
		printf("escapechar %s\n", vbuf);
	}

	/* oIPQoS */
	printf("ipqos %s ", iptos2str(o->ip_qos_interactive));
	printf("%s\n", iptos2str(o->ip_qos_bulk));

	/* oRekeyLimit */
	printf("rekeylimit %lld %d\n",
	    (long long)o->rekey_limit, o->rekey_interval);

	/* oStreamLocalBindMask */
	printf("streamlocalbindmask 0%o\n",
	    o->fwd_opts.streamlocal_bind_mask);
}
