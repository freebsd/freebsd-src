/*-
 * Copyright (c) 2020 Alfredo Dal'Ava Junior <alfredo@freebsd.org>
 * Copyright (c) 2017 Enji Cooper <ngie@freebsd.org>
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
 * From: FreeBSD: src/lib/libkvm/tests/kvm_geterr_test.c
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <errno.h>
#include <fcntl.h>
#include <kvm.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include <atf-c.h>

#include "kvm_test_common.h"

ATF_TC(kvm_read_positive_test_no_error);
ATF_TC_HEAD(kvm_read_positive_test_no_error, tc)
{

	atf_tc_set_md_var(tc, "descr",
	    "test that kvm_read returns a sane value");
	atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(kvm_read_positive_test_no_error, tc)
{
	kvm_t *kd;
	struct nlist nl[] = {
#define	SYMNAME	"_mp_maxcpus"
#define	X_MAXCPUS	0
		{ SYMNAME, 0, 0, 0, 0 },
		{ NULL, 0, 0, 0, 0 },
	};
	ssize_t rc;
	int sysctl_maxcpus, mp_maxcpus, retcode;
	size_t len = sizeof(sysctl_maxcpus);

	errbuf_clear();
	kd = kvm_open(NULL, NULL, NULL, O_RDONLY, errbuf);
	ATF_CHECK(!errbuf_has_error(errbuf));
	ATF_REQUIRE_MSG(kd != NULL, "kvm_open failed: %s", errbuf);
	retcode = kvm_nlist(kd, nl);
	ATF_REQUIRE_MSG(retcode != -1,
	    "kvm_nlist failed (returned %d): %s", retcode, kvm_geterr(kd));
	if (nl[X_MAXCPUS].n_type == 0)
		atf_tc_skip("symbol (\"%s\") couldn't be found", SYMNAME);

	rc = kvm_read(kd, nl[X_MAXCPUS].n_value, &mp_maxcpus,
	    sizeof(mp_maxcpus));

	ATF_REQUIRE_MSG(rc != -1, "kvm_read failed: %s", kvm_geterr(kd));
	ATF_REQUIRE_MSG(kvm_close(kd) == 0, "kvm_close failed: %s",
	    strerror(errno));

	/* Check if value read from kvm_read is sane */
        retcode = sysctlbyname("kern.smp.maxcpus", &sysctl_maxcpus, &len, NULL, 0);
	ATF_REQUIRE_MSG(retcode == 0, "sysctl read failed : %d", retcode);
	ATF_REQUIRE_EQ_MSG(mp_maxcpus, sysctl_maxcpus,
	    "failed: kvm_read of mp_maxcpus returned %d but sysctl maxcpus returned %d",
	    mp_maxcpus, sysctl_maxcpus);
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, kvm_read_positive_test_no_error);
	return (atf_no_error());
}
