/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * Created: Sat Mar 18 22:15:47 1995 ylo
 * Code to connect to a remote host, and to perform the client side of the
 * login (authentication) dialog.
 */

#include "includes.h"
RCSID("$OpenBSD: sshconnect.c,v 1.58 2000/03/23 22:15:33 markus Exp $");

#include <ssl/bn.h>
#include "xmalloc.h"
#include "rsa.h"
#include "ssh.h"
#include "packet.h"
#include "authfd.h"
#include "cipher.h"
#include "mpaux.h"
#include "uidswap.h"
#include "compat.h"
#include "readconf.h"

#include <ssl/rsa.h>
#include <ssl/dsa.h>
#include <ssl/md5.h>
#include "key.h"
#include "hostfile.h"

/* Session id for the current session. */
unsigned char session_id[16];

/* authentications supported by server */
unsigned int supported_authentications;

extern Options options;
extern char *__progname;

/*
 * Connect to the given ssh server using a proxy command.
 */
int
ssh_proxy_connect(const char *host, u_short port, uid_t original_real_uid,
		  const char *proxy_command)
{
	Buffer command;
	const char *cp;
	char *command_string;
	int pin[2], pout[2];
	int pid;
	char strport[NI_MAXSERV];

	/* Convert the port number into a string. */
	snprintf(strport, sizeof strport, "%hu", port);

	/* Build the final command string in the buffer by making the
	   appropriate substitutions to the given proxy command. */
	buffer_init(&command);
	for (cp = proxy_command; *cp; cp++) {
		if (cp[0] == '%' && cp[1] == '%') {
			buffer_append(&command, "%", 1);
			cp++;
			continue;
		}
		if (cp[0] == '%' && cp[1] == 'h') {
			buffer_append(&command, host, strlen(host));
			cp++;
			continue;
		}
		if (cp[0] == '%' && cp[1] == 'p') {
			buffer_append(&command, strport, strlen(strport));
			cp++;
			continue;
		}
		buffer_append(&command, cp, 1);
	}
	buffer_append(&command, "\0", 1);

	/* Get the final command string. */
	command_string = buffer_ptr(&command);

	/* Create pipes for communicating with the proxy. */
	if (pipe(pin) < 0 || pipe(pout) < 0)
		fatal("Could not create pipes to communicate with the proxy: %.100s",
		      strerror(errno));

	debug("Executing proxy command: %.500s", command_string);

	/* Fork and execute the proxy command. */
	if ((pid = fork()) == 0) {
		char *argv[10];

		/* Child.  Permanently give up superuser privileges. */
		permanently_set_uid(original_real_uid);

		/* Redirect stdin and stdout. */
		close(pin[1]);
		if (pin[0] != 0) {
			if (dup2(pin[0], 0) < 0)
				perror("dup2 stdin");
			close(pin[0]);
		}
		close(pout[0]);
		if (dup2(pout[1], 1) < 0)
			perror("dup2 stdout");
		/* Cannot be 1 because pin allocated two descriptors. */
		close(pout[1]);

		/* Stderr is left as it is so that error messages get
		   printed on the user's terminal. */
		argv[0] = "/bin/sh";
		argv[1] = "-c";
		argv[2] = command_string;
		argv[3] = NULL;

		/* Execute the proxy command.  Note that we gave up any
		   extra privileges above. */
		execv("/bin/sh", argv);
		perror("/bin/sh");
		exit(1);
	}
	/* Parent. */
	if (pid < 0)
		fatal("fork failed: %.100s", strerror(errno));

	/* Close child side of the descriptors. */
	close(pin[0]);
	close(pout[1]);

	/* Free the command name. */
	buffer_free(&command);

	/* Set the connection file descriptors. */
	packet_set_connection(pout[0], pin[1]);

	return 1;
}

/*
 * Creates a (possibly privileged) socket for use as the ssh connection.
 */
int
ssh_create_socket(uid_t original_real_uid, int privileged, int family)
{
	int sock;

	/*
	 * If we are running as root and want to connect to a privileged
	 * port, bind our own socket to a privileged port.
	 */
	if (privileged) {
		int p = IPPORT_RESERVED - 1;
		sock = rresvport_af(&p, family);
		if (sock < 0)
			error("rresvport: af=%d %.100s", family, strerror(errno));
		else
			debug("Allocated local port %d.", p);
	} else {
		/*
		 * Just create an ordinary socket on arbitrary port.  We use
		 * the user's uid to create the socket.
		 */
		temporarily_use_uid(original_real_uid);
		sock = socket(family, SOCK_STREAM, 0);
		if (sock < 0)
			error("socket: %.100s", strerror(errno));
		restore_uid();
	}
	return sock;
}

/*
 * Opens a TCP/IP connection to the remote server on the given host.
 * The address of the remote host will be returned in hostaddr.
 * If port is 0, the default port will be used.  If anonymous is zero,
 * a privileged port will be allocated to make the connection.
 * This requires super-user privileges if anonymous is false.
 * Connection_attempts specifies the maximum number of tries (one per
 * second).  If proxy_command is non-NULL, it specifies the command (with %h
 * and %p substituted for host and port, respectively) to use to contact
 * the daemon.
 */
int
ssh_connect(const char *host, struct sockaddr_storage * hostaddr,
	    u_short port, int connection_attempts,
	    int anonymous, uid_t original_real_uid,
	    const char *proxy_command)
{
	int sock = -1, attempt;
	struct servent *sp;
	struct addrinfo hints, *ai, *aitop;
	char ntop[NI_MAXHOST], strport[NI_MAXSERV];
	int gaierr;
	struct linger linger;

	debug("ssh_connect: getuid %d geteuid %d anon %d",
	      (int) getuid(), (int) geteuid(), anonymous);

	/* Get default port if port has not been set. */
	if (port == 0) {
		sp = getservbyname(SSH_SERVICE_NAME, "tcp");
		if (sp)
			port = ntohs(sp->s_port);
		else
			port = SSH_DEFAULT_PORT;
	}
	/* If a proxy command is given, connect using it. */
	if (proxy_command != NULL)
		return ssh_proxy_connect(host, port, original_real_uid, proxy_command);

	/* No proxy command. */

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = IPv4or6;
	hints.ai_socktype = SOCK_STREAM;
	snprintf(strport, sizeof strport, "%d", port);
	if ((gaierr = getaddrinfo(host, strport, &hints, &aitop)) != 0)
		fatal("%s: %.100s: %s", __progname, host,
		    gai_strerror(gaierr));

	/*
	 * Try to connect several times.  On some machines, the first time
	 * will sometimes fail.  In general socket code appears to behave
	 * quite magically on many machines.
	 */
	for (attempt = 0; attempt < connection_attempts; attempt++) {
		if (attempt > 0)
			debug("Trying again...");

		/* Loop through addresses for this host, and try each one in
 		   sequence until the connection succeeds. */
		for (ai = aitop; ai; ai = ai->ai_next) {
			if (ai->ai_family != AF_INET && ai->ai_family != AF_INET6)
				continue;
			if (getnameinfo(ai->ai_addr, ai->ai_addrlen,
			    ntop, sizeof(ntop), strport, sizeof(strport),
			    NI_NUMERICHOST|NI_NUMERICSERV) != 0) {
				error("ssh_connect: getnameinfo failed");
				continue;
			}
			debug("Connecting to %.200s [%.100s] port %s.",
				host, ntop, strport);

			/* Create a socket for connecting. */
			sock = ssh_create_socket(original_real_uid, 
			    !anonymous && geteuid() == 0 && port < IPPORT_RESERVED,
			    ai->ai_family);
			if (sock < 0)
				continue;

			/* Connect to the host.  We use the user's uid in the
			 * hope that it will help with tcp_wrappers showing
			 * the remote uid as root.
			 */
			temporarily_use_uid(original_real_uid);
			if (connect(sock, ai->ai_addr, ai->ai_addrlen) >= 0) {
				/* Successful connection. */
				memcpy(hostaddr, ai->ai_addr, sizeof(*hostaddr));
				restore_uid();
				break;
			} else {
				debug("connect: %.100s", strerror(errno));
				restore_uid();
				/*
				 * Close the failed socket; there appear to
				 * be some problems when reusing a socket for
				 * which connect() has already returned an
				 * error.
				 */
				shutdown(sock, SHUT_RDWR);
				close(sock);
			}
		}
		if (ai)
			break;	/* Successful connection. */

		/* Sleep a moment before retrying. */
		sleep(1);
	}

	freeaddrinfo(aitop);

	/* Return failure if we didn't get a successful connection. */
	if (attempt >= connection_attempts)
		return 0;

	debug("Connection established.");

	/*
	 * Set socket options.  We would like the socket to disappear as soon
	 * as it has been closed for whatever reason.
	 */
	/* setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (void *)&on, sizeof(on)); */
	linger.l_onoff = 1;
	linger.l_linger = 5;
	setsockopt(sock, SOL_SOCKET, SO_LINGER, (void *) &linger, sizeof(linger));

	/* Set the connection. */
	packet_set_connection(sock, sock);

	return 1;
}

/*
 * Checks if the user has an authentication agent, and if so, tries to
 * authenticate using the agent.
 */
int
try_agent_authentication()
{
	int status, type;
	char *comment;
	AuthenticationConnection *auth;
	unsigned char response[16];
	unsigned int i;
	BIGNUM *e, *n, *challenge;

	/* Get connection to the agent. */
	auth = ssh_get_authentication_connection();
	if (!auth)
		return 0;

	e = BN_new();
	n = BN_new();
	challenge = BN_new();

	/* Loop through identities served by the agent. */
	for (status = ssh_get_first_identity(auth, e, n, &comment);
	     status;
	     status = ssh_get_next_identity(auth, e, n, &comment)) {
		int plen, clen;

		/* Try this identity. */
		debug("Trying RSA authentication via agent with '%.100s'", comment);
		xfree(comment);

		/* Tell the server that we are willing to authenticate using this key. */
		packet_start(SSH_CMSG_AUTH_RSA);
		packet_put_bignum(n);
		packet_send();
		packet_write_wait();

		/* Wait for server's response. */
		type = packet_read(&plen);

		/* The server sends failure if it doesn\'t like our key or
		   does not support RSA authentication. */
		if (type == SSH_SMSG_FAILURE) {
			debug("Server refused our key.");
			continue;
		}
		/* Otherwise it should have sent a challenge. */
		if (type != SSH_SMSG_AUTH_RSA_CHALLENGE)
			packet_disconnect("Protocol error during RSA authentication: %d",
					  type);

		packet_get_bignum(challenge, &clen);

		packet_integrity_check(plen, clen, type);

		debug("Received RSA challenge from server.");

		/* Ask the agent to decrypt the challenge. */
		if (!ssh_decrypt_challenge(auth, e, n, challenge,
					   session_id, 1, response)) {
			/* The agent failed to authenticate this identifier although it
			   advertised it supports this.  Just return a wrong value. */
			log("Authentication agent failed to decrypt challenge.");
			memset(response, 0, sizeof(response));
		}
		debug("Sending response to RSA challenge.");

		/* Send the decrypted challenge back to the server. */
		packet_start(SSH_CMSG_AUTH_RSA_RESPONSE);
		for (i = 0; i < 16; i++)
			packet_put_char(response[i]);
		packet_send();
		packet_write_wait();

		/* Wait for response from the server. */
		type = packet_read(&plen);

		/* The server returns success if it accepted the authentication. */
		if (type == SSH_SMSG_SUCCESS) {
			debug("RSA authentication accepted by server.");
			BN_clear_free(e);
			BN_clear_free(n);
			BN_clear_free(challenge);
			return 1;
		}
		/* Otherwise it should return failure. */
		if (type != SSH_SMSG_FAILURE)
			packet_disconnect("Protocol error waiting RSA auth response: %d",
					  type);
	}

	BN_clear_free(e);
	BN_clear_free(n);
	BN_clear_free(challenge);

	debug("RSA authentication using agent refused.");
	return 0;
}

/*
 * Computes the proper response to a RSA challenge, and sends the response to
 * the server.
 */
void
respond_to_rsa_challenge(BIGNUM * challenge, RSA * prv)
{
	unsigned char buf[32], response[16];
	MD5_CTX md;
	int i, len;

	/* Decrypt the challenge using the private key. */
	rsa_private_decrypt(challenge, challenge, prv);

	/* Compute the response. */
	/* The response is MD5 of decrypted challenge plus session id. */
	len = BN_num_bytes(challenge);
	if (len <= 0 || len > sizeof(buf))
		packet_disconnect("respond_to_rsa_challenge: bad challenge length %d",
				  len);

	memset(buf, 0, sizeof(buf));
	BN_bn2bin(challenge, buf + sizeof(buf) - len);
	MD5_Init(&md);
	MD5_Update(&md, buf, 32);
	MD5_Update(&md, session_id, 16);
	MD5_Final(response, &md);

	debug("Sending response to host key RSA challenge.");

	/* Send the response back to the server. */
	packet_start(SSH_CMSG_AUTH_RSA_RESPONSE);
	for (i = 0; i < 16; i++)
		packet_put_char(response[i]);
	packet_send();
	packet_write_wait();

	memset(buf, 0, sizeof(buf));
	memset(response, 0, sizeof(response));
	memset(&md, 0, sizeof(md));
}

/*
 * Checks if the user has authentication file, and if so, tries to authenticate
 * the user using it.
 */
int
try_rsa_authentication(const char *authfile)
{
	BIGNUM *challenge;
	RSA *private_key;
	RSA *public_key;
	char *passphrase, *comment;
	int type, i;
	int plen, clen;

	/* Try to load identification for the authentication key. */
	public_key = RSA_new();
	if (!load_public_key(authfile, public_key, &comment)) {
		RSA_free(public_key);
		/* Could not load it.  Fail. */
		return 0;
	}
	debug("Trying RSA authentication with key '%.100s'", comment);

	/* Tell the server that we are willing to authenticate using this key. */
	packet_start(SSH_CMSG_AUTH_RSA);
	packet_put_bignum(public_key->n);
	packet_send();
	packet_write_wait();

	/* We no longer need the public key. */
	RSA_free(public_key);

	/* Wait for server's response. */
	type = packet_read(&plen);

	/*
	 * The server responds with failure if it doesn\'t like our key or
	 * doesn\'t support RSA authentication.
	 */
	if (type == SSH_SMSG_FAILURE) {
		debug("Server refused our key.");
		xfree(comment);
		return 0;
	}
	/* Otherwise, the server should respond with a challenge. */
	if (type != SSH_SMSG_AUTH_RSA_CHALLENGE)
		packet_disconnect("Protocol error during RSA authentication: %d", type);

	/* Get the challenge from the packet. */
	challenge = BN_new();
	packet_get_bignum(challenge, &clen);

	packet_integrity_check(plen, clen, type);

	debug("Received RSA challenge from server.");

	private_key = RSA_new();
	/*
	 * Load the private key.  Try first with empty passphrase; if it
	 * fails, ask for a passphrase.
	 */
	if (!load_private_key(authfile, "", private_key, NULL)) {
		char buf[300];
		snprintf(buf, sizeof buf, "Enter passphrase for RSA key '%.100s': ",
		    comment);
		if (!options.batch_mode)
			passphrase = read_passphrase(buf, 0);
		else {
			debug("Will not query passphrase for %.100s in batch mode.",
			      comment);
			passphrase = xstrdup("");
		}

		/* Load the authentication file using the pasphrase. */
		if (!load_private_key(authfile, passphrase, private_key, NULL)) {
			memset(passphrase, 0, strlen(passphrase));
			xfree(passphrase);
			error("Bad passphrase.");

			/* Send a dummy response packet to avoid protocol error. */
			packet_start(SSH_CMSG_AUTH_RSA_RESPONSE);
			for (i = 0; i < 16; i++)
				packet_put_char(0);
			packet_send();
			packet_write_wait();

			/* Expect the server to reject it... */
			packet_read_expect(&plen, SSH_SMSG_FAILURE);
			xfree(comment);
			return 0;
		}
		/* Destroy the passphrase. */
		memset(passphrase, 0, strlen(passphrase));
		xfree(passphrase);
	}
	/* We no longer need the comment. */
	xfree(comment);

	/* Compute and send a response to the challenge. */
	respond_to_rsa_challenge(challenge, private_key);

	/* Destroy the private key. */
	RSA_free(private_key);

	/* We no longer need the challenge. */
	BN_clear_free(challenge);

	/* Wait for response from the server. */
	type = packet_read(&plen);
	if (type == SSH_SMSG_SUCCESS) {
		debug("RSA authentication accepted by server.");
		return 1;
	}
	if (type != SSH_SMSG_FAILURE)
		packet_disconnect("Protocol error waiting RSA auth response: %d", type);
	debug("RSA authentication refused.");
	return 0;
}

/*
 * Tries to authenticate the user using combined rhosts or /etc/hosts.equiv
 * authentication and RSA host authentication.
 */
int
try_rhosts_rsa_authentication(const char *local_user, RSA * host_key)
{
	int type;
	BIGNUM *challenge;
	int plen, clen;

	debug("Trying rhosts or /etc/hosts.equiv with RSA host authentication.");

	/* Tell the server that we are willing to authenticate using this key. */
	packet_start(SSH_CMSG_AUTH_RHOSTS_RSA);
	packet_put_string(local_user, strlen(local_user));
	packet_put_int(BN_num_bits(host_key->n));
	packet_put_bignum(host_key->e);
	packet_put_bignum(host_key->n);
	packet_send();
	packet_write_wait();

	/* Wait for server's response. */
	type = packet_read(&plen);

	/* The server responds with failure if it doesn't admit our
	   .rhosts authentication or doesn't know our host key. */
	if (type == SSH_SMSG_FAILURE) {
		debug("Server refused our rhosts authentication or host key.");
		return 0;
	}
	/* Otherwise, the server should respond with a challenge. */
	if (type != SSH_SMSG_AUTH_RSA_CHALLENGE)
		packet_disconnect("Protocol error during RSA authentication: %d", type);

	/* Get the challenge from the packet. */
	challenge = BN_new();
	packet_get_bignum(challenge, &clen);

	packet_integrity_check(plen, clen, type);

	debug("Received RSA challenge for host key from server.");

	/* Compute a response to the challenge. */
	respond_to_rsa_challenge(challenge, host_key);

	/* We no longer need the challenge. */
	BN_clear_free(challenge);

	/* Wait for response from the server. */
	type = packet_read(&plen);
	if (type == SSH_SMSG_SUCCESS) {
		debug("Rhosts or /etc/hosts.equiv with RSA host authentication accepted by server.");
		return 1;
	}
	if (type != SSH_SMSG_FAILURE)
		packet_disconnect("Protocol error waiting RSA auth response: %d", type);
	debug("Rhosts or /etc/hosts.equiv with RSA host authentication refused.");
	return 0;
}

#ifdef KRB4
int
try_kerberos_authentication()
{
	KTEXT_ST auth;		/* Kerberos data */
	char *reply;
	char inst[INST_SZ];
	char *realm;
	CREDENTIALS cred;
	int r, type, plen;
	socklen_t slen;
	Key_schedule schedule;
	u_long checksum, cksum;
	MSG_DAT msg_data;
	struct sockaddr_in local, foreign;
	struct stat st;

	/* Don't do anything if we don't have any tickets. */
	if (stat(tkt_string(), &st) < 0)
		return 0;

	strncpy(inst, (char *) krb_get_phost(get_canonical_hostname()), INST_SZ);

	realm = (char *) krb_realmofhost(get_canonical_hostname());
	if (!realm) {
		debug("Kerberos V4: no realm for %s", get_canonical_hostname());
		return 0;
	}
	/* This can really be anything. */
	checksum = (u_long) getpid();

	r = krb_mk_req(&auth, KRB4_SERVICE_NAME, inst, realm, checksum);
	if (r != KSUCCESS) {
		debug("Kerberos V4 krb_mk_req failed: %s", krb_err_txt[r]);
		return 0;
	}
	/* Get session key to decrypt the server's reply with. */
	r = krb_get_cred(KRB4_SERVICE_NAME, inst, realm, &cred);
	if (r != KSUCCESS) {
		debug("get_cred failed: %s", krb_err_txt[r]);
		return 0;
	}
	des_key_sched((des_cblock *) cred.session, schedule);

	/* Send authentication info to server. */
	packet_start(SSH_CMSG_AUTH_KERBEROS);
	packet_put_string((char *) auth.dat, auth.length);
	packet_send();
	packet_write_wait();

	/* Zero the buffer. */
	(void) memset(auth.dat, 0, MAX_KTXT_LEN);

	slen = sizeof(local);
	memset(&local, 0, sizeof(local));
	if (getsockname(packet_get_connection_in(),
			(struct sockaddr *) & local, &slen) < 0)
		debug("getsockname failed: %s", strerror(errno));

	slen = sizeof(foreign);
	memset(&foreign, 0, sizeof(foreign));
	if (getpeername(packet_get_connection_in(),
			(struct sockaddr *) & foreign, &slen) < 0) {
		debug("getpeername failed: %s", strerror(errno));
		fatal_cleanup();
	}
	/* Get server reply. */
	type = packet_read(&plen);
	switch (type) {
	case SSH_SMSG_FAILURE:
		/* Should really be SSH_SMSG_AUTH_KERBEROS_FAILURE */
		debug("Kerberos V4 authentication failed.");
		return 0;
		break;

	case SSH_SMSG_AUTH_KERBEROS_RESPONSE:
		/* SSH_SMSG_AUTH_KERBEROS_SUCCESS */
		debug("Kerberos V4 authentication accepted.");

		/* Get server's response. */
		reply = packet_get_string((unsigned int *) &auth.length);
		memcpy(auth.dat, reply, auth.length);
		xfree(reply);

		packet_integrity_check(plen, 4 + auth.length, type);

		/*
		 * If his response isn't properly encrypted with the session
		 * key, and the decrypted checksum fails to match, he's
		 * bogus. Bail out.
		 */
		r = krb_rd_priv(auth.dat, auth.length, schedule, &cred.session,
				&foreign, &local, &msg_data);
		if (r != KSUCCESS) {
			debug("Kerberos V4 krb_rd_priv failed: %s", krb_err_txt[r]);
			packet_disconnect("Kerberos V4 challenge failed!");
		}
		/* Fetch the (incremented) checksum that we supplied in the request. */
		(void) memcpy((char *) &cksum, (char *) msg_data.app_data, sizeof(cksum));
		cksum = ntohl(cksum);

		/* If it matches, we're golden. */
		if (cksum == checksum + 1) {
			debug("Kerberos V4 challenge successful.");
			return 1;
		} else
			packet_disconnect("Kerberos V4 challenge failed!");
		break;

	default:
		packet_disconnect("Protocol error on Kerberos V4 response: %d", type);
	}
	return 0;
}

#endif /* KRB4 */

#ifdef AFS
int
send_kerberos_tgt()
{
	CREDENTIALS *creds;
	char pname[ANAME_SZ], pinst[INST_SZ], prealm[REALM_SZ];
	int r, type, plen;
	char buffer[8192];
	struct stat st;

	/* Don't do anything if we don't have any tickets. */
	if (stat(tkt_string(), &st) < 0)
		return 0;

	creds = xmalloc(sizeof(*creds));

	if ((r = krb_get_tf_fullname(TKT_FILE, pname, pinst, prealm)) != KSUCCESS) {
		debug("Kerberos V4 tf_fullname failed: %s", krb_err_txt[r]);
		return 0;
	}
	if ((r = krb_get_cred("krbtgt", prealm, prealm, creds)) != GC_OK) {
		debug("Kerberos V4 get_cred failed: %s", krb_err_txt[r]);
		return 0;
	}
	if (time(0) > krb_life_to_time(creds->issue_date, creds->lifetime)) {
		debug("Kerberos V4 ticket expired: %s", TKT_FILE);
		return 0;
	}
	creds_to_radix(creds, (unsigned char *)buffer);
	xfree(creds);

	packet_start(SSH_CMSG_HAVE_KERBEROS_TGT);
	packet_put_string(buffer, strlen(buffer));
	packet_send();
	packet_write_wait();

	type = packet_read(&plen);

	if (type == SSH_SMSG_FAILURE)
		debug("Kerberos TGT for realm %s rejected.", prealm);
	else if (type != SSH_SMSG_SUCCESS)
		packet_disconnect("Protocol error on Kerberos TGT response: %d", type);

	return 1;
}

void
send_afs_tokens(void)
{
	CREDENTIALS creds;
	struct ViceIoctl parms;
	struct ClearToken ct;
	int i, type, len, plen;
	char buf[2048], *p, *server_cell;
	char buffer[8192];

	/* Move over ktc_GetToken, here's something leaner. */
	for (i = 0; i < 100; i++) {	/* just in case */
		parms.in = (char *) &i;
		parms.in_size = sizeof(i);
		parms.out = buf;
		parms.out_size = sizeof(buf);
		if (k_pioctl(0, VIOCGETTOK, &parms, 0) != 0)
			break;
		p = buf;

		/* Get secret token. */
		memcpy(&creds.ticket_st.length, p, sizeof(unsigned int));
		if (creds.ticket_st.length > MAX_KTXT_LEN)
			break;
		p += sizeof(unsigned int);
		memcpy(creds.ticket_st.dat, p, creds.ticket_st.length);
		p += creds.ticket_st.length;

		/* Get clear token. */
		memcpy(&len, p, sizeof(len));
		if (len != sizeof(struct ClearToken))
			break;
		p += sizeof(len);
		memcpy(&ct, p, len);
		p += len;
		p += sizeof(len);	/* primary flag */
		server_cell = p;

		/* Flesh out our credentials. */
		strlcpy(creds.service, "afs", sizeof creds.service);
		creds.instance[0] = '\0';
		strlcpy(creds.realm, server_cell, REALM_SZ);
		memcpy(creds.session, ct.HandShakeKey, DES_KEY_SZ);
		creds.issue_date = ct.BeginTimestamp;
		creds.lifetime = krb_time_to_life(creds.issue_date, ct.EndTimestamp);
		creds.kvno = ct.AuthHandle;
		snprintf(creds.pname, sizeof(creds.pname), "AFS ID %d", ct.ViceId);
		creds.pinst[0] = '\0';

		/* Encode token, ship it off. */
		if (!creds_to_radix(&creds, (unsigned char*) buffer))
			break;
		packet_start(SSH_CMSG_HAVE_AFS_TOKEN);
		packet_put_string(buffer, strlen(buffer));
		packet_send();
		packet_write_wait();

		/* Roger, Roger. Clearance, Clarence. What's your vector,
		   Victor? */
		type = packet_read(&plen);

		if (type == SSH_SMSG_FAILURE)
			debug("AFS token for cell %s rejected.", server_cell);
		else if (type != SSH_SMSG_SUCCESS)
			packet_disconnect("Protocol error on AFS token response: %d", type);
	}
}

#endif /* AFS */

/*
 * Tries to authenticate with any string-based challenge/response system.
 * Note that the client code is not tied to s/key or TIS.
 */
int
try_skey_authentication()
{
	int type, i;
	int payload_len;
	unsigned int clen;
	char *challenge, *response;

	debug("Doing skey authentication.");

	/* request a challenge */
	packet_start(SSH_CMSG_AUTH_TIS);
	packet_send();
	packet_write_wait();

	type = packet_read(&payload_len);
	if (type != SSH_SMSG_FAILURE &&
	    type != SSH_SMSG_AUTH_TIS_CHALLENGE) {
		packet_disconnect("Protocol error: got %d in response "
				  "to skey-auth", type);
	}
	if (type != SSH_SMSG_AUTH_TIS_CHALLENGE) {
		debug("No challenge for skey authentication.");
		return 0;
	}
	challenge = packet_get_string(&clen);
	packet_integrity_check(payload_len, (4 + clen), type);
	if (options.cipher == SSH_CIPHER_NONE)
		log("WARNING: Encryption is disabled! "
		    "Reponse will be transmitted in clear text.");
	fprintf(stderr, "%s\n", challenge);
	xfree(challenge);
	fflush(stderr);
	for (i = 0; i < options.number_of_password_prompts; i++) {
		if (i != 0)
			error("Permission denied, please try again.");
		response = read_passphrase("Response: ", 0);
		packet_start(SSH_CMSG_AUTH_TIS_RESPONSE);
		packet_put_string(response, strlen(response));
		memset(response, 0, strlen(response));
		xfree(response);
		packet_send();
		packet_write_wait();
		type = packet_read(&payload_len);
		if (type == SSH_SMSG_SUCCESS)
			return 1;
		if (type != SSH_SMSG_FAILURE)
			packet_disconnect("Protocol error: got %d in response "
					  "to skey-auth-reponse", type);
	}
	/* failure */
	return 0;
}

/*
 * Tries to authenticate with plain passwd authentication.
 */
int
try_password_authentication(char *prompt)
{
	int type, i, payload_len;
	char *password;

	debug("Doing password authentication.");
	if (options.cipher == SSH_CIPHER_NONE)
		log("WARNING: Encryption is disabled! Password will be transmitted in clear text.");
	for (i = 0; i < options.number_of_password_prompts; i++) {
		if (i != 0)
			error("Permission denied, please try again.");
		password = read_passphrase(prompt, 0);
		packet_start(SSH_CMSG_AUTH_PASSWORD);
		packet_put_string(password, strlen(password));
		memset(password, 0, strlen(password));
		xfree(password);
		packet_send();
		packet_write_wait();

		type = packet_read(&payload_len);
		if (type == SSH_SMSG_SUCCESS)
			return 1;
		if (type != SSH_SMSG_FAILURE)
			packet_disconnect("Protocol error: got %d in response to passwd auth", type);
	}
	/* failure */
	return 0;
}

/*
 * Waits for the server identification string, and sends our own
 * identification string.
 */
void
ssh_exchange_identification()
{
	char buf[256], remote_version[256];	/* must be same size! */
	int remote_major, remote_minor, i;
	int connection_in = packet_get_connection_in();
	int connection_out = packet_get_connection_out();

	/* Read other side\'s version identification. */
	for (i = 0; i < sizeof(buf) - 1; i++) {
		int len = read(connection_in, &buf[i], 1);
		if (len < 0)
			fatal("ssh_exchange_identification: read: %.100s", strerror(errno));
		if (len != 1)
			fatal("ssh_exchange_identification: Connection closed by remote host");
		if (buf[i] == '\r') {
			buf[i] = '\n';
			buf[i + 1] = 0;
			break;
		}
		if (buf[i] == '\n') {
			buf[i + 1] = 0;
			break;
		}
	}
	buf[sizeof(buf) - 1] = 0;

	/*
	 * Check that the versions match.  In future this might accept
	 * several versions and set appropriate flags to handle them.
	 */
	if (sscanf(buf, "SSH-%d.%d-%[^\n]\n", &remote_major, &remote_minor,
		   remote_version) != 3)
		fatal("Bad remote protocol version identification: '%.100s'", buf);
	debug("Remote protocol version %d.%d, remote software version %.100s",
	      remote_major, remote_minor, remote_version);

	/* Check if the remote protocol version is too old. */
	if (remote_major == 1 && remote_minor < 3)
		fatal("Remote machine has too old SSH software version.");

	/* We speak 1.3, too. */
	if (remote_major == 1 && remote_minor == 3) {
		enable_compat13();
		if (options.forward_agent) {
			log("Agent forwarding disabled for protocol 1.3");
			options.forward_agent = 0;
		}
	}
#if 0
	/*
	 * Removed for now, to permit compatibility with latter versions. The
	 * server will reject our version and disconnect if it doesn't
	 * support it.
	 */
	if (remote_major != PROTOCOL_MAJOR)
		fatal("Protocol major versions differ: %d vs. %d",
		      PROTOCOL_MAJOR, remote_major);
#endif

	/* Send our own protocol version identification. */
	snprintf(buf, sizeof buf, "SSH-%d.%d-%.100s\n",
	    PROTOCOL_MAJOR, PROTOCOL_MINOR, SSH_VERSION);
	if (atomicio(write, connection_out, buf, strlen(buf)) != strlen(buf))
		fatal("write: %.100s", strerror(errno));
}

int ssh_cipher_default = SSH_CIPHER_3DES;

int
read_yes_or_no(const char *prompt, int defval)
{
	char buf[1024];
	FILE *f;
	int retval = -1;

	if (isatty(0))
		f = stdin;
	else
		f = fopen("/dev/tty", "rw");

	if (f == NULL)
		return 0;

	fflush(stdout);

	while (1) {
		fprintf(stderr, "%s", prompt);
		if (fgets(buf, sizeof(buf), f) == NULL) {
			/* Print a newline (the prompt probably didn\'t have one). */
			fprintf(stderr, "\n");
			strlcpy(buf, "no", sizeof buf);
		}
		/* Remove newline from response. */
		if (strchr(buf, '\n'))
			*strchr(buf, '\n') = 0;

		if (buf[0] == 0)
			retval = defval;
		if (strcmp(buf, "yes") == 0)
			retval = 1;
		if (strcmp(buf, "no") == 0)
			retval = 0;

		if (retval != -1) {
			if (f != stdin)
				fclose(f);
			return retval;
		}
	}
}

/*
 * check whether the supplied host key is valid, return only if ok.
 */

void
check_host_key(char *host, struct sockaddr *hostaddr, Key *host_key)
{
	Key *file_key;
	char *ip = NULL;
	char hostline[1000], *hostp;
	HostStatus host_status;
	HostStatus ip_status;
	int local = 0, host_ip_differ = 0;
	char ntop[NI_MAXHOST];

	/*
	 * Force accepting of the host key for loopback/localhost. The
	 * problem is that if the home directory is NFS-mounted to multiple
	 * machines, localhost will refer to a different machine in each of
	 * them, and the user will get bogus HOST_CHANGED warnings.  This
	 * essentially disables host authentication for localhost; however,
	 * this is probably not a real problem.
	 */
	switch (hostaddr->sa_family) {
	case AF_INET:
		local = (ntohl(((struct sockaddr_in *)hostaddr)->sin_addr.s_addr) >> 24) == IN_LOOPBACKNET;
		break;
	case AF_INET6:
		local = IN6_IS_ADDR_LOOPBACK(&(((struct sockaddr_in6 *)hostaddr)->sin6_addr));
		break;
	default:
		local = 0;
		break;
	}
	if (local) {
		debug("Forcing accepting of host key for loopback/localhost.");
		return;
	}

	/*
	 * Turn off check_host_ip for proxy connects, since
	 * we don't have the remote ip-address
	 */
	if (options.proxy_command != NULL && options.check_host_ip)
		options.check_host_ip = 0;

	if (options.check_host_ip) {
		if (getnameinfo(hostaddr, hostaddr->sa_len, ntop, sizeof(ntop),
		    NULL, 0, NI_NUMERICHOST) != 0)
			fatal("check_host_key: getnameinfo failed");
		ip = xstrdup(ntop);
	}

	/*
	 * Store the host key from the known host file in here so that we can
	 * compare it with the key for the IP address.
	 */
	file_key = key_new(host_key->type);

	/*
	 * Check if the host key is present in the user\'s list of known
	 * hosts or in the systemwide list.
	 */
	host_status = check_host_in_hostfile(options.user_hostfile, host, host_key, file_key);
	if (host_status == HOST_NEW)
		host_status = check_host_in_hostfile(options.system_hostfile, host, host_key, file_key);
	/*
	 * Also perform check for the ip address, skip the check if we are
	 * localhost or the hostname was an ip address to begin with
	 */
	if (options.check_host_ip && !local && strcmp(host, ip)) {
		Key *ip_key = key_new(host_key->type);
		ip_status = check_host_in_hostfile(options.user_hostfile, ip, host_key, ip_key);

		if (ip_status == HOST_NEW)
			ip_status = check_host_in_hostfile(options.system_hostfile, ip, host_key, ip_key);
		if (host_status == HOST_CHANGED &&
		    (ip_status != HOST_CHANGED || !key_equal(ip_key, file_key)))
			host_ip_differ = 1;

		key_free(ip_key);
	} else
		ip_status = host_status;

	key_free(file_key);

	switch (host_status) {
	case HOST_OK:
		/* The host is known and the key matches. */
		debug("Host '%.200s' is known and matches the host key.", host);
		if (options.check_host_ip) {
			if (ip_status == HOST_NEW) {
				if (!add_host_to_hostfile(options.user_hostfile, ip, host_key))
					log("Failed to add the host key for IP address '%.30s' to the list of known hosts (%.30s).",
					    ip, options.user_hostfile);
				else
					log("Warning: Permanently added host key for IP address '%.30s' to the list of known hosts.",
					    ip);
			} else if (ip_status != HOST_OK)
				log("Warning: the host key for '%.200s' differs from the key for the IP address '%.30s'",
				    host, ip);
		}
		break;
	case HOST_NEW:
		/* The host is new. */
		if (options.strict_host_key_checking == 1) {
			/* User has requested strict host key checking.  We will not add the host key
			   automatically.  The only alternative left is to abort. */
			fatal("No host key is known for %.200s and you have requested strict checking.", host);
		} else if (options.strict_host_key_checking == 2) {
			/* The default */
			char prompt[1024];
			char *fp = key_fingerprint(host_key);
			snprintf(prompt, sizeof(prompt),
			    "The authenticity of host '%.200s' can't be established.\n"
			    "Key fingerprint is %s.\n"
			    "Are you sure you want to continue connecting (yes/no)? ",
			    host, fp);
			if (!read_yes_or_no(prompt, -1))
				fatal("Aborted by user!\n");
		}
		if (options.check_host_ip && ip_status == HOST_NEW && strcmp(host, ip)) {
			snprintf(hostline, sizeof(hostline), "%s,%s", host, ip);
			hostp = hostline;
		} else
			hostp = host;

		/* If not in strict mode, add the key automatically to the local known_hosts file. */
		if (!add_host_to_hostfile(options.user_hostfile, hostp, host_key))
			log("Failed to add the host to the list of known hosts (%.500s).",
			    options.user_hostfile);
		else
			log("Warning: Permanently added '%.200s' to the list of known hosts.",
			    hostp);
		break;
	case HOST_CHANGED:
		if (options.check_host_ip && host_ip_differ) {
			char *msg;
			if (ip_status == HOST_NEW)
				msg = "is unknown";
			else if (ip_status == HOST_OK)
				msg = "is unchanged";
			else
				msg = "has a different value";
			error("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
			error("@       WARNING: POSSIBLE DNS SPOOFING DETECTED!          @");
			error("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
			error("The host key for %s has changed,", host);
			error("and the key for the according IP address %s", ip);
			error("%s. This could either mean that", msg);
			error("DNS SPOOFING is happening or the IP address for the host");
			error("and its host key have changed at the same time");
		}
		/* The host key has changed. */
		error("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
		error("@    WARNING: REMOTE HOST IDENTIFICATION HAS CHANGED!     @");
		error("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
		error("IT IS POSSIBLE THAT SOMEONE IS DOING SOMETHING NASTY!");
		error("Someone could be eavesdropping on you right now (man-in-the-middle attack)!");
		error("It is also possible that the host key has just been changed.");
		error("Please contact your system administrator.");
		error("Add correct host key in %.100s to get rid of this message.",
		      options.user_hostfile);

		/*
		 * If strict host key checking is in use, the user will have
		 * to edit the key manually and we can only abort.
		 */
		if (options.strict_host_key_checking)
			fatal("Host key for %.200s has changed and you have requested strict checking.", host);

		/*
		 * If strict host key checking has not been requested, allow
		 * the connection but without password authentication or
		 * agent forwarding.
		 */
		if (options.password_authentication) {
			error("Password authentication is disabled to avoid trojan horses.");
			options.password_authentication = 0;
		}
		if (options.forward_agent) {
			error("Agent forwarding is disabled to avoid trojan horses.");
			options.forward_agent = 0;
		}
		/*
		 * XXX Should permit the user to change to use the new id.
		 * This could be done by converting the host key to an
		 * identifying sentence, tell that the host identifies itself
		 * by that sentence, and ask the user if he/she whishes to
		 * accept the authentication.
		 */
		break;
	}
	if (options.check_host_ip)
		xfree(ip);
}
void
check_rsa_host_key(char *host, struct sockaddr *hostaddr, RSA *host_key)
{
	Key k;
	k.type = KEY_RSA;
	k.rsa = host_key;
	check_host_key(host, hostaddr, &k);
}

/*
 * SSH1 key exchange
 */
void
ssh_kex(char *host, struct sockaddr *hostaddr)
{
	int i;
	BIGNUM *key;
	RSA *host_key;
	RSA *public_key;
	int bits, rbits;
	unsigned char session_key[SSH_SESSION_KEY_LENGTH];
	unsigned char cookie[8];
	unsigned int supported_ciphers;
	unsigned int server_flags, client_flags;
	int payload_len, clen, sum_len = 0;
	u_int32_t rand = 0;

	debug("Waiting for server public key.");

	/* Wait for a public key packet from the server. */
	packet_read_expect(&payload_len, SSH_SMSG_PUBLIC_KEY);

	/* Get cookie from the packet. */
	for (i = 0; i < 8; i++)
		cookie[i] = packet_get_char();

	/* Get the public key. */
	public_key = RSA_new();
	bits = packet_get_int();/* bits */
	public_key->e = BN_new();
	packet_get_bignum(public_key->e, &clen);
	sum_len += clen;
	public_key->n = BN_new();
	packet_get_bignum(public_key->n, &clen);
	sum_len += clen;

	rbits = BN_num_bits(public_key->n);
	if (bits != rbits) {
		log("Warning: Server lies about size of server public key: "
		    "actual size is %d bits vs. announced %d.", rbits, bits);
		log("Warning: This may be due to an old implementation of ssh.");
	}
	/* Get the host key. */
	host_key = RSA_new();
	bits = packet_get_int();/* bits */
	host_key->e = BN_new();
	packet_get_bignum(host_key->e, &clen);
	sum_len += clen;
	host_key->n = BN_new();
	packet_get_bignum(host_key->n, &clen);
	sum_len += clen;

	rbits = BN_num_bits(host_key->n);
	if (bits != rbits) {
		log("Warning: Server lies about size of server host key: "
		    "actual size is %d bits vs. announced %d.", rbits, bits);
		log("Warning: This may be due to an old implementation of ssh.");
	}

	/* Get protocol flags. */
	server_flags = packet_get_int();
	packet_set_protocol_flags(server_flags);

	supported_ciphers = packet_get_int();
	supported_authentications = packet_get_int();

	debug("Received server public key (%d bits) and host key (%d bits).",
	      BN_num_bits(public_key->n), BN_num_bits(host_key->n));

	packet_integrity_check(payload_len,
			       8 + 4 + sum_len + 0 + 4 + 0 + 0 + 4 + 4 + 4,
			       SSH_SMSG_PUBLIC_KEY);

	check_rsa_host_key(host, hostaddr, host_key);

	client_flags = SSH_PROTOFLAG_SCREEN_NUMBER | SSH_PROTOFLAG_HOST_IN_FWD_OPEN;

	compute_session_id(session_id, cookie, host_key->n, public_key->n);

	/* Generate a session key. */
	arc4random_stir();

	/*
	 * Generate an encryption key for the session.   The key is a 256 bit
	 * random number, interpreted as a 32-byte key, with the least
	 * significant 8 bits being the first byte of the key.
	 */
	for (i = 0; i < 32; i++) {
		if (i % 4 == 0)
			rand = arc4random();
		session_key[i] = rand & 0xff;
		rand >>= 8;
	}

	/*
	 * According to the protocol spec, the first byte of the session key
	 * is the highest byte of the integer.  The session key is xored with
	 * the first 16 bytes of the session id.
	 */
	key = BN_new();
	BN_set_word(key, 0);
	for (i = 0; i < SSH_SESSION_KEY_LENGTH; i++) {
		BN_lshift(key, key, 8);
		if (i < 16)
			BN_add_word(key, session_key[i] ^ session_id[i]);
		else
			BN_add_word(key, session_key[i]);
	}

	/*
	 * Encrypt the integer using the public key and host key of the
	 * server (key with smaller modulus first).
	 */
	if (BN_cmp(public_key->n, host_key->n) < 0) {
		/* Public key has smaller modulus. */
		if (BN_num_bits(host_key->n) <
		    BN_num_bits(public_key->n) + SSH_KEY_BITS_RESERVED) {
			fatal("respond_to_rsa_challenge: host_key %d < public_key %d + "
			      "SSH_KEY_BITS_RESERVED %d",
			      BN_num_bits(host_key->n),
			      BN_num_bits(public_key->n),
			      SSH_KEY_BITS_RESERVED);
		}
		rsa_public_encrypt(key, key, public_key);
		rsa_public_encrypt(key, key, host_key);
	} else {
		/* Host key has smaller modulus (or they are equal). */
		if (BN_num_bits(public_key->n) <
		    BN_num_bits(host_key->n) + SSH_KEY_BITS_RESERVED) {
			fatal("respond_to_rsa_challenge: public_key %d < host_key %d + "
			      "SSH_KEY_BITS_RESERVED %d",
			      BN_num_bits(public_key->n),
			      BN_num_bits(host_key->n),
			      SSH_KEY_BITS_RESERVED);
		}
		rsa_public_encrypt(key, key, host_key);
		rsa_public_encrypt(key, key, public_key);
	}

	/* Destroy the public keys since we no longer need them. */
	RSA_free(public_key);
	RSA_free(host_key);

	if (options.cipher == SSH_CIPHER_NOT_SET) {
		if (cipher_mask() & supported_ciphers & (1 << ssh_cipher_default))
			options.cipher = ssh_cipher_default;
		else {
			debug("Cipher %s not supported, using %.100s instead.",
			      cipher_name(ssh_cipher_default),
			      cipher_name(SSH_FALLBACK_CIPHER));
			options.cipher = SSH_FALLBACK_CIPHER;
		}
	}
	/* Check that the selected cipher is supported. */
	if (!(supported_ciphers & (1 << options.cipher)))
		fatal("Selected cipher type %.100s not supported by server.",
		      cipher_name(options.cipher));

	debug("Encryption type: %.100s", cipher_name(options.cipher));

	/* Send the encrypted session key to the server. */
	packet_start(SSH_CMSG_SESSION_KEY);
	packet_put_char(options.cipher);

	/* Send the cookie back to the server. */
	for (i = 0; i < 8; i++)
		packet_put_char(cookie[i]);

	/* Send and destroy the encrypted encryption key integer. */
	packet_put_bignum(key);
	BN_clear_free(key);

	/* Send protocol flags. */
	packet_put_int(client_flags);

	/* Send the packet now. */
	packet_send();
	packet_write_wait();

	debug("Sent encrypted session key.");

	/* Set the encryption key. */
	packet_set_encryption_key(session_key, SSH_SESSION_KEY_LENGTH, options.cipher);

	/* We will no longer need the session key here.  Destroy any extra copies. */
	memset(session_key, 0, sizeof(session_key));

	/*
	 * Expect a success message from the server.  Note that this message
	 * will be received in encrypted form.
	 */
	packet_read_expect(&payload_len, SSH_SMSG_SUCCESS);

	debug("Received encrypted confirmation.");
}

/*
 * Authenticate user
 */
void
ssh_userauth(int host_key_valid, RSA *own_host_key,
    uid_t original_real_uid, char *host)
{
	int i, type;
	int payload_len;
	struct passwd *pw;
	const char *server_user, *local_user;

	/* Get local user name.  Use it as server user if no user name was given. */
	pw = getpwuid(original_real_uid);
	if (!pw)
		fatal("User id %d not found from user database.", original_real_uid);
	local_user = xstrdup(pw->pw_name);
	server_user = options.user ? options.user : local_user;

	/* Send the name of the user to log in as on the server. */
	packet_start(SSH_CMSG_USER);
	packet_put_string(server_user, strlen(server_user));
	packet_send();
	packet_write_wait();

	/*
	 * The server should respond with success if no authentication is
	 * needed (the user has no password).  Otherwise the server responds
	 * with failure.
	 */
	type = packet_read(&payload_len);

	/* check whether the connection was accepted without authentication. */
	if (type == SSH_SMSG_SUCCESS)
		return;
	if (type != SSH_SMSG_FAILURE)
		packet_disconnect("Protocol error: got %d in response to SSH_CMSG_USER",
				  type);

#ifdef AFS
	/* Try Kerberos tgt passing if the server supports it. */
	if ((supported_authentications & (1 << SSH_PASS_KERBEROS_TGT)) &&
	    options.kerberos_tgt_passing) {
		if (options.cipher == SSH_CIPHER_NONE)
			log("WARNING: Encryption is disabled! Ticket will be transmitted in the clear!");
		(void) send_kerberos_tgt();
	}
	/* Try AFS token passing if the server supports it. */
	if ((supported_authentications & (1 << SSH_PASS_AFS_TOKEN)) &&
	    options.afs_token_passing && k_hasafs()) {
		if (options.cipher == SSH_CIPHER_NONE)
			log("WARNING: Encryption is disabled! Token will be transmitted in the clear!");
		send_afs_tokens();
	}
#endif /* AFS */

#ifdef KRB4
	if ((supported_authentications & (1 << SSH_AUTH_KERBEROS)) &&
	    options.kerberos_authentication) {
		debug("Trying Kerberos authentication.");
		if (try_kerberos_authentication()) {
			/* The server should respond with success or failure. */
			type = packet_read(&payload_len);
			if (type == SSH_SMSG_SUCCESS)
				return;
			if (type != SSH_SMSG_FAILURE)
				packet_disconnect("Protocol error: got %d in response to Kerberos auth", type);
		}
	}
#endif /* KRB4 */

	/*
	 * Use rhosts authentication if running in privileged socket and we
	 * do not wish to remain anonymous.
	 */
	if ((supported_authentications & (1 << SSH_AUTH_RHOSTS)) &&
	    options.rhosts_authentication) {
		debug("Trying rhosts authentication.");
		packet_start(SSH_CMSG_AUTH_RHOSTS);
		packet_put_string(local_user, strlen(local_user));
		packet_send();
		packet_write_wait();

		/* The server should respond with success or failure. */
		type = packet_read(&payload_len);
		if (type == SSH_SMSG_SUCCESS)
			return;
		if (type != SSH_SMSG_FAILURE)
			packet_disconnect("Protocol error: got %d in response to rhosts auth",
					  type);
	}
	/*
	 * Try .rhosts or /etc/hosts.equiv authentication with RSA host
	 * authentication.
	 */
	if ((supported_authentications & (1 << SSH_AUTH_RHOSTS_RSA)) &&
	    options.rhosts_rsa_authentication && host_key_valid) {
		if (try_rhosts_rsa_authentication(local_user, own_host_key))
			return;
	}
	/* Try RSA authentication if the server supports it. */
	if ((supported_authentications & (1 << SSH_AUTH_RSA)) &&
	    options.rsa_authentication) {
		/*
		 * Try RSA authentication using the authentication agent. The
		 * agent is tried first because no passphrase is needed for
		 * it, whereas identity files may require passphrases.
		 */
		if (try_agent_authentication())
			return;

		/* Try RSA authentication for each identity. */
		for (i = 0; i < options.num_identity_files; i++)
			if (try_rsa_authentication(options.identity_files[i]))
				return;
	}
	/* Try skey authentication if the server supports it. */
	if ((supported_authentications & (1 << SSH_AUTH_TIS)) &&
	    options.skey_authentication && !options.batch_mode) {
		if (try_skey_authentication())
			return;
	}
	/* Try password authentication if the server supports it. */
	if ((supported_authentications & (1 << SSH_AUTH_PASSWORD)) &&
	    options.password_authentication && !options.batch_mode) {
		char prompt[80];

		snprintf(prompt, sizeof(prompt), "%.30s@%.40s's password: ",
		    server_user, host);
		if (try_password_authentication(prompt))
			return;
	}
	/* All authentication methods have failed.  Exit with an error message. */
	fatal("Permission denied.");
	/* NOTREACHED */
}
/*
 * Starts a dialog with the server, and authenticates the current user on the
 * server.  This does not need any extra privileges.  The basic connection
 * to the server must already have been established before this is called.
 * If login fails, this function prints an error and never returns.
 * This function does not require super-user privileges.
 */
void
ssh_login(int host_key_valid, RSA *own_host_key, const char *orighost,
    struct sockaddr *hostaddr, uid_t original_real_uid)
{
	char *host, *cp;

	/* Convert the user-supplied hostname into all lowercase. */
	host = xstrdup(orighost);
	for (cp = host; *cp; cp++)
		if (isupper(*cp))
			*cp = tolower(*cp);

	/* Exchange protocol version identification strings with the server. */
	ssh_exchange_identification();

	/* Put the connection into non-blocking mode. */
	packet_set_nonblocking();

	supported_authentications = 0;
	/* key exchange */
	ssh_kex(host, hostaddr);
	if (supported_authentications == 0)
		fatal("supported_authentications == 0.");

	/* authenticate user */
	ssh_userauth(host_key_valid, own_host_key, original_real_uid, host);
}
