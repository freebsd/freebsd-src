/*-
 * Copyright (c) 2004 Robert N. M. Watson
 * All rights reserved.
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
 *
 * $FreeBSD$
 */

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * The test tool exercises IP-level socket options by interrogating the
 * getsockopt()/setsockopt() APIs.  It does not currently test that the
 * intended semantics of each option are implemented (i.e., that setting IP
 * options on the socket results in packets with the desired IP options in
 * it).
 */

/*
 * Exercise the IP_OPTIONS socket option.  Confirm the following properties:
 *
 * - That there is no initial set of options (length returned is 0).
 * - That if we set a specific set of options, we can read it back.
 * - That if we then reset the options, they go away.
 *
 * Use a UDP socket for this.
 */
static void
test_ip_options(void)
{
	u_int32_t new_options, test_options[2];
	socklen_t len;
	int sock;

	sock = socket(PF_INET, SOCK_DGRAM, 0);
	if (sock == -1)
		err(-1, "test_ip_options: socket(SOCK_DGRAM)");

	/*
	 * Start off by confirming the default IP options on a socket are to
	 * have no options set.
	 */
	len = sizeof(test_options);
	if (getsockopt(sock, IPPROTO_IP, IP_OPTIONS, test_options, &len) < 0)
		err(-1, "test_ip_options: initial getsockopt()");

	if (len != 0)
		errx(-1, "test_ip_options: initial getsockopt() returned "
		    "%d bytes", len);

#define	TEST_MAGIC	0xc34e4212
#define	NEW_OPTIONS	htonl(IPOPT_EOL | (IPOPT_NOP << 8) | (IPOPT_NOP << 16) \
			 | (IPOPT_NOP << 24))

	/*
	 * Write some new options into the socket.
	 */
	new_options = NEW_OPTIONS;
	if (setsockopt(sock, IPPROTO_IP, IP_OPTIONS, &new_options,
	    sizeof(new_options)) < 0)
		err(-1, "test_ip_options: setsockopt(NOP|NOP|NOP|EOL)");

	/*
	 * Store some random cruft in a local variable and retrieve the
	 * options to make sure they set.  Note that we pass in an array
	 * of u_int32_t's so that if whatever ended up in the option was
	 * larger than what we put in, we find out about it here.
	 */
	test_options[0] = TEST_MAGIC;
	test_options[1] = TEST_MAGIC;
	len = sizeof(test_options);
	if (getsockopt(sock, IPPROTO_IP, IP_OPTIONS, test_options, &len) < 0)
		err(-1, "test_ip_options: getsockopt() after set");

	/*
	 * Getting the right amount back is important.
	 */
	if (len != sizeof(new_options))
		errx(-1, "test_ip_options: getsockopt() after set returned "
		    "%d bytes of data", len);

	/*
	 * One posible failure mode is that the call succeeds but neglects to
	 * copy out the data.
 	 */
	if (test_options[0] == TEST_MAGIC)
		errx(-1, "test_ip_options: getsockopt() after set didn't "
		    "return data");

	/*
	 * Make sure we get back what we wrote on.
	 */
	if (new_options != test_options[0])
		errx(-1, "test_ip_options: getsockopt() after set returned "
		    "wrong options (%08x, %08x)", new_options,
		    test_options[0]);

	/*
	 * Now we reset the value to make sure clearing works.
	 */
	if (setsockopt(sock, IPPROTO_IP, IP_OPTIONS, NULL, 0) < 0)
		err(-1, "test_ip_options: setsockopt() to reset");

	/*
	 * Make sure it was really cleared.
	 */
	test_options[0] = TEST_MAGIC;
	test_options[1] = TEST_MAGIC;
	len = sizeof(test_options);
	if (getsockopt(sock, IPPROTO_IP, IP_OPTIONS, test_options, &len) < 0)
		err(-1, "test_ip_options: getsockopt() after reset");

	if (len != 0)
		errx(-1, "test_ip_options: getsockopt() after reset returned "
		    "%d bytes", len);

	close(sock);
}

/*
 * This test checks the behavior of the IP_HDRINCL socket option, which
 * allows users with privilege to specify the full header on an IP raw
 * socket.  We test that the option can only be used with raw IP sockets, not
 * with UDP or TCP sockets.  We also confirm that the raw socket is only
 * available to a privileged user (subject to the UID when called).  We
 * confirm that it defaults to off
 */
static void
test_ip_hdrincl(void)
{
	int flag[2], sock;
	socklen_t len;

	/*
	 * Try to receive or set the IP_HDRINCL flag on a TCP socket.
	 */
	sock = socket(PF_INET, SOCK_STREAM, 0);
	if (sock == -1) {
		perror("socket");
		exit(-1);
	}

	flag[0] = -1;
	len = sizeof(flag[0]);
	if (getsockopt(sock, IPPROTO_IP, IP_HDRINCL, flag, &len) == 0) {
		fprintf(stderr, "getsockopt(IP_HDRINCL) on TCP succeeded\n");
		exit(-1);
	}

	if (errno != ENOPROTOOPT) {
		fprintf(stderr, "getsockopt(IP_HDRINCL) on TCP returned %d "
		    "(%s) not ENOPROTOOPT\n", errno, strerror(errno));
		exit(-1);
	}

	flag[0] = 1;
	if (setsockopt(sock, IPPROTO_IP, IP_HDRINCL, flag, sizeof(flag[0]))
	    == 0) {
		fprintf(stderr, "setsockopt(IP_HDRINCL) on TCP succeeded\n");
		exit(-1);
	}

	if (errno != ENOPROTOOPT) {
		fprintf(stderr, "setsockopt(IP_HDRINCL) on TCP returned %d "
		    "(%s) not ENOPROTOOPT\n", errno, strerror(errno));
		exit(-1);
	}

	close(sock);

	/*
	 * Try to receive or set the IP_HDRINCL flag on a UDP socket.
	 */
	sock = socket(PF_INET, SOCK_DGRAM, 0);
	if (sock == -1) {
		perror("socket");
		exit(-1);
	}

	flag[0] = -1;
	len = sizeof(flag[0]);
	if (getsockopt(sock, IPPROTO_IP, IP_HDRINCL, flag, &len) == 0) {
		fprintf(stderr, "getsockopt(IP_HDRINCL) on UDP succeeded\n");
		exit(-1);
	}

	if (errno != ENOPROTOOPT) {
		fprintf(stderr, "getsockopt(IP_HDRINCL) on UDP returned %d "
		    "(%s) not ENOPROTOOPT\n", errno, strerror(errno));
		exit(-1);
	}

	if (setsockopt(sock, IPPROTO_IP, IP_HDRINCL, flag, sizeof(flag[0]))
	    == 0) {
		fprintf(stderr, "setsockopt(IP_HDRINCL) on UDPsucceeded\n");
		exit(-1);
	}

	if (errno != ENOPROTOOPT) {
		fprintf(stderr, "setsockopt(IP_HDRINCL) on UDP returned %d "
		    "(%s) not ENOPROTOOPT\n", errno, strerror(errno));
		exit(-1);
	}

	close(sock);

	/*
	 * Now try on a raw socket.  Access ontrol should prevent non-root
	 * users from creating the raw socket, so check that here based on
	 * geteuid().  If we're non-root, we just return assuming the socket
	 * create fails since the remainder of the tests apply only on a raw
	 * socket.
	 */
	sock = socket(PF_INET, SOCK_RAW, 0);
	if (geteuid() != 0) {
		if (sock != -1)
			errx(-1, "test_ip_hdrincl: created raw socket as "
			    "uid %d", geteuid());
		return;
	}
	if (sock == -1) {
		perror("test_ip_hdrincl: socket(PF_INET, SOCK_RAW)");
		exit(-1);
	}

	/*
	 * Make sure the initial value of the flag is 0 (disabled).
	 */
	flag[0] = -1;
	flag[1] = -1;
	len = sizeof(flag);
	if (getsockopt(sock, IPPROTO_IP, IP_HDRINCL, flag, &len) < 0) {
		perror("test_ip_hdrincl: getsockopt(IP_HDRINCL) on raw");
		exit(-1);
	}

	if (len != sizeof(flag[0])) {
		fprintf(stderr, "test_ip_hdrincl: %d bytes returned on "
		    "initial get\n", len);
		exit(-1);
	}

	if (flag[0] != 0) {
		fprintf(stderr, "test_ip_hdrincl: initial flag value of %d\n",
		    flag[0]);
		exit(-1);
	}

	/*
	 * Enable the IP_HDRINCL flag.
	 */
	flag[0] = 1;
	if (setsockopt(sock, IPPROTO_IP, IP_HDRINCL, flag, sizeof(flag[0]))
	    < 0) {
		perror("test_ip_hdrincl: setsockopt(IP_HDRINCL, 1)");
		exit(-1);
	}

	/*
	 * Check that the IP_HDRINCL flag was set.
	 */
	flag[0] = -1;
	flag[1] = -1;
	len = sizeof(flag);
	if (getsockopt(sock, IPPROTO_IP, IP_HDRINCL, flag, &len) < 0) {
		perror("test_ip_hdrincl: getsockopt(IP_HDRINCL) after set");
		exit(-1);
	}

	if (flag[0] == 0) {
		fprintf(stderr, "test_ip_hdrincl: getsockopt(IP_HDRINCL) "
		    "after set had flag of %d\n", flag[0]);
		exit(-1);
	}

#define	HISTORICAL_INP_HDRINCL	8
	if (flag[0] != HISTORICAL_INP_HDRINCL) {
		fprintf(stderr, "test_ip_hdrincl: WARNING: getsockopt(IP_H"
		    "DRINCL) after set had non-historical value of %d\n",
		    flag[0]);
	}

	/*
	 * Reset the IP_HDRINCL flag to 0.
	 */
	flag[0] = 0;
	if (setsockopt(sock, IPPROTO_IP, IP_HDRINCL, flag, sizeof(flag[0]))
	    < 0) {
		perror("test_ip_hdrincl: setsockopt(IP_HDRINCL, 0)");
		exit(-1);
	}

	/*
	 * Check that the IP_HDRINCL flag was reset to 0.
	 */
	flag[0] = -1;
	flag[1] = -1;
	len = sizeof(flag);
	if (getsockopt(sock, IPPROTO_IP, IP_HDRINCL, flag, &len) < 0) {
		perror("test_ip_hdrincl: getsockopt(IP_HDRINCL) after reset");
		exit(-1);
	}

	if (flag[0] != 0) {
		fprintf(stderr, "test_ip_hdrincl: getsockopt(IP_HDRINCL) "
		    "after set had flag of %d\n", flag[0]);
		exit(-1);
	}

	close(sock);
}

/*
 * As with other non-int or larger sized socket options, the IP_TOS and
 * IP_TTL fields in kernel is stored as an 8-bit value, reflecting the IP
 * header fields, but useful I/O to the field occurs using 32-bit integers.
 * The FreeBSD kernel will permit writes from variables at least an int in
 * size (and ignore additional bytes), and will permit a read to buffers 1
 * byte or larger (but depending on endianness, may truncate out useful
 * values if the caller provides less room).
 *
 * Given the limitations of the API, use a UDP socket to confirm that the
 * following are true:
 *
 * - We can read the IP_TOS/IP_TTL options.
 * - The initial value of the TOS option is 0, TTL is 64.
 * - That if we provide more than 32 bits of storage, we get back only 32
 *   bits of data.
 * - When we set it to a non-zero value expressible with a u_char, we can
 *   read that value back.
 * - When we reset it back to zero, we can read it as 0.
 * - When we set it to a value >255, the value is truncated to something less
 *   than 255.
 */
static void
test_ip_uchar(int option, char *optionname, int initial)
{
	int sock, val[2];
	socklen_t len;

	sock = socket(PF_INET, SOCK_DGRAM, 0);
	if (sock == -1)
		err(-1, "test_ip_tosttl(%s): socket", optionname);

	/*
	 * Check that the initial value is 0, and that the size is one
	 * u_char;
	 */
	val[0] = -1;
	val[1] = -1;
	len = sizeof(val);
	if (getsockopt(sock, IPPROTO_IP, option, val, &len) < 0)
		err(-1, "test_ip_tosttl: initial getsockopt(%s)",
		    optionname);

	if (len != sizeof(val[0]))
		errx(-1, "test_ip_tosttl(%s): initial getsockopt() returned "
		    "%d bytes", optionname, len);

	if (val[0] == -1)
		errx(-1, "test_ip_tosttl(%s): initial getsockopt() didn't "
		    "return data", optionname);

	if (val[0] != initial)
		errx(-1, "test_ip_tosttl(%s): initial getsockopt() returned "
		   "value of %d, not %d", optionname, val[0], initial);

	/*
	 * Set the field to a valid value.
	 */
	val[0] = 128;
	val[1] = -1;
	if (setsockopt(sock, IPPROTO_IP, option, val, sizeof(val[0])) < 0)
		err(-1, "test_ip_tosttl(%s): setsockopt(128)", optionname);

	/*
	 * Check that when we read back the field, we get the same value.
	 */
	val[0] = -1;
	val[1] = -1;
	len = sizeof(val);
	if (getsockopt(sock, IPPROTO_IP, option, val, &len) < 0)
		err(-1, "test_ip_tosttl(%s): getsockopt() after set to 128",
		    optionname);

	if (len != sizeof(val[0]))
		errx(-1, "test_ip_tosttl(%s): getsockopt() after set to 128 "
		    "returned %d bytes", optionname, len);

	if (val[0] == -1)
		errx(-1, "test_ip_tosttl(%s): getsockopt() after set to 128 "
		    "didn't return data", optionname);

	if (val[0] != 128)
		errx(-1, "test_ip_tosttl(%s): getsockopt() after set to 128 "
		    "returned %d", optionname, val[0]);

	/*
	 * Reset the value to 0, check that it was reset.
	 */
	val[0] = 0;
	val[1] = 0;
	if (setsockopt(sock, IPPROTO_IP, option, val, sizeof(val[0])) < 0)
		err(-1, "test_ip_tosttl(%s): setsockopt() to reset from 128",
		    optionname);

	if (len != sizeof(val[0]))
		errx(-1, "test_ip_tosttl(%s): getsockopt() after reset from "
		    "128 returned %d bytes", optionname, len);

	if (val[0] == -1)
		errx(-1, "test_ip_tosttl(%s): getsockopt() after reset from "
		    "128 didn't return data", optionname);

	if (val[0] != 0)
		errx(-1, "test_ip_tosttl(%s): getsockopt() after reset from "
		    "128 returned %d", optionname, val[0]);

	/*
	 * Set the value to something out of range and check that it comes
	 * back truncated, or that we get EINVAL back.  Traditional u_char
	 * IP socket options truncate, but newer ones (such as multicast
	 * socket options) will return EINVAL.
	 */
	val[0] = 32000;
	val[1] = -1;
	if (setsockopt(sock, IPPROTO_IP, option, val, sizeof(val[0])) < 0) {
		/*
		 * EINVAL is a fine outcome, no need to run the truncation
		 * tests.
		 */
		if (errno == EINVAL) {
			close(sock);
			return;
		}
		err(-1, "test_ip_tosttl(%s): getsockopt(32000)", optionname);
	}

	val[0] = -1;
	val[1] = -1;
	len = sizeof(val);
	if (getsockopt(sock, IPPROTO_IP, option, val, &len) < 0)
		err(-1, "test_ip_tosttl(%s): getsockopt() after set to 32000",
		    optionname);

	if (len != sizeof(val[0]))
		errx(-1, "test_ip_tosttl(%s): getsockopt() after set to 32000"
		    "returned %d bytes",  optionname, len);

	if (val[0] == -1)
		errx(-1, "test_ip_tosttl(%s): getsockopt() after set to 32000"
		    "didn't return data", optionname);

	if (val[0] == 32000)
		errx(-1, "test_ip_tosttl(%s): getsockopt() after set to 32000"
		    "returned 32000: failed to truncate", optionname);

	close(sock);
}

/*
 * Generic test for a boolean socket option.  Caller provides the option
 * number, string name, expected default (initial) value, and whether or not
 * the option is root-only.  For each option, test:
 *
 * - That we can read the option.
 * - That the initial value is as expected.
 * - That we can modify the value.
 * - That on modification, the new value can be read back.
 * - That we can reset the value.
 * - that on reset, the new value can be read back.
 *
 * Test using a UDP socket.
 */
#define	BOOLEAN_ANYONE		1
#define	BOOLEAN_ROOTONLY	1
static void
test_ip_boolean(int option, char *optionname, int initial, int rootonly)
{
	int newvalue, sock, val[2];
	socklen_t len;

	sock = socket(PF_INET, SOCK_DGRAM, 0);
	if (sock == -1)
		err(-1, "test_ip_boolean(%s): socket", optionname);

	/*
	 * The default for a boolean might be true or false.  If it's false,
	 * we will try setting it to true (but using a non-1 value of true).
	 * If it's true, we'll set it to false.
	 */
	if (initial == 0)
		newvalue = 0xff;
	else
		newvalue = 0;

	val[0] = -1;
	val[1] = -1;
	len = sizeof(val);
	if (getsockopt(sock, IPPROTO_IP, option, val, &len) < 0)
		err(-1, "test_ip_boolean: initial getsockopt()");

	if (len != sizeof(val[0]))
		errx(-1, "test_ip_boolean(%s): initial getsockopt() returned "
		   "%d bytes", optionname, len);

	if (val[0] == -1)
		errx(-1, "test_ip_boolean(%s): initial getsockopt() didn't "
		    "return data", optionname);

	if (val[0] != initial)
		errx(-1, "test_ip_boolean(%s): initial getsockopt() returned "
		    "%d (expected %d)", optionname, val[0], initial);

	/*
	 * Set the socket option to a new non-default value.
	 */
	if (setsockopt(sock, IPPROTO_IP, option, &newvalue, sizeof(newvalue))
	    < 0)
		err(-1, "test_ip_boolean(%s): setsockopt() to %d", optionname,
		    newvalue);

	/*
	 * Read the value back and see if it is not the default (note: will
	 * not be what we set it to, as we set it to 0xff above).
	 */
	val[0] = -1;
	val[1] = -1;
	len = sizeof(val);
	if (getsockopt(sock, IPPROTO_IP, option, val, &len) < 0)
		err(-1, "test_ip_boolean(%s): getsockopt() after set to %d",
		    optionname, newvalue);

	if (len != sizeof(val[0]))
		errx(-1, "test_ip_boolean(%s): getsockopt() after set to %d "
		    "returned %d bytes", optionname, newvalue, len);

	if (val[0] == -1)
		errx(-1, "test_ip_boolean(%s): getsockopt() after set to %d "
		    "didn't return data", optionname, newvalue);

	/*
	 * If we set it to true, check for '1', otherwise '0.
	 */
	if (val[0] != (newvalue ? 1 : 0))
		errx(-1, "test_ip_boolean(%s): getsockopt() after set to %d "
		    "returned %d", optionname, newvalue, val[0]);

	/*
	 * Reset to initial value.
	 */
	newvalue = initial;
	if (setsockopt(sock, IPPROTO_IP, option, &newvalue, sizeof(newvalue))
	    < 0)
		err(-1, "test_ip_boolean(%s): setsockopt() to reset",
		    optionname);

	/*
	 * Check reset version.
	 */
	val[0] = -1;
	val[1] = -1;
	len = sizeof(val);
	if (getsockopt(sock, IPPROTO_IP, option, val, &len) < 0)
		err(-1, "test_ip_boolean(%s): getsockopt() after reset",
		    optionname);

	if (len != sizeof(val[0]))
		errx(-1, "test_ip_boolean(%s): getsockopt() after reset "
		    "returned %d bytes", optionname, len);

	if (val[0] == -1)
		errx(-1, "test_ip_boolean(%s): getsockopt() after reset "
		    "didn't return data", optionname);

	if (val[0] != newvalue)
		errx(-1, "test_ip_boolean(%s): getsockopt() after reset "
		    "returned %d", optionname, newvalue);

	close(sock);
}

/*
 * XXX: For now, nothing here.
 */
static void
test_ip_multicast_if(void)
{

	/*
	 * It's probably worth trying INADDR_ANY and INADDR_LOOPBACK here
	 * to see what happens.
	 */
}

/*
 * XXX: For now, nothing here.
 */
static void
test_ip_multicast_vif(void)
{

	/*
	 * This requires some knowledge of the number of virtual interfaces,
	 * and what is valid.
	 */
}

/*
 * XXX: For now, nothing here.
 */
static void
test_ip_multicast_membership(void)
{

}

static void
test_ip_multicast(void)
{

	test_ip_multicast_if();
	test_ip_multicast_vif();

	/*
	 * Test the multicast TTL exactly as we would the regular TTL, only
	 * expect a different default.
	 */
	test_ip_uchar(IP_MULTICAST_TTL, "IP_MULTICAST_TTL", 1);

	/*
	 * The multicast loopback flag can be tested using our boolean
	 * tester, but only because the FreeBSD API is a bit more flexible
	 * than earlir APIs and will accept an int as well as a u_char.
	 * Loopback is enabled by default.
	 */
	test_ip_boolean(IP_MULTICAST_LOOP, "IP_MULTICAST_LOOP", 1,
	    BOOLEAN_ANYONE);

	test_ip_multicast_membership();
}

static void
testsuite(void)
{

	test_ip_hdrincl();

	test_ip_uchar(IP_TOS, "IP_TOS", 0);
	test_ip_uchar(IP_TTL, "IP_TTL", 64);

	test_ip_boolean(IP_RECVOPTS, "IP_RECVOPTS", 0, BOOLEAN_ANYONE);
	test_ip_boolean(IP_RECVRETOPTS, "IP_RECVRETOPTS", 0, BOOLEAN_ANYONE);
	test_ip_boolean(IP_RECVDSTADDR, "IP_RECVDSTADDR", 0, BOOLEAN_ANYONE);
	test_ip_boolean(IP_RECVTTL, "IP_RECVTTL", 0, BOOLEAN_ANYONE);
	test_ip_boolean(IP_RECVIF, "IP_RECVIF", 0, BOOLEAN_ANYONE);
	test_ip_boolean(IP_FAITH, "IP_FAITH", 0, BOOLEAN_ANYONE);
	test_ip_boolean(IP_ONESBCAST, "IP_ONESBCAST", 0, BOOLEAN_ANYONE);

	/*
	 * XXX: Still need to test:
	 * IP_PORTRANGE
	 * IP_IPSEC_POLICY?
	 */

	test_ip_multicast();

	test_ip_options();
}

/*
 * Very simply exercise that we can get and set each option.  If we're running
 * as root, run it also as nobody.  If not as root, complain about that.
 */
int
main(int argc, char *argv[])
{

	if (geteuid() != 0) {
		warnx("Not running as root, can't run as-root tests");
		fprintf(stderr, "\n");
		fprintf(stderr, "Running tests with uid %d\n", geteuid());
		testsuite();
		fprintf(stderr, "PASS\n");
	} else {
		fprintf(stderr, "Running tests with uid 0\n");
		testsuite();
		if (setuid(65534) != 0)
			err(-1, "setuid(65534)");
		fprintf(stderr, "Running tests with uid 65535\n");
		testsuite();
		fprintf(stderr, "PASS\n");
	}
	exit(0);
}
