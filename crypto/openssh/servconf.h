/*
 * 
 * servconf.h
 * 
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * 
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * 
 * Created: Mon Aug 21 15:35:03 1995 ylo
 * 
 * Definitions for server configuration data and for the functions reading it.
 * 
 */

/* RCSID("$Id: servconf.h,v 1.15 2000/01/04 00:08:00 markus Exp $"); */

#ifndef SERVCONF_H
#define SERVCONF_H

#define MAX_PORTS		256	/* Max # ports. */

#define MAX_ALLOW_USERS		256	/* Max # users on allow list. */
#define MAX_DENY_USERS		256	/* Max # users on deny list. */
#define MAX_ALLOW_GROUPS	256	/* Max # groups on allow list. */
#define MAX_DENY_GROUPS		256	/* Max # groups on deny list. */

typedef struct {
	unsigned int num_ports;
	unsigned int ports_from_cmdline;
	u_short ports[MAX_PORTS];	/* Port number to listen on. */
	char   *listen_addr;		/* Address on which the server listens. */
	struct addrinfo *listen_addrs;	/* Addresses on which the server listens. */
	char   *host_key_file;	/* File containing host key. */
	int     server_key_bits;/* Size of the server key. */
	int     login_grace_time;	/* Disconnect if no auth in this time
					 * (sec). */
	int     key_regeneration_time;	/* Server key lifetime (seconds). */
	int     permit_root_login;	/* If true, permit root login. */
	int     ignore_rhosts;	/* Ignore .rhosts and .shosts. */
	int     ignore_user_known_hosts;	/* Ignore ~/.ssh/known_hosts
						 * for RhostsRsaAuth */
	int     print_motd;	/* If true, print /etc/motd. */
	int     check_mail;	/* If true, check for new mail. */
	int     x11_forwarding;	/* If true, permit inet (spoofing) X11 fwd. */
	int     x11_display_offset;	/* What DISPLAY number to start
					 * searching at */
	int     strict_modes;	/* If true, require string home dir modes. */
	int     keepalives;	/* If true, set SO_KEEPALIVE. */
	SyslogFacility log_facility;	/* Facility for system logging. */
	LogLevel log_level;	/* Level for system logging. */
	int     rhosts_authentication;	/* If true, permit rhosts
					 * authentication. */
	int     rhosts_rsa_authentication;	/* If true, permit rhosts RSA
						 * authentication. */
	int     rsa_authentication;	/* If true, permit RSA authentication. */
#ifdef KRB4
	int     kerberos_authentication;	/* If true, permit Kerberos
						 * authentication. */
	int     kerberos_or_local_passwd;	/* If true, permit kerberos
						 * and any other password
						 * authentication mechanism,
						 * such as SecurID or
						 * /etc/passwd */
	int     kerberos_ticket_cleanup;	/* If true, destroy ticket
						 * file on logout. */
#endif
#ifdef AFS
	int     kerberos_tgt_passing;	/* If true, permit Kerberos tgt
					 * passing. */
	int     afs_token_passing;	/* If true, permit AFS token passing. */
#endif
	int     password_authentication;	/* If true, permit password
						 * authentication. */
#ifdef SKEY
	int     skey_authentication;	/* If true, permit s/key
					 * authentication. */
#endif
	int     permit_empty_passwd;	/* If false, do not permit empty
					 * passwords. */
	int     use_login;	/* If true, login(1) is used */
	unsigned int num_allow_users;
	char   *allow_users[MAX_ALLOW_USERS];
	unsigned int num_deny_users;
	char   *deny_users[MAX_DENY_USERS];
	unsigned int num_allow_groups;
	char   *allow_groups[MAX_ALLOW_GROUPS];
	unsigned int num_deny_groups;
	char   *deny_groups[MAX_DENY_GROUPS];
}       ServerOptions;
/*
 * Initializes the server options to special values that indicate that they
 * have not yet been set.
 */
void    initialize_server_options(ServerOptions * options);

/*
 * Reads the server configuration file.  This only sets the values for those
 * options that have the special value indicating they have not been set.
 */
void    read_server_config(ServerOptions * options, const char *filename);

/* Sets values for those values that have not yet been set. */
void    fill_default_server_options(ServerOptions * options);

#endif				/* SERVCONF_H */
