/*	$OpenBSD: readconf.h,v 1.67 2005/06/08 11:25:09 djm Exp $	*/

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

#include "key.h"

/* Data structure for representing a forwarding request. */

typedef struct {
	char	 *listen_host;		/* Host (address) to listen on. */
	u_short	  listen_port;		/* Port to forward. */
	char	 *connect_host;		/* Host to connect. */
	u_short	  connect_port;		/* Port to connect on connect_host. */
}       Forward;
/* Data structure for representing option data. */

#define MAX_SEND_ENV	256

typedef struct {
	int     forward_agent;	/* Forward authentication agent. */
	int     forward_x11;	/* Forward X11 display. */
	int     forward_x11_trusted;	/* Trust Forward X11 display. */
	char   *xauth_location;	/* Location for xauth program */
	int     gateway_ports;	/* Allow remote connects to forwarded ports. */
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
	int	protocol;	/* Protocol in order of preference. */
	char   *hostname;	/* Real host to connect. */
	char   *host_key_alias;	/* hostname alias for .ssh/known_hosts */
	char   *proxy_command;	/* Proxy command for connecting the host. */
	char   *user;		/* User to log in as. */
	int     escape_char;	/* Escape character; -2 = none */

	char   *system_hostfile;/* Path for /etc/ssh/ssh_known_hosts. */
	char   *user_hostfile;	/* Path for $HOME/.ssh/known_hosts. */
	char   *system_hostfile2;
	char   *user_hostfile2;
	char   *preferred_authentications;
	char   *bind_address;	/* local socket address for connection to sshd */
	char   *smartcard_device; /* Smartcard reader device */
	int	verify_host_key_dns;	/* Verify host key using DNS */

	int     num_identity_files;	/* Number of files for RSA/DSA identities. */
	char   *identity_files[SSH_MAX_IDENTITY_FILES];
	Key    *identity_keys[SSH_MAX_IDENTITY_FILES];

	/* Local TCP/IP forward requests. */
	int     num_local_forwards;
	Forward local_forwards[SSH_MAX_FORWARDS_PER_DIRECTION];

	/* Remote TCP/IP forward requests. */
	int     num_remote_forwards;
	Forward remote_forwards[SSH_MAX_FORWARDS_PER_DIRECTION];
	int	clear_forwardings;

	int	enable_ssh_keysign;
	int	rekey_limit;
	int	no_host_authentication_for_localhost;
	int	identities_only;
	int	server_alive_interval;
	int	server_alive_count_max;

	int     num_send_env;
	char   *send_env[MAX_SEND_ENV];

	char	*control_path;
	int	control_master;

	int	hash_known_hosts;
}       Options;

#define SSHCTL_MASTER_NO	0
#define SSHCTL_MASTER_YES	1
#define SSHCTL_MASTER_AUTO	2
#define SSHCTL_MASTER_ASK	3
#define SSHCTL_MASTER_AUTO_ASK	4

void     initialize_options(Options *);
void     fill_default_options(Options *);
int	 read_config_file(const char *, const char *, Options *, int);
int	 parse_forward(Forward *, const char *);

int
process_config_line(Options *, const char *, char *, const char *, int, int *);

void	 add_local_forward(Options *, const Forward *);
void	 add_remote_forward(Options *, const Forward *);

#endif				/* READCONF_H */
