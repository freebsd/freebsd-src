/*
 * Copyright (c) 2001,2002 Damien Miller.  All rights reserved.
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

RCSID("$OpenBSD: sftp.c,v 1.26 2002/02/12 12:32:27 djm Exp $");

/* XXX: short-form remote directory listings (like 'ls -C') */

#include "buffer.h"
#include "xmalloc.h"
#include "log.h"
#include "pathnames.h"
#include "misc.h"

#include "sftp.h"
#include "sftp-common.h"
#include "sftp-client.h"
#include "sftp-int.h"

FILE* infile;
size_t copy_buffer_len = 32768;
size_t num_requests = 16;

static void
connect_to_server(char *path, char **args, int *in, int *out, pid_t *sshpid)
{
	int c_in, c_out;
#ifdef USE_PIPES
	int pin[2], pout[2];
	if ((pipe(pin) == -1) || (pipe(pout) == -1))
		fatal("pipe: %s", strerror(errno));
	*in = pin[0];
	*out = pout[1];
	c_in = pout[0];
	c_out = pin[1];
#else /* USE_PIPES */
	int inout[2];
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, inout) == -1)
		fatal("socketpair: %s", strerror(errno));
	*in = *out = inout[0];
	c_in = c_out = inout[1];
#endif /* USE_PIPES */

	if ((*sshpid = fork()) == -1)
		fatal("fork: %s", strerror(errno));
	else if (*sshpid == 0) {
		if ((dup2(c_in, STDIN_FILENO) == -1) ||
		    (dup2(c_out, STDOUT_FILENO) == -1)) {
			fprintf(stderr, "dup2: %s\n", strerror(errno));
			exit(1);
		}
		close(*in);
		close(*out);
		close(c_in);
		close(c_out);
		execv(path, args);
		fprintf(stderr, "exec: %s: %s\n", path, strerror(errno));
		exit(1);
	}

	close(c_in);
	close(c_out);
}

static void
usage(void)
{
	extern char *__progname;
	
	fprintf(stderr,
	    "usage: %s [-vC1] [-b batchfile] [-o option] [-s subsystem|path] [-B buffer_size]\n"
	    "            [-F config] [-P direct server path] [-S program]\n"
	    "            [user@]host[:file [file]]\n", __progname);
	exit(1);
}

int
main(int argc, char **argv)
{
	int in, out, ch;
	pid_t sshpid;
	char *host, *userhost, *cp, *file2;
	int debug_level = 0, sshver = 2;
	char *file1 = NULL, *sftp_server = NULL;
	char *ssh_program = _PATH_SSH_PROGRAM, *sftp_direct = NULL;
	LogLevel ll = SYSLOG_LEVEL_INFO;
	arglist args;
	extern int optind;
	extern char *optarg;

	args.list = NULL;
	addargs(&args, "ssh");		/* overwritten with ssh_program */
	addargs(&args, "-oFallBackToRsh no");
	addargs(&args, "-oForwardX11 no");
	addargs(&args, "-oForwardAgent no");
	addargs(&args, "-oClearAllForwardings yes");
	ll = SYSLOG_LEVEL_INFO;
	infile = stdin;		/* Read from STDIN unless changed by -b */

	while ((ch = getopt(argc, argv, "1hvCo:s:S:b:B:F:P:R:")) != -1) {
		switch (ch) {
		case 'C':
			addargs(&args, "-C");
			break;
		case 'v':
			if (debug_level < 3) {
				addargs(&args, "-v");
				ll = SYSLOG_LEVEL_DEBUG1 + debug_level;
			}
			debug_level++;
			break;
		case 'F':
		case 'o':
			addargs(&args, "-%c%s", ch, optarg);
			break;
		case '1':
			sshver = 1;
			if (sftp_server == NULL)
				sftp_server = _PATH_SFTP_SERVER;
			break;
		case 's':
			sftp_server = optarg;
			break;
		case 'S':
			ssh_program = optarg;
			break;
		case 'b':
			if (infile == stdin) {
				infile = fopen(optarg, "r");
				if (infile == NULL)
					fatal("%s (%s).", strerror(errno), optarg);
			} else
				fatal("Filename already specified.");
			break;
		case 'P':
			sftp_direct = optarg;
			break;
		case 'B':
			copy_buffer_len = strtol(optarg, &cp, 10);
			if (copy_buffer_len == 0 || *cp != '\0')
				fatal("Invalid buffer size \"%s\"", optarg);
			break;
		case 'R':
			num_requests = strtol(optarg, &cp, 10);
			if (num_requests == 0 || *cp != '\0')
				fatal("Invalid number of requests \"%s\"", 
				    optarg);
			break;
		case 'h':
		default:
			usage();
		}
	}

	if (sftp_direct == NULL) {
		if (optind == argc || argc > (optind + 2))
			usage();

		userhost = xstrdup(argv[optind]);
		file2 = argv[optind+1];

		if ((cp = colon(userhost)) != NULL) {
			*cp++ = '\0';
			file1 = cp;
		}

		if ((host = strchr(userhost, '@')) == NULL)
			host = userhost;
		else {
			*host++ = '\0';
			if (!userhost[0]) {
				fprintf(stderr, "Missing username\n");
				usage();
			}
			addargs(&args, "-l%s",userhost);
		}

		host = cleanhostname(host);
		if (!*host) {
			fprintf(stderr, "Missing hostname\n");
			usage();
		}

		log_init(argv[0], ll, SYSLOG_FACILITY_USER, 1);
		addargs(&args, "-oProtocol %d", sshver);

		/* no subsystem if the server-spec contains a '/' */
		if (sftp_server == NULL || strchr(sftp_server, '/') == NULL)
			addargs(&args, "-s");

		addargs(&args, "%s", host);
		addargs(&args, "%s", (sftp_server != NULL ? 
		    sftp_server : "sftp"));
		args.list[0] = ssh_program;

		fprintf(stderr, "Connecting to %s...\n", host);
		connect_to_server(ssh_program, args.list, &in, &out, 
		    &sshpid);
	} else {
		args.list = NULL;
		addargs(&args, "sftp-server");

		fprintf(stderr, "Attaching to %s...\n", sftp_direct);
		connect_to_server(sftp_direct, args.list, &in, &out, 
		    &sshpid);
	}

	interactive_loop(in, out, file1, file2);

	close(in);
	close(out);
	if (infile != stdin)
		fclose(infile);

	if (waitpid(sshpid, NULL, 0) == -1)
		fatal("Couldn't wait for ssh process: %s", strerror(errno));

	exit(0);
}
