/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * Ssh client program.  This program can be used to log into a remote machine.
 * The software supports strong authentication, encryption, and forwarding
 * of X11, TCP/IP, and authentication connections.
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 *
 * Copyright (c) 1999 Niels Provos.  All rights reserved.
 *
 * Modified to work with SSL by Niels Provos <provos@citi.umich.edu>
 * in Canada (German citizen).
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
RCSID("$OpenBSD: ssh.c,v 1.164 2002/02/14 23:28:00 markus Exp $");
RCSID("$FreeBSD$");

#include <openssl/evp.h>
#include <openssl/err.h>

#include "ssh.h"
#include "ssh1.h"
#include "ssh2.h"
#include "compat.h"
#include "cipher.h"
#include "xmalloc.h"
#include "packet.h"
#include "buffer.h"
#include "uidswap.h"
#include "channels.h"
#include "key.h"
#include "authfd.h"
#include "authfile.h"
#include "pathnames.h"
#include "clientloop.h"
#include "log.h"
#include "readconf.h"
#include "sshconnect.h"
#include "tildexpand.h"
#include "dispatch.h"
#include "misc.h"
#include "kex.h"
#include "mac.h"
#include "sshtty.h"

#ifdef SMARTCARD
#include <openssl/engine.h>
#include "scard.h"
#endif

extern char *__progname;

/* Flag indicating whether IPv4 or IPv6.  This can be set on the command line.
   Default value is AF_UNSPEC means both IPv4 and IPv6. */
extern int IPv4or6;

/* Flag indicating whether debug mode is on.  This can be set on the command line. */
int debug_flag = 0;

/* Flag indicating whether a tty should be allocated */
int tty_flag = 0;
int no_tty_flag = 0;
int force_tty_flag = 0;

/* don't exec a shell */
int no_shell_flag = 0;

/*
 * Flag indicating that nothing should be read from stdin.  This can be set
 * on the command line.
 */
int stdin_null_flag = 0;

/*
 * Flag indicating that ssh should fork after authentication.  This is useful
 * so that the pasphrase can be entered manually, and then ssh goes to the
 * background.
 */
int fork_after_authentication_flag = 0;

/*
 * General data structure for command line options and options configurable
 * in configuration files.  See readconf.h.
 */
Options options;

/* optional user configfile */
char *config = NULL;

/*
 * Name of the host we are connecting to.  This is the name given on the
 * command line, or the HostName specified for the user-supplied name in a
 * configuration file.
 */
char *host;

/* socket address the host resolves to */
struct sockaddr_storage hostaddr;

/* Private host keys. */
struct {
	Key     **keys;
	int	nkeys;
} sensitive_data;

/* Original real UID. */
uid_t original_real_uid;

/* command to be executed */
Buffer command;

/* Should we execute a command or invoke a subsystem? */
int subsystem_flag = 0;

/* Prints a help message to the user.  This function never returns. */

static void
usage(void)
{
	fprintf(stderr, "Usage: %s [options] host [command]\n", __progname);
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "  -l user     Log in using this user name.\n");
	fprintf(stderr, "  -n          Redirect input from " _PATH_DEVNULL ".\n");
	fprintf(stderr, "  -F config   Config file (default: ~/%s).\n",
	     _PATH_SSH_USER_CONFFILE);
	fprintf(stderr, "  -A          Enable authentication agent forwarding.\n");
	fprintf(stderr, "  -a          Disable authentication agent forwarding (default).\n");
#ifdef AFS
	fprintf(stderr, "  -k          Disable Kerberos ticket and AFS token forwarding.\n");
#endif				/* AFS */
	fprintf(stderr, "  -X          Enable X11 connection forwarding.\n");
	fprintf(stderr, "  -x          Disable X11 connection forwarding (default).\n");
	fprintf(stderr, "  -i file     Identity for public key authentication "
	    "(default: ~/.ssh/identity)\n");
#ifdef SMARTCARD
	fprintf(stderr, "  -I reader   Set smartcard reader.\n");
#endif
	fprintf(stderr, "  -t          Tty; allocate a tty even if command is given.\n");
	fprintf(stderr, "  -T          Do not allocate a tty.\n");
	fprintf(stderr, "  -v          Verbose; display verbose debugging messages.\n");
	fprintf(stderr, "              Multiple -v increases verbosity.\n");
	fprintf(stderr, "  -V          Display version number only.\n");
	fprintf(stderr, "  -P          Don't allocate a privileged port.\n");
	fprintf(stderr, "  -q          Quiet; don't display any warning messages.\n");
	fprintf(stderr, "  -f          Fork into background after authentication.\n");
	fprintf(stderr, "  -e char     Set escape character; ``none'' = disable (default: ~).\n");

	fprintf(stderr, "  -c cipher   Select encryption algorithm\n");
	fprintf(stderr, "  -m macs     Specify MAC algorithms for protocol version 2.\n");
	fprintf(stderr, "  -p port     Connect to this port.  Server must be on the same port.\n");
	fprintf(stderr, "  -L listen-port:host:port   Forward local port to remote address\n");
	fprintf(stderr, "  -R listen-port:host:port   Forward remote port to local address\n");
	fprintf(stderr, "              These cause %s to listen for connections on a port, and\n", __progname);
	fprintf(stderr, "              forward them to the other side by connecting to host:port.\n");
	fprintf(stderr, "  -D port     Enable dynamic application-level port forwarding.\n");
	fprintf(stderr, "  -C          Enable compression.\n");
	fprintf(stderr, "  -N          Do not execute a shell or command.\n");
	fprintf(stderr, "  -g          Allow remote hosts to connect to forwarded ports.\n");
	fprintf(stderr, "  -1          Force protocol version 1.\n");
	fprintf(stderr, "  -2          Force protocol version 2.\n");
	fprintf(stderr, "  -4          Use IPv4 only.\n");
	fprintf(stderr, "  -6          Use IPv6 only.\n");
	fprintf(stderr, "  -o 'option' Process the option as if it was read from a configuration file.\n");
	fprintf(stderr, "  -s          Invoke command (mandatory) as SSH2 subsystem.\n");
	fprintf(stderr, "  -b addr     Local IP address.\n");
	exit(1);
}

/*
 * Connects to the given host using rsh (or prints an error message and exits
 * if rsh is not available).  This function never returns.
 */
static void
rsh_connect(char *host, char *user, Buffer * command)
{
	char *args[10];
	int i;

	log("Using rsh.  WARNING: Connection will not be encrypted.");
	/* Build argument list for rsh. */
	i = 0;
#ifndef	_PATH_RSH
#define	_PATH_RSH	"/usr/bin/rsh"
#endif
	args[i++] = _PATH_RSH;
	/* host may have to come after user on some systems */
	args[i++] = host;
	if (user) {
		args[i++] = "-l";
		args[i++] = user;
	}
	if (buffer_len(command) > 0) {
		buffer_append(command, "\0", 1);
		args[i++] = buffer_ptr(command);
	}
	args[i++] = NULL;
	if (debug_flag) {
		for (i = 0; args[i]; i++) {
			if (i != 0)
				fprintf(stderr, " ");
			fprintf(stderr, "%s", args[i]);
		}
		fprintf(stderr, "\n");
	}
	execv(_PATH_RSH, args);
	perror(_PATH_RSH);
	exit(1);
}

static int ssh_session(void);
static int ssh_session2(void);
static void load_public_identity_files(void);

/*
 * Main program for the ssh client.
 */
int
main(int ac, char **av)
{
	int i, opt, exit_status, cerr;
	u_short fwd_port, fwd_host_port;
	char sfwd_port[6], sfwd_host_port[6];
	char *p, *cp, buf[256];
	struct stat st;
	struct passwd *pw;
	int dummy;
	uid_t original_effective_uid;
	extern int optind, optreset;
	extern char *optarg;

	/*
	 * Save the original real uid.  It will be needed later (uid-swapping
	 * may clobber the real uid).
	 */
	original_real_uid = getuid();
	original_effective_uid = geteuid();

	/* If we are installed setuid root be careful to not drop core. */
	if (original_real_uid != original_effective_uid) {
		struct rlimit rlim;
		rlim.rlim_cur = rlim.rlim_max = 0;
		if (setrlimit(RLIMIT_CORE, &rlim) < 0)
			fatal("setrlimit failed: %.100s", strerror(errno));
	}
	/* Get user data. */
	pw = getpwuid(original_real_uid);
	if (!pw) {
		log("unknown user %d", original_real_uid);
		exit(1);
	}
	/* Take a copy of the returned structure. */
	pw = pwcopy(pw);

	/*
	 * Use uid-swapping to give up root privileges for the duration of
	 * option processing.  We will re-instantiate the rights when we are
	 * ready to create the privileged port, and will permanently drop
	 * them when the port has been created (actually, when the connection
	 * has been made, as we may need to create the port several times).
	 */
	temporarily_use_uid(pw);

	/*
	 * Set our umask to something reasonable, as some files are created
	 * with the default umask.  This will make them world-readable but
	 * writable only by the owner, which is ok for all files for which we
	 * don't set the modes explicitly.
	 */
	umask(022);

	/* Initialize option structure to indicate that no values have been set. */
	initialize_options(&options);

	/* Parse command-line arguments. */
	host = NULL;

again:
	while ((opt = getopt(ac, av,
	    "1246ab:c:e:fgi:kl:m:no:p:qstvxACD:F:I:L:NPR:TVX")) != -1) {
		switch (opt) {
		case '1':
			options.protocol = SSH_PROTO_1;
			break;
		case '2':
			options.protocol = SSH_PROTO_2;
			break;
		case '4':
			IPv4or6 = AF_INET;
			break;
		case '6':
			IPv4or6 = AF_INET6;
			break;
		case 'n':
			stdin_null_flag = 1;
			break;
		case 'f':
			fork_after_authentication_flag = 1;
			stdin_null_flag = 1;
			break;
		case 'x':
			options.forward_x11 = 0;
			break;
		case 'X':
			options.forward_x11 = 1;
			break;
		case 'g':
			options.gateway_ports = 1;
			break;
		case 'P':
			options.use_privileged_port = 0;
			break;
		case 'a':
			options.forward_agent = 0;
			break;
		case 'A':
			options.forward_agent = 1;
			break;
#ifdef AFS
		case 'k':
			options.kerberos_tgt_passing = 0;
			options.afs_token_passing = 0;
			break;
#endif
		case 'i':
			if (stat(optarg, &st) < 0) {
				fprintf(stderr, "Warning: Identity file %s "
				    "does not exist.\n", optarg);
				break;
			}
			if (options.num_identity_files >=
			    SSH_MAX_IDENTITY_FILES)
				fatal("Too many identity files specified "
				    "(max %d)", SSH_MAX_IDENTITY_FILES);
			options.identity_files[options.num_identity_files++] =
			    xstrdup(optarg);
			break;
		case 'I':
#ifdef SMARTCARD
			options.smartcard_device = xstrdup(optarg);
#else
			fprintf(stderr, "no support for smartcards.\n");
#endif
			break;
		case 't':
			if (tty_flag)
				force_tty_flag = 1;
			tty_flag = 1;
			break;
		case 'v':
			if (0 == debug_flag) {
				debug_flag = 1;
				options.log_level = SYSLOG_LEVEL_DEBUG1;
			} else if (options.log_level < SYSLOG_LEVEL_DEBUG3) {
				options.log_level++;
				break;
			} else
				fatal("Too high debugging level.");
			/* fallthrough */
		case 'V':
			fprintf(stderr,
			    "%s, SSH protocols %d.%d/%d.%d, OpenSSL 0x%8.8lx\n",
			    SSH_VERSION,
			    PROTOCOL_MAJOR_1, PROTOCOL_MINOR_1,
			    PROTOCOL_MAJOR_2, PROTOCOL_MINOR_2,
			    SSLeay());
			if (opt == 'V')
				exit(0);
			break;
		case 'q':
			options.log_level = SYSLOG_LEVEL_QUIET;
			break;
		case 'e':
			if (optarg[0] == '^' && optarg[2] == 0 &&
			    (u_char) optarg[1] >= 64 &&
			    (u_char) optarg[1] < 128)
				options.escape_char = (u_char) optarg[1] & 31;
			else if (strlen(optarg) == 1)
				options.escape_char = (u_char) optarg[0];
			else if (strcmp(optarg, "none") == 0)
				options.escape_char = SSH_ESCAPECHAR_NONE;
			else {
				fprintf(stderr, "Bad escape character '%s'.\n",
				    optarg);
				exit(1);
			}
			break;
		case 'c':
			if (ciphers_valid(optarg)) {
				/* SSH2 only */
				options.ciphers = xstrdup(optarg);
				options.cipher = SSH_CIPHER_ILLEGAL;
			} else {
				/* SSH1 only */
				options.cipher = cipher_number(optarg);
				if (options.cipher == -1) {
					fprintf(stderr,
					    "Unknown cipher type '%s'\n",
					    optarg);
					exit(1);
				}
				if (options.cipher == SSH_CIPHER_3DES)
					options.ciphers = "3des-cbc";
				else if (options.cipher == SSH_CIPHER_BLOWFISH)
					options.ciphers = "blowfish-cbc";
				else
					options.ciphers = (char *)-1;
			}
			break;
		case 'm':
			if (mac_valid(optarg))
				options.macs = xstrdup(optarg);
			else {
				fprintf(stderr, "Unknown mac type '%s'\n",
				    optarg);
				exit(1);
			}
			break;
		case 'p':
			options.port = a2port(optarg);
			if (options.port == 0) {
				fprintf(stderr, "Bad port '%s'\n", optarg);
				exit(1);
			}
			break;
		case 'l':
			options.user = optarg;
			break;

		case 'L':
		case 'R':
			if (sscanf(optarg, "%5[0-9]:%255[^:]:%5[0-9]",
			    sfwd_port, buf, sfwd_host_port) != 3 &&
			    sscanf(optarg, "%5[0-9]/%255[^/]/%5[0-9]",
			    sfwd_port, buf, sfwd_host_port) != 3) {
				fprintf(stderr,
				    "Bad forwarding specification '%s'\n",
				    optarg);
				usage();
				/* NOTREACHED */
			}
			if ((fwd_port = a2port(sfwd_port)) == 0 ||
	  		    (fwd_host_port = a2port(sfwd_host_port)) == 0) {
				fprintf(stderr,
				    "Bad forwarding port(s) '%s'\n", optarg);
				exit(1);
			}
			if (opt == 'L')
				add_local_forward(&options, fwd_port, buf,
				    fwd_host_port);
			else if (opt == 'R')
				add_remote_forward(&options, fwd_port, buf,
				    fwd_host_port);
			break;

		case 'D':
			fwd_port = a2port(optarg);
			if (fwd_port == 0) {
				fprintf(stderr, "Bad dynamic port '%s'\n",
				    optarg);
				exit(1);
			}
			add_local_forward(&options, fwd_port, "socks4", 0);
			break;

		case 'C':
			options.compression = 1;
			break;
		case 'N':
			no_shell_flag = 1;
			no_tty_flag = 1;
			break;
		case 'T':
			no_tty_flag = 1;
			break;
		case 'o':
			dummy = 1;
			if (process_config_line(&options, host ? host : "",
			    optarg, "command-line", 0, &dummy) != 0)
				exit(1);
			break;
		case 's':
			subsystem_flag = 1;
			break;
		case 'b':
			options.bind_address = optarg;
			break;
		case 'F':
			config = optarg;
			break;
		default:
			usage();
		}
	}

	ac -= optind;
	av += optind;

	if (ac > 0 && !host && **av != '-') {
		if (strchr(*av, '@')) {
			p = xstrdup(*av);
			cp = strchr(p, '@');
			if (cp == NULL || cp == p)
				usage();
			options.user = p;
			*cp = '\0';
			host = ++cp;
		} else
			host = *av;
		ac--, av++;
		if (ac > 0) {
			optind = 0;
			optreset = 1;
			goto again;
		}
	}

	/* Check that we got a host name. */
	if (!host)
		usage();

	SSLeay_add_all_algorithms();
	ERR_load_crypto_strings();
	channel_set_af(IPv4or6);

	/* Initialize the command to execute on remote host. */
	buffer_init(&command);

	/*
	 * Save the command to execute on the remote host in a buffer. There
	 * is no limit on the length of the command, except by the maximum
	 * packet size.  Also sets the tty flag if there is no command.
	 */
	if (!ac) {
		/* No command specified - execute shell on a tty. */
		tty_flag = 1;
		if (subsystem_flag) {
			fprintf(stderr,
			    "You must specify a subsystem to invoke.\n");
			usage();
		}
	} else {
		/* A command has been specified.  Store it into the buffer. */
		for (i = 0; i < ac; i++) {
			if (i)
				buffer_append(&command, " ", 1);
			buffer_append(&command, av[i], strlen(av[i]));
		}
	}

	/* Cannot fork to background if no command. */
	if (fork_after_authentication_flag && buffer_len(&command) == 0 && !no_shell_flag)
		fatal("Cannot fork into background without a command to execute.");

	/* Allocate a tty by default if no command specified. */
	if (buffer_len(&command) == 0)
		tty_flag = 1;

	/* Force no tty*/
	if (no_tty_flag)
		tty_flag = 0;
	/* Do not allocate a tty if stdin is not a tty. */
	if (!isatty(fileno(stdin)) && !force_tty_flag) {
		if (tty_flag)
			log("Pseudo-terminal will not be allocated because stdin is not a terminal.");
		tty_flag = 0;
	}

	/*
	 * Initialize "log" output.  Since we are the client all output
	 * actually goes to stderr.
	 */
	log_init(av[0], options.log_level == -1 ? SYSLOG_LEVEL_INFO : options.log_level,
	    SYSLOG_FACILITY_USER, 1);

	/*
	 * Read per-user configuration file.  Ignore the system wide config
	 * file if the user specifies a config file on the command line.
	 */
	if (config != NULL) {
		if (!read_config_file(config, host, &options))
			fatal("Can't open user config file %.100s: "
			    "%.100s", config, strerror(errno));
	} else  {
		snprintf(buf, sizeof buf, "%.100s/%.100s", pw->pw_dir,
		    _PATH_SSH_USER_CONFFILE);
		(void)read_config_file(buf, host, &options);

		/* Read systemwide configuration file after use config. */
		(void)read_config_file(_PATH_HOST_CONFIG_FILE, host, &options);
	}

	/* Fill configuration defaults. */
	fill_default_options(&options);

	/* reinit */
	log_init(av[0], options.log_level, SYSLOG_FACILITY_USER, 1);

	if (options.user == NULL)
		options.user = xstrdup(pw->pw_name);

	if (options.hostname != NULL)
		host = options.hostname;

	/* Find canonic host name. */
	if (strchr(host, '.') == 0) {
		struct addrinfo hints;
		struct addrinfo *ai = NULL;
		int errgai;
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = IPv4or6;
		hints.ai_flags = AI_CANONNAME;
		hints.ai_socktype = SOCK_STREAM;
		errgai = getaddrinfo(host, NULL, &hints, &ai);
		if (errgai == 0) {
			if (ai->ai_canonname != NULL)
				host = xstrdup(ai->ai_canonname);
			freeaddrinfo(ai);
		}
	}
	/* Disable rhosts authentication if not running as root. */
	if (original_effective_uid != 0 || !options.use_privileged_port) {
		debug("Rhosts Authentication disabled, "
		    "originating port will not be trusted.");
		options.rhosts_authentication = 0;
	}
	/*
	 * If using rsh has been selected, exec it now (without trying
	 * anything else).  Note that we must release privileges first.
	 */
	if (options.use_rsh) {
		/*
		 * Restore our superuser privileges.  This must be done
		 * before permanently setting the uid.
		 */
		restore_uid();

		/* Switch to the original uid permanently. */
		permanently_set_uid(pw);

		/* Execute rsh. */
		rsh_connect(host, options.user, &command);
		fatal("rsh_connect returned");
	}
	/* Restore our superuser privileges. */
	restore_uid();

	/* Open a connection to the remote host. */

	cerr = ssh_connect(host, &hostaddr, options.port, IPv4or6,
	    options.connection_attempts,
	    original_effective_uid != 0 || !options.use_privileged_port,
	    pw, options.proxy_command);

	/*
	 * If we successfully made the connection, load the host private key
	 * in case we will need it later for combined rsa-rhosts
	 * authentication. This must be done before releasing extra
	 * privileges, because the file is only readable by root.
	 */
	sensitive_data.nkeys = 0;
	sensitive_data.keys = NULL;
	if (!cerr && (options.rhosts_rsa_authentication ||
	    options.hostbased_authentication)) {
		sensitive_data.nkeys = 3;
		sensitive_data.keys = xmalloc(sensitive_data.nkeys*sizeof(Key));
		sensitive_data.keys[0] = key_load_private_type(KEY_RSA1,
		    _PATH_HOST_KEY_FILE, "", NULL);
		sensitive_data.keys[1] = key_load_private_type(KEY_DSA,
		    _PATH_HOST_DSA_KEY_FILE, "", NULL);
		sensitive_data.keys[2] = key_load_private_type(KEY_RSA,
		    _PATH_HOST_RSA_KEY_FILE, "", NULL);
	}
	/*
	 * Get rid of any extra privileges that we may have.  We will no
	 * longer need them.  Also, extra privileges could make it very hard
	 * to read identity files and other non-world-readable files from the
	 * user's home directory if it happens to be on a NFS volume where
	 * root is mapped to nobody.
	 */

	/*
	 * Note that some legacy systems need to postpone the following call
	 * to permanently_set_uid() until the private hostkey is destroyed
	 * with RSA_free().  Otherwise the calling user could ptrace() the
	 * process, read the private hostkey and impersonate the host.
	 * OpenBSD does not allow ptracing of setuid processes.
	 */
	permanently_set_uid(pw);

	/*
	 * Now that we are back to our own permissions, create ~/.ssh
	 * directory if it doesn\'t already exist.
	 */
	snprintf(buf, sizeof buf, "%.100s%s%.100s", pw->pw_dir, strcmp(pw->pw_dir, "/") ? "/" : "", _PATH_SSH_USER_DIR);
	if (stat(buf, &st) < 0)
		if (mkdir(buf, 0700) < 0)
			error("Could not create directory '%.200s'.", buf);

	/* Check if the connection failed, and try "rsh" if appropriate. */
	if (cerr) {
		if (!options.fallback_to_rsh)
			exit(1);
		if (options.port != 0)
			log("Secure connection to %.100s on port %hu refused; "
			    "reverting to insecure method",
			    host, options.port);
		else
			log("Secure connection to %.100s refused; "
			    "reverting to insecure method.", host);

		rsh_connect(host, options.user, &command);
		fatal("rsh_connect returned");
	}
	/* load options.identity_files */
	load_public_identity_files();

	/* Expand ~ in known host file names. */
	/* XXX mem-leaks: */
	options.system_hostfile =
	    tilde_expand_filename(options.system_hostfile, original_real_uid);
	options.user_hostfile =
	    tilde_expand_filename(options.user_hostfile, original_real_uid);
	options.system_hostfile2 =
	    tilde_expand_filename(options.system_hostfile2, original_real_uid);
	options.user_hostfile2 =
	    tilde_expand_filename(options.user_hostfile2, original_real_uid);

	signal(SIGPIPE, SIG_IGN); /* ignore SIGPIPE early */

	/* Log into the remote system.  This never returns if the login fails. */
	ssh_login(sensitive_data.keys, sensitive_data.nkeys,
	    host, (struct sockaddr *)&hostaddr, pw);

	/* We no longer need the private host keys.  Clear them now. */
	if (sensitive_data.nkeys != 0) {
		for (i = 0; i < sensitive_data.nkeys; i++) {
			if (sensitive_data.keys[i] != NULL) {
				/* Destroys contents safely */
				debug3("clear hostkey %d", i);
				key_free(sensitive_data.keys[i]);
				sensitive_data.keys[i] = NULL;
			}
		}
		xfree(sensitive_data.keys);
	}
	for (i = 0; i < options.num_identity_files; i++) {
		if (options.identity_files[i]) {
			xfree(options.identity_files[i]);
			options.identity_files[i] = NULL;
		}
		if (options.identity_keys[i]) {
			key_free(options.identity_keys[i]);
			options.identity_keys[i] = NULL;
		}
	}

	exit_status = compat20 ? ssh_session2() : ssh_session();
	packet_close();
	return exit_status;
}

static void
x11_get_proto(char **_proto, char **_data)
{
	char line[512];
	static char proto[512], data[512];
	FILE *f;
	int got_data = 0, i;
	char *display;

	*_proto = proto;
	*_data = data;
	proto[0] = data[0] = '\0';
	if (options.xauth_location && (display = getenv("DISPLAY"))) {
		/* Try to get Xauthority information for the display. */
		if (strncmp(display, "localhost:", 10) == 0)
			/*
			 * Handle FamilyLocal case where $DISPLAY does
			 * not match an authorization entry.  For this we
			 * just try "xauth list unix:displaynum.screennum".
			 * XXX: "localhost" match to determine FamilyLocal
			 *      is not perfect.
			 */
			snprintf(line, sizeof line, "%.100s list unix:%s 2>"
			    _PATH_DEVNULL, options.xauth_location, display+10);
		else
			snprintf(line, sizeof line, "%.100s list %.200s 2>"
			    _PATH_DEVNULL, options.xauth_location, display);
		debug2("x11_get_proto %s", line);
		f = popen(line, "r");
		if (f && fgets(line, sizeof(line), f) &&
		    sscanf(line, "%*s %511s %511s", proto, data) == 2)
			got_data = 1;
		if (f)
			pclose(f);
	}
	/*
	 * If we didn't get authentication data, just make up some
	 * data.  The forwarding code will check the validity of the
	 * response anyway, and substitute this data.  The X11
	 * server, however, will ignore this fake data and use
	 * whatever authentication mechanisms it was using otherwise
	 * for the local connection.
	 */
	if (!got_data) {
		u_int32_t rand = 0;

		strlcpy(proto, "MIT-MAGIC-COOKIE-1", sizeof proto);
		for (i = 0; i < 16; i++) {
			if (i % 4 == 0)
				rand = arc4random();
			snprintf(data + 2 * i, sizeof data - 2 * i, "%02x", rand & 0xff);
			rand >>= 8;
		}
	}
}

static void
ssh_init_forwarding(void)
{
	int success = 0;
	int i;

	/* Initiate local TCP/IP port forwardings. */
	for (i = 0; i < options.num_local_forwards; i++) {
		debug("Connections to local port %d forwarded to remote address %.200s:%d",
		    options.local_forwards[i].port,
		    options.local_forwards[i].host,
		    options.local_forwards[i].host_port);
		success += channel_setup_local_fwd_listener(
		    options.local_forwards[i].port,
		    options.local_forwards[i].host,
		    options.local_forwards[i].host_port,
		    options.gateway_ports);
	}
	if (i > 0 && success == 0)
		error("Could not request local forwarding.");

	/* Initiate remote TCP/IP port forwardings. */
	for (i = 0; i < options.num_remote_forwards; i++) {
		debug("Connections to remote port %d forwarded to local address %.200s:%d",
		    options.remote_forwards[i].port,
		    options.remote_forwards[i].host,
		    options.remote_forwards[i].host_port);
		channel_request_remote_forwarding(
		    options.remote_forwards[i].port,
		    options.remote_forwards[i].host,
		    options.remote_forwards[i].host_port);
	}
}

static void
check_agent_present(void)
{
	if (options.forward_agent) {
		/* Clear agent forwarding if we don\'t have an agent. */
		int authfd = ssh_get_authentication_socket();
		if (authfd < 0)
			options.forward_agent = 0;
		else
			ssh_close_authentication_socket(authfd);
	}
}

static int
ssh_session(void)
{
	int type;
	int interactive = 0;
	int have_tty = 0;
	struct winsize ws;
	char *cp;

	/* Enable compression if requested. */
	if (options.compression) {
		debug("Requesting compression at level %d.", options.compression_level);

		if (options.compression_level < 1 || options.compression_level > 9)
			fatal("Compression level must be from 1 (fast) to 9 (slow, best).");

		/* Send the request. */
		packet_start(SSH_CMSG_REQUEST_COMPRESSION);
		packet_put_int(options.compression_level);
		packet_send();
		packet_write_wait();
		type = packet_read();
		if (type == SSH_SMSG_SUCCESS)
			packet_start_compression(options.compression_level);
		else if (type == SSH_SMSG_FAILURE)
			log("Warning: Remote host refused compression.");
		else
			packet_disconnect("Protocol error waiting for compression response.");
	}
	/* Allocate a pseudo tty if appropriate. */
	if (tty_flag) {
		debug("Requesting pty.");

		/* Start the packet. */
		packet_start(SSH_CMSG_REQUEST_PTY);

		/* Store TERM in the packet.  There is no limit on the
		   length of the string. */
		cp = getenv("TERM");
		if (!cp)
			cp = "";
		packet_put_cstring(cp);

		/* Store window size in the packet. */
		if (ioctl(fileno(stdin), TIOCGWINSZ, &ws) < 0)
			memset(&ws, 0, sizeof(ws));
		packet_put_int(ws.ws_row);
		packet_put_int(ws.ws_col);
		packet_put_int(ws.ws_xpixel);
		packet_put_int(ws.ws_ypixel);

		/* Store tty modes in the packet. */
		tty_make_modes(fileno(stdin), NULL);

		/* Send the packet, and wait for it to leave. */
		packet_send();
		packet_write_wait();

		/* Read response from the server. */
		type = packet_read();
		if (type == SSH_SMSG_SUCCESS) {
			interactive = 1;
			have_tty = 1;
		} else if (type == SSH_SMSG_FAILURE)
			log("Warning: Remote host failed or refused to allocate a pseudo tty.");
		else
			packet_disconnect("Protocol error waiting for pty request response.");
	}
	/* Request X11 forwarding if enabled and DISPLAY is set. */
	if (options.forward_x11 && getenv("DISPLAY") != NULL) {
		char *proto, *data;
		/* Get reasonable local authentication information. */
		x11_get_proto(&proto, &data);
		/* Request forwarding with authentication spoofing. */
		debug("Requesting X11 forwarding with authentication spoofing.");
		x11_request_forwarding_with_spoofing(0, proto, data);

		/* Read response from the server. */
		type = packet_read();
		if (type == SSH_SMSG_SUCCESS) {
			interactive = 1;
		} else if (type == SSH_SMSG_FAILURE) {
			log("Warning: Remote host denied X11 forwarding.");
		} else {
			packet_disconnect("Protocol error waiting for X11 forwarding");
		}
	}
	/* Tell the packet module whether this is an interactive session. */
	packet_set_interactive(interactive);

	/* Request authentication agent forwarding if appropriate. */
	check_agent_present();

	if (options.forward_agent) {
		debug("Requesting authentication agent forwarding.");
		auth_request_forwarding();

		/* Read response from the server. */
		type = packet_read();
		packet_check_eom();
		if (type != SSH_SMSG_SUCCESS)
			log("Warning: Remote host denied authentication agent forwarding.");
	}

	/* Initiate port forwardings. */
	ssh_init_forwarding();

	/* If requested, let ssh continue in the background. */
	if (fork_after_authentication_flag)
		if (daemon(1, 1) < 0)
			fatal("daemon() failed: %.200s", strerror(errno));

	/*
	 * If a command was specified on the command line, execute the
	 * command now. Otherwise request the server to start a shell.
	 */
	if (buffer_len(&command) > 0) {
		int len = buffer_len(&command);
		if (len > 900)
			len = 900;
		debug("Sending command: %.*s", len, (u_char *)buffer_ptr(&command));
		packet_start(SSH_CMSG_EXEC_CMD);
		packet_put_string(buffer_ptr(&command), buffer_len(&command));
		packet_send();
		packet_write_wait();
	} else {
		debug("Requesting shell.");
		packet_start(SSH_CMSG_EXEC_SHELL);
		packet_send();
		packet_write_wait();
	}

	/* Enter the interactive session. */
	return client_loop(have_tty, tty_flag ?
	    options.escape_char : SSH_ESCAPECHAR_NONE, 0);
}

static void
client_subsystem_reply(int type, u_int32_t seq, void *ctxt)
{
	int id, len;

	id = packet_get_int();
	len = buffer_len(&command);
	if (len > 900)
		len = 900;
	packet_check_eom();
	if (type == SSH2_MSG_CHANNEL_FAILURE)
		fatal("Request for subsystem '%.*s' failed on channel %d",
		    len, (u_char *)buffer_ptr(&command), id);
}

/* request pty/x11/agent/tcpfwd/shell for channel */
static void
ssh_session2_setup(int id, void *arg)
{
	int len;
	int interactive = 0;
	struct termios tio;

	debug("ssh_session2_setup: id %d", id);

	if (tty_flag) {
		struct winsize ws;
		char *cp;
		cp = getenv("TERM");
		if (!cp)
			cp = "";
		/* Store window size in the packet. */
		if (ioctl(fileno(stdin), TIOCGWINSZ, &ws) < 0)
			memset(&ws, 0, sizeof(ws));

		channel_request_start(id, "pty-req", 0);
		packet_put_cstring(cp);
		packet_put_int(ws.ws_col);
		packet_put_int(ws.ws_row);
		packet_put_int(ws.ws_xpixel);
		packet_put_int(ws.ws_ypixel);
		tio = get_saved_tio();
		tty_make_modes(/*ignored*/ 0, &tio);
		packet_send();
		interactive = 1;
		/* XXX wait for reply */
	}
	if (options.forward_x11 &&
	    getenv("DISPLAY") != NULL) {
		char *proto, *data;
		/* Get reasonable local authentication information. */
		x11_get_proto(&proto, &data);
		/* Request forwarding with authentication spoofing. */
		debug("Requesting X11 forwarding with authentication spoofing.");
		x11_request_forwarding_with_spoofing(id, proto, data);
		interactive = 1;
		/* XXX wait for reply */
	}

	check_agent_present();
	if (options.forward_agent) {
		debug("Requesting authentication agent forwarding.");
		channel_request_start(id, "auth-agent-req@openssh.com", 0);
		packet_send();
	}

	len = buffer_len(&command);
	if (len > 0) {
		if (len > 900)
			len = 900;
		if (subsystem_flag) {
			debug("Sending subsystem: %.*s", len, (u_char *)buffer_ptr(&command));
			channel_request_start(id, "subsystem", /*want reply*/ 1);
			/* register callback for reply */
			/* XXX we asume that client_loop has already been called */
			dispatch_set(SSH2_MSG_CHANNEL_FAILURE, &client_subsystem_reply);
			dispatch_set(SSH2_MSG_CHANNEL_SUCCESS, &client_subsystem_reply);
		} else {
			debug("Sending command: %.*s", len, (u_char *)buffer_ptr(&command));
			channel_request_start(id, "exec", 0);
		}
		packet_put_string(buffer_ptr(&command), buffer_len(&command));
		packet_send();
	} else {
		channel_request_start(id, "shell", 0);
		packet_send();
	}

	packet_set_interactive(interactive);
}

/* open new channel for a session */
static int
ssh_session2_open(void)
{
	Channel *c;
	int window, packetmax, in, out, err;

	if (stdin_null_flag) {
		in = open(_PATH_DEVNULL, O_RDONLY);
	} else {
		in = dup(STDIN_FILENO);
	}
	out = dup(STDOUT_FILENO);
	err = dup(STDERR_FILENO);

	if (in < 0 || out < 0 || err < 0)
		fatal("dup() in/out/err failed");

	/* enable nonblocking unless tty */
	if (!isatty(in))
		set_nonblock(in);
	if (!isatty(out))
		set_nonblock(out);
	if (!isatty(err))
		set_nonblock(err);

	window = CHAN_SES_WINDOW_DEFAULT;
	packetmax = CHAN_SES_PACKET_DEFAULT;
	if (tty_flag) {
		window >>= 1;
		packetmax >>= 1;
	}
	c = channel_new(
	    "session", SSH_CHANNEL_OPENING, in, out, err,
	    window, packetmax, CHAN_EXTENDED_WRITE,
	    xstrdup("client-session"), /*nonblock*/0);

	debug3("ssh_session2_open: channel_new: %d", c->self);

	channel_send_open(c->self);
	if (!no_shell_flag)
		channel_register_confirm(c->self, ssh_session2_setup);

	return c->self;
}

static int
ssh_session2(void)
{
	int id = -1;

	/* XXX should be pre-session */
	ssh_init_forwarding();

	if (!no_shell_flag || (datafellows & SSH_BUG_DUMMYCHAN))
		id = ssh_session2_open();

	/* If requested, let ssh continue in the background. */
	if (fork_after_authentication_flag)
		if (daemon(1, 1) < 0)
			fatal("daemon() failed: %.200s", strerror(errno));

	return client_loop(tty_flag, tty_flag ?
	    options.escape_char : SSH_ESCAPECHAR_NONE, id);
}

static void
load_public_identity_files(void)
{
	char *filename;
	Key *public;
	int i = 0;

#ifdef SMARTCARD
	if (options.smartcard_device != NULL &&
	    options.num_identity_files + 1 < SSH_MAX_IDENTITY_FILES &&
	    (public = sc_get_key(options.smartcard_device)) != NULL ) {
		Key *new;

		if (options.num_identity_files + 2 > SSH_MAX_IDENTITY_FILES)
			options.num_identity_files = SSH_MAX_IDENTITY_FILES - 2;
		memmove(&options.identity_files[2], &options.identity_files[0],
		    sizeof(char *) * options.num_identity_files);
		options.num_identity_files += 2;
		i = 2;

		/* XXX ssh1 vs ssh2 */
		new = key_new(KEY_RSA);
		new->flags = KEY_FLAG_EXT;
		BN_copy(new->rsa->n, public->rsa->n);
		BN_copy(new->rsa->e, public->rsa->e);
		RSA_set_method(new->rsa, sc_get_engine());
		options.identity_keys[0] = new;
		options.identity_files[0] = xstrdup("smartcard rsa key");;

		new = key_new(KEY_RSA1);
		new->flags = KEY_FLAG_EXT;
		BN_copy(new->rsa->n, public->rsa->n);
		BN_copy(new->rsa->e, public->rsa->e);
		RSA_set_method(new->rsa, sc_get_engine());
		options.identity_keys[1] = new;
		options.identity_files[1] = xstrdup("smartcard rsa1 key");

		key_free(public);
	}
#endif /* SMARTCARD */
	for (; i < options.num_identity_files; i++) {
		filename = tilde_expand_filename(options.identity_files[i],
		    original_real_uid);
		public = key_load_public(filename, NULL);
		debug("identity file %s type %d", filename,
		    public ? public->type : -1);
		xfree(options.identity_files[i]);
		options.identity_files[i] = filename;
		options.identity_keys[i] = public;
	}
}
