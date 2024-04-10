/*-
 * Copyright (c) 2015 John Baldwin <jhb@FreeBSD.org>
 * Copyright (c) 2023 The FreeBSD Foundation
 *
 * This software was developed by Jake Freeland <jfree@FreeBSD.org>
 * under sponsorship from the FreeBSD Foundation.
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
#include <sys/capsicum.h>
#include <sys/cpuset.h>
#include <sys/ktrace.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/sysent.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/user.h>
#include <sys/wait.h>

#include <machine/sysarch.h>
#include <netinet/in.h>

#include <atf-c.h>
#include <capsicum_helpers.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <sysdecode.h>

/*
 * A variant of ATF_REQUIRE that is suitable for use in child
 * processes.  This only works if the parent process is tripped up by
 * the early exit and fails some requirement itself.
 */
#define	CHILD_REQUIRE(exp) do {				\
	if (!(exp))					\
		child_fail_require(__FILE__, __LINE__,	\
		    #exp " not met\n");			\
} while (0)
#define	CHILD_REQUIRE_EQ(actual, expected) do {			\
	__typeof__(expected) _e = expected;			\
	__typeof__(actual) _a = actual;				\
	if (_e != _a)						\
		child_fail_require(__FILE__, __LINE__, #actual	\
		    " (%jd) == " #expected " (%jd) not met\n",	\
		    (intmax_t)_a, (intmax_t)_e);		\
} while (0)

static __dead2 void
child_fail_require(const char *file, int line, const char *fmt, ...)
{
	va_list ap;
	char buf[1024];

	/* Use write() not fprintf() to avoid possible duplicate output. */
	snprintf(buf, sizeof(buf), "%s:%d: ", file, line);
	write(STDERR_FILENO, buf, strlen(buf));
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	write(STDERR_FILENO, buf, strlen(buf));
	va_end(ap);

	_exit(32);
}

/*
 * Determine sysdecode ABI based on proc's ABI in sv_flags.
 */
static enum sysdecode_abi
syscallabi(u_int sv_flags)
{
	switch (sv_flags & SV_ABI_MASK) {
	case SV_ABI_FREEBSD:
		return (SYSDECODE_ABI_FREEBSD);
	case SV_ABI_LINUX:
#ifdef __LP64__
		if ((sv_flags & SV_ILP32) != 0)
			return (SYSDECODE_ABI_LINUX32);
#endif
		return (SYSDECODE_ABI_LINUX);
	}
	return (SYSDECODE_ABI_UNKNOWN);
}

/*
 * Start tracing capability violations and notify child that it can execute.
 * Return @numv capability violations from child in @v.
 */
static void
cap_trace_child(int cpid, struct ktr_cap_fail *v, int numv)
{
	struct ktr_header header;
	int error, fd, i;

	ATF_REQUIRE((fd = open("ktrace.out",
	    O_RDONLY | O_CREAT | O_TRUNC)) != -1);
	ATF_REQUIRE(ktrace("ktrace.out", KTROP_SET,
	    KTRFAC_CAPFAIL, cpid) != -1);
	/* Notify child that we've starting tracing. */
	ATF_REQUIRE(kill(cpid, SIGUSR1) != -1);
	/* Wait for child to raise violation and exit. */
	ATF_REQUIRE(waitpid(cpid, &error, 0) != -1);
	ATF_REQUIRE(WIFEXITED(error));
	ATF_REQUIRE_EQ(WEXITSTATUS(error), 0);
	/* Read ktrace header and ensure violation occurred. */
	for (i = 0; i < numv; ++i) {
		ATF_REQUIRE((error = read(fd, &header, sizeof(header))) != -1);
		ATF_REQUIRE_EQ(error, sizeof(header));
		ATF_REQUIRE_EQ(header.ktr_len, sizeof(*v));
		ATF_REQUIRE_EQ(header.ktr_pid, cpid);
		/* Read the capability violation. */
		ATF_REQUIRE((error = read(fd, v + i,
		    sizeof(*v))) != -1);
		ATF_REQUIRE_EQ(error, sizeof(*v));
	}
	ATF_REQUIRE(close(fd) != -1);
}

/*
 * Test if ktrace will record an operation that is done with
 * insufficient rights.
 */
ATF_TC_WITHOUT_HEAD(ktrace__cap_not_capable);
ATF_TC_BODY(ktrace__cap_not_capable, tc)
{
	struct ktr_cap_fail violation;
	cap_rights_t rights;
	sigset_t set = { };
	pid_t pid;
	int error;

	/* Block SIGUSR1 so child does not terminate. */
	ATF_REQUIRE(sigaddset(&set, SIGUSR1) != -1);
	ATF_REQUIRE(sigprocmask(SIG_BLOCK, &set, NULL) != -1);

	ATF_REQUIRE((pid = fork()) != -1);
	if (pid == 0) {
		/* Limit fd rights to CAP_READ. */
		cap_rights_init(&rights, CAP_READ);
		CHILD_REQUIRE(caph_rights_limit(STDIN_FILENO, &rights) != -1);
		CHILD_REQUIRE(caph_enter() != -1);
		/* Wait until ktrace has started. */
		CHILD_REQUIRE(sigwait(&set, &error) != -1);
		CHILD_REQUIRE_EQ(error, SIGUSR1);
		/* Write without CAP_WRITE. */
		CHILD_REQUIRE(write(STDIN_FILENO, &pid, sizeof(pid)) == -1);
		CHILD_REQUIRE_EQ(errno, ENOTCAPABLE);
		exit(0);
	}

	cap_trace_child(pid, &violation, 1);
	ATF_REQUIRE_EQ(violation.cap_type, CAPFAIL_NOTCAPABLE);
	ATF_REQUIRE(cap_rights_is_set(&violation.cap_data.cap_needed,
	    CAP_WRITE));
}

/*
 * Test if ktrace will record an attempt to increase rights.
 */
ATF_TC_WITHOUT_HEAD(ktrace__cap_increase_rights);
ATF_TC_BODY(ktrace__cap_increase_rights, tc)
{
	struct ktr_cap_fail violation;
	cap_rights_t rights;
	sigset_t set = { };
	pid_t pid;
	int error;

	/* Block SIGUSR1 so child does not terminate. */
	ATF_REQUIRE(sigaddset(&set, SIGUSR1) != -1);
	ATF_REQUIRE(sigprocmask(SIG_BLOCK, &set, NULL) != -1);

	ATF_REQUIRE((pid = fork()) != -1);
	if (pid == 0) {
		/* Limit fd rights to CAP_READ. */
		cap_rights_init(&rights, CAP_READ);
		CHILD_REQUIRE(caph_rights_limit(STDIN_FILENO, &rights) != -1);
		CHILD_REQUIRE(caph_enter() != -1);
		/* Wait until ktrace has started. */
		CHILD_REQUIRE(sigwait(&set, &error) != -1);
		CHILD_REQUIRE_EQ(error, SIGUSR1);
		/* Increase fd rights to include CAP_WRITE. */
		cap_rights_set(&rights, CAP_WRITE);
		CHILD_REQUIRE(caph_rights_limit(STDIN_FILENO, &rights) == -1);
		CHILD_REQUIRE_EQ(errno, ENOTCAPABLE);
		exit(0);
	}

	cap_trace_child(pid, &violation, 1);
	ATF_REQUIRE_EQ(violation.cap_type, CAPFAIL_INCREASE);
	ATF_REQUIRE(cap_rights_is_set(&violation.cap_data.cap_needed,
	    CAP_WRITE));
}

/*
 * Test if disallowed syscalls are reported as capability violations.
 */
ATF_TC_WITHOUT_HEAD(ktrace__cap_syscall);
ATF_TC_BODY(ktrace__cap_syscall, tc)
{
	struct kinfo_file kinf;
	struct ktr_cap_fail violation[2];
	sigset_t set = { };
	pid_t pid;
	int error;

	/* Block SIGUSR1 so child does not terminate. */
	ATF_REQUIRE(sigaddset(&set, SIGUSR1) != -1);
	ATF_REQUIRE(sigprocmask(SIG_BLOCK, &set, NULL) != -1);

	ATF_REQUIRE((pid = fork()) != -1);
	if (pid == 0) {
		/* Wait until ktrace has started. */
		CHILD_REQUIRE(sigwait(&set, &error) != -1);
		CHILD_REQUIRE_EQ(error, SIGUSR1);
		/* chdir() is not permitted in capability mode. */
		CHILD_REQUIRE(chdir(".") != -1);
		kinf.kf_structsize = sizeof(struct kinfo_file);
		/*
		 * fcntl() is permitted in capability mode,
		 * but the F_KINFO cmd is not.
		 */
		CHILD_REQUIRE(fcntl(STDIN_FILENO, F_KINFO, &kinf) != -1);
		exit(0);
	}

	cap_trace_child(pid, violation, 2);
	ATF_REQUIRE_EQ(violation[0].cap_type, CAPFAIL_SYSCALL);
	error = syscallabi(violation[0].cap_svflags);
	ATF_REQUIRE_STREQ(sysdecode_syscallname(error, violation[0].cap_code),
	    "chdir");

	ATF_REQUIRE_EQ(violation[1].cap_type, CAPFAIL_SYSCALL);
	error = syscallabi(violation[1].cap_svflags);
	ATF_REQUIRE_STREQ(sysdecode_syscallname(error, violation[1].cap_code),
	    "fcntl");
	ATF_REQUIRE_EQ(violation[1].cap_data.cap_int, F_KINFO);
}

/*
 * Test if sending a signal to another process is reported as
 * a signal violation.
 */
ATF_TC_WITHOUT_HEAD(ktrace__cap_signal);
ATF_TC_BODY(ktrace__cap_signal, tc)
{
	struct ktr_cap_fail violation;
	sigset_t set = { };
	pid_t pid;
	int error;

	/* Block SIGUSR1 so child does not terminate. */
	ATF_REQUIRE(sigaddset(&set, SIGUSR1) != -1);
	ATF_REQUIRE(sigprocmask(SIG_BLOCK, &set, NULL) != -1);

	ATF_REQUIRE((pid = fork()) != -1);
	if (pid == 0) {
		/* Wait until ktrace has started. */
		CHILD_REQUIRE(sigwait(&set, &error) != -1);
		CHILD_REQUIRE_EQ(error, SIGUSR1);
		/*
		 * Signals may only be sent to ourself. Sending signals
		 * to other processes is not allowed in capability mode.
		 */
		CHILD_REQUIRE(kill(getppid(), SIGCONT) != -1);
		exit(0);
	}

	cap_trace_child(pid, &violation, 1);
	ATF_REQUIRE_EQ(violation.cap_type, CAPFAIL_SIGNAL);
	error = syscallabi(violation.cap_svflags);
	ATF_REQUIRE_STREQ(sysdecode_syscallname(error, violation.cap_code),
	    "kill");
	ATF_REQUIRE_EQ(violation.cap_data.cap_int, SIGCONT);
}

/*
 * Test if opening a socket with a restricted protocol is reported
 * as a protocol violation.
 */
ATF_TC_WITHOUT_HEAD(ktrace__cap_proto);
ATF_TC_BODY(ktrace__cap_proto, tc)
{
	struct ktr_cap_fail violation;
	sigset_t set = { };
	pid_t pid;
	int error;

	/* Block SIGUSR1 so child does not terminate. */
	ATF_REQUIRE(sigaddset(&set, SIGUSR1) != -1);
	ATF_REQUIRE(sigprocmask(SIG_BLOCK, &set, NULL) != -1);

	ATF_REQUIRE((pid = fork()) != -1);
	if (pid == 0) {
		/* Wait until ktrace has started. */
		CHILD_REQUIRE(sigwait(&set, &error) != -1);
		CHILD_REQUIRE_EQ(error, SIGUSR1);
		/*
		 * Certain protocols may not be used in capability mode.
		 * ICMP's raw-protocol interface is not allowed.
		 */
		CHILD_REQUIRE(close(socket(AF_INET, SOCK_RAW,
		    IPPROTO_ICMP)) != -1);
		exit(0);
	}

	cap_trace_child(pid, &violation, 1);
	ATF_REQUIRE_EQ(violation.cap_type, CAPFAIL_PROTO);
	error = syscallabi(violation.cap_svflags);
	ATF_REQUIRE_STREQ(sysdecode_syscallname(error, violation.cap_code),
	    "socket");
	ATF_REQUIRE_EQ(violation.cap_data.cap_int, IPPROTO_ICMP);
}

/*
 * Test if sending data to an address using a socket is
 * reported as a sockaddr violation.
 */
ATF_TC_WITHOUT_HEAD(ktrace__cap_sockaddr);
ATF_TC_BODY(ktrace__cap_sockaddr, tc)
{
	struct sockaddr_in addr = { }, *saddr;
	struct ktr_cap_fail violation;
	sigset_t set = { };
	pid_t pid;
	int error, sfd;

	/* Block SIGUSR1 so child does not terminate. */
	ATF_REQUIRE(sigaddset(&set, SIGUSR1) != -1);
	ATF_REQUIRE(sigprocmask(SIG_BLOCK, &set, NULL) != -1);

	CHILD_REQUIRE((sfd = socket(AF_INET, SOCK_DGRAM,
	    IPPROTO_UDP)) != -1);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(5000);
	addr.sin_addr.s_addr = INADDR_ANY;
	CHILD_REQUIRE(bind(sfd, (const struct sockaddr *)&addr,
	    sizeof(addr)) != -1);

	ATF_REQUIRE((pid = fork()) != -1);
	if (pid == 0) {
		/* Wait until ktrace has started. */
		CHILD_REQUIRE(sigwait(&set, &error) != -1);
		CHILD_REQUIRE_EQ(error, SIGUSR1);
		/*
		 * Sending data to an address is not permitted.
		 * In this case, sending data to @addr causes a
		 * violation.
		 */
		CHILD_REQUIRE(sendto(sfd, NULL, 0, 0,
		    (const struct sockaddr *)&addr, sizeof(addr)) != -1);
		exit(0);
	}

	cap_trace_child(pid, &violation, 1);
	ATF_REQUIRE_EQ(violation.cap_type, CAPFAIL_SOCKADDR);
	error = syscallabi(violation.cap_svflags);
	ATF_REQUIRE_STREQ(sysdecode_syscallname(error, violation.cap_code),
	    "sendto");
	saddr = (struct sockaddr_in *)&violation.cap_data.cap_sockaddr;
	ATF_REQUIRE_EQ(saddr->sin_family, AF_INET);
	ATF_REQUIRE_EQ(saddr->sin_port, htons(5000));
	ATF_REQUIRE_EQ(saddr->sin_addr.s_addr, INADDR_ANY);
	close(sfd);
}

/*
 * Test if openat() with AT_FDCWD and absolute path are reported
 * as namei violations.
 */
ATF_TC_WITHOUT_HEAD(ktrace__cap_namei);
ATF_TC_BODY(ktrace__cap_namei, tc)
{
	struct ktr_cap_fail violation[2];
	sigset_t set = { };
	pid_t pid;
	int error;

	/* Block SIGUSR1 so child does not terminate. */
	ATF_REQUIRE(sigaddset(&set, SIGUSR1) != -1);
	ATF_REQUIRE(sigprocmask(SIG_BLOCK, &set, NULL) != -1);

	ATF_REQUIRE((pid = fork()) != -1);
	if (pid == 0) {
		/* Wait until ktrace has started. */
		CHILD_REQUIRE(sigwait(&set, &error) != -1);
		CHILD_REQUIRE_EQ(error, SIGUSR1);
		/*
		 * The AT_FDCWD file descriptor has not been opened
		 * and will be inaccessible in capability mode.
		 */
		CHILD_REQUIRE(close(openat(AT_FDCWD, "ktrace.out",
		    O_RDONLY | O_CREAT)) != -1);
		/*
		 * Absolute paths are inaccessible in capability mode.
		 */
		CHILD_REQUIRE(close(openat(-1, "/", O_RDONLY)) != -1);
		exit(0);
	}

	cap_trace_child(pid, violation, 2);
	ATF_REQUIRE_EQ(violation[0].cap_type, CAPFAIL_NAMEI);
	error = syscallabi(violation[0].cap_svflags);
	ATF_REQUIRE_STREQ(sysdecode_syscallname(error, violation[0].cap_code),
	    "openat");
	ATF_REQUIRE_STREQ(violation[0].cap_data.cap_path, "AT_FDCWD");

	ATF_REQUIRE_EQ(violation[1].cap_type, CAPFAIL_NAMEI);
	error = syscallabi(violation[1].cap_svflags);
	ATF_REQUIRE_STREQ(sysdecode_syscallname(error, violation[1].cap_code),
	    "openat");
	ATF_REQUIRE_STREQ(violation[1].cap_data.cap_path, "/");
}

/*
 * Test if changing another process's cpu set is recorded as
 * a cpuset violation.
 */
ATF_TC_WITHOUT_HEAD(ktrace__cap_cpuset);
ATF_TC_BODY(ktrace__cap_cpuset, tc)
{
	struct ktr_cap_fail violation;
	cpuset_t cpuset_mask = { };
	sigset_t set = { };
	pid_t pid;
	int error;

	/* Block SIGUSR1 so child does not terminate. */
	ATF_REQUIRE(sigaddset(&set, SIGUSR1) != -1);
	ATF_REQUIRE(sigprocmask(SIG_BLOCK, &set, NULL) != -1);

	ATF_REQUIRE((pid = fork()) != -1);
	if (pid == 0) {
		/* Wait until ktrace has started. */
		CHILD_REQUIRE(sigwait(&set, &error) != -1);
		CHILD_REQUIRE_EQ(error, SIGUSR1);
		/*
		 * Set cpu 0 affinity for parent process.
		 * Other process's cpu sets are restricted in capability
		 * mode, so this will raise a violation.
		 */
		CPU_SET(0, &cpuset_mask);
		CHILD_REQUIRE(cpuset_setaffinity(CPU_LEVEL_WHICH, CPU_WHICH_PID,
		    getppid(), sizeof(cpuset_mask), &cpuset_mask) != -1);
		exit(0);
	}

	cap_trace_child(pid, &violation, 1);
	ATF_REQUIRE_EQ(violation.cap_type, CAPFAIL_CPUSET);
	error = syscallabi(violation.cap_svflags);
	ATF_REQUIRE_STREQ(sysdecode_syscallname(error, violation.cap_code),
	    "cpuset_setaffinity");
}

ATF_TC_WITHOUT_HEAD(ktrace__cap_shm_open);
ATF_TC_BODY(ktrace__cap_shm_open, tc)
{
	struct ktr_cap_fail violation;
	sigset_t set = { };
	pid_t pid;
	int error;

	/* Block SIGUSR1 so child does not terminate. */
	ATF_REQUIRE(sigaddset(&set, SIGUSR1) != -1);
	ATF_REQUIRE(sigprocmask(SIG_BLOCK, &set, NULL) != -1);

	ATF_REQUIRE((pid = fork()) != -1);
	if (pid == 0) {
		/* Wait until ktrace has started. */
		CHILD_REQUIRE(sigwait(&set, &error) != -1);
		CHILD_REQUIRE_EQ(error, SIGUSR1);

		CHILD_REQUIRE(shm_open("/ktrace_shm", O_RDWR | O_CREAT,
		    0600) != -1);
		CHILD_REQUIRE(shm_unlink("/ktrace_shm") != -1);
		exit(0);
	}

	cap_trace_child(pid, &violation, 1);
	ATF_REQUIRE_EQ(violation.cap_type, CAPFAIL_NAMEI);
	error = syscallabi(violation.cap_svflags);
	ATF_REQUIRE_STREQ(sysdecode_syscallname(error, violation.cap_code),
	    "shm_open2");
	ATF_REQUIRE_STREQ(violation.cap_data.cap_path, "/ktrace_shm");
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, ktrace__cap_not_capable);
	ATF_TP_ADD_TC(tp, ktrace__cap_increase_rights);
	ATF_TP_ADD_TC(tp, ktrace__cap_syscall);
	ATF_TP_ADD_TC(tp, ktrace__cap_signal);
	ATF_TP_ADD_TC(tp, ktrace__cap_proto);
	ATF_TP_ADD_TC(tp, ktrace__cap_sockaddr);
	ATF_TP_ADD_TC(tp, ktrace__cap_namei);
	ATF_TP_ADD_TC(tp, ktrace__cap_cpuset);
	ATF_TP_ADD_TC(tp, ktrace__cap_shm_open);
	return (atf_no_error());
}
