/*-
 *
 * Copyright (c) 2024 Gleb Smirnoff <glebius@FreeBSD.org>
 * Copyright (c) 2014 Spectra Logic Corporation. All rights reserved.
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

#include <sys/cdefs.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/un.h>

#include <stdio.h>

#include <atf-c.h>

/*
 * Helper functions
 */

#define MIN(x, y)	((x) < (y) ? (x) : (y))
#define MAX(x, y)	((x) > (y) ? (x) : (y))

static void
do_socketpair(int *sv)
{
	int s;

	s = socketpair(PF_LOCAL, SOCK_SEQPACKET, 0, sv);
	ATF_REQUIRE_EQ(0, s);
	ATF_REQUIRE(sv[0] >= 0);
	ATF_REQUIRE(sv[1] >= 0);
	ATF_REQUIRE(sv[0] != sv[1]);
}

static void
do_socketpair_nonblocking(int *sv)
{
	int s;

	s = socketpair(PF_LOCAL, SOCK_SEQPACKET, 0, sv);
	ATF_REQUIRE_EQ(0, s);
	ATF_REQUIRE(sv[0] >= 0);
	ATF_REQUIRE(sv[1] >= 0);
	ATF_REQUIRE(sv[0] != sv[1]);
	ATF_REQUIRE(-1 != fcntl(sv[0], F_SETFL, O_NONBLOCK));
	ATF_REQUIRE(-1 != fcntl(sv[1], F_SETFL, O_NONBLOCK));
}

/*
 * Returns a bound and listening socket.
 * @return	const char* The path to the socket
 */
static const struct sockaddr_un *
mk_listening_socket(int *sv)
{
	/* ATF's isolation mechanisms will guarantee uniqueness of this file */
	static const struct sockaddr_un sun = {
		.sun_family = AF_LOCAL,
		.sun_len = sizeof(sun),
		.sun_path = "sock",
	};
	int s, r, l;

	s = socket(PF_LOCAL, SOCK_SEQPACKET, 0);
	ATF_REQUIRE(s >= 0);

	r = bind(s, (struct sockaddr *)&sun, sizeof(sun));
	l = listen(s, -1);
	ATF_CHECK_EQ(0, r);
	ATF_CHECK_EQ(0, l);

	if (sv != NULL)
		*sv = s;

	return (&sun);
}

/*
 * Returns a pair of sockets made the hard way: bind, listen, connect & accept
 * @return	const char* The path to the socket
 */
static const struct sockaddr_un *
mk_pair_of_sockets(int *sv)
{
	const struct sockaddr_un *sun;
	int s, s2, err, s1;

	sun = mk_listening_socket(&s);

	/* Create the other socket */
	s2 = socket(PF_LOCAL, SOCK_SEQPACKET, 0);
	ATF_REQUIRE(s2 >= 0);
	err = connect(s2, (struct sockaddr *)sun, sizeof(*sun));
	if (err != 0) {
		perror("connect");
		atf_tc_fail("connect(2) failed");
	}

	/* Accept it */
	s1 = accept(s, NULL, NULL);
	if (s1 == -1) {
		perror("accept");
		atf_tc_fail("accept(2) failed");
	}

	sv[0] = s1;
	sv[1] = s2;

	close(s);

	return (sun);
}

static volatile sig_atomic_t got_sigpipe = 0;
static void
shutdown_send_sigpipe_handler(int __unused x)
{
	got_sigpipe = 1;
}

/*
 * Parameterized test function bodies
 */
static void
test_eagain(int sndbufsize, int rcvbufsize)
{
	int i;
	int sv[2];
	const size_t totalsize = (sndbufsize + rcvbufsize) * 2;
	const size_t pktsize = MIN(sndbufsize, rcvbufsize) / 4;
	const int numpkts = totalsize / pktsize;
	char sndbuf[pktsize];
	ssize_t ssize;

	/* setup the socket pair */
	do_socketpair_nonblocking(sv);
	/* Setup the buffers */
	ATF_REQUIRE_EQ(0, setsockopt(sv[0], SOL_SOCKET, SO_SNDBUF, &sndbufsize,
	    sizeof(sndbufsize)));
	ATF_REQUIRE_EQ(0, setsockopt(sv[1], SOL_SOCKET, SO_RCVBUF, &rcvbufsize,
	    sizeof(rcvbufsize)));

	bzero(sndbuf, pktsize);
	/* Send data until we get EAGAIN */
	for(i=0; i < numpkts; i++) {
		ssize = send(sv[0], sndbuf, pktsize, MSG_EOR);
		if (ssize == -1) {
			if (errno == EAGAIN) {
				close(sv[0]);
				close(sv[1]);
				atf_tc_pass();
			}
			else {
				perror("send");
				atf_tc_fail("send returned < 0 but not EAGAIN");
			}
		}
	}
	atf_tc_fail("Never got EAGAIN");
}

static void
test_sendrecv_symmetric_buffers(int bufsize, int blocking) {
	int s;
	int sv[2];
	const ssize_t pktsize = bufsize / 2;
	char sndbuf[pktsize];
	char recv_buf[pktsize];
	ssize_t ssize, rsize;

	/* setup the socket pair */
	if (blocking)
		do_socketpair(sv);
	else
		do_socketpair_nonblocking(sv);

	/* Setup the buffers */
	s = setsockopt(sv[0], SOL_SOCKET, SO_SNDBUF, &bufsize, sizeof(bufsize));
	ATF_REQUIRE_EQ(0, s);
	s = setsockopt(sv[1], SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof(bufsize));
	ATF_REQUIRE_EQ(0, s);

	/* Fill the send buffer */
	bzero(sndbuf, pktsize);

	/* send and receive the packet */
	ssize = send(sv[0], sndbuf, pktsize, MSG_EOR);
	if (ssize < 0) {
		perror("send");
		atf_tc_fail("send returned < 0");
	}
	ATF_CHECK_EQ_MSG(pktsize, ssize, "expected %zd=send(...) but got %zd",
	    pktsize, ssize);

	rsize = recv(sv[1], recv_buf, pktsize, MSG_WAITALL);
	if (rsize < 0) {
		perror("recv");
		atf_tc_fail("recv returned < 0");
	}
	ATF_CHECK_EQ_MSG(pktsize, rsize, "expected %zd=send(...) but got %zd",
	    pktsize, rsize);
	close(sv[0]);
	close(sv[1]);
}

static void
test_pipe_simulator(int sndbufsize, int rcvbufsize)
{
	int num_sent, num_received;
	int sv[2];
	const ssize_t pktsize = MIN(sndbufsize, rcvbufsize) / 4;
	int numpkts;
	char sndbuf[pktsize];
	char rcvbuf[pktsize];
	char comparebuf[pktsize];
	ssize_t ssize, rsize;
	bool currently_sending = true;

	/* setup the socket pair */
	do_socketpair_nonblocking(sv);
	/* Setup the buffers */
	ATF_REQUIRE_EQ(0, setsockopt(sv[0], SOL_SOCKET, SO_SNDBUF, &sndbufsize,
	    sizeof(sndbufsize)));
	ATF_REQUIRE_EQ(0, setsockopt(sv[1], SOL_SOCKET, SO_RCVBUF, &rcvbufsize,
	    sizeof(rcvbufsize)));

	/* Send a total amount of data comfortably greater than the buffers */
	numpkts = MAX(sndbufsize, rcvbufsize) * 8 / pktsize;
	for (num_sent=0, num_received=0;
	     num_sent < numpkts || num_received < numpkts; ) {
		if (currently_sending && num_sent < numpkts) {
			/* The simulated sending process */
			/* fill the buffer */
			memset(sndbuf, num_sent, pktsize);
			ssize = send(sv[0], sndbuf, pktsize, MSG_EOR);
			if (ssize < 0) {
				if (errno == EAGAIN)
					currently_sending = false;
				else {
					perror("send");
					atf_tc_fail("send failed");
				}
			} else  {
				ATF_CHECK_EQ_MSG(pktsize, ssize,
				    "expected %zd=send(...) but got %zd",
				    pktsize, ssize);
				num_sent++;
			}
		} else {
			/* The simulated receiving process */
			rsize = recv(sv[1], rcvbuf, pktsize, MSG_WAITALL);
			if (rsize < 0) {
				if (errno == EAGAIN) {
					currently_sending = true;
					ATF_REQUIRE_MSG(num_sent < numpkts,
					    "Packets were lost!");
				}
				else {
					perror("recv");
					atf_tc_fail("recv failed");
				}
			} else  {
				ATF_CHECK_EQ_MSG(pktsize, rsize,
				    "expected %zd=recv(...) but got %zd",
				    pktsize, rsize);
				memset(comparebuf, num_received, pktsize);
				ATF_CHECK_EQ_MSG(0, memcmp(comparebuf, rcvbuf,
				    			   pktsize),
				    "Received data miscompare");
				num_received++;
			}
		}
	}
	close(sv[0]);
	close(sv[1]);
}

typedef struct {
	ssize_t	pktsize;
	int	numpkts;
	int	so;
} test_pipe_thread_data_t;

static void*
test_pipe_writer(void* args)
{
	test_pipe_thread_data_t* td = args;
	char sndbuf[td->pktsize];
	ssize_t ssize;
	int i;

	for(i=0; i < td->numpkts; i++) {
			memset(sndbuf, i, td->pktsize);
			ssize = send(td->so, sndbuf, td->pktsize, MSG_EOR);
			if (ssize < 0) {
				perror("send");
				atf_tc_fail("send returned < 0");
			}
			ATF_CHECK_EQ_MSG(td->pktsize, ssize,
			    		 "expected %zd=send(...) but got %zd",
			    		  td->pktsize, ssize);
	}
	return (0);
}

static void*
test_pipe_reader(void* args)
{
	test_pipe_thread_data_t* td = args;
	char rcvbuf[td->pktsize];
	char comparebuf[td->pktsize];
	ssize_t rsize;
	int i, d;

	for(i=0; i < td->numpkts; i++) {
		memset(comparebuf, i, td->pktsize);
		rsize = recv(td->so, rcvbuf, td->pktsize, MSG_WAITALL);
		if (rsize < 0) {
			perror("recv");
			atf_tc_fail("recv returned < 0");
		}
		ATF_CHECK_EQ_MSG(td->pktsize, rsize,
		    		 "expected %zd=send(...) but got %zd",
				 td->pktsize, rsize);
		d = memcmp(comparebuf, rcvbuf, td->pktsize);
		ATF_CHECK_EQ_MSG(0, d,
		    		 "Received data miscompare on packet %d", i);
	}
	return (0);
}


static void
test_pipe(int sndbufsize, int rcvbufsize)
{
	test_pipe_thread_data_t writer_data, reader_data;
	pthread_t writer, reader;
	int sv[2];
	const size_t pktsize = MIN(sndbufsize, rcvbufsize) / 4;
	int numpkts;

	/* setup the socket pair */
	do_socketpair(sv);
	/* Setup the buffers */
	ATF_REQUIRE_EQ(0, setsockopt(sv[0], SOL_SOCKET, SO_SNDBUF, &sndbufsize,
	    sizeof(sndbufsize)));
	ATF_REQUIRE_EQ(0, setsockopt(sv[1], SOL_SOCKET, SO_RCVBUF, &rcvbufsize,
	    sizeof(rcvbufsize)));

	/* Send a total amount of data comfortably greater than the buffers */
	numpkts = MAX(sndbufsize, rcvbufsize) * 8 / pktsize;

	/* Start the child threads */
	writer_data.pktsize = pktsize;
	writer_data.numpkts = numpkts;
	writer_data.so = sv[0];
	reader_data.pktsize = pktsize;
	reader_data.numpkts = numpkts;
	reader_data.so = sv[1];
	ATF_REQUIRE_EQ(0, pthread_create(&writer, NULL, test_pipe_writer,
	    				 (void*)&writer_data));
	/*
	 * Give the writer time to start writing, and hopefully block, before
	 * starting the reader.  This increases the likelihood of the test case
	 * failing due to PR kern/185812
	 */
	usleep(1000);
	ATF_REQUIRE_EQ(0, pthread_create(&reader, NULL, test_pipe_reader,
	    				 (void*)&reader_data));

	/* Join the children */
	ATF_REQUIRE_EQ(0, pthread_join(writer, NULL));
	ATF_REQUIRE_EQ(0, pthread_join(reader, NULL));
	close(sv[0]);
	close(sv[1]);
}


/*
 * Test Cases
 */

/* Create a SEQPACKET socket */
ATF_TC_WITHOUT_HEAD(create_socket);
ATF_TC_BODY(create_socket, tc)
{
	int s;

	s = socket(PF_LOCAL, SOCK_SEQPACKET, 0);
	ATF_REQUIRE(s >= 0);
	close(s);
}

/* Create SEQPACKET sockets using socketpair(2) */
ATF_TC_WITHOUT_HEAD(create_socketpair);
ATF_TC_BODY(create_socketpair, tc)
{
	int sv[2];
	int s;

	s = socketpair(PF_LOCAL, SOCK_SEQPACKET, 0, sv);
	ATF_CHECK_EQ(0, s);
	ATF_CHECK(sv[0] >= 0);
	ATF_CHECK(sv[1] >= 0);
	ATF_CHECK(sv[0] != sv[1]);
	close(sv[0]);
	close(sv[1]);
}

/* Call listen(2) without first calling bind(2).  It should fail */
ATF_TC_WITHOUT_HEAD(listen_unbound);
ATF_TC_BODY(listen_unbound, tc)
{
	int s, r;

	s = socket(PF_LOCAL, SOCK_SEQPACKET, 0);
	ATF_REQUIRE(s > 0);
	r = listen(s, -1);
	/* expect listen to fail since we haven't called bind(2) */
	ATF_CHECK(r != 0);
	close(s);
}

/* Bind the socket to a file */
ATF_TC_WITHOUT_HEAD(bind);
ATF_TC_BODY(bind, tc)
{
	struct sockaddr_un sun;
	/* ATF's isolation mechanisms will guarantee uniqueness of this file */
	const char *path = "sock";
	int s, r;

	s = socket(PF_LOCAL, SOCK_SEQPACKET, 0);
	ATF_REQUIRE(s >= 0);

	bzero(&sun, sizeof(sun));
	sun.sun_family = AF_LOCAL;
	sun.sun_len = sizeof(sun);
	strlcpy(sun.sun_path, path, sizeof(sun.sun_path));
	r = bind(s, (struct sockaddr *)&sun, sizeof(sun));
	ATF_CHECK_EQ(0, r);
	close(s);
}

/* listen(2) a socket that is already bound(2) should succeed */
ATF_TC_WITHOUT_HEAD(listen_bound);
ATF_TC_BODY(listen_bound, tc)
{
	int s;

	(void)mk_listening_socket(&s);
	close(s);
}

/* connect(2) can make a connection */
ATF_TC_WITHOUT_HEAD(connect);
ATF_TC_BODY(connect, tc)
{
	const struct sockaddr_un *sun;
	int s, err, s2;

	sun = mk_listening_socket(&s);

	/* Create the other socket */
	s2 = socket(PF_LOCAL, SOCK_SEQPACKET, 0);
	ATF_REQUIRE(s2 >= 0);
	err = connect(s2, (struct sockaddr *)sun, sizeof(*sun));
	if (err != 0) {
		perror("connect");
		atf_tc_fail("connect(2) failed");
	}
	close(s);
	close(s2);
}

/*
 * An undocumented feature that we probably want to preserve: sending to
 * a socket that isn't yet accepted lands data on the socket.  It can be
 * read after accept(2).
 */
ATF_TC_WITHOUT_HEAD(send_before_accept);
ATF_TC_BODY(send_before_accept, tc)
{
	const char buf[] = "hello";
	char repl[sizeof(buf)];
	const struct sockaddr_un *sun;
	int l, s, a;

	sun = mk_listening_socket(&l);

	ATF_REQUIRE((s = socket(PF_LOCAL, SOCK_SEQPACKET, 0)) > 0);
	ATF_REQUIRE(connect(s, (struct sockaddr *)sun, sizeof(*sun)) == 0);
	ATF_REQUIRE(send(s, &buf, sizeof(buf), 0) == sizeof(buf));
	ATF_REQUIRE((a = accept(l, NULL, NULL)) != 1);
	ATF_REQUIRE(recv(a, &repl, sizeof(repl), 0) == sizeof(buf));
	ATF_REQUIRE(strcmp(buf, repl) == 0);
	close(l);
	close(s);
	close(a);
}

/*
 * Test that close(2) of the peer ends in EPIPE when we try to send(2).
 * Test both normal case as well as a peer that was not accept(2)-ed.
 */
static bool sigpipe_received = false;
static void
sigpipe_handler(int signo __unused)
{
	sigpipe_received = true;
}

ATF_TC_WITHOUT_HEAD(send_to_closed);
ATF_TC_BODY(send_to_closed, tc)
{
	struct sigaction sa = {
		.sa_handler = sigpipe_handler,
	};
	const struct sockaddr_un *sun;
	int l, s, a;

	ATF_REQUIRE(sigemptyset(&sa.sa_mask) == 0);
	ATF_REQUIRE(sigaction(SIGPIPE, &sa, NULL) == 0);

	sun = mk_listening_socket(&l);

	ATF_REQUIRE((s = socket(PF_LOCAL, SOCK_SEQPACKET, 0)) > 0);
	ATF_REQUIRE(connect(s, (struct sockaddr *)sun, sizeof(*sun)) == 0);
	ATF_REQUIRE((a = accept(l, NULL, NULL)) != 1);
	close(a);
	ATF_REQUIRE(send(s, &s, sizeof(s), 0) == -1);
	ATF_REQUIRE(errno == EPIPE);
	ATF_REQUIRE(sigpipe_received == true);
	close(s);

	ATF_REQUIRE((s = socket(PF_LOCAL, SOCK_SEQPACKET, 0)) > 0);
	ATF_REQUIRE(connect(s, (struct sockaddr *)sun, sizeof(*sun)) == 0);
	close(l);
	sigpipe_received = false;
	ATF_REQUIRE(send(s, &s, sizeof(s), 0) == -1);
	ATF_REQUIRE(errno == EPIPE);
	ATF_REQUIRE(sigpipe_received == true);
	close(s);

	sa.sa_handler = SIG_DFL;
	ATF_REQUIRE(sigaction(SIGPIPE, &sa, NULL) == 0);
}

/* Implied connect is unix/dgram only feature. Fails on stream or seqpacket. */
ATF_TC_WITHOUT_HEAD(implied_connect);
ATF_TC_BODY(implied_connect, tc)
{
	const struct sockaddr_un *sun;
	int l, s;

	sun = mk_listening_socket(&l);

	ATF_REQUIRE((s = socket(PF_LOCAL, SOCK_SEQPACKET, 0)) > 0);
	ATF_REQUIRE(sendto(s, &s, sizeof(s), 0, (struct sockaddr *)sun,
	    sizeof(*sun)) == -1);
	ATF_REQUIRE(errno == ENOTCONN);
	close(l);
	close(s);
}

/* accept(2) can receive a connection */
ATF_TC_WITHOUT_HEAD(accept);
ATF_TC_BODY(accept, tc)
{
	int sv[2];

	mk_pair_of_sockets(sv);
	close(sv[0]);
	close(sv[1]);
}


/* Set O_NONBLOCK on the socket */
ATF_TC_WITHOUT_HEAD(fcntl_nonblock);
ATF_TC_BODY(fcntl_nonblock, tc)
{
	int s;

	s = socket(PF_LOCAL, SOCK_SEQPACKET, 0);
	ATF_REQUIRE(s >= 0);
	if (fcntl(s, F_SETFL, O_NONBLOCK) == -1) {
		perror("fcntl");
		atf_tc_fail("fcntl failed");
	}
	close(s);
}

/* Resize the send and receive buffers */
ATF_TC_WITHOUT_HEAD(resize_buffers);
ATF_TC_BODY(resize_buffers, tc)
{
	int s;
	int sndbuf = 12345;
	int rcvbuf = 23456;
	int xs, xr;
	socklen_t sl = sizeof(xs);

	s = socket(PF_LOCAL, SOCK_SEQPACKET, 0);
	ATF_REQUIRE(s >= 0);

	printf("                       Socket Buffer Sizes\n");
	printf("                              | SNDBUF  | RCVBUF  |\n");
	ATF_CHECK_EQ(0, getsockopt(s, SOL_SOCKET, SO_SNDBUF, &xs, &sl));
	ATF_CHECK_EQ(0, getsockopt(s, SOL_SOCKET, SO_RCVBUF, &xr, &sl));
	printf("Default                       | %7d | %7d |\n", xs, xr);

	if (setsockopt(s, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf)) != 0){
		perror("setsockopt");
		atf_tc_fail("setsockopt(SO_SNDBUF) failed");
	}
	ATF_CHECK_EQ(0, getsockopt(s, SOL_SOCKET, SO_SNDBUF, &xs, &sl));
	ATF_CHECK_EQ(0, getsockopt(s, SOL_SOCKET, SO_RCVBUF, &xr, &sl));
	printf("After changing SNDBUF         | %7d | %7d |\n", xs, xr);

	if (setsockopt(s, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf)) != 0){
		perror("setsockopt");
		atf_tc_fail("setsockopt(SO_RCVBUF) failed");
	}
	ATF_CHECK_EQ(0, getsockopt(s, SOL_SOCKET, SO_SNDBUF, &xs, &sl));
	ATF_CHECK_EQ(0, getsockopt(s, SOL_SOCKET, SO_RCVBUF, &xr, &sl));
	printf("After changing RCVBUF         | %7d | %7d |\n", xs, xr);
	close(s);
}

/*
 * Resize the send and receive buffers of a connected socketpair
 * Print some useful debugging info too
 */
ATF_TC_WITHOUT_HEAD(resize_connected_buffers);
ATF_TC_BODY(resize_connected_buffers, tc)
{
	int sv[2];
	int sndbuf = 12345;
	int rcvbuf = 23456;
	int err;
	int ls, lr, rs, rr;
	socklen_t sl = sizeof(ls);

	/* setup the socket pair */
	do_socketpair(sv);

	printf("                       Socket Buffer Sizes\n");
	printf("                              | Left Socket       | Right Socket      |\n");
	printf("                              | SNDBUF  | RCVBUF  | SNDBUF  | RCVBUF  |\n");
	ATF_CHECK_EQ(0, getsockopt(sv[0], SOL_SOCKET, SO_SNDBUF, &ls, &sl));
	ATF_CHECK_EQ(0, getsockopt(sv[0], SOL_SOCKET, SO_RCVBUF, &lr, &sl));
	ATF_CHECK_EQ(0, getsockopt(sv[1], SOL_SOCKET, SO_SNDBUF, &rs, &sl));
	ATF_CHECK_EQ(0, getsockopt(sv[1], SOL_SOCKET, SO_RCVBUF, &rr, &sl));
	printf("Default                       | %7d | %7d | %7d | %7d |\n",
	    ls, lr, rs, rr);

	/* Update one side's send buffer */
	err = setsockopt(sv[0], SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));
	if (err != 0){
		perror("setsockopt");
		atf_tc_fail("setsockopt(SO_SNDBUF) failed");
	}

	ATF_CHECK_EQ(0, getsockopt(sv[0], SOL_SOCKET, SO_SNDBUF, &ls, &sl));
	ATF_CHECK_EQ(0, getsockopt(sv[0], SOL_SOCKET, SO_RCVBUF, &lr, &sl));
	ATF_CHECK_EQ(0, getsockopt(sv[1], SOL_SOCKET, SO_SNDBUF, &rs, &sl));
	ATF_CHECK_EQ(0, getsockopt(sv[1], SOL_SOCKET, SO_RCVBUF, &rr, &sl));
	printf("After changing Left's SNDBUF  | %7d | %7d | %7d | %7d |\n",
	    ls, lr, rs, rr);

	/* Update the same side's receive buffer */
	err = setsockopt(sv[0], SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));
	if (err != 0){
		perror("setsockopt");
		atf_tc_fail("setsockopt(SO_RCVBUF) failed");
	}

	ATF_CHECK_EQ(0, getsockopt(sv[0], SOL_SOCKET, SO_SNDBUF, &ls, &sl));
	ATF_CHECK_EQ(0, getsockopt(sv[0], SOL_SOCKET, SO_RCVBUF, &lr, &sl));
	ATF_CHECK_EQ(0, getsockopt(sv[1], SOL_SOCKET, SO_SNDBUF, &rs, &sl));
	ATF_CHECK_EQ(0, getsockopt(sv[1], SOL_SOCKET, SO_RCVBUF, &rr, &sl));
	printf("After changing Left's RCVBUF  | %7d | %7d | %7d | %7d |\n",
	    ls, lr, rs, rr);
	close(sv[0]);
	close(sv[1]);
}


/* send(2) and recv(2) a single short record */
ATF_TC_WITHOUT_HEAD(send_recv);
ATF_TC_BODY(send_recv, tc)
{
	int sv[2];
	const int bufsize = 64;
	const char *data = "data";
	char recv_buf[bufsize];
	ssize_t datalen;
	ssize_t ssize, rsize;

	/* setup the socket pair */
	do_socketpair(sv);

	/* send and receive a small packet */
	datalen = strlen(data) + 1;	/* +1 for the null */
	ssize = send(sv[0], data, datalen, MSG_EOR);
	if (ssize < 0) {
		perror("send");
		atf_tc_fail("send returned < 0");
	}
	ATF_CHECK_EQ_MSG(datalen, ssize, "expected %zd=send(...) but got %zd",
	    datalen, ssize);

	rsize = recv(sv[1], recv_buf, bufsize, MSG_WAITALL);
	ATF_CHECK_EQ(datalen, rsize);
	close(sv[0]);
	close(sv[1]);
}

/* sendto(2) and recvfrom(2) a single short record
 * According to The Open Group Base Specifications Issue 6 IEEE Std 1003.1, 2004
 * Edition, sendto(2) is exactly the same as send(2) on a connection-mode socket
 *
 * According to the same spec, not all protocols are required to provide the
 * source addres in recvfrom(2).
 */
ATF_TC_WITHOUT_HEAD(sendto_recvfrom);
ATF_TC_BODY(sendto_recvfrom, tc)
{
#ifdef TEST_SEQ_PACKET_SOURCE_ADDRESS
	const sockaddr_un *sun;
#endif
	struct sockaddr_storage from;
	int sv[2];
	const int bufsize = 64;
	const char *data = "data";
	char recv_buf[bufsize];
	ssize_t datalen;
	ssize_t ssize, rsize;
	socklen_t fromlen;

	/* setup the socket pair */
#ifdef TEST_SEQ_PACKET_SOURCE_ADDRESS
	sun =
#endif
		mk_pair_of_sockets(sv);

	/* send and receive a small packet */
	datalen = strlen(data) + 1;	/* +1 for the null */
	ssize = sendto(sv[0], data, datalen, MSG_EOR, NULL, 0);
	if (ssize < 0) {
		perror("send");
		atf_tc_fail("send returned < 0");
	}
	ATF_CHECK_EQ_MSG(datalen, ssize, "expected %zd=send(...) but got %zd",
	    datalen, ssize);

	fromlen = sizeof(from);
	rsize = recvfrom(sv[1], recv_buf, bufsize, MSG_WAITALL,
	    (struct sockaddr*)&from, &fromlen);
	if (ssize < 0) {
		perror("recvfrom");
		atf_tc_fail("recvfrom returned < 0");
	}
	ATF_CHECK_EQ(datalen, rsize);

#ifdef TEST_SEQ_PACKET_SOURCE_ADDRESS
	/*
	 * FreeBSD does not currently provide the source address for SEQ_PACKET
	 * AF_UNIX sockets, and POSIX does not require it, so these two checks
	 * are disabled.  If FreeBSD gains that feature in the future, then
	 * these checks may be reenabled
	 */
	ATF_CHECK_EQ(PF_LOCAL, from.ss_family);
	ATF_CHECK_STREQ(sun->sun_path, ((struct sockaddr_un*)&from)->sun_path);
#endif
	close(sv[0]);
	close(sv[1]);
}

/*
 * send(2) and recv(2) a single short record with sockets created the
 * traditional way, involving bind, listen, connect, and accept
 */
ATF_TC_WITHOUT_HEAD(send_recv_with_connect);
ATF_TC_BODY(send_recv_with_connect, tc)
{
	int sv[2];
	const int bufsize = 64;
	const char *data = "data";
	char recv_buf[bufsize];
	ssize_t datalen;
	ssize_t ssize, rsize;

	mk_pair_of_sockets(sv);

	/* send and receive a small packet */
	datalen = strlen(data) + 1;	/* +1 for the null */
	ssize = send(sv[0], data, datalen, MSG_EOR);
	if (ssize < 0) {
		perror("send");
		atf_tc_fail("send returned < 0");
	}
	ATF_CHECK_EQ_MSG(datalen, ssize, "expected %zd=send(...) but got %zd",
	    datalen, ssize);

	rsize = recv(sv[1], recv_buf, bufsize, MSG_WAITALL);
	ATF_CHECK_EQ(datalen, rsize);
	close(sv[0]);
	close(sv[1]);
}

/* send(2) should fail on a shutdown socket */
ATF_TC_WITHOUT_HEAD(shutdown_send);
ATF_TC_BODY(shutdown_send, tc)
{
	const struct sockaddr_un *sun;
	const char *data = "data";
	ssize_t datalen, ssize;
	int s, err, s2;

	sun = mk_listening_socket(&s);

	/* Create the other socket */
	s2 = socket(PF_LOCAL, SOCK_SEQPACKET, 0);
	ATF_REQUIRE(s2 >= 0);
	err = connect(s2, (struct sockaddr *)sun, sizeof(*sun));
	if (err != 0) {
		perror("connect");
		atf_tc_fail("connect(2) failed");
	}

	ATF_CHECK_EQ(0, shutdown(s2, SHUT_RDWR));
	datalen = strlen(data) + 1;	/* +1 for the null */
	/* USE MSG_NOSIGNAL so we don't get SIGPIPE */
	ssize = send(s2, data, datalen, MSG_EOR | MSG_NOSIGNAL);
	ATF_CHECK_EQ(EPIPE, errno);
	ATF_CHECK_EQ(-1, ssize);
	close(s);
	close(s2);
}

/* send(2) should cause SIGPIPE on a shutdown socket */
ATF_TC_WITHOUT_HEAD(shutdown_send_sigpipe);
ATF_TC_BODY(shutdown_send_sigpipe, tc)
{
	const struct sockaddr_un *sun;
	const char *data = "data";
	ssize_t datalen;
	int s, err, s2;

	sun = mk_listening_socket(&s);

	/* Create the other socket */
	s2 = socket(PF_LOCAL, SOCK_SEQPACKET, 0);
	ATF_REQUIRE(s2 >= 0);
	err = connect(s2, (struct sockaddr *)sun, sizeof(*sun));
	if (err != 0) {
		perror("connect");
		atf_tc_fail("connect(2) failed");
	}

	ATF_CHECK_EQ(0, shutdown(s2, SHUT_RDWR));
	ATF_REQUIRE(SIG_ERR != signal(SIGPIPE, shutdown_send_sigpipe_handler));
	datalen = strlen(data) + 1;	/* +1 for the null */
	(void)send(s2, data, datalen, MSG_EOR);
	ATF_CHECK_EQ(1, got_sigpipe);
	close(s);
	close(s2);
}

/* nonblocking send(2) and recv(2) a single short record */
ATF_TC_WITHOUT_HEAD(send_recv_nonblocking);
ATF_TC_BODY(send_recv_nonblocking, tc)
{
	int sv[2];
	const int bufsize = 64;
	const char *data = "data";
	char recv_buf[bufsize];
	ssize_t datalen;
	ssize_t ssize, rsize;

	/* setup the socket pair */
	do_socketpair_nonblocking(sv);

	/* Verify that there is nothing to receive */
	rsize = recv(sv[1], recv_buf, bufsize, MSG_WAITALL);
	ATF_CHECK_EQ(EAGAIN, errno);
	ATF_CHECK_EQ(-1, rsize);

	/* send and receive a small packet */
	datalen = strlen(data) + 1;	/* +1 for the null */
	ssize = send(sv[0], data, datalen, MSG_EOR);
	if (ssize < 0) {
		perror("send");
		atf_tc_fail("send returned < 0");
	}
	ATF_CHECK_EQ_MSG(datalen, ssize, "expected %zd=send(...) but got %zd",
	    datalen, ssize);

	rsize = recv(sv[1], recv_buf, bufsize, MSG_WAITALL);
	ATF_CHECK_EQ(datalen, rsize);
	close(sv[0]);
	close(sv[1]);
}

/*
 * We should get EAGAIN if we try to send a message larger than the socket
 * buffer, with nonblocking sockets.  Test with several different sockbuf sizes
 */
ATF_TC_WITHOUT_HEAD(eagain_8k_8k);
ATF_TC_BODY(eagain_8k_8k, tc)
{
	test_eagain(8192, 8192);
}
ATF_TC_WITHOUT_HEAD(eagain_8k_128k);
ATF_TC_BODY(eagain_8k_128k, tc)
{
	test_eagain(8192, 131072);
}
ATF_TC_WITHOUT_HEAD(eagain_128k_8k);
ATF_TC_BODY(eagain_128k_8k, tc)
{
	test_eagain(131072, 8192);
}
ATF_TC_WITHOUT_HEAD(eagain_128k_128k);
ATF_TC_BODY(eagain_128k_128k, tc)
{
	test_eagain(131072, 131072);
}


/*
 * nonblocking send(2) and recv(2) of several records, which should collectively
 * fill up the send buffer but not the receive buffer
 */
ATF_TC_WITHOUT_HEAD(rcvbuf_oversized);
ATF_TC_BODY(rcvbuf_oversized, tc)
{
	int i;
	int sv[2];
	const ssize_t pktsize = 1024;
	const int sndbufsize = 8192;
	const int rcvbufsize = 131072;
	const size_t geometric_mean_bufsize = 32768;
	const int numpkts = geometric_mean_bufsize / pktsize;
	char sndbuf[pktsize];
	char recv_buf[pktsize];
	ssize_t ssize, rsize;

	/* setup the socket pair */
	do_socketpair_nonblocking(sv);
	ATF_REQUIRE_EQ(0, setsockopt(sv[0], SOL_SOCKET, SO_SNDBUF, &sndbufsize,
	    sizeof(sndbufsize)));
	ATF_REQUIRE_EQ(0, setsockopt(sv[1], SOL_SOCKET, SO_RCVBUF, &rcvbufsize,
	    sizeof(rcvbufsize)));

	/*
	 * Send and receive packets that are collectively greater than the send
	 * buffer, but less than the receive buffer
	 */
	for (i=0; i < numpkts; i++) {
		/* Fill the buffer */
		memset(sndbuf, i, pktsize);

		/* send the packet */
		ssize = send(sv[0], sndbuf, pktsize, MSG_EOR);
		if (ssize < 0) {
			perror("send");
			atf_tc_fail("send returned < 0");
		}
		ATF_CHECK_EQ_MSG(pktsize, ssize,
		    "expected %zd=send(...) but got %zd", pktsize, ssize);

		/* Receive it */

		rsize = recv(sv[1], recv_buf, pktsize, MSG_WAITALL);
		if (rsize < 0) {
			perror("recv");
			atf_tc_fail("recv returned < 0");
		}
		ATF_CHECK_EQ_MSG(pktsize, rsize,
		    "expected %zd=send(...) but got %zd", pktsize, rsize);

		/* Verify the contents */
		ATF_CHECK_EQ_MSG(0, memcmp(sndbuf, recv_buf, pktsize),
		    "Received data miscompare");
	}

	/* Trying to receive again should return EAGAIN */
	rsize = recv(sv[1], recv_buf, pktsize, MSG_WAITALL);
	ATF_CHECK_EQ(EAGAIN, errno);
	ATF_CHECK_EQ(-1, rsize);
	close(sv[0]);
	close(sv[1]);
}

/*
 * Simulate the behavior of a blocking pipe.  The sender will send until his
 * buffer fills up, then we'll simulate a scheduler switch that will allow the
 * receiver to read until his buffer empties.  Repeat the process until the
 * transfer is complete.
 * Repeat the test with multiple send and receive buffer sizes
 */
ATF_TC_WITHOUT_HEAD(pipe_simulator_8k_8k);
ATF_TC_BODY(pipe_simulator_8k_8k, tc)
{
	test_pipe_simulator(8192, 8192);
}

ATF_TC_WITHOUT_HEAD(pipe_simulator_8k_128k);
ATF_TC_BODY(pipe_simulator_8k_128k, tc)
{
	test_pipe_simulator(8192, 131072);
}

ATF_TC_WITHOUT_HEAD(pipe_simulator_128k_8k);
ATF_TC_BODY(pipe_simulator_128k_8k, tc)
{
	test_pipe_simulator(131072, 8192);
}

ATF_TC_WITHOUT_HEAD(pipe_simulator_128k_128k);
ATF_TC_BODY(pipe_simulator_128k_128k, tc)
{
	test_pipe_simulator(131072, 131072);
}

/*
 * Test blocking I/O by passing data between two threads.  The total amount of
 * data will be >> buffer size to force blocking.  Repeat the test with multiple
 * send and receive buffer sizes
 */
ATF_TC_WITHOUT_HEAD(pipe_8k_8k);
ATF_TC_BODY(pipe_8k_8k, tc)
{
	test_pipe(8192, 8192);
}

ATF_TC_WITHOUT_HEAD(pipe_8k_128k);
ATF_TC_BODY(pipe_8k_128k, tc)
{
	test_pipe(8192, 131072);
}

ATF_TC_WITHOUT_HEAD(pipe_128k_8k);
ATF_TC_BODY(pipe_128k_8k, tc)
{
	test_pipe(131072, 8192);
}

ATF_TC_WITHOUT_HEAD(pipe_128k_128k);
ATF_TC_BODY(pipe_128k_128k, tc)
{
	test_pipe(131072, 131072);
}


/*
 * Test single-packet I/O with and without blocking, with symmetric buffers of
 * various sizes
 */
ATF_TC_WITHOUT_HEAD(sendrecv_8k);
ATF_TC_BODY(sendrecv_8k, tc)
{
	test_sendrecv_symmetric_buffers(8 * 1024, true);
}
ATF_TC_WITHOUT_HEAD(sendrecv_16k);
ATF_TC_BODY(sendrecv_16k, tc)
{
	test_sendrecv_symmetric_buffers(16 * 1024, true);
}
ATF_TC_WITHOUT_HEAD(sendrecv_32k);
ATF_TC_BODY(sendrecv_32k, tc)
{
	test_sendrecv_symmetric_buffers(32 * 1024, true);
}
ATF_TC_WITHOUT_HEAD(sendrecv_64k);
ATF_TC_BODY(sendrecv_64k, tc)
{
	test_sendrecv_symmetric_buffers(64 * 1024, true);
}
ATF_TC_WITHOUT_HEAD(sendrecv_128k);
ATF_TC_BODY(sendrecv_128k, tc)
{
	test_sendrecv_symmetric_buffers(128 * 1024, true);
}
ATF_TC_WITHOUT_HEAD(sendrecv_8k_nonblocking);
ATF_TC_BODY(sendrecv_8k_nonblocking, tc)
{
	test_sendrecv_symmetric_buffers(8 * 1024, false);
}
ATF_TC_WITHOUT_HEAD(sendrecv_16k_nonblocking);
ATF_TC_BODY(sendrecv_16k_nonblocking, tc)
{
	test_sendrecv_symmetric_buffers(16 * 1024, false);
}
ATF_TC_WITHOUT_HEAD(sendrecv_32k_nonblocking);
ATF_TC_BODY(sendrecv_32k_nonblocking, tc)
{
	test_sendrecv_symmetric_buffers(32 * 1024, false);
}
ATF_TC_WITHOUT_HEAD(sendrecv_64k_nonblocking);
ATF_TC_BODY(sendrecv_64k_nonblocking, tc)
{
	test_sendrecv_symmetric_buffers(64 * 1024, false);
}
ATF_TC_WITHOUT_HEAD(sendrecv_128k_nonblocking);
ATF_TC_BODY(sendrecv_128k_nonblocking, tc)
{
	test_sendrecv_symmetric_buffers(128 * 1024, false);
}

ATF_TC(random_eor_and_waitall);
ATF_TC_HEAD(random_eor_and_waitall, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test random sized send/recv with "
	    "randomly placed MSG_EOR and randomly applied MSG_WAITALL on "
	    "PF_UNIX/SOCK_SEQPACKET");
}

struct random_eor_params {
	u_long recvspace;
	char *sendbuf;
	size_t *records;
	u_int nrecords;
	int sock;
	u_short seed[6];
};

#define	RANDOM_TESTSIZE	((size_t)100 * 1024 * 1024)
/* Below defines are factor of recvspace. */
#define	RANDOM_MAXRECORD	10
#define	RANDOM_SENDSIZE	2
#define	RANDOM_RECVSIZE	4

static void *
sending_thread(void *arg)
{
	struct random_eor_params *params = arg;
	size_t off = 0;
	int eor = 0;

	while (off < RANDOM_TESTSIZE) {
		ssize_t len;
		int flags;

		len = nrand48(&params->seed[3]) %
		    (RANDOM_SENDSIZE * params->recvspace);
		if (off + len >= params->records[eor]) {
			len = params->records[eor] - off;
			flags = MSG_EOR;
			eor++;
		} else
			flags = 0;
		ATF_REQUIRE(send(params->sock, &params->sendbuf[off], len,
		    flags) == len);
		off += len;
#ifdef DEBUG
		printf("send %zd%s\n", off, flags ? " EOR" : "");
#endif
	}

	return (NULL);
}

ATF_TC_BODY(random_eor_and_waitall, tc)
{
	struct random_eor_params params;
	void *recvbuf;
	pthread_t t;
	size_t off;
	int fd[2], eor;

	arc4random_buf(params.seed, sizeof(params.seed));
	printf("Using seed:");
	for (u_int i = 0; i < (u_int)sizeof(params.seed)/sizeof(u_short); i++)
		printf(" 0x%.4x,", params.seed[i]);
	printf("\n");

	ATF_REQUIRE((params.sendbuf = malloc(RANDOM_TESTSIZE)) != NULL);
	for (u_int i = 0; i < RANDOM_TESTSIZE / (u_int )sizeof(long); i++)
		((long *)params.sendbuf)[i] = nrand48(&params.seed[0]);

	ATF_REQUIRE(sysctlbyname("net.local.stream.recvspace",
	    &params.recvspace, &(size_t){sizeof(u_long)}, NULL, 0) != -1);
	ATF_REQUIRE((recvbuf =
	    malloc(RANDOM_RECVSIZE * params.recvspace)) != NULL);

	params.nrecords = 2 * RANDOM_TESTSIZE /
	    (RANDOM_MAXRECORD * params.recvspace);

	ATF_REQUIRE((params.records =
	    malloc(params.nrecords * sizeof(size_t *))) != NULL);
	off = 0;
	for (u_int i = 0; i < params.nrecords; i++) {
		off += 1 + nrand48(&params.seed[0]) %
		    (RANDOM_MAXRECORD * params.recvspace);
		if (off > RANDOM_TESTSIZE) {
			params.nrecords = i;
			break;
		}
		params.records[i] = off;
	}
	params.records[params.nrecords - 1] = RANDOM_TESTSIZE;

	ATF_REQUIRE(socketpair(PF_LOCAL, SOCK_SEQPACKET, 0, fd) == 0);
	params.sock = fd[0];
	ATF_REQUIRE(pthread_create(&t, NULL, sending_thread, &params) == 0);

	off = 0;
	eor = 0;
	while (off < RANDOM_TESTSIZE) {
		struct iovec iov = {
			.iov_base = recvbuf,
			.iov_len = nrand48(&params.seed[0]) %
			    (RANDOM_RECVSIZE * params.recvspace)
		};
		struct msghdr hdr = {
			.msg_iov = &iov,
			.msg_iovlen = 1,
		};
		size_t len;
		int waitall = iov.iov_len & 0x1 ? MSG_WAITALL : 0;

		len = recvmsg(fd[1], &hdr, waitall);
		if (waitall && !(hdr.msg_flags & MSG_EOR))
			ATF_CHECK_EQ_MSG(len, iov.iov_len,
			    "recvmsg(MSG_WAITALL): %zd, expected %zd",
			    len, iov.iov_len);
		if (off + len == params.records[eor]) {
			ATF_REQUIRE_MSG(hdr.msg_flags & MSG_EOR,
			    "recvmsg(): expected EOR @ %zd", off + len);
			eor++;
		} else {
			ATF_REQUIRE_MSG(off + len < params.records[eor],
			    "recvmsg() past EOR: %zd, expected %zd",
			    off + len, params.records[eor]);
			ATF_REQUIRE_MSG(!(hdr.msg_flags & MSG_EOR),
			    "recvmsg() spurious EOR at %zd, expected %zd",
			    off + len, params.records[eor]);
		}
		ATF_REQUIRE_MSG(0 == memcmp(params.sendbuf + off, recvbuf, len),
		    "data corruption past %zd", off);
		off += len;
#ifdef DEBUG
		printf("recv %zd%s %zd/%zd%s\n", off,
		    (hdr.msg_flags & MSG_EOR) ?  " EOR" : "",
		    len, iov.iov_len,
		    waitall ? " WAITALL" : "");
#endif
	}

	ATF_REQUIRE(pthread_join(t, NULL) == 0);
	free(params.sendbuf);
	free(recvbuf);
	free(params.records);
}

/*
 * Main.
 */

ATF_TP_ADD_TCS(tp)
{
	/* Basic creation and connection tests */
	ATF_TP_ADD_TC(tp, create_socket);
	ATF_TP_ADD_TC(tp, create_socketpair);
	ATF_TP_ADD_TC(tp, listen_unbound);
	ATF_TP_ADD_TC(tp, bind);
	ATF_TP_ADD_TC(tp, listen_bound);
	ATF_TP_ADD_TC(tp, connect);
	ATF_TP_ADD_TC(tp, accept);
	ATF_TP_ADD_TC(tp, fcntl_nonblock);
	ATF_TP_ADD_TC(tp, resize_buffers);
	ATF_TP_ADD_TC(tp, resize_connected_buffers);

	/* Unthreaded I/O tests */
	ATF_TP_ADD_TC(tp, send_recv);
	ATF_TP_ADD_TC(tp, send_recv_nonblocking);
	ATF_TP_ADD_TC(tp, send_recv_with_connect);
	ATF_TP_ADD_TC(tp, sendto_recvfrom);
	ATF_TP_ADD_TC(tp, send_before_accept);
	ATF_TP_ADD_TC(tp, send_to_closed);
	ATF_TP_ADD_TC(tp, implied_connect);
	ATF_TP_ADD_TC(tp, shutdown_send);
	ATF_TP_ADD_TC(tp, shutdown_send_sigpipe);
	ATF_TP_ADD_TC(tp, eagain_8k_8k);
	ATF_TP_ADD_TC(tp, eagain_8k_128k);
	ATF_TP_ADD_TC(tp, eagain_128k_8k);
	ATF_TP_ADD_TC(tp, eagain_128k_128k);
	ATF_TP_ADD_TC(tp, sendrecv_8k);
	ATF_TP_ADD_TC(tp, sendrecv_16k);
	ATF_TP_ADD_TC(tp, sendrecv_32k);
	ATF_TP_ADD_TC(tp, sendrecv_64k);
	ATF_TP_ADD_TC(tp, sendrecv_128k);
	ATF_TP_ADD_TC(tp, sendrecv_8k_nonblocking);
	ATF_TP_ADD_TC(tp, sendrecv_16k_nonblocking);
	ATF_TP_ADD_TC(tp, sendrecv_32k_nonblocking);
	ATF_TP_ADD_TC(tp, sendrecv_64k_nonblocking);
	ATF_TP_ADD_TC(tp, sendrecv_128k_nonblocking);
	ATF_TP_ADD_TC(tp, rcvbuf_oversized);
	ATF_TP_ADD_TC(tp, pipe_simulator_8k_8k);
	ATF_TP_ADD_TC(tp, pipe_simulator_8k_128k);
	ATF_TP_ADD_TC(tp, pipe_simulator_128k_8k);
	ATF_TP_ADD_TC(tp, pipe_simulator_128k_128k);

	/* Threaded I/O tests with blocking sockets */
	ATF_TP_ADD_TC(tp, pipe_8k_8k);
	ATF_TP_ADD_TC(tp, pipe_8k_128k);
	ATF_TP_ADD_TC(tp, pipe_128k_8k);
	ATF_TP_ADD_TC(tp, pipe_128k_128k);
	ATF_TP_ADD_TC(tp, random_eor_and_waitall);

	return atf_no_error();
}
