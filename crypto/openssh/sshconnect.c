/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * Code to connect to a remote host, and to perform the client side of the
 * login (authentication) dialog.
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 */

#include "includes.h"
RCSID("$OpenBSD: sshconnect.c,v 1.79 2000/09/17 15:52:51 markus Exp $");
RCSID("$FreeBSD$");

#include <openssl/bn.h>
#include <openssl/dsa.h>
#include <openssl/rsa.h>

#include "xmalloc.h"
#include "rsa.h"
#include "ssh.h"
#include "buffer.h"
#include "packet.h"
#include "uidswap.h"
#include "compat.h"
#include "readconf.h"
#include "key.h"
#include "sshconnect.h"
#include "hostfile.h"

char *client_version_string = NULL;
char *server_version_string = NULL;

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
	pid_t pid;
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
 * The canonical host name used to connect will be returned in *host.
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
ssh_connect(char **host, struct sockaddr_storage * hostaddr,
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

	debug("ssh_connect: getuid %u geteuid %u anon %d",
	      (u_int) getuid(), (u_int) geteuid(), anonymous);

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
		return ssh_proxy_connect(*host, port, original_real_uid, proxy_command);

	/* No proxy command. */

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = IPv4or6;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_CANONNAME;
	snprintf(strport, sizeof strport, "%d", port);
	if ((gaierr = getaddrinfo(*host, strport, &hints, &aitop)) != 0)
		fatal("%s: %.100s: %s", __progname, *host,
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
				ai->ai_canonname, ntop, strport);

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
				memcpy(hostaddr, ai->ai_addr, ai->ai_addrlen); 
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
		if (ai) {
#if 0
			if (ai->ai_canonname != NULL)
				*host = xstrdup(ai->ai_canonname);
#endif
			break;	/* Successful connection. */
		}

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
 * Waits for the server identification string, and sends our own
 * identification string.
 */
void
ssh_exchange_identification()
{
	char buf[256], remote_version[256];	/* must be same size! */
	int remote_major, remote_minor, i, mismatch;
	int connection_in = packet_get_connection_in();
	int connection_out = packet_get_connection_out();

	/* Read other side\'s version identification. */
	for (;;) {
		for (i = 0; i < sizeof(buf) - 1; i++) {
			int len = atomicio(read, connection_in, &buf[i], 1);
			if (len < 0)
				fatal("ssh_exchange_identification: read: %.100s", strerror(errno));
			if (len != 1)
				fatal("ssh_exchange_identification: Connection closed by remote host");
			if (buf[i] == '\r') {
				buf[i] = '\n';
				buf[i + 1] = 0;
				continue;		/**XXX wait for \n */
			}
			if (buf[i] == '\n') {
				buf[i + 1] = 0;
				break;
			}
		}
		buf[sizeof(buf) - 1] = 0;
		if (strncmp(buf, "SSH-", 4) == 0)
			break;
		debug("ssh_exchange_identification: %s", buf);
	}
	server_version_string = xstrdup(buf);

	/*
	 * Check that the versions match.  In future this might accept
	 * several versions and set appropriate flags to handle them.
	 */
	if (sscanf(server_version_string, "SSH-%d.%d-%[^\n]\n",
	    &remote_major, &remote_minor, remote_version) != 3)
		fatal("Bad remote protocol version identification: '%.100s'", buf);
	debug("Remote protocol version %d.%d, remote software version %.100s",
	      remote_major, remote_minor, remote_version);

	compat_datafellows(remote_version);
	mismatch = 0;

	switch(remote_major) {
	case 1:
		if (remote_minor == 99 &&
		    (options.protocol & SSH_PROTO_2) &&
		    !(options.protocol & SSH_PROTO_1_PREFERRED)) {
			enable_compat20();
			break;
		}
		if (!(options.protocol & SSH_PROTO_1)) {
			mismatch = 1;
			break;
		}
		if (remote_minor < 3) {
			fatal("Remote machine has too old SSH software version.");
		} else if (remote_minor == 3) {
			/* We speak 1.3, too. */
			enable_compat13();
			if (options.forward_agent) {
				log("Agent forwarding disabled for protocol 1.3");
				options.forward_agent = 0;
			}
		}
		break;
	case 2:
		if (options.protocol & SSH_PROTO_2) {
			enable_compat20();
			break;
		}
		/* FALLTHROUGH */
	default:
		mismatch = 1;
		break;
	}
	if (mismatch)
		fatal("Protocol major versions differ: %d vs. %d",
		    (options.protocol & SSH_PROTO_2) ? PROTOCOL_MAJOR_2 : PROTOCOL_MAJOR_1,
		    remote_major);
	if (compat20)
		packet_set_ssh2_format();
	/* Send our own protocol version identification. */
	snprintf(buf, sizeof buf, "SSH-%d.%d-%.100s\n",
	    compat20 ? PROTOCOL_MAJOR_2 : PROTOCOL_MAJOR_1,
	    compat20 ? PROTOCOL_MINOR_2 : PROTOCOL_MINOR_1,
	    SSH_VERSION);
	if (atomicio(write, connection_out, buf, strlen(buf)) != strlen(buf))
		fatal("write: %.100s", strerror(errno));
	client_version_string = xstrdup(buf);
	chop(client_version_string);
	chop(server_version_string);
	debug("Local version string %.100s", client_version_string);
}

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
		else if (strcmp(buf, "no") == 0)
			retval = 0;
		else
			fprintf(stderr, "Please type 'yes' or 'no'.\n");

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
check_host_key(char *host, struct sockaddr *hostaddr, Key *host_key,
	const char *user_hostfile, const char *system_hostfile)
{
	Key *file_key;
	char *type = key_type(host_key);
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
	/**  hostaddr == 0! */
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
	host_status = check_host_in_hostfile(user_hostfile, host, host_key, file_key);
	if (host_status == HOST_NEW)
		host_status = check_host_in_hostfile(system_hostfile, host, host_key, file_key);
	/*
	 * Also perform check for the ip address, skip the check if we are
	 * localhost or the hostname was an ip address to begin with
	 */
	if (options.check_host_ip && !local && strcmp(host, ip)) {
		Key *ip_key = key_new(host_key->type);
		ip_status = check_host_in_hostfile(user_hostfile, ip, host_key, ip_key);

		if (ip_status == HOST_NEW)
			ip_status = check_host_in_hostfile(system_hostfile, ip, host_key, ip_key);
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
		debug("Host '%.200s' is known and matches the %s host key.",
		    host, type);
		if (options.check_host_ip) {
			if (ip_status == HOST_NEW) {
				if (!add_host_to_hostfile(user_hostfile, ip, host_key))
					log("Failed to add the %s host key for IP address '%.30s' to the list of known hosts (%.30s).",
					    type, ip, user_hostfile);
				else
					log("Warning: Permanently added the %s host key for IP address '%.30s' to the list of known hosts.",
					    type, ip);
			} else if (ip_status != HOST_OK)
				log("Warning: the %s host key for '%.200s' differs from the key for the IP address '%.30s'",
				    type, host, ip);
		}
		break;
	case HOST_NEW:
		/* The host is new. */
		if (options.strict_host_key_checking == 1) {
			/* User has requested strict host key checking.  We will not add the host key
			   automatically.  The only alternative left is to abort. */
			fatal("No %s host key is known for %.200s and you have requested strict checking.", type, host);
		} else if (options.strict_host_key_checking == 2) {
			/* The default */
			char prompt[1024];
			char *fp = key_fingerprint(host_key);
			snprintf(prompt, sizeof(prompt),
			    "The authenticity of host '%.200s' can't be established.\n"
			    "%s key fingerprint is %s.\n"
			    "Are you sure you want to continue connecting (yes/no)? ",
			    host, type, fp);
			if (!read_yes_or_no(prompt, -1))
				fatal("Aborted by user!\n");
		}
		if (options.check_host_ip && ip_status == HOST_NEW && strcmp(host, ip)) {
			snprintf(hostline, sizeof(hostline), "%s,%s", host, ip);
			hostp = hostline;
		} else
			hostp = host;

		/* If not in strict mode, add the key automatically to the local known_hosts file. */
		if (!add_host_to_hostfile(user_hostfile, hostp, host_key))
			log("Failed to add the host to the list of known hosts (%.500s).",
			    user_hostfile);
		else
			log("Warning: Permanently added '%.200s' (%s) to the list of known hosts.",
			    hostp, type);
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
			error("The %s host key for %s has changed,", type, host);
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
		error("It is also possible that the %s host key has just been changed.", type);
		error("Please contact your system administrator.");
		error("Add correct host key in %.100s to get rid of this message.",
		      user_hostfile);

		/*
		 * If strict host key checking is in use, the user will have
		 * to edit the key manually and we can only abort.
		 */
		if (options.strict_host_key_checking)
			fatal("%s host key for %.200s has changed and you have requested strict checking.", type, host);

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

#ifdef KRB5
int
try_krb5_authentication(krb5_context *context, krb5_auth_context *auth_context)
{
  krb5_error_code problem;
  const char *tkfile; 
  struct stat buf;
  krb5_ccache ccache = NULL;
  const char *remotehost;
  krb5_data ap;
  int type, payload_len;
  krb5_ap_rep_enc_part *reply = NULL; 
  int ret;

  memset(&ap, 0, sizeof(ap));
  
  problem = krb5_init_context(context);
  if (problem) {
     ret = 0;
     goto out;
  }
  
  tkfile = krb5_cc_default_name(*context);
  if (strncmp(tkfile, "FILE:", 5) == 0)
     tkfile += 5;
  
  if (stat(tkfile, &buf) == 0 && getuid() != buf.st_uid) {
    debug("Kerberos V5: could not get default ccache (permission denied).");
    ret = 0;
    goto out;
  }
  
  problem = krb5_cc_default(*context, &ccache);
  if (problem) { 
     ret = 0; 
     goto out;
  }
  
  remotehost = get_canonical_hostname();
  
  problem = krb5_mk_req(*context, auth_context, AP_OPTS_MUTUAL_REQUIRED,
			"host", remotehost, NULL, ccache, &ap);
  if (problem) { 
     ret = 0;
     goto out;
  }
  
  packet_start(SSH_CMSG_AUTH_KERBEROS);
  packet_put_string((char *) ap.data, ap.length);
  packet_send();
  packet_write_wait();

  xfree(ap.data);
  ap.length = 0;

  type = packet_read(&payload_len);
   switch (type) {
        case SSH_SMSG_FAILURE:
                /* Should really be SSH_SMSG_AUTH_KERBEROS_FAILURE */
                debug("Kerberos V5 authentication failed.");
                ret = 0;
                break;

         case SSH_SMSG_AUTH_KERBEROS_RESPONSE:
                /* SSH_SMSG_AUTH_KERBEROS_SUCCESS */
                debug("Kerberos V5 authentication accepted.");

                /* Get server's response. */
                ap.data = packet_get_string((unsigned int *) &ap.length);

                packet_integrity_check(payload_len, 4 + ap.length, type);
                /* XXX je to dobre? */

                problem = krb5_rd_rep(*context, *auth_context, &ap, &reply);
                if (problem) { 
		   ret = 0;
		   goto out;
                } 
		ret = 1;
		break;
	
	default:
		packet_disconnect("Protocol error on Kerberos V5 response: %d", type);
		ret = 0; 
		break;

   }
 
out:  
   if (ccache != NULL) 
       krb5_cc_close(*context, ccache);
   if (reply != NULL)
      krb5_free_ap_rep_enc_part(*context, reply); 
   if (ap.length > 0)
      krb5_data_free(&ap);
	
   return ret;
  
}

void
send_krb5_tgt(krb5_context context, krb5_auth_context auth_context) 
{
  int fd; 
  int type, payload_len;
  krb5_error_code problem; 
  krb5_data outbuf;
  krb5_ccache ccache = NULL;
  krb5_creds creds; 
  krb5_kdc_flags flags; 
  const char* remotehost = get_canonical_hostname(); 
 
  memset(&creds, 0, sizeof(creds)); 
  memset(&outbuf, 0, sizeof(outbuf));
  
  fd = packet_get_connection_in(); 
  problem = krb5_auth_con_setaddrs_from_fd(context, auth_context, &fd);
  if (problem) {
     goto out;
  }
  
#if 0
  tkfile = krb5_cc_default_name(context);
  if (strncmp(tkfile, "FILE:", 5) == 0)
     tkfile += 5;
  
  if (stat(tkfile, &buf) == 0 && getuid() != buf.st_uid) {
     debug("Kerberos V5: could not get default ccache (permission denied).");
     goto out;
  }
#endif
  
  problem = krb5_cc_default(context, &ccache);  
  if (problem) {
     goto out;
  }
  
  problem = krb5_cc_get_principal(context, ccache, &creds.client);
  if (problem) {
     goto out;
  }
  
  problem = krb5_build_principal(context, &creds.server,
	                         strlen(creds.client->realm),
				 creds.client->realm,
				 "krbtgt",
				 creds.client->realm,
				 NULL);
  if (problem) {
     goto out;
  }
  
  creds.times.endtime = 0;
  
  flags.i = 0;
  flags.b.forwarded = 1;
  flags.b.forwardable = krb5_config_get_bool(context,  NULL,
	                  "libdefaults", "forwardable", NULL);
  
  problem = krb5_get_forwarded_creds (context,
	                              auth_context,
				      ccache,
				      flags.i,
				      remotehost,
				      &creds,
				      &outbuf);
  if (problem) {
     goto out;
  }
  
  packet_start(SSH_CMSG_HAVE_KERBEROS_TGT);
  packet_put_string((char *)outbuf.data, outbuf.length);
  packet_send();
  packet_write_wait();
  
  type = packet_read(&payload_len);
  switch (type) {
     case SSH_SMSG_SUCCESS:
	break;
     case SSH_SMSG_FAILURE:
	break;
     default:
	break;
  }

out:
  if (creds.client)
     krb5_free_principal(context, creds.client);
  if (creds.server)
     krb5_free_principal(context, creds.server);
  if (ccache)
     krb5_cc_close(context, ccache); 
  if (outbuf.data)
     xfree(outbuf.data);
  
  return;
}
#endif /* KRB5 */

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
	struct passwd *pw;
	char *host, *cp;
	char *server_user, *local_user;

	/* Get local user name.  Use it as server user if no user name was given. */
	pw = getpwuid(original_real_uid);
	if (!pw)
		fatal("User id %u not found from user database.", original_real_uid);
	local_user = xstrdup(pw->pw_name);
	server_user = options.user ? options.user : local_user;

	/* Convert the user-supplied hostname into all lowercase. */
	host = xstrdup(orighost);
	for (cp = host; *cp; cp++)
		if (isupper(*cp))
			*cp = tolower(*cp);

	/* Exchange protocol version identification strings with the server. */
	ssh_exchange_identification();

	/* Put the connection into non-blocking mode. */
	packet_set_nonblocking();

	/* key exchange */
	/* authenticate user */
	if (compat20) {
		ssh_kex2(host, hostaddr);
		ssh_userauth2(server_user, host);
	} else {
		ssh_kex(host, hostaddr);
		ssh_userauth(local_user, server_user, host, host_key_valid, own_host_key);
	}
}

void
ssh_put_password(char *password)
{
	int size;
	char *padded;

	size = roundup(strlen(password) + 1, 32);
	padded = xmalloc(size);
	memset(padded, 0, size);
	strlcpy(padded, password, size);
	packet_put_string(padded, size);
	memset(padded, 0, size);
	xfree(padded);
}
