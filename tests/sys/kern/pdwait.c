/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 ConnectWise
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

#include <sys/types.h>
#include <sys/capsicum.h>
#include <sys/mman.h>
#include <sys/procdesc.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/user.h>
#include <sys/wait.h>

#include <atf-c.h>
#include <signal.h>
#include <string.h>

static void*
unmapped(void) {
	void *unmapped;

	unmapped = mmap(NULL, PAGE_SIZE, PROT_NONE, MAP_GUARD, -1, 0);
	ATF_REQUIRE(unmapped != MAP_FAILED);

	return(unmapped);
}

/* basic usage */
ATF_TC_WITHOUT_HEAD(basic);
ATF_TC_BODY(basic, tc)
{
	int fdp = -1;
	pid_t pid;
	int r, status;
	struct __wrusage ru;
	siginfo_t si;

	bzero(&ru, sizeof(ru));

	pid = pdfork(&fdp, 0);
	if (pid == 0)
		_exit(42);
	ATF_REQUIRE_MSG(pid >= 0, "pdfork failed: %s", strerror(errno));
	ATF_REQUIRE_MSG(fdp >= 0, "pdfork didn't return a process descriptor");

	r = pdwait(fdp, &status, WEXITED, &ru, &si);
	ATF_CHECK_EQ(r, 0);
	ATF_REQUIRE(WIFEXITED(status) && WEXITSTATUS(status) == 42);
	ATF_CHECK(ru.wru_self.ru_stime.tv_usec > 0);
	ATF_CHECK_EQ(si.si_signo, SIGCHLD);
	ATF_CHECK_EQ(si.si_pid, pid);
	ATF_CHECK_EQ(si.si_status, WEXITSTATUS(status));

	close(fdp);
}

/* pdwait should work in capability mode */
ATF_TC_WITHOUT_HEAD(capsicum);
ATF_TC_BODY(capsicum, tc)
{
	int fdp = -1;
	pid_t pid;
	int status, r;

	pid = pdfork(&fdp, 0);
	if (pid == 0)
		_exit(42);
	ATF_REQUIRE_MSG(pid >= 0, "pdfork failed: %s", strerror(errno));
	ATF_REQUIRE_MSG(fdp >= 0, "pdfork didn't return a process descriptor");

	ATF_CHECK_EQ_MSG(0, cap_enter(), "cap_enter: %s", strerror(errno));
	r = pdwait(fdp, &status, WEXITED, NULL, NULL);
	ATF_CHECK_EQ(r, 0);
	ATF_REQUIRE(WIFEXITED(status) && WEXITSTATUS(status) == 42);

	close(fdp);
}

/* pdwait should return EBADF if its argument is not a file descriptor */
ATF_TC_WITHOUT_HEAD(ebadf);
ATF_TC_BODY(ebadf, tc)
{
	ATF_REQUIRE_ERRNO(EBADF, pdwait(99999, NULL, WEXITED, NULL, NULL) < 0);
}

/* pdwait should return efault if the status argument is invalid.  */
ATF_TC_WITHOUT_HEAD(efault1);
ATF_TC_BODY(efault1, tc)
{
	int fdp = -1;
	pid_t pid;

	pid = pdfork(&fdp, 0);
	if (pid == 0)
		_exit(42);
	ATF_REQUIRE_MSG(pid >= 0, "pdfork failed: %s", strerror(errno));
	ATF_REQUIRE_MSG(fdp >= 0, "pdfork didn't return a process descriptor");

	ATF_CHECK_ERRNO(EFAULT,
	    pdwait(fdp, (int*)unmapped(), WEXITED, NULL, NULL) < 0);

	close(fdp);
}

/* pdwait should return efault2 if the usage argument is invalid.  */
ATF_TC_WITHOUT_HEAD(efault2);
ATF_TC_BODY(efault2, tc)
{
	int fdp = -1;
	pid_t pid;

	pid = pdfork(&fdp, 0);
	if (pid == 0)
		_exit(42);
	ATF_REQUIRE_MSG(pid >= 0, "pdfork failed: %s", strerror(errno));
	ATF_REQUIRE_MSG(fdp >= 0, "pdfork didn't return a process descriptor");

	ATF_CHECK_ERRNO(EFAULT,
	    pdwait(fdp, NULL, WEXITED, (struct __wrusage*)unmapped(), NULL) < 0
	);

	close(fdp);
}

/* pdwait should return efault if the siginfo argument is invalid.  */
ATF_TC_WITHOUT_HEAD(efault3);
ATF_TC_BODY(efault3, tc)
{
	int fdp = -1;
	pid_t pid;

	pid = pdfork(&fdp, 0);
	if (pid == 0)
		_exit(42);
	ATF_REQUIRE_MSG(pid >= 0, "pdfork failed: %s", strerror(errno));
	ATF_REQUIRE_MSG(fdp >= 0, "pdfork didn't return a process descriptor");

	ATF_CHECK_ERRNO(EFAULT,
	    pdwait(fdp, NULL, WEXITED, NULL, (struct __siginfo*)unmapped()) < 0
	);

	close(fdp);
}

/* pdwait should return einval if the arguments are bad */
ATF_TC_WITHOUT_HEAD(einval);
ATF_TC_BODY(einval, tc)
{
	int fdp = -1;
	pid_t pid;

	pid = pdfork(&fdp, 0);
	if (pid == 0)
		_exit(42);
	ATF_REQUIRE_MSG(pid >= 0, "pdfork failed: %s", strerror(errno));
	ATF_REQUIRE_MSG(fdp >= 0, "pdfork didn't return a process descriptor");

	ATF_CHECK_ERRNO(EINVAL, pdwait(fdp, NULL, 0, NULL, NULL) < 0);
	ATF_CHECK_ERRNO(EINVAL, pdwait(fdp, NULL, -1, NULL, NULL) < 0);
	ATF_CHECK_ERRNO(EINVAL,
	    pdwait(STDERR_FILENO, NULL, WEXITED, NULL, NULL) < 0);

	close(fdp);
}

/* pdwait should fail without the cap_pdwait_rights bit */
ATF_TC_WITHOUT_HEAD(enotcap);
ATF_TC_BODY(enotcap, tc)
{
	cap_rights_t rights;
	int fdp = -1;
	pid_t pid;
	int status;

	/*cap_rights_init(&rights, CAP_RIGHTS_ALL);*/
	CAP_ALL(&rights);
	cap_rights_clear(&rights, CAP_PDWAIT);

	pid = pdfork(&fdp, 0);
	if (pid == 0)
		_exit(42);
	ATF_REQUIRE_MSG(pid >= 0, "pdfork failed: %s", strerror(errno));
	ATF_REQUIRE_MSG(fdp >= 0, "pdfork didn't return a process descriptor");

	ATF_CHECK_EQ_MSG(0, cap_enter(), "cap_enter: %s", strerror(errno));
	ATF_REQUIRE_EQ_MSG(0, cap_rights_limit(fdp, &rights),
	    "cap_rights_limit %s", strerror(errno));

	ATF_REQUIRE_ERRNO(ENOTCAPABLE,
	    pdwait(fdp, &status, WEXITED, NULL, NULL) < 0);

	close(fdp);
}

/*
 * Even though the process descriptor is still open, there is no more process
 * to signal after pdwait() has returned.
 */
ATF_TC_WITHOUT_HEAD(pdkill_after_pdwait);
ATF_TC_BODY(pdkill_after_pdwait, tc)
{
	int fdp = -1;
	pid_t pid;
	int r, status;

	pid = pdfork(&fdp, 0);
	if (pid == 0)
		_exit(42);
	ATF_REQUIRE_MSG(pid >= 0, "pdfork failed: %s", strerror(errno));
	ATF_REQUIRE_MSG(fdp >= 0, "pdfork didn't return a process descriptor");

	r = pdwait(fdp, &status, WEXITED, NULL, NULL);
	ATF_CHECK_EQ(r, 0);
	ATF_REQUIRE(WIFEXITED(status) && WEXITSTATUS(status) == 42);

	ATF_REQUIRE_ERRNO(ESRCH, pdkill(fdp, SIGTERM) < 0);

	close(fdp);
}

/*
 * Even though the process descriptor is still open, there is no more status to
 * return after a pid-based wait() function has already returned it.
 */
ATF_TC_WITHOUT_HEAD(pdwait_after_waitpid);
ATF_TC_BODY(pdwait_after_waitpid, tc)
{
	int fdp = -1;
	pid_t pid, waited_pid;
	int status;

	pid = pdfork(&fdp, 0);
	if (pid == 0)
		_exit(42);
	ATF_REQUIRE_MSG(pid >= 0, "pdfork failed: %s", strerror(errno));
	ATF_REQUIRE_MSG(fdp >= 0, "pdfork didn't return a process descriptor");

	waited_pid = waitpid(pid, &status, WEXITED);

	ATF_CHECK_EQ(pid, waited_pid);
	ATF_REQUIRE(WIFEXITED(status) && WEXITSTATUS(status) == 42);

	ATF_REQUIRE_ERRNO(ESRCH, pdwait(fdp, NULL, WEXITED, NULL, NULL) < 0);

	close(fdp);
}

/* Called twice, waitpid should return ESRCH the second time */
ATF_TC_WITHOUT_HEAD(twice);
ATF_TC_BODY(twice, tc)
{
	int fdp = -1;
	pid_t pid;
	int r, status;

	pid = pdfork(&fdp, 0);
	if (pid == 0)
		_exit(42);
	ATF_REQUIRE_MSG(pid >= 0, "pdfork failed: %s", strerror(errno));
	ATF_REQUIRE_MSG(fdp >= 0, "pdfork didn't return a process descriptor");

	r = pdwait(fdp, &status, WEXITED, NULL, NULL);
	ATF_CHECK_EQ(r, 0);
	ATF_REQUIRE(WIFEXITED(status) && WEXITSTATUS(status) == 42);

	ATF_REQUIRE_ERRNO(ESRCH, pdwait(fdp, NULL, WEXITED, NULL, NULL) < 0);

	close(fdp);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, basic);
	ATF_TP_ADD_TC(tp, capsicum);
	ATF_TP_ADD_TC(tp, ebadf);
	ATF_TP_ADD_TC(tp, enotcap);
	ATF_TP_ADD_TC(tp, twice);
	ATF_TP_ADD_TC(tp, efault1);
	ATF_TP_ADD_TC(tp, efault2);
	ATF_TP_ADD_TC(tp, efault3);
	ATF_TP_ADD_TC(tp, einval);
	ATF_TP_ADD_TC(tp, pdwait_after_waitpid);
	ATF_TP_ADD_TC(tp, pdkill_after_pdwait);

	return (atf_no_error());
}
