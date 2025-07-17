/* $OpenBSD: servconf.h,v 1.168 2024/09/15 01:18:26 djm Exp $ */

/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * Definitions for server configuration data and for the functions reading it.
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 */

#ifndef SERVCONF_H
#define SERVCONF_H

#include <openbsd-compat/sys-queue.h>

#define MAX_PORTS		256	/* Max # ports. */

/* permit_root_login */
#define	PERMIT_NOT_SET		-1
#define	PERMIT_NO		0
#define	PERMIT_FORCED_ONLY	1
#define	PERMIT_NO_PASSWD	2
#define	PERMIT_YES		3

/* PermitOpen */
#define PERMITOPEN_ANY		0
#define PERMITOPEN_NONE		-2

/* IgnoreRhosts */
#define IGNORE_RHOSTS_NO	0
#define IGNORE_RHOSTS_YES	1
#define IGNORE_RHOSTS_SHOSTS	2

#define DEFAULT_AUTH_FAIL_MAX	6	/* Default for MaxAuthTries */
#define DEFAULT_SESSIONS_MAX	10	/* Default for MaxSessions */

/* Magic name for internal sftp-server */
#define INTERNAL_SFTP_NAME	"internal-sftp"

/* PubkeyAuthOptions flags */
#define PUBKEYAUTH_TOUCH_REQUIRED	(1)
#define PUBKEYAUTH_VERIFY_REQUIRED	(1<<1)

struct ssh;

/*
 * Used to store addresses from ListenAddr directives. These may be
 * incomplete, as they may specify addresses that need to be merged
 * with any ports requested by ListenPort.
 */
struct queued_listenaddr {
	char *addr;
	int port; /* <=0 if unspecified */
	char *rdomain;
};

/* Resolved listen addresses, grouped by optional routing domain */
struct listenaddr {
	char *rdomain;
	struct addrinfo *addrs;
};

#define PER_SOURCE_PENALTY_OVERFLOW_DENY_ALL	1
#define PER_SOURCE_PENALTY_OVERFLOW_PERMISSIVE	2
struct per_source_penalty {
	int	enabled;
	int	max_sources4;
	int	max_sources6;
	int	overflow_mode;
	int	overflow_mode6;
	int	penalty_crash;
	int	penalty_grace;
	int	penalty_authfail;
	int	penalty_noauth;
	int	penalty_refuseconnection;
	int	penalty_max;
	int	penalty_min;
};

typedef struct {
	u_int	num_ports;
	u_int	ports_from_cmdline;
	int	ports[MAX_PORTS];	/* Port number to listen on. */
	struct queued_listenaddr *queued_listen_addrs;
	u_int	num_queued_listens;
	struct listenaddr *listen_addrs;
	u_int	num_listen_addrs;
	int	address_family;		/* Address family used by the server. */

	char	*routing_domain;	/* Bind session to routing domain */

	char   **host_key_files;	/* Files containing host keys. */
	int	*host_key_file_userprovided; /* Key was specified by user. */
	u_int	num_host_key_files;     /* Number of files for host keys. */
	char   **host_cert_files;	/* Files containing host certs. */
	u_int	num_host_cert_files;	/* Number of files for host certs. */

	char   *host_key_agent;		/* ssh-agent socket for host keys. */
	char   *pid_file;		/* Where to put our pid */
	char   *moduli_file;		/* moduli file for DH-GEX */
	int     login_grace_time;	/* Disconnect if no auth in this time
					 * (sec). */
	int     permit_root_login;	/* PERMIT_*, see above */
	int     ignore_rhosts;	/* Ignore .rhosts and .shosts. */
	int     ignore_user_known_hosts;	/* Ignore ~/.ssh/known_hosts
						 * for RhostsRsaAuth */
	int     print_motd;	/* If true, print /etc/motd. */
	int	print_lastlog;	/* If true, print lastlog */
	int     x11_forwarding;	/* If true, permit inet (spoofing) X11 fwd. */
	int     x11_display_offset;	/* What DISPLAY number to start
					 * searching at */
	int     x11_use_localhost;	/* If true, use localhost for fake X11 server. */
	char   *xauth_location;	/* Location of xauth program */
	int	permit_tty;	/* If false, deny pty allocation */
	int	permit_user_rc;	/* If false, deny ~/.ssh/rc execution */
	int     strict_modes;	/* If true, require string home dir modes. */
	int     tcp_keep_alive;	/* If true, set SO_KEEPALIVE. */
	int	ip_qos_interactive;	/* IP ToS/DSCP/class for interactive */
	int	ip_qos_bulk;		/* IP ToS/DSCP/class for bulk traffic */
	char   *ciphers;	/* Supported SSH2 ciphers. */
	char   *macs;		/* Supported SSH2 macs. */
	char   *kex_algorithms;	/* SSH2 kex methods in order of preference. */
	struct ForwardOptions fwd_opts;	/* forwarding options */
	SyslogFacility log_facility;	/* Facility for system logging. */
	LogLevel log_level;	/* Level for system logging. */
	u_int	num_log_verbose;	/* Verbose log overrides */
	char	**log_verbose;
	int     hostbased_authentication;	/* If true, permit ssh2 hostbased auth */
	int     hostbased_uses_name_from_packet_only; /* experimental */
	char   *hostbased_accepted_algos; /* Algos allowed for hostbased */
	char   *hostkeyalgorithms;	/* SSH2 server key types */
	char   *ca_sign_algorithms;	/* Allowed CA signature algorithms */
	int     pubkey_authentication;	/* If true, permit ssh2 pubkey authentication. */
	char   *pubkey_accepted_algos;	/* Signature algos allowed for pubkey */
	int	pubkey_auth_options;	/* -1 or mask of PUBKEYAUTH_* flags */
	int     kerberos_authentication;	/* If true, permit Kerberos
						 * authentication. */
	int     kerberos_or_local_passwd;	/* If true, permit kerberos
						 * and any other password
						 * authentication mechanism,
						 * such as SecurID or
						 * /etc/passwd */
	int     kerberos_ticket_cleanup;	/* If true, destroy ticket
						 * file on logout. */
	int     kerberos_get_afs_token;		/* If true, try to get AFS token if
						 * authenticated with Kerberos. */
	int     gss_authentication;	/* If true, permit GSSAPI authentication */
	int     gss_cleanup_creds;	/* If true, destroy cred cache on logout */
	int     gss_strict_acceptor;	/* If true, restrict the GSSAPI acceptor name */
	int     password_authentication;	/* If true, permit password
						 * authentication. */
	int     kbd_interactive_authentication;	/* If true, permit */
	int     permit_empty_passwd;	/* If false, do not permit empty
					 * passwords. */
	int     permit_user_env;	/* If true, read ~/.ssh/environment */
	char   *permit_user_env_allowlist; /* pattern-list of allowed env names */
	int     compression;	/* If true, compression is allowed */
	int	allow_tcp_forwarding; /* One of FORWARD_* */
	int	allow_streamlocal_forwarding; /* One of FORWARD_* */
	int	allow_agent_forwarding;
	int	disable_forwarding;
	u_int num_allow_users;
	char   **allow_users;
	u_int num_deny_users;
	char   **deny_users;
	u_int num_allow_groups;
	char   **allow_groups;
	u_int num_deny_groups;
	char   **deny_groups;

	u_int num_subsystems;
	char   **subsystem_name;
	char   **subsystem_command;
	char   **subsystem_args;

	u_int num_accept_env;
	char   **accept_env;
	u_int num_setenv;
	char   **setenv;

	int	max_startups_begin;
	int	max_startups_rate;
	int	max_startups;
	int	per_source_max_startups;
	int	per_source_masklen_ipv4;
	int	per_source_masklen_ipv6;
	char	*per_source_penalty_exempt;
	struct per_source_penalty per_source_penalty;
	int	max_authtries;
	int	max_sessions;
	char   *banner;			/* SSH-2 banner message */
	int	use_dns;
	int	client_alive_interval;	/*
					 * poke the client this often to
					 * see if it's still there
					 */
	int	client_alive_count_max;	/*
					 * If the client is unresponsive
					 * for this many intervals above,
					 * disconnect the session
					 */

	u_int	num_authkeys_files;	/* Files containing public keys */
	char   **authorized_keys_files;

	char   *adm_forced_command;

	int	use_pam;		/* Enable auth via PAM */
	char   *pam_service_name;

	int	permit_tun;

	char   **permitted_opens;	/* May also be one of PERMITOPEN_* */
	u_int   num_permitted_opens;
	char   **permitted_listens; /* May also be one of PERMITOPEN_* */
	u_int   num_permitted_listens;

	char   *chroot_directory;
	char   *revoked_keys_file;
	char   *trusted_user_ca_keys;
	char   *authorized_keys_command;
	char   *authorized_keys_command_user;
	char   *authorized_principals_file;
	char   *authorized_principals_command;
	char   *authorized_principals_command_user;

	int64_t rekey_limit;
	int	rekey_interval;

	char   *version_addendum;	/* Appended to SSH banner */

	u_int	num_auth_methods;
	char   **auth_methods;

	int	fingerprint_hash;
	int	expose_userauth_info;
	u_int64_t timing_secret;
	char   *sk_provider;
	int	required_rsa_size;	/* minimum size of RSA keys */

	char	**channel_timeouts;	/* inactivity timeout by channel type */
	u_int	num_channel_timeouts;

	int	unused_connection_timeout;

	char   *sshd_session_path;

	int	refuse_connection;

	int	use_blacklist;
}       ServerOptions;

/* Information about the incoming connection as used by Match */
struct connection_info {
	const char *user;
	int user_invalid;
	const char *host;	/* possibly resolved hostname */
	const char *address;	/* remote address */
	const char *laddress;	/* local address */
	int lport;		/* local port */
	const char *rdomain;	/* routing domain if available */
	int test;		/* test mode, allow some attributes to be
				 * unspecified */
};

/* List of included files for re-exec from the parsed configuration */
struct include_item {
	char *selector;
	char *filename;
	struct sshbuf *contents;
	TAILQ_ENTRY(include_item) entry;
};
TAILQ_HEAD(include_list, include_item);


/*
 * These are string config options that must be copied between the
 * Match sub-config and the main config, and must be sent from the
 * privsep child to the privsep master. We use a macro to ensure all
 * the options are copied and the copies are done in the correct order.
 *
 * NB. an option must appear in servconf.c:copy_set_server_options() or
 * COPY_MATCH_STRING_OPTS here but never both.
 */
#define COPY_MATCH_STRING_OPTS() do { \
		M_CP_STROPT(banner); \
		M_CP_STROPT(trusted_user_ca_keys); \
		M_CP_STROPT(revoked_keys_file); \
		M_CP_STROPT(authorized_keys_command); \
		M_CP_STROPT(authorized_keys_command_user); \
		M_CP_STROPT(authorized_principals_file); \
		M_CP_STROPT(authorized_principals_command); \
		M_CP_STROPT(authorized_principals_command_user); \
		M_CP_STROPT(hostbased_accepted_algos); \
		M_CP_STROPT(pubkey_accepted_algos); \
		M_CP_STROPT(ca_sign_algorithms); \
		M_CP_STROPT(routing_domain); \
		M_CP_STROPT(permit_user_env_allowlist); \
		M_CP_STROPT(pam_service_name); \
		M_CP_STRARRAYOPT(authorized_keys_files, num_authkeys_files); \
		M_CP_STRARRAYOPT(allow_users, num_allow_users); \
		M_CP_STRARRAYOPT(deny_users, num_deny_users); \
		M_CP_STRARRAYOPT(allow_groups, num_allow_groups); \
		M_CP_STRARRAYOPT(deny_groups, num_deny_groups); \
		M_CP_STRARRAYOPT(accept_env, num_accept_env); \
		M_CP_STRARRAYOPT(setenv, num_setenv); \
		M_CP_STRARRAYOPT(auth_methods, num_auth_methods); \
		M_CP_STRARRAYOPT(permitted_opens, num_permitted_opens); \
		M_CP_STRARRAYOPT(permitted_listens, num_permitted_listens); \
		M_CP_STRARRAYOPT(channel_timeouts, num_channel_timeouts); \
		M_CP_STRARRAYOPT(log_verbose, num_log_verbose); \
		M_CP_STRARRAYOPT(subsystem_name, num_subsystems); \
		M_CP_STRARRAYOPT(subsystem_command, num_subsystems); \
		M_CP_STRARRAYOPT(subsystem_args, num_subsystems); \
	} while (0)

void	 initialize_server_options(ServerOptions *);
void	 fill_default_server_options(ServerOptions *);
int	 process_server_config_line(ServerOptions *, char *, const char *, int,
	    int *, struct connection_info *, struct include_list *includes);
void	 load_server_config(const char *, struct sshbuf *);
void	 parse_server_config(ServerOptions *, const char *, struct sshbuf *,
	    struct include_list *includes, struct connection_info *, int);
void	 parse_server_match_config(ServerOptions *,
	    struct include_list *includes, struct connection_info *);
int	 parse_server_match_testspec(struct connection_info *, char *);
void	 servconf_merge_subsystems(ServerOptions *, ServerOptions *);
void	 copy_set_server_options(ServerOptions *, ServerOptions *, int);
void	 dump_config(ServerOptions *);
char	*derelativise_path(const char *);
void	 servconf_add_hostkey(const char *, const int,
	    ServerOptions *, const char *path, int);
void	 servconf_add_hostcert(const char *, const int,
	    ServerOptions *, const char *path);

#endif				/* SERVCONF_H */
