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
RCSID("$FreeBSD$");
RCSID("$OpenBSD: readconf.c,v 1.127 2003/12/16 15:49:51 markus Exp $");

#include "ssh.h"
#include "xmalloc.h"
#include "compat.h"
#include "cipher.h"
#include "pathnames.h"
#include "log.h"
#include "readconf.h"
#include "match.h"
#include "misc.h"
#include "kex.h"
#include "mac.h"

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
	oForwardAgent, oForwardX11, oForwardX11Trusted, oGatewayPorts,
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
	oHostKeyAlgorithms, oBindAddress, oSmartcardDevice,
	oClearAllForwardings, oNoHostAuthenticationForLocalhost,
	oEnableSSHKeysign, oRekeyLimit, oVerifyHostKeyDNS, oConnectTimeout,
	oAddressFamily, oGssAuthentication, oGssDelegateCreds,
	oServerAliveInterval, oServerAliveCountMax, oIdentitiesOnly,
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
	{ "identityfile2", oIdentityFile },			/* alias */
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
	{ "userknownhostsfile", oUserKnownHostsFile },		/* obsolete */
	{ "globalknownhostsfile2", oGlobalKnownHostsFile2 },
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
#ifdef SMARTCARD
	{ "smartcarddevice", oSmartcardDevice },
#else
	{ "smartcarddevice", oUnsupported },
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
	{ "versionaddendum", oVersionAddendum },
	{ NULL, oBadOption }
};

/*
 * Adds a local TCP/IP port forward to options.  Never returns if there is an
 * error.
 */

void
add_local_forward(Options *options, u_short port, const char *host,
		  u_short host_port)
{
	Forward *fwd;
#ifndef NO_IPPORT_RESERVED_CONCEPT
	extern uid_t original_real_uid;
	if (port < IPPORT_RESERVED && original_real_uid != 0)
		fatal("Privileged ports can only be forwarded by root.");
#endif
	if (options->num_local_forwards >= SSH_MAX_FORWARDS_PER_DIRECTION)
		fatal("Too many local forwards (max %d).", SSH_MAX_FORWARDS_PER_DIRECTION);
	fwd = &options->local_forwards[options->num_local_forwards++];
	fwd->port = port;
	fwd->host = xstrdup(host);
	fwd->host_port = host_port;
}

/*
 * Adds a remote TCP/IP port forward to options.  Never returns if there is
 * an error.
 */

void
add_remote_forward(Options *options, u_short port, const char *host,
		   u_short host_port)
{
	Forward *fwd;
	if (options->num_remote_forwards >= SSH_MAX_FORWARDS_PER_DIRECTION)
		fatal("Too many remote forwards (max %d).",
		    SSH_MAX_FORWARDS_PER_DIRECTION);
	fwd = &options->remote_forwards[options->num_remote_forwards++];
	fwd->port = port;
	fwd->host = xstrdup(host);
	fwd->host_port = host_port;
}

static void
clear_forwardings(Options *options)
{
	int i;

	for (i = 0; i < options->num_local_forwards; i++)
		xfree(options->local_forwards[i].host);
	options->num_local_forwards = 0;
	for (i = 0; i < options->num_remote_forwards; i++)
		xfree(options->remote_forwards[i].host);
	options->num_remote_forwards = 0;
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
	char buf[256], *s, **charptr, *endofnumber, *keyword, *arg;
	int opcode, *intptr, value;
	size_t len;
	u_short fwd_port, fwd_host_port;
	char sfwd_host_port[6];

	/* Strip trailing whitespace */
	for(len = strlen(line) - 1; len > 0; len--) {
		if (strchr(WHITESPACE, line[len]) == NULL)
			break;
		line[len] = '\0';
	}

	s = line;
	/* Get the keyword. (Each line is supposed to begin with a keyword). */
	keyword = strdelim(&s);
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
		if (*intptr == -1)
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

	case oGatewayPorts:
		intptr = &options->gateway_ports;
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
		intptr = &options->rekey_limit;
		arg = strdelim(&s);
		if (!arg || *arg == '\0')
			fatal("%.200s line %d: Missing argument.", filename, linenum);
		if (arg[0] < '0' || arg[0] > '9')
			fatal("%.200s line %d: Bad number.", filename, linenum);
		value = strtol(arg, &endofnumber, 10);
		if (arg == endofnumber)
			fatal("%.200s line %d: Bad number.", filename, linenum);
		switch (toupper(*endofnumber)) {
		case 'K':
			value *= 1<<10;
			break;
		case 'M':
			value *= 1<<20;
			break;
		case 'G':
			value *= 1<<30;
			break;
		}
		if (*activep && *intptr == -1)
			*intptr = value;
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
			charptr =  &options->identity_files[*intptr];
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

	case oSmartcardDevice:
		charptr = &options->smartcard_device;
		goto parse_string;

	case oProxyCommand:
		if (s == NULL)
			fatal("%.200s line %d: Missing argument.", filename, linenum);
		charptr = &options->proxy_command;
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
		intptr = (int *) &options->log_level;
		arg = strdelim(&s);
		value = log_level_number(arg);
		if (value == SYSLOG_LEVEL_NOT_SET)
			fatal("%.200s line %d: unsupported log level '%s'",
			    filename, linenum, arg ? arg : "<NONE>");
		if (*activep && (LogLevel) *intptr == SYSLOG_LEVEL_NOT_SET)
			*intptr = (LogLevel) value;
		break;

	case oLocalForward:
	case oRemoteForward:
		arg = strdelim(&s);
		if (!arg || *arg == '\0')
			fatal("%.200s line %d: Missing port argument.",
			    filename, linenum);
		if ((fwd_port = a2port(arg)) == 0)
			fatal("%.200s line %d: Bad listen port.",
			    filename, linenum);
		arg = strdelim(&s);
		if (!arg || *arg == '\0')
			fatal("%.200s line %d: Missing second argument.",
			    filename, linenum);
		if (sscanf(arg, "%255[^:]:%5[0-9]", buf, sfwd_host_port) != 2 &&
		    sscanf(arg, "%255[^/]/%5[0-9]", buf, sfwd_host_port) != 2)
			fatal("%.200s line %d: Bad forwarding specification.",
			    filename, linenum);
		if ((fwd_host_port = a2port(sfwd_host_port)) == 0)
			fatal("%.200s line %d: Bad forwarding port.",
			    filename, linenum);
		if (*activep) {
			if (opcode == oLocalForward)
				add_local_forward(options, fwd_port, buf,
				    fwd_host_port);
			else if (opcode == oRemoteForward)
				add_remote_forward(options, fwd_port, buf,
				    fwd_host_port);
		}
		break;

	case oDynamicForward:
		arg = strdelim(&s);
		if (!arg || *arg == '\0')
			fatal("%.200s line %d: Missing port argument.",
			    filename, linenum);
		fwd_port = a2port(arg);
		if (fwd_port == 0)
			fatal("%.200s line %d: Badly formatted port number.",
			    filename, linenum);
		if (*activep)
			add_local_forward(options, fwd_port, "socks", 0);
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
read_config_file(const char *filename, const char *host, Options *options)
{
	FILE *f;
	char line[1024];
	int active, linenum;
	int bad_options = 0;

	/* Open the file. */
	f = fopen(filename, "r");
	if (!f)
		return 0;

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
	options->num_local_forwards = 0;
	options->num_remote_forwards = 0;
	options->clear_forwardings = -1;
	options->log_level = SYSLOG_LEVEL_NOT_SET;
	options->preferred_authentications = NULL;
	options->bind_address = NULL;
	options->smartcard_device = NULL;
	options->enable_ssh_keysign = - 1;
	options->no_host_authentication_for_localhost = - 1;
	options->identities_only = - 1;
	options->rekey_limit = - 1;
	options->verify_host_key_dns = -1;
	options->server_alive_interval = -1;
	options->server_alive_count_max = -1;
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
	/* options->hostkeyalgorithms, default set in myproposals.h */
	if (options->protocol == SSH_PROTO_UNKNOWN)
		options->protocol = SSH_PROTO_1|SSH_PROTO_2;
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
	/* options->proxy_command should not be set by default */
	/* options->user will be set in the main program if appropriate */
	/* options->hostname will be set in the main program if appropriate */
	/* options->host_key_alias should not be set by default */
	/* options->preferred_authentications will be set in ssh */
}
