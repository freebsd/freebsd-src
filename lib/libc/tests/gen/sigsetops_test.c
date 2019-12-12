/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2019 Kyle Evans <kevans@FreeBSD.org>
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <errno.h>
#include <signal.h>
#include <stdbool.h>

#include <atf-c.h>

/* Return the status of the specified sig's bit. */
static bool
sigbitstatus(const sigset_t *set, int sig)
{

	return (set->__bits[_SIG_WORD(sig)] & _SIG_BIT(sig)) != 0;
}

/* Verify that sig is the lone bit set in the sigset. */
static void
siglonebit(const sigset_t *set, int sig)
{
	int i;

	for (i = 0; i < _SIG_WORDS; ++i) {
		if (i != _SIG_WORD(sig))
			ATF_REQUIRE_MSG(set->__bits[i] == 0,
			    "word %d altered to %x", i, set->__bits[i]);
		else
			ATF_REQUIRE_MSG((set->__bits[i] & ~_SIG_BIT(sig)) == 0,
			    "word %d has other bits set: %x", i,
			    set->__bits[i] & ~_SIG_BIT(sig));
	}
}

static void
sigcompare(const sigset_t *left, const sigset_t *right)
{
	int i;

	for (i = 0; i < _SIG_WORDS; ++i) {
		ATF_REQUIRE_MSG(left->__bits[i] == right->__bits[i],
		    "sig comparison failed at %d; left=%x, right=%x",
		    i, left->__bits[i], right->__bits[i]);
	}
}

/*
 * Test implementation details of our sigsetops... make sure the correct bits
 * are getting set, for the most part, and that duplicate operations don't
 * error out.
 */
ATF_TC_WITHOUT_HEAD(posix_sigsetop_test);
ATF_TC_BODY(posix_sigsetop_test, tc)
{
	sigset_t set;
	int i;

	ATF_REQUIRE(sigfillset(&set) == 0);
	for (i = 0; i < _SIG_WORDS; ++i) {
		ATF_REQUIRE_MSG(set.__bits[i] == ~0U, "sigfillset failed @ %d",
		    i);
	}
	ATF_REQUIRE(sigemptyset(&set) == 0);
	for (i = 0; i < _SIG_WORDS; ++i) {
		ATF_REQUIRE_MSG(set.__bits[i] == 0, "sigemptyset failed @ %d",
		    i);
	}
	/* Ensure that sigismember reflects the empty set status. */
	for (i = 1; i < NSIG; ++i) {
		ATF_REQUIRE(sigismember(&set, i) == 0);
	}

	ATF_REQUIRE(sigaddset(&set, -1) == -1 && errno == EINVAL);
	ATF_REQUIRE(sigaddset(&set, _SIG_MAXSIG + 1) == -1 && errno == EINVAL);
	ATF_REQUIRE(sigdelset(&set, -1) == -1 && errno == EINVAL);
	ATF_REQUIRE(sigdelset(&set, _SIG_MAXSIG + 1) == -1 && errno == EINVAL);

	ATF_REQUIRE(sigaddset(&set, SIGSEGV) == 0);
	ATF_REQUIRE(sigismember(&set, SIGSEGV) != 0);
	ATF_REQUIRE_MSG(sigbitstatus(&set, SIGSEGV), "sigaddset failure");
	siglonebit(&set, SIGSEGV);

	/*
	 * A second addition should succeed without altering the state.  This
	 * should be trivially true.
	 */
	ATF_REQUIRE(sigaddset(&set, SIGSEGV) == 0);
	ATF_REQUIRE_MSG(sigbitstatus(&set, SIGSEGV),
	    "sigaddset twice changed bit");

	ATF_REQUIRE(sigdelset(&set, SIGSEGV) == 0);
	ATF_REQUIRE_MSG(!sigbitstatus(&set, SIGSEGV), "sigdelset failure");
	ATF_REQUIRE(sigismember(&set, SIGSEGV) == 0);
	ATF_REQUIRE(sigdelset(&set, SIGSEGV) == 0);
	ATF_REQUIRE_MSG(!sigbitstatus(&set, SIGSEGV),
	    "sigdelset twice changed bit");
	for (i = 0; i < _SIG_WORDS; ++i) {
		ATF_REQUIRE_MSG(set.__bits[i] == 0, "set not empty @ %d",
		    i);
	}
	for (i = 1; i < NSIG; ++i) {
		ATF_REQUIRE(sigismember(&set, i) == 0);
	}
}

/*
 * Test extended sigset ops for union/intersection and testing of empty set.
 */
ATF_TC_WITHOUT_HEAD(extended_sigsetop_test);
ATF_TC_BODY(extended_sigsetop_test, tc)
{
	sigset_t chkset, set1, set2, set3;

	sigemptyset(&chkset);
	sigemptyset(&set1);
	sigemptyset(&set2);
	ATF_REQUIRE(sigisemptyset(&chkset) != 0);
	sigaddset(&set1, SIGSEGV);
	sigaddset(&set2, SIGKILL);
	sigaddset(&chkset, SIGSEGV);
	ATF_REQUIRE(sigisemptyset(&chkset) == 0);
	sigaddset(&chkset, SIGKILL);
	ATF_REQUIRE(sigorset(&set3, &set1, &set2) == 0);
	ATF_REQUIRE(sigbitstatus(&set3, SIGSEGV));
	ATF_REQUIRE(sigbitstatus(&set3, SIGKILL));

	/*
	 * chkset was built with our POSIX-specified set operations that we've
	 * already tested, so it's a good comparison.
	 */
	sigcompare(&chkset, &set3);
	/*
	 * Clear chkset; make sure sigisemptyset() still looks ok.  sigaddset
	 * and sigdelset have already been tested to make sure that they're not
	 * touching other bits.
	 */
	sigdelset(&chkset, SIGSEGV);
	sigdelset(&chkset, SIGKILL);
	ATF_REQUIRE(sigisemptyset(&chkset) != 0);
	ATF_REQUIRE(sigandset(&set3, &set1, &set2) == 0);
	/* Make sure we clobbered these. */
	ATF_REQUIRE(!sigbitstatus(&set3, SIGSEGV));
	ATF_REQUIRE(!sigbitstatus(&set3, SIGKILL));
	ATF_REQUIRE(sigisemptyset(&set3) != 0);
	/* Rebuild for sigandset test */
	sigemptyset(&set1);
	sigemptyset(&set2);
	sigaddset(&set1, SIGSEGV);
	sigaddset(&set1, SIGKILL);
	sigaddset(&set2, SIGSEGV);
	ATF_REQUIRE(sigandset(&set3, &set1, &set2) == 0);
	ATF_REQUIRE(sigbitstatus(&set3, SIGSEGV));
	ATF_REQUIRE(!sigbitstatus(&set3, SIGKILL));
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, posix_sigsetop_test);
	ATF_TP_ADD_TC(tp, extended_sigsetop_test);

	return (atf_no_error());
}
