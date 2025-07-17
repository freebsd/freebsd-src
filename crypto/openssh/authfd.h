/* $OpenBSD: authfd.h,v 1.52 2023/12/18 14:46:56 djm Exp $ */

/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * Functions to interface with the SSH_AUTHENTICATION_FD socket.
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 */

#ifndef AUTHFD_H
#define AUTHFD_H

struct sshbuf;
struct sshkey;

/* List of identities returned by ssh_fetch_identitylist() */
struct ssh_identitylist {
	size_t nkeys;
	struct sshkey **keys;
	char **comments;
};

/* Key destination restrictions */
struct dest_constraint_hop {
	char *user;	/* wildcards allowed */
	char *hostname; /* used to matching cert principals and for display */
	int is_ca;
	u_int nkeys;	/* number of entries in *both* 'keys' and 'key_is_ca' */
	struct sshkey **keys;
	int *key_is_ca;
};
struct dest_constraint {
	struct dest_constraint_hop from;
	struct dest_constraint_hop to;
};

int	ssh_get_authentication_socket(int *fdp);
int	ssh_get_authentication_socket_path(const char *authsocket, int *fdp);
void	ssh_close_authentication_socket(int sock);

int	ssh_lock_agent(int sock, int lock, const char *password);
int	ssh_fetch_identitylist(int sock, struct ssh_identitylist **idlp);
void	ssh_free_identitylist(struct ssh_identitylist *idl);
int	ssh_add_identity_constrained(int sock, struct sshkey *key,
    const char *comment, u_int life, u_int confirm, u_int maxsign,
    const char *provider, struct dest_constraint **dest_constraints,
    size_t ndest_constraints);
int	ssh_agent_has_key(int sock, const struct sshkey *key);
int	ssh_remove_identity(int sock, const struct sshkey *key);
int	ssh_update_card(int sock, int add, const char *reader_id,
	    const char *pin, u_int life, u_int confirm,
	    struct dest_constraint **dest_constraints,
	    size_t ndest_constraints,
	    int cert_only, struct sshkey **certs, size_t ncerts);
int	ssh_remove_all_identities(int sock, int version);

int	ssh_agent_sign(int sock, const struct sshkey *key,
	    u_char **sigp, size_t *lenp,
	    const u_char *data, size_t datalen, const char *alg, u_int compat);

int	ssh_agent_bind_hostkey(int sock, const struct sshkey *key,
    const struct sshbuf *session_id, const struct sshbuf *signature,
    int forwarding);

/* Messages for the authentication agent connection. */
#define SSH_AGENTC_REQUEST_RSA_IDENTITIES	1
#define SSH_AGENT_RSA_IDENTITIES_ANSWER		2
#define SSH_AGENTC_RSA_CHALLENGE		3
#define SSH_AGENT_RSA_RESPONSE			4
#define SSH_AGENT_FAILURE			5
#define SSH_AGENT_SUCCESS			6
#define SSH_AGENTC_ADD_RSA_IDENTITY		7
#define SSH_AGENTC_REMOVE_RSA_IDENTITY		8
#define SSH_AGENTC_REMOVE_ALL_RSA_IDENTITIES	9

/* private OpenSSH extensions for SSH2 */
#define SSH2_AGENTC_REQUEST_IDENTITIES		11
#define SSH2_AGENT_IDENTITIES_ANSWER		12
#define SSH2_AGENTC_SIGN_REQUEST		13
#define SSH2_AGENT_SIGN_RESPONSE		14
#define SSH2_AGENTC_ADD_IDENTITY		17
#define SSH2_AGENTC_REMOVE_IDENTITY		18
#define SSH2_AGENTC_REMOVE_ALL_IDENTITIES	19

/* smartcard */
#define SSH_AGENTC_ADD_SMARTCARD_KEY		20
#define SSH_AGENTC_REMOVE_SMARTCARD_KEY		21

/* lock/unlock the agent */
#define SSH_AGENTC_LOCK				22
#define SSH_AGENTC_UNLOCK			23

/* add key with constraints */
#define SSH_AGENTC_ADD_RSA_ID_CONSTRAINED	24
#define SSH2_AGENTC_ADD_ID_CONSTRAINED		25
#define SSH_AGENTC_ADD_SMARTCARD_KEY_CONSTRAINED 26

/* generic extension mechanism */
#define SSH_AGENTC_EXTENSION			27

#define	SSH_AGENT_CONSTRAIN_LIFETIME		1
#define	SSH_AGENT_CONSTRAIN_CONFIRM		2
#define	SSH_AGENT_CONSTRAIN_MAXSIGN		3
#define	SSH_AGENT_CONSTRAIN_EXTENSION		255

/* extended failure messages */
#define SSH2_AGENT_FAILURE			30

/* additional error code for ssh.com's ssh-agent2 */
#define SSH_COM_AGENT2_FAILURE			102

#define	SSH_AGENT_OLD_SIGNATURE			0x01
#define	SSH_AGENT_RSA_SHA2_256			0x02
#define	SSH_AGENT_RSA_SHA2_512			0x04

#endif				/* AUTHFD_H */
