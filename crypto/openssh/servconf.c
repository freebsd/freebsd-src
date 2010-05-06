/* $OpenBSD: servconf.c,v 1.207 2010/03/25 23:38:28 djm Exp $ */
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
__RCSID("$FreeBSD$");

#include <sys/types.h>
#include <sys/socket.h>

#include <netdb.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>

#include "openbsd-compat/sys-queue.h"
#include "xmalloc.h"
#include "ssh.h"
#include "log.h"
#include "buffer.h"
#include "servconf.h"
#include "compat.h"
#include "pathnames.h"
#include "misc.h"
#include "cipher.h"
#include "key.h"
#include "kex.h"
#include "mac.h"
#include "match.h"
#include "channels.h"
#include "groupaccess.h"
#include "version.h"

static void add_listen_addr(ServerOptions *, char *, int);
static void add_one_listen_addr(ServerOptions *, char *, int);

/* Use of privilege separation or not */
extern int use_privsep;
extern Buffer cfg;

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
	options->num_host_cert_files = 0;
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
	options->allow_agent_forwarding = -1;
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
	options->max_sessions = -1;
	options->banner = NULL;
	options->use_dns = -1;
	options->client_alive_interval = -1;
	options->client_alive_count_max = -1;
	options->authorized_keys_file = NULL;
	options->authorized_keys_file2 = NULL;
	options->num_accept_env = 0;
	options->permit_tun = -1;
	options->num_permitted_opens = -1;
	options->adm_forced_command = NULL;
	options->chroot_directory = NULL;
	options->zero_knowledge_password_authentication = -1;
	options->revoked_keys_file = NULL;
	options->trusted_user_ca_keys = NULL;
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
                            _PATH_HOST_RSA_KEY_FILE;
			options->host_key_files[options->num_host_key_files++] =
			    _PATH_HOST_DSA_KEY_FILE;
		}
	}
	/* No certificates by default */
	if (options->num_ports == 0)
		options->ports[options->num_ports++] = SSH_DEFAULT_PORT;
	if (options->listen_addrs == NULL)
		add_listen_addr(options, NULL, 0);
	if (options->pid_file == NULL)
		options->pid_file = _PATH_SSH_DAEMON_PID_FILE;
	if (options->server_key_bits == -1)
		options->server_key_bits = 1024;
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
		options->password_authentication = 0;
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
	if (options->allow_agent_forwarding == -1)
		options->allow_agent_forwarding = 1;
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
	if (options->max_sessions == -1)
		options->max_sessions = DEFAULT_SESSIONS_MAX;
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
	if (options->permit_tun == -1)
		options->permit_tun = SSH_TUNMODE_NO;
	if (options->zero_knowledge_password_authentication == -1)
		options->zero_knowledge_password_authentication = 0;

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
	sMaxStartups, sMaxAuthTries, sMaxSessions,
	sBanner, sUseDNS, sHostbasedAuthentication,
	sHostbasedUsesNameFromPacketOnly, sClientAliveInterval,
	sClientAliveCountMax, sAuthorizedKeysFile, sAuthorizedKeysFile2,
	sGssAuthentication, sGssCleanupCreds, sAcceptEnv, sPermitTunnel,
	sMatch, sPermitOpen, sForceCommand, sChrootDirectory,
	sUsePrivilegeSeparation, sAllowAgentForwarding,
	sZeroKnowledgePasswordAuthentication, sHostCertificate,
	sRevokedKeys, sTrustedUserCAKeys,
	sVersionAddendum,
	sDeprecated, sUnsupported
} ServerOpCodes;

#define SSHCFG_GLOBAL	0x01	/* allowed in main section of sshd_config */
#define SSHCFG_MATCH	0x02	/* allowed inside a Match section */
#define SSHCFG_ALL	(SSHCFG_GLOBAL|SSHCFG_MATCH)

/* Textual representation of the tokens. */
static struct {
	const char *name;
	ServerOpCodes opcode;
	u_int flags;
} keywords[] = {
	/* Portable-specific options */
#ifdef USE_PAM
	{ "usepam", sUsePAM, SSHCFG_GLOBAL },
#else
	{ "usepam", sUnsupported, SSHCFG_GLOBAL },
#endif
	{ "pamauthenticationviakbdint", sDeprecated, SSHCFG_GLOBAL },
	/* Standard Options */
	{ "port", sPort, SSHCFG_GLOBAL },
	{ "hostkey", sHostKeyFile, SSHCFG_GLOBAL },
	{ "hostdsakey", sHostKeyFile, SSHCFG_GLOBAL },		/* alias */
	{ "pidfile", sPidFile, SSHCFG_GLOBAL },
	{ "serverkeybits", sServerKeyBits, SSHCFG_GLOBAL },
	{ "logingracetime", sLoginGraceTime, SSHCFG_GLOBAL },
	{ "keyregenerationinterval", sKeyRegenerationTime, SSHCFG_GLOBAL },
	{ "permitrootlogin", sPermitRootLogin, SSHCFG_ALL },
	{ "syslogfacility", sLogFacility, SSHCFG_GLOBAL },
	{ "loglevel", sLogLevel, SSHCFG_GLOBAL },
	{ "rhostsauthentication", sDeprecated, SSHCFG_GLOBAL },
	{ "rhostsrsaauthentication", sRhostsRSAAuthentication, SSHCFG_ALL },
	{ "hostbasedauthentication", sHostbasedAuthentication, SSHCFG_ALL },
	{ "hostbasedusesnamefrompacketonly", sHostbasedUsesNameFromPacketOnly, SSHCFG_GLOBAL },
	{ "rsaauthentication", sRSAAuthentication, SSHCFG_ALL },
	{ "pubkeyauthentication", sPubkeyAuthentication, SSHCFG_ALL },
	{ "dsaauthentication", sPubkeyAuthentication, SSHCFG_GLOBAL }, /* alias */
#ifdef KRB5
	{ "kerberosauthentication", sKerberosAuthentication, SSHCFG_ALL },
	{ "kerberosorlocalpasswd", sKerberosOrLocalPasswd, SSHCFG_GLOBAL },
	{ "kerberosticketcleanup", sKerberosTicketCleanup, SSHCFG_GLOBAL },
#ifdef USE_AFS
	{ "kerberosgetafstoken", sKerberosGetAFSToken, SSHCFG_GLOBAL },
#else
	{ "kerberosgetafstoken", sUnsupported, SSHCFG_GLOBAL },
#endif
#else
	{ "kerberosauthentication", sUnsupported, SSHCFG_ALL },
	{ "kerberosorlocalpasswd", sUnsupported, SSHCFG_GLOBAL },
	{ "kerberosticketcleanup", sUnsupported, SSHCFG_GLOBAL },
	{ "kerberosgetafstoken", sUnsupported, SSHCFG_GLOBAL },
#endif
	{ "kerberostgtpassing", sUnsupported, SSHCFG_GLOBAL },
	{ "afstokenpassing", sUnsupported, SSHCFG_GLOBAL },
#ifdef GSSAPI
	{ "gssapiauthentication", sGssAuthentication, SSHCFG_ALL },
	{ "gssapicleanupcredentials", sGssCleanupCreds, SSHCFG_GLOBAL },
#else
	{ "gssapiauthentication", sUnsupported, SSHCFG_ALL },
	{ "gssapicleanupcredentials", sUnsupported, SSHCFG_GLOBAL },
#endif
	{ "passwordauthentication", sPasswordAuthentication, SSHCFG_ALL },
	{ "kbdinteractiveauthentication", sKbdInteractiveAuthentication, SSHCFG_ALL },
	{ "challengeresponseauthentication", sChallengeResponseAuthentication, SSHCFG_GLOBAL },
	{ "skeyauthentication", sChallengeResponseAuthentication, SSHCFG_GLOBAL }, /* alias */
#ifdef JPAKE
	{ "zeroknowledgepasswordauthentication", sZeroKnowledgePasswordAuthentication, SSHCFG_ALL },
#else
	{ "zeroknowledgepasswordauthentication", sUnsupported, SSHCFG_ALL },
#endif
	{ "checkmail", sDeprecated, SSHCFG_GLOBAL },
	{ "listenaddress", sListenAddress, SSHCFG_GLOBAL },
	{ "addressfamily", sAddressFamily, SSHCFG_GLOBAL },
	{ "printmotd", sPrintMotd, SSHCFG_GLOBAL },
	{ "printlastlog", sPrintLastLog, SSHCFG_GLOBAL },
	{ "ignorerhosts", sIgnoreRhosts, SSHCFG_GLOBAL },
	{ "ignoreuserknownhosts", sIgnoreUserKnownHosts, SSHCFG_GLOBAL },
	{ "x11forwarding", sX11Forwarding, SSHCFG_ALL },
	{ "x11displayoffset", sX11DisplayOffset, SSHCFG_ALL },
	{ "x11uselocalhost", sX11UseLocalhost, SSHCFG_ALL },
	{ "xauthlocation", sXAuthLocation, SSHCFG_GLOBAL },
	{ "strictmodes", sStrictModes, SSHCFG_GLOBAL },
	{ "permitemptypasswords", sEmptyPasswd, SSHCFG_ALL },
	{ "permituserenvironment", sPermitUserEnvironment, SSHCFG_GLOBAL },
	{ "uselogin", sUseLogin, SSHCFG_GLOBAL },
	{ "compression", sCompression, SSHCFG_GLOBAL },
	{ "tcpkeepalive", sTCPKeepAlive, SSHCFG_GLOBAL },
	{ "keepalive", sTCPKeepAlive, SSHCFG_GLOBAL },	/* obsolete alias */
	{ "allowtcpforwarding", sAllowTcpForwarding, SSHCFG_ALL },
	{ "allowagentforwarding", sAllowAgentForwarding, SSHCFG_ALL },
	{ "allowusers", sAllowUsers, SSHCFG_GLOBAL },
	{ "denyusers", sDenyUsers, SSHCFG_GLOBAL },
	{ "allowgroups", sAllowGroups, SSHCFG_GLOBAL },
	{ "denygroups", sDenyGroups, SSHCFG_GLOBAL },
	{ "ciphers", sCiphers, SSHCFG_GLOBAL },
	{ "macs", sMacs, SSHCFG_GLOBAL },
	{ "protocol", sProtocol, SSHCFG_GLOBAL },
	{ "gatewayports", sGatewayPorts, SSHCFG_ALL },
	{ "subsystem", sSubsystem, SSHCFG_GLOBAL },
	{ "maxstartups", sMaxStartups, SSHCFG_GLOBAL },
	{ "maxauthtries", sMaxAuthTries, SSHCFG_ALL },
	{ "maxsessions", sMaxSessions, SSHCFG_ALL },
	{ "banner", sBanner, SSHCFG_ALL },
	{ "usedns", sUseDNS, SSHCFG_GLOBAL },
	{ "verifyreversemapping", sDeprecated, SSHCFG_GLOBAL },
	{ "reversemappingcheck", sDeprecated, SSHCFG_GLOBAL },
	{ "clientaliveinterval", sClientAliveInterval, SSHCFG_GLOBAL },
	{ "clientalivecountmax", sClientAliveCountMax, SSHCFG_GLOBAL },
	{ "authorizedkeysfile", sAuthorizedKeysFile, SSHCFG_GLOBAL },
	{ "authorizedkeysfile2", sAuthorizedKeysFile2, SSHCFG_GLOBAL },
	{ "useprivilegeseparation", sUsePrivilegeSeparation, SSHCFG_GLOBAL},
	{ "acceptenv", sAcceptEnv, SSHCFG_GLOBAL },
	{ "permittunnel", sPermitTunnel, SSHCFG_GLOBAL },
	{ "match", sMatch, SSHCFG_ALL },
	{ "permitopen", sPermitOpen, SSHCFG_ALL },
	{ "forcecommand", sForceCommand, SSHCFG_ALL },
	{ "chrootdirectory", sChrootDirectory, SSHCFG_ALL },
	{ "hostcertificate", sHostCertificate, SSHCFG_GLOBAL },
	{ "revokedkeys", sRevokedKeys, SSHCFG_ALL },
	{ "trustedusercakeys", sTrustedUserCAKeys, SSHCFG_ALL },
	{ "versionaddendum", sVersionAddendum, SSHCFG_GLOBAL },
	{ NULL, sBadOption, 0 }
};

static struct {
	int val;
	char *text;
} tunmode_desc[] = {
	{ SSH_TUNMODE_NO, "no" },
	{ SSH_TUNMODE_POINTOPOINT, "point-to-point" },
	{ SSH_TUNMODE_ETHERNET, "ethernet" },
	{ SSH_TUNMODE_YES, "yes" },
	{ -1, NULL }
};

/*
 * Returns the number of the token pointed to by cp or sBadOption.
 */

static ServerOpCodes
parse_token(const char *cp, const char *filename,
	    int linenum, u_int *flags)
{
	u_int i;

	for (i = 0; keywords[i].name; i++)
		if (strcasecmp(cp, keywords[i].name) == 0) {
			*flags = keywords[i].flags;
			return keywords[i].opcode;
		}

	error("%s: line %d: Bad configuration option: %s",
	    filename, linenum, cp);
	return sBadOption;
}

char *
derelativise_path(const char *path)
{
	char *expanded, *ret, cwd[MAXPATHLEN];

	expanded = tilde_expand_filename(path, getuid());
	if (*expanded == '/')
		return expanded;
	if (getcwd(cwd, sizeof(cwd)) == NULL)
		fatal("%s: getcwd: %s", __func__, strerror(errno));
	xasprintf(&ret, "%s/%s", cwd, expanded);
	xfree(expanded);
	return ret;
}

static void
add_listen_addr(ServerOptions *options, char *addr, int port)
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
add_one_listen_addr(ServerOptions *options, char *addr, int port)
{
	struct addrinfo hints, *ai, *aitop;
	char strport[NI_MAXSERV];
	int gaierr;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = options->address_family;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = (addr == NULL) ? AI_PASSIVE : 0;
	snprintf(strport, sizeof strport, "%d", port);
	if ((gaierr = getaddrinfo(addr, strport, &hints, &aitop)) != 0)
		fatal("bad addr or host: %s (%s)",
		    addr ? addr : "<NULL>",
		    ssh_gai_strerror(gaierr));
	for (ai = aitop; ai->ai_next; ai = ai->ai_next)
		;
	ai->ai_next = options->listen_addrs;
	options->listen_addrs = aitop;
}

/*
 * The strategy for the Match blocks is that the config file is parsed twice.
 *
 * The first time is at startup.  activep is initialized to 1 and the
 * directives in the global context are processed and acted on.  Hitting a
 * Match directive unsets activep and the directives inside the block are
 * checked for syntax only.
 *
 * The second time is after a connection has been established but before
 * authentication.  activep is initialized to 2 and global config directives
 * are ignored since they have already been processed.  If the criteria in a
 * Match block is met, activep is set and the subsequent directives
 * processed and actioned until EOF or another Match block unsets it.  Any
 * options set are copied into the main server config.
 *
 * Potential additions/improvements:
 *  - Add Match support for pre-kex directives, eg Protocol, Ciphers.
 *
 *  - Add a Tag directive (idea from David Leonard) ala pf, eg:
 *	Match Address 192.168.0.*
 *		Tag trusted
 *	Match Group wheel
 *		Tag trusted
 *	Match Tag trusted
 *		AllowTcpForwarding yes
 *		GatewayPorts clientspecified
 *		[...]
 *
 *  - Add a PermittedChannelRequests directive
 *	Match Group shell
 *		PermittedChannelRequests session,forwarded-tcpip
 */

static int
match_cfg_line_group(const char *grps, int line, const char *user)
{
	int result = 0;
	struct passwd *pw;

	if (user == NULL)
		goto out;

	if ((pw = getpwnam(user)) == NULL) {
		debug("Can't match group at line %d because user %.100s does "
		    "not exist", line, user);
	} else if (ga_init(pw->pw_name, pw->pw_gid) == 0) {
		debug("Can't Match group because user %.100s not in any group "
		    "at line %d", user, line);
	} else if (ga_match_pattern_list(grps) != 1) {
		debug("user %.100s does not match group list %.100s at line %d",
		    user, grps, line);
	} else {
		debug("user %.100s matched group list %.100s at line %d", user,
		    grps, line);
		result = 1;
	}
out:
	ga_free();
	return result;
}

static int
match_cfg_line(char **condition, int line, const char *user, const char *host,
    const char *address)
{
	int result = 1;
	char *arg, *attrib, *cp = *condition;
	size_t len;

	if (user == NULL)
		debug3("checking syntax for 'Match %s'", cp);
	else
		debug3("checking match for '%s' user %s host %s addr %s", cp,
		    user ? user : "(null)", host ? host : "(null)",
		    address ? address : "(null)");

	while ((attrib = strdelim(&cp)) && *attrib != '\0') {
		if ((arg = strdelim(&cp)) == NULL || *arg == '\0') {
			error("Missing Match criteria for %s", attrib);
			return -1;
		}
		len = strlen(arg);
		if (strcasecmp(attrib, "user") == 0) {
			if (!user) {
				result = 0;
				continue;
			}
			if (match_pattern_list(user, arg, len, 0) != 1)
				result = 0;
			else
				debug("user %.100s matched 'User %.100s' at "
				    "line %d", user, arg, line);
		} else if (strcasecmp(attrib, "group") == 0) {
			switch (match_cfg_line_group(arg, line, user)) {
			case -1:
				return -1;
			case 0:
				result = 0;
			}
		} else if (strcasecmp(attrib, "host") == 0) {
			if (!host) {
				result = 0;
				continue;
			}
			if (match_hostname(host, arg, len) != 1)
				result = 0;
			else
				debug("connection from %.100s matched 'Host "
				    "%.100s' at line %d", host, arg, line);
		} else if (strcasecmp(attrib, "address") == 0) {
			switch (addr_match_list(address, arg)) {
			case 1:
				debug("connection from %.100s matched 'Address "
				    "%.100s' at line %d", address, arg, line);
				break;
			case 0:
			case -1:
				result = 0;
				break;
			case -2:
				return -1;
			}
		} else {
			error("Unsupported Match attribute %s", attrib);
			return -1;
		}
	}
	if (user != NULL)
		debug3("match %sfound", result ? "" : "not ");
	*condition = cp;
	return result;
}

#define WHITESPACE " \t\r\n"

int
process_server_config_line(ServerOptions *options, char *line,
    const char *filename, int linenum, int *activep, const char *user,
    const char *host, const char *address)
{
	char *cp, **charptr, *arg, *p;
	int cmdline = 0, *intptr, value, n;
	SyslogFacility *log_facility_ptr;
	LogLevel *log_level_ptr;
	ServerOpCodes opcode;
	int port;
	u_int i, flags = 0;
	size_t len;

	cp = line;
	if ((arg = strdelim(&cp)) == NULL)
		return 0;
	/* Ignore leading whitespace */
	if (*arg == '\0')
		arg = strdelim(&cp);
	if (!arg || !*arg || *arg == '#')
		return 0;
	intptr = NULL;
	charptr = NULL;
	opcode = parse_token(arg, filename, linenum, &flags);

	if (activep == NULL) { /* We are processing a command line directive */
		cmdline = 1;
		activep = &cmdline;
	}
	if (*activep && opcode != sMatch)
		debug3("%s:%d setting %s %s", filename, linenum, arg, cp);
	if (*activep == 0 && !(flags & SSHCFG_MATCH)) {
		if (user == NULL) {
			fatal("%s line %d: Directive '%s' is not allowed "
			    "within a Match block", filename, linenum, arg);
		} else { /* this is a directive we have already processed */
			while (arg)
				arg = strdelim(&cp);
			return 0;
		}
	}

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
		if (options->ports[options->num_ports-1] <= 0)
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
		if (*activep && *intptr == -1)
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
		else if ((port = a2port(arg)) <= 0)
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
		if (*activep && *charptr == NULL) {
			*charptr = derelativise_path(arg);
			/* increase optional counter */
			if (intptr != NULL)
				*intptr = *intptr + 1;
		}
		break;

	case sHostCertificate:
		intptr = &options->num_host_cert_files;
		if (*intptr >= MAX_HOSTKEYS)
			fatal("%s line %d: too many host certificates "
			    "specified (max %d).", filename, linenum,
			    MAX_HOSTCERTS);
		charptr = &options->host_cert_files[*intptr];
		goto parse_filename;
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
		if (*activep && *intptr == -1)
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
		if (*activep && *intptr == -1)
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

	case sZeroKnowledgePasswordAuthentication:
		intptr = &options->zero_knowledge_password_authentication;
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
		if (*activep && *intptr == -1)
			*intptr = value;
		break;

	case sUseDNS:
		intptr = &options->use_dns;
		goto parse_flag;

	case sLogFacility:
		log_facility_ptr = &options->log_facility;
		arg = strdelim(&cp);
		value = log_facility_number(arg);
		if (value == SYSLOG_FACILITY_NOT_SET)
			fatal("%.200s line %d: unsupported log facility '%s'",
			    filename, linenum, arg ? arg : "<NONE>");
		if (*log_facility_ptr == -1)
			*log_facility_ptr = (SyslogFacility) value;
		break;

	case sLogLevel:
		log_level_ptr = &options->log_level;
		arg = strdelim(&cp);
		value = log_level_number(arg);
		if (value == SYSLOG_LEVEL_NOT_SET)
			fatal("%.200s line %d: unsupported log level '%s'",
			    filename, linenum, arg ? arg : "<NONE>");
		if (*log_level_ptr == -1)
			*log_level_ptr = (LogLevel) value;
		break;

	case sAllowTcpForwarding:
		intptr = &options->allow_tcp_forwarding;
		goto parse_flag;

	case sAllowAgentForwarding:
		intptr = &options->allow_agent_forwarding;
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
				fatal("%s line %d: too many deny users.",
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
		if (!*activep) {
			arg = strdelim(&cp);
			break;
		}
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

		/* Collect arguments (separate to executable) */
		p = xstrdup(arg);
		len = strlen(p) + 1;
		while ((arg = strdelim(&cp)) != NULL && *arg != '\0') {
			len += 1 + strlen(arg);
			p = xrealloc(p, 1, len);
			strlcat(p, " ", len);
			strlcat(p, arg, len);
		}
		options->subsystem_args[options->num_subsystems] = p;
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

	case sMaxSessions:
		intptr = &options->max_sessions;
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
		charptr = (opcode == sAuthorizedKeysFile) ?
		    &options->authorized_keys_file :
		    &options->authorized_keys_file2;
		arg = strdelim(&cp);
		if (!arg || *arg == '\0')
			fatal("%s line %d: missing file name.",
			    filename, linenum);
		if (*activep && *charptr == NULL) {
			*charptr = tilde_expand_filename(arg, getuid());
			/* increase optional counter */
			if (intptr != NULL)
				*intptr = *intptr + 1;
		}
		break;

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
			if (!*activep)
				break;
			options->accept_env[options->num_accept_env++] =
			    xstrdup(arg);
		}
		break;

	case sPermitTunnel:
		intptr = &options->permit_tun;
		arg = strdelim(&cp);
		if (!arg || *arg == '\0')
			fatal("%s line %d: Missing yes/point-to-point/"
			    "ethernet/no argument.", filename, linenum);
		value = -1;
		for (i = 0; tunmode_desc[i].val != -1; i++)
			if (strcmp(tunmode_desc[i].text, arg) == 0) {
				value = tunmode_desc[i].val;
				break;
			}
		if (value == -1)
			fatal("%s line %d: Bad yes/point-to-point/ethernet/"
			    "no argument: %s", filename, linenum, arg);
		if (*intptr == -1)
			*intptr = value;
		break;

	case sMatch:
		if (cmdline)
			fatal("Match directive not supported as a command-line "
			   "option");
		value = match_cfg_line(&cp, linenum, user, host, address);
		if (value < 0)
			fatal("%s line %d: Bad Match condition", filename,
			    linenum);
		*activep = value;
		break;

	case sPermitOpen:
		arg = strdelim(&cp);
		if (!arg || *arg == '\0')
			fatal("%s line %d: missing PermitOpen specification",
			    filename, linenum);
		n = options->num_permitted_opens;	/* modified later */
		if (strcmp(arg, "any") == 0) {
			if (*activep && n == -1) {
				channel_clear_adm_permitted_opens();
				options->num_permitted_opens = 0;
			}
			break;
		}
		if (*activep && n == -1)
			channel_clear_adm_permitted_opens();
		for (; arg != NULL && *arg != '\0'; arg = strdelim(&cp)) {
			p = hpdelim(&arg);
			if (p == NULL)
				fatal("%s line %d: missing host in PermitOpen",
				    filename, linenum);
			p = cleanhostname(p);
			if (arg == NULL || (port = a2port(arg)) <= 0)
				fatal("%s line %d: bad port number in "
				    "PermitOpen", filename, linenum);
			if (*activep && n == -1)
				options->num_permitted_opens =
				    channel_add_adm_permitted_opens(p, port);
		}
		break;

	case sForceCommand:
		if (cp == NULL)
			fatal("%.200s line %d: Missing argument.", filename,
			    linenum);
		len = strspn(cp, WHITESPACE);
		if (*activep && options->adm_forced_command == NULL)
			options->adm_forced_command = xstrdup(cp + len);
		return 0;

	case sChrootDirectory:
		charptr = &options->chroot_directory;

		arg = strdelim(&cp);
		if (!arg || *arg == '\0')
			fatal("%s line %d: missing file name.",
			    filename, linenum);
		if (*activep && *charptr == NULL)
			*charptr = xstrdup(arg);
		break;

	case sTrustedUserCAKeys:
		charptr = &options->trusted_user_ca_keys;
		goto parse_filename;

	case sRevokedKeys:
		charptr = &options->revoked_keys_file;
		goto parse_filename;

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
parse_server_match_config(ServerOptions *options, const char *user,
    const char *host, const char *address)
{
	ServerOptions mo;

	initialize_server_options(&mo);
	parse_server_config(&mo, "reprocess config", &cfg, user, host, address);
	copy_set_server_options(options, &mo, 0);
}

/* Helper macros */
#define M_CP_INTOPT(n) do {\
	if (src->n != -1) \
		dst->n = src->n; \
} while (0)
#define M_CP_STROPT(n) do {\
	if (src->n != NULL) { \
		if (dst->n != NULL) \
			xfree(dst->n); \
		dst->n = src->n; \
	} \
} while(0)

/*
 * Copy any supported values that are set.
 *
 * If the preauth flag is set, we do not bother copying the string or
 * array values that are not used pre-authentication, because any that we
 * do use must be explictly sent in mm_getpwnamallow().
 */
void
copy_set_server_options(ServerOptions *dst, ServerOptions *src, int preauth)
{
	M_CP_INTOPT(password_authentication);
	M_CP_INTOPT(gss_authentication);
	M_CP_INTOPT(rsa_authentication);
	M_CP_INTOPT(pubkey_authentication);
	M_CP_INTOPT(kerberos_authentication);
	M_CP_INTOPT(hostbased_authentication);
	M_CP_INTOPT(kbd_interactive_authentication);
	M_CP_INTOPT(zero_knowledge_password_authentication);
	M_CP_INTOPT(permit_root_login);
	M_CP_INTOPT(permit_empty_passwd);

	M_CP_INTOPT(allow_tcp_forwarding);
	M_CP_INTOPT(allow_agent_forwarding);
	M_CP_INTOPT(gateway_ports);
	M_CP_INTOPT(x11_display_offset);
	M_CP_INTOPT(x11_forwarding);
	M_CP_INTOPT(x11_use_localhost);
	M_CP_INTOPT(max_sessions);
	M_CP_INTOPT(max_authtries);

	M_CP_STROPT(banner);
	if (preauth)
		return;
	M_CP_STROPT(adm_forced_command);
	M_CP_STROPT(chroot_directory);
	M_CP_STROPT(trusted_user_ca_keys);
	M_CP_STROPT(revoked_keys_file);
}

#undef M_CP_INTOPT
#undef M_CP_STROPT

void
parse_server_config(ServerOptions *options, const char *filename, Buffer *conf,
    const char *user, const char *host, const char *address)
{
	int active, linenum, bad_options = 0;
	char *cp, *obuf, *cbuf;

	debug2("%s: config %s len %d", __func__, filename, buffer_len(conf));

	obuf = cbuf = xstrdup(buffer_ptr(conf));
	active = user ? 0 : 1;
	linenum = 1;
	while ((cp = strsep(&cbuf, "\n")) != NULL) {
		if (process_server_config_line(options, cp, filename,
		    linenum++, &active, user, host, address) != 0)
			bad_options++;
	}
	xfree(obuf);
	if (bad_options > 0)
		fatal("%s: terminating, %d bad configuration options",
		    filename, bad_options);
}

static const char *
fmt_intarg(ServerOpCodes code, int val)
{
	if (code == sAddressFamily) {
		switch (val) {
		case AF_INET:
			return "inet";
		case AF_INET6:
			return "inet6";
		case AF_UNSPEC:
			return "any";
		default:
			return "UNKNOWN";
		}
	}
	if (code == sPermitRootLogin) {
		switch (val) {
		case PERMIT_NO_PASSWD:
			return "without-password";
		case PERMIT_FORCED_ONLY:
			return "forced-commands-only";
		case PERMIT_YES:
			return "yes";
		}
	}
	if (code == sProtocol) {
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
	}
	if (code == sGatewayPorts && val == 2)
		return "clientspecified";
	if (code == sCompression && val == COMP_DELAYED)
		return "delayed";
	switch (val) {
	case -1:
		return "unset";
	case 0:
		return "no";
	case 1:
		return "yes";
	}
	return "UNKNOWN";
}

static const char *
lookup_opcode_name(ServerOpCodes code)
{
	u_int i;

	for (i = 0; keywords[i].name != NULL; i++)
		if (keywords[i].opcode == code)
			return(keywords[i].name);
	return "UNKNOWN";
}

static void
dump_cfg_int(ServerOpCodes code, int val)
{
	printf("%s %d\n", lookup_opcode_name(code), val);
}

static void
dump_cfg_fmtint(ServerOpCodes code, int val)
{
	printf("%s %s\n", lookup_opcode_name(code), fmt_intarg(code, val));
}

static void
dump_cfg_string(ServerOpCodes code, const char *val)
{
	if (val == NULL)
		return;
	printf("%s %s\n", lookup_opcode_name(code), val);
}

static void
dump_cfg_strarray(ServerOpCodes code, u_int count, char **vals)
{
	u_int i;

	for (i = 0; i < count; i++)
		printf("%s %s\n", lookup_opcode_name(code),  vals[i]);
}

void
dump_config(ServerOptions *o)
{
	u_int i;
	int ret;
	struct addrinfo *ai;
	char addr[NI_MAXHOST], port[NI_MAXSERV], *s = NULL;

	/* these are usually at the top of the config */
	for (i = 0; i < o->num_ports; i++)
		printf("port %d\n", o->ports[i]);
	dump_cfg_fmtint(sProtocol, o->protocol);
	dump_cfg_fmtint(sAddressFamily, o->address_family);

	/* ListenAddress must be after Port */
	for (ai = o->listen_addrs; ai; ai = ai->ai_next) {
		if ((ret = getnameinfo(ai->ai_addr, ai->ai_addrlen, addr,
		    sizeof(addr), port, sizeof(port),
		    NI_NUMERICHOST|NI_NUMERICSERV)) != 0) {
			error("getnameinfo failed: %.100s",
			    (ret != EAI_SYSTEM) ? gai_strerror(ret) :
			    strerror(errno));
		} else {
			if (ai->ai_family == AF_INET6)
				printf("listenaddress [%s]:%s\n", addr, port);
			else
				printf("listenaddress %s:%s\n", addr, port);
		}
	}

	/* integer arguments */
#ifdef USE_PAM
	dump_cfg_int(sUsePAM, o->use_pam);
#endif
	dump_cfg_int(sServerKeyBits, o->server_key_bits);
	dump_cfg_int(sLoginGraceTime, o->login_grace_time);
	dump_cfg_int(sKeyRegenerationTime, o->key_regeneration_time);
	dump_cfg_int(sX11DisplayOffset, o->x11_display_offset);
	dump_cfg_int(sMaxAuthTries, o->max_authtries);
	dump_cfg_int(sMaxSessions, o->max_sessions);
	dump_cfg_int(sClientAliveInterval, o->client_alive_interval);
	dump_cfg_int(sClientAliveCountMax, o->client_alive_count_max);

	/* formatted integer arguments */
	dump_cfg_fmtint(sPermitRootLogin, o->permit_root_login);
	dump_cfg_fmtint(sIgnoreRhosts, o->ignore_rhosts);
	dump_cfg_fmtint(sIgnoreUserKnownHosts, o->ignore_user_known_hosts);
	dump_cfg_fmtint(sRhostsRSAAuthentication, o->rhosts_rsa_authentication);
	dump_cfg_fmtint(sHostbasedAuthentication, o->hostbased_authentication);
	dump_cfg_fmtint(sHostbasedUsesNameFromPacketOnly,
	    o->hostbased_uses_name_from_packet_only);
	dump_cfg_fmtint(sRSAAuthentication, o->rsa_authentication);
	dump_cfg_fmtint(sPubkeyAuthentication, o->pubkey_authentication);
#ifdef KRB5
	dump_cfg_fmtint(sKerberosAuthentication, o->kerberos_authentication);
	dump_cfg_fmtint(sKerberosOrLocalPasswd, o->kerberos_or_local_passwd);
	dump_cfg_fmtint(sKerberosTicketCleanup, o->kerberos_ticket_cleanup);
# ifdef USE_AFS
	dump_cfg_fmtint(sKerberosGetAFSToken, o->kerberos_get_afs_token);
# endif
#endif
#ifdef GSSAPI
	dump_cfg_fmtint(sGssAuthentication, o->gss_authentication);
	dump_cfg_fmtint(sGssCleanupCreds, o->gss_cleanup_creds);
#endif
#ifdef JPAKE
	dump_cfg_fmtint(sZeroKnowledgePasswordAuthentication,
	    o->zero_knowledge_password_authentication);
#endif
	dump_cfg_fmtint(sPasswordAuthentication, o->password_authentication);
	dump_cfg_fmtint(sKbdInteractiveAuthentication,
	    o->kbd_interactive_authentication);
	dump_cfg_fmtint(sChallengeResponseAuthentication,
	    o->challenge_response_authentication);
	dump_cfg_fmtint(sPrintMotd, o->print_motd);
	dump_cfg_fmtint(sPrintLastLog, o->print_lastlog);
	dump_cfg_fmtint(sX11Forwarding, o->x11_forwarding);
	dump_cfg_fmtint(sX11UseLocalhost, o->x11_use_localhost);
	dump_cfg_fmtint(sStrictModes, o->strict_modes);
	dump_cfg_fmtint(sTCPKeepAlive, o->tcp_keep_alive);
	dump_cfg_fmtint(sEmptyPasswd, o->permit_empty_passwd);
	dump_cfg_fmtint(sPermitUserEnvironment, o->permit_user_env);
	dump_cfg_fmtint(sUseLogin, o->use_login);
	dump_cfg_fmtint(sCompression, o->compression);
	dump_cfg_fmtint(sGatewayPorts, o->gateway_ports);
	dump_cfg_fmtint(sUseDNS, o->use_dns);
	dump_cfg_fmtint(sAllowTcpForwarding, o->allow_tcp_forwarding);
	dump_cfg_fmtint(sUsePrivilegeSeparation, use_privsep);

	/* string arguments */
	dump_cfg_string(sPidFile, o->pid_file);
	dump_cfg_string(sXAuthLocation, o->xauth_location);
	dump_cfg_string(sCiphers, o->ciphers);
	dump_cfg_string(sMacs, o->macs);
	dump_cfg_string(sBanner, o->banner);
	dump_cfg_string(sAuthorizedKeysFile, o->authorized_keys_file);
	dump_cfg_string(sAuthorizedKeysFile2, o->authorized_keys_file2);
	dump_cfg_string(sForceCommand, o->adm_forced_command);
	dump_cfg_string(sChrootDirectory, o->chroot_directory);
	dump_cfg_string(sTrustedUserCAKeys, o->trusted_user_ca_keys);
	dump_cfg_string(sRevokedKeys, o->revoked_keys_file);

	/* string arguments requiring a lookup */
	dump_cfg_string(sLogLevel, log_level_name(o->log_level));
	dump_cfg_string(sLogFacility, log_facility_name(o->log_facility));

	/* string array arguments */
	dump_cfg_strarray(sHostKeyFile, o->num_host_key_files,
	     o->host_key_files);
	dump_cfg_strarray(sHostKeyFile, o->num_host_cert_files,
	     o->host_cert_files);
	dump_cfg_strarray(sAllowUsers, o->num_allow_users, o->allow_users);
	dump_cfg_strarray(sDenyUsers, o->num_deny_users, o->deny_users);
	dump_cfg_strarray(sAllowGroups, o->num_allow_groups, o->allow_groups);
	dump_cfg_strarray(sDenyGroups, o->num_deny_groups, o->deny_groups);
	dump_cfg_strarray(sAcceptEnv, o->num_accept_env, o->accept_env);

	/* other arguments */
	for (i = 0; i < o->num_subsystems; i++)
		printf("subsystem %s %s\n", o->subsystem_name[i],
		    o->subsystem_args[i]);

	printf("maxstartups %d:%d:%d\n", o->max_startups_begin,
	    o->max_startups_rate, o->max_startups);

	for (i = 0; tunmode_desc[i].val != -1; i++)
		if (tunmode_desc[i].val == o->permit_tun) {
			s = tunmode_desc[i].text;
			break;
		}
	dump_cfg_string(sPermitTunnel, s);

	channel_print_adm_permitted_opens();
}
