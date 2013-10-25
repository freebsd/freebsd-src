/*-
 * Copyright (c) 2013 Philip Withnall
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/endian.h>
#include <sys/ioccom.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <machine/cheri.h>

#include "wayland-itc-buffer.h"

/**
 * This measures the performance difference between inter-process and
 * inter-thread communcations (IPC and ITC) using UNIX sockets and
 * capability-based FIFOs, respectively.
 *
 * When testing IPC, a local UNIX socket pair is created, then the program forks
 * and runs tests between the parent and child process. When testing ITC, a
 * capability FIFO is created, then the program spawns a thread and runs tests
 * between the main and spawned thread.
 *
 * Note that client output is sent to stderr and server output is sent to stdout
 * so they don’t fight.
 */

/* Test type: IPC or ITC. */
typedef enum {
	TEST_IPC,
	TEST_ITC
} test_type_t;

/* Generic structure representing the communication medium in use: socket or
 * capability FIFO. sock is used iff the test type is TEST_IPC; buf is used
 * otherwise. */
typedef union {
	int sock;
	struct {
		struct wl_itc_buffer *buf;
		struct chericap *buf_cap;
	} buf;
} test_pipe_t;

/* Message sizes to test (in bytes). */
static const size_t data_sizes[] = {
	1,
	16,
	32,
	64,
	128,
	256,
	512,
	1024,
	2048,
	4096,
	8192,
};

static const char *
format_test_type(test_type_t test_type)
{
	switch (test_type) {
	case TEST_IPC:
		return "ipc";
	case TEST_ITC:
		return "itc";
	default:
		return NULL;
	}
}

static void
print_csv_header(FILE *fd, int argc, const char * const *argv)
{
	char formatted_date[50];
	time_t _time;
	struct tm *__time;
	int i;

	_time = time(NULL);
	__time = localtime(&_time);
	strftime(formatted_date, sizeof(formatted_date),
	         "%Y-%m-%d %H:%M:%S", __time);

	fprintf(fd, "# Generated on %s by `", formatted_date);

	for (i = 0; i < argc; i++) {
		fprintf(fd, (i > 0) ? " %s" : "%s", argv[i]);
	}
	fprintf(fd, "`\n");

	fprintf(fd, "test, test_type, endpoint, total_time, num_round_trips, "
	            "message_size\n");
}

static void
print_csv_results_line(FILE *fd, const char *test_name, test_type_t test_type,
                       const char *endpoint, uint64_t total_time,
                       unsigned int num_round_trips, size_t message_size)
{
	fprintf(fd, "%s, %s, %s, %.5f, %u, %lu\n", test_name,
	        format_test_type(test_type), endpoint, total_time / 1000000000.0,
	        num_round_trips, message_size);
}

/* Function prototype for a test method. */
typedef int (*test_func_t) (test_type_t test_type, unsigned int n_repeats,
                            test_pipe_t out_p, test_pipe_t in_p,
                            void *test_data_out, size_t test_data_out_len,
                            void *test_data_in, size_t test_data_in_len);

struct client_params_t {
	test_type_t test_type;
	unsigned int n_repeats;
	test_func_t client_func;
	struct wl_itc_buffer *buf_in;
	struct chericap *buf_in_cap;
	struct wl_itc_buffer *buf_out;
	struct chericap *buf_out_cap;
	void *test_data_out;
	size_t test_data_out_len;
	void *test_data_in;
	size_t test_data_in_len;
};

/**
 * Thread callback for the client. The return value of the test function being
 * run is always ignored (the server’s return value is used instead).
 *
 * This expects a client_params_t as its argument, which it takes ownership of
 * and frees.
 */
static void *
client_thread_cb (void *arg)
{
	struct client_params_t *params = arg;
	test_pipe_t p1, p2;
	int retval;

	p1.buf.buf = params->buf_in;
	p1.buf.buf_cap = params->buf_in_cap;
	p2.buf.buf = params->buf_out;
	p2.buf.buf_cap = params->buf_out_cap;

	retval = params->client_func(params->test_type, params->n_repeats,
	                             p1, p2, params->test_data_out,
	                             params->test_data_out_len,
	                             params->test_data_in,
	                             params->test_data_in_len);

	/* Tidy up. */
	wl_itc_buffer_destroy(params->buf_in, params->buf_in_cap);
	wl_itc_buffer_destroy(params->buf_out, params->buf_out_cap);

	free(params->buf_in_cap);
	free(params->buf_out_cap);

	free(params);

	return NULL;
}

/**
 * Creates a communications medium then forks the process or spawns a thread
 * according to the current test_type, and runs server_func in one and
 * client_func in the other. This function then waits for the child process to
 * die or the client thread to finish; and returns the value returned by the
 * server process/thread.
 *
 * test_data_out[_len] gives a buffer of test message data for the client to
 * read. test_data_in[_len] gives a buffer of the same length for the server to
 * write to.
 */
static int
fork_tests(test_type_t test_type, unsigned int n_repeats, void *test_data_out,
           size_t test_data_out_len, void *test_data_in,
           size_t test_data_in_len,
           test_func_t server_func, test_func_t client_func)
{
	switch (test_type) {
	case TEST_IPC: {
		pid_t p;
		int s1[2], s2[2];
		int retval;
		test_pipe_t p1, p2;

		/* Create two one-way sockets between the server and client. */
		if (socketpair(AF_UNIX, SOCK_STREAM, 0, s1) != 0 ||
		    socketpair(AF_UNIX, SOCK_STREAM, 0, s2) != 0) {
			fprintf(stderr, "Error creating sockets: %s\n",
			        strerror(errno));
			return 1;
		}

		p = fork();

		if (p == -1) {
			fprintf(stderr, "Error forking process: %s\n",
			        strerror(errno));
			retval = 1;
		} else if (p == 0) {
			/* Child/Client process. */
			p1.sock = s1[0];
			p2.sock = s2[0];

			retval = client_func(test_type, n_repeats, p1, p2,
			                     test_data_out, test_data_out_len,
			                     test_data_in, test_data_in_len);

			/* http://stackoverflow.com/questions/10794899/\
			 * fork-output-with-waitpid */
			fflush(stdout);
			fflush(stderr);

			exit(retval);
		} else {
			/* Parent/Server process. */
			p1.sock = s1[1];
			p2.sock = s2[1];

			retval = server_func(test_type, n_repeats, p2, p1,
			                     test_data_out, test_data_out_len,
			                     test_data_in, test_data_in_len);

			/* http://stackoverflow.com/questions/10794899/\
			 * fork-output-with-waitpid */
			fflush(stdout);
			fflush(stderr);

			waitpid(p, NULL, 0);
		}

		close(s1[0]);
		close(s2[0]);
		close(s1[1]);
		close(s2[1]);

		return retval;
	}
	case TEST_ITC: {
		struct wl_itc_buffer *buf1, *buf2;
		struct chericap *buf1_cap, *buf2_cap;
		pthread_t client_thread;
		struct client_params_t *client_params;
		int retval;
		test_pipe_t p1, p2;

		if (posix_memalign((void **) &buf1_cap, CHERICAP_SIZE,
		                   sizeof(*buf1_cap)) != 0 ||
		    posix_memalign((void **) &buf2_cap, CHERICAP_SIZE,
		                   sizeof(*buf2_cap)) != 0) {
			fprintf(stderr, "Error allocating capabilities: %s\n",
			        strerror(errno));
			return 1;
		}

		/* Create two one-way buffers between the server and client.
		 * These are created with two references each (one for each end
		 * of the buffer). */
		buf1 = wl_itc_buffer_create(0, buf1_cap);
		buf2 = wl_itc_buffer_create(0, buf2_cap);

		/* Set up the client’s parameters. */
		client_params = malloc(sizeof(*client_params));
		if (client_params == NULL) {
			fprintf(stderr, "Error allocating client_params: %s\n",
			        strerror(errno));

			free(buf1_cap);
			free(buf2_cap);

			return 1;
		}

		client_params->buf_in = buf1;
		client_params->buf_in_cap = buf1_cap;
		client_params->buf_out = buf2;
		client_params->buf_out_cap = buf2_cap;
		client_params->test_type = test_type;
		client_params->n_repeats = n_repeats;
		client_params->client_func = client_func;
		client_params->test_data_out = test_data_out;
		client_params->test_data_out_len = test_data_out_len;
		client_params->test_data_in = test_data_in;
		client_params->test_data_in_len = test_data_in_len;

		/* Launch the client. Ownership of client_params is transferred
		 * to the thread. */
		if (pthread_create(&client_thread, NULL, client_thread_cb,
		                   client_params) != 0) {
			fprintf(stderr, "Error creating thread: %s\n",
			        strerror(errno));

			wl_itc_buffer_destroy(buf1, buf1_cap);
			wl_itc_buffer_destroy(buf2, buf2_cap);
			wl_itc_buffer_destroy(buf1, buf1_cap);
			wl_itc_buffer_destroy(buf2, buf2_cap);

			free(buf1_cap);
			free(buf2_cap);

			free(client_params);

			return 1;
		}

		/* Run the server and wait for the client to terminate. */
		p1.buf.buf = buf1;
		p1.buf.buf_cap = buf1_cap;
		p2.buf.buf = buf2;
		p2.buf.buf_cap = buf2_cap;

		retval = server_func(test_type, n_repeats, p2, p1,
		                     test_data_out, test_data_out_len,
		                     test_data_in, test_data_in_len);

		wl_itc_buffer_destroy(buf1, buf1_cap);
		wl_itc_buffer_destroy(buf2, buf2_cap);

		pthread_join(client_thread, NULL);

		return retval;
	}
	default:
		return 1;
	}
}

/* Send a message from one process/thread to the other. This is non-blocking;
 * messages are always sent asynchronously. test_data_out[_len] is used as the
 * message body. 1 is returned in case of error; 0 otherwise. */
static int
send_message(test_type_t test_type, test_pipe_t p, void *test_data_out,
             size_t test_data_out_len)
{
	struct iovec iov;
	struct msghdr msg;
	ssize_t len;

	/* Set up a transmit buffer. */
	iov.iov_base = test_data_out;
	iov.iov_len = test_data_out_len;

	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	msg.msg_flags = 0;

	switch (test_type) {
	case TEST_IPC:
		len = sendmsg(p.sock, &msg, MSG_NOSIGNAL | MSG_DONTWAIT);
		break;
	case TEST_ITC: {
		do {
			len = wl_itc_buffer_sendmsg(p.buf.buf, p.buf.buf_cap,
			                            &msg,
			                            MSG_NOSIGNAL |
			                            MSG_DONTWAIT);
		} while (len == -1 &&
		         (errno == EAGAIN || errno == EWOULDBLOCK));

		break;
	}
	default:
		return 1;
	}

	/* Handle errors. */
	if (len == -1) {
		fprintf(stderr, "Error sending message: %s\n", strerror(errno));
		return 1;
	} else if ((size_t) len != test_data_out_len) {
		fprintf(stderr, "Error sending full message.\n");
		return 1;
	}

	return 0;
}

/* Receive a message on a process/thread. This is blocking; messages are always
 * received synchronously. test_data_in[_len] is used as the input buffer for
 * the message. 1 is returned in case of error; 0 otherwise. */
static int
receive_message(test_type_t test_type, test_pipe_t p, void *test_data_in,
                size_t test_data_in_len)
{
	struct iovec iov;
	struct msghdr msg;
	ssize_t len;

	/* Set up a receive buffer. */
	iov.iov_base = test_data_in;
	iov.iov_len = test_data_in_len;

	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	msg.msg_flags = 0;

	/* Receive a message. */
	switch (test_type) {
	case TEST_IPC:
		len = recvmsg(p.sock, &msg, 0);
		break;
	case TEST_ITC:
		len = wl_itc_buffer_recvmsg(p.buf.buf, p.buf.buf_cap, &msg, 0);
		break;
	default:
		return 1;
	}

	/* Handle errors. */
	if (len == -1) {
		fprintf(stderr, "Error receiving message: %s\n",
		        strerror(errno));
		return 1;
	} else if ((size_t) len != test_data_in_len) {
		fprintf(stderr, "Error receiving full message (%lu vs %lu).\n",
		        len, test_data_in_len);
		return 1;
	}

	return 0;
}

/* Server half of test_latency(). */
static int
test_latency_server(test_type_t test_type, unsigned int n_repeats,
                    test_pipe_t out_p, test_pipe_t in_p, void *test_data_out,
                    size_t test_data_out_len, void *test_data_in,
                    size_t test_data_in_len)
{
	unsigned int i;
	struct timespec tp_start, tp_end;
	uint64_t total_time;

	if (clock_gettime(CLOCK_REALTIME_PRECISE, &tp_start) != 0) {
		fprintf(stderr, "Failed to get time: %s\n",
		        strerror(errno));
		return 1;
	}

	/* Receive n_repeats requests and send a response for each
	 * immediately. */
	for (i = 0; i < n_repeats; i++) {
		if (receive_message(test_type, in_p, test_data_in,
		                    test_data_in_len) != 0 ||
		    send_message(test_type, out_p, test_data_out,
		                 test_data_out_len) != 0) {
			fprintf(stderr, "Error receiving/sending message: %s\n",
			        strerror(errno));
			return 1;
		}
	}

	if (clock_gettime(CLOCK_REALTIME_PRECISE, &tp_end) != 0) {
		fprintf(stderr, "Failed to get time: %s\n",
		        strerror(errno));
		return 1;
	}

	/* Calculate results. */
	total_time =
		((uint64_t) tp_end.tv_sec * 1000000000 + tp_end.tv_nsec) -
		((uint64_t) tp_start.tv_sec * 1000000000 + tp_start.tv_nsec);

	/* Print out the results, formally. */
	print_csv_results_line(stdout, "latency", test_type, "server",
	                       total_time, n_repeats, test_data_out_len);

	return 0;
}

/* Client half of test_latency(). */
static int
test_latency_client(test_type_t test_type, unsigned int n_repeats,
                    test_pipe_t out_p, test_pipe_t in_p, void *test_data_out,
                    size_t test_data_out_len, void *test_data_in,
                    size_t test_data_in_len)
{
	unsigned int i;
	struct timespec tp_start, tp_end;
	uint64_t total_time;

	if (clock_gettime(CLOCK_REALTIME_PRECISE, &tp_start) != 0) {
		fprintf(stderr, "Failed to get time: %s\n",
		        strerror(errno));
		return 1;
	}

	/* Send n_repeats requests, waiting for a response from each one before
	 * sending the next. */
	for (i = 0; i < n_repeats; i++) {
		if (send_message(test_type, out_p, test_data_out,
		                 test_data_out_len) != 0 ||
		    receive_message(test_type, in_p, test_data_in,
		                    test_data_in_len) != 0) {
			fprintf(stderr, "Error sending/receiving message: %s\n",
			        strerror(errno));
			return 1;
		}
	}

	if (clock_gettime(CLOCK_REALTIME_PRECISE, &tp_end) != 0) {
		fprintf(stderr, "Failed to get time: %s\n",
		        strerror(errno));
		return 1;
	}

	/* Calculate results. */
	total_time =
		((uint64_t) tp_end.tv_sec * 1000000000 + tp_end.tv_nsec) -
		((uint64_t) tp_start.tv_sec * 1000000000 + tp_start.tv_nsec);

	/* Print out the results, formally. */
	print_csv_results_line(stderr, "latency", test_type, "client",
	                       total_time, n_repeats, test_data_out_len);

	return 0;
}

/* Test communications latency. This sends n_repeats requests from client to
 * server, waiting for a response from each before sending the next.
 * test_data_out[_len] is used as the message data; test_data_in[_len] is a
 * buffer for receiving messages into.
 *
 * A timer is started before sending the first message, and stopped after
 * receiving the response for the final message. */
static int
test_latency(test_type_t test_type, unsigned int n_repeats, void *test_data_out,
                    size_t test_data_out_len, void *test_data_in,
                    size_t test_data_in_len)
{
	return fork_tests(test_type, n_repeats, test_data_out,
	                  test_data_out_len, test_data_in, test_data_in_len,
	                  test_latency_server, test_latency_client);
}

/* Server half of test_bandwidth(). */
static int
test_bandwidth_server(test_type_t test_type, unsigned int n_repeats,
                      test_pipe_t out_p __unused, test_pipe_t in_p,
                      void *test_data_out __unused,
                      size_t test_data_out_len __unused,
                      void *test_data_in, size_t test_data_in_len)
{
	unsigned int i;
	struct timespec tp_start, tp_end;
	uint64_t total_time;

	if (clock_gettime(CLOCK_REALTIME_PRECISE, &tp_start) != 0) {
		fprintf(stderr, "Failed to get time: %s\n",
		        strerror(errno));
		return 1;
	}

	/* Receive n_repeats requests. */
	for (i = 0; i < n_repeats; i++) {
		if (receive_message(test_type, in_p, test_data_in,
		                    test_data_in_len) != 0) {
			fprintf(stderr, "Error receiving/sending message: %s\n",
			        strerror(errno));
			return 1;
		}
	}

	if (clock_gettime(CLOCK_REALTIME_PRECISE, &tp_end) != 0) {
		fprintf(stderr, "Failed to get time: %s\n",
		        strerror(errno));
		return 1;
	}

	/* Calculate results. */
	total_time =
		((uint64_t) tp_end.tv_sec * 1000000000 + tp_end.tv_nsec) -
		((uint64_t) tp_start.tv_sec * 1000000000 + tp_start.tv_nsec);

	/* Print out the results, formally. */
	print_csv_results_line(stdout, "bandwidth", test_type, "server",
	                       total_time, n_repeats, test_data_in_len);

	return 0;
}

/* Client half of test_bandwidth(). */
static int
test_bandwidth_client(test_type_t test_type, unsigned int n_repeats,
                      test_pipe_t out_p, test_pipe_t in_p __unused,
                      void *test_data_out, size_t test_data_out_len,
                      void *test_data_in __unused,
                      size_t test_data_in_len __unused)
{
	unsigned int i;
	struct timespec tp_start, tp_end;
	uint64_t total_time;

	if (clock_gettime(CLOCK_REALTIME_PRECISE, &tp_start) != 0) {
		fprintf(stderr, "Failed to get time: %s\n",
		        strerror(errno));
		return 1;
	}

	/* Send n_repeats requests without waiting for a response for any of
	 * them. */
	for (i = 0; i < n_repeats; i++) {
		if (send_message(test_type, out_p, test_data_out,
		                 test_data_out_len) != 0) {
			fprintf(stderr, "Error sending/receiving message: %s\n",
			        strerror(errno));
			return 1;
		}
	}

	if (clock_gettime(CLOCK_REALTIME_PRECISE, &tp_end) != 0) {
		fprintf(stderr, "Failed to get time: %s\n",
		        strerror(errno));
		return 1;
	}

	/* Calculate results. */
	total_time =
		((uint64_t) tp_end.tv_sec * 1000000000 + tp_end.tv_nsec) -
		((uint64_t) tp_start.tv_sec * 1000000000 + tp_start.tv_nsec);

	/* Print out the results, formally. */
	print_csv_results_line(stderr, "bandwidth", test_type, "client",
	                       total_time, n_repeats, test_data_out_len);

	return 0;
}

/* Test communications bandwidth. This sends n_repeats messages from client to
 * server without waiting for responses to them.
 * test_data_out[_len] is used as the message data; test_data_in[_len] is a
 * buffer for receiving messages into.
 *
 * A timer is started before sending the first message and stopped after sending
 * the final message. */
static int
test_bandwidth(test_type_t test_type, unsigned int n_repeats,
               void *test_data_out, size_t test_data_out_len,
               void *test_data_in, size_t test_data_in_len)
{
	return fork_tests(test_type, n_repeats, test_data_out,
	                  test_data_out_len, test_data_in, test_data_in_len,
	                  test_bandwidth_server, test_bandwidth_client);
}

/* Server half of test_variance(). */
static int
test_variance_server(test_type_t test_type __unused, unsigned int n_repeats __unused,
                     test_pipe_t out_p __unused, test_pipe_t in_p __unused,
                     void *test_data_out, size_t test_data_out_len,
                     void *test_data_in, size_t test_data_in_len)
{
	unsigned int i;

	/* Receive n_repeats requests and send a response for each
	 * immediately. */
	for (i = 0; i < n_repeats; i++) {
		struct timespec tp_start, tp_end;
		uint64_t total_time;

		if (clock_gettime(CLOCK_REALTIME_PRECISE, &tp_start) != 0) {
			fprintf(stderr, "Failed to get time: %s\n",
			        strerror(errno));
			return 1;
		}

		if (receive_message(test_type, in_p, test_data_in,
		                    test_data_in_len) != 0 ||
		    send_message(test_type, out_p, test_data_out,
		                 test_data_out_len) != 0) {
			fprintf(stderr, "Error receiving/sending message: %s\n",
			        strerror(errno));
			return 1;
		}

		if (clock_gettime(CLOCK_REALTIME_PRECISE, &tp_end) != 0) {
			fprintf(stderr, "Failed to get time: %s\n",
			        strerror(errno));
			return 1;
		}

		/* Calculate results. */
		total_time =
			((uint64_t) tp_end.tv_sec * 1000000000 + tp_end.tv_nsec) -
			((uint64_t) tp_start.tv_sec * 1000000000 + tp_start.tv_nsec);

		/* Print out the results, formally. */
		print_csv_results_line(stdout, "variance", test_type, "server",
		                       total_time, 1, test_data_out_len);
	}

	return 0;
}

/* Client half of test_variance(). */
static int
test_variance_client(test_type_t test_type, unsigned int n_repeats,
                     test_pipe_t out_p, test_pipe_t in_p, void *test_data_out,
                     size_t test_data_out_len, void *test_data_in,
                     size_t test_data_in_len)
{
	unsigned int i;

	/* Send/Receive n_repeats request/response pairs. */
	for (i = 0; i < n_repeats; i++) {
		struct timespec tp_start, tp_end;
		uint64_t total_time;

		if (clock_gettime(CLOCK_REALTIME_PRECISE, &tp_start) != 0) {
			fprintf(stderr, "Failed to get time: %s\n",
			        strerror(errno));
			return 1;
		}

		if (send_message(test_type, out_p, test_data_out,
		                 test_data_out_len) != 0 ||
		    receive_message(test_type, in_p, test_data_in,
		                    test_data_in_len) != 0) {
			fprintf(stderr, "Error sending/receiving message: %s\n",
			        strerror(errno));
			return 1;
		}

		if (clock_gettime(CLOCK_REALTIME_PRECISE, &tp_end) != 0) {
			fprintf(stderr, "Failed to get time: %s\n",
			        strerror(errno));
			return 1;
		}

		/* Calculate results. */
		total_time =
			((uint64_t) tp_end.tv_sec * 1000000000 + tp_end.tv_nsec) -
			((uint64_t) tp_start.tv_sec * 1000000000 + tp_start.tv_nsec);

		/* Print out the results, formally. */
		print_csv_results_line(stderr, "variance", test_type, "client",
		                       total_time, 1, test_data_out_len);
	}

	return 0;
}

/* Test variance of communications latency. This is similar to the
 * test_latency() test, but timing is performed for each request–response pair,
 * instead of over all request–response pairs. */
static int
test_variance(test_type_t test_type, unsigned int n_repeats, void *test_data_out,
              size_t test_data_out_len, void *test_data_in,
              size_t test_data_in_len)
{
	return fork_tests(test_type, n_repeats, test_data_out,
	                  test_data_out_len, test_data_in, test_data_in_len,
	                  test_variance_server, test_variance_client);
}

/* Returns a main()-style error number. */
static int
usage(void)
{
	printf("communications-performance ipc|itc [n_repeats]\n\n");
	printf("This runs a series of communications performance tests, "
	       "outputting a CSV file of results to standard "
	       "output.\n\n");
	printf("The mandatory first parameter specifies whether inter-process "
	       "or inter-thread communications should be tested.\n");
	printf("The optional second parameter specifies the number of repeats "
	       "to perform for each test.\n");

	return 1;
}

int
main(int argc, char *argv[])
{
	int retval = 0;
	int n_repeats;
	test_type_t test_type;
	unsigned int i;

	if (argc < 2) {
		return usage();
	}

	/* Parse the test type. */
	if (strcmp(argv[1], "ipc") == 0) {
		test_type = TEST_IPC;
	} else if (strcmp(argv[1], "itc") == 0) {
		test_type = TEST_ITC;
	} else {
		fprintf(stderr, "Invalid test type ‘%s’.\n", argv[1]);
		return usage();
	}

	/* Parse the number of repeats (or leave it at the default: -1). */
	if (argc >= 3) {
		char *endptr;

		n_repeats = strtol(argv[2], &endptr, 0);
		if (*endptr != '\0' || n_repeats < 1) {
			fprintf(stderr, "Invalid repeat count ‘%s’.\n",
			        argv[2]);
			return usage();
		}
	} else {
		n_repeats = -1;
	}

	/* Defaults. */
	if (n_repeats == -1) {
		n_repeats = 1000;
	}

	/* Output the CSV header. */
	print_csv_header(stdout, argc, (const char * const *) argv);
	print_csv_header(stderr, argc, (const char * const *) argv);

	/* Repeat for different message sizes. */
	for (i = 0; i < sizeof(data_sizes) / sizeof(*data_sizes); i++) {
		static void *test_data_out, *test_data_in;
		size_t data_size = data_sizes[i];

		/* Allocate the test data. */
		test_data_out = malloc(data_size);
		test_data_in = malloc(data_size);

		/* Run the tests. */
		/*fprintf(stderr, "Running ‘latency’ test for %luB messages…\n",
		        data_size);*/
		retval = test_latency(test_type, n_repeats, test_data_out,
		                      data_size, test_data_in, data_size);
		if (retval != 0) goto done_loop;

		/*fprintf(stderr,
		        "Running ‘bandwidth’ test for %luB messages…\n",
		        data_size);*/
		retval = test_bandwidth(test_type, n_repeats, test_data_out,
		                        data_size, test_data_in, data_size);
		if (retval != 0) goto done_loop;

		/*fprintf(stderr, "Running ‘variance’ test for %luB messages…\n",
		        data_size);*/
		retval = test_variance(test_type, n_repeats, test_data_out,
		                       data_size, test_data_in, data_size);
		if (retval != 0) goto done_loop;

done_loop:
		free(test_data_in);
		free(test_data_out);
	}

	return retval;
}
