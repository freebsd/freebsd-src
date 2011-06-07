/* $OpenBSD: readconf.c,v 1.190 2010/11/13 23:27:50 djm Exp $ */
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

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>

#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "xmalloc.h"
#include "ssh.h"
#include "compat.h"
#include "cipher.h"
#include "pathnames.h"
#include "log.h"
#include "key.h"
#include "readconf.h"
#include "match.h"
#include "misc.h"
#include "buffer.h"
#include "kex.h"
#include "mac.h"
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
	oForwardAgent, oForwardX11, oForwardX11Trusted, oForwardX11Timeout,
	oGatewayPorts, oExitOnForwardFailure,
	oPasswordAuthentication, oRSAAuthentication,
	oChallengeResponseAuthentication, oXAuthLocation,
	oIdentityFile, oHostName, oPort, oCipher, oRemoteForward, oLocalForward,
	oUser, oHost, oEscapeChar, oRhostsRSAAuthentication, oProxyCommand,
	oGlobalKnownHostsFile, oUserKnownHostsFile, oConnectionAttempts,
	oBatchMode, oCheckHostIP, oStrictHostKeyChecking, oCompression,
	oCompressionLevel, oTCPKeepAlive, oNumberOfPasswordPrompts,
	oUsePrivilegedPort, oLogLevel, oCiphers, oProtocol, oMacs,
	oGlobalKnownHostsFile2, oUserKnownHostsFile2, oPubkeyAuthentication,
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
	oVisualHostKey, oUseRoaming, oZeroKnowledgePasswordAuthentication,
	oKexAlgorithms, oIPQoS,
	oVersionAddendum,
	oDeprecated, oUnsupported
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
	{ "escapechar", oEscapeChar },
	{ "globalknownhostsfile", oGlobalKnownHostsFile },
	{ "globalknownhostsfile2", oGlobalKnownHostsFile2 },	/* obsolete */
	{ "userknownhostsfile", oUserKnownHostsFile },
	{ "userknownhostsfile2", oUserKnownHostsFile2 },	/* obsolete */
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
#ifdef JPAKE
	{ "zeroknowledgepasswordauthentication",
	    oZeroKnowledgePasswordAuthentication },
#else
	{ "zeroknowledgepasswordauthentication", oUnsupported },
#endif
	{ "kexalgorithms", oKexAlgorithms },
	{ "ipqos", oIPQoS },

	{ "versionaddendum", oVersionAddendum },
	{ NULL, oBadOption }
};

/*
 * Adds a local TCP/IP port forward to options.  Never returns if there is an
 * error.
 */

void
add_local_forward(Options *options, const Forward *newfwd)
{
	Forward *fwd;
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
		fatal("Privileged ports can only be forwarded by root.");
#endif
	options->local_forwards = xrealloc(options->local_forwards,
	    options->num_local_forwards + 1,
	    sizeof(*options->local_forwards));
	fwd = &options->local_forwards[options->num_local_forwards++];

	fwd->listen_host = newfwd->listen_host;
	fwd->listen_port = newfwd->listen_port;
	fwd->connect_host = newfwd->connect_host;
	fwd->connect_port = newfwd->connect_port;
}

/*
 * Adds a remote TCP/IP port forward to options.  Never returns if there is
 * an error.
 */

void
add_remote_forward(Options *options, const Forward *newfwd)
{
	Forward *fwd;

	options->remote_forwards = xrealloc(options->remote_forwards,
	    options->num_remote_forwards + 1,
	    sizeof(*options->remote_forwards));
	fwd = &options->remote_forwards[options->num_remote_forwards++];

	fwd->listen_host = newfwd->listen_host;
	fwd->listen_port = newfwd->listen_port;
	fwd->connect_host = newfwd->connect_host;
	fwd->connect_port = newfwd->connect_port;
	fwd->allocated_port = 0;
}

static void
clear_forwardings(Options *options)
{
	int i;

	for (i = 0; i < options->num_local_forwards; i++) {
		if (options->local_forwards[i].listen_host != NULL)
			xfree(options->local_forwards[i].listen_host);
		xfree(options->local_forwards[i].connect_host);
	}
	if (options->num_local_forwards > 0) {
		xfree(options->local_forwards);
		options->local_forwards = NULL;
	}
	options->num_local_forwards = 0;
	for (i = 0; i < options->num_remote_forwards; i++) {
		if (options->remote_forwards[i].listen_host != NULL)
			xfree(options->remote_forwards[i].listen_host);
		xfree(options->remote_forwards[i].connect_host);
	}
	if (options->num_remote_forwards > 0) {
		xfree(options->remote_forwards);
		options->remote_forwards = NULL;
	}
	options->num_remote_forwards = 0;
	options->tun_open = SSH_TUNMODE_NO;
}

/*
 * Returns the number of the token pointed to by cp or oBadOption.
 */

static OpCodes
parse_token(const char *cp, const char *filename, int linenum)
{
	u_int i;

	for (i = 0; keywords[i].name; i++)
		if (strcasecmp(cp, keywords[i].name) == 0)
			return keywords[i].opcode;

	error("%s: line %d: Bad configuration option: %s",
	    filename, linenum, cp);
	return oBadOption;
}

/*
 * Processes a single option line as used in the configuration files. This
 * only sets those values that have not already been set.
 */
#define WHITESPACE " \t\r\n"

int
process_config_line(Options *options, const char *host,
		    char *line, const char *filename, int linenum,
		    int *activep)
{
	char *s, **charptr, *endofnumber, *keyword, *arg, *arg2, fwdarg[256];
	int opcode, *intptr, value, value2, scale;
	LogLevel *log_level_ptr;
	long long orig, val64;
	size_t len;
	Forward fwd;

	/* Strip trailing whitespace */
	for (len = strlen(line) - 1; len > 0; len--) {
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

	opcode = parse_token(keyword, filename, linenum);

	switch (opcode) {
	case oBadOption:
		/* don't panic, but count bad options */
		return -1;
		/* NOTREACHED */
	case oConnectTimeout:
		intptr = &options->connection_timeout;
parse_time:
		arg = strdelim(&s);
		if (!arg || *arg == '\0')
			fatal("%s line %d: missing time value.",
			    filename, linenum);
		if ((value = convtime(arg)) == -1)
			fatal("%s line %d: invalid time value.",
			    filename, linenum);
		if (*activep && *intptr == -1)
			*intptr = value;
		break;

	case oForwardAgent:
		intptr = &options->forward_agent;
parse_flag:
		arg = strdelim(&s);
		if (!arg || *arg == '\0')
			fatal("%.200s line %d: Missing yes/no argument.", filename, linenum);
		value = 0;	/* To avoid compiler warning... */
		if (strcmp(arg, "yes") == 0 || strcmp(arg, "true") == 0)
			value = 1;
		else if (strcmp(arg, "no") == 0 || strcmp(arg, "false") == 0)
			value = 0;
		else
			fatal("%.200s line %d: Bad yes/no argument.", filename, linenum);
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
		intptr = &options->gateway_ports;
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

	case oZeroKnowledgePasswordAuthentication:
		intptr = &options->zero_knowledge_password_authentication;
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
		goto parse_yesnoask;

	case oStrictHostKeyChecking:
		intptr = &options->strict_host_key_checking;
parse_yesnoask:
		arg = strdelim(&s);
		if (!arg || *arg == '\0')
			fatal("%.200s line %d: Missing yes/no/ask argument.",
			    filename, linenum);
		value = 0;	/* To avoid compiler warning... */
		if (strcmp(arg, "yes") == 0 || strcmp(arg, "true") == 0)
			value = 1;
		else if (strcmp(arg, "no") == 0 || strcmp(arg, "false") == 0)
			value = 0;
		else if (strcmp(arg, "ask") == 0)
			value = 2;
		else
			fatal("%.200s line %d: Bad yes/no/ask argument.", filename, linenum);
		if (*activep && *intptr == -1)
			*intptr = value;
		break;

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
			fatal("%.200s line %d: Missing argument.", filename, linenum);
		if (arg[0] < '0' || arg[0] > '9')
			fatal("%.200s line %d: Bad number.", filename, linenum);
		orig = val64 = strtoll(arg, &endofnumber, 10);
		if (arg == endofnumber)
			fatal("%.200s line %d: Bad number.", filename, linenum);
		switch (toupper(*endofnumber)) {
		case '\0':
			scale = 1;
			break;
		case 'K':
			scale = 1<<10;
			break;
		case 'M':
			scale = 1<<20;
			break;
		case 'G':
			scale = 1<<30;
			break;
		default:
			fatal("%.200s line %d: Invalid RekeyLimit suffix",
			    filename, linenum);
		}
		val64 *= scale;
		/* detect integer wrap and too-large limits */
		if ((val64 / scale) != orig || val64 > UINT_MAX)
			fatal("%.200s line %d: RekeyLimit too large",
			    filename, linenum);
		if (val64 < 16)
			fatal("%.200s line %d: RekeyLimit too small",
			    filename, linenum);
		if (*activep && options->rekey_limit == -1)
			options->rekey_limit = (u_int32_t)val64;
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
			charptr = &options->identity_files[*intptr];
			*charptr = xstrdup(arg);
			*intptr = *intptr + 1;
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
			fatal("%.200s line %d: Missing argument.", filename, linenum);
		if (*activep && *charptr == NULL)
			*charptr = xstrdup(arg);
		break;

	case oGlobalKnownHostsFile:
		charptr = &options->system_hostfile;
		goto parse_string;

	case oUserKnownHostsFile:
		charptr = &options->user_hostfile;
		goto parse_string;

	case oGlobalKnownHostsFile2:
		charptr = &options->system_hostfile2;
		goto parse_string;

	case oUserKnownHostsFile2:
		charptr = &options->user_hostfile2;
		goto parse_string;

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
		if (!ciphers_valid(arg))
			fatal("%.200s line %d: Bad SSH2 cipher spec '%s'.",
			    filename, linenum, arg ? arg : "<NONE>");
		if (*activep && options->ciphers == NULL)
			options->ciphers = xstrdup(arg);
		break;

	case oMacs:
		arg = strdelim(&s);
		if (!arg || *arg == '\0')
			fatal("%.200s line %d: Missing argument.", filename, linenum);
		if (!mac_valid(arg))
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
		if (!kex_names_valid(arg))
			fatal("%.200s line %d: Bad SSH2 KexAlgorithms '%s'.",
			    filename, linenum, arg ? arg : "<NONE>");
		if (*activep && options->kex_algorithms == NULL)
			options->kex_algorithms = xstrdup(arg);
		break;

	case oHostKeyAlgorithms:
		arg = strdelim(&s);
		if (!arg || *arg == '\0')
			fatal("%.200s line %d: Missing argument.", filename, linenum);
		if (!key_names_valid2(arg))
			fatal("%.200s line %d: Bad protocol 2 host key algorithms '%s'.",
			    filename, linenum, arg ? arg : "<NONE>");
		if (*activep && options->hostkeyalgorithms == NULL)
			options->hostkeyalgorithms = xstrdup(arg);
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
		*activep = 0;
		while ((arg = strdelim(&s)) != NULL && *arg != '\0')
			if (match_pattern(host, arg)) {
				debug("Applying options for %.100s", arg);
				*activep = 1;
				break;
			}
		/* Avoid garbage check below, as strdelim is done. */
		return 0;

	case oEscapeChar:
		intptr = &options->escape_char;
		arg = strdelim(&s);
		if (!arg || *arg == '\0')
			fatal("%.200s line %d: Missing argument.", filename, linenum);
		if (arg[0] == '^' && arg[2] == 0 &&
		    (u_char) arg[1] >= 64 && (u_char) arg[1] < 128)
			value = (u_char) arg[1] & 31;
		else if (strlen(arg) == 1)
			value = (u_char) arg[0];
		else if (strcmp(arg, "none") == 0)
			value = SSH_ESCAPECHAR_NONE;
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
		arg = strdelim(&s);
		if (!arg || *arg == '\0')
			fatal("%s line %d: missing address family.",
			    filename, linenum);
		intptr = &options->address_family;
		if (strcasecmp(arg, "inet") == 0)
			value = AF_INET;
		else if (strcasecmp(arg, "inet6") == 0)
			value = AF_INET6;
		else if (strcasecmp(arg, "any") == 0)
			value = AF_UNSPEC;
		else
			fatal("Unsupported AddressFamily \"%s\"", arg);
		if (*activep && *intptr == -1)
			*intptr = value;
		break;

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
		arg = strdelim(&s);
		if (!arg || *arg == '\0')
			fatal("%.200s line %d: Missing ControlMaster argument.",
			    filename, linenum);
		value = 0;	/* To avoid compiler warning... */
		if (strcmp(arg, "yes") == 0 || strcmp(arg, "true") == 0)
			value = SSHCTL_MASTER_YES;
		else if (strcmp(arg, "no") == 0 || strcmp(arg, "false") == 0)
			value = SSHCTL_MASTER_NO;
		else if (strcmp(arg, "auto") == 0)
			value = SSHCTL_MASTER_AUTO;
		else if (strcmp(arg, "ask") == 0)
			value = SSHCTL_MASTER_ASK;
		else if (strcmp(arg, "autoask") == 0)
			value = SSHCTL_MASTER_AUTO_ASK;
		else
			fatal("%.200s line %d: Bad ControlMaster argument.",
			    filename, linenum);
		if (*activep && *intptr == -1)
			*intptr = value;
		break;

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
		arg = strdelim(&s);
		if (!arg || *arg == '\0')
			fatal("%s line %d: Missing yes/point-to-point/"
			    "ethernet/no argument.", filename, linenum);
		value = 0;	/* silence compiler */
		if (strcasecmp(arg, "ethernet") == 0)
			value = SSH_TUNMODE_ETHERNET;
		else if (strcasecmp(arg, "point-to-point") == 0)
			value = SSH_TUNMODE_POINTOPOINT;
		else if (strcasecmp(arg, "yes") == 0)
			value = SSH_TUNMODE_DEFAULT;
		else if (strcasecmp(arg, "no") == 0)
			value = SSH_TUNMODE_NO;
		else
			fatal("%s line %d: Bad yes/point-to-point/ethernet/"
			    "no argument: %s", filename, linenum, arg);
		if (*activep)
			*intptr = value;
		break;

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

	case oVersionAddendum:
		ssh_version_set_addendum(strtok(s, "\n"));
		do {
			arg = strdelim(&s);
		} while (arg != NULL && *arg != '\0');
		break;

	case oDeprecated:
		debug("%s line %d: Deprecated option \"%s\"",
		    filename, linenum, keyword);
		return 0;

	case oUnsupported:
		error("%s line %d: Unsupported option \"%s\"",
		    filename, linenum, keyword);
		return 0;

	default:
		fatal("process_config_line: Unimplemented opcode %d", opcode);
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
read_config_file(const char *filename, const char *host, Options *options,
    int checkperm)
{
	FILE *f;
	char line[1024];
	int active, linenum;
	int bad_options = 0;

	if ((f = fopen(filename, "r")) == NULL)
		return 0;

	if (checkperm) {
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
		if (process_config_line(options, host, line, filename, linenum, &active) != 0)
			bad_options++;
	}
	fclose(f);
	if (bad_options > 0)
		fatal("%s: terminating, %d bad configuration options",
		    filename, bad_options);
	return 1;
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
	options->forward_agent = -1;
	options->forward_x11 = -1;
	options->forward_x11_trusted = -1;
	options->forward_x11_timeout = -1;
	options->exit_on_forward_failure = -1;
	options->xauth_location = NULL;
	options->gateway_ports = -1;
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
	options->system_hostfile = NULL;
	options->user_hostfile = NULL;
	options->system_hostfile2 = NULL;
	options->user_hostfile2 = NULL;
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
	options->use_roaming = -1;
	options->visual_host_key = -1;
	options->zero_knowledge_password_authentication = -1;
	options->ip_qos_interactive = -1;
	options->ip_qos_bulk = -1;
}

/*
 * Called after processing other sources of option data, this fills those
 * options for which no value has been specified with their default values.
 */

void
fill_default_options(Options * options)
{
	int len;

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
	if (options->gateway_ports == -1)
		options->gateway_ports = 0;
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
	/* options->ciphers, default set in myproposals.h */
	/* options->macs, default set in myproposals.h */
	/* options->kex_algorithms, default set in myproposals.h */
	/* options->hostkeyalgorithms, default set in myproposals.h */
	if (options->protocol == SSH_PROTO_UNKNOWN)
		options->protocol = SSH_PROTO_2;
	if (options->num_identity_files == 0) {
		if (options->protocol & SSH_PROTO_1) {
			len = 2 + strlen(_PATH_SSH_CLIENT_IDENTITY) + 1;
			options->identity_files[options->num_identity_files] =
			    xmalloc(len);
			snprintf(options->identity_files[options->num_identity_files++],
			    len, "~/%.100s", _PATH_SSH_CLIENT_IDENTITY);
		}
		if (options->protocol & SSH_PROTO_2) {
			len = 2 + strlen(_PATH_SSH_CLIENT_ID_RSA) + 1;
			options->identity_files[options->num_identity_files] =
			    xmalloc(len);
			snprintf(options->identity_files[options->num_identity_files++],
			    len, "~/%.100s", _PATH_SSH_CLIENT_ID_RSA);

			len = 2 + strlen(_PATH_SSH_CLIENT_ID_DSA) + 1;
			options->identity_files[options->num_identity_files] =
			    xmalloc(len);
			snprintf(options->identity_files[options->num_identity_files++],
			    len, "~/%.100s", _PATH_SSH_CLIENT_ID_DSA);
#ifdef OPENSSL_HAS_ECC
			len = 2 + strlen(_PATH_SSH_CLIENT_ID_ECDSA) + 1;
			options->identity_files[options->num_identity_files] =
			    xmalloc(len);
			snprintf(options->identity_files[options->num_identity_files++],
			    len, "~/%.100s", _PATH_SSH_CLIENT_ID_ECDSA);
#endif
		}
	}
	if (options->escape_char == -1)
		options->escape_char = '~';
	if (options->system_hostfile == NULL)
		options->system_hostfile = _PATH_SSH_SYSTEM_HOSTFILE;
	if (options->user_hostfile == NULL)
		options->user_hostfile = _PATH_SSH_USER_HOSTFILE;
	if (options->system_hostfile2 == NULL)
		options->system_hostfile2 = _PATH_SSH_SYSTEM_HOSTFILE2;
	if (options->user_hostfile2 == NULL)
		options->user_hostfile2 = _PATH_SSH_USER_HOSTFILE2;
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
	if (options->verify_host_key_dns == -1)
		options->verify_host_key_dns = 0;
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
	if (options->use_roaming == -1)
		options->use_roaming = 1;
	if (options->visual_host_key == -1)
		options->visual_host_key = 0;
	if (options->zero_knowledge_password_authentication == -1)
		options->zero_knowledge_password_authentication = 0;
	if (options->ip_qos_interactive == -1)
		options->ip_qos_interactive = IPTOS_LOWDELAY;
	if (options->ip_qos_bulk == -1)
		options->ip_qos_bulk = IPTOS_THROUGHPUT;
	/* options->local_command should not be set by default */
	/* options->proxy_command should not be set by default */
	/* options->user will be set in the main program if appropriate */
	/* options->hostname will be set in the main program if appropriate */
	/* options->host_key_alias should not be set by default */
	/* options->preferred_authentications will be set in ssh */
}

/*
 * parse_forward
 * parses a string containing a port forwarding specification of the form:
 *   dynamicfwd == 0
 *	[listenhost:]listenport:connecthost:connectport
 *   dynamicfwd == 1
 *	[listenhost:]listenport
 * returns number of arguments parsed or zero on error
 */
int
parse_forward(Forward *fwd, const char *fwdspec, int dynamicfwd, int remotefwd)
{
	int i;
	char *p, *cp, *fwdarg[4];

	memset(fwd, '\0', sizeof(*fwd));

	cp = p = xstrdup(fwdspec);

	/* skip leading spaces */
	while (isspace(*cp))
		cp++;

	for (i = 0; i < 4; ++i)
		if ((fwdarg[i] = hpdelim(&cp)) == NULL)
			break;

	/* Check for trailing garbage */
	if (cp != NULL)
		i = 0;	/* failure */

	switch (i) {
	case 1:
		fwd->listen_host = NULL;
		fwd->listen_port = a2port(fwdarg[0]);
		fwd->connect_host = xstrdup("socks");
		break;

	case 2:
		fwd->listen_host = xstrdup(cleanhostname(fwdarg[0]));
		fwd->listen_port = a2port(fwdarg[1]);
		fwd->connect_host = xstrdup("socks");
		break;

	case 3:
		fwd->listen_host = NULL;
		fwd->listen_port = a2port(fwdarg[0]);
		fwd->connect_host = xstrdup(cleanhostname(fwdarg[1]));
		fwd->connect_port = a2port(fwdarg[2]);
		break;

	case 4:
		fwd->listen_host = xstrdup(cleanhostname(fwdarg[0]));
		fwd->listen_port = a2port(fwdarg[1]);
		fwd->connect_host = xstrdup(cleanhostname(fwdarg[2]));
		fwd->connect_port = a2port(fwdarg[3]);
		break;
	default:
		i = 0; /* failure */
	}

	xfree(p);

	if (dynamicfwd) {
		if (!(i == 1 || i == 2))
			goto fail_free;
	} else {
		if (!(i == 3 || i == 4))
			goto fail_free;
		if (fwd->connect_port <= 0)
			goto fail_free;
	}

	if (fwd->listen_port < 0 || (!remotefwd && fwd->listen_port == 0))
		goto fail_free;

	if (fwd->connect_host != NULL &&
	    strlen(fwd->connect_host) >= NI_MAXHOST)
		goto fail_free;
	if (fwd->listen_host != NULL &&
	    strlen(fwd->listen_host) >= NI_MAXHOST)
		goto fail_free;


	return (i);

 fail_free:
	if (fwd->connect_host != NULL) {
		xfree(fwd->connect_host);
		fwd->connect_host = NULL;
	}
	if (fwd->listen_host != NULL) {
		xfree(fwd->listen_host);
		fwd->listen_host = NULL;
	}
	return (0);
}
