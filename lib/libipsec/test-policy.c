/*
 * Copyright (C) 1995, 1996, 1997, 1998, and 1999 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
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
#include <sys/param.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <netinet6/in6.h>
#include <net/pfkeyv2.h>
#include <netkey/key_debug.h>
#include <netinet6/ipsec.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <err.h>

char *requests[] = {
"must_error",				/* error */
"in ipsec must_error",			/* error */
"out ipsec esp/must_error",		/* error */
"out discard",
"out none",
"in entrust",
"out entrust",
"in bypass",				/* may be error */
"out ipsec esp",			/* error */
"in ipsec ah/transport",
"in ipsec ah/tunnel",			/* error */
"out ipsec ah/transport/",
"out ipsec ah/tunnel/",			/* error */
"in ipsec esp / transport / 10.0.0.1-10.0.0.2",
"in ipsec esp/tunnel/::1-::2",
"in ipsec esp/tunnel/10.0.0.1-::2",	/* error */
"in ipsec esp/tunnel/::1-::2/require",
"out ipsec ah/transport//use",
"out ipsec ah/transport esp/use",
"in ipsec ah/transport esp/tunnel",	/* error */
"in ipsec
	ah / transport
	esp / tunnel / ::1-::2",
"
out ipsec
ah/transport/::1-::2 esp/tunnel/::3-::4/use ah/transport/::5-::6/require
ah/transport/::1-::2 esp/tunnel/::3-::4/use ah/transport/::5-::6/require
ah/transport/::1-::2 esp/tunnel/::3-::4/use ah/transport/::5-::6/require
",
"out ipsec esp/transport/fec0::10-fec0::11/use",
};

int test(char *buf, int family);

int
main(ac, av)
	int ac;
	char **av;
{
	int do_setsockopt;
	char *buf;
	int i;

	if (ac != 1)
		do_setsockopt = 1;
	else
		do_setsockopt = 0;

	for (i = 0; i < sizeof(requests)/sizeof(requests[0]); i++) {
		printf("*** requests ***\n");
		printf("\t[%s]\n", requests[i]);

		buf = ipsec_set_policy(requests[i], strlen(requests[i]));
		if (buf == NULL) {
			printf("ipsec_set_policy: %s\n", ipsec_strerror());
			continue;
		}

		printf("\tsetlen:%d\n", ipsec_get_policylen(buf));

		if (do_setsockopt) {
			printf("\tPF_INET:\n");
			test(buf, PF_INET);

			printf("\tPF_INET6:\n");
			test(buf, PF_INET6);
		} else {
			kdebug_sadb_x_policy((struct sadb_ext *)buf);
		}
		free(buf);
	}

	return 0;
}

int
test(policy, family)
	char *policy;
	int family;
{
	int so, proto, optname;
	int len;
	char getbuf[1024];

	switch (family) {
	case PF_INET:
		proto = IPPROTO_IP;
		optname = IP_IPSEC_POLICY;
		break;
	case PF_INET6:
		proto = IPPROTO_IPV6;
		optname = IPV6_IPSEC_POLICY;
		break;
	}

	if ((so = socket(family, SOCK_DGRAM, 0)) < 0)
		err(1, "socket");

	len = ipsec_get_policylen(policy);
	if (setsockopt(so, proto, optname, policy, len) < 0) {
		printf("error on setsockopt");
		goto end;
	}

	len = sizeof(getbuf);
	memset(getbuf, 0, sizeof(getbuf));
	if (getsockopt(so, proto, optname, getbuf, &len) < 0) {
		printf("error on getsockopt");
		goto end;
	}

    {
	char *buf = NULL;

	printf("\tgetlen:%d\n", len);

	if ((buf = ipsec_dump_policy(getbuf, NULL)) == NULL) {
		printf("%s\n", ipsec_strerror());
		goto end;
	} else {
		printf("\t[%s]\n", buf);
		free(buf);
	}
    }

    end:
	close (so);

	return 0;
}

