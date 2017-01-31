/* $OpenBSD: servconf.h,v 1.123 2016/11/30 03:00:05 djm Exp $ */

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

#define MAX_PORTS		256	/* Max # ports. */

#define MAX_ALLOW_USERS		256	/* Max # users on allow list. */
#define MAX_DENY_USERS		256	/* Max # users on deny list. */
#define MAX_ALLOW_GROUPS	256	/* Max # groups on allow list. */
#define MAX_DENY_GROUPS		256	/* Max # groups on deny list. */
#define MAX_SUBSYSTEMS		256	/* Max # subsystems. */
#define MAX_HOSTKEYS		256	/* Max # hostkeys. */
#define MAX_HOSTCERTS		256	/* Max # host certificates. */
#define MAX_ACCEPT_ENV		256	/* Max # of env vars. */
#define MAX_MATCH_GROUPS	256	/* Max # of groups for Match. */
#define MAX_AUTHKEYS_FILES	256	/* Max # of authorized_keys files. */
#define MAX_AUTH_METHODS	256	/* Max # of AuthenticationMethods. */

/* permit_root_login */
#define	PERMIT_NOT_SET		-1
#define	PERMIT_NO		0
#define	PERMIT_FORCED_ONLY	1
#define	PERMIT_NO_PASSWD	2
#define	PERMIT_YES		3

/* use_privsep */
#define PRIVSEP_OFF		0
#define PRIVSEP_ON		1
#define PRIVSEP_NOSANDBOX	2

/* AllowTCPForwarding */
#define FORWARD_DENY		0
#define FORWARD_REMOTE		(1)
#define FORWARD_LOCAL		(1<<1)
#define FORWARD_ALLOW		(FORWARD_REMOTE|FORWARD_LOCAL)

#define DEFAULT_AUTH_FAIL_MAX	6	/* Default for MaxAuthTries */
#define DEFAULT_SESSIONS_MAX	10	/* Default for MaxSessions */

/* Magic name for internal sftp-server */
#define INTERNAL_SFTP_NAME	"internal-sftp"

typedef struct {
	u_int	num_ports;
	u_int	ports_from_cmdline;
	int	ports[MAX_PORTS];	/* Port number to listen on. */
	u_int	num_queued_listens;
	char   **queued_listen_addrs;
	int    *queued_listen_ports;
	struct addrinfo *listen_addrs;	/* Addresses on which the server listens. */
	int     address_family;		/* Address family used by the server. */
	char   *host_key_files[MAX_HOSTKEYS];	/* Files containing host keys. */
	int     num_host_key_files;     /* Number of files for host keys. */
	char   *host_cert_files[MAX_HOSTCERTS];	/* Files containing host certs. */
	int     num_host_cert_files;     /* Number of files for host certs. */
	char   *host_key_agent;		 /* ssh-agent socket for host keys. */
	char   *pid_file;	/* Where to put our pid */
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
	int     hostbased_authentication;	/* If true, permit ssh2 hostbased auth */
	int     hostbased_uses_name_from_packet_only; /* experimental */
	char   *hostbased_key_types;	/* Key types allowed for hostbased */
	char   *hostkeyalgorithms;	/* SSH2 server key types */
	int     pubkey_authentication;	/* If true, permit ssh2 pubkey authentication. */
	char   *pubkey_key_types;	/* Key types allowed for public key */
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
	int     challenge_response_authentication;
	int     permit_empty_passwd;	/* If false, do not permit empty
					 * passwords. */
	int     permit_user_env;	/* If true, read ~/.ssh/environment */
	int     compression;	/* If true, compression is allowed */
	int	allow_tcp_forwarding; /* One of FORWARD_* */
	int	allow_streamlocal_forwarding; /* One of FORWARD_* */
	int	allow_agent_forwarding;
	int	disable_forwarding;
	u_int num_allow_users;
	char   *allow_users[MAX_ALLOW_USERS];
	u_int num_deny_users;
	char   *deny_users[MAX_DENY_USERS];
	u_int num_allow_groups;
	char   *allow_groups[MAX_ALLOW_GROUPS];
	u_int num_deny_groups;
	char   *deny_groups[MAX_DENY_GROUPS];

	u_int num_subsystems;
	char   *subsystem_name[MAX_SUBSYSTEMS];
	char   *subsystem_command[MAX_SUBSYSTEMS];
	char   *subsystem_args[MAX_SUBSYSTEMS];

	u_int num_accept_env;
	char   *accept_env[MAX_ACCEPT_ENV];

	int	max_startups_begin;
	int	max_startups_rate;
	int	max_startups;
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

	u_int num_authkeys_files;	/* Files containing public keys */
	char   *authorized_keys_files[MAX_AUTHKEYS_FILES];

	char   *adm_forced_command;

	int	use_pam;		/* Enable auth via PAM */

	int	permit_tun;

	int	num_permitted_opens;

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
	char   *auth_methods[MAX_AUTH_METHODS];

	int	fingerprint_hash;
}       ServerOptions;

/* Information about the incoming connection as used by Match */
struct connection_info {
	const char *user;
	const char *host;	/* possibly resolved hostname */
	const char *address; 	/* remote address */
	const char *laddress;	/* local address */
	int lport;		/* local port */
};


/*
 * These are string config options that must be copied between the
 * Match sub-config and the main config, and must be sent from the
 * privsep slave to the privsep master. We use a macro to ensure all
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
		M_CP_STROPT(hostbased_key_types); \
		M_CP_STROPT(pubkey_key_types); \
		M_CP_STRARRAYOPT(authorized_keys_files, num_authkeys_files); \
		M_CP_STRARRAYOPT(allow_users, num_allow_users); \
		M_CP_STRARRAYOPT(deny_users, num_deny_users); \
		M_CP_STRARRAYOPT(allow_groups, num_allow_groups); \
		M_CP_STRARRAYOPT(deny_groups, num_deny_groups); \
		M_CP_STRARRAYOPT(accept_env, num_accept_env); \
		M_CP_STRARRAYOPT(auth_methods, num_auth_methods); \
	} while (0)

struct connection_info *get_connection_info(int, int);
void	 initialize_server_options(ServerOptions *);
void	 fill_default_server_options(ServerOptions *);
int	 process_server_config_line(ServerOptions *, char *, const char *, int,
	     int *, struct connection_info *);
void	 load_server_config(const char *, Buffer *);
void	 parse_server_config(ServerOptions *, const char *, Buffer *,
	     struct connection_info *);
void	 parse_server_match_config(ServerOptions *, struct connection_info *);
int	 parse_server_match_testspec(struct connection_info *, char *);
int	 server_match_spec_complete(struct connection_info *);
void	 copy_set_server_options(ServerOptions *, ServerOptions *, int);
void	 dump_config(ServerOptions *);
char	*derelativise_path(const char *);

#endif				/* SERVCONF_H */
