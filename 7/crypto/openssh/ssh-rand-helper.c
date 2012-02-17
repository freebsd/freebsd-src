/*
 * Copyright (c) 2001-2002 Damien Miller.  All rights reserved.
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

#include <sys/types.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>

#include <stdarg.h>
#include <stddef.h>
#include <string.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#ifdef HAVE_SYS_UN_H
# include <sys/un.h>
#endif

#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>

#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/crypto.h>

/* SunOS 4.4.4 needs this */
#ifdef HAVE_FLOATINGPOINT_H
# include <floatingpoint.h>
#endif /* HAVE_FLOATINGPOINT_H */

#include "misc.h"
#include "xmalloc.h"
#include "atomicio.h"
#include "pathnames.h"
#include "log.h"

/* Number of bytes we write out */
#define OUTPUT_SEED_SIZE	48

/* Length of on-disk seedfiles */
#define SEED_FILE_SIZE		1024

/* Maximum number of command-line arguments to read from file */
#define NUM_ARGS		10

/* Minimum number of usable commands to be considered sufficient */
#define MIN_ENTROPY_SOURCES	16

/* Path to on-disk seed file (relative to user's home directory */
#ifndef SSH_PRNG_SEED_FILE
# define SSH_PRNG_SEED_FILE      _PATH_SSH_USER_DIR"/prng_seed"
#endif

/* Path to PRNG commands list */
#ifndef SSH_PRNG_COMMAND_FILE
# define SSH_PRNG_COMMAND_FILE   SSHDIR "/ssh_prng_cmds"
#endif

extern char *__progname;

#define WHITESPACE " \t\n"

#ifndef RUSAGE_SELF
# define RUSAGE_SELF 0
#endif
#ifndef RUSAGE_CHILDREN
# define RUSAGE_CHILDREN 0
#endif

#if !defined(PRNGD_SOCKET) && !defined(PRNGD_PORT)
# define USE_SEED_FILES
#endif

typedef struct {
	/* Proportion of data that is entropy */
	double rate;
	/* Counter goes positive if this command times out */
	unsigned int badness;
	/* Increases by factor of two each timeout */
	unsigned int sticky_badness;
	/* Path to executable */
	char *path;
	/* argv to pass to executable */
	char *args[NUM_ARGS]; /* XXX: arbitrary limit */
	/* full command string (debug) */
	char *cmdstring;
} entropy_cmd_t;

/* slow command timeouts (all in milliseconds) */
/* static int entropy_timeout_default = ENTROPY_TIMEOUT_MSEC; */
static int entropy_timeout_current = ENTROPY_TIMEOUT_MSEC;

/* this is initialised from a file, by prng_read_commands() */
static entropy_cmd_t *entropy_cmds = NULL;

/* Prototypes */
double stir_from_system(void);
double stir_from_programs(void);
double stir_gettimeofday(double entropy_estimate);
double stir_clock(double entropy_estimate);
double stir_rusage(int who, double entropy_estimate);
double hash_command_output(entropy_cmd_t *src, unsigned char *hash);
int get_random_bytes_prngd(unsigned char *buf, int len,
    unsigned short tcp_port, char *socket_path);

/*
 * Collect 'len' bytes of entropy into 'buf' from PRNGD/EGD daemon
 * listening either on 'tcp_port', or via Unix domain socket at *
 * 'socket_path'.
 * Either a non-zero tcp_port or a non-null socket_path must be
 * supplied.
 * Returns 0 on success, -1 on error
 */
int
get_random_bytes_prngd(unsigned char *buf, int len,
    unsigned short tcp_port, char *socket_path)
{
	int fd, addr_len, rval, errors;
	u_char msg[2];
	struct sockaddr_storage addr;
	struct sockaddr_in *addr_in = (struct sockaddr_in *)&addr;
	struct sockaddr_un *addr_un = (struct sockaddr_un *)&addr;
	mysig_t old_sigpipe;

	/* Sanity checks */
	if (socket_path == NULL && tcp_port == 0)
		fatal("You must specify a port or a socket");
	if (socket_path != NULL &&
	    strlen(socket_path) >= sizeof(addr_un->sun_path))
		fatal("Random pool path is too long");
	if (len <= 0 || len > 255)
		fatal("Too many bytes (%d) to read from PRNGD", len);

	memset(&addr, '\0', sizeof(addr));

	if (tcp_port != 0) {
		addr_in->sin_family = AF_INET;
		addr_in->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		addr_in->sin_port = htons(tcp_port);
		addr_len = sizeof(*addr_in);
	} else {
		addr_un->sun_family = AF_UNIX;
		strlcpy(addr_un->sun_path, socket_path,
		    sizeof(addr_un->sun_path));
		addr_len = offsetof(struct sockaddr_un, sun_path) +
		    strlen(socket_path) + 1;
	}

	old_sigpipe = mysignal(SIGPIPE, SIG_IGN);

	errors = 0;
	rval = -1;
reopen:
	fd = socket(addr.ss_family, SOCK_STREAM, 0);
	if (fd == -1) {
		error("Couldn't create socket: %s", strerror(errno));
		goto done;
	}

	if (connect(fd, (struct sockaddr*)&addr, addr_len) == -1) {
		if (tcp_port != 0) {
			error("Couldn't connect to PRNGD port %d: %s",
			    tcp_port, strerror(errno));
		} else {
			error("Couldn't connect to PRNGD socket \"%s\": %s",
			    addr_un->sun_path, strerror(errno));
		}
		goto done;
	}

	/* Send blocking read request to PRNGD */
	msg[0] = 0x02;
	msg[1] = len;

	if (atomicio(vwrite, fd, msg, sizeof(msg)) != sizeof(msg)) {
		if (errno == EPIPE && errors < 10) {
			close(fd);
			errors++;
			goto reopen;
		}
		error("Couldn't write to PRNGD socket: %s",
		    strerror(errno));
		goto done;
	}

	if (atomicio(read, fd, buf, len) != (size_t)len) {
		if (errno == EPIPE && errors < 10) {
			close(fd);
			errors++;
			goto reopen;
		}
		error("Couldn't read from PRNGD socket: %s",
		    strerror(errno));
		goto done;
	}

	rval = 0;
done:
	mysignal(SIGPIPE, old_sigpipe);
	if (fd != -1)
		close(fd);
	return rval;
}

static int
seed_from_prngd(unsigned char *buf, size_t bytes)
{
#ifdef PRNGD_PORT
	debug("trying egd/prngd port %d", PRNGD_PORT);
	if (get_random_bytes_prngd(buf, bytes, PRNGD_PORT, NULL) == 0)
		return 0;
#endif
#ifdef PRNGD_SOCKET
	debug("trying egd/prngd socket %s", PRNGD_SOCKET);
	if (get_random_bytes_prngd(buf, bytes, 0, PRNGD_SOCKET) == 0)
		return 0;
#endif
	return -1;
}

double
stir_gettimeofday(double entropy_estimate)
{
	struct timeval tv;

	if (gettimeofday(&tv, NULL) == -1)
		fatal("Couldn't gettimeofday: %s", strerror(errno));

	RAND_add(&tv, sizeof(tv), entropy_estimate);

	return entropy_estimate;
}

double
stir_clock(double entropy_estimate)
{
#ifdef HAVE_CLOCK
	clock_t c;

	c = clock();
	RAND_add(&c, sizeof(c), entropy_estimate);

	return entropy_estimate;
#else /* _HAVE_CLOCK */
	return 0;
#endif /* _HAVE_CLOCK */
}

double
stir_rusage(int who, double entropy_estimate)
{
#ifdef HAVE_GETRUSAGE
	struct rusage ru;

	if (getrusage(who, &ru) == -1)
		return 0;

	RAND_add(&ru, sizeof(ru), entropy_estimate);

	return entropy_estimate;
#else /* _HAVE_GETRUSAGE */
	return 0;
#endif /* _HAVE_GETRUSAGE */
}

static int
timeval_diff(struct timeval *t1, struct timeval *t2)
{
	int secdiff, usecdiff;

	secdiff = t2->tv_sec - t1->tv_sec;
	usecdiff = (secdiff*1000000) + (t2->tv_usec - t1->tv_usec);
	return (int)(usecdiff / 1000);
}

double
hash_command_output(entropy_cmd_t *src, unsigned char *hash)
{
	char buf[8192];
	fd_set rdset;
	int bytes_read, cmd_eof, error_abort, msec_elapsed, p[2];
	int status, total_bytes_read;
	static int devnull = -1;
	pid_t pid;
	SHA_CTX sha;
	struct timeval tv_start, tv_current;

	debug3("Reading output from \'%s\'", src->cmdstring);

	if (devnull == -1) {
		devnull = open("/dev/null", O_RDWR);
		if (devnull == -1)
			fatal("Couldn't open /dev/null: %s",
			    strerror(errno));
	}

	if (pipe(p) == -1)
		fatal("Couldn't open pipe: %s", strerror(errno));

	(void)gettimeofday(&tv_start, NULL); /* record start time */

	switch (pid = fork()) {
		case -1: /* Error */
			close(p[0]);
			close(p[1]);
			fatal("Couldn't fork: %s", strerror(errno));
			/* NOTREACHED */
		case 0: /* Child */
			dup2(devnull, STDIN_FILENO);
			dup2(p[1], STDOUT_FILENO);
			dup2(p[1], STDERR_FILENO);
			close(p[0]);
			close(p[1]);
			close(devnull);

			execv(src->path, (char**)(src->args));

			debug("(child) Couldn't exec '%s': %s",
			    src->cmdstring, strerror(errno));
			_exit(-1);
		default: /* Parent */
			break;
	}

	RAND_add(&pid, sizeof(&pid), 0.0);

	close(p[1]);

	/* Hash output from child */
	SHA1_Init(&sha);

	cmd_eof = error_abort = msec_elapsed = total_bytes_read = 0;
	while (!error_abort && !cmd_eof) {
		int ret;
		struct timeval tv;
		int msec_remaining;

		(void) gettimeofday(&tv_current, 0);
		msec_elapsed = timeval_diff(&tv_start, &tv_current);
		if (msec_elapsed >= entropy_timeout_current) {
			error_abort=1;
			continue;
		}
		msec_remaining = entropy_timeout_current - msec_elapsed;

		FD_ZERO(&rdset);
		FD_SET(p[0], &rdset);
		tv.tv_sec = msec_remaining / 1000;
		tv.tv_usec = (msec_remaining % 1000) * 1000;

		ret = select(p[0] + 1, &rdset, NULL, NULL, &tv);

		RAND_add(&tv, sizeof(tv), 0.0);

		switch (ret) {
		case 0:
			/* timer expired */
			error_abort = 1;
			kill(pid, SIGINT);
			break;
		case 1:
			/* command input */
			do {
				bytes_read = read(p[0], buf, sizeof(buf));
			} while (bytes_read == -1 && errno == EINTR);
			RAND_add(&bytes_read, sizeof(&bytes_read), 0.0);
			if (bytes_read == -1) {
				error_abort = 1;
				break;
			} else if (bytes_read) {
				SHA1_Update(&sha, buf, bytes_read);
				total_bytes_read += bytes_read;
			} else {
				cmd_eof = 1;
			}
			break;
		case -1:
		default:
			/* error */
			debug("Command '%s': select() failed: %s",
			    src->cmdstring, strerror(errno));
			error_abort = 1;
			break;
		}
	}

	SHA1_Final(hash, &sha);

	close(p[0]);

	debug3("Time elapsed: %d msec", msec_elapsed);

	if (waitpid(pid, &status, 0) == -1) {
		error("Couldn't wait for child '%s' completion: %s",
		    src->cmdstring, strerror(errno));
		return 0.0;
	}

	RAND_add(&status, sizeof(&status), 0.0);

	if (error_abort) {
		/*
		 * Closing p[0] on timeout causes the entropy command to
		 * SIGPIPE. Take whatever output we got, and mark this
		 * command as slow
		 */
		debug2("Command '%s' timed out", src->cmdstring);
		src->sticky_badness *= 2;
		src->badness = src->sticky_badness;
		return total_bytes_read;
	}

	if (WIFEXITED(status)) {
		if (WEXITSTATUS(status) == 0) {
			return total_bytes_read;
		} else {
			debug2("Command '%s' exit status was %d",
			    src->cmdstring, WEXITSTATUS(status));
			src->badness = src->sticky_badness = 128;
			return 0.0;
		}
	} else if (WIFSIGNALED(status)) {
		debug2("Command '%s' returned on uncaught signal %d !",
		    src->cmdstring, status);
		src->badness = src->sticky_badness = 128;
		return 0.0;
	} else
		return 0.0;
}

double
stir_from_system(void)
{
	double total_entropy_estimate;
	long int i;

	total_entropy_estimate = 0;

	i = getpid();
	RAND_add(&i, sizeof(i), 0.5);
	total_entropy_estimate += 0.1;

	i = getppid();
	RAND_add(&i, sizeof(i), 0.5);
	total_entropy_estimate += 0.1;

	i = getuid();
	RAND_add(&i, sizeof(i), 0.0);
	i = getgid();
	RAND_add(&i, sizeof(i), 0.0);

	total_entropy_estimate += stir_gettimeofday(1.0);
	total_entropy_estimate += stir_clock(0.5);
	total_entropy_estimate += stir_rusage(RUSAGE_SELF, 2.0);

	return total_entropy_estimate;
}

double
stir_from_programs(void)
{
	int c;
	double entropy, total_entropy;
	unsigned char hash[SHA_DIGEST_LENGTH];

	total_entropy = 0;
	for(c = 0; entropy_cmds[c].path != NULL; c++) {
		if (!entropy_cmds[c].badness) {
			/* Hash output from command */
			entropy = hash_command_output(&entropy_cmds[c],
			    hash);

			/* Scale back estimate by command's rate */
			entropy *= entropy_cmds[c].rate;

			/* Upper bound of entropy is SHA_DIGEST_LENGTH */
			if (entropy > SHA_DIGEST_LENGTH)
				entropy = SHA_DIGEST_LENGTH;

			/* Stir it in */
			RAND_add(hash, sizeof(hash), entropy);

			debug3("Got %0.2f bytes of entropy from '%s'",
			    entropy, entropy_cmds[c].cmdstring);

			total_entropy += entropy;

			/* Execution time should be a bit unpredictable */
			total_entropy += stir_gettimeofday(0.05);
			total_entropy += stir_clock(0.05);
			total_entropy += stir_rusage(RUSAGE_SELF, 0.1);
			total_entropy += stir_rusage(RUSAGE_CHILDREN, 0.1);
		} else {
			debug2("Command '%s' disabled (badness %d)",
			    entropy_cmds[c].cmdstring,
			    entropy_cmds[c].badness);

			if (entropy_cmds[c].badness > 0)
				entropy_cmds[c].badness--;
		}
	}

	return total_entropy;
}

/*
 * prng seedfile functions
 */
int
prng_check_seedfile(char *filename)
{
	struct stat st;

	/*
	 * XXX raceable: eg replace seed between this stat and subsequent
	 * open. Not such a problem because we don't really trust the
	 * seed file anyway.
	 * XXX: use secure path checking as elsewhere in OpenSSH
	 */
	if (lstat(filename, &st) == -1) {
		/* Give up on hard errors */
		if (errno != ENOENT)
			debug("WARNING: Couldn't stat random seed file "
			    "\"%.100s\": %s", filename, strerror(errno));
		return 0;
	}

	/* regular file? */
	if (!S_ISREG(st.st_mode))
		fatal("PRNG seedfile %.100s is not a regular file",
		    filename);

	/* mode 0600, owned by root or the current user? */
	if (((st.st_mode & 0177) != 0) || !(st.st_uid == getuid())) {
		debug("WARNING: PRNG seedfile %.100s must be mode 0600, "
		    "owned by uid %li", filename, (long int)getuid());
		return 0;
	}

	return 1;
}

void
prng_write_seedfile(void)
{
	int fd, save_errno;
	unsigned char seed[SEED_FILE_SIZE];
	char filename[MAXPATHLEN], tmpseed[MAXPATHLEN];
	struct passwd *pw;
	mode_t old_umask;

	pw = getpwuid(getuid());
	if (pw == NULL)
		fatal("Couldn't get password entry for current user "
		    "(%li): %s", (long int)getuid(), strerror(errno));

	/* Try to ensure that the parent directory is there */
	snprintf(filename, sizeof(filename), "%.512s/%s", pw->pw_dir,
	    _PATH_SSH_USER_DIR);
	if (mkdir(filename, 0700) < 0 && errno != EEXIST)
		fatal("mkdir %.200s: %s", filename, strerror(errno));

	snprintf(filename, sizeof(filename), "%.512s/%s", pw->pw_dir,
	    SSH_PRNG_SEED_FILE);

	strlcpy(tmpseed, filename, sizeof(tmpseed));
	if (strlcat(tmpseed, ".XXXXXXXXXX", sizeof(tmpseed)) >=
	    sizeof(tmpseed))
		fatal("PRNG seed filename too long");

	if (RAND_bytes(seed, sizeof(seed)) <= 0)
		fatal("PRNG seed extraction failed");

	/* Don't care if the seed doesn't exist */
	prng_check_seedfile(filename);

	old_umask = umask(0177);

	if ((fd = mkstemp(tmpseed)) == -1) {
		debug("WARNING: couldn't make temporary PRNG seedfile %.100s "
		    "(%.100s)", tmpseed, strerror(errno));
	} else {
		debug("writing PRNG seed to file %.100s", tmpseed);
		if (atomicio(vwrite, fd, &seed, sizeof(seed)) < sizeof(seed)) {
			save_errno = errno;
			close(fd);
			unlink(tmpseed);
			fatal("problem writing PRNG seedfile %.100s "
			    "(%.100s)", filename, strerror(save_errno));
		}
		close(fd);
		debug("moving temporary PRNG seed to file %.100s", filename);
		if (rename(tmpseed, filename) == -1) {
			save_errno = errno;
			unlink(tmpseed);
			fatal("problem renaming PRNG seedfile from %.100s "
			    "to %.100s (%.100s)", tmpseed, filename,
			    strerror(save_errno));
		}
	}
	umask(old_umask);
}

void
prng_read_seedfile(void)
{
	int fd;
	char seed[SEED_FILE_SIZE], filename[MAXPATHLEN];
	struct passwd *pw;

	pw = getpwuid(getuid());
	if (pw == NULL)
		fatal("Couldn't get password entry for current user "
		    "(%li): %s", (long int)getuid(), strerror(errno));

	snprintf(filename, sizeof(filename), "%.512s/%s", pw->pw_dir,
		SSH_PRNG_SEED_FILE);

	debug("loading PRNG seed from file %.100s", filename);

	if (!prng_check_seedfile(filename)) {
		verbose("Random seed file not found or invalid, ignoring.");
		return;
	}

	/* open the file and read in the seed */
	fd = open(filename, O_RDONLY);
	if (fd == -1)
		fatal("could not open PRNG seedfile %.100s (%.100s)",
		    filename, strerror(errno));

	if (atomicio(read, fd, &seed, sizeof(seed)) < sizeof(seed)) {
		verbose("invalid or short read from PRNG seedfile "
		    "%.100s - ignoring", filename);
		memset(seed, '\0', sizeof(seed));
	}
	close(fd);

	/* stir in the seed, with estimated entropy zero */
	RAND_add(&seed, sizeof(seed), 0.0);
}


/*
 * entropy command initialisation functions
 */
int
prng_read_commands(char *cmdfilename)
{
	char cmd[SEED_FILE_SIZE], *cp, line[1024], path[SEED_FILE_SIZE];
	double est;
	entropy_cmd_t *entcmd;
	FILE *f;
	int cur_cmd, linenum, num_cmds, arg;

	if ((f = fopen(cmdfilename, "r")) == NULL) {
		fatal("couldn't read entropy commands file %.100s: %.100s",
		    cmdfilename, strerror(errno));
	}

	num_cmds = 64;
	entcmd = xcalloc(num_cmds, sizeof(entropy_cmd_t));

	/* Read in file */
	cur_cmd = linenum = 0;
	while (fgets(line, sizeof(line), f)) {
		linenum++;

		/* Skip leading whitespace, blank lines and comments */
		cp = line + strspn(line, WHITESPACE);
		if ((*cp == 0) || (*cp == '#'))
			continue; /* done with this line */

		/*
		 * The first non-whitespace char should be a double quote
		 * delimiting the commandline
		 */
		if (*cp != '"') {
			error("bad entropy command, %.100s line %d",
			    cmdfilename, linenum);
			continue;
		}

		/*
		 * First token, command args (incl. argv[0]) in double
		 * quotes
		 */
		cp = strtok(cp, "\"");
		if (cp == NULL) {
			error("missing or bad command string, %.100s "
			    "line %d -- ignored", cmdfilename, linenum);
			continue;
		}
		strlcpy(cmd, cp, sizeof(cmd));

		/* Second token, full command path */
		if ((cp = strtok(NULL, WHITESPACE)) == NULL) {
			error("missing command path, %.100s "
			    "line %d -- ignored", cmdfilename, linenum);
			continue;
		}

		/* Did configure mark this as dead? */
		if (strncmp("undef", cp, 5) == 0)
			continue;

		strlcpy(path, cp, sizeof(path));

		/* Third token, entropy rate estimate for this command */
		if ((cp = strtok(NULL, WHITESPACE)) == NULL) {
			error("missing entropy estimate, %.100s "
			    "line %d -- ignored", cmdfilename, linenum);
			continue;
		}
		est = strtod(cp, NULL);

		/* end of line */
		if ((cp = strtok(NULL, WHITESPACE)) != NULL) {
			error("garbage at end of line %d in %.100s "
			    "-- ignored", linenum, cmdfilename);
			continue;
		}

		/* save the command for debug messages */
		entcmd[cur_cmd].cmdstring = xstrdup(cmd);

		/* split the command args */
		cp = strtok(cmd, WHITESPACE);
		arg = 0;
		do {
			entcmd[cur_cmd].args[arg] = xstrdup(cp);
			arg++;
		} while(arg < NUM_ARGS && (cp = strtok(NULL, WHITESPACE)));

		if (strtok(NULL, WHITESPACE))
			error("ignored extra commands (max %d), %.100s "
			    "line %d", NUM_ARGS, cmdfilename, linenum);

		/* Copy the command path and rate estimate */
		entcmd[cur_cmd].path = xstrdup(path);
		entcmd[cur_cmd].rate = est;

		/* Initialise other values */
		entcmd[cur_cmd].sticky_badness = 1;

		cur_cmd++;

		/*
		 * If we've filled the array, reallocate it twice the size
		 * Do this now because even if this we're on the last
		 * command we need another slot to mark the last entry
		 */
		if (cur_cmd == num_cmds) {
			num_cmds *= 2;
			entcmd = xrealloc(entcmd, num_cmds,
			    sizeof(entropy_cmd_t));
		}
	}

	/* zero the last entry */
	memset(&entcmd[cur_cmd], '\0', sizeof(entropy_cmd_t));

	/* trim to size */
	entropy_cmds = xrealloc(entcmd, (cur_cmd + 1),
	    sizeof(entropy_cmd_t));

	debug("Loaded %d entropy commands from %.100s", cur_cmd,
	    cmdfilename);

	fclose(f);
	return cur_cmd < MIN_ENTROPY_SOURCES ? -1 : 0;
}

void
usage(void)
{
	fprintf(stderr, "Usage: %s [options]\n", __progname);
	fprintf(stderr, "  -v          Verbose; display verbose debugging messages.\n");
	fprintf(stderr, "              Multiple -v increases verbosity.\n");
	fprintf(stderr, "  -x          Force output in hexadecimal (for debugging)\n");
	fprintf(stderr, "  -X          Force output in binary\n");
	fprintf(stderr, "  -b bytes    Number of bytes to output (default %d)\n",
	    OUTPUT_SEED_SIZE);
}

int
main(int argc, char **argv)
{
	unsigned char *buf;
	int ret, ch, debug_level, output_hex, bytes;
	extern char *optarg;
	LogLevel ll;

	__progname = ssh_get_progname(argv[0]);
	log_init(argv[0], SYSLOG_LEVEL_INFO, SYSLOG_FACILITY_USER, 1);

	ll = SYSLOG_LEVEL_INFO;
	debug_level = output_hex = 0;
	bytes = OUTPUT_SEED_SIZE;

	/* Don't write binary data to a tty, unless we are forced to */
	if (isatty(STDOUT_FILENO))
		output_hex = 1;

	while ((ch = getopt(argc, argv, "vxXhb:")) != -1) {
		switch (ch) {
		case 'v':
			if (debug_level < 3)
				ll = SYSLOG_LEVEL_DEBUG1 + debug_level++;
			break;
		case 'x':
			output_hex = 1;
			break;
		case 'X':
			output_hex = 0;
			break;
		case 'b':
			if ((bytes = atoi(optarg)) <= 0)
				fatal("Invalid number of output bytes");
			break;
		case 'h':
			usage();
			exit(0);
		default:
			error("Invalid commandline option");
			usage();
		}
	}

	log_init(argv[0], ll, SYSLOG_FACILITY_USER, 1);

#ifdef USE_SEED_FILES
	prng_read_seedfile();
#endif

	buf = xmalloc(bytes);

	/*
	 * Seed the RNG from wherever we can
	 */

	/* Take whatever is on the stack, but don't credit it */
	RAND_add(buf, bytes, 0);

	debug("Seeded RNG with %i bytes from system calls",
	    (int)stir_from_system());

	/* try prngd, fall back to commands if prngd fails or not configured */
	if (seed_from_prngd(buf, bytes) == 0) {
		RAND_add(buf, bytes, bytes);
	} else {
		/* Read in collection commands */
		if (prng_read_commands(SSH_PRNG_COMMAND_FILE) == -1)
			fatal("PRNG initialisation failed -- exiting.");
		debug("Seeded RNG with %i bytes from programs",
		    (int)stir_from_programs());
	}

#ifdef USE_SEED_FILES
	prng_write_seedfile();
#endif

	/*
	 * Write the seed to stdout
	 */

	if (!RAND_status())
		fatal("Not enough entropy in RNG");

	if (RAND_bytes(buf, bytes) <= 0)
		fatal("Couldn't extract entropy from PRNG");

	if (output_hex) {
		for(ret = 0; ret < bytes; ret++)
			printf("%02x", (unsigned char)(buf[ret]));
		printf("\n");
	} else
		ret = atomicio(vwrite, STDOUT_FILENO, buf, bytes);

	memset(buf, '\0', bytes);
	xfree(buf);

	return ret == bytes ? 0 : 1;
}

/*
 * We may attempt to re-seed during mkstemp if we are using the one in the
 * compat library (via mkstemp -> _gettemp -> arc4random -> seed_rng) so we
 * need our own seed_rng().  We must also check that we have enough entropy.
 */
void
seed_rng(void)
{
	if (!RAND_status())
		fatal("Not enough entropy in RNG");
}
