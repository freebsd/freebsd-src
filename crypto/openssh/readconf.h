/* $OpenBSD: readconf.h,v 1.110 2015/07/10 06:21:53 markus Exp $ */
/* $FreeBSD$ */

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

#define MAX_SEND_ENV		256
#define SSH_MAX_HOSTS_FILES	32
#define MAX_CANON_DOMAINS	32
#define PATH_MAX_SUN		(sizeof((struct sockaddr_un *)0)->sun_path)

struct allowed_cname {
	char *source_list;
	char *target_list;
};

typedef struct {
	int     forward_agent;	/* Forward authentication agent. */
	int     forward_x11;	/* Forward X11 display. */
	int     forward_x11_timeout;	/* Expiration for Cookies */
	int     forward_x11_trusted;	/* Trust Forward X11 display. */
	int     exit_on_forward_failure;	/* Exit if bind(2) fails for -L/-R */
	char   *xauth_location;	/* Location for xauth program */
	struct ForwardOptions fwd_opts;	/* forwarding options */
	int     use_privileged_port;	/* Don't use privileged port if false. */
	int     rhosts_rsa_authentication;	/* Try rhosts with RSA
						 * authentication. */
	int     rsa_authentication;	/* Try RSA authentication. */
	int     pubkey_authentication;	/* Try ssh2 pubkey authentication. */
	int     hostbased_authentication;	/* ssh2's rhosts_rsa */
	int     challenge_response_authentication;
					/* Try S/Key or TIS, authentication. */
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
	int     compression_level;	/* Compression level 1 (fast) to 9
					 * (best). */
	int     tcp_keep_alive;	/* Set SO_KEEPALIVE. */
	int	ip_qos_interactive;	/* IP ToS/DSCP/class for interactive */
	int	ip_qos_bulk;		/* IP ToS/DSCP/class for bulk traffic */
	LogLevel log_level;	/* Level for logging. */

	int     port;		/* Port to connect. */
	int     address_family;
	int     connection_attempts;	/* Max attempts (seconds) before
					 * giving up */
	int     connection_timeout;	/* Max time (seconds) before
					 * aborting connection attempt */
	int     number_of_password_prompts;	/* Max number of password
						 * prompts. */
	int     cipher;		/* Cipher to use. */
	char   *ciphers;	/* SSH2 ciphers in order of preference. */
	char   *macs;		/* SSH2 macs in order of preference. */
	char   *hostkeyalgorithms;	/* SSH2 server key types in order of preference. */
	char   *kex_algorithms;	/* SSH2 kex methods in order of preference. */
	int	protocol;	/* Protocol in order of preference. */
	char   *hostname;	/* Real host to connect. */
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
	char   *pkcs11_provider; /* PKCS#11 provider */
	int	verify_host_key_dns;	/* Verify host key using DNS */

	int     num_identity_files;	/* Number of files for RSA/DSA identities. */
	char   *identity_files[SSH_MAX_IDENTITY_FILES];
	int    identity_file_userprovided[SSH_MAX_IDENTITY_FILES];
	struct sshkey *identity_keys[SSH_MAX_IDENTITY_FILES];

	/* Local TCP/IP forward requests. */
	int     num_local_forwards;
	struct Forward *local_forwards;

	/* Remote TCP/IP forward requests. */
	int     num_remote_forwards;
	struct Forward *remote_forwards;
	int	clear_forwardings;

	int	enable_ssh_keysign;
	int64_t rekey_limit;
	int	rekey_interval;
	int	no_host_authentication_for_localhost;
	int	identities_only;
	int	server_alive_interval;
	int	server_alive_count_max;

	int     num_send_env;
	char   *send_env[MAX_SEND_ENV];

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
	int	visual_host_key;

	int	use_roaming;

	int	request_tty;

	int	proxy_use_fdpass;

	int	num_canonical_domains;
	char	*canonical_domains[MAX_CANON_DOMAINS];
	int	canonicalize_hostname;
	int	canonicalize_max_dots;
	int	canonicalize_fallback_local;
	int	num_permitted_cnames;
	struct allowed_cname permitted_cnames[MAX_CANON_DOMAINS];

	char	*revoked_host_keys;

	int	 fingerprint_hash;

	int	 update_hostkeys; /* one of SSH_UPDATE_HOSTKEYS_* */

	char   *hostbased_key_types;
	char   *pubkey_key_types;

	char	*version_addendum; /* Appended to SSH banner */

	char	*ignored_unknown; /* Pattern list of unknown tokens to ignore */
}       Options;

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

#define SSHCONF_CHECKPERM	1  /* check permissions on config file */
#define SSHCONF_USERCONF	2  /* user provided config file not system */
#define SSHCONF_POSTCANON	4  /* After hostname canonicalisation */

#define SSH_UPDATE_HOSTKEYS_NO	0
#define SSH_UPDATE_HOSTKEYS_YES	1
#define SSH_UPDATE_HOSTKEYS_ASK	2

void     initialize_options(Options *);
void     fill_default_options(Options *);
void	 fill_default_options_for_canonicalization(Options *);
int	 process_config_line(Options *, struct passwd *, const char *,
    const char *, char *, const char *, int, int *, int);
int	 read_config_file(const char *, struct passwd *, const char *,
    const char *, Options *, int);
int	 parse_forward(struct Forward *, const char *, int, int);
int	 default_ssh_port(void);
int	 option_clear_or_none(const char *);
void	 dump_client_config(Options *o, const char *host);

void	 add_local_forward(Options *, const struct Forward *);
void	 add_remote_forward(Options *, const struct Forward *);
void	 add_identity_file(Options *, const char *, const char *, int);

#endif				/* READCONF_H */
