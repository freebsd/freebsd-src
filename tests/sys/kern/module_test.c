/*-
 * Copyright (c) 2022 University of Cambridge
 *
 * This software was developed by Ararat River Consulting, LLC under
 * sponsorship from the University of Cambridge Computer Laboratory
 * (Department of Computer Science and Technology) and Google, Inc.
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

#include <sys/param.h>
#include <sys/module.h>
#include <errno.h>
#include <atf-c.h>

ATF_TC_WITHOUT_HEAD(modfind);
ATF_TC_BODY(modfind, tc)
{
	int modid;

	/* This module is present in sys/kern/subr_bus.c. */
	modid = modfind("rootbus");
	ATF_REQUIRE(modid > 0);

	modid = modfind("nonexistent_module");
	ATF_REQUIRE(modid == -1);
	ATF_REQUIRE(errno == ENOENT);
}

ATF_TC_WITHOUT_HEAD(modnext);
ATF_TC_BODY(modnext, tc)
{
	int modid;

	/* This assumes -1 is never used as a valid module id. */
	modid = modnext(-1);
	ATF_REQUIRE(modid == -1);
	ATF_REQUIRE(errno == ENOENT);

	modid = modnext(0);
	ATF_REQUIRE(modid > 0);

	for (;;) {
		modid = modnext(modid);
		ATF_REQUIRE(modid >= 0);
		if (modid == 0)
			break;
	}
}

ATF_TC_WITHOUT_HEAD(modfnext);
ATF_TC_BODY(modfnext, tc)
{
	int modid;

	/* This assumes -1 is never used as a valid module id. */
	modid = modfnext(-1);
	ATF_REQUIRE(modid == -1);
	ATF_REQUIRE(errno == ENOENT);

	modid = modfnext(0);
	ATF_REQUIRE(modid == -1);
	ATF_REQUIRE(errno == ENOENT);

	modid = modnext(0);
	ATF_REQUIRE(modid > 0);

	for (;;) {
		modid = modfnext(modid);
		ATF_REQUIRE(modid >= 0);
		if (modid == 0)
			break;
	}
}

ATF_TC_WITHOUT_HEAD(modstat);
ATF_TC_BODY(modstat, tc)
{
	struct module_stat ms;
	int modid;

	ms.version = sizeof(ms);
	ATF_REQUIRE(modstat(0, &ms) == -1);
	ATF_REQUIRE(errno == ENOENT);

	modid = modnext(0);
	ATF_REQUIRE(modid > 0);

	ATF_REQUIRE(modstat(modid, NULL) == -1);
	ATF_REQUIRE(errno == EFAULT);

	ms.version = 0;
	ATF_REQUIRE(modstat(modid, &ms) == -1);
	ATF_REQUIRE(errno == EINVAL);

	ms.version = sizeof(ms);
	ATF_REQUIRE(modstat(modid, &ms) == 0);
	ATF_REQUIRE(ms.id == modid);
	if (strnlen(ms.name, sizeof(ms.name)) < sizeof(ms.name))
		ATF_REQUIRE(modfind(ms.name) == modid);
}

ATF_TC_WITHOUT_HEAD(modstat_v1);
ATF_TC_BODY(modstat_v1, tc)
{
	struct module_stat_v1 {
		int	version;
		char	name[32];
		int	refs;
		int	id;
	} ms;
	int modid;

	ms.version = sizeof(ms);
	ATF_REQUIRE(modstat(0, (struct module_stat *)&ms) == -1);
	ATF_REQUIRE(errno == ENOENT);

	modid = modnext(0);
	ATF_REQUIRE(modid > 0);

	ATF_REQUIRE(modstat(modid, NULL) == -1);
	ATF_REQUIRE(errno == EFAULT);

	ms.version = 0;
	ATF_REQUIRE(modstat(modid, (struct module_stat *)&ms) == -1);
	ATF_REQUIRE(errno == EINVAL);

	ms.version = sizeof(ms);
	ATF_REQUIRE(modstat(modid, (struct module_stat *)&ms) == 0);
	ATF_REQUIRE(ms.id == modid);
	if (strnlen(ms.name, sizeof(ms.name)) < sizeof(ms.name))
		ATF_REQUIRE(modfind(ms.name) == modid);
}

ATF_TC_WITHOUT_HEAD(modstat_v2);
ATF_TC_BODY(modstat_v2, tc)
{
	struct module_stat_v2 {
		int	version;
		char	name[32];
		int	refs;
		int	id;
		modspecific_t data;
	} ms;
	int modid;

	ms.version = sizeof(ms);
	ATF_REQUIRE(modstat(0, (struct module_stat *)&ms) == -1);
	ATF_REQUIRE(errno == ENOENT);

	modid = modnext(0);
	ATF_REQUIRE(modid > 0);

	ATF_REQUIRE(modstat(modid, NULL) == -1);
	ATF_REQUIRE(errno == EFAULT);

	ms.version = 0;
	ATF_REQUIRE(modstat(modid, (struct module_stat *)&ms) == -1);
	ATF_REQUIRE(errno == EINVAL);

	ms.version = sizeof(ms);
	ATF_REQUIRE(modstat(modid, (struct module_stat *)&ms) == 0);
	ATF_REQUIRE(ms.id == modid);
	if (strnlen(ms.name, sizeof(ms.name)) < sizeof(ms.name))
		ATF_REQUIRE(modfind(ms.name) == modid);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, modfind);
	ATF_TP_ADD_TC(tp, modnext);
	ATF_TP_ADD_TC(tp, modfnext);
	ATF_TP_ADD_TC(tp, modstat);
	ATF_TP_ADD_TC(tp, modstat_v1);
	ATF_TP_ADD_TC(tp, modstat_v2);

	return (atf_no_error());
}
