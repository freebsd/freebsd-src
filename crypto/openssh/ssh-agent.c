/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * The authentication agent program.
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 *
 * SSH2 implementation,
 * Copyright (c) 2000 Markus Friedl. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "includes.h"
RCSID("$OpenBSD: ssh-agent.c,v 1.35 2000/09/07 20:27:54 deraadt Exp $");
RCSID("$FreeBSD$");

#include "ssh.h"
#include "rsa.h"
#include "buffer.h"
#include "bufaux.h"
#include "xmalloc.h"
#include "packet.h"
#include "getput.h"
#include "mpaux.h"

#include <openssl/evp.h>
#include <openssl/md5.h>
#include <openssl/dsa.h>
#include <openssl/rsa.h>
#include "key.h"
#include "authfd.h"
#include "dsa.h"
#include "kex.h"

typedef struct {
	int fd;
	enum {
		AUTH_UNUSED, AUTH_SOCKET, AUTH_CONNECTION
	} type;
	Buffer input;
	Buffer output;
} SocketEntry;

unsigned int sockets_alloc = 0;
SocketEntry *sockets = NULL;

typedef struct {
	Key *key;
	char *comment;
} Identity;

typedef struct {
	int nentries;
	Identity *identities;
} Idtab;

/* private key table, one per protocol version */
Idtab idtable[3];

int max_fd = 0;

/* pid of shell == parent of agent */
pid_t parent_pid = -1;

/* pathname and directory for AUTH_SOCKET */
char socket_name[1024];
char socket_dir[1024];

extern char *__progname;

void
idtab_init(void)
{
	int i;
	for (i = 0; i <=2; i++){
		idtable[i].identities = NULL;
		idtable[i].nentries = 0;
	}
}

/* return private key table for requested protocol version */
Idtab *
idtab_lookup(int version)
{
	if (version < 1 || version > 2)
		fatal("internal error, bad protocol version %d", version);
	return &idtable[version];
}

/* return matching private key for given public key */
Key *
lookup_private_key(Key *key, int *idx, int version)
{
	int i;
	Idtab *tab = idtab_lookup(version);
	for (i = 0; i < tab->nentries; i++) {
		if (key_equal(key, tab->identities[i].key)) {
			if (idx != NULL)
				*idx = i;
			return tab->identities[i].key;
		}
	}
	return NULL;
}

/* send list of supported public keys to 'client' */
void
process_request_identities(SocketEntry *e, int version)
{
	Idtab *tab = idtab_lookup(version);
	Buffer msg;
	int i;

	buffer_init(&msg);
	buffer_put_char(&msg, (version == 1) ?
	    SSH_AGENT_RSA_IDENTITIES_ANSWER : SSH2_AGENT_IDENTITIES_ANSWER);
	buffer_put_int(&msg, tab->nentries);
	for (i = 0; i < tab->nentries; i++) {
		Identity *id = &tab->identities[i];
		if (id->key->type == KEY_RSA) {
			buffer_put_int(&msg, BN_num_bits(id->key->rsa->n));
			buffer_put_bignum(&msg, id->key->rsa->e);
			buffer_put_bignum(&msg, id->key->rsa->n);
		} else {
			unsigned char *blob;
			unsigned int blen;
			dsa_make_key_blob(id->key, &blob, &blen);
			buffer_put_string(&msg, blob, blen);
			xfree(blob);
		}
		buffer_put_cstring(&msg, id->comment);
	}
	buffer_put_int(&e->output, buffer_len(&msg));
	buffer_append(&e->output, buffer_ptr(&msg), buffer_len(&msg));
	buffer_free(&msg);
}

/* ssh1 only */
void
process_authentication_challenge1(SocketEntry *e)
{
	Key *key, *private;
	BIGNUM *challenge;
	int i, len;
	Buffer msg;
	MD5_CTX md;
	unsigned char buf[32], mdbuf[16], session_id[16];
	unsigned int response_type;

	buffer_init(&msg);
	key = key_new(KEY_RSA);
	challenge = BN_new();

	buffer_get_int(&e->input);				/* ignored */
	buffer_get_bignum(&e->input, key->rsa->e);
	buffer_get_bignum(&e->input, key->rsa->n);
	buffer_get_bignum(&e->input, challenge);

	/* Only protocol 1.1 is supported */
	if (buffer_len(&e->input) == 0)
		goto failure;
	buffer_get(&e->input, (char *) session_id, 16);
	response_type = buffer_get_int(&e->input);
	if (response_type != 1)
		goto failure;

	private = lookup_private_key(key, NULL, 1);
	if (private != NULL) {
		/* Decrypt the challenge using the private key. */
		rsa_private_decrypt(challenge, challenge, private->rsa);

		/* The response is MD5 of decrypted challenge plus session id. */
		len = BN_num_bytes(challenge);
		if (len <= 0 || len > 32) {
			log("process_authentication_challenge: bad challenge length %d", len);
			goto failure;
		}
		memset(buf, 0, 32);
		BN_bn2bin(challenge, buf + 32 - len);
		MD5_Init(&md);
		MD5_Update(&md, buf, 32);
		MD5_Update(&md, session_id, 16);
		MD5_Final(mdbuf, &md);

		/* Send the response. */
		buffer_put_char(&msg, SSH_AGENT_RSA_RESPONSE);
		for (i = 0; i < 16; i++)
			buffer_put_char(&msg, mdbuf[i]);
		goto send;
	}

failure:
	/* Unknown identity or protocol error.  Send failure. */
	buffer_put_char(&msg, SSH_AGENT_FAILURE);
send:
	buffer_put_int(&e->output, buffer_len(&msg));
	buffer_append(&e->output, buffer_ptr(&msg), buffer_len(&msg));
	key_free(key);
	BN_clear_free(challenge);
	buffer_free(&msg);
}

/* ssh2 only */
void
process_sign_request2(SocketEntry *e)
{
	extern int datafellows;
	Key *key, *private;
	unsigned char *blob, *data, *signature = NULL;
	unsigned int blen, dlen, slen = 0;
	Buffer msg;
	int ok = -1;

	datafellows = 0;
	
	blob = buffer_get_string(&e->input, &blen);
	data = buffer_get_string(&e->input, &dlen);
	buffer_get_int(&e->input);			/* flags, unused */

	key = dsa_key_from_blob(blob, blen);
	if (key != NULL) {
		private = lookup_private_key(key, NULL, 2);
		if (private != NULL)
			ok = dsa_sign(private, &signature, &slen, data, dlen);
	}
	key_free(key);
	buffer_init(&msg);
	if (ok == 0) {
		buffer_put_char(&msg, SSH2_AGENT_SIGN_RESPONSE);
		buffer_put_string(&msg, signature, slen);
	} else {
		buffer_put_char(&msg, SSH_AGENT_FAILURE);
	}
	buffer_put_int(&e->output, buffer_len(&msg));
	buffer_append(&e->output, buffer_ptr(&msg),
	    buffer_len(&msg));
	buffer_free(&msg);
	xfree(data);
	xfree(blob);
	if (signature != NULL)
		xfree(signature);
}

/* shared */
void
process_remove_identity(SocketEntry *e, int version)
{
	Key *key = NULL, *private;
	unsigned char *blob;
	unsigned int blen;
	unsigned int bits;
	int success = 0;

	switch(version){
	case 1:
		key = key_new(KEY_RSA);
		bits = buffer_get_int(&e->input);
		buffer_get_bignum(&e->input, key->rsa->e);
		buffer_get_bignum(&e->input, key->rsa->n);

		if (bits != key_size(key))
			log("Warning: identity keysize mismatch: actual %d, announced %d",
			      key_size(key), bits);
		break;
	case 2:
		blob = buffer_get_string(&e->input, &blen);
		key = dsa_key_from_blob(blob, blen);
		xfree(blob);
		break;
	}
	if (key != NULL) {
		int idx;
		private = lookup_private_key(key, &idx, version);
		if (private != NULL) {
			/*
			 * We have this key.  Free the old key.  Since we
			 * don\'t want to leave empty slots in the middle of
			 * the array, we actually free the key there and copy
			 * data from the last entry.
			 */
			Idtab *tab = idtab_lookup(version);
			key_free(tab->identities[idx].key);
			xfree(tab->identities[idx].comment);
			if (idx != tab->nentries)
				tab->identities[idx] = tab->identities[tab->nentries];
			tab->nentries--;
			success = 1;
		}
		key_free(key);
	}
	buffer_put_int(&e->output, 1);
	buffer_put_char(&e->output,
	    success ? SSH_AGENT_SUCCESS : SSH_AGENT_FAILURE);
}

void
process_remove_all_identities(SocketEntry *e, int version)
{
	unsigned int i;
	Idtab *tab = idtab_lookup(version);

	/* Loop over all identities and clear the keys. */
	for (i = 0; i < tab->nentries; i++) {
		key_free(tab->identities[i].key);
		xfree(tab->identities[i].comment);
	}

	/* Mark that there are no identities. */
	tab->nentries = 0;

	/* Send success. */
	buffer_put_int(&e->output, 1);
	buffer_put_char(&e->output, SSH_AGENT_SUCCESS);
	return;
}

void
process_add_identity(SocketEntry *e, int version)
{
	Key *k = NULL;
	RSA *rsa;
	BIGNUM *aux;
	BN_CTX *ctx;
	char *type;
	char *comment;
	int success = 0;
	Idtab *tab = idtab_lookup(version);

	switch (version) {
	case 1:
		k = key_new(KEY_RSA);
		rsa = k->rsa;

		/* allocate mem for private key */
		/* XXX rsa->n and rsa->e are already allocated */
		rsa->d = BN_new();
		rsa->iqmp = BN_new();
		rsa->q = BN_new();
		rsa->p = BN_new();
		rsa->dmq1 = BN_new();
		rsa->dmp1 = BN_new();

		buffer_get_int(&e->input);		 /* ignored */

		buffer_get_bignum(&e->input, rsa->n);
		buffer_get_bignum(&e->input, rsa->e);
		buffer_get_bignum(&e->input, rsa->d);
		buffer_get_bignum(&e->input, rsa->iqmp);

		/* SSH and SSL have p and q swapped */
		buffer_get_bignum(&e->input, rsa->q);	/* p */
		buffer_get_bignum(&e->input, rsa->p);	/* q */

		/* Generate additional parameters */
		aux = BN_new();
		ctx = BN_CTX_new();

		BN_sub(aux, rsa->q, BN_value_one());
		BN_mod(rsa->dmq1, rsa->d, aux, ctx);

		BN_sub(aux, rsa->p, BN_value_one());
		BN_mod(rsa->dmp1, rsa->d, aux, ctx);

		BN_clear_free(aux);
		BN_CTX_free(ctx);

		break;
	case 2:
		type = buffer_get_string(&e->input, NULL);
		if (strcmp(type, KEX_DSS)) {
			buffer_clear(&e->input);
			xfree(type);
			goto send;
		}
		xfree(type);

		k = key_new(KEY_DSA);

		/* allocate mem for private key */
		k->dsa->priv_key = BN_new();

		buffer_get_bignum2(&e->input, k->dsa->p);
		buffer_get_bignum2(&e->input, k->dsa->q);
		buffer_get_bignum2(&e->input, k->dsa->g);
		buffer_get_bignum2(&e->input, k->dsa->pub_key);
		buffer_get_bignum2(&e->input, k->dsa->priv_key);

		break;
	}

	comment = buffer_get_string(&e->input, NULL);
	if (k == NULL) {
		xfree(comment);
		goto send;
	}
	success = 1;
	if (lookup_private_key(k, NULL, version) == NULL) {
		if (tab->nentries == 0)
			tab->identities = xmalloc(sizeof(Identity));
		else
			tab->identities = xrealloc(tab->identities,
			    (tab->nentries + 1) * sizeof(Identity));
		tab->identities[tab->nentries].key = k;
		tab->identities[tab->nentries].comment = comment;
		/* Increment the number of identities. */
		tab->nentries++;
	} else {
		key_free(k);
		xfree(comment);
	}
send:
	buffer_put_int(&e->output, 1);
	buffer_put_char(&e->output,
	    success ? SSH_AGENT_SUCCESS : SSH_AGENT_FAILURE);
}

/* dispatch incoming messages */

void
process_message(SocketEntry *e)
{
	unsigned int msg_len;
	unsigned int type;
	unsigned char *cp;
	if (buffer_len(&e->input) < 5)
		return;		/* Incomplete message. */
	cp = (unsigned char *) buffer_ptr(&e->input);
	msg_len = GET_32BIT(cp);
	if (msg_len > 256 * 1024) {
		shutdown(e->fd, SHUT_RDWR);
		close(e->fd);
		e->type = AUTH_UNUSED;
		return;
	}
	if (buffer_len(&e->input) < msg_len + 4)
		return;
	buffer_consume(&e->input, 4);
	type = buffer_get_char(&e->input);

	switch (type) {
	/* ssh1 */
	case SSH_AGENTC_RSA_CHALLENGE:
		process_authentication_challenge1(e);
		break;
	case SSH_AGENTC_REQUEST_RSA_IDENTITIES:
		process_request_identities(e, 1);
		break;
	case SSH_AGENTC_ADD_RSA_IDENTITY:
		process_add_identity(e, 1);
		break;
	case SSH_AGENTC_REMOVE_RSA_IDENTITY:
		process_remove_identity(e, 1);
		break;
	case SSH_AGENTC_REMOVE_ALL_RSA_IDENTITIES:
		process_remove_all_identities(e, 1);
		break;
	/* ssh2 */
	case SSH2_AGENTC_SIGN_REQUEST:
		process_sign_request2(e);
		break;
	case SSH2_AGENTC_REQUEST_IDENTITIES:
		process_request_identities(e, 2);
		break;
	case SSH2_AGENTC_ADD_IDENTITY:
		process_add_identity(e, 2);
		break;
	case SSH2_AGENTC_REMOVE_IDENTITY:
		process_remove_identity(e, 2);
		break;
	case SSH2_AGENTC_REMOVE_ALL_IDENTITIES:
		process_remove_all_identities(e, 2);
		break;
	default:
		/* Unknown message.  Respond with failure. */
		error("Unknown message %d", type);
		buffer_clear(&e->input);
		buffer_put_int(&e->output, 1);
		buffer_put_char(&e->output, SSH_AGENT_FAILURE);
		break;
	}
}

void
new_socket(int type, int fd)
{
	unsigned int i, old_alloc;
	if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0)
		error("fcntl O_NONBLOCK: %s", strerror(errno));

	if (fd > max_fd)
		max_fd = fd;

	for (i = 0; i < sockets_alloc; i++)
		if (sockets[i].type == AUTH_UNUSED) {
			sockets[i].fd = fd;
			sockets[i].type = type;
			buffer_init(&sockets[i].input);
			buffer_init(&sockets[i].output);
			return;
		}
	old_alloc = sockets_alloc;
	sockets_alloc += 10;
	if (sockets)
		sockets = xrealloc(sockets, sockets_alloc * sizeof(sockets[0]));
	else
		sockets = xmalloc(sockets_alloc * sizeof(sockets[0]));
	for (i = old_alloc; i < sockets_alloc; i++)
		sockets[i].type = AUTH_UNUSED;
	sockets[old_alloc].type = type;
	sockets[old_alloc].fd = fd;
	buffer_init(&sockets[old_alloc].input);
	buffer_init(&sockets[old_alloc].output);
}

void
prepare_select(fd_set *readset, fd_set *writeset)
{
	unsigned int i;
	for (i = 0; i < sockets_alloc; i++)
		switch (sockets[i].type) {
		case AUTH_SOCKET:
		case AUTH_CONNECTION:
			FD_SET(sockets[i].fd, readset);
			if (buffer_len(&sockets[i].output) > 0)
				FD_SET(sockets[i].fd, writeset);
			break;
		case AUTH_UNUSED:
			break;
		default:
			fatal("Unknown socket type %d", sockets[i].type);
			break;
		}
}

void
after_select(fd_set *readset, fd_set *writeset)
{
	unsigned int i;
	int len, sock;
	socklen_t slen;
	char buf[1024];
	struct sockaddr_un sunaddr;

	for (i = 0; i < sockets_alloc; i++)
		switch (sockets[i].type) {
		case AUTH_UNUSED:
			break;
		case AUTH_SOCKET:
			if (FD_ISSET(sockets[i].fd, readset)) {
				slen = sizeof(sunaddr);
				sock = accept(sockets[i].fd, (struct sockaddr *) & sunaddr, &slen);
				if (sock < 0) {
					perror("accept from AUTH_SOCKET");
					break;
				}
				new_socket(AUTH_CONNECTION, sock);
			}
			break;
		case AUTH_CONNECTION:
			if (buffer_len(&sockets[i].output) > 0 &&
			    FD_ISSET(sockets[i].fd, writeset)) {
				len = write(sockets[i].fd, buffer_ptr(&sockets[i].output),
					 buffer_len(&sockets[i].output));
				if (len <= 0) {
					shutdown(sockets[i].fd, SHUT_RDWR);
					close(sockets[i].fd);
					sockets[i].type = AUTH_UNUSED;
					buffer_free(&sockets[i].input);
					buffer_free(&sockets[i].output);
					break;
				}
				buffer_consume(&sockets[i].output, len);
			}
			if (FD_ISSET(sockets[i].fd, readset)) {
				len = read(sockets[i].fd, buf, sizeof(buf));
				if (len <= 0) {
					shutdown(sockets[i].fd, SHUT_RDWR);
					close(sockets[i].fd);
					sockets[i].type = AUTH_UNUSED;
					buffer_free(&sockets[i].input);
					buffer_free(&sockets[i].output);
					break;
				}
				buffer_append(&sockets[i].input, buf, len);
				process_message(&sockets[i]);
			}
			break;
		default:
			fatal("Unknown type %d", sockets[i].type);
		}
}

void
check_parent_exists(int sig)
{
	if (parent_pid != -1 && kill(parent_pid, 0) < 0) {
		/* printf("Parent has died - Authentication agent exiting.\n"); */
		exit(1);
	}
	signal(SIGALRM, check_parent_exists);
	alarm(10);
}

void
cleanup_socket(void)
{
	remove(socket_name);
	rmdir(socket_dir);
}

void
cleanup_exit(int i)
{
	cleanup_socket();
	exit(i);
}

void
usage()
{
	fprintf(stderr, "ssh-agent version %s\n", SSH_VERSION);
	fprintf(stderr, "Usage: %s [-c | -s] [-k] [command {args...]]\n",
		__progname);
	exit(1);
}

int
main(int ac, char **av)
{
	fd_set readset, writeset;
	int sock, c_flag = 0, k_flag = 0, s_flag = 0, ch;
	struct sockaddr_un sunaddr;
	pid_t pid;
	char *shell, *format, *pidstr, pidstrbuf[1 + 3 * sizeof pid];

	/* check if RSA support exists */
	if (rsa_alive() == 0) {
		fprintf(stderr,
			"%s: no RSA support in libssl and libcrypto.  See ssl(8).\n",
			__progname);
		exit(1);
	}
	while ((ch = getopt(ac, av, "cks")) != -1) {
		switch (ch) {
		case 'c':
			if (s_flag)
				usage();
			c_flag++;
			break;
		case 'k':
			k_flag++;
			break;
		case 's':
			if (c_flag)
				usage();
			s_flag++;
			break;
		default:
			usage();
		}
	}
	ac -= optind;
	av += optind;

	if (ac > 0 && (c_flag || k_flag || s_flag))
		usage();

	if (ac == 0 && !c_flag && !k_flag && !s_flag) {
		shell = getenv("SHELL");
		if (shell != NULL && strncmp(shell + strlen(shell) - 3, "csh", 3) == 0)
			c_flag = 1;
	}
	if (k_flag) {
		pidstr = getenv(SSH_AGENTPID_ENV_NAME);
		if (pidstr == NULL) {
			fprintf(stderr, "%s not set, cannot kill agent\n",
				SSH_AGENTPID_ENV_NAME);
			exit(1);
		}
		pid = atoi(pidstr);
		if (pid < 1) {	/* XXX PID_MAX check too */
		/* Yes, PID_MAX check please */
			fprintf(stderr, "%s=\"%s\", which is not a good PID\n",
				SSH_AGENTPID_ENV_NAME, pidstr);
			exit(1);
		}
		if (kill(pid, SIGTERM) == -1) {
			perror("kill");
			exit(1);
		}
		format = c_flag ? "unsetenv %s;\n" : "unset %s;\n";
		printf(format, SSH_AUTHSOCKET_ENV_NAME);
		printf(format, SSH_AGENTPID_ENV_NAME);
		printf("echo Agent pid %d killed;\n", pid);
		exit(0);
	}
	parent_pid = getpid();

	/* Create private directory for agent socket */
	strlcpy(socket_dir, "/tmp/ssh-XXXXXXXX", sizeof socket_dir);
	if (mkdtemp(socket_dir) == NULL) {
		perror("mkdtemp: private socket dir");
		exit(1);
	}
	snprintf(socket_name, sizeof socket_name, "%s/agent.%d", socket_dir,
		 parent_pid);

	/*
	 * Create socket early so it will exist before command gets run from
	 * the parent.
	 */
	sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock < 0) {
		perror("socket");
		cleanup_exit(1);
	}
	memset(&sunaddr, 0, sizeof(sunaddr));
	sunaddr.sun_family = AF_UNIX;
	strlcpy(sunaddr.sun_path, socket_name, sizeof(sunaddr.sun_path));
	if (bind(sock, (struct sockaddr *) & sunaddr, sizeof(sunaddr)) < 0) {
		perror("bind");
		cleanup_exit(1);
	}
	if (listen(sock, 5) < 0) {
		perror("listen");
		cleanup_exit(1);
	}
	/*
	 * Fork, and have the parent execute the command, if any, or present
	 * the socket data.  The child continues as the authentication agent.
	 */
	pid = fork();
	if (pid == -1) {
		perror("fork");
		exit(1);
	}
	if (pid != 0) {		/* Parent - execute the given command. */
		close(sock);
		snprintf(pidstrbuf, sizeof pidstrbuf, "%d", pid);
		if (ac == 0) {
			format = c_flag ? "setenv %s %s;\n" : "%s=%s; export %s;\n";
			printf(format, SSH_AUTHSOCKET_ENV_NAME, socket_name,
			       SSH_AUTHSOCKET_ENV_NAME);
			printf(format, SSH_AGENTPID_ENV_NAME, pidstrbuf,
			       SSH_AGENTPID_ENV_NAME);
			printf("echo Agent pid %d;\n", pid);
			exit(0);
		}
		setenv(SSH_AUTHSOCKET_ENV_NAME, socket_name, 1);
		setenv(SSH_AGENTPID_ENV_NAME, pidstrbuf, 1);
		execvp(av[0], av);
		perror(av[0]);
		exit(1);
	}
	close(0);
	close(1);
	close(2);

	if (setsid() == -1) {
		perror("setsid");
		cleanup_exit(1);
	}
	if (atexit(cleanup_socket) < 0) {
		perror("atexit");
		cleanup_exit(1);
	}
	new_socket(AUTH_SOCKET, sock);
	if (ac > 0) {
		signal(SIGALRM, check_parent_exists);
		alarm(10);
	}
	idtab_init();
	signal(SIGINT, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, cleanup_exit);
	signal(SIGTERM, cleanup_exit);
	while (1) {
		FD_ZERO(&readset);
		FD_ZERO(&writeset);
		prepare_select(&readset, &writeset);
		if (select(max_fd + 1, &readset, &writeset, NULL, NULL) < 0) {
			if (errno == EINTR)
				continue;
			exit(1);
		}
		after_select(&readset, &writeset);
	}
	/* NOTREACHED */
}
