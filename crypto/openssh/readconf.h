/* $OpenBSD: readconf.h,v 1.156 2024/03/04 02:16:11 djm Exp $ */

/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * Functions for reading the configuration file.
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 */

#ifndef READCONF_H
#define READCONF_H

/* Data structure for representing option data. */

#define SSH_MAX_HOSTS_FILES	32
#define MAX_CANON_DOMAINS	32
#define PATH_MAX_SUN		(sizeof((struct sockaddr_un *)0)->sun_path)

struct allowed_cname {
	char *source_list;
	char *target_list;
};

typedef struct {
	char   *host_arg;	/* Host arg as specified on command line. */
	int     forward_agent;	/* Forward authentication agent. */
	char   *forward_agent_sock_path; /* Optional path of the agent. */
	int     forward_x11;	/* Forward X11 display. */
	int     forward_x11_timeout;	/* Expiration for Cookies */
	int     forward_x11_trusted;	/* Trust Forward X11 display. */
	int     exit_on_forward_failure;	/* Exit if bind(2) fails for -L/-R */
	char   *xauth_location;	/* Location for xauth program */
	struct ForwardOptions fwd_opts;	/* forwarding options */
	int     pubkey_authentication;	/* Try ssh2 pubkey authentication. */
	int     hostbased_authentication;	/* ssh2's rhosts_rsa */
	int     gss_authentication;	/* Try GSS authentication */
	int     gss_deleg_creds;	/* Delegate GSS credentials */
	int     password_authentication;	/* Try password
						 * authentication. */
	int     kbd_interactive_authentication; /* Try keyboard-interactive auth. */
	char	*kbd_interactive_devices; /* Keyboard-interactive auth devices. */
	int     batch_mode;	/* Batch mode: do not ask for passwords. */
	int     check_host_ip;	/* Also keep track of keys for IP address */
	int     strict_host_key_checking;	/* Strict host key checking. */
	int     compression;	/* Compress packets in both directions. */
	int     tcp_keep_alive;	/* Set SO_KEEPALIVE. */
	int	ip_qos_interactive;	/* IP ToS/DSCP/class for interactive */
	int	ip_qos_bulk;		/* IP ToS/DSCP/class for bulk traffic */
	SyslogFacility log_facility;	/* Facility for system logging. */
	LogLevel log_level;	/* Level for logging. */
	u_int	num_log_verbose;	/* Verbose log overrides */
	char   **log_verbose;
	int     port;		/* Port to connect. */
	int     address_family;
	int     connection_attempts;	/* Max attempts (seconds) before
					 * giving up */
	int     connection_timeout;	/* Max time (seconds) before
					 * aborting connection attempt */
	int     number_of_password_prompts;	/* Max number of password
						 * prompts. */
	char   *ciphers;	/* SSH2 ciphers in order of preference. */
	char   *macs;		/* SSH2 macs in order of preference. */
	char   *hostkeyalgorithms;	/* SSH2 server key types in order of preference. */
	char   *kex_algorithms;	/* SSH2 kex methods in order of preference. */
	char   *ca_sign_algorithms;	/* Allowed CA signature algorithms */
	char   *hostname;	/* Real host to connect. */
	char   *tag;		/* Configuration tag name. */
	char   *host_key_alias;	/* hostname alias for .ssh/known_hosts */
	char   *proxy_command;	/* Proxy command for connecting the host. */
	char   *user;		/* User to log in as. */
	int     escape_char;	/* Escape character; -2 = none */

	u_int	num_system_hostfiles;	/* Paths for /etc/ssh/ssh_known_hosts */
	char   *system_hostfiles[SSH_MAX_HOSTS_FILES];
	u_int	num_user_hostfiles;	/* Path for $HOME/.ssh/known_hosts */
	char   *user_hostfiles[SSH_MAX_HOSTS_FILES];
	char   *preferred_authentications;
	char   *bind_address;	/* local socket address for connection to sshd */
	char   *bind_interface;	/* local interface for bind address */
	char   *pkcs11_provider; /* PKCS#11 provider */
	char   *sk_provider; /* Security key provider */
	int	verify_host_key_dns;	/* Verify host key using DNS */

	int     num_identity_files;	/* Number of files for identities. */
	char   *identity_files[SSH_MAX_IDENTITY_FILES];
	int    identity_file_userprovided[SSH_MAX_IDENTITY_FILES];
	struct sshkey *identity_keys[SSH_MAX_IDENTITY_FILES];

	int	num_certificate_files; /* Number of extra certificates for ssh. */
	char	*certificate_files[SSH_MAX_CERTIFICATE_FILES];
	int	certificate_file_userprovided[SSH_MAX_CERTIFICATE_FILES];
	struct sshkey *certificates[SSH_MAX_CERTIFICATE_FILES];

	int	add_keys_to_agent;
	int	add_keys_to_agent_lifespan;
	char   *identity_agent;		/* Optional path to ssh-agent socket */

	/* Local TCP/IP forward requests. */
	int     num_local_forwards;
	struct Forward *local_forwards;

	/* Remote TCP/IP forward requests. */
	int     num_remote_forwards;
	struct Forward *remote_forwards;
	int	clear_forwardings;

	/* Restrict remote dynamic forwarding */
	char  **permitted_remote_opens;
	u_int	num_permitted_remote_opens;

	/* stdio forwarding (-W) host and port */
	char   *stdio_forward_host;
	int	stdio_forward_port;

	int	enable_ssh_keysign;
	int64_t rekey_limit;
	int	rekey_interval;
	int	no_host_authentication_for_localhost;
	int	identities_only;
	int	server_alive_interval;
	int	server_alive_count_max;

	u_int	num_send_env;
	char	**send_env;
	u_int	num_setenv;
	char	**setenv;

	char	*control_path;
	int	control_master;
	int     control_persist; /* ControlPersist flag */
	int     control_persist_timeout; /* ControlPersist timeout (seconds) */

	int	hash_known_hosts;

	int	tun_open;	/* tun(4) */
	int     tun_local;	/* force tun device (optional) */
	int     tun_remote;	/* force tun device (optional) */

	char	*local_command;
	int	permit_local_command;
	char	*remote_command;
	int	visual_host_key;

	int	request_tty;
	int	session_type;
	int	stdin_null;
	int	fork_after_authentication;

	int	proxy_use_fdpass;

	int	num_canonical_domains;
	char	**canonical_domains;
	int	canonicalize_hostname;
	int	canonicalize_max_dots;
	int	canonicalize_fallback_local;
	int	num_permitted_cnames;
	struct allowed_cname *permitted_cnames;

	char	*revoked_host_keys;

	int	 fingerprint_hash;

	int	 update_hostkeys; /* one of SSH_UPDATE_HOSTKEYS_* */

	char   *hostbased_accepted_algos;
	char   *pubkey_accepted_algos;

	char   *jump_user;
	char   *jump_host;
	int	jump_port;
	char   *jump_extra;

	char   *known_hosts_command;

	int	required_rsa_size;	/* minimum size of RSA keys */
	int	enable_escape_commandline;	/* ~C commandline */
	int	obscure_keystroke_timing_interval;

	char	**channel_timeouts;	/* inactivity timeout by channel type */
	u_int	num_channel_timeouts;

	char	*ignored_unknown; /* Pattern list of unknown tokens to ignore */
}       Options;

#define SSH_PUBKEY_AUTH_NO	0x00
#define SSH_PUBKEY_AUTH_UNBOUND	0x01
#define SSH_PUBKEY_AUTH_HBOUND	0x02
#define SSH_PUBKEY_AUTH_ALL	0x03

#define SSH_CANONICALISE_NO	0
#define SSH_CANONICALISE_YES	1
#define SSH_CANONICALISE_ALWAYS	2

#define SSHCTL_MASTER_NO	0
#define SSHCTL_MASTER_YES	1
#define SSHCTL_MASTER_AUTO	2
#define SSHCTL_MASTER_ASK	3
#define SSHCTL_MASTER_AUTO_ASK	4

#define REQUEST_TTY_AUTO	0
#define REQUEST_TTY_NO		1
#define REQUEST_TTY_YES		2
#define REQUEST_TTY_FORCE	3

#define SESSION_TYPE_NONE	0
#define SESSION_TYPE_SUBSYSTEM	1
#define SESSION_TYPE_DEFAULT	2

#define SSHCONF_CHECKPERM	1  /* check permissions on config file */
#define SSHCONF_USERCONF	2  /* user provided config file not system */
#define SSHCONF_FINAL		4  /* Final pass over config, after canon. */
#define SSHCONF_NEVERMATCH	8  /* Match/Host never matches; internal only */

#define SSH_UPDATE_HOSTKEYS_NO	0
#define SSH_UPDATE_HOSTKEYS_YES	1
#define SSH_UPDATE_HOSTKEYS_ASK	2

#define SSH_STRICT_HOSTKEY_OFF	0
#define SSH_STRICT_HOSTKEY_NEW	1
#define SSH_STRICT_HOSTKEY_YES	2
#define SSH_STRICT_HOSTKEY_ASK	3

/* ObscureKeystrokes parameters */
#define SSH_KEYSTROKE_DEFAULT_INTERVAL_MS	20
#define SSH_KEYSTROKE_CHAFF_MIN_MS		1024
#define SSH_KEYSTROKE_CHAFF_RNG_MS		2048

const char *kex_default_pk_alg(void);
char	*ssh_connection_hash(const char *thishost, const char *host,
    const char *portstr, const char *user, const char *jump_host);
void     initialize_options(Options *);
int      fill_default_options(Options *);
void	 fill_default_options_for_canonicalization(Options *);
void	 free_options(Options *o);
int	 process_config_line(Options *, struct passwd *, const char *,
    const char *, char *, const char *, int, int *, int);
int	 read_config_file(const char *, struct passwd *, const char *,
    const char *, Options *, int, int *);
int	 parse_forward(struct Forward *, const char *, int, int);
int	 parse_jump(const char *, Options *, int);
int	 parse_ssh_uri(const char *, char **, char **, int *);
int	 default_ssh_port(void);
int	 option_clear_or_none(const char *);
int	 config_has_permitted_cnames(Options *);
void	 dump_client_config(Options *o, const char *host);

void	 add_local_forward(Options *, const struct Forward *);
void	 add_remote_forward(Options *, const struct Forward *);
void	 add_identity_file(Options *, const char *, const char *, int);
void	 add_certificate_file(Options *, const char *, int);

#endif				/* READCONF_H */
