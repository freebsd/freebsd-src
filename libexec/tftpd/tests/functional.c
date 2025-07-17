/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018 Alan Somers.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <netinet/in.h>

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdalign.h>
#include <stdio.h>
#include <unistd.h>

#include <atf-c.h>
#include <libutil.h>

static const uint16_t BASEPORT = 6969;
static const char pidfile[] = "tftpd.pid";
static int protocol = PF_UNSPEC;
static int s = -1;	/* tftp client socket */
static struct sockaddr_storage addr; /* Destination address for the client */
static bool s_flag = false;	/* Pass -s to tftpd */
static bool w_flag = false;	/* Pass -w to tftpd */

/* Helper functions*/
static void require_bufeq(const char *expected, size_t expected_len,
    const char *actual, size_t len);

/*
 * Receive a response from tftpd
 * @param	hdr		The reply's expected header, as a char array
 * @param	contents	The reply's expected contents, as a char array
 * @param	contents_len	Length of contents
 */
#define RECV(hdr, contents, contents_len) do {				\
	char buffer[1024];						\
	struct sockaddr_storage from;					\
	socklen_t fromlen = sizeof(from);				\
	ssize_t r = recvfrom(s, buffer, sizeof(buffer), 0,		\
	    (struct sockaddr *)&from, &fromlen);			\
	ATF_REQUIRE(r > 0);						\
	require_bufeq((hdr), sizeof(hdr), buffer,			\
	    MIN((size_t)r, sizeof(hdr)));				\
	require_bufeq((const char *) (contents), (contents_len),	\
	    &buffer[sizeof(hdr)], r - sizeof(hdr));			\
	if (protocol == PF_INET) {					\
		((struct sockaddr_in *)&addr)->sin_port =		\
		    ((struct sockaddr_in *)&from)->sin_port;		\
	} else {							\
		((struct sockaddr_in6 *)&addr)->sin6_port =		\
		    ((struct sockaddr_in6 *)&from)->sin6_port;		\
	}								\
} while(0)

static void
recv_ack(uint16_t blocknum)
{
	char hdr[] = {0, 4, blocknum >> 8, blocknum & 0xFF};
	RECV(hdr, NULL, 0);
}

static void
recv_oack(const char *options, size_t options_len)
{
	char hdr[] = {0, 6};
	RECV(hdr, options, options_len);
}

/*
 * Receive a data packet from tftpd
 * @param	blocknum	Expected block number to be received
 * @param	contents	Pointer to expected contents
 * @param	contents_len	Length of contents expected to receive
 */
static void
recv_data(uint16_t blocknum, const char *contents, size_t contents_len)
{
	char hdr[] = {0, 3, blocknum >> 8, blocknum & 0xFF};
	RECV(hdr, contents, contents_len);
}

#define RECV_ERROR(code, msg) do {					\
	char hdr[] = {0, 5, code >> 8, code & 0xFF};			\
	RECV(hdr, msg, sizeof(msg));					\
} while (0)

/* 
 * send a command to tftpd.
 * @param	cmd		Command to send, as a char array
 */
static void
send_bytes(const void *cmd, size_t len)
{
	ssize_t r;

	r = sendto(s, cmd, len, 0, (struct sockaddr *)(&addr), addr.ss_len);
	ATF_REQUIRE(r >= 0);
	ATF_REQUIRE_EQ(len, (size_t)r);
}

static void
send_data(uint16_t blocknum, const char *contents, size_t contents_len)
{
	char buffer[1024];

	buffer[0] = 0;	/* DATA opcode high byte */
	buffer[1] = 3;	/* DATA opcode low byte */
	buffer[2] = blocknum >> 8;
	buffer[3] = blocknum & 0xFF;
	memmove(&buffer[4], contents, contents_len);
	send_bytes(buffer, 4 + contents_len);
}

/* 
 * send a command to tftpd.
 * @param	cmd		Command to send, as a const string
 *				(terminating NUL will be ignored)
 */
#define SEND_STR(cmd)							\
	ATF_REQUIRE_EQ(sizeof(cmd) - 1,					\
	    sendto(s, (cmd), sizeof(cmd) - 1, 0,			\
	    (struct sockaddr *)(&addr), addr.ss_len))

/*
 * Acknowledge block blocknum
 */
static void
send_ack(uint16_t blocknum)
{
	char packet[] = {
		0, 4,		/* ACK opcode in BE */
		blocknum >> 8,
		blocknum & 0xFF
	};

	send_bytes(packet, sizeof(packet));
}

/*
 * build an option string
 */
#define OPTION_STR(name, value)	name "\000" value "\000"

/* 
 * send a read request to tftpd.
 * @param	filename	filename as a string, absolute or relative
 * @param	mode		either "octet" or "netascii"
 */
#define SEND_RRQ(filename, mode)					\
	SEND_STR("\0\001" filename "\0" mode "\0")

/*
 * send a read request with options
 */
#define SEND_RRQ_OPT(filename, mode, options)				\
	SEND_STR("\0\001" filename "\0" mode "\000" options)

/* 
 * send a write request to tftpd.
 * @param	filename	filename as a string, absolute or relative
 * @param	mode		either "octet" or "netascii"
 */
#define SEND_WRQ(filename, mode)					\
	SEND_STR("\0\002" filename "\0" mode "\0")

/*
 * send a write request with options
 */
#define SEND_WRQ_OPT(filename, mode, options)				\
	SEND_STR("\0\002" filename "\0" mode "\000" options)

/* Define a test case, for both IPv4 and IPv6 */
#define TFTPD_TC_DEFINE(name, head, ...)				\
static void								\
name ## _body(void);							\
ATF_TC_WITH_CLEANUP(name ## _v4);					\
ATF_TC_HEAD(name ## _v4, tc)						\
{									\
	head								\
}									\
ATF_TC_BODY(name ## _v4, tc)						\
{									\
	int exitcode = 0;						\
	__VA_ARGS__;							\
	protocol = AF_INET;						\
	s = setup(&addr, __COUNTER__);					\
	name ## _body();						\
	close(s);							\
	if (exitcode >= 0)						\
		check_server(exitcode);					\
}									\
ATF_TC_CLEANUP(name ## _v4, tc)						\
{									\
	cleanup();							\
}									\
ATF_TC_WITH_CLEANUP(name ## _v6);					\
ATF_TC_HEAD(name ## _v6, tc)						\
{									\
	head								\
}									\
ATF_TC_BODY(name ## _v6, tc)						\
{									\
	int exitcode = 0;						\
	__VA_ARGS__;							\
	protocol = AF_INET6;						\
	s = setup(&addr, __COUNTER__);					\
	name ## _body();						\
	close(s);							\
	if (exitcode >= 0)						\
		check_server(exitcode);					\
}									\
ATF_TC_CLEANUP(name ## _v6, tc)						\
{									\
	cleanup();							\
}									\
static void								\
name ## _body(void)

/* Add the IPv4 and IPv6 versions of a test case */
#define TFTPD_TC_ADD(tp, name) do {					\
	ATF_TP_ADD_TC(tp, name ## _v4);					\
	ATF_TP_ADD_TC(tp, name ## _v6);					\
} while (0)

static void
sigalrm(int signo __unused)
{
}

/* Check that server exits with specific exit code */
static void
check_server(int exitcode)
{
	struct sigaction sa = { .sa_handler = sigalrm };
	struct itimerval it = { .it_value = { .tv_sec = 30 } };
	FILE *f;
	pid_t pid;
	int wstatus;

	f = fopen(pidfile, "r");
	ATF_REQUIRE(f != NULL);
	ATF_REQUIRE_INTEQ(1, fscanf(f, "%d", &pid));
	ATF_CHECK_INTEQ(0, fclose(f));
	ATF_REQUIRE_INTEQ(0, sigaction(SIGALRM, &sa, NULL));
	ATF_REQUIRE_EQ(0, setitimer(ITIMER_REAL, &it, NULL));
	ATF_REQUIRE_EQ(pid, waitpid(pid, &wstatus, 0));
	ATF_CHECK(WIFEXITED(wstatus));
	ATF_CHECK_INTEQ(exitcode, WEXITSTATUS(wstatus));
	unlink(pidfile);
}

/* Standard cleanup used by all testcases */
static void
cleanup(void)
{
	FILE *f;
	pid_t pid;

	f = fopen(pidfile, "r");
	if (f == NULL)
		return;
	unlink(pidfile);
	if (fscanf(f, "%d", &pid) == 1) {
		kill(pid, SIGTERM);
		waitpid(pid, NULL, 0);
	}
	fclose(f);
}

/* Assert that two binary buffers are identical */
static void
require_bufeq(const char *expected, size_t expected_len,
    const char *actual, size_t len)
{
	size_t i;

	ATF_REQUIRE_EQ_MSG(expected_len, len,
	    "Expected %zu bytes but got %zu", expected_len, len);
	for (i = 0; i < len; i++) {
		ATF_REQUIRE_EQ_MSG(expected[i], actual[i],
		    "Expected %#hhx at position %zu; got %hhx instead",
		    expected[i], i, actual[i]);
	}
}

/*
 * Start tftpd and return its communicating socket
 * @param	to	Will be filled in for use with sendto
 * @param	idx	Unique identifier of the test case
 * @return		Socket ready to use
 */
static int
setup(struct sockaddr_storage *to, uint16_t idx)
{
	int client_s, server_s, pid, argv_idx;
	char execname[] = "/usr/libexec/tftpd";
	char b_flag_str[] = "-b";
	char s_flag_str[] = "-s";
	char w_flag_str[] = "-w";
	char pwd[MAXPATHLEN];
	char *argv[10];
	struct sockaddr_in addr4;
	struct sockaddr_in6 addr6;
	struct sockaddr *server_addr;
	struct pidfh *pfh;
	uint16_t port = BASEPORT + idx;
	socklen_t len;
	int pd[2];

	ATF_REQUIRE_EQ(0, pipe2(pd, O_CLOEXEC));

	if (protocol == PF_INET) {
		len = sizeof(addr4);
		bzero(&addr4, len);
		addr4.sin_len = len;
		addr4.sin_family = PF_INET;
		addr4.sin_port = htons(port);
		server_addr = (struct sockaddr *)&addr4;
	} else {
		len = sizeof(addr6);
		bzero(&addr6, len);
		addr6.sin6_len = len;
		addr6.sin6_family = PF_INET6;
		addr6.sin6_port = htons(port);
		server_addr = (struct sockaddr *)&addr6;
	}

	ATF_REQUIRE_EQ(pwd, getcwd(pwd, sizeof(pwd)));
	
	/* Must bind(2) pre-fork so it happens before the client's send(2) */
	server_s = socket(protocol, SOCK_DGRAM, 0);
	if (server_s < 0 && errno == EAFNOSUPPORT) {
		atf_tc_skip("This test requires IPv%d support",
		    protocol == PF_INET ? 4 : 6);
	}
	ATF_REQUIRE_MSG(server_s >= 0,
	    "socket failed with error %s", strerror(errno));
	ATF_REQUIRE_EQ_MSG(0, bind(server_s, server_addr, len),
	    "bind failed with error %s", strerror(errno));

	pid = fork();
	switch (pid) {
	case -1:
		atf_tc_fail("fork failed");
		break;
	case 0:
		/* In child */
		pfh = pidfile_open(pidfile, 0644, NULL);
		ATF_REQUIRE_MSG(pfh != NULL,
		    "pidfile_open: %s", strerror(errno));
		ATF_REQUIRE_EQ(0, pidfile_write(pfh));
		ATF_REQUIRE_EQ(0, pidfile_close(pfh));

		bzero(argv, sizeof(argv));
		argv[0] = execname;
		argv_idx = 1;
		argv[argv_idx++] = b_flag_str;
		if (w_flag)
			argv[argv_idx++] = w_flag_str;
		if (s_flag)
			argv[argv_idx++] = s_flag_str;
		argv[argv_idx++] = pwd;
		ATF_REQUIRE_EQ(STDOUT_FILENO, dup2(server_s, STDOUT_FILENO));
		ATF_REQUIRE_EQ(STDIN_FILENO, dup2(server_s, STDIN_FILENO));
		ATF_REQUIRE_EQ(STDERR_FILENO, dup2(server_s, STDERR_FILENO));
		execv(execname, argv);
		atf_tc_fail("exec failed");
		break;
	default:
		/* In parent */
		ATF_REQUIRE_INTEQ(0, close(pd[1]));
		/* block until other end is closed on exec() or exit() */
		ATF_REQUIRE_INTEQ(0, read(pd[0], &pd[1], sizeof(pd[1])));
		ATF_REQUIRE_INTEQ(0, close(pd[0]));
		bzero(to, sizeof(*to));
		if (protocol == PF_INET) {
			struct sockaddr_in *to4 = (struct sockaddr_in *)to;
			to4->sin_len = sizeof(*to4);
			to4->sin_family = PF_INET;
			to4->sin_port = htons(port);
			to4->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		} else {
			struct in6_addr loopback = IN6ADDR_LOOPBACK_INIT;
			struct sockaddr_in6 *to6 = (struct sockaddr_in6 *)to;
			to6->sin6_len = sizeof(*to6);
			to6->sin6_family = PF_INET6;
			to6->sin6_port = htons(port);
			to6->sin6_addr = loopback;
		}

		ATF_REQUIRE_INTEQ(0, close(server_s));
		ATF_REQUIRE((client_s = socket(protocol, SOCK_DGRAM, 0)) > 0);
		break;
	}

	/* Clear the client's umask.  Test cases will specify exact modes */
	umask(0000);

	return (client_s);
}

/* Like write(2), but never returns less than the requested length */
static void
write_all(int fd, const void *buf, size_t nbytes)
{
	ssize_t r;

	while (nbytes > 0) {
		r = write(fd, buf, nbytes);
		ATF_REQUIRE(r > 0);
		nbytes -= (size_t)r;
		buf = (const char *)buf + (size_t)r;
	}
}


/*
 * Test Cases
 */

/*
 * Read a file, specified by absolute pathname.
 */
TFTPD_TC_DEFINE(abspath,)
{
	int fd;
	char command[1024];
	size_t pathlen;
	char suffix[] = {'\0', 'o', 'c', 't', 'e', 't', '\0'};

	command[0] = 0;		/* RRQ high byte */
	command[1] = 1;		/* RRQ low byte */
	ATF_REQUIRE(getcwd(&command[2], sizeof(command) - 2) != NULL);
	pathlen = strlcat(&command[2], "/abspath.txt", sizeof(command) - 2);
	ATF_REQUIRE(pathlen + sizeof(suffix) < sizeof(command) - 2);
	memmove(&command[2 + pathlen], suffix, sizeof(suffix));

	fd = open("abspath.txt", O_CREAT | O_RDONLY, 0644);
	ATF_REQUIRE(fd >= 0);
	close(fd);

	send_bytes(command, 2 + pathlen + sizeof(suffix));
	recv_data(1, NULL, 0);
	send_ack(1);
}

/*
 * Attempt to read a file outside of the allowed directory(ies)
 */
TFTPD_TC_DEFINE(dotdot,)
{
	ATF_REQUIRE_EQ(0, mkdir("subdir", 0777));
	SEND_RRQ("../disallowed.txt", "octet");
	RECV_ERROR(2, "Access violation");
	s = setup(&addr, __COUNTER__);
	SEND_RRQ("subdir/../../disallowed.txt", "octet");
	RECV_ERROR(2, "Access violation");
	s = setup(&addr, __COUNTER__);
	SEND_RRQ("/etc/passwd", "octet");
	RECV_ERROR(2, "Access violation");
}

/*
 * With "-s", tftpd should chroot to the specified directory
 */
TFTPD_TC_DEFINE(s_flag,
    atf_tc_set_md_var(tc, "require.user", "root");,
    s_flag = true)
{
	int fd;
	char contents[] = "small";

	fd = open("small.txt", O_RDWR | O_CREAT, 0644);
	ATF_REQUIRE(fd >= 0);
	write_all(fd, contents, strlen(contents) + 1);
	close(fd);

	SEND_RRQ("/small.txt", "octet");
	recv_data(1, contents, strlen(contents) + 1);
	send_ack(1);
}

/*
 * Read a file, and simulate a dropped ACK packet
 */
TFTPD_TC_DEFINE(rrq_dropped_ack,)
{
	int fd;
	char contents[] = "small";

	fd = open("small.txt", O_RDWR | O_CREAT, 0644);
	ATF_REQUIRE(fd >= 0);
	write_all(fd, contents, strlen(contents) + 1);
	close(fd);

	SEND_RRQ("small.txt", "octet");
	recv_data(1, contents, strlen(contents) + 1);
	/*
	 * client "sends" the ack, but network drops it
	 * Eventually, tftpd should resend the data packet
	 */
	recv_data(1, contents, strlen(contents) + 1);
	send_ack(1);
}

/*
 * Read a file, and simulate a dropped DATA packet
 */
TFTPD_TC_DEFINE(rrq_dropped_data,)
{
	int fd;
	size_t i;
	uint32_t contents[192];
	char buffer[1024];

	for (i = 0; i < nitems(contents); i++)
		contents[i] = i;

	fd = open("medium.txt", O_RDWR | O_CREAT, 0644);
	ATF_REQUIRE(fd >= 0);
	write_all(fd, contents, sizeof(contents));
	close(fd);

	SEND_RRQ("medium.txt", "octet");
	recv_data(1, (const char *)&contents[0], 512);
	send_ack(1);
	(void) recvfrom(s, buffer, sizeof(buffer), 0, NULL, NULL);
	/*
	 * server "sends" the data, but network drops it
	 * Eventually, client should resend the last ACK
	 */
	send_ack(1);
	recv_data(2, (const char *)&contents[128], 256);
	send_ack(2);
}

/*
 * Read a medium file, and simulate a duplicated ACK packet
 */
TFTPD_TC_DEFINE(rrq_duped_ack,)
{
	int fd;
	size_t i;
	uint32_t contents[192];

	for (i = 0; i < nitems(contents); i++)
		contents[i] = i;

	fd = open("medium.txt", O_RDWR | O_CREAT, 0644);
	ATF_REQUIRE(fd >= 0);
	write_all(fd, contents, sizeof(contents));
	close(fd);

	SEND_RRQ("medium.txt", "octet");
	recv_data(1, (const char *)&contents[0], 512);
	send_ack(1);
	send_ack(1);	/* Dupe an ACK packet */
	recv_data(2, (const char *)&contents[128], 256);
	recv_data(2, (const char *)&contents[128], 256);
	send_ack(2);
}


/*
 * Attempt to read a file without read permissions
 */
TFTPD_TC_DEFINE(rrq_eaccess,)
{
	int fd;

	fd = open("empty.txt", O_CREAT | O_RDONLY, 0000);
	ATF_REQUIRE(fd >= 0);
	close(fd);

	SEND_RRQ("empty.txt", "octet");
	RECV_ERROR(2, "Access violation");
}

/*
 * Read an empty file
 */
TFTPD_TC_DEFINE(rrq_empty,)
{
	int fd;

	fd = open("empty.txt", O_CREAT | O_RDONLY, 0644);
	ATF_REQUIRE(fd >= 0);
	close(fd);

	SEND_RRQ("empty.txt", "octet");
	recv_data(1, NULL, 0);
	send_ack(1);
}

/*
 * Read a medium file of more than one block
 */
TFTPD_TC_DEFINE(rrq_medium,)
{
	int fd;
	size_t i;
	uint32_t contents[192];

	for (i = 0; i < nitems(contents); i++)
		contents[i] = i;

	fd = open("medium.txt", O_RDWR | O_CREAT, 0644);
	ATF_REQUIRE(fd >= 0);
	write_all(fd, contents, sizeof(contents));
	close(fd);

	SEND_RRQ("medium.txt", "octet");
	recv_data(1, (const char *)&contents[0], 512);
	send_ack(1);
	recv_data(2, (const char *)&contents[128], 256);
	send_ack(2);
}

/*
 * Read a medium file with a window size of 2.
 */
TFTPD_TC_DEFINE(rrq_medium_window,)
{
	int fd;
	size_t i;
	uint32_t contents[192];
	char options[] = OPTION_STR("windowsize", "2");

	for (i = 0; i < nitems(contents); i++)
		contents[i] = i;

	fd = open("medium.txt", O_RDWR | O_CREAT, 0644);
	ATF_REQUIRE(fd >= 0);
	write_all(fd, contents, sizeof(contents));
	close(fd);

	SEND_RRQ_OPT("medium.txt", "octet", OPTION_STR("windowsize", "2"));
	recv_oack(options, sizeof(options) - 1);
	send_ack(0);
	recv_data(1, (const char *)&contents[0], 512);
	recv_data(2, (const char *)&contents[128], 256);
	send_ack(2);
}

/*
 * Read a file in netascii format
 */
TFTPD_TC_DEFINE(rrq_netascii,)
{
	int fd;
	char contents[] = "foo\nbar\rbaz\n";
	/* 
	 * Weirdly, RFC-764 says that CR must be followed by NUL if a line feed
	 * is not intended
	 */
	char expected[] = "foo\r\nbar\r\0baz\r\n";

	fd = open("unix.txt", O_RDWR | O_CREAT, 0644);
	ATF_REQUIRE(fd >= 0);
	write_all(fd, contents, strlen(contents) + 1);
	close(fd);

	SEND_RRQ("unix.txt", "netascii");
	recv_data(1, expected, sizeof(expected));
	send_ack(1);
}

/*
 * Read a file that doesn't exist
 */
TFTPD_TC_DEFINE(rrq_nonexistent,)
{
	SEND_RRQ("nonexistent.txt", "octet");
	RECV_ERROR(1, "File not found");
}

/*
 * Attempt to read a file whose name exceeds PATH_MAX
 */
TFTPD_TC_DEFINE(rrq_path_max,)
{
#define AReallyBigFileName \
	    "AReallyBigFileNameXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"\
	    "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"\
	    "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"\
	    "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"\
	    "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"\
	    "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"\
	    "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"\
	    "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"\
	    "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"\
	    "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"\
	    "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"\
	    "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"\
	    "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"\
	    "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"\
	    "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"\
	    "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"\
	    ".txt"
	ATF_REQUIRE_MSG(strlen(AReallyBigFileName) > PATH_MAX,
	    "Somebody increased PATH_MAX.  Update the test");
	SEND_RRQ(AReallyBigFileName, "octet");
	RECV_ERROR(4, "Illegal TFTP operation");
}

/*
 * Read a small file of less than one block
 */
TFTPD_TC_DEFINE(rrq_small,)
{
	int fd;
	char contents[] = "small";

	fd = open("small.txt", O_RDWR | O_CREAT, 0644);
	ATF_REQUIRE(fd >= 0);
	write_all(fd, contents, strlen(contents) + 1);
	close(fd);

	SEND_RRQ("small.txt", "octet");
	recv_data(1, contents, strlen(contents) + 1);
	send_ack(1);
}

/*
 * Read a file following the example in RFC 7440.
 */
TFTPD_TC_DEFINE(rrq_window_rfc7440,)
{
	int fd;
	size_t i;
	char options[] = OPTION_STR("windowsize", "4");
	alignas(uint32_t) char contents[13 * 512 - 4];
	uint32_t *u32p;

	u32p = (uint32_t *)contents;
	for (i = 0; i < sizeof(contents) / sizeof(uint32_t); i++)
		u32p[i] = i;

	fd = open("rfc7440.txt", O_RDWR | O_CREAT, 0644);
	ATF_REQUIRE(fd >= 0);
	write_all(fd, contents, sizeof(contents));
	close(fd);

	SEND_RRQ_OPT("rfc7440.txt", "octet", OPTION_STR("windowsize", "4"));
	recv_oack(options, sizeof(options) - 1);
	send_ack(0);
	recv_data(1, &contents[0 * 512], 512);
	recv_data(2, &contents[1 * 512], 512);
	recv_data(3, &contents[2 * 512], 512);
	recv_data(4, &contents[3 * 512], 512);
	send_ack(4);
	recv_data(5, &contents[4 * 512], 512);
	recv_data(6, &contents[5 * 512], 512);
	recv_data(7, &contents[6 * 512], 512);
	recv_data(8, &contents[7 * 512], 512);

	/* ACK 5 as if 6-8 were dropped. */
	send_ack(5);
	recv_data(6, &contents[5 * 512], 512);
	recv_data(7, &contents[6 * 512], 512);
	recv_data(8, &contents[7 * 512], 512);
	recv_data(9, &contents[8 * 512], 512);
	send_ack(9);
	recv_data(10, &contents[9 * 512], 512);
	recv_data(11, &contents[10 * 512], 512);
	recv_data(12, &contents[11 * 512], 512);
	recv_data(13, &contents[12 * 512], 508);

	/* Drop ACK and after timeout receive 10-13. */
	recv_data(10, &contents[9 * 512], 512);
	recv_data(11, &contents[10 * 512], 512);
	recv_data(12, &contents[11 * 512], 512);
	recv_data(13, &contents[12 * 512], 508);
	send_ack(13);
}

/*
 * Try to transfer a file with an unknown mode.
 */
TFTPD_TC_DEFINE(unknown_modes,)
{
	SEND_RRQ("foo.txt", "ascii");	/* Misspelling of "ascii" */
	RECV_ERROR(4, "Illegal TFTP operation");
	s = setup(&addr, __COUNTER__);
	SEND_RRQ("foo.txt", "binary");	/* Obsolete.  Use "octet" instead */
	RECV_ERROR(4, "Illegal TFTP operation");
	s = setup(&addr, __COUNTER__);
	SEND_RRQ("foo.txt", "en_US.UTF-8");
	RECV_ERROR(4, "Illegal TFTP operation");
	s = setup(&addr, __COUNTER__);
	SEND_RRQ("foo.txt", "mail");	/* Obsolete in RFC-1350 */
	RECV_ERROR(4, "Illegal TFTP operation");
}

/*
 * Send an unknown opcode.  tftpd should respond with the appropriate error
 */
TFTPD_TC_DEFINE(unknown_opcode,)
{
	/* Looks like an RRQ or WRQ request, but with a bad opcode */
	SEND_STR("\0\007foo.txt\0octet\0");
	RECV_ERROR(4, "Illegal TFTP operation");
}

/*
 * Invoke tftpd with "-w" and write to a nonexistent file.
 */
TFTPD_TC_DEFINE(w_flag,, w_flag = 1;)
{
	int fd;
	ssize_t r;
	char contents[] = "small";
	char buffer[1024];
	size_t contents_len;

	contents_len = strlen(contents) + 1;
	SEND_WRQ("small.txt", "octet");
	recv_ack(0);
	send_data(1, contents, contents_len);
	recv_ack(1);

	fd = open("small.txt", O_RDONLY);
	ATF_REQUIRE(fd >= 0);
	r = read(fd, buffer, sizeof(buffer));
	ATF_REQUIRE(r > 0);
	close(fd);
	require_bufeq(contents, contents_len, buffer, (size_t)r);
}

/*
 * Write a medium file, and simulate a dropped ACK packet
 */
TFTPD_TC_DEFINE(wrq_dropped_ack,)
{
	int fd;
	size_t i;
	ssize_t r;
	uint32_t contents[192];
	char buffer[1024];

	for (i = 0; i < nitems(contents); i++)
		contents[i] = i;

	fd = open("medium.txt", O_RDWR | O_CREAT, 0666);
	ATF_REQUIRE(fd >= 0);
	close(fd);

	SEND_WRQ("medium.txt", "octet");
	recv_ack(0);
	send_data(1, (const char *)&contents[0], 512);
	/* 
	 * Servers "sends" an ACK packet, but network drops it.
	 * Eventually, server should resend the last ACK
	 */
	(void) recvfrom(s, buffer, sizeof(buffer), 0, NULL, NULL);
	recv_ack(1);
	send_data(2, (const char *)&contents[128], 256);
	recv_ack(2);

	fd = open("medium.txt", O_RDONLY);
	ATF_REQUIRE(fd >= 0);
	r = read(fd, buffer, sizeof(buffer));
	ATF_REQUIRE(r > 0);
	close(fd);
	require_bufeq((const char *)contents, 768, buffer, (size_t)r);
}

/*
 * Write a small file, and simulate a dropped DATA packet
 */
TFTPD_TC_DEFINE(wrq_dropped_data,)
{
	int fd;
	ssize_t r;
	char contents[] = "small";
	size_t contents_len;
	char buffer[1024];

	fd = open("small.txt", O_RDWR | O_CREAT, 0666);
	ATF_REQUIRE(fd >= 0);
	close(fd);
	contents_len = strlen(contents) + 1;

	SEND_WRQ("small.txt", "octet");
	recv_ack(0);
	/* 
	 * Client "sends" a DATA packet, but network drops it.
	 * Eventually, server should resend the last ACK
	 */
	recv_ack(0);
	send_data(1, contents, contents_len);
	recv_ack(1);

	fd = open("small.txt", O_RDONLY);
	ATF_REQUIRE(fd >= 0);
	r = read(fd, buffer, sizeof(buffer));
	ATF_REQUIRE(r > 0);
	close(fd);
	require_bufeq(contents, contents_len, buffer, (size_t)r);
}

/*
 * Write a medium file, and simulate a duplicated DATA packet
 */
TFTPD_TC_DEFINE(wrq_duped_data,)
{
	int fd;
	size_t i;
	ssize_t r;
	uint32_t contents[192];
	char buffer[1024];

	for (i = 0; i < nitems(contents); i++)
		contents[i] = i;

	fd = open("medium.txt", O_RDWR | O_CREAT, 0666);
	ATF_REQUIRE(fd >= 0);
	close(fd);

	SEND_WRQ("medium.txt", "octet");
	recv_ack(0);
	send_data(1, (const char *)&contents[0], 512);
	send_data(1, (const char *)&contents[0], 512);
	recv_ack(1);
	recv_ack(1);
	send_data(2, (const char *)&contents[128], 256);
	recv_ack(2);

	fd = open("medium.txt", O_RDONLY);
	ATF_REQUIRE(fd >= 0);
	r = read(fd, buffer, sizeof(buffer));
	ATF_REQUIRE(r > 0);
	close(fd);
	require_bufeq((const char *)contents, 768, buffer, (size_t)r);
}

/*
 * Attempt to write a file without write permissions
 */
TFTPD_TC_DEFINE(wrq_eaccess,)
{
	int fd;

	fd = open("empty.txt", O_CREAT | O_RDONLY, 0440);
	ATF_REQUIRE(fd >= 0);
	close(fd);

	SEND_WRQ("empty.txt", "octet");
	RECV_ERROR(2, "Access violation");
}

/*
 * Attempt to write a file without world write permissions, but with world
 * read permissions
 */
TFTPD_TC_DEFINE(wrq_eaccess_world_readable,)
{
	int fd;

	fd = open("empty.txt", O_CREAT | O_RDONLY, 0444);
	ATF_REQUIRE(fd >= 0);
	close(fd);

	SEND_WRQ("empty.txt", "octet");
	RECV_ERROR(2, "Access violation");
}


/*
 * Write a medium file of more than one block
 */
TFTPD_TC_DEFINE(wrq_medium,)
{
	int fd;
	size_t i;
	ssize_t r;
	uint32_t contents[192];
	char buffer[1024];

	for (i = 0; i < nitems(contents); i++)
		contents[i] = i;

	fd = open("medium.txt", O_RDWR | O_CREAT, 0666);
	ATF_REQUIRE(fd >= 0);
	close(fd);

	SEND_WRQ("medium.txt", "octet");
	recv_ack(0);
	send_data(1, (const char *)&contents[0], 512);
	recv_ack(1);
	send_data(2, (const char *)&contents[128], 256);
	recv_ack(2);

	fd = open("medium.txt", O_RDONLY);
	ATF_REQUIRE(fd >= 0);
	r = read(fd, buffer, sizeof(buffer));
	ATF_REQUIRE(r > 0);
	close(fd);
	require_bufeq((const char *)contents, 768, buffer, (size_t)r);
}

/*
 * Write a medium file with a window size of 2.
 */
TFTPD_TC_DEFINE(wrq_medium_window,)
{
	int fd;
	size_t i;
	ssize_t r;
	uint32_t contents[192];
	char buffer[1024];
	char options[] = OPTION_STR("windowsize", "2");

	for (i = 0; i < nitems(contents); i++)
		contents[i] = i;

	fd = open("medium.txt", O_RDWR | O_CREAT, 0666);
	ATF_REQUIRE(fd >= 0);
	close(fd);

	SEND_WRQ_OPT("medium.txt", "octet", OPTION_STR("windowsize", "2"));
	recv_oack(options, sizeof(options) - 1);
	send_data(1, (const char *)&contents[0], 512);
	send_data(2, (const char *)&contents[128], 256);
	recv_ack(2);

	fd = open("medium.txt", O_RDONLY);
	ATF_REQUIRE(fd >= 0);
	r = read(fd, buffer, sizeof(buffer));
	ATF_REQUIRE(r > 0);
	close(fd);
	require_bufeq((const char *)contents, 768, buffer, (size_t)r);
}

/*
 * Write a file in netascii format
 */
TFTPD_TC_DEFINE(wrq_netascii,)
{
	int fd;
	ssize_t r;
	/* 
	 * Weirdly, RFC-764 says that CR must be followed by NUL if a line feed
	 * is not intended
	 */
	char contents[] = "foo\r\nbar\r\0baz\r\n";
	char expected[] = "foo\nbar\rbaz\n";
	size_t contents_len;
	char buffer[1024];

	fd = open("unix.txt", O_RDWR | O_CREAT, 0666);
	ATF_REQUIRE(fd >= 0);
	close(fd);
	contents_len = sizeof(contents);

	SEND_WRQ("unix.txt", "netascii");
	recv_ack(0);
	send_data(1, contents, contents_len);
	recv_ack(1);

	fd = open("unix.txt", O_RDONLY);
	ATF_REQUIRE(fd >= 0);
	r = read(fd, buffer, sizeof(buffer));
	ATF_REQUIRE(r > 0);
	close(fd);
	require_bufeq(expected, sizeof(expected), buffer, (size_t)r);
}

/*
 * Attempt to write to a nonexistent file.  With the default options, this
 * isn't allowed.
 */
TFTPD_TC_DEFINE(wrq_nonexistent,)
{
	SEND_WRQ("nonexistent.txt", "octet");
	RECV_ERROR(1, "File not found");
}

/*
 * Write a small file of less than one block
 */
TFTPD_TC_DEFINE(wrq_small,)
{
	int fd;
	ssize_t r;
	char contents[] = "small";
	size_t contents_len;
	char buffer[1024];

	fd = open("small.txt", O_RDWR | O_CREAT, 0666);
	ATF_REQUIRE(fd >= 0);
	close(fd);
	contents_len = strlen(contents) + 1;

	SEND_WRQ("small.txt", "octet");
	recv_ack(0);
	send_data(1, contents, contents_len);
	recv_ack(1);

	fd = open("small.txt", O_RDONLY);
	ATF_REQUIRE(fd >= 0);
	r = read(fd, buffer, sizeof(buffer));
	ATF_REQUIRE(r > 0);
	close(fd);
	require_bufeq(contents, contents_len, buffer, (size_t)r);
}

/*
 * Write an empty file over a non-empty one
 */
TFTPD_TC_DEFINE(wrq_truncate,)
{
	int fd;
	char contents[] = "small";
	struct stat sb;

	fd = open("small.txt", O_RDWR | O_CREAT, 0666);
	ATF_REQUIRE(fd >= 0);
	write_all(fd, contents, strlen(contents) + 1);
	close(fd);

	SEND_WRQ("small.txt", "octet");
	recv_ack(0);
	send_data(1, NULL, 0);
	recv_ack(1);

	ATF_REQUIRE_EQ(0, stat("small.txt", &sb));
	ATF_REQUIRE_EQ(0, sb.st_size);
}

/*
 * Write a file following the example in RFC 7440.
 */
TFTPD_TC_DEFINE(wrq_window_rfc7440,)
{
	int fd;
	size_t i;
	ssize_t r;
	char options[] = OPTION_STR("windowsize", "4");
	alignas(uint32_t) char contents[13 * 512 - 4];
	char buffer[sizeof(contents)];
	uint32_t *u32p;

	u32p = (uint32_t *)contents;
	for (i = 0; i < sizeof(contents) / sizeof(uint32_t); i++)
		u32p[i] = i;

	fd = open("rfc7440.txt", O_RDWR | O_CREAT, 0666);
	ATF_REQUIRE(fd >= 0);
	close(fd);

	SEND_WRQ_OPT("rfc7440.txt", "octet", OPTION_STR("windowsize", "4"));
	recv_oack(options, sizeof(options) - 1);
	send_data(1, &contents[0 * 512], 512);
	send_data(2, &contents[1 * 512], 512);
	send_data(3, &contents[2 * 512], 512);
	send_data(4, &contents[3 * 512], 512);
	recv_ack(4);
	send_data(5, &contents[4 * 512], 512);

	/* Drop 6-8. */
	recv_ack(5);
	send_data(6, &contents[5 * 512], 512);
	send_data(7, &contents[6 * 512], 512);
	send_data(8, &contents[7 * 512], 512);
	send_data(9, &contents[8 * 512], 512);
	recv_ack(9);

	/* Drop 11. */
	send_data(10, &contents[9 * 512], 512);
	send_data(12, &contents[11 * 512], 512);

	/*
	 * We can't send 13 here as tftpd has probably already seen 12
	 * and sent the ACK of 10 if running locally.  While it would
	 * recover by sending another ACK of 10, our state machine
	 * would be out of sync.
	 */

	/* Ignore ACK for 10 and resend 10-13. */
	recv_ack(10);
	send_data(10, &contents[9 * 512], 512);
	send_data(11, &contents[10 * 512], 512);
	send_data(12, &contents[11 * 512], 512);
	send_data(13, &contents[12 * 512], 508);
	recv_ack(13);

	fd = open("rfc7440.txt", O_RDONLY);
	ATF_REQUIRE(fd >= 0);
	r = read(fd, buffer, sizeof(buffer));
	ATF_REQUIRE(r > 0);
	close(fd);
	require_bufeq(contents, sizeof(contents), buffer, (size_t)r);
}

/*
 * Send less than four bytes
 */
TFTPD_TC_DEFINE(short_packet1, /* no head */, exitcode = 1)
{
	SEND_STR("\1");
}
TFTPD_TC_DEFINE(short_packet2, /* no head */, exitcode = 1)
{
	SEND_STR("\1\2");
}
TFTPD_TC_DEFINE(short_packet3, /* no head */, exitcode = 1)
{
	SEND_STR("\1\2\3");
}


/*
 * Main
 */

ATF_TP_ADD_TCS(tp)
{
	TFTPD_TC_ADD(tp, abspath);
	TFTPD_TC_ADD(tp, dotdot);
	TFTPD_TC_ADD(tp, s_flag);
	TFTPD_TC_ADD(tp, rrq_dropped_ack);
	TFTPD_TC_ADD(tp, rrq_dropped_data);
	TFTPD_TC_ADD(tp, rrq_duped_ack);
	TFTPD_TC_ADD(tp, rrq_eaccess);
	TFTPD_TC_ADD(tp, rrq_empty);
	TFTPD_TC_ADD(tp, rrq_medium);
	TFTPD_TC_ADD(tp, rrq_medium_window);
	TFTPD_TC_ADD(tp, rrq_netascii);
	TFTPD_TC_ADD(tp, rrq_nonexistent);
	TFTPD_TC_ADD(tp, rrq_path_max);
	TFTPD_TC_ADD(tp, rrq_small);
	TFTPD_TC_ADD(tp, rrq_window_rfc7440);
	TFTPD_TC_ADD(tp, unknown_modes);
	TFTPD_TC_ADD(tp, unknown_opcode);
	TFTPD_TC_ADD(tp, w_flag);
	TFTPD_TC_ADD(tp, wrq_dropped_ack);
	TFTPD_TC_ADD(tp, wrq_dropped_data);
	TFTPD_TC_ADD(tp, wrq_duped_data);
	TFTPD_TC_ADD(tp, wrq_eaccess);
	TFTPD_TC_ADD(tp, wrq_eaccess_world_readable);
	TFTPD_TC_ADD(tp, wrq_medium);
	TFTPD_TC_ADD(tp, wrq_medium_window);
	TFTPD_TC_ADD(tp, wrq_netascii);
	TFTPD_TC_ADD(tp, wrq_nonexistent);
	TFTPD_TC_ADD(tp, wrq_small);
	TFTPD_TC_ADD(tp, wrq_truncate);
	TFTPD_TC_ADD(tp, wrq_window_rfc7440);
	TFTPD_TC_ADD(tp, short_packet1);
	TFTPD_TC_ADD(tp, short_packet2);
	TFTPD_TC_ADD(tp, short_packet3);

	return (atf_no_error());
}
