/*	$NetBSD: t_basic.c,v 1.5 2011/06/26 13:13:31 christos Exp $	*/

/*-
 * Copyright (c) 2010 The NetBSD Foundation, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND
 * CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: t_basic.c,v 1.5 2011/06/26 13:13:31 christos Exp $");
#endif /* not lint */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip_carp.h>

#include <rump/rump.h>
#include <rump/rump_syscalls.h>

#include <atf-c.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include "../config/netconfig.c"
#include "../../h_macros.h"

static bool oknow = false;

static void
sighnd(int sig)
{

	ATF_REQUIRE_EQ(sig, SIGCHLD);
	if (oknow)
		return;

	atf_tc_fail("child died unexpectedly");
}

ATF_TC(handover);
ATF_TC_HEAD(handover, tc)
{

	atf_tc_set_md_var(tc, "descr", "check that carp handover works if "
	    "the master dies");
}

#define THEBUS "buuuuuuus,etherbus"

static void
child(bool master)
{
	char ifname[IFNAMSIZ];
	struct carpreq cr;
	struct ifreq ifr;
	const char *carpif;
	int s;

	/* helps reading carp debug output */
	if (master)
		carpif = "carp0";
	else
		carpif = "carp1";

	/*
	 * Should use sysctl, bug debug is dabug.
	 */
	{
	//extern int rumpns_carp_opts[]; /* XXX */
	//rumpns_carp_opts[CARPCTL_LOG] = 1;
	}


	rump_init();

	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, carpif, sizeof(ifr.ifr_name));

	RL(s = rump_sys_socket(PF_INET, SOCK_DGRAM, 0));
	RL(rump_sys_ioctl(s, SIOCIFCREATE, &ifr));

	netcfg_rump_makeshmif(THEBUS, ifname);

	if (master)
		netcfg_rump_if(ifname, "10.1.1.1", "255.255.255.0");
	else
		netcfg_rump_if(ifname, "10.1.1.2", "255.255.255.0");

	/* configure the carp interface */
	ifr.ifr_data = &cr;
	RL(rump_sys_ioctl(s, SIOCGVH, &ifr));

	strlcpy(cr.carpr_carpdev, ifname, sizeof(cr.carpr_carpdev));
	cr.carpr_vhid = 175;
	if (master)
		cr.carpr_advskew = 0;
	else
		cr.carpr_advskew = 200;
	cr.carpr_advbase = 1;
	strcpy((char *)cr.carpr_key, "s3cret");

	RL(rump_sys_ioctl(s, SIOCSVH, &ifr));
	netcfg_rump_if(carpif, "10.1.1.100", "255.255.255.0");

	/* tassa pause()en enka muuta voi */
	pause();
}

ATF_TC_BODY(handover, tc)
{
	char ifname[IFNAMSIZ];
	pid_t mpid, cpid;
	int i, status;

	signal(SIGCHLD, sighnd);

	/* fork master */
	switch (mpid = fork()) {
	case -1:
		atf_tc_fail_errno("fork failed");
		/*NOTREACHED*/
	case 0:
		child(true);
		/*NOTREACHED*/
	default:
		break;
	}

	usleep(500000);

	/* fork backup */
	switch (cpid = fork()) {
	case -1:
		kill(mpid, SIGKILL);
		atf_tc_fail_errno("fork failed");
		/*NOTREACHED*/
	case 0:
		child(false);
		/*NOTREACHED*/
	default:
		break;
	}

	usleep(500000);

	rump_init();
	netcfg_rump_makeshmif(THEBUS, ifname);
	netcfg_rump_if(ifname, "10.1.1.240", "255.255.255.0");

	/* check that the primary addresses are up */
	ATF_REQUIRE_EQ(netcfg_rump_pingtest("10.1.1.1", 1000), true);
	ATF_REQUIRE_EQ(netcfg_rump_pingtest("10.1.1.2", 1000), true);

	/* give carp a while to croak */
	sleep(4);

	/* check that the shared IP works */
	ATF_REQUIRE_EQ(netcfg_rump_pingtest("10.1.1.100", 500), true);

	/* KILLING SPREE */
	oknow = true;
	kill(mpid, SIGKILL);
	wait(&status);
	usleep(10000); /* just in case */
	oknow = false;

	/* check that primary is now dead */
	ATF_REQUIRE_EQ(netcfg_rump_pingtest("10.1.1.1", 100), false);

	/* do it in installments. carp will cluck meanwhile */
	for (i = 0; i < 5; i++) {
		if (netcfg_rump_pingtest("10.1.1.100", 1000) == true)
			break;
	}
	if (i == 5)
		atf_tc_fail("failed to failover");

	/* to kill the child */
	oknow = true;
	kill(cpid, SIGKILL);

	/* clean & done */
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, handover);

	return atf_no_error();
}
