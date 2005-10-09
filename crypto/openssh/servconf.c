/*
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 */

#include "includes.h"
RCSID("$OpenBSD: servconf.c,v 1.144 2005/08/06 10:03:12 dtucker Exp $");
RCSID("$FreeBSD$");

#include "ssh.h"
#include "log.h"
#include "servconf.h"
#include "xmalloc.h"
#include "compat.h"
#include "pathnames.h"
#include "misc.h"
#include "cipher.h"
#include "kex.h"
#include "mac.h"

static void add_listen_addr(ServerOptions *, char *, u_short);
static void add_one_listen_addr(ServerOptions *, char *, u_short);

/* Use of privilege separation or not */
extern int use_privsep;

/* Initializes the server options to their default values. */

void
initialize_server_options(ServerOptions *options)
{
	memset(options, 0, sizeof(*options));

	/* Portable-specific options */
	options->use_pam = -1;

	/* Standard Options */
	options->num_ports = 0;
	options->ports_from_cmdline = 0;
	options->listen_addrs = NULL;
	options->address_family = -1;
	options->num_host_key_files = 0;
	options->pid_file = NULL;
	options->server_key_bits = -1;
	options->login_grace_time = -1;
	options->key_regeneration_time = -1;
	options->permit_root_login = PERMIT_NOT_SET;
	options->ignore_rhosts = -1;
	options->ignore_user_known_hosts = -1;
	options->print_motd = -1;
	options->print_lastlog = -1;
	options->x11_forwarding = -1;
	options->x11_display_offset = -1;
	options->x11_use_localhost = -1;
	options->xauth_location = NULL;
	options->strict_modes = -1;
	options->tcp_keep_alive = -1;
	options->log_facility = SYSLOG_FACILITY_NOT_SET;
	options->log_level = SYSLOG_LEVEL_NOT_SET;
	options->rhosts_rsa_authentication = -1;
	options->hostbased_authentication = -1;
	options->hostbased_uses_name_from_packet_only = -1;
	options->rsa_authentication = -1;
	options->pubkey_authentication = -1;
	options->kerberos_authentication = -1;
	options->kerberos_or_local_passwd = -1;
	options->kerberos_ticket_cleanup = -1;
	options->kerberos_get_afs_token = -1;
	options->gss_authentication=-1;
	options->gss_cleanup_creds = -1;
	options->password_authentication = -1;
	options->kbd_interactive_authentication = -1;
	options->challenge_response_authentication = -1;
	options->permit_empty_passwd = -1;
	options->permit_user_env = -1;
	options->use_login = -1;
	options->compression = -1;
	options->allow_tcp_forwarding = -1;
	options->num_allow_users = 0;
	options->num_deny_users = 0;
	options->num_allow_groups = 0;
	options->num_deny_groups = 0;
	options->ciphers = NULL;
	options->macs = NULL;
	options->protocol = SSH_PROTO_UNKNOWN;
	options->gateway_ports = -1;
	options->num_subsystems = 0;
	options->max_startups_begin = -1;
	options->max_startups_rate = -1;
	options->max_startups = -1;
	options->max_authtries = -1;
	options->banner = NULL;
	options->use_dns = -1;
	options->client_alive_interval = -1;
	options->client_alive_count_max = -1;
	options->authorized_keys_file = NULL;
	options->authorized_keys_file2 = NULL;
	options->num_accept_env = 0;

	/* Needs to be accessable in many places */
	use_privsep = -1;
}

void
fill_default_server_options(ServerOptions *options)
{
	/* Portable-specific options */
	if (options->use_pam == -1)
		options->use_pam = 1;

	/* Standard Options */
	if (options->protocol == SSH_PROTO_UNKNOWN)
		options->protocol = SSH_PROTO_2;
	if (options->num_host_key_files == 0) {
		/* fill default hostkeys for protocols */
		if (options->protocol & SSH_PROTO_1)
			options->host_key_files[options->num_host_key_files++] =
			    _PATH_HOST_KEY_FILE;
		if (options->protocol & SSH_PROTO_2) {
			options->host_key_files[options->num_host_key_files++] =
			    _PATH_HOST_DSA_KEY_FILE;
		}
	}
	if (options->num_ports == 0)
		options->ports[options->num_ports++] = SSH_DEFAULT_PORT;
	if (options->listen_addrs == NULL)
		add_listen_addr(options, NULL, 0);
	if (options->pid_file == NULL)
		options->pid_file = _PATH_SSH_DAEMON_PID_FILE;
	if (options->server_key_bits == -1)
		options->server_key_bits = 768;
	if (options->login_grace_time == -1)
		options->login_grace_time = 120;
	if (options->key_regeneration_time == -1)
		options->key_regeneration_time = 3600;
	if (options->permit_root_login == PERMIT_NOT_SET)
		options->permit_root_login = PERMIT_NO;
	if (options->ignore_rhosts == -1)
		options->ignore_rhosts = 1;
	if (options->ignore_user_known_hosts == -1)
		options->ignore_user_known_hosts = 0;
	if (options->print_motd == -1)
		options->print_motd = 1;
	if (options->print_lastlog == -1)
		options->print_lastlog = 1;
	if (options->x11_forwarding == -1)
		options->x11_forwarding = 1;
	if (options->x11_display_offset == -1)
		options->x11_display_offset = 10;
	if (options->x11_use_localhost == -1)
		options->x11_use_localhost = 1;
	if (options->xauth_location == NULL)
		options->xauth_location = _PATH_XAUTH;
	if (options->strict_modes == -1)
		options->strict_modes = 1;
	if (options->tcp_keep_alive == -1)
		options->tcp_keep_alive = 1;
	if (options->log_facility == SYSLOG_FACILITY_NOT_SET)
		options->log_facility = SYSLOG_FACILITY_AUTH;
	if (options->log_level == SYSLOG_LEVEL_NOT_SET)
		options->log_level = SYSLOG_LEVEL_INFO;
	if (options->rhosts_rsa_authentication == -1)
		options->rhosts_rsa_authentication = 0;
	if (options->hostbased_authentication == -1)
		options->hostbased_authentication = 0;
	if (options->hostbased_uses_name_from_packet_only == -1)
		options->hostbased_uses_name_from_packet_only = 0;
	if (options->rsa_authentication == -1)
		options->rsa_authentication = 1;
	if (options->pubkey_authentication == -1)
		options->pubkey_authentication = 1;
	if (options->kerberos_authentication == -1)
		options->kerberos_authentication = 0;
	if (options->kerberos_or_local_passwd == -1)
		options->kerberos_or_local_passwd = 1;
	if (options->kerberos_ticket_cleanup == -1)
		options->kerberos_ticket_cleanup = 1;
	if (options->kerberos_get_afs_token == -1)
		options->kerberos_get_afs_token = 0;
	if (options->gss_authentication == -1)
		options->gss_authentication = 0;
	if (options->gss_cleanup_creds == -1)
		options->gss_cleanup_creds = 1;
	if (options->password_authentication == -1)
#ifdef USE_PAM
		options->password_authentication = 0;
#else
		options->password_authentication = 1;
#endif
	if (options->kbd_interactive_authentication == -1)
		options->kbd_interactive_authentication = 0;
	if (options->challenge_response_authentication == -1)
		options->challenge_response_authentication = 1;
	if (options->permit_empty_passwd == -1)
		options->permit_empty_passwd = 0;
	if (options->permit_user_env == -1)
		options->permit_user_env = 0;
	if (options->use_login == -1)
		options->use_login = 0;
	if (options->compression == -1)
		options->compression = COMP_DELAYED;
	if (options->allow_tcp_forwarding == -1)
		options->allow_tcp_forwarding = 1;
	if (options->gateway_ports == -1)
		options->gateway_ports = 0;
	if (options->max_startups == -1)
		options->max_startups = 10;
	if (options->max_startups_rate == -1)
		options->max_startups_rate = 100;		/* 100% */
	if (options->max_startups_begin == -1)
		options->max_startups_begin = options->max_startups;
	if (options->max_authtries == -1)
		options->max_authtries = DEFAULT_AUTH_FAIL_MAX;
	if (options->use_dns == -1)
		options->use_dns = 1;
	if (options->client_alive_interval == -1)
		options->client_alive_interval = 0;
	if (options->client_alive_count_max == -1)
		options->client_alive_count_max = 3;
	if (options->authorized_keys_file2 == NULL) {
		/* authorized_keys_file2 falls back to authorized_keys_file */
		if (options->authorized_keys_file != NULL)
			options->authorized_keys_file2 = options->authorized_keys_file;
		else
			options->authorized_keys_file2 = _PATH_SSH_USER_PERMITTED_KEYS2;
	}
	if (options->authorized_keys_file == NULL)
		options->authorized_keys_file = _PATH_SSH_USER_PERMITTED_KEYS;

	/* Turn privilege separation on by default */
	if (use_privsep == -1)
		use_privsep = 1;

#ifndef HAVE_MMAP
	if (use_privsep && options->compression == 1) {
		error("This platform does not support both privilege "
		    "separation and compression");
		error("Compression disabled");
		options->compression = 0;
	}
#endif

}

/* Keyword tokens. */
typedef enum {
	sBadOption,		/* == unknown option */
	/* Portable-specific options */
	sUsePAM,
	/* Standard Options */
	sPort, sHostKeyFile, sServerKeyBits, sLoginGraceTime, sKeyRegenerationTime,
	sPermitRootLogin, sLogFacility, sLogLevel,
	sRhostsRSAAuthentication, sRSAAuthentication,
	sKerberosAuthentication, sKerberosOrLocalPasswd, sKerberosTicketCleanup,
	sKerberosGetAFSToken,
	sKerberosTgtPassing, sChallengeResponseAuthentication,
	sPasswordAuthentication, sKbdInteractiveAuthentication,
	sListenAddress, sAddressFamily,
	sPrintMotd, sPrintLastLog, sIgnoreRhosts,
	sX11Forwarding, sX11DisplayOffset, sX11UseLocalhost,
	sStrictModes, sEmptyPasswd, sTCPKeepAlive,
	sPermitUserEnvironment, sUseLogin, sAllowTcpForwarding, sCompression,
	sAllowUsers, sDenyUsers, sAllowGroups, sDenyGroups,
	sIgnoreUserKnownHosts, sCiphers, sMacs, sProtocol, sPidFile,
	sGatewayPorts, sPubkeyAuthentication, sXAuthLocation, sSubsystem,
	sMaxStartups, sMaxAuthTries,
	sBanner, sUseDNS, sHostbasedAuthentication,
	sHostbasedUsesNameFromPacketOnly, sClientAliveInterval,
	sClientAliveCountMax, sAuthorizedKeysFile, sAuthorizedKeysFile2,
	sGssAuthentication, sGssCleanupCreds, sAcceptEnv,
	sUsePrivilegeSeparation,
	sVersionAddendum,
	sDeprecated, sUnsupported
} ServerOpCodes;

/* Textual representation of the tokens. */
static struct {
	const char *name;
	ServerOpCodes opcode;
} keywords[] = {
	/* Portable-specific options */
#ifdef USE_PAM
	{ "usepam", sUsePAM },
#else
	{ "usepam", sUnsupported },
#endif
	{ "pamauthenticationviakbdint", sDeprecated },
	/* Standard Options */
	{ "port", sPort },
	{ "hostkey", sHostKeyFile },
	{ "hostdsakey", sHostKeyFile },					/* alias */
	{ "pidfile", sPidFile },
	{ "serverkeybits", sServerKeyBits },
	{ "logingracetime", sLoginGraceTime },
	{ "keyregenerationinterval", sKeyRegenerationTime },
	{ "permitrootlogin", sPermitRootLogin },
	{ "syslogfacility", sLogFacility },
	{ "loglevel", sLogLevel },
	{ "rhostsauthentication", sDeprecated },
	{ "rhostsrsaauthentication", sRhostsRSAAuthentication },
	{ "hostbasedauthentication", sHostbasedAuthentication },
	{ "hostbasedusesnamefrompacketonly", sHostbasedUsesNameFromPacketOnly },
	{ "rsaauthentication", sRSAAuthentication },
	{ "pubkeyauthentication", sPubkeyAuthentication },
	{ "dsaauthentication", sPubkeyAuthentication },			/* alias */
#ifdef KRB5
	{ "kerberosauthentication", sKerberosAuthentication },
	{ "kerberosorlocalpasswd", sKerberosOrLocalPasswd },
	{ "kerberosticketcleanup", sKerberosTicketCleanup },
#ifdef USE_AFS
	{ "kerberosgetafstoken", sKerberosGetAFSToken },
#else
	{ "kerberosgetafstoken", sUnsupported },
#endif
#else
	{ "kerberosauthentication", sUnsupported },
	{ "kerberosorlocalpasswd", sUnsupported },
	{ "kerberosticketcleanup", sUnsupported },
	{ "kerberosgetafstoken", sUnsupported },
#endif
	{ "kerberostgtpassing", sUnsupported },
	{ "afstokenpassing", sUnsupported },
#ifdef GSSAPI
	{ "gssapiauthentication", sGssAuthentication },
	{ "gssapicleanupcredentials", sGssCleanupCreds },
#else
	{ "gssapiauthentication", sUnsupported },
	{ "gssapicleanupcredentials", sUnsupported },
#endif
	{ "passwordauthentication", sPasswordAuthentication },
	{ "kbdinteractiveauthentication", sKbdInteractiveAuthentication },
	{ "challengeresponseauthentication", sChallengeResponseAuthentication },
	{ "skeyauthentication", sChallengeResponseAuthentication }, /* alias */
	{ "checkmail", sDeprecated },
	{ "listenaddress", sListenAddress },
	{ "addressfamily", sAddressFamily },
	{ "printmotd", sPrintMotd },
	{ "printlastlog", sPrintLastLog },
	{ "ignorerhosts", sIgnoreRhosts },
	{ "ignoreuserknownhosts", sIgnoreUserKnownHosts },
	{ "x11forwarding", sX11Forwarding },
	{ "x11displayoffset", sX11DisplayOffset },
	{ "x11uselocalhost", sX11UseLocalhost },
	{ "xauthlocation", sXAuthLocation },
	{ "strictmodes", sStrictModes },
	{ "permitemptypasswords", sEmptyPasswd },
	{ "permituserenvironment", sPermitUserEnvironment },
	{ "uselogin", sUseLogin },
	{ "compression", sCompression },
	{ "tcpkeepalive", sTCPKeepAlive },
	{ "keepalive", sTCPKeepAlive },				/* obsolete alias */
	{ "allowtcpforwarding", sAllowTcpForwarding },
	{ "allowusers", sAllowUsers },
	{ "denyusers", sDenyUsers },
	{ "allowgroups", sAllowGroups },
	{ "denygroups", sDenyGroups },
	{ "ciphers", sCiphers },
	{ "macs", sMacs },
	{ "protocol", sProtocol },
	{ "gatewayports", sGatewayPorts },
	{ "subsystem", sSubsystem },
	{ "maxstartups", sMaxStartups },
	{ "maxauthtries", sMaxAuthTries },
	{ "banner", sBanner },
	{ "usedns", sUseDNS },
	{ "verifyreversemapping", sDeprecated },
	{ "reversemappingcheck", sDeprecated },
	{ "clientaliveinterval", sClientAliveInterval },
	{ "clientalivecountmax", sClientAliveCountMax },
	{ "authorizedkeysfile", sAuthorizedKeysFile },
	{ "authorizedkeysfile2", sAuthorizedKeysFile2 },
	{ "useprivilegeseparation", sUsePrivilegeSeparation},
	{ "acceptenv", sAcceptEnv },
	{ "versionaddendum", sVersionAddendum },
	{ NULL, sBadOption }
};

/*
 * Returns the number of the token pointed to by cp or sBadOption.
 */

static ServerOpCodes
parse_token(const char *cp, const char *filename,
	    int linenum)
{
	u_int i;

	for (i = 0; keywords[i].name; i++)
		if (strcasecmp(cp, keywords[i].name) == 0)
			return keywords[i].opcode;

	error("%s: line %d: Bad configuration option: %s",
	    filename, linenum, cp);
	return sBadOption;
}

static void
add_listen_addr(ServerOptions *options, char *addr, u_short port)
{
	u_int i;

	if (options->num_ports == 0)
		options->ports[options->num_ports++] = SSH_DEFAULT_PORT;
	if (options->address_family == -1)
		options->address_family = AF_UNSPEC;
	if (port == 0)
		for (i = 0; i < options->num_ports; i++)
			add_one_listen_addr(options, addr, options->ports[i]);
	else
		add_one_listen_addr(options, addr, port);
}

static void
add_one_listen_addr(ServerOptions *options, char *addr, u_short port)
{
	struct addrinfo hints, *ai, *aitop;
	char strport[NI_MAXSERV];
	int gaierr;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = options->address_family;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = (addr == NULL) ? AI_PASSIVE : 0;
	snprintf(strport, sizeof strport, "%u", port);
	if ((gaierr = getaddrinfo(addr, strport, &hints, &aitop)) != 0)
		fatal("bad addr or host: %s (%s)",
		    addr ? addr : "<NULL>",
		    gai_strerror(gaierr));
	for (ai = aitop; ai->ai_next; ai = ai->ai_next)
		;
	ai->ai_next = options->listen_addrs;
	options->listen_addrs = aitop;
}

int
process_server_config_line(ServerOptions *options, char *line,
    const char *filename, int linenum)
{
	char *cp, **charptr, *arg, *p;
	int *intptr, value, n;
	ServerOpCodes opcode;
	u_short port;
	u_int i;

	cp = line;
	arg = strdelim(&cp);
	/* Ignore leading whitespace */
	if (*arg == '\0')
		arg = strdelim(&cp);
	if (!arg || !*arg || *arg == '#')
		return 0;
	intptr = NULL;
	charptr = NULL;
	opcode = parse_token(arg, filename, linenum);
	switch (opcode) {
	/* Portable-specific options */
	case sUsePAM:
		intptr = &options->use_pam;
		goto parse_flag;

	/* Standard Options */
	case sBadOption:
		return -1;
	case sPort:
		/* ignore ports from configfile if cmdline specifies ports */
		if (options->ports_from_cmdline)
			return 0;
		if (options->listen_addrs != NULL)
			fatal("%s line %d: ports must be specified before "
			    "ListenAddress.", filename, linenum);
		if (options->num_ports >= MAX_PORTS)
			fatal("%s line %d: too many ports.",
			    filename, linenum);
		arg = strdelim(&cp);
		if (!arg || *arg == '\0')
			fatal("%s line %d: missing port number.",
			    filename, linenum);
		options->ports[options->num_ports++] = a2port(arg);
		if (options->ports[options->num_ports-1] == 0)
			fatal("%s line %d: Badly formatted port number.",
			    filename, linenum);
		break;

	case sServerKeyBits:
		intptr = &options->server_key_bits;
parse_int:
		arg = strdelim(&cp);
		if (!arg || *arg == '\0')
			fatal("%s line %d: missing integer value.",
			    filename, linenum);
		value = atoi(arg);
		if (*intptr == -1)
			*intptr = value;
		break;

	case sLoginGraceTime:
		intptr = &options->login_grace_time;
parse_time:
		arg = strdelim(&cp);
		if (!arg || *arg == '\0')
			fatal("%s line %d: missing time value.",
			    filename, linenum);
		if ((value = convtime(arg)) == -1)
			fatal("%s line %d: invalid time value.",
			    filename, linenum);
		if (*intptr == -1)
			*intptr = value;
		break;

	case sKeyRegenerationTime:
		intptr = &options->key_regeneration_time;
		goto parse_time;

	case sListenAddress:
		arg = strdelim(&cp);
		if (arg == NULL || *arg == '\0')
			fatal("%s line %d: missing address",
			    filename, linenum);
		/* check for bare IPv6 address: no "[]" and 2 or more ":" */
		if (strchr(arg, '[') == NULL && (p = strchr(arg, ':')) != NULL
		    && strchr(p+1, ':') != NULL) {
			add_listen_addr(options, arg, 0);
			break;
		}
		p = hpdelim(&arg);
		if (p == NULL)
			fatal("%s line %d: bad address:port usage",
			    filename, linenum);
		p = cleanhostname(p);
		if (arg == NULL)
			port = 0;
		else if ((port = a2port(arg)) == 0)
			fatal("%s line %d: bad port number", filename, linenum);

		add_listen_addr(options, p, port);

		break;

	case sAddressFamily:
		arg = strdelim(&cp);
		if (!arg || *arg == '\0')
			fatal("%s line %d: missing address family.",
			    filename, linenum);
		intptr = &options->address_family;
		if (options->listen_addrs != NULL)
			fatal("%s line %d: address family must be specified before "
			    "ListenAddress.", filename, linenum);
		if (strcasecmp(arg, "inet") == 0)
			value = AF_INET;
		else if (strcasecmp(arg, "inet6") == 0)
			value = AF_INET6;
		else if (strcasecmp(arg, "any") == 0)
			value = AF_UNSPEC;
		else
			fatal("%s line %d: unsupported address family \"%s\".",
			    filename, linenum, arg);
		if (*intptr == -1)
			*intptr = value;
		break;

	case sHostKeyFile:
		intptr = &options->num_host_key_files;
		if (*intptr >= MAX_HOSTKEYS)
			fatal("%s line %d: too many host keys specified (max %d).",
			    filename, linenum, MAX_HOSTKEYS);
		charptr = &options->host_key_files[*intptr];
parse_filename:
		arg = strdelim(&cp);
		if (!arg || *arg == '\0')
			fatal("%s line %d: missing file name.",
			    filename, linenum);
		if (*charptr == NULL) {
			*charptr = tilde_expand_filename(arg, getuid());
			/* increase optional counter */
			if (intptr != NULL)
				*intptr = *intptr + 1;
		}
		break;

	case sPidFile:
		charptr = &options->pid_file;
		goto parse_filename;

	case sPermitRootLogin:
		intptr = &options->permit_root_login;
		arg = strdelim(&cp);
		if (!arg || *arg == '\0')
			fatal("%s line %d: missing yes/"
			    "without-password/forced-commands-only/no "
			    "argument.", filename, linenum);
		value = 0;	/* silence compiler */
		if (strcmp(arg, "without-password") == 0)
			value = PERMIT_NO_PASSWD;
		else if (strcmp(arg, "forced-commands-only") == 0)
			value = PERMIT_FORCED_ONLY;
		else if (strcmp(arg, "yes") == 0)
			value = PERMIT_YES;
		else if (strcmp(arg, "no") == 0)
			value = PERMIT_NO;
		else
			fatal("%s line %d: Bad yes/"
			    "without-password/forced-commands-only/no "
			    "argument: %s", filename, linenum, arg);
		if (*intptr == -1)
			*intptr = value;
		break;

	case sIgnoreRhosts:
		intptr = &options->ignore_rhosts;
parse_flag:
		arg = strdelim(&cp);
		if (!arg || *arg == '\0')
			fatal("%s line %d: missing yes/no argument.",
			    filename, linenum);
		value = 0;	/* silence compiler */
		if (strcmp(arg, "yes") == 0)
			value = 1;
		else if (strcmp(arg, "no") == 0)
			value = 0;
		else
			fatal("%s line %d: Bad yes/no argument: %s",
				filename, linenum, arg);
		if (*intptr == -1)
			*intptr = value;
		break;

	case sIgnoreUserKnownHosts:
		intptr = &options->ignore_user_known_hosts;
		goto parse_flag;

	case sRhostsRSAAuthentication:
		intptr = &options->rhosts_rsa_authentication;
		goto parse_flag;

	case sHostbasedAuthentication:
		intptr = &options->hostbased_authentication;
		goto parse_flag;

	case sHostbasedUsesNameFromPacketOnly:
		intptr = &options->hostbased_uses_name_from_packet_only;
		goto parse_flag;

	case sRSAAuthentication:
		intptr = &options->rsa_authentication;
		goto parse_flag;

	case sPubkeyAuthentication:
		intptr = &options->pubkey_authentication;
		goto parse_flag;

	case sKerberosAuthentication:
		intptr = &options->kerberos_authentication;
		goto parse_flag;

	case sKerberosOrLocalPasswd:
		intptr = &options->kerberos_or_local_passwd;
		goto parse_flag;

	case sKerberosTicketCleanup:
		intptr = &options->kerberos_ticket_cleanup;
		goto parse_flag;

	case sKerberosGetAFSToken:
		intptr = &options->kerberos_get_afs_token;
		goto parse_flag;

	case sGssAuthentication:
		intptr = &options->gss_authentication;
		goto parse_flag;

	case sGssCleanupCreds:
		intptr = &options->gss_cleanup_creds;
		goto parse_flag;

	case sPasswordAuthentication:
		intptr = &options->password_authentication;
		goto parse_flag;

	case sKbdInteractiveAuthentication:
		intptr = &options->kbd_interactive_authentication;
		goto parse_flag;

	case sChallengeResponseAuthentication:
		intptr = &options->challenge_response_authentication;
		goto parse_flag;

	case sPrintMotd:
		intptr = &options->print_motd;
		goto parse_flag;

	case sPrintLastLog:
		intptr = &options->print_lastlog;
		goto parse_flag;

	case sX11Forwarding:
		intptr = &options->x11_forwarding;
		goto parse_flag;

	case sX11DisplayOffset:
		intptr = &options->x11_display_offset;
		goto parse_int;

	case sX11UseLocalhost:
		intptr = &options->x11_use_localhost;
		goto parse_flag;

	case sXAuthLocation:
		charptr = &options->xauth_location;
		goto parse_filename;

	case sStrictModes:
		intptr = &options->strict_modes;
		goto parse_flag;

	case sTCPKeepAlive:
		intptr = &options->tcp_keep_alive;
		goto parse_flag;

	case sEmptyPasswd:
		intptr = &options->permit_empty_passwd;
		goto parse_flag;

	case sPermitUserEnvironment:
		intptr = &options->permit_user_env;
		goto parse_flag;

	case sUseLogin:
		intptr = &options->use_login;
		goto parse_flag;

	case sCompression:
		intptr = &options->compression;
		arg = strdelim(&cp);
		if (!arg || *arg == '\0')
			fatal("%s line %d: missing yes/no/delayed "
			    "argument.", filename, linenum);
		value = 0;	/* silence compiler */
		if (strcmp(arg, "delayed") == 0)
			value = COMP_DELAYED;
		else if (strcmp(arg, "yes") == 0)
			value = COMP_ZLIB;
		else if (strcmp(arg, "no") == 0)
			value = COMP_NONE;
		else
			fatal("%s line %d: Bad yes/no/delayed "
			    "argument: %s", filename, linenum, arg);
		if (*intptr == -1)
			*intptr = value;
		break;

	case sGatewayPorts:
		intptr = &options->gateway_ports;
		arg = strdelim(&cp);
		if (!arg || *arg == '\0')
			fatal("%s line %d: missing yes/no/clientspecified "
			    "argument.", filename, linenum);
		value = 0;	/* silence compiler */
		if (strcmp(arg, "clientspecified") == 0)
			value = 2;
		else if (strcmp(arg, "yes") == 0)
			value = 1;
		else if (strcmp(arg, "no") == 0)
			value = 0;
		else
			fatal("%s line %d: Bad yes/no/clientspecified "
			    "argument: %s", filename, linenum, arg);
		if (*intptr == -1)
			*intptr = value;
		break;

	case sUseDNS:
		intptr = &options->use_dns;
		goto parse_flag;

	case sLogFacility:
		intptr = (int *) &options->log_facility;
		arg = strdelim(&cp);
		value = log_facility_number(arg);
		if (value == SYSLOG_FACILITY_NOT_SET)
			fatal("%.200s line %d: unsupported log facility '%s'",
			    filename, linenum, arg ? arg : "<NONE>");
		if (*intptr == -1)
			*intptr = (SyslogFacility) value;
		break;

	case sLogLevel:
		intptr = (int *) &options->log_level;
		arg = strdelim(&cp);
		value = log_level_number(arg);
		if (value == SYSLOG_LEVEL_NOT_SET)
			fatal("%.200s line %d: unsupported log level '%s'",
			    filename, linenum, arg ? arg : "<NONE>");
		if (*intptr == -1)
			*intptr = (LogLevel) value;
		break;

	case sAllowTcpForwarding:
		intptr = &options->allow_tcp_forwarding;
		goto parse_flag;

	case sUsePrivilegeSeparation:
		intptr = &use_privsep;
		goto parse_flag;

	case sAllowUsers:
		while ((arg = strdelim(&cp)) && *arg != '\0') {
			if (options->num_allow_users >= MAX_ALLOW_USERS)
				fatal("%s line %d: too many allow users.",
				    filename, linenum);
			options->allow_users[options->num_allow_users++] =
			    xstrdup(arg);
		}
		break;

	case sDenyUsers:
		while ((arg = strdelim(&cp)) && *arg != '\0') {
			if (options->num_deny_users >= MAX_DENY_USERS)
				fatal( "%s line %d: too many deny users.",
				    filename, linenum);
			options->deny_users[options->num_deny_users++] =
			    xstrdup(arg);
		}
		break;

	case sAllowGroups:
		while ((arg = strdelim(&cp)) && *arg != '\0') {
			if (options->num_allow_groups >= MAX_ALLOW_GROUPS)
				fatal("%s line %d: too many allow groups.",
				    filename, linenum);
			options->allow_groups[options->num_allow_groups++] =
			    xstrdup(arg);
		}
		break;

	case sDenyGroups:
		while ((arg = strdelim(&cp)) && *arg != '\0') {
			if (options->num_deny_groups >= MAX_DENY_GROUPS)
				fatal("%s line %d: too many deny groups.",
				    filename, linenum);
			options->deny_groups[options->num_deny_groups++] = xstrdup(arg);
		}
		break;

	case sCiphers:
		arg = strdelim(&cp);
		if (!arg || *arg == '\0')
			fatal("%s line %d: Missing argument.", filename, linenum);
		if (!ciphers_valid(arg))
			fatal("%s line %d: Bad SSH2 cipher spec '%s'.",
			    filename, linenum, arg ? arg : "<NONE>");
		if (options->ciphers == NULL)
			options->ciphers = xstrdup(arg);
		break;

	case sMacs:
		arg = strdelim(&cp);
		if (!arg || *arg == '\0')
			fatal("%s line %d: Missing argument.", filename, linenum);
		if (!mac_valid(arg))
			fatal("%s line %d: Bad SSH2 mac spec '%s'.",
			    filename, linenum, arg ? arg : "<NONE>");
		if (options->macs == NULL)
			options->macs = xstrdup(arg);
		break;

	case sProtocol:
		intptr = &options->protocol;
		arg = strdelim(&cp);
		if (!arg || *arg == '\0')
			fatal("%s line %d: Missing argument.", filename, linenum);
		value = proto_spec(arg);
		if (value == SSH_PROTO_UNKNOWN)
			fatal("%s line %d: Bad protocol spec '%s'.",
			    filename, linenum, arg ? arg : "<NONE>");
		if (*intptr == SSH_PROTO_UNKNOWN)
			*intptr = value;
		break;

	case sSubsystem:
		if (options->num_subsystems >= MAX_SUBSYSTEMS) {
			fatal("%s line %d: too many subsystems defined.",
			    filename, linenum);
		}
		arg = strdelim(&cp);
		if (!arg || *arg == '\0')
			fatal("%s line %d: Missing subsystem name.",
			    filename, linenum);
		for (i = 0; i < options->num_subsystems; i++)
			if (strcmp(arg, options->subsystem_name[i]) == 0)
				fatal("%s line %d: Subsystem '%s' already defined.",
				    filename, linenum, arg);
		options->subsystem_name[options->num_subsystems] = xstrdup(arg);
		arg = strdelim(&cp);
		if (!arg || *arg == '\0')
			fatal("%s line %d: Missing subsystem command.",
			    filename, linenum);
		options->subsystem_command[options->num_subsystems] = xstrdup(arg);
		options->num_subsystems++;
		break;

	case sMaxStartups:
		arg = strdelim(&cp);
		if (!arg || *arg == '\0')
			fatal("%s line %d: Missing MaxStartups spec.",
			    filename, linenum);
		if ((n = sscanf(arg, "%d:%d:%d",
		    &options->max_startups_begin,
		    &options->max_startups_rate,
		    &options->max_startups)) == 3) {
			if (options->max_startups_begin >
			    options->max_startups ||
			    options->max_startups_rate > 100 ||
			    options->max_startups_rate < 1)
				fatal("%s line %d: Illegal MaxStartups spec.",
				    filename, linenum);
		} else if (n != 1)
			fatal("%s line %d: Illegal MaxStartups spec.",
			    filename, linenum);
		else
			options->max_startups = options->max_startups_begin;
		break;

	case sMaxAuthTries:
		intptr = &options->max_authtries;
		goto parse_int;

	case sBanner:
		charptr = &options->banner;
		goto parse_filename;
	/*
	 * These options can contain %X options expanded at
	 * connect time, so that you can specify paths like:
	 *
	 * AuthorizedKeysFile	/etc/ssh_keys/%u
	 */
	case sAuthorizedKeysFile:
	case sAuthorizedKeysFile2:
		charptr = (opcode == sAuthorizedKeysFile ) ?
		    &options->authorized_keys_file :
		    &options->authorized_keys_file2;
		goto parse_filename;

	case sClientAliveInterval:
		intptr = &options->client_alive_interval;
		goto parse_time;

	case sClientAliveCountMax:
		intptr = &options->client_alive_count_max;
		goto parse_int;

	case sAcceptEnv:
		while ((arg = strdelim(&cp)) && *arg != '\0') {
			if (strchr(arg, '=') != NULL)
				fatal("%s line %d: Invalid environment name.",
				    filename, linenum);
			if (options->num_accept_env >= MAX_ACCEPT_ENV)
				fatal("%s line %d: too many allow env.",
				    filename, linenum);
			options->accept_env[options->num_accept_env++] =
			    xstrdup(arg);
		}
		break;

	case sVersionAddendum:
                ssh_version_set_addendum(strtok(cp, "\n"));
                do {
                        arg = strdelim(&cp);
                } while (arg != NULL && *arg != '\0');
		break;

	case sDeprecated:
		logit("%s line %d: Deprecated option %s",
		    filename, linenum, arg);
		while (arg)
		    arg = strdelim(&cp);
		break;

	case sUnsupported:
		logit("%s line %d: Unsupported option %s",
		    filename, linenum, arg);
		while (arg)
		    arg = strdelim(&cp);
		break;

	default:
		fatal("%s line %d: Missing handler for opcode %s (%d)",
		    filename, linenum, arg, opcode);
	}
	if ((arg = strdelim(&cp)) != NULL && *arg != '\0')
		fatal("%s line %d: garbage at end of line; \"%.200s\".",
		    filename, linenum, arg);
	return 0;
}

/* Reads the server configuration file. */

void
load_server_config(const char *filename, Buffer *conf)
{
	char line[1024], *cp;
	FILE *f;

	debug2("%s: filename %s", __func__, filename);
	if ((f = fopen(filename, "r")) == NULL) {
		perror(filename);
		exit(1);
	}
	buffer_clear(conf);
	while (fgets(line, sizeof(line), f)) {
		/*
		 * Trim out comments and strip whitespace
		 * NB - preserve newlines, they are needed to reproduce
		 * line numbers later for error messages
		 */
		if ((cp = strchr(line, '#')) != NULL)
			memcpy(cp, "\n", 2);
		cp = line + strspn(line, " \t\r");

		buffer_append(conf, cp, strlen(cp));
	}
	buffer_append(conf, "\0", 1);
	fclose(f);
	debug2("%s: done config len = %d", __func__, buffer_len(conf));
}

void
parse_server_config(ServerOptions *options, const char *filename, Buffer *conf)
{
	int linenum, bad_options = 0;
	char *cp, *obuf, *cbuf;

	debug2("%s: config %s len %d", __func__, filename, buffer_len(conf));

	obuf = cbuf = xstrdup(buffer_ptr(conf));
	linenum = 1;
	while ((cp = strsep(&cbuf, "\n")) != NULL) {
		if (process_server_config_line(options, cp, filename,
		    linenum++) != 0)
			bad_options++;
	}
	xfree(obuf);
	if (bad_options > 0)
		fatal("%s: terminating, %d bad configuration options",
		    filename, bad_options);
}
