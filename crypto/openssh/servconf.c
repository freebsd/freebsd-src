
/* $OpenBSD: servconf.c,v 1.392 2023/03/05 05:34:09 dtucker Exp $ */
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

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#ifdef __OpenBSD__
#include <sys/sysctl.h>
#endif

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#ifdef HAVE_NET_ROUTE_H
#include <net/route.h>
#endif

#include <ctype.h>
#include <netdb.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <limits.h>
#include <stdarg.h>
#include <errno.h>
#ifdef HAVE_UTIL_H
#include <util.h>
#endif
#ifdef USE_SYSTEM_GLOB
# include <glob.h>
#else
# include "openbsd-compat/glob.h"
#endif

#include "openbsd-compat/sys-queue.h"
#include "xmalloc.h"
#include "ssh.h"
#include "log.h"
#include "sshbuf.h"
#include "misc.h"
#include "servconf.h"
#include "pathnames.h"
#include "cipher.h"
#include "sshkey.h"
#include "kex.h"
#include "mac.h"
#include "match.h"
#include "channels.h"
#include "groupaccess.h"
#include "canohost.h"
#include "packet.h"
#include "ssherr.h"
#include "hostfile.h"
#include "auth.h"
#include "myproposal.h"
#include "digest.h"
#include "version.h"

static void add_listen_addr(ServerOptions *, const char *,
    const char *, int);
static void add_one_listen_addr(ServerOptions *, const char *,
    const char *, int);
static void parse_server_config_depth(ServerOptions *options,
    const char *filename, struct sshbuf *conf, struct include_list *includes,
    struct connection_info *connectinfo, int flags, int *activep, int depth);

/* Use of privilege separation or not */
extern int use_privsep;
extern struct sshbuf *cfg;

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
	options->queued_listen_addrs = NULL;
	options->num_queued_listens = 0;
	options->listen_addrs = NULL;
	options->num_listen_addrs = 0;
	options->address_family = -1;
	options->routing_domain = NULL;
	options->num_host_key_files = 0;
	options->num_host_cert_files = 0;
	options->host_key_agent = NULL;
	options->pid_file = NULL;
	options->login_grace_time = -1;
	options->permit_root_login = PERMIT_NOT_SET;
	options->ignore_rhosts = -1;
	options->ignore_user_known_hosts = -1;
	options->print_motd = -1;
	options->print_lastlog = -1;
	options->x11_forwarding = -1;
	options->x11_display_offset = -1;
	options->x11_use_localhost = -1;
	options->permit_tty = -1;
	options->permit_user_rc = -1;
	options->xauth_location = NULL;
	options->strict_modes = -1;
	options->tcp_keep_alive = -1;
	options->log_facility = SYSLOG_FACILITY_NOT_SET;
	options->log_level = SYSLOG_LEVEL_NOT_SET;
	options->num_log_verbose = 0;
	options->log_verbose = NULL;
	options->hostbased_authentication = -1;
	options->hostbased_uses_name_from_packet_only = -1;
	options->hostbased_accepted_algos = NULL;
	options->hostkeyalgorithms = NULL;
	options->pubkey_authentication = -1;
	options->pubkey_auth_options = -1;
	options->pubkey_accepted_algos = NULL;
	options->kerberos_authentication = -1;
	options->kerberos_or_local_passwd = -1;
	options->kerberos_ticket_cleanup = -1;
	options->kerberos_get_afs_token = -1;
	options->gss_authentication=-1;
	options->gss_cleanup_creds = -1;
	options->gss_strict_acceptor = -1;
	options->password_authentication = -1;
	options->kbd_interactive_authentication = -1;
	options->permit_empty_passwd = -1;
	options->permit_user_env = -1;
	options->permit_user_env_allowlist = NULL;
	options->compression = -1;
	options->rekey_limit = -1;
	options->rekey_interval = -1;
	options->allow_tcp_forwarding = -1;
	options->allow_streamlocal_forwarding = -1;
	options->allow_agent_forwarding = -1;
	options->num_allow_users = 0;
	options->num_deny_users = 0;
	options->num_allow_groups = 0;
	options->num_deny_groups = 0;
	options->ciphers = NULL;
	options->macs = NULL;
	options->kex_algorithms = NULL;
	options->ca_sign_algorithms = NULL;
	options->fwd_opts.gateway_ports = -1;
	options->fwd_opts.streamlocal_bind_mask = (mode_t)-1;
	options->fwd_opts.streamlocal_bind_unlink = -1;
	options->num_subsystems = 0;
	options->max_startups_begin = -1;
	options->max_startups_rate = -1;
	options->max_startups = -1;
	options->per_source_max_startups = -1;
	options->per_source_masklen_ipv4 = -1;
	options->per_source_masklen_ipv6 = -1;
	options->max_authtries = -1;
	options->max_sessions = -1;
	options->banner = NULL;
	options->use_dns = -1;
	options->client_alive_interval = -1;
	options->client_alive_count_max = -1;
	options->num_authkeys_files = 0;
	options->num_accept_env = 0;
	options->num_setenv = 0;
	options->permit_tun = -1;
	options->permitted_opens = NULL;
	options->permitted_listens = NULL;
	options->adm_forced_command = NULL;
	options->chroot_directory = NULL;
	options->authorized_keys_command = NULL;
	options->authorized_keys_command_user = NULL;
	options->revoked_keys_file = NULL;
	options->sk_provider = NULL;
	options->trusted_user_ca_keys = NULL;
	options->authorized_principals_file = NULL;
	options->authorized_principals_command = NULL;
	options->authorized_principals_command_user = NULL;
	options->ip_qos_interactive = -1;
	options->ip_qos_bulk = -1;
	options->version_addendum = NULL;
	options->fingerprint_hash = -1;
	options->disable_forwarding = -1;
	options->expose_userauth_info = -1;
	options->required_rsa_size = -1;
	options->channel_timeouts = NULL;
	options->num_channel_timeouts = 0;
	options->unused_connection_timeout = -1;
	options->use_blacklist = -1;
}

/* Returns 1 if a string option is unset or set to "none" or 0 otherwise. */
static int
option_clear_or_none(const char *o)
{
	return o == NULL || strcasecmp(o, "none") == 0;
}

static void
assemble_algorithms(ServerOptions *o)
{
	char *all_cipher, *all_mac, *all_kex, *all_key, *all_sig;
	char *def_cipher, *def_mac, *def_kex, *def_key, *def_sig;
	int r;

	all_cipher = cipher_alg_list(',', 0);
	all_mac = mac_alg_list(',');
	all_kex = kex_alg_list(',');
	all_key = sshkey_alg_list(0, 0, 1, ',');
	all_sig = sshkey_alg_list(0, 1, 1, ',');
	/* remove unsupported algos from default lists */
	def_cipher = match_filter_allowlist(KEX_SERVER_ENCRYPT, all_cipher);
	def_mac = match_filter_allowlist(KEX_SERVER_MAC, all_mac);
	def_kex = match_filter_allowlist(KEX_SERVER_KEX, all_kex);
	def_key = match_filter_allowlist(KEX_DEFAULT_PK_ALG, all_key);
	def_sig = match_filter_allowlist(SSH_ALLOWED_CA_SIGALGS, all_sig);
#define ASSEMBLE(what, defaults, all) \
	do { \
		if ((r = kex_assemble_names(&o->what, defaults, all)) != 0) \
			fatal_fr(r, "%s", #what); \
	} while (0)
	ASSEMBLE(ciphers, def_cipher, all_cipher);
	ASSEMBLE(macs, def_mac, all_mac);
	ASSEMBLE(kex_algorithms, def_kex, all_kex);
	ASSEMBLE(hostkeyalgorithms, def_key, all_key);
	ASSEMBLE(hostbased_accepted_algos, def_key, all_key);
	ASSEMBLE(pubkey_accepted_algos, def_key, all_key);
	ASSEMBLE(ca_sign_algorithms, def_sig, all_sig);
#undef ASSEMBLE
	free(all_cipher);
	free(all_mac);
	free(all_kex);
	free(all_key);
	free(all_sig);
	free(def_cipher);
	free(def_mac);
	free(def_kex);
	free(def_key);
	free(def_sig);
}

static const char *defaultkey = "[default]";

void
servconf_add_hostkey(const char *file, const int line,
    ServerOptions *options, const char *path, int userprovided)
{
	char *apath = derelativise_path(path);

	if (file == defaultkey && access(path, R_OK) != 0)
		return;
	opt_array_append2(file, line, "HostKey",
	    &options->host_key_files, &options->host_key_file_userprovided,
	    &options->num_host_key_files, apath, userprovided);
	free(apath);
}

void
servconf_add_hostcert(const char *file, const int line,
    ServerOptions *options, const char *path)
{
	char *apath = derelativise_path(path);

	opt_array_append(file, line, "HostCertificate",
	    &options->host_cert_files, &options->num_host_cert_files, apath);
	free(apath);
}

void
fill_default_server_options(ServerOptions *options)
{
	u_int i;

	/* Portable-specific options */
	if (options->use_pam == -1)
		options->use_pam = 1;

	/* Standard Options */
	if (options->num_host_key_files == 0) {
		/* fill default hostkeys for protocols */
		servconf_add_hostkey(defaultkey, 0, options,
		    _PATH_HOST_RSA_KEY_FILE, 0);
#ifdef OPENSSL_HAS_ECC
		servconf_add_hostkey(defaultkey, 0, options,
		    _PATH_HOST_ECDSA_KEY_FILE, 0);
#endif
		servconf_add_hostkey(defaultkey, 0, options,
		    _PATH_HOST_ED25519_KEY_FILE, 0);
#ifdef WITH_XMSS
		servconf_add_hostkey(defaultkey, 0, options,
		    _PATH_HOST_XMSS_KEY_FILE, 0);
#endif /* WITH_XMSS */
	}
	if (options->num_host_key_files == 0)
		fatal("No host key files found");
	/* No certificates by default */
	if (options->num_ports == 0)
		options->ports[options->num_ports++] = SSH_DEFAULT_PORT;
	if (options->address_family == -1)
		options->address_family = AF_UNSPEC;
	if (options->listen_addrs == NULL)
		add_listen_addr(options, NULL, NULL, 0);
	if (options->pid_file == NULL)
		options->pid_file = xstrdup(_PATH_SSH_DAEMON_PID_FILE);
	if (options->moduli_file == NULL)
		options->moduli_file = xstrdup(_PATH_DH_MODULI);
	if (options->login_grace_time == -1)
		options->login_grace_time = 120;
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
		options->x11_forwarding = 0;
	if (options->x11_display_offset == -1)
		options->x11_display_offset = 10;
	if (options->x11_use_localhost == -1)
		options->x11_use_localhost = 1;
	if (options->xauth_location == NULL)
		options->xauth_location = xstrdup(_PATH_XAUTH);
	if (options->permit_tty == -1)
		options->permit_tty = 1;
	if (options->permit_user_rc == -1)
		options->permit_user_rc = 1;
	if (options->strict_modes == -1)
		options->strict_modes = 1;
	if (options->tcp_keep_alive == -1)
		options->tcp_keep_alive = 1;
	if (options->log_facility == SYSLOG_FACILITY_NOT_SET)
		options->log_facility = SYSLOG_FACILITY_AUTH;
	if (options->log_level == SYSLOG_LEVEL_NOT_SET)
		options->log_level = SYSLOG_LEVEL_INFO;
	if (options->hostbased_authentication == -1)
		options->hostbased_authentication = 0;
	if (options->hostbased_uses_name_from_packet_only == -1)
		options->hostbased_uses_name_from_packet_only = 0;
	if (options->pubkey_authentication == -1)
		options->pubkey_authentication = 1;
	if (options->pubkey_auth_options == -1)
		options->pubkey_auth_options = 0;
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
	if (options->gss_strict_acceptor == -1)
		options->gss_strict_acceptor = 1;
	if (options->password_authentication == -1)
		options->password_authentication = 0;
	if (options->kbd_interactive_authentication == -1)
		options->kbd_interactive_authentication = 1;
	if (options->permit_empty_passwd == -1)
		options->permit_empty_passwd = 0;
	if (options->permit_user_env == -1) {
		options->permit_user_env = 0;
		options->permit_user_env_allowlist = NULL;
	}
	if (options->compression == -1)
#ifdef WITH_ZLIB
		options->compression = COMP_DELAYED;
#else
		options->compression = COMP_NONE;
#endif

	if (options->rekey_limit == -1)
		options->rekey_limit = 0;
	if (options->rekey_interval == -1)
		options->rekey_interval = 0;
	if (options->allow_tcp_forwarding == -1)
		options->allow_tcp_forwarding = FORWARD_ALLOW;
	if (options->allow_streamlocal_forwarding == -1)
		options->allow_streamlocal_forwarding = FORWARD_ALLOW;
	if (options->allow_agent_forwarding == -1)
		options->allow_agent_forwarding = 1;
	if (options->fwd_opts.gateway_ports == -1)
		options->fwd_opts.gateway_ports = 0;
	if (options->max_startups == -1)
		options->max_startups = 100;
	if (options->max_startups_rate == -1)
		options->max_startups_rate = 30;		/* 30% */
	if (options->max_startups_begin == -1)
		options->max_startups_begin = 10;
	if (options->per_source_max_startups == -1)
		options->per_source_max_startups = INT_MAX;
	if (options->per_source_masklen_ipv4 == -1)
		options->per_source_masklen_ipv4 = 32;
	if (options->per_source_masklen_ipv6 == -1)
		options->per_source_masklen_ipv6 = 128;
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
	if (options->num_authkeys_files == 0) {
		opt_array_append(defaultkey, 0, "AuthorizedKeysFiles",
		    &options->authorized_keys_files,
		    &options->num_authkeys_files,
		    _PATH_SSH_USER_PERMITTED_KEYS);
		opt_array_append(defaultkey, 0, "AuthorizedKeysFiles",
		    &options->authorized_keys_files,
		    &options->num_authkeys_files,
		    _PATH_SSH_USER_PERMITTED_KEYS2);
	}
	if (options->permit_tun == -1)
		options->permit_tun = SSH_TUNMODE_NO;
	if (options->ip_qos_interactive == -1)
		options->ip_qos_interactive = IPTOS_DSCP_AF21;
	if (options->ip_qos_bulk == -1)
		options->ip_qos_bulk = IPTOS_DSCP_CS1;
	if (options->version_addendum == NULL)
		options->version_addendum = xstrdup(SSH_VERSION_FREEBSD);
	if (options->fwd_opts.streamlocal_bind_mask == (mode_t)-1)
		options->fwd_opts.streamlocal_bind_mask = 0177;
	if (options->fwd_opts.streamlocal_bind_unlink == -1)
		options->fwd_opts.streamlocal_bind_unlink = 0;
	if (options->fingerprint_hash == -1)
		options->fingerprint_hash = SSH_FP_HASH_DEFAULT;
	if (options->disable_forwarding == -1)
		options->disable_forwarding = 0;
	if (options->expose_userauth_info == -1)
		options->expose_userauth_info = 0;
	if (options->sk_provider == NULL)
		options->sk_provider = xstrdup("internal");
	if (options->required_rsa_size == -1)
		options->required_rsa_size = SSH_RSA_MINIMUM_MODULUS_SIZE;
	if (options->unused_connection_timeout == -1)
		options->unused_connection_timeout = 0;
	if (options->use_blacklist == -1)
		options->use_blacklist = 0;

	assemble_algorithms(options);

	/* Turn privilege separation and sandboxing on by default */
	if (use_privsep == -1)
		use_privsep = PRIVSEP_ON;

#define CLEAR_ON_NONE(v) \
	do { \
		if (option_clear_or_none(v)) { \
			free(v); \
			v = NULL; \
		} \
	} while(0)
#define CLEAR_ON_NONE_ARRAY(v, nv, none) \
	do { \
		if (options->nv == 1 && \
		    strcasecmp(options->v[0], none) == 0) { \
			free(options->v[0]); \
			free(options->v); \
			options->v = NULL; \
			options->nv = 0; \
		} \
	} while (0)
	CLEAR_ON_NONE(options->pid_file);
	CLEAR_ON_NONE(options->xauth_location);
	CLEAR_ON_NONE(options->banner);
	CLEAR_ON_NONE(options->trusted_user_ca_keys);
	CLEAR_ON_NONE(options->revoked_keys_file);
	CLEAR_ON_NONE(options->sk_provider);
	CLEAR_ON_NONE(options->authorized_principals_file);
	CLEAR_ON_NONE(options->adm_forced_command);
	CLEAR_ON_NONE(options->chroot_directory);
	CLEAR_ON_NONE(options->routing_domain);
	CLEAR_ON_NONE(options->host_key_agent);

	for (i = 0; i < options->num_host_key_files; i++)
		CLEAR_ON_NONE(options->host_key_files[i]);
	for (i = 0; i < options->num_host_cert_files; i++)
		CLEAR_ON_NONE(options->host_cert_files[i]);

	CLEAR_ON_NONE_ARRAY(channel_timeouts, num_channel_timeouts, "none");
	CLEAR_ON_NONE_ARRAY(auth_methods, num_auth_methods, "any");
#undef CLEAR_ON_NONE
#undef CLEAR_ON_NONE_ARRAY
}

/* Keyword tokens. */
typedef enum {
	sBadOption,		/* == unknown option */
	/* Portable-specific options */
	sUsePAM,
	/* Standard Options */
	sPort, sHostKeyFile, sLoginGraceTime,
	sPermitRootLogin, sLogFacility, sLogLevel, sLogVerbose,
	sKerberosAuthentication, sKerberosOrLocalPasswd, sKerberosTicketCleanup,
	sKerberosGetAFSToken, sPasswordAuthentication,
	sKbdInteractiveAuthentication, sListenAddress, sAddressFamily,
	sPrintMotd, sPrintLastLog, sIgnoreRhosts,
	sX11Forwarding, sX11DisplayOffset, sX11UseLocalhost,
	sPermitTTY, sStrictModes, sEmptyPasswd, sTCPKeepAlive,
	sPermitUserEnvironment, sAllowTcpForwarding, sCompression,
	sRekeyLimit, sAllowUsers, sDenyUsers, sAllowGroups, sDenyGroups,
	sIgnoreUserKnownHosts, sCiphers, sMacs, sPidFile, sModuliFile,
	sGatewayPorts, sPubkeyAuthentication, sPubkeyAcceptedAlgorithms,
	sXAuthLocation, sSubsystem, sMaxStartups, sMaxAuthTries, sMaxSessions,
	sBanner, sUseDNS, sHostbasedAuthentication,
	sHostbasedUsesNameFromPacketOnly, sHostbasedAcceptedAlgorithms,
	sHostKeyAlgorithms, sPerSourceMaxStartups, sPerSourceNetBlockSize,
	sClientAliveInterval, sClientAliveCountMax, sAuthorizedKeysFile,
	sGssAuthentication, sGssCleanupCreds, sGssStrictAcceptor,
	sAcceptEnv, sSetEnv, sPermitTunnel,
	sMatch, sPermitOpen, sPermitListen, sForceCommand, sChrootDirectory,
	sUsePrivilegeSeparation, sAllowAgentForwarding,
	sHostCertificate, sInclude,
	sRevokedKeys, sTrustedUserCAKeys, sAuthorizedPrincipalsFile,
	sAuthorizedPrincipalsCommand, sAuthorizedPrincipalsCommandUser,
	sKexAlgorithms, sCASignatureAlgorithms, sIPQoS, sVersionAddendum,
	sAuthorizedKeysCommand, sAuthorizedKeysCommandUser,
	sAuthenticationMethods, sHostKeyAgent, sPermitUserRC,
	sStreamLocalBindMask, sStreamLocalBindUnlink,
	sAllowStreamLocalForwarding, sFingerprintHash, sDisableForwarding,
	sExposeAuthInfo, sRDomain, sPubkeyAuthOptions, sSecurityKeyProvider,
	sRequiredRSASize, sChannelTimeout, sUnusedConnectionTimeout,
	sUseBlacklist,
	sDeprecated, sIgnore, sUnsupported
} ServerOpCodes;

#define SSHCFG_GLOBAL		0x01	/* allowed in main section of config */
#define SSHCFG_MATCH		0x02	/* allowed inside a Match section */
#define SSHCFG_ALL		(SSHCFG_GLOBAL|SSHCFG_MATCH)
#define SSHCFG_NEVERMATCH	0x04  /* Match never matches; internal only */
#define SSHCFG_MATCH_ONLY	0x08  /* Match only in conditional blocks; internal only */

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
	{ "hostkeyagent", sHostKeyAgent, SSHCFG_GLOBAL },
	{ "pidfile", sPidFile, SSHCFG_GLOBAL },
	{ "modulifile", sModuliFile, SSHCFG_GLOBAL },
	{ "serverkeybits", sDeprecated, SSHCFG_GLOBAL },
	{ "logingracetime", sLoginGraceTime, SSHCFG_GLOBAL },
	{ "keyregenerationinterval", sDeprecated, SSHCFG_GLOBAL },
	{ "permitrootlogin", sPermitRootLogin, SSHCFG_ALL },
	{ "syslogfacility", sLogFacility, SSHCFG_GLOBAL },
	{ "loglevel", sLogLevel, SSHCFG_ALL },
	{ "logverbose", sLogVerbose, SSHCFG_ALL },
	{ "rhostsauthentication", sDeprecated, SSHCFG_GLOBAL },
	{ "rhostsrsaauthentication", sDeprecated, SSHCFG_ALL },
	{ "hostbasedauthentication", sHostbasedAuthentication, SSHCFG_ALL },
	{ "hostbasedusesnamefrompacketonly", sHostbasedUsesNameFromPacketOnly, SSHCFG_ALL },
	{ "hostbasedacceptedalgorithms", sHostbasedAcceptedAlgorithms, SSHCFG_ALL },
	{ "hostbasedacceptedkeytypes", sHostbasedAcceptedAlgorithms, SSHCFG_ALL }, /* obsolete */
	{ "hostkeyalgorithms", sHostKeyAlgorithms, SSHCFG_GLOBAL },
	{ "rsaauthentication", sDeprecated, SSHCFG_ALL },
	{ "pubkeyauthentication", sPubkeyAuthentication, SSHCFG_ALL },
	{ "pubkeyacceptedalgorithms", sPubkeyAcceptedAlgorithms, SSHCFG_ALL },
	{ "pubkeyacceptedkeytypes", sPubkeyAcceptedAlgorithms, SSHCFG_ALL }, /* obsolete */
	{ "pubkeyauthoptions", sPubkeyAuthOptions, SSHCFG_ALL },
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
	{ "gssapistrictacceptorcheck", sGssStrictAcceptor, SSHCFG_GLOBAL },
#else
	{ "gssapiauthentication", sUnsupported, SSHCFG_ALL },
	{ "gssapicleanupcredentials", sUnsupported, SSHCFG_GLOBAL },
	{ "gssapistrictacceptorcheck", sUnsupported, SSHCFG_GLOBAL },
#endif
	{ "passwordauthentication", sPasswordAuthentication, SSHCFG_ALL },
	{ "kbdinteractiveauthentication", sKbdInteractiveAuthentication, SSHCFG_ALL },
	{ "challengeresponseauthentication", sKbdInteractiveAuthentication, SSHCFG_ALL }, /* alias */
	{ "skeyauthentication", sKbdInteractiveAuthentication, SSHCFG_ALL }, /* alias */
	{ "checkmail", sDeprecated, SSHCFG_GLOBAL },
	{ "listenaddress", sListenAddress, SSHCFG_GLOBAL },
	{ "addressfamily", sAddressFamily, SSHCFG_GLOBAL },
	{ "printmotd", sPrintMotd, SSHCFG_GLOBAL },
#ifdef DISABLE_LASTLOG
	{ "printlastlog", sUnsupported, SSHCFG_GLOBAL },
#else
	{ "printlastlog", sPrintLastLog, SSHCFG_GLOBAL },
#endif
	{ "ignorerhosts", sIgnoreRhosts, SSHCFG_ALL },
	{ "ignoreuserknownhosts", sIgnoreUserKnownHosts, SSHCFG_GLOBAL },
	{ "x11forwarding", sX11Forwarding, SSHCFG_ALL },
	{ "x11displayoffset", sX11DisplayOffset, SSHCFG_ALL },
	{ "x11uselocalhost", sX11UseLocalhost, SSHCFG_ALL },
	{ "xauthlocation", sXAuthLocation, SSHCFG_GLOBAL },
	{ "strictmodes", sStrictModes, SSHCFG_GLOBAL },
	{ "permitemptypasswords", sEmptyPasswd, SSHCFG_ALL },
	{ "permituserenvironment", sPermitUserEnvironment, SSHCFG_GLOBAL },
	{ "uselogin", sDeprecated, SSHCFG_GLOBAL },
	{ "compression", sCompression, SSHCFG_GLOBAL },
	{ "rekeylimit", sRekeyLimit, SSHCFG_ALL },
	{ "tcpkeepalive", sTCPKeepAlive, SSHCFG_GLOBAL },
	{ "keepalive", sTCPKeepAlive, SSHCFG_GLOBAL },	/* obsolete alias */
	{ "allowtcpforwarding", sAllowTcpForwarding, SSHCFG_ALL },
	{ "allowagentforwarding", sAllowAgentForwarding, SSHCFG_ALL },
	{ "allowusers", sAllowUsers, SSHCFG_ALL },
	{ "denyusers", sDenyUsers, SSHCFG_ALL },
	{ "allowgroups", sAllowGroups, SSHCFG_ALL },
	{ "denygroups", sDenyGroups, SSHCFG_ALL },
	{ "ciphers", sCiphers, SSHCFG_GLOBAL },
	{ "macs", sMacs, SSHCFG_GLOBAL },
	{ "protocol", sIgnore, SSHCFG_GLOBAL },
	{ "gatewayports", sGatewayPorts, SSHCFG_ALL },
	{ "subsystem", sSubsystem, SSHCFG_GLOBAL },
	{ "maxstartups", sMaxStartups, SSHCFG_GLOBAL },
	{ "persourcemaxstartups", sPerSourceMaxStartups, SSHCFG_GLOBAL },
	{ "persourcenetblocksize", sPerSourceNetBlockSize, SSHCFG_GLOBAL },
	{ "maxauthtries", sMaxAuthTries, SSHCFG_ALL },
	{ "maxsessions", sMaxSessions, SSHCFG_ALL },
	{ "banner", sBanner, SSHCFG_ALL },
	{ "usedns", sUseDNS, SSHCFG_GLOBAL },
	{ "verifyreversemapping", sDeprecated, SSHCFG_GLOBAL },
	{ "reversemappingcheck", sDeprecated, SSHCFG_GLOBAL },
	{ "clientaliveinterval", sClientAliveInterval, SSHCFG_ALL },
	{ "clientalivecountmax", sClientAliveCountMax, SSHCFG_ALL },
	{ "authorizedkeysfile", sAuthorizedKeysFile, SSHCFG_ALL },
	{ "authorizedkeysfile2", sDeprecated, SSHCFG_ALL },
	{ "useprivilegeseparation", sDeprecated, SSHCFG_GLOBAL},
	{ "acceptenv", sAcceptEnv, SSHCFG_ALL },
	{ "setenv", sSetEnv, SSHCFG_ALL },
	{ "permittunnel", sPermitTunnel, SSHCFG_ALL },
	{ "permittty", sPermitTTY, SSHCFG_ALL },
	{ "permituserrc", sPermitUserRC, SSHCFG_ALL },
	{ "match", sMatch, SSHCFG_ALL },
	{ "permitopen", sPermitOpen, SSHCFG_ALL },
	{ "permitlisten", sPermitListen, SSHCFG_ALL },
	{ "forcecommand", sForceCommand, SSHCFG_ALL },
	{ "chrootdirectory", sChrootDirectory, SSHCFG_ALL },
	{ "hostcertificate", sHostCertificate, SSHCFG_GLOBAL },
	{ "revokedkeys", sRevokedKeys, SSHCFG_ALL },
	{ "trustedusercakeys", sTrustedUserCAKeys, SSHCFG_ALL },
	{ "authorizedprincipalsfile", sAuthorizedPrincipalsFile, SSHCFG_ALL },
	{ "kexalgorithms", sKexAlgorithms, SSHCFG_GLOBAL },
	{ "include", sInclude, SSHCFG_ALL },
	{ "ipqos", sIPQoS, SSHCFG_ALL },
	{ "authorizedkeyscommand", sAuthorizedKeysCommand, SSHCFG_ALL },
	{ "authorizedkeyscommanduser", sAuthorizedKeysCommandUser, SSHCFG_ALL },
	{ "authorizedprincipalscommand", sAuthorizedPrincipalsCommand, SSHCFG_ALL },
	{ "authorizedprincipalscommanduser", sAuthorizedPrincipalsCommandUser, SSHCFG_ALL },
	{ "versionaddendum", sVersionAddendum, SSHCFG_GLOBAL },
	{ "authenticationmethods", sAuthenticationMethods, SSHCFG_ALL },
	{ "streamlocalbindmask", sStreamLocalBindMask, SSHCFG_ALL },
	{ "streamlocalbindunlink", sStreamLocalBindUnlink, SSHCFG_ALL },
	{ "allowstreamlocalforwarding", sAllowStreamLocalForwarding, SSHCFG_ALL },
	{ "fingerprinthash", sFingerprintHash, SSHCFG_GLOBAL },
	{ "disableforwarding", sDisableForwarding, SSHCFG_ALL },
	{ "exposeauthinfo", sExposeAuthInfo, SSHCFG_ALL },
	{ "rdomain", sRDomain, SSHCFG_ALL },
	{ "casignaturealgorithms", sCASignatureAlgorithms, SSHCFG_ALL },
	{ "securitykeyprovider", sSecurityKeyProvider, SSHCFG_GLOBAL },
	{ "requiredrsasize", sRequiredRSASize, SSHCFG_ALL },
	{ "channeltimeout", sChannelTimeout, SSHCFG_ALL },
	{ "unusedconnectiontimeout", sUnusedConnectionTimeout, SSHCFG_ALL },
	{ "useblacklist", sUseBlacklist, SSHCFG_GLOBAL },
	{ "useblocklist", sUseBlacklist, SSHCFG_GLOBAL }, /* alias */

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

/* Returns an opcode name from its number */

static const char *
lookup_opcode_name(ServerOpCodes code)
{
	u_int i;

	for (i = 0; keywords[i].name != NULL; i++)
		if (keywords[i].opcode == code)
			return(keywords[i].name);
	return "UNKNOWN";
}


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
	char *expanded, *ret, cwd[PATH_MAX];

	if (strcasecmp(path, "none") == 0)
		return xstrdup("none");
	expanded = tilde_expand_filename(path, getuid());
	if (path_absolute(expanded))
		return expanded;
	if (getcwd(cwd, sizeof(cwd)) == NULL)
		fatal_f("getcwd: %s", strerror(errno));
	xasprintf(&ret, "%s/%s", cwd, expanded);
	free(expanded);
	return ret;
}

static void
add_listen_addr(ServerOptions *options, const char *addr,
    const char *rdomain, int port)
{
	u_int i;

	if (port > 0)
		add_one_listen_addr(options, addr, rdomain, port);
	else {
		for (i = 0; i < options->num_ports; i++) {
			add_one_listen_addr(options, addr, rdomain,
			    options->ports[i]);
		}
	}
}

static void
add_one_listen_addr(ServerOptions *options, const char *addr,
    const char *rdomain, int port)
{
	struct addrinfo hints, *ai, *aitop;
	char strport[NI_MAXSERV];
	int gaierr;
	u_int i;

	/* Find listen_addrs entry for this rdomain */
	for (i = 0; i < options->num_listen_addrs; i++) {
		if (rdomain == NULL && options->listen_addrs[i].rdomain == NULL)
			break;
		if (rdomain == NULL || options->listen_addrs[i].rdomain == NULL)
			continue;
		if (strcmp(rdomain, options->listen_addrs[i].rdomain) == 0)
			break;
	}
	if (i >= options->num_listen_addrs) {
		/* No entry for this rdomain; allocate one */
		if (i >= INT_MAX)
			fatal_f("too many listen addresses");
		options->listen_addrs = xrecallocarray(options->listen_addrs,
		    options->num_listen_addrs, options->num_listen_addrs + 1,
		    sizeof(*options->listen_addrs));
		i = options->num_listen_addrs++;
		if (rdomain != NULL)
			options->listen_addrs[i].rdomain = xstrdup(rdomain);
	}
	/* options->listen_addrs[i] points to the addresses for this rdomain */

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
	ai->ai_next = options->listen_addrs[i].addrs;
	options->listen_addrs[i].addrs = aitop;
}

/* Returns nonzero if the routing domain name is valid */
static int
valid_rdomain(const char *name)
{
#if defined(HAVE_SYS_VALID_RDOMAIN)
	return sys_valid_rdomain(name);
#elif defined(__OpenBSD__)
	const char *errstr;
	long long num;
	struct rt_tableinfo info;
	int mib[6];
	size_t miblen = sizeof(mib);

	if (name == NULL)
		return 1;

	num = strtonum(name, 0, 255, &errstr);
	if (errstr != NULL)
		return 0;

	/* Check whether the table actually exists */
	memset(mib, 0, sizeof(mib));
	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[4] = NET_RT_TABLE;
	mib[5] = (int)num;
	if (sysctl(mib, 6, &info, &miblen, NULL, 0) == -1)
		return 0;

	return 1;
#else /* defined(__OpenBSD__) */
	error("Routing domains are not supported on this platform");
	return 0;
#endif
}

/*
 * Queue a ListenAddress to be processed once we have all of the Ports
 * and AddressFamily options.
 */
static void
queue_listen_addr(ServerOptions *options, const char *addr,
    const char *rdomain, int port)
{
	struct queued_listenaddr *qla;

	options->queued_listen_addrs = xrecallocarray(
	    options->queued_listen_addrs,
	    options->num_queued_listens, options->num_queued_listens + 1,
	    sizeof(*options->queued_listen_addrs));
	qla = &options->queued_listen_addrs[options->num_queued_listens++];
	qla->addr = xstrdup(addr);
	qla->port = port;
	qla->rdomain = rdomain == NULL ? NULL : xstrdup(rdomain);
}

/*
 * Process queued (text) ListenAddress entries.
 */
static void
process_queued_listen_addrs(ServerOptions *options)
{
	u_int i;
	struct queued_listenaddr *qla;

	if (options->num_ports == 0)
		options->ports[options->num_ports++] = SSH_DEFAULT_PORT;
	if (options->address_family == -1)
		options->address_family = AF_UNSPEC;

	for (i = 0; i < options->num_queued_listens; i++) {
		qla = &options->queued_listen_addrs[i];
		add_listen_addr(options, qla->addr, qla->rdomain, qla->port);
		free(qla->addr);
		free(qla->rdomain);
	}
	free(options->queued_listen_addrs);
	options->queued_listen_addrs = NULL;
	options->num_queued_listens = 0;
}

/*
 * Inform channels layer of permitopen options for a single forwarding
 * direction (local/remote).
 */
static void
process_permitopen_list(struct ssh *ssh, ServerOpCodes opcode,
    char **opens, u_int num_opens)
{
	u_int i;
	int port;
	char *host, *arg, *oarg;
	int where = opcode == sPermitOpen ? FORWARD_LOCAL : FORWARD_REMOTE;
	const char *what = lookup_opcode_name(opcode);

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
		host = hpdelim(&arg);
		if (host == NULL)
			fatal_f("missing host in %s", what);
		host = cleanhostname(host);
		if (arg == NULL || ((port = permitopen_port(arg)) < 0))
			fatal_f("bad port number in %s", what);
		/* Send it to channels layer */
		channel_add_permission(ssh, FORWARD_ADM,
		    where, host, port);
		free(oarg);
	}
}

/*
 * Inform channels layer of permitopen options from configuration.
 */
void
process_permitopen(struct ssh *ssh, ServerOptions *options)
{
	process_permitopen_list(ssh, sPermitOpen,
	    options->permitted_opens, options->num_permitted_opens);
	process_permitopen_list(ssh, sPermitListen,
	    options->permitted_listens,
	    options->num_permitted_listens);
}

/* Parse a ChannelTimeout clause "pattern=interval" */
static int
parse_timeout(const char *s, char **typep, u_int *secsp)
{
	char *cp, *sdup;
	int secs;

	if (typep != NULL)
		*typep = NULL;
	if (secsp != NULL)
		*secsp = 0;
	if (s == NULL)
		return -1;
	sdup = xstrdup(s);

	if ((cp = strchr(sdup, '=')) == NULL || cp == sdup) {
		free(sdup);
		return -1;
	}
	*cp++ = '\0';
	if ((secs = convtime(cp)) < 0) {
		free(sdup);
		return -1;
	}
	/* success */
	if (typep != NULL)
		*typep = xstrdup(sdup);
	if (secsp != NULL)
		*secsp = (u_int)secs;
	free(sdup);
	return 0;
}

void
process_channel_timeouts(struct ssh *ssh, ServerOptions *options)
{
	u_int i, secs;
	char *type;

	debug3_f("setting %u timeouts", options->num_channel_timeouts);
	channel_clear_timeouts(ssh);
	for (i = 0; i < options->num_channel_timeouts; i++) {
		if (parse_timeout(options->channel_timeouts[i],
		    &type, &secs) != 0) {
			fatal_f("internal error: bad timeout %s",
			    options->channel_timeouts[i]);
		}
		channel_add_timeout(ssh, type, secs);
		free(type);
	}
}

struct connection_info *
get_connection_info(struct ssh *ssh, int populate, int use_dns)
{
	static struct connection_info ci;

	if (ssh == NULL || !populate)
		return &ci;
	ci.host = auth_get_canonical_hostname(ssh, use_dns);
	ci.address = ssh_remote_ipaddr(ssh);
	ci.laddress = ssh_local_ipaddr(ssh);
	ci.lport = ssh_local_port(ssh);
	ci.rdomain = ssh_packet_rdomain_in(ssh);
	return &ci;
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
 *  - Add Match support for pre-kex directives, eg. Ciphers.
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

static void
match_test_missing_fatal(const char *criteria, const char *attrib)
{
	fatal("'Match %s' in configuration but '%s' not in connection "
	    "test specification.", criteria, attrib);
}

/*
 * All of the attributes on a single Match line are ANDed together, so we need
 * to check every attribute and set the result to zero if any attribute does
 * not match.
 */
static int
match_cfg_line(char **condition, int line, struct connection_info *ci)
{
	int result = 1, attributes = 0, port;
	char *arg, *attrib, *cp = *condition;

	if (ci == NULL)
		debug3("checking syntax for 'Match %s'", cp);
	else
		debug3("checking match for '%s' user %s host %s addr %s "
		    "laddr %s lport %d", cp, ci->user ? ci->user : "(null)",
		    ci->host ? ci->host : "(null)",
		    ci->address ? ci->address : "(null)",
		    ci->laddress ? ci->laddress : "(null)", ci->lport);

	while ((attrib = strdelim(&cp)) && *attrib != '\0') {
		/* Terminate on comment */
		if (*attrib == '#') {
			cp = NULL; /* mark all arguments consumed */
			break;
		}
		arg = NULL;
		attributes++;
		/* Criterion "all" has no argument and must appear alone */
		if (strcasecmp(attrib, "all") == 0) {
			if (attributes > 1 || ((arg = strdelim(&cp)) != NULL &&
			    *arg != '\0' && *arg != '#')) {
				error("'all' cannot be combined with other "
				    "Match attributes");
				return -1;
			}
			if (arg != NULL && *arg == '#')
				cp = NULL; /* mark all arguments consumed */
			*condition = cp;
			return 1;
		}
		/* All other criteria require an argument */
		if ((arg = strdelim(&cp)) == NULL ||
		    *arg == '\0' || *arg == '#') {
			error("Missing Match criteria for %s", attrib);
			return -1;
		}
		if (strcasecmp(attrib, "user") == 0) {
			if (ci == NULL || (ci->test && ci->user == NULL)) {
				result = 0;
				continue;
			}
			if (ci->user == NULL)
				match_test_missing_fatal("User", "user");
			if (match_usergroup_pattern_list(ci->user, arg) != 1)
				result = 0;
			else
				debug("user %.100s matched 'User %.100s' at "
				    "line %d", ci->user, arg, line);
		} else if (strcasecmp(attrib, "group") == 0) {
			if (ci == NULL || (ci->test && ci->user == NULL)) {
				result = 0;
				continue;
			}
			if (ci->user == NULL)
				match_test_missing_fatal("Group", "user");
			switch (match_cfg_line_group(arg, line, ci->user)) {
			case -1:
				return -1;
			case 0:
				result = 0;
			}
		} else if (strcasecmp(attrib, "host") == 0) {
			if (ci == NULL || (ci->test && ci->host == NULL)) {
				result = 0;
				continue;
			}
			if (ci->host == NULL)
				match_test_missing_fatal("Host", "host");
			if (match_hostname(ci->host, arg) != 1)
				result = 0;
			else
				debug("connection from %.100s matched 'Host "
				    "%.100s' at line %d", ci->host, arg, line);
		} else if (strcasecmp(attrib, "address") == 0) {
			if (ci == NULL || (ci->test && ci->address == NULL)) {
				if (addr_match_list(NULL, arg) != 0)
					fatal("Invalid Match address argument "
					    "'%s' at line %d", arg, line);
				result = 0;
				continue;
			}
			if (ci->address == NULL)
				match_test_missing_fatal("Address", "addr");
			switch (addr_match_list(ci->address, arg)) {
			case 1:
				debug("connection from %.100s matched 'Address "
				    "%.100s' at line %d", ci->address, arg, line);
				break;
			case 0:
			case -1:
				result = 0;
				break;
			case -2:
				return -1;
			}
		} else if (strcasecmp(attrib, "localaddress") == 0){
			if (ci == NULL || (ci->test && ci->laddress == NULL)) {
				if (addr_match_list(NULL, arg) != 0)
					fatal("Invalid Match localaddress "
					    "argument '%s' at line %d", arg,
					    line);
				result = 0;
				continue;
			}
			if (ci->laddress == NULL)
				match_test_missing_fatal("LocalAddress",
				    "laddr");
			switch (addr_match_list(ci->laddress, arg)) {
			case 1:
				debug("connection from %.100s matched "
				    "'LocalAddress %.100s' at line %d",
				    ci->laddress, arg, line);
				break;
			case 0:
			case -1:
				result = 0;
				break;
			case -2:
				return -1;
			}
		} else if (strcasecmp(attrib, "localport") == 0) {
			if ((port = a2port(arg)) == -1) {
				error("Invalid LocalPort '%s' on Match line",
				    arg);
				return -1;
			}
			if (ci == NULL || (ci->test && ci->lport == -1)) {
				result = 0;
				continue;
			}
			if (ci->lport == 0)
				match_test_missing_fatal("LocalPort", "lport");
			/* TODO support port lists */
			if (port == ci->lport)
				debug("connection from %.100s matched "
				    "'LocalPort %d' at line %d",
				    ci->laddress, port, line);
			else
				result = 0;
		} else if (strcasecmp(attrib, "rdomain") == 0) {
			if (ci == NULL || (ci->test && ci->rdomain == NULL)) {
				result = 0;
				continue;
			}
			if (ci->rdomain == NULL)
				match_test_missing_fatal("RDomain", "rdomain");
			if (match_pattern_list(ci->rdomain, arg, 0) != 1)
				result = 0;
			else
				debug("user %.100s matched 'RDomain %.100s' at "
				    "line %d", ci->rdomain, arg, line);
		} else {
			error("Unsupported Match attribute %s", attrib);
			return -1;
		}
	}
	if (attributes == 0) {
		error("One or more attributes required for Match");
		return -1;
	}
	if (ci != NULL)
		debug3("match %sfound", result ? "" : "not ");
	*condition = cp;
	return result;
}

#define WHITESPACE " \t\r\n"

/* Multistate option parsing */
struct multistate {
	char *key;
	int value;
};
static const struct multistate multistate_flag[] = {
	{ "yes",			1 },
	{ "no",				0 },
	{ NULL, -1 }
};
static const struct multistate multistate_ignore_rhosts[] = {
	{ "yes",			IGNORE_RHOSTS_YES },
	{ "no",				IGNORE_RHOSTS_NO },
	{ "shosts-only",		IGNORE_RHOSTS_SHOSTS },
	{ NULL, -1 }
};
static const struct multistate multistate_addressfamily[] = {
	{ "inet",			AF_INET },
	{ "inet6",			AF_INET6 },
	{ "any",			AF_UNSPEC },
	{ NULL, -1 }
};
static const struct multistate multistate_permitrootlogin[] = {
	{ "without-password",		PERMIT_NO_PASSWD },
	{ "prohibit-password",		PERMIT_NO_PASSWD },
	{ "forced-commands-only",	PERMIT_FORCED_ONLY },
	{ "yes",			PERMIT_YES },
	{ "no",				PERMIT_NO },
	{ NULL, -1 }
};
static const struct multistate multistate_compression[] = {
#ifdef WITH_ZLIB
	{ "yes",			COMP_DELAYED },
	{ "delayed",			COMP_DELAYED },
#endif
	{ "no",				COMP_NONE },
	{ NULL, -1 }
};
static const struct multistate multistate_gatewayports[] = {
	{ "clientspecified",		2 },
	{ "yes",			1 },
	{ "no",				0 },
	{ NULL, -1 }
};
static const struct multistate multistate_tcpfwd[] = {
	{ "yes",			FORWARD_ALLOW },
	{ "all",			FORWARD_ALLOW },
	{ "no",				FORWARD_DENY },
	{ "remote",			FORWARD_REMOTE },
	{ "local",			FORWARD_LOCAL },
	{ NULL, -1 }
};

static int
process_server_config_line_depth(ServerOptions *options, char *line,
    const char *filename, int linenum, int *activep,
    struct connection_info *connectinfo, int *inc_flags, int depth,
    struct include_list *includes)
{
	char *str, ***chararrayptr, **charptr, *arg, *arg2, *p, *keyword;
	int cmdline = 0, *intptr, value, value2, n, port, oactive, r, found;
	SyslogFacility *log_facility_ptr;
	LogLevel *log_level_ptr;
	ServerOpCodes opcode;
	u_int i, *uintptr, uvalue, flags = 0;
	size_t len;
	long long val64;
	const struct multistate *multistate_ptr;
	const char *errstr;
	struct include_item *item;
	glob_t gbuf;
	char **oav = NULL, **av;
	int oac = 0, ac;
	int ret = -1;

	/* Strip trailing whitespace. Allow \f (form feed) at EOL only */
	if ((len = strlen(line)) == 0)
		return 0;
	for (len--; len > 0; len--) {
		if (strchr(WHITESPACE "\f", line[len]) == NULL)
			break;
		line[len] = '\0';
	}

	str = line;
	if ((keyword = strdelim(&str)) == NULL)
		return 0;
	/* Ignore leading whitespace */
	if (*keyword == '\0')
		keyword = strdelim(&str);
	if (!keyword || !*keyword || *keyword == '#')
		return 0;
	if (str == NULL || *str == '\0') {
		error("%s line %d: no argument after keyword \"%s\"",
		    filename, linenum, keyword);
		return -1;
	}
	intptr = NULL;
	charptr = NULL;
	opcode = parse_token(keyword, filename, linenum, &flags);

	if (argv_split(str, &oac, &oav, 1) != 0) {
		error("%s line %d: invalid quotes", filename, linenum);
		return -1;
	}
	ac = oac;
	av = oav;

	if (activep == NULL) { /* We are processing a command line directive */
		cmdline = 1;
		activep = &cmdline;
	}
	if (*activep && opcode != sMatch && opcode != sInclude)
		debug3("%s:%d setting %s %s", filename, linenum, keyword, str);
	if (*activep == 0 && !(flags & SSHCFG_MATCH)) {
		if (connectinfo == NULL) {
			fatal("%s line %d: Directive '%s' is not allowed "
			    "within a Match block", filename, linenum, keyword);
		} else { /* this is a directive we have already processed */
			ret = 0;
			goto out;
		}
	}

	switch (opcode) {
	/* Portable-specific options */
	case sUsePAM:
		intptr = &options->use_pam;
		goto parse_flag;

	/* Standard Options */
	case sBadOption:
		goto out;
	case sPort:
		/* ignore ports from configfile if cmdline specifies ports */
		if (options->ports_from_cmdline) {
			argv_consume(&ac);
			break;
		}
		if (options->num_ports >= MAX_PORTS)
			fatal("%s line %d: too many ports.",
			    filename, linenum);
		arg = argv_next(&ac, &av);
		if (!arg || *arg == '\0')
			fatal("%s line %d: missing port number.",
			    filename, linenum);
		options->ports[options->num_ports++] = a2port(arg);
		if (options->ports[options->num_ports-1] <= 0)
			fatal("%s line %d: Badly formatted port number.",
			    filename, linenum);
		break;

	case sLoginGraceTime:
		intptr = &options->login_grace_time;
 parse_time:
		arg = argv_next(&ac, &av);
		if (!arg || *arg == '\0')
			fatal("%s line %d: missing time value.",
			    filename, linenum);
		if ((value = convtime(arg)) == -1)
			fatal("%s line %d: invalid time value.",
			    filename, linenum);
		if (*activep && *intptr == -1)
			*intptr = value;
		break;

	case sListenAddress:
		arg = argv_next(&ac, &av);
		if (arg == NULL || *arg == '\0')
			fatal("%s line %d: missing address",
			    filename, linenum);
		/* check for bare IPv6 address: no "[]" and 2 or more ":" */
		if (strchr(arg, '[') == NULL && (p = strchr(arg, ':')) != NULL
		    && strchr(p+1, ':') != NULL) {
			port = 0;
			p = arg;
		} else {
			arg2 = NULL;
			p = hpdelim(&arg);
			if (p == NULL)
				fatal("%s line %d: bad address:port usage",
				    filename, linenum);
			p = cleanhostname(p);
			if (arg == NULL)
				port = 0;
			else if ((port = a2port(arg)) <= 0)
				fatal("%s line %d: bad port number",
				    filename, linenum);
		}
		/* Optional routing table */
		arg2 = NULL;
		if ((arg = argv_next(&ac, &av)) != NULL) {
			if (strcmp(arg, "rdomain") != 0 ||
			    (arg2 = argv_next(&ac, &av)) == NULL)
				fatal("%s line %d: bad ListenAddress syntax",
				    filename, linenum);
			if (!valid_rdomain(arg2))
				fatal("%s line %d: bad routing domain",
				    filename, linenum);
		}
		queue_listen_addr(options, p, arg2, port);

		break;

	case sAddressFamily:
		intptr = &options->address_family;
		multistate_ptr = multistate_addressfamily;
 parse_multistate:
		arg = argv_next(&ac, &av);
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

	case sHostKeyFile:
		arg = argv_next(&ac, &av);
		if (!arg || *arg == '\0')
			fatal("%s line %d: missing file name.",
			    filename, linenum);
		if (*activep) {
			servconf_add_hostkey(filename, linenum,
			    options, arg, 1);
		}
		break;

	case sHostKeyAgent:
		charptr = &options->host_key_agent;
		arg = argv_next(&ac, &av);
		if (!arg || *arg == '\0')
			fatal("%s line %d: missing socket name.",
			    filename, linenum);
		if (*activep && *charptr == NULL)
			*charptr = !strcmp(arg, SSH_AUTHSOCKET_ENV_NAME) ?
			    xstrdup(arg) : derelativise_path(arg);
		break;

	case sHostCertificate:
		arg = argv_next(&ac, &av);
		if (!arg || *arg == '\0')
			fatal("%s line %d: missing file name.",
			    filename, linenum);
		if (*activep)
			servconf_add_hostcert(filename, linenum, options, arg);
		break;

	case sPidFile:
		charptr = &options->pid_file;
 parse_filename:
		arg = argv_next(&ac, &av);
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

	case sModuliFile:
		charptr = &options->moduli_file;
		goto parse_filename;

	case sPermitRootLogin:
		intptr = &options->permit_root_login;
		multistate_ptr = multistate_permitrootlogin;
		goto parse_multistate;

	case sIgnoreRhosts:
		intptr = &options->ignore_rhosts;
		multistate_ptr = multistate_ignore_rhosts;
		goto parse_multistate;

	case sIgnoreUserKnownHosts:
		intptr = &options->ignore_user_known_hosts;
 parse_flag:
		multistate_ptr = multistate_flag;
		goto parse_multistate;

	case sHostbasedAuthentication:
		intptr = &options->hostbased_authentication;
		goto parse_flag;

	case sHostbasedUsesNameFromPacketOnly:
		intptr = &options->hostbased_uses_name_from_packet_only;
		goto parse_flag;

	case sHostbasedAcceptedAlgorithms:
		charptr = &options->hostbased_accepted_algos;
 parse_pubkey_algos:
		arg = argv_next(&ac, &av);
		if (!arg || *arg == '\0')
			fatal("%s line %d: Missing argument.",
			    filename, linenum);
		if (*arg != '-' &&
		    !sshkey_names_valid2(*arg == '+' || *arg == '^' ?
		    arg + 1 : arg, 1))
			fatal("%s line %d: Bad key types '%s'.",
			    filename, linenum, arg ? arg : "<NONE>");
		if (*activep && *charptr == NULL)
			*charptr = xstrdup(arg);
		break;

	case sHostKeyAlgorithms:
		charptr = &options->hostkeyalgorithms;
		goto parse_pubkey_algos;

	case sCASignatureAlgorithms:
		charptr = &options->ca_sign_algorithms;
		goto parse_pubkey_algos;

	case sPubkeyAuthentication:
		intptr = &options->pubkey_authentication;
		goto parse_flag;

	case sPubkeyAcceptedAlgorithms:
		charptr = &options->pubkey_accepted_algos;
		goto parse_pubkey_algos;

	case sPubkeyAuthOptions:
		intptr = &options->pubkey_auth_options;
		value = 0;
		while ((arg = argv_next(&ac, &av)) != NULL) {
			if (strcasecmp(arg, "none") == 0)
				continue;
			if (strcasecmp(arg, "touch-required") == 0)
				value |= PUBKEYAUTH_TOUCH_REQUIRED;
			else if (strcasecmp(arg, "verify-required") == 0)
				value |= PUBKEYAUTH_VERIFY_REQUIRED;
			else {
				error("%s line %d: unsupported %s option %s",
				    filename, linenum, keyword, arg);
				goto out;
			}
		}
		if (*activep && *intptr == -1)
			*intptr = value;
		break;

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

	case sGssStrictAcceptor:
		intptr = &options->gss_strict_acceptor;
		goto parse_flag;

	case sPasswordAuthentication:
		intptr = &options->password_authentication;
		goto parse_flag;

	case sKbdInteractiveAuthentication:
		intptr = &options->kbd_interactive_authentication;
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
 parse_int:
		arg = argv_next(&ac, &av);
		if ((errstr = atoi_err(arg, &value)) != NULL)
			fatal("%s line %d: %s integer value %s.",
			    filename, linenum, keyword, errstr);
		if (*activep && *intptr == -1)
			*intptr = value;
		break;

	case sX11UseLocalhost:
		intptr = &options->x11_use_localhost;
		goto parse_flag;

	case sXAuthLocation:
		charptr = &options->xauth_location;
		goto parse_filename;

	case sPermitTTY:
		intptr = &options->permit_tty;
		goto parse_flag;

	case sPermitUserRC:
		intptr = &options->permit_user_rc;
		goto parse_flag;

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
		charptr = &options->permit_user_env_allowlist;
		arg = argv_next(&ac, &av);
		if (!arg || *arg == '\0')
			fatal("%s line %d: %s missing argument.",
			    filename, linenum, keyword);
		value = 0;
		p = NULL;
		if (strcmp(arg, "yes") == 0)
			value = 1;
		else if (strcmp(arg, "no") == 0)
			value = 0;
		else {
			/* Pattern-list specified */
			value = 1;
			p = xstrdup(arg);
		}
		if (*activep && *intptr == -1) {
			*intptr = value;
			*charptr = p;
			p = NULL;
		}
		free(p);
		break;

	case sCompression:
		intptr = &options->compression;
		multistate_ptr = multistate_compression;
		goto parse_multistate;

	case sRekeyLimit:
		arg = argv_next(&ac, &av);
		if (!arg || *arg == '\0')
			fatal("%s line %d: %s missing argument.",
			    filename, linenum, keyword);
		if (strcmp(arg, "default") == 0) {
			val64 = 0;
		} else {
			if (scan_scaled(arg, &val64) == -1)
				fatal("%.200s line %d: Bad %s number '%s': %s",
				    filename, linenum, keyword,
				    arg, strerror(errno));
			if (val64 != 0 && val64 < 16)
				fatal("%.200s line %d: %s too small",
				    filename, linenum, keyword);
		}
		if (*activep && options->rekey_limit == -1)
			options->rekey_limit = val64;
		if (ac != 0) { /* optional rekey interval present */
			if (strcmp(av[0], "none") == 0) {
				(void)argv_next(&ac, &av);	/* discard */
				break;
			}
			intptr = &options->rekey_interval;
			goto parse_time;
		}
		break;

	case sGatewayPorts:
		intptr = &options->fwd_opts.gateway_ports;
		multistate_ptr = multistate_gatewayports;
		goto parse_multistate;

	case sUseDNS:
		intptr = &options->use_dns;
		goto parse_flag;

	case sLogFacility:
		log_facility_ptr = &options->log_facility;
		arg = argv_next(&ac, &av);
		value = log_facility_number(arg);
		if (value == SYSLOG_FACILITY_NOT_SET)
			fatal("%.200s line %d: unsupported log facility '%s'",
			    filename, linenum, arg ? arg : "<NONE>");
		if (*log_facility_ptr == -1)
			*log_facility_ptr = (SyslogFacility) value;
		break;

	case sLogLevel:
		log_level_ptr = &options->log_level;
		arg = argv_next(&ac, &av);
		value = log_level_number(arg);
		if (value == SYSLOG_LEVEL_NOT_SET)
			fatal("%.200s line %d: unsupported log level '%s'",
			    filename, linenum, arg ? arg : "<NONE>");
		if (*activep && *log_level_ptr == -1)
			*log_level_ptr = (LogLevel) value;
		break;

	case sLogVerbose:
		found = options->num_log_verbose == 0;
		i = 0;
		while ((arg = argv_next(&ac, &av)) != NULL) {
			if (*arg == '\0') {
				error("%s line %d: keyword %s empty argument",
				    filename, linenum, keyword);
				goto out;
			}
			/* Allow "none" only in first position */
			if (strcasecmp(arg, "none") == 0) {
				if (i > 0 || ac > 0) {
					error("%s line %d: keyword %s \"none\" "
					    "argument must appear alone.",
					    filename, linenum, keyword);
					goto out;
				}
			}
			i++;
			if (!found || !*activep)
				continue;
			opt_array_append(filename, linenum, keyword,
			    &options->log_verbose, &options->num_log_verbose,
			    arg);
		}
		break;

	case sAllowTcpForwarding:
		intptr = &options->allow_tcp_forwarding;
		multistate_ptr = multistate_tcpfwd;
		goto parse_multistate;

	case sAllowStreamLocalForwarding:
		intptr = &options->allow_streamlocal_forwarding;
		multistate_ptr = multistate_tcpfwd;
		goto parse_multistate;

	case sAllowAgentForwarding:
		intptr = &options->allow_agent_forwarding;
		goto parse_flag;

	case sDisableForwarding:
		intptr = &options->disable_forwarding;
		goto parse_flag;

	case sAllowUsers:
		chararrayptr = &options->allow_users;
		uintptr = &options->num_allow_users;
 parse_allowdenyusers:
		while ((arg = argv_next(&ac, &av)) != NULL) {
			if (*arg == '\0' ||
			    match_user(NULL, NULL, NULL, arg) == -1)
				fatal("%s line %d: invalid %s pattern: \"%s\"",
				    filename, linenum, keyword, arg);
			if (!*activep)
				continue;
			opt_array_append(filename, linenum, keyword,
			    chararrayptr, uintptr, arg);
		}
		break;

	case sDenyUsers:
		chararrayptr = &options->deny_users;
		uintptr = &options->num_deny_users;
		goto parse_allowdenyusers;

	case sAllowGroups:
		chararrayptr = &options->allow_groups;
		uintptr = &options->num_allow_groups;
 parse_allowdenygroups:
		while ((arg = argv_next(&ac, &av)) != NULL) {
			if (*arg == '\0')
				fatal("%s line %d: empty %s pattern",
				    filename, linenum, keyword);
			if (!*activep)
				continue;
			opt_array_append(filename, linenum, keyword,
			    chararrayptr, uintptr, arg);
		}
		break;

	case sDenyGroups:
		chararrayptr = &options->deny_groups;
		uintptr = &options->num_deny_groups;
		goto parse_allowdenygroups;

	case sCiphers:
		arg = argv_next(&ac, &av);
		if (!arg || *arg == '\0')
			fatal("%s line %d: %s missing argument.",
			    filename, linenum, keyword);
		if (*arg != '-' &&
		    !ciphers_valid(*arg == '+' || *arg == '^' ? arg + 1 : arg))
			fatal("%s line %d: Bad SSH2 cipher spec '%s'.",
			    filename, linenum, arg ? arg : "<NONE>");
		if (options->ciphers == NULL)
			options->ciphers = xstrdup(arg);
		break;

	case sMacs:
		arg = argv_next(&ac, &av);
		if (!arg || *arg == '\0')
			fatal("%s line %d: %s missing argument.",
			    filename, linenum, keyword);
		if (*arg != '-' &&
		    !mac_valid(*arg == '+' || *arg == '^' ? arg + 1 : arg))
			fatal("%s line %d: Bad SSH2 mac spec '%s'.",
			    filename, linenum, arg ? arg : "<NONE>");
		if (options->macs == NULL)
			options->macs = xstrdup(arg);
		break;

	case sKexAlgorithms:
		arg = argv_next(&ac, &av);
		if (!arg || *arg == '\0')
			fatal("%s line %d: %s missing argument.",
			    filename, linenum, keyword);
		if (*arg != '-' &&
		    !kex_names_valid(*arg == '+' || *arg == '^' ?
		    arg + 1 : arg))
			fatal("%s line %d: Bad SSH2 KexAlgorithms '%s'.",
			    filename, linenum, arg ? arg : "<NONE>");
		if (options->kex_algorithms == NULL)
			options->kex_algorithms = xstrdup(arg);
		break;

	case sSubsystem:
		if (options->num_subsystems >= MAX_SUBSYSTEMS) {
			fatal("%s line %d: too many subsystems defined.",
			    filename, linenum);
		}
		arg = argv_next(&ac, &av);
		if (!arg || *arg == '\0')
			fatal("%s line %d: %s missing argument.",
			    filename, linenum, keyword);
		if (!*activep) {
			arg = argv_next(&ac, &av);
			break;
		}
		for (i = 0; i < options->num_subsystems; i++)
			if (strcmp(arg, options->subsystem_name[i]) == 0)
				fatal("%s line %d: Subsystem '%s' "
				    "already defined.", filename, linenum, arg);
		options->subsystem_name[options->num_subsystems] = xstrdup(arg);
		arg = argv_next(&ac, &av);
		if (!arg || *arg == '\0')
			fatal("%s line %d: Missing subsystem command.",
			    filename, linenum);
		options->subsystem_command[options->num_subsystems] = xstrdup(arg);

		/* Collect arguments (separate to executable) */
		p = xstrdup(arg);
		len = strlen(p) + 1;
		while ((arg = argv_next(&ac, &av)) != NULL) {
			len += 1 + strlen(arg);
			p = xreallocarray(p, 1, len);
			strlcat(p, " ", len);
			strlcat(p, arg, len);
		}
		options->subsystem_args[options->num_subsystems] = p;
		options->num_subsystems++;
		break;

	case sMaxStartups:
		arg = argv_next(&ac, &av);
		if (!arg || *arg == '\0')
			fatal("%s line %d: %s missing argument.",
			    filename, linenum, keyword);
		if ((n = sscanf(arg, "%d:%d:%d",
		    &options->max_startups_begin,
		    &options->max_startups_rate,
		    &options->max_startups)) == 3) {
			if (options->max_startups_begin >
			    options->max_startups ||
			    options->max_startups_rate > 100 ||
			    options->max_startups_rate < 1)
				fatal("%s line %d: Invalid %s spec.",
				    filename, linenum, keyword);
		} else if (n != 1)
			fatal("%s line %d: Invalid %s spec.",
			    filename, linenum, keyword);
		else
			options->max_startups = options->max_startups_begin;
		if (options->max_startups <= 0 ||
		    options->max_startups_begin <= 0)
			fatal("%s line %d: Invalid %s spec.",
			    filename, linenum, keyword);
		break;

	case sPerSourceNetBlockSize:
		arg = argv_next(&ac, &av);
		if (!arg || *arg == '\0')
			fatal("%s line %d: %s missing argument.",
			    filename, linenum, keyword);
		switch (n = sscanf(arg, "%d:%d", &value, &value2)) {
		case 2:
			if (value2 < 0 || value2 > 128)
				n = -1;
			/* FALLTHROUGH */
		case 1:
			if (value < 0 || value > 32)
				n = -1;
		}
		if (n != 1 && n != 2)
			fatal("%s line %d: Invalid %s spec.",
			    filename, linenum, keyword);
		if (*activep) {
			options->per_source_masklen_ipv4 = value;
			options->per_source_masklen_ipv6 = value2;
		}
		break;

	case sPerSourceMaxStartups:
		arg = argv_next(&ac, &av);
		if (!arg || *arg == '\0')
			fatal("%s line %d: %s missing argument.",
			    filename, linenum, keyword);
		if (strcmp(arg, "none") == 0) { /* no limit */
			value = INT_MAX;
		} else {
			if ((errstr = atoi_err(arg, &value)) != NULL)
				fatal("%s line %d: %s integer value %s.",
				    filename, linenum, keyword, errstr);
		}
		if (*activep)
			options->per_source_max_startups = value;
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
		uvalue = options->num_authkeys_files;
		while ((arg = argv_next(&ac, &av)) != NULL) {
			if (*arg == '\0') {
				error("%s line %d: keyword %s empty argument",
				    filename, linenum, keyword);
				goto out;
			}
			arg2 = tilde_expand_filename(arg, getuid());
			if (*activep && uvalue == 0) {
				opt_array_append(filename, linenum, keyword,
				    &options->authorized_keys_files,
				    &options->num_authkeys_files, arg2);
			}
			free(arg2);
		}
		break;

	case sAuthorizedPrincipalsFile:
		charptr = &options->authorized_principals_file;
		arg = argv_next(&ac, &av);
		if (!arg || *arg == '\0')
			fatal("%s line %d: %s missing argument.",
			    filename, linenum, keyword);
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
		while ((arg = argv_next(&ac, &av)) != NULL) {
			if (*arg == '\0' || strchr(arg, '=') != NULL)
				fatal("%s line %d: Invalid environment name.",
				    filename, linenum);
			if (!*activep)
				continue;
			opt_array_append(filename, linenum, keyword,
			    &options->accept_env, &options->num_accept_env,
			    arg);
		}
		break;

	case sSetEnv:
		uvalue = options->num_setenv;
		while ((arg = argv_next(&ac, &av)) != NULL) {
			if (*arg == '\0' || strchr(arg, '=') == NULL)
				fatal("%s line %d: Invalid environment.",
				    filename, linenum);
			if (!*activep || uvalue != 0)
				continue;
			if (lookup_setenv_in_list(arg, options->setenv,
			    options->num_setenv) != NULL) {
				debug2("%s line %d: ignoring duplicate env "
				    "name \"%.64s\"", filename, linenum, arg);
				continue;
			}
			opt_array_append(filename, linenum, keyword,
			    &options->setenv, &options->num_setenv, arg);
		}
		break;

	case sPermitTunnel:
		intptr = &options->permit_tun;
		arg = argv_next(&ac, &av);
		if (!arg || *arg == '\0')
			fatal("%s line %d: %s missing argument.",
			    filename, linenum, keyword);
		value = -1;
		for (i = 0; tunmode_desc[i].val != -1; i++)
			if (strcmp(tunmode_desc[i].text, arg) == 0) {
				value = tunmode_desc[i].val;
				break;
			}
		if (value == -1)
			fatal("%s line %d: bad %s argument %s",
			    filename, linenum, keyword, arg);
		if (*activep && *intptr == -1)
			*intptr = value;
		break;

	case sInclude:
		if (cmdline) {
			fatal("Include directive not supported as a "
			    "command-line option");
		}
		value = 0;
		while ((arg2 = argv_next(&ac, &av)) != NULL) {
			if (*arg2 == '\0') {
				error("%s line %d: keyword %s empty argument",
				    filename, linenum, keyword);
				goto out;
			}
			value++;
			found = 0;
			if (*arg2 != '/' && *arg2 != '~') {
				xasprintf(&arg, "%s/%s", SSHDIR, arg2);
			} else
				arg = xstrdup(arg2);

			/*
			 * Don't let included files clobber the containing
			 * file's Match state.
			 */
			oactive = *activep;

			/* consult cache of include files */
			TAILQ_FOREACH(item, includes, entry) {
				if (strcmp(item->selector, arg) != 0)
					continue;
				if (item->filename != NULL) {
					parse_server_config_depth(options,
					    item->filename, item->contents,
					    includes, connectinfo,
					    (*inc_flags & SSHCFG_MATCH_ONLY
					        ? SSHCFG_MATCH_ONLY : (oactive
					            ? 0 : SSHCFG_NEVERMATCH)),
					    activep, depth + 1);
				}
				found = 1;
				*activep = oactive;
			}
			if (found != 0) {
				free(arg);
				continue;
			}

			/* requested glob was not in cache */
			debug2("%s line %d: new include %s",
			    filename, linenum, arg);
			if ((r = glob(arg, 0, NULL, &gbuf)) != 0) {
				if (r != GLOB_NOMATCH) {
					fatal("%s line %d: include \"%s\" glob "
					    "failed", filename, linenum, arg);
				}
				/*
				 * If no entry matched then record a
				 * placeholder to skip later glob calls.
				 */
				debug2("%s line %d: no match for %s",
				    filename, linenum, arg);
				item = xcalloc(1, sizeof(*item));
				item->selector = strdup(arg);
				TAILQ_INSERT_TAIL(includes,
				    item, entry);
			}
			if (gbuf.gl_pathc > INT_MAX)
				fatal_f("too many glob results");
			for (n = 0; n < (int)gbuf.gl_pathc; n++) {
				debug2("%s line %d: including %s",
				    filename, linenum, gbuf.gl_pathv[n]);
				item = xcalloc(1, sizeof(*item));
				item->selector = strdup(arg);
				item->filename = strdup(gbuf.gl_pathv[n]);
				if ((item->contents = sshbuf_new()) == NULL)
					fatal_f("sshbuf_new failed");
				load_server_config(item->filename,
				    item->contents);
				parse_server_config_depth(options,
				    item->filename, item->contents,
				    includes, connectinfo,
				    (*inc_flags & SSHCFG_MATCH_ONLY
				        ? SSHCFG_MATCH_ONLY : (oactive
				            ? 0 : SSHCFG_NEVERMATCH)),
				    activep, depth + 1);
				*activep = oactive;
				TAILQ_INSERT_TAIL(includes, item, entry);
			}
			globfree(&gbuf);
			free(arg);
		}
		if (value == 0) {
			fatal("%s line %d: %s missing filename argument",
			    filename, linenum, keyword);
		}
		break;

	case sMatch:
		if (cmdline)
			fatal("Match directive not supported as a command-line "
			    "option");
		value = match_cfg_line(&str, linenum,
		    (*inc_flags & SSHCFG_NEVERMATCH ? NULL : connectinfo));
		if (value < 0)
			fatal("%s line %d: Bad Match condition", filename,
			    linenum);
		*activep = (*inc_flags & SSHCFG_NEVERMATCH) ? 0 : value;
		/*
		 * The MATCH_ONLY flag is applicable only until the first
		 * match block.
		 */
		*inc_flags &= ~SSHCFG_MATCH_ONLY;
		/*
		 * If match_cfg_line() didn't consume all its arguments then
		 * arrange for the extra arguments check below to fail.
		 */
		if (str == NULL || *str == '\0')
			argv_consume(&ac);
		break;

	case sPermitListen:
	case sPermitOpen:
		if (opcode == sPermitListen) {
			uintptr = &options->num_permitted_listens;
			chararrayptr = &options->permitted_listens;
		} else {
			uintptr = &options->num_permitted_opens;
			chararrayptr = &options->permitted_opens;
		}
		arg = argv_next(&ac, &av);
		if (!arg || *arg == '\0')
			fatal("%s line %d: %s missing argument.",
			    filename, linenum, keyword);
		uvalue = *uintptr;	/* modified later */
		if (strcmp(arg, "any") == 0 || strcmp(arg, "none") == 0) {
			if (*activep && uvalue == 0) {
				*uintptr = 1;
				*chararrayptr = xcalloc(1,
				    sizeof(**chararrayptr));
				(*chararrayptr)[0] = xstrdup(arg);
			}
			break;
		}
		for (; arg != NULL && *arg != '\0'; arg = argv_next(&ac, &av)) {
			if (opcode == sPermitListen &&
			    strchr(arg, ':') == NULL) {
				/*
				 * Allow bare port number for PermitListen
				 * to indicate a wildcard listen host.
				 */
				xasprintf(&arg2, "*:%s", arg);
			} else {
				arg2 = xstrdup(arg);
				p = hpdelim(&arg);
				if (p == NULL) {
					fatal("%s line %d: %s missing host",
					    filename, linenum, keyword);
				}
				p = cleanhostname(p);
			}
			if (arg == NULL ||
			    ((port = permitopen_port(arg)) < 0)) {
				fatal("%s line %d: %s bad port number",
				    filename, linenum, keyword);
			}
			if (*activep && uvalue == 0) {
				opt_array_append(filename, linenum, keyword,
				    chararrayptr, uintptr, arg2);
			}
			free(arg2);
		}
		break;

	case sForceCommand:
		if (str == NULL || *str == '\0')
			fatal("%s line %d: %s missing argument.",
			    filename, linenum, keyword);
		len = strspn(str, WHITESPACE);
		if (*activep && options->adm_forced_command == NULL)
			options->adm_forced_command = xstrdup(str + len);
		argv_consume(&ac);
		break;

	case sChrootDirectory:
		charptr = &options->chroot_directory;

		arg = argv_next(&ac, &av);
		if (!arg || *arg == '\0')
			fatal("%s line %d: %s missing argument.",
			    filename, linenum, keyword);
		if (*activep && *charptr == NULL)
			*charptr = xstrdup(arg);
		break;

	case sTrustedUserCAKeys:
		charptr = &options->trusted_user_ca_keys;
		goto parse_filename;

	case sRevokedKeys:
		charptr = &options->revoked_keys_file;
		goto parse_filename;

	case sSecurityKeyProvider:
		charptr = &options->sk_provider;
		arg = argv_next(&ac, &av);
		if (!arg || *arg == '\0')
			fatal("%s line %d: %s missing argument.",
			    filename, linenum, keyword);
		if (*activep && *charptr == NULL) {
			*charptr = strcasecmp(arg, "internal") == 0 ?
			    xstrdup(arg) : derelativise_path(arg);
			/* increase optional counter */
			if (intptr != NULL)
				*intptr = *intptr + 1;
		}
		break;

	case sIPQoS:
		arg = argv_next(&ac, &av);
		if (!arg || *arg == '\0')
			fatal("%s line %d: %s missing argument.",
			    filename, linenum, keyword);
		if ((value = parse_ipqos(arg)) == -1)
			fatal("%s line %d: Bad %s value: %s",
			    filename, linenum, keyword, arg);
		arg = argv_next(&ac, &av);
		if (arg == NULL)
			value2 = value;
		else if ((value2 = parse_ipqos(arg)) == -1)
			fatal("%s line %d: Bad %s value: %s",
			    filename, linenum, keyword, arg);
		if (*activep) {
			options->ip_qos_interactive = value;
			options->ip_qos_bulk = value2;
		}
		break;

	case sVersionAddendum:
		if (str == NULL || *str == '\0')
			fatal("%s line %d: %s missing argument.",
			    filename, linenum, keyword);
		len = strspn(str, WHITESPACE);
		if (strchr(str + len, '\r') != NULL) {
			fatal("%.200s line %d: Invalid %s argument",
			    filename, linenum, keyword);
		}
		if ((arg = strchr(line, '#')) != NULL) {
			*arg = '\0';
			rtrim(line);
		}
		if (*activep && options->version_addendum == NULL) {
			if (strcasecmp(str + len, "none") == 0)
				options->version_addendum = xstrdup("");
			else
				options->version_addendum = xstrdup(str + len);
		}
		argv_consume(&ac);
		break;

	case sAuthorizedKeysCommand:
		charptr = &options->authorized_keys_command;
 parse_command:
		len = strspn(str, WHITESPACE);
		if (str[len] != '/' && strcasecmp(str + len, "none") != 0) {
			fatal("%.200s line %d: %s must be an absolute path",
			    filename, linenum, keyword);
		}
		if (*activep && options->authorized_keys_command == NULL)
			*charptr = xstrdup(str + len);
		argv_consume(&ac);
		break;

	case sAuthorizedKeysCommandUser:
		charptr = &options->authorized_keys_command_user;
 parse_localuser:
		arg = argv_next(&ac, &av);
		if (!arg || *arg == '\0') {
			fatal("%s line %d: missing %s argument.",
			    filename, linenum, keyword);
		}
		if (*activep && *charptr == NULL)
			*charptr = xstrdup(arg);
		break;

	case sAuthorizedPrincipalsCommand:
		charptr = &options->authorized_principals_command;
		goto parse_command;

	case sAuthorizedPrincipalsCommandUser:
		charptr = &options->authorized_principals_command_user;
		goto parse_localuser;

	case sAuthenticationMethods:
		found = options->num_auth_methods == 0;
		value = 0; /* seen "any" pseudo-method */
		value2 = 0; /* successfully parsed any method */
		while ((arg = argv_next(&ac, &av)) != NULL) {
			if (strcmp(arg, "any") == 0) {
				if (options->num_auth_methods > 0) {
					fatal("%s line %d: \"any\" must "
					    "appear alone in %s",
					    filename, linenum, keyword);
				}
				value = 1;
			} else if (value) {
				fatal("%s line %d: \"any\" must appear "
				    "alone in %s", filename, linenum, keyword);
			} else if (auth2_methods_valid(arg, 0) != 0) {
				fatal("%s line %d: invalid %s method list.",
				    filename, linenum, keyword);
			}
			value2 = 1;
			if (!found || !*activep)
				continue;
			opt_array_append(filename, linenum, keyword,
			    &options->auth_methods,
			    &options->num_auth_methods, arg);
		}
		if (value2 == 0) {
			fatal("%s line %d: no %s specified",
			    filename, linenum, keyword);
		}
		break;

	case sStreamLocalBindMask:
		arg = argv_next(&ac, &av);
		if (!arg || *arg == '\0')
			fatal("%s line %d: %s missing argument.",
			    filename, linenum, keyword);
		/* Parse mode in octal format */
		value = strtol(arg, &p, 8);
		if (arg == p || value < 0 || value > 0777)
			fatal("%s line %d: Invalid %s.",
			    filename, linenum, keyword);
		if (*activep)
			options->fwd_opts.streamlocal_bind_mask = (mode_t)value;
		break;

	case sStreamLocalBindUnlink:
		intptr = &options->fwd_opts.streamlocal_bind_unlink;
		goto parse_flag;

	case sFingerprintHash:
		arg = argv_next(&ac, &av);
		if (!arg || *arg == '\0')
			fatal("%s line %d: %s missing argument.",
			    filename, linenum, keyword);
		if ((value = ssh_digest_alg_by_name(arg)) == -1)
			fatal("%.200s line %d: Invalid %s algorithm \"%s\".",
			    filename, linenum, keyword, arg);
		if (*activep)
			options->fingerprint_hash = value;
		break;

	case sExposeAuthInfo:
		intptr = &options->expose_userauth_info;
		goto parse_flag;

	case sRDomain:
#if !defined(__OpenBSD__) && !defined(HAVE_SYS_SET_PROCESS_RDOMAIN)
		fatal("%s line %d: setting RDomain not supported on this "
		    "platform.", filename, linenum);
#endif
		charptr = &options->routing_domain;
		arg = argv_next(&ac, &av);
		if (!arg || *arg == '\0')
			fatal("%s line %d: %s missing argument.",
			    filename, linenum, keyword);
		if (strcasecmp(arg, "none") != 0 && strcmp(arg, "%D") != 0 &&
		    !valid_rdomain(arg))
			fatal("%s line %d: invalid routing domain",
			    filename, linenum);
		if (*activep && *charptr == NULL)
			*charptr = xstrdup(arg);
		break;

	case sRequiredRSASize:
		intptr = &options->required_rsa_size;
		goto parse_int;

	case sChannelTimeout:
		uvalue = options->num_channel_timeouts;
		i = 0;
		while ((arg = argv_next(&ac, &av)) != NULL) {
			/* Allow "none" only in first position */
			if (strcasecmp(arg, "none") == 0) {
				if (i > 0 || ac > 0) {
					error("%s line %d: keyword %s \"none\" "
					    "argument must appear alone.",
					    filename, linenum, keyword);
					goto out;
				}
			} else if (parse_timeout(arg, NULL, NULL) != 0) {
				fatal("%s line %d: invalid channel timeout %s",
				    filename, linenum, arg);
			}
			if (!*activep || uvalue != 0)
				continue;
			opt_array_append(filename, linenum, keyword,
			    &options->channel_timeouts,
			    &options->num_channel_timeouts, arg);
		}
		break;

	case sUnusedConnectionTimeout:
		intptr = &options->unused_connection_timeout;
		/* peek at first arg for "none" so we can reuse parse_time */
		if (av[0] != NULL && strcasecmp(av[0], "none") == 0) {
			(void)argv_next(&ac, &av); /* consume arg */
			if (*activep)
				*intptr = 0;
			break;
		}
		goto parse_time;

	case sUseBlacklist:
		intptr = &options->use_blacklist;
		goto parse_flag;

	case sDeprecated:
	case sIgnore:
	case sUnsupported:
		do_log2(opcode == sIgnore ?
		    SYSLOG_LEVEL_DEBUG2 : SYSLOG_LEVEL_INFO,
		    "%s line %d: %s option %s", filename, linenum,
		    opcode == sUnsupported ? "Unsupported" : "Deprecated",
		    keyword);
		argv_consume(&ac);
		break;

	default:
		fatal("%s line %d: Missing handler for opcode %s (%d)",
		    filename, linenum, keyword, opcode);
	}
	/* Check that there is no garbage at end of line. */
	if (ac > 0) {
		error("%.200s line %d: keyword %s extra arguments "
		    "at end of line", filename, linenum, keyword);
		goto out;
	}

	/* success */
	ret = 0;
 out:
	argv_free(oav, oac);
	return ret;
}

int
process_server_config_line(ServerOptions *options, char *line,
    const char *filename, int linenum, int *activep,
    struct connection_info *connectinfo, struct include_list *includes)
{
	int inc_flags = 0;

	return process_server_config_line_depth(options, line, filename,
	    linenum, activep, connectinfo, &inc_flags, 0, includes);
}


/* Reads the server configuration file. */

void
load_server_config(const char *filename, struct sshbuf *conf)
{
	struct stat st;
	char *line = NULL, *cp;
	size_t linesize = 0;
	FILE *f;
	int r;

	debug2_f("filename %s", filename);
	if ((f = fopen(filename, "r")) == NULL) {
		perror(filename);
		exit(1);
	}
	sshbuf_reset(conf);
	/* grow buffer, so realloc is avoided for large config files */
	if (fstat(fileno(f), &st) == 0 && st.st_size > 0 &&
	    (r = sshbuf_allocate(conf, st.st_size)) != 0)
		fatal_fr(r, "allocate");
	while (getline(&line, &linesize, f) != -1) {
		/*
		 * Strip whitespace
		 * NB - preserve newlines, they are needed to reproduce
		 * line numbers later for error messages
		 */
		cp = line + strspn(line, " \t\r");
		if ((r = sshbuf_put(conf, cp, strlen(cp))) != 0)
			fatal_fr(r, "sshbuf_put");
	}
	free(line);
	if ((r = sshbuf_put_u8(conf, 0)) != 0)
		fatal_fr(r, "sshbuf_put_u8");
	fclose(f);
	debug2_f("done config len = %zu", sshbuf_len(conf));
}

void
parse_server_match_config(ServerOptions *options,
   struct include_list *includes, struct connection_info *connectinfo)
{
	ServerOptions mo;

	initialize_server_options(&mo);
	parse_server_config(&mo, "reprocess config", cfg, includes,
	    connectinfo, 0);
	copy_set_server_options(options, &mo, 0);
}

int parse_server_match_testspec(struct connection_info *ci, char *spec)
{
	char *p;

	while ((p = strsep(&spec, ",")) && *p != '\0') {
		if (strncmp(p, "addr=", 5) == 0) {
			ci->address = xstrdup(p + 5);
		} else if (strncmp(p, "host=", 5) == 0) {
			ci->host = xstrdup(p + 5);
		} else if (strncmp(p, "user=", 5) == 0) {
			ci->user = xstrdup(p + 5);
		} else if (strncmp(p, "laddr=", 6) == 0) {
			ci->laddress = xstrdup(p + 6);
		} else if (strncmp(p, "rdomain=", 8) == 0) {
			ci->rdomain = xstrdup(p + 8);
		} else if (strncmp(p, "lport=", 6) == 0) {
			ci->lport = a2port(p + 6);
			if (ci->lport == -1) {
				fprintf(stderr, "Invalid port '%s' in test mode"
				    " specification %s\n", p+6, p);
				return -1;
			}
		} else {
			fprintf(stderr, "Invalid test mode specification %s\n",
			    p);
			return -1;
		}
	}
	return 0;
}

/*
 * Copy any supported values that are set.
 *
 * If the preauth flag is set, we do not bother copying the string or
 * array values that are not used pre-authentication, because any that we
 * do use must be explicitly sent in mm_getpwnamallow().
 */
void
copy_set_server_options(ServerOptions *dst, ServerOptions *src, int preauth)
{
#define M_CP_INTOPT(n) do {\
	if (src->n != -1) \
		dst->n = src->n; \
} while (0)

	M_CP_INTOPT(password_authentication);
	M_CP_INTOPT(gss_authentication);
	M_CP_INTOPT(pubkey_authentication);
	M_CP_INTOPT(pubkey_auth_options);
	M_CP_INTOPT(kerberos_authentication);
	M_CP_INTOPT(hostbased_authentication);
	M_CP_INTOPT(hostbased_uses_name_from_packet_only);
	M_CP_INTOPT(kbd_interactive_authentication);
	M_CP_INTOPT(permit_root_login);
	M_CP_INTOPT(permit_empty_passwd);
	M_CP_INTOPT(ignore_rhosts);

	M_CP_INTOPT(allow_tcp_forwarding);
	M_CP_INTOPT(allow_streamlocal_forwarding);
	M_CP_INTOPT(allow_agent_forwarding);
	M_CP_INTOPT(disable_forwarding);
	M_CP_INTOPT(expose_userauth_info);
	M_CP_INTOPT(permit_tun);
	M_CP_INTOPT(fwd_opts.gateway_ports);
	M_CP_INTOPT(fwd_opts.streamlocal_bind_unlink);
	M_CP_INTOPT(x11_display_offset);
	M_CP_INTOPT(x11_forwarding);
	M_CP_INTOPT(x11_use_localhost);
	M_CP_INTOPT(permit_tty);
	M_CP_INTOPT(permit_user_rc);
	M_CP_INTOPT(max_sessions);
	M_CP_INTOPT(max_authtries);
	M_CP_INTOPT(client_alive_count_max);
	M_CP_INTOPT(client_alive_interval);
	M_CP_INTOPT(ip_qos_interactive);
	M_CP_INTOPT(ip_qos_bulk);
	M_CP_INTOPT(rekey_limit);
	M_CP_INTOPT(rekey_interval);
	M_CP_INTOPT(log_level);
	M_CP_INTOPT(required_rsa_size);
	M_CP_INTOPT(unused_connection_timeout);

	/*
	 * The bind_mask is a mode_t that may be unsigned, so we can't use
	 * M_CP_INTOPT - it does a signed comparison that causes compiler
	 * warnings.
	 */
	if (src->fwd_opts.streamlocal_bind_mask != (mode_t)-1) {
		dst->fwd_opts.streamlocal_bind_mask =
		    src->fwd_opts.streamlocal_bind_mask;
	}

	/* M_CP_STROPT and M_CP_STRARRAYOPT should not appear before here */
#define M_CP_STROPT(n) do {\
	if (src->n != NULL && dst->n != src->n) { \
		free(dst->n); \
		dst->n = src->n; \
	} \
} while(0)
#define M_CP_STRARRAYOPT(s, num_s) do {\
	u_int i; \
	if (src->num_s != 0) { \
		for (i = 0; i < dst->num_s; i++) \
			free(dst->s[i]); \
		free(dst->s); \
		dst->s = xcalloc(src->num_s, sizeof(*dst->s)); \
		for (i = 0; i < src->num_s; i++) \
			dst->s[i] = xstrdup(src->s[i]); \
		dst->num_s = src->num_s; \
	} \
} while(0)

	/* See comment in servconf.h */
	COPY_MATCH_STRING_OPTS();

	/* Arguments that accept '+...' need to be expanded */
	assemble_algorithms(dst);

	/*
	 * The only things that should be below this point are string options
	 * which are only used after authentication.
	 */
	if (preauth)
		return;

	/* These options may be "none" to clear a global setting */
	M_CP_STROPT(adm_forced_command);
	if (option_clear_or_none(dst->adm_forced_command)) {
		free(dst->adm_forced_command);
		dst->adm_forced_command = NULL;
	}
	M_CP_STROPT(chroot_directory);
	if (option_clear_or_none(dst->chroot_directory)) {
		free(dst->chroot_directory);
		dst->chroot_directory = NULL;
	}
}

#undef M_CP_INTOPT
#undef M_CP_STROPT
#undef M_CP_STRARRAYOPT

#define SERVCONF_MAX_DEPTH	16
static void
parse_server_config_depth(ServerOptions *options, const char *filename,
    struct sshbuf *conf, struct include_list *includes,
    struct connection_info *connectinfo, int flags, int *activep, int depth)
{
	int linenum, bad_options = 0;
	char *cp, *obuf, *cbuf;

	if (depth < 0 || depth > SERVCONF_MAX_DEPTH)
		fatal("Too many recursive configuration includes");

	debug2_f("config %s len %zu%s", filename, sshbuf_len(conf),
	    (flags & SSHCFG_NEVERMATCH ? " [checking syntax only]" : ""));

	if ((obuf = cbuf = sshbuf_dup_string(conf)) == NULL)
		fatal_f("sshbuf_dup_string failed");
	linenum = 1;
	while ((cp = strsep(&cbuf, "\n")) != NULL) {
		if (process_server_config_line_depth(options, cp,
		    filename, linenum++, activep, connectinfo, &flags,
		    depth, includes) != 0)
			bad_options++;
	}
	free(obuf);
	if (bad_options > 0)
		fatal("%s: terminating, %d bad configuration options",
		    filename, bad_options);
}

void
parse_server_config(ServerOptions *options, const char *filename,
    struct sshbuf *conf, struct include_list *includes,
    struct connection_info *connectinfo, int reexec)
{
	int active = connectinfo ? 0 : 1;
	parse_server_config_depth(options, filename, conf, includes,
	    connectinfo, (connectinfo ? SSHCFG_MATCH_ONLY : 0), &active, 0);
	if (!reexec)
		process_queued_listen_addrs(options);
}

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
fmt_intarg(ServerOpCodes code, int val)
{
	if (val == -1)
		return "unset";
	switch (code) {
	case sAddressFamily:
		return fmt_multistate_int(val, multistate_addressfamily);
	case sPermitRootLogin:
		return fmt_multistate_int(val, multistate_permitrootlogin);
	case sGatewayPorts:
		return fmt_multistate_int(val, multistate_gatewayports);
	case sCompression:
		return fmt_multistate_int(val, multistate_compression);
	case sAllowTcpForwarding:
		return fmt_multistate_int(val, multistate_tcpfwd);
	case sAllowStreamLocalForwarding:
		return fmt_multistate_int(val, multistate_tcpfwd);
	case sIgnoreRhosts:
		return fmt_multistate_int(val, multistate_ignore_rhosts);
	case sFingerprintHash:
		return ssh_digest_alg_name(val);
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

static void
dump_cfg_int(ServerOpCodes code, int val)
{
	if (code == sUnusedConnectionTimeout && val == 0) {
		printf("%s none\n", lookup_opcode_name(code));
		return;
	}
	printf("%s %d\n", lookup_opcode_name(code), val);
}

static void
dump_cfg_oct(ServerOpCodes code, int val)
{
	printf("%s 0%o\n", lookup_opcode_name(code), val);
}

static void
dump_cfg_fmtint(ServerOpCodes code, int val)
{
	printf("%s %s\n", lookup_opcode_name(code), fmt_intarg(code, val));
}

static void
dump_cfg_string(ServerOpCodes code, const char *val)
{
	printf("%s %s\n", lookup_opcode_name(code),
	    val == NULL ? "none" : val);
}

static void
dump_cfg_strarray(ServerOpCodes code, u_int count, char **vals)
{
	u_int i;

	for (i = 0; i < count; i++)
		printf("%s %s\n", lookup_opcode_name(code), vals[i]);
}

static void
dump_cfg_strarray_oneline(ServerOpCodes code, u_int count, char **vals)
{
	u_int i;

	switch (code) {
	case sAuthenticationMethods:
	case sChannelTimeout:
		break;
	default:
		if (count <= 0)
			return;
		break;
	}

	printf("%s", lookup_opcode_name(code));
	for (i = 0; i < count; i++)
		printf(" %s",  vals[i]);
	if (code == sAuthenticationMethods && count == 0)
		printf(" any");
	else if (code == sChannelTimeout && count == 0)
		printf(" none");
	printf("\n");
}

static char *
format_listen_addrs(struct listenaddr *la)
{
	int r;
	struct addrinfo *ai;
	char addr[NI_MAXHOST], port[NI_MAXSERV];
	char *laddr1 = xstrdup(""), *laddr2 = NULL;

	/*
	 * ListenAddress must be after Port.  add_one_listen_addr pushes
	 * addresses onto a stack, so to maintain ordering we need to
	 * print these in reverse order.
	 */
	for (ai = la->addrs; ai; ai = ai->ai_next) {
		if ((r = getnameinfo(ai->ai_addr, ai->ai_addrlen, addr,
		    sizeof(addr), port, sizeof(port),
		    NI_NUMERICHOST|NI_NUMERICSERV)) != 0) {
			error("getnameinfo: %.100s", ssh_gai_strerror(r));
			continue;
		}
		laddr2 = laddr1;
		if (ai->ai_family == AF_INET6) {
			xasprintf(&laddr1, "listenaddress [%s]:%s%s%s\n%s",
			    addr, port,
			    la->rdomain == NULL ? "" : " rdomain ",
			    la->rdomain == NULL ? "" : la->rdomain,
			    laddr2);
		} else {
			xasprintf(&laddr1, "listenaddress %s:%s%s%s\n%s",
			    addr, port,
			    la->rdomain == NULL ? "" : " rdomain ",
			    la->rdomain == NULL ? "" : la->rdomain,
			    laddr2);
		}
		free(laddr2);
	}
	return laddr1;
}

void
dump_config(ServerOptions *o)
{
	char *s;
	u_int i;

	/* these are usually at the top of the config */
	for (i = 0; i < o->num_ports; i++)
		printf("port %d\n", o->ports[i]);
	dump_cfg_fmtint(sAddressFamily, o->address_family);

	for (i = 0; i < o->num_listen_addrs; i++) {
		s = format_listen_addrs(&o->listen_addrs[i]);
		printf("%s", s);
		free(s);
	}

	/* integer arguments */
#ifdef USE_PAM
	dump_cfg_fmtint(sUsePAM, o->use_pam);
#endif
	dump_cfg_int(sLoginGraceTime, o->login_grace_time);
	dump_cfg_int(sX11DisplayOffset, o->x11_display_offset);
	dump_cfg_int(sMaxAuthTries, o->max_authtries);
	dump_cfg_int(sMaxSessions, o->max_sessions);
	dump_cfg_int(sClientAliveInterval, o->client_alive_interval);
	dump_cfg_int(sClientAliveCountMax, o->client_alive_count_max);
	dump_cfg_int(sRequiredRSASize, o->required_rsa_size);
	dump_cfg_oct(sStreamLocalBindMask, o->fwd_opts.streamlocal_bind_mask);
	dump_cfg_int(sUnusedConnectionTimeout, o->unused_connection_timeout);

	/* formatted integer arguments */
	dump_cfg_fmtint(sPermitRootLogin, o->permit_root_login);
	dump_cfg_fmtint(sIgnoreRhosts, o->ignore_rhosts);
	dump_cfg_fmtint(sIgnoreUserKnownHosts, o->ignore_user_known_hosts);
	dump_cfg_fmtint(sHostbasedAuthentication, o->hostbased_authentication);
	dump_cfg_fmtint(sHostbasedUsesNameFromPacketOnly,
	    o->hostbased_uses_name_from_packet_only);
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
	dump_cfg_fmtint(sPasswordAuthentication, o->password_authentication);
	dump_cfg_fmtint(sKbdInteractiveAuthentication,
	    o->kbd_interactive_authentication);
	dump_cfg_fmtint(sPrintMotd, o->print_motd);
#ifndef DISABLE_LASTLOG
	dump_cfg_fmtint(sPrintLastLog, o->print_lastlog);
#endif
	dump_cfg_fmtint(sX11Forwarding, o->x11_forwarding);
	dump_cfg_fmtint(sX11UseLocalhost, o->x11_use_localhost);
	dump_cfg_fmtint(sPermitTTY, o->permit_tty);
	dump_cfg_fmtint(sPermitUserRC, o->permit_user_rc);
	dump_cfg_fmtint(sStrictModes, o->strict_modes);
	dump_cfg_fmtint(sTCPKeepAlive, o->tcp_keep_alive);
	dump_cfg_fmtint(sEmptyPasswd, o->permit_empty_passwd);
	dump_cfg_fmtint(sCompression, o->compression);
	dump_cfg_fmtint(sGatewayPorts, o->fwd_opts.gateway_ports);
	dump_cfg_fmtint(sUseDNS, o->use_dns);
	dump_cfg_fmtint(sAllowTcpForwarding, o->allow_tcp_forwarding);
	dump_cfg_fmtint(sAllowAgentForwarding, o->allow_agent_forwarding);
	dump_cfg_fmtint(sDisableForwarding, o->disable_forwarding);
	dump_cfg_fmtint(sAllowStreamLocalForwarding, o->allow_streamlocal_forwarding);
	dump_cfg_fmtint(sStreamLocalBindUnlink, o->fwd_opts.streamlocal_bind_unlink);
	dump_cfg_fmtint(sFingerprintHash, o->fingerprint_hash);
	dump_cfg_fmtint(sExposeAuthInfo, o->expose_userauth_info);
	dump_cfg_fmtint(sUseBlacklist, o->use_blacklist);

	/* string arguments */
	dump_cfg_string(sPidFile, o->pid_file);
	dump_cfg_string(sModuliFile, o->moduli_file);
	dump_cfg_string(sXAuthLocation, o->xauth_location);
	dump_cfg_string(sCiphers, o->ciphers);
	dump_cfg_string(sMacs, o->macs);
	dump_cfg_string(sBanner, o->banner);
	dump_cfg_string(sForceCommand, o->adm_forced_command);
	dump_cfg_string(sChrootDirectory, o->chroot_directory);
	dump_cfg_string(sTrustedUserCAKeys, o->trusted_user_ca_keys);
	dump_cfg_string(sRevokedKeys, o->revoked_keys_file);
	dump_cfg_string(sSecurityKeyProvider, o->sk_provider);
	dump_cfg_string(sAuthorizedPrincipalsFile,
	    o->authorized_principals_file);
	dump_cfg_string(sVersionAddendum, *o->version_addendum == '\0'
	    ? "none" : o->version_addendum);
	dump_cfg_string(sAuthorizedKeysCommand, o->authorized_keys_command);
	dump_cfg_string(sAuthorizedKeysCommandUser, o->authorized_keys_command_user);
	dump_cfg_string(sAuthorizedPrincipalsCommand, o->authorized_principals_command);
	dump_cfg_string(sAuthorizedPrincipalsCommandUser, o->authorized_principals_command_user);
	dump_cfg_string(sHostKeyAgent, o->host_key_agent);
	dump_cfg_string(sKexAlgorithms, o->kex_algorithms);
	dump_cfg_string(sCASignatureAlgorithms, o->ca_sign_algorithms);
	dump_cfg_string(sHostbasedAcceptedAlgorithms, o->hostbased_accepted_algos);
	dump_cfg_string(sHostKeyAlgorithms, o->hostkeyalgorithms);
	dump_cfg_string(sPubkeyAcceptedAlgorithms, o->pubkey_accepted_algos);
#if defined(__OpenBSD__) || defined(HAVE_SYS_SET_PROCESS_RDOMAIN)
	dump_cfg_string(sRDomain, o->routing_domain);
#endif

	/* string arguments requiring a lookup */
	dump_cfg_string(sLogLevel, log_level_name(o->log_level));
	dump_cfg_string(sLogFacility, log_facility_name(o->log_facility));

	/* string array arguments */
	dump_cfg_strarray_oneline(sAuthorizedKeysFile, o->num_authkeys_files,
	    o->authorized_keys_files);
	dump_cfg_strarray(sHostKeyFile, o->num_host_key_files,
	    o->host_key_files);
	dump_cfg_strarray(sHostCertificate, o->num_host_cert_files,
	    o->host_cert_files);
	dump_cfg_strarray(sAllowUsers, o->num_allow_users, o->allow_users);
	dump_cfg_strarray(sDenyUsers, o->num_deny_users, o->deny_users);
	dump_cfg_strarray(sAllowGroups, o->num_allow_groups, o->allow_groups);
	dump_cfg_strarray(sDenyGroups, o->num_deny_groups, o->deny_groups);
	dump_cfg_strarray(sAcceptEnv, o->num_accept_env, o->accept_env);
	dump_cfg_strarray(sSetEnv, o->num_setenv, o->setenv);
	dump_cfg_strarray_oneline(sAuthenticationMethods,
	    o->num_auth_methods, o->auth_methods);
	dump_cfg_strarray_oneline(sLogVerbose,
	    o->num_log_verbose, o->log_verbose);
	dump_cfg_strarray_oneline(sChannelTimeout,
	    o->num_channel_timeouts, o->channel_timeouts);

	/* other arguments */
	for (i = 0; i < o->num_subsystems; i++)
		printf("subsystem %s %s\n", o->subsystem_name[i],
		    o->subsystem_args[i]);

	printf("maxstartups %d:%d:%d\n", o->max_startups_begin,
	    o->max_startups_rate, o->max_startups);
	printf("persourcemaxstartups ");
	if (o->per_source_max_startups == INT_MAX)
		printf("none\n");
	else
		printf("%d\n", o->per_source_max_startups);
	printf("persourcenetblocksize %d:%d\n", o->per_source_masklen_ipv4,
	    o->per_source_masklen_ipv6);

	s = NULL;
	for (i = 0; tunmode_desc[i].val != -1; i++) {
		if (tunmode_desc[i].val == o->permit_tun) {
			s = tunmode_desc[i].text;
			break;
		}
	}
	dump_cfg_string(sPermitTunnel, s);

	printf("ipqos %s ", iptos2str(o->ip_qos_interactive));
	printf("%s\n", iptos2str(o->ip_qos_bulk));

	printf("rekeylimit %llu %d\n", (unsigned long long)o->rekey_limit,
	    o->rekey_interval);

	printf("permitopen");
	if (o->num_permitted_opens == 0)
		printf(" any");
	else {
		for (i = 0; i < o->num_permitted_opens; i++)
			printf(" %s", o->permitted_opens[i]);
	}
	printf("\n");
	printf("permitlisten");
	if (o->num_permitted_listens == 0)
		printf(" any");
	else {
		for (i = 0; i < o->num_permitted_listens; i++)
			printf(" %s", o->permitted_listens[i]);
	}
	printf("\n");

	if (o->permit_user_env_allowlist == NULL) {
		dump_cfg_fmtint(sPermitUserEnvironment, o->permit_user_env);
	} else {
		printf("permituserenvironment %s\n",
		    o->permit_user_env_allowlist);
	}

	printf("pubkeyauthoptions");
	if (o->pubkey_auth_options == 0)
		printf(" none");
	if (o->pubkey_auth_options & PUBKEYAUTH_TOUCH_REQUIRED)
		printf(" touch-required");
	if (o->pubkey_auth_options & PUBKEYAUTH_VERIFY_REQUIRED)
		printf(" verify-required");
	printf("\n");
}
