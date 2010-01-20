/*-
 * Copyright (c) 2007 Robert N. M. Watson
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

#include <net/ethernet.h>

#include <stdio.h>
#include <string.h>

static int testnum;

#define	OK()	do {							\
	printf("ok %d %s\n", testnum, __func__);			\
	return;								\
} while (0)

#define	NOTOK(why)	do {						\
	printf("not ok %d %s # %s\n", testnum, __func__, why);		\
	return;								\
} while (0)

#define	TODO()	NOTOK("TODO")

static const char		*ether_line_string =
				    "01:23:45:67:89:ab ether_line_hostname";
static const char		*ether_line_hostname = "ether_line_hostname";
static const struct ether_addr	 ether_line_addr = {
	{ 0x01, 0x23, 0x45, 0x67, 0x89, 0xab }
};

static void
test_ether_line(void)
{
	struct ether_addr e;
	char hostname[256];

	testnum++;
	if (ether_line(ether_line_string, &e, hostname) != 0)
		NOTOK("returned error");
	if (bcmp(&e, &ether_line_addr, ETHER_ADDR_LEN) != 0)
		NOTOK("bad address");
	if (strcmp(hostname, ether_line_hostname) != 0) {
		printf("hostname: %s\n", hostname);
		NOTOK("bad hostname");
	}
	OK();
}

static const char		*ether_line_bad_1_string = "x";

static void
test_ether_line_bad_1(void)
{
	struct ether_addr e;
	char hostname[256];

	testnum++;
	if (ether_line(ether_line_bad_1_string, &e, hostname) == 0)
		NOTOK("returned success");
	OK();
}

static const char		*ether_line_bad_2_string = "x x";

static void
test_ether_line_bad_2(void)
{
	struct ether_addr e;
	char hostname[256];

	testnum++;
	if (ether_line(ether_line_bad_2_string, &e, hostname) == 0)
		NOTOK("returned success");
	OK();
}

static const char		*ether_aton_string = "01:23:45:67:89:ab";
static const struct ether_addr	 ether_aton_addr = {
	{ 0x01, 0x23, 0x45, 0x67, 0x89, 0xab }
};

static void
test_ether_aton_r(void)
{
	struct ether_addr e, *ep;

	testnum++;
	ep = ether_aton_r(ether_aton_string, &e);
	if (ep == NULL)
		NOTOK("returned NULL");
	if (ep != &e)
		NOTOK("returned different pointer");
	if (bcmp(&e, &ether_aton_addr, ETHER_ADDR_LEN) != 0)
		NOTOK("bad address");
	OK();
}

static const char		*ether_aton_bad_string = "x";

static void
test_ether_aton_r_bad(void)
{
	struct ether_addr e, *ep;

	testnum++;
	ep = ether_aton_r(ether_aton_bad_string, &e);
	if (ep == &e)
		NOTOK("returned success");
	if (ep != NULL)
		NOTOK("returned different pointer");
	OK();
}

static void
test_ether_aton(void)
{
	struct ether_addr *ep;

	testnum++;
	ep = ether_aton(ether_aton_string);
	if (ep == NULL)
		NOTOK("returned NULL");
	if (bcmp(ep, &ether_aton_addr, ETHER_ADDR_LEN) != 0)
		NOTOK("bad address");
	OK();
}

static void
test_ether_aton_bad(void)
{
	struct ether_addr *ep;

	testnum++;
	ep = ether_aton(ether_aton_bad_string);
	if (ep != NULL)
		NOTOK("returned success");
	OK();
}

static const char		*ether_ntoa_string = "01:23:45:67:89:ab";
static const struct ether_addr	 ether_ntoa_addr = {
	{ 0x01, 0x23, 0x45, 0x67, 0x89, 0xab }
};

static void
test_ether_ntoa_r(void)
{
	char buf[256], *cp;

	testnum++;
	cp = ether_ntoa_r(&ether_ntoa_addr, buf);
	if (cp == NULL)
		NOTOK("returned NULL");
	if (cp != buf)
		NOTOK("returned different pointer");
	if (strcmp(cp, ether_ntoa_string) != 0)
		NOTOK("bad string");
	OK();
}

static void
test_ether_ntoa(void)
{
	char *cp;

	testnum++;
	cp = ether_ntoa(&ether_ntoa_addr);
	if (cp == NULL)
		NOTOK("returned NULL");
	if (strcmp(cp, ether_ntoa_string) != 0)
		NOTOK("bad string");
	OK();
}

static void
test_ether_ntohost(void)
{

	testnum++;
	TODO();
}

static void
test_ether_hostton(void)
{

	testnum++;
	TODO();
}

int
main(int argc, char *argv[])
{

	printf("1..11\n");

	test_ether_line();
	test_ether_line_bad_1();
	test_ether_line_bad_2();
	test_ether_aton_r();
	test_ether_aton_r_bad();
	test_ether_aton();
	test_ether_aton_bad();
	test_ether_ntoa_r();
	test_ether_ntoa();
	test_ether_ntohost();
	test_ether_hostton();
	return (0);
}
