/*
 * Copyright (c) 2009 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2009, Secure Endpoints Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <config.h>
#include <err.h>
#include "getarg.h"

#include "roken.h"

#include <ifaddrs.h>

static int verbose_counter;
static int version_flag;
static int help_flag;

static struct getargs args[] = {
    {"verbose",	0,	arg_counter,	&verbose_counter,"verbose",	NULL},
    {"version",	0,	arg_flag,	&version_flag,	"print version",NULL},
    {"help",	0,	arg_flag,	&help_flag,	NULL,		NULL}
};

static void
usage(int ret)
{
    arg_printusage (args,
		    sizeof(args) / sizeof(args[0]),
		    NULL, "");
    exit (ret);
}


static void
print_addr(const char *s, struct sockaddr *sa)
{
    int i;
    printf("  %s=%d/", s, sa->sa_family);
#ifdef HAVE_STRUCT_SOCKADDR_SA_LEN
    for(i = 0; i < sa->sa_len - ((long)sa->sa_data - (long)&sa->sa_family); i++)
	printf("%02x", ((unsigned char*)sa->sa_data)[i]);
#else
    for(i = 0; i < sizeof(sa->sa_data); i++)
	printf("%02x", ((unsigned char*)sa->sa_data)[i]);
#endif
    printf("\n");
}

static void
print_ifaddrs(struct ifaddrs *x)
{
    struct ifaddrs *p;

    for(p = x; p; p = p->ifa_next) {
	if (verbose_counter) {
	    printf("%s\n", p->ifa_name);
	    printf("  flags=%x\n", p->ifa_flags);
	    if(p->ifa_addr)
		print_addr("addr", p->ifa_addr);
	    if(p->ifa_dstaddr)
		print_addr("dstaddr", p->ifa_dstaddr);
	    if(p->ifa_netmask)
		print_addr("netmask", p->ifa_netmask);
	    printf("  %p\n", p->ifa_data);
	}
    }
}

int
main(int argc, char **argv)
{
    struct ifaddrs *addrs = NULL;
    int ret, optidx = 0;

    setprogname (argv[0]);

    if (getarg (args, sizeof(args) / sizeof(args[0]), argc, argv,
		&optidx))
	usage (1);

    if (help_flag)
	usage (0);

    if (version_flag) {
	fprintf (stderr, "%s from %s-%s\n", getprogname(), PACKAGE, VERSION);
	return 0;
    }

    if (rk_SOCK_INIT())
	errx(1, "Couldn't initialize sockets. Err=%d\n", rk_SOCK_ERRNO);

    ret = getifaddrs(&addrs);
    if (ret != 0)
	err(1, "getifaddrs");

    if (addrs == NULL)
	errx(1, "address == NULL");

    print_ifaddrs(addrs);

    /* Check that freeifaddrs doesn't crash */
    freeifaddrs(addrs);

    rk_SOCK_EXIT();

    return 0;
}
