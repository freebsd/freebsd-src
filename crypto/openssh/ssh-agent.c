/*	$OpenBSD: ssh-agent.c,v 1.26 2000/03/16 20:56:14 markus Exp $	*/

/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * Created: Wed Mar 29 03:46:59 1995 ylo
 * The authentication agent program.
 */

#include "includes.h"
RCSID("$OpenBSD: ssh-agent.c,v 1.26 2000/03/16 20:56:14 markus Exp $");

#include "ssh.h"
#include "rsa.h"
#include "authfd.h"
#include "buffer.h"
#include "bufaux.h"
#include "xmalloc.h"
#include "packet.h"
#include "getput.h"
#include "mpaux.h"

#include <ssl/md5.h>

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
	RSA *key;
	char *comment;
} Identity;

unsigned int num_identities = 0;
Identity *identities = NULL;

int max_fd = 0;

/* pid of shell == parent of agent */
int parent_pid = -1;

/* pathname and directory for AUTH_SOCKET */
char socket_name[1024];
char socket_dir[1024];

extern char *__progname;

void
process_request_identity(SocketEntry *e)
{
	Buffer msg;
	int i;

	buffer_init(&msg);
	buffer_put_char(&msg, SSH_AGENT_RSA_IDENTITIES_ANSWER);
	buffer_put_int(&msg, num_identities);
	for (i = 0; i < num_identities; i++) {
		buffer_put_int(&msg, BN_num_bits(identities[i].key->n));
		buffer_put_bignum(&msg, identities[i].key->e);
		buffer_put_bignum(&msg, identities[i].key->n);
		buffer_put_string(&msg, identities[i].comment,
				  strlen(identities[i].comment));
	}
	buffer_put_int(&e->output, buffer_len(&msg));
	buffer_append(&e->output, buffer_ptr(&msg), buffer_len(&msg));
	buffer_free(&msg);
}

void
process_authentication_challenge(SocketEntry *e)
{
	int i, pub_bits, len;
	BIGNUM *pub_e, *pub_n, *challenge;
	Buffer msg;
	MD5_CTX md;
	unsigned char buf[32], mdbuf[16], session_id[16];
	unsigned int response_type;

	buffer_init(&msg);
	pub_e = BN_new();
	pub_n = BN_new();
	challenge = BN_new();
	pub_bits = buffer_get_int(&e->input);
	buffer_get_bignum(&e->input, pub_e);
	buffer_get_bignum(&e->input, pub_n);
	buffer_get_bignum(&e->input, challenge);
	if (buffer_len(&e->input) == 0) {
		/* Compatibility code for old servers. */
		memset(session_id, 0, 16);
		response_type = 0;
	} else {
		/* New code. */
		buffer_get(&e->input, (char *) session_id, 16);
		response_type = buffer_get_int(&e->input);
	}
	for (i = 0; i < num_identities; i++)
		if (pub_bits == BN_num_bits(identities[i].key->n) &&
		    BN_cmp(pub_e, identities[i].key->e) == 0 &&
		    BN_cmp(pub_n, identities[i].key->n) == 0) {
			/* Decrypt the challenge using the private key. */
			rsa_private_decrypt(challenge, challenge, identities[i].key);

			/* Compute the desired response. */
			switch (response_type) {
			case 0:/* As of protocol 1.0 */
				/* This response type is no longer supported. */
				log("Compatibility with ssh protocol 1.0 no longer supported.");
				buffer_put_char(&msg, SSH_AGENT_FAILURE);
				goto send;

			case 1:/* As of protocol 1.1 */
				/* The response is MD5 of decrypted challenge plus session id. */
				len = BN_num_bytes(challenge);

				if (len <= 0 || len > 32) {
					fatal("process_authentication_challenge: "
					 "bad challenge length %d", len);
				}
				memset(buf, 0, 32);
				BN_bn2bin(challenge, buf + 32 - len);
				MD5_Init(&md);
				MD5_Update(&md, buf, 32);
				MD5_Update(&md, session_id, 16);
				MD5_Final(mdbuf, &md);
				break;

			default:
				fatal("process_authentication_challenge: bad response_type %d",
				      response_type);
				break;
			}

			/* Send the response. */
			buffer_put_char(&msg, SSH_AGENT_RSA_RESPONSE);
			for (i = 0; i < 16; i++)
				buffer_put_char(&msg, mdbuf[i]);

			goto send;
		}
	/* Unknown identity.  Send failure. */
	buffer_put_char(&msg, SSH_AGENT_FAILURE);
send:
	buffer_put_int(&e->output, buffer_len(&msg));
	buffer_append(&e->output, buffer_ptr(&msg),
		      buffer_len(&msg));
	buffer_free(&msg);
	BN_clear_free(pub_e);
	BN_clear_free(pub_n);
	BN_clear_free(challenge);
}

void
process_remove_identity(SocketEntry *e)
{
	unsigned int bits;
	unsigned int i;
	BIGNUM *dummy, *n;

	dummy = BN_new();
	n = BN_new();

	/* Get the key from the packet. */
	bits = buffer_get_int(&e->input);
	buffer_get_bignum(&e->input, dummy);
	buffer_get_bignum(&e->input, n);

	if (bits != BN_num_bits(n))
		error("Warning: identity keysize mismatch: actual %d, announced %d",
		      BN_num_bits(n), bits);

	/* Check if we have the key. */
	for (i = 0; i < num_identities; i++)
		if (BN_cmp(identities[i].key->n, n) == 0) {
			/*
			 * We have this key.  Free the old key.  Since we
			 * don\'t want to leave empty slots in the middle of
			 * the array, we actually free the key there and copy
			 * data from the last entry.
			 */
			RSA_free(identities[i].key);
			xfree(identities[i].comment);
			if (i < num_identities - 1)
				identities[i] = identities[num_identities - 1];
			num_identities--;
			BN_clear_free(dummy);
			BN_clear_free(n);

			/* Send success. */
			buffer_put_int(&e->output, 1);
			buffer_put_char(&e->output, SSH_AGENT_SUCCESS);
			return;
		}
	/* We did not have the key. */
	BN_clear(dummy);
	BN_clear(n);

	/* Send failure. */
	buffer_put_int(&e->output, 1);
	buffer_put_char(&e->output, SSH_AGENT_FAILURE);
}

/*
 * Removes all identities from the agent.
 */
void
process_remove_all_identities(SocketEntry *e)
{
	unsigned int i;

	/* Loop over all identities and clear the keys. */
	for (i = 0; i < num_identities; i++) {
		RSA_free(identities[i].key);
		xfree(identities[i].comment);
	}

	/* Mark that there are no identities. */
	num_identities = 0;

	/* Send success. */
	buffer_put_int(&e->output, 1);
	buffer_put_char(&e->output, SSH_AGENT_SUCCESS);
	return;
}

/*
 * Adds an identity to the agent.
 */
void
process_add_identity(SocketEntry *e)
{
	RSA *k;
	int i;
	BIGNUM *aux;
	BN_CTX *ctx;

	if (num_identities == 0)
		identities = xmalloc(sizeof(Identity));
	else
		identities = xrealloc(identities, (num_identities + 1) * sizeof(Identity));

	identities[num_identities].key = RSA_new();
	k = identities[num_identities].key;
	buffer_get_int(&e->input);	/* bits */
	k->n = BN_new();
	buffer_get_bignum(&e->input, k->n);
	k->e = BN_new();
	buffer_get_bignum(&e->input, k->e);
	k->d = BN_new();
	buffer_get_bignum(&e->input, k->d);
	k->iqmp = BN_new();
	buffer_get_bignum(&e->input, k->iqmp);
	/* SSH and SSL have p and q swapped */
	k->q = BN_new();
	buffer_get_bignum(&e->input, k->q);	/* p */
	k->p = BN_new();
	buffer_get_bignum(&e->input, k->p);	/* q */

	/* Generate additional parameters */
	aux = BN_new();
	ctx = BN_CTX_new();

	BN_sub(aux, k->q, BN_value_one());
	k->dmq1 = BN_new();
	BN_mod(k->dmq1, k->d, aux, ctx);

	BN_sub(aux, k->p, BN_value_one());
	k->dmp1 = BN_new();
	BN_mod(k->dmp1, k->d, aux, ctx);

	BN_clear_free(aux);
	BN_CTX_free(ctx);

	identities[num_identities].comment = buffer_get_string(&e->input, NULL);

	/* Check if we already have the key. */
	for (i = 0; i < num_identities; i++)
		if (BN_cmp(identities[i].key->n, k->n) == 0) {
			/*
			 * We already have this key.  Clear and free the new
			 * data and return success.
			 */
			RSA_free(k);
			xfree(identities[num_identities].comment);

			/* Send success. */
			buffer_put_int(&e->output, 1);
			buffer_put_char(&e->output, SSH_AGENT_SUCCESS);
			return;
		}
	/* Increment the number of identities. */
	num_identities++;

	/* Send a success message. */
	buffer_put_int(&e->output, 1);
	buffer_put_char(&e->output, SSH_AGENT_SUCCESS);
}

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
	case SSH_AGENTC_REQUEST_RSA_IDENTITIES:
		process_request_identity(e);
		break;
	case SSH_AGENTC_RSA_CHALLENGE:
		process_authentication_challenge(e);
		break;
	case SSH_AGENTC_ADD_RSA_IDENTITY:
		process_add_identity(e);
		break;
	case SSH_AGENTC_REMOVE_RSA_IDENTITY:
		process_remove_identity(e);
		break;
	case SSH_AGENTC_REMOVE_ALL_RSA_IDENTITIES:
		process_remove_all_identities(e);
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
	if (kill(parent_pid, 0) < 0) {
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
