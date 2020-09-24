# $FreeBSD$

PACKAGE=			tests

.include <bsd.own.mk>

.if ${MACHINE_CPUARCH} != "aarch64" && ${MACHINE_CPUARCH} != "riscv"
ATF_TESTS_C+=			brk_test
.endif
ATF_TESTS_C+=			queue_test
ATF_TESTS_C+=			sendfile_test

# TODO: clone, lwp_create, lwp_ctl, posix_fadvise, recvmmsg,
# swapcontext
NETBSD_ATF_TESTS_C+=		access_test
NETBSD_ATF_TESTS_C+=		bind_test
NETBSD_ATF_TESTS_C+=		chroot_test
NETBSD_ATF_TESTS_C+=		clock_gettime_test
NETBSD_ATF_TESTS_C+=		clock_nanosleep_test
NETBSD_ATF_TESTS_C+=		connect_test
NETBSD_ATF_TESTS_C+=		dup_test
NETBSD_ATF_TESTS_C+=		fsync_test
NETBSD_ATF_TESTS_C+=		getcontext_test
NETBSD_ATF_TESTS_C+=		getgroups_test
NETBSD_ATF_TESTS_C+=		getitimer_test
NETBSD_ATF_TESTS_C+=		getlogin_test
NETBSD_ATF_TESTS_C+=		getpid_test
NETBSD_ATF_TESTS_C+=		getrusage_test
NETBSD_ATF_TESTS_C+=		getsid_test
NETBSD_ATF_TESTS_C+=		getsockname_test
NETBSD_ATF_TESTS_C+=		gettimeofday_test
NETBSD_ATF_TESTS_C+=		issetugid_test
NETBSD_ATF_TESTS_C+=		kevent_test
NETBSD_ATF_TESTS_C+=		kill_test
NETBSD_ATF_TESTS_C+=		link_test
NETBSD_ATF_TESTS_C+=		listen_test
NETBSD_ATF_TESTS_C+=		mincore_test
NETBSD_ATF_TESTS_C+=		mkdir_test
NETBSD_ATF_TESTS_C+=		mkfifo_test
NETBSD_ATF_TESTS_C+=		mknod_test
NETBSD_ATF_TESTS_C+=		mlock_test
NETBSD_ATF_TESTS_C+=		mmap_test
NETBSD_ATF_TESTS_C+=		mprotect_test
NETBSD_ATF_TESTS_C+=		msgctl_test
NETBSD_ATF_TESTS_C+=		msgget_test
NETBSD_ATF_TESTS_C+=		msgrcv_test
NETBSD_ATF_TESTS_C+=		msgsnd_test
NETBSD_ATF_TESTS_C+=		msync_test
NETBSD_ATF_TESTS_C+=		nanosleep_test
NETBSD_ATF_TESTS_C+=		pipe_test
NETBSD_ATF_TESTS_C+=		pipe2_test
NETBSD_ATF_TESTS_C+=		poll_test
NETBSD_ATF_TESTS_C+=		posix_fallocate_test
NETBSD_ATF_TESTS_C+=		revoke_test
NETBSD_ATF_TESTS_C+=		select_test
NETBSD_ATF_TESTS_C+=		setrlimit_test
NETBSD_ATF_TESTS_C+=		setuid_test
NETBSD_ATF_TESTS_C+=		sigaction_test
NETBSD_ATF_TESTS_C+=		sigqueue_test
NETBSD_ATF_TESTS_C+=		sigtimedwait_test
NETBSD_ATF_TESTS_C+=		socketpair_test
NETBSD_ATF_TESTS_C+=		stat_test
NETBSD_ATF_TESTS_C+=		timer_create_test
NETBSD_ATF_TESTS_C+=		truncate_test
NETBSD_ATF_TESTS_C+=		ucontext_test
NETBSD_ATF_TESTS_C+=		umask_test
NETBSD_ATF_TESTS_C+=		unlink_test
NETBSD_ATF_TESTS_C+=		wait_test
NETBSD_ATF_TESTS_C+=		wait_noproc_test
NETBSD_ATF_TESTS_C+=		wait_noproc_wnohang_test
NETBSD_ATF_TESTS_C+=		write_test

LIBADD.getpid_test+=		pthread
LIBADD.timer_create_test+=	rt

.include "../Makefile.netbsd-tests"

SRCS.mlock_test+=		mlock_helper.c
SRCS.setrlimit_test+=		mlock_helper.c

FILESGROUPS+=		truncate_test_FILES

truncate_test_FILES=	truncate_test.root_owned
truncate_test_FILESDIR=	${TESTSDIR}
truncate_test_FILESMODE= 0600
truncate_test_FILESOWN= root
truncate_test_FILESGRP= wheel
truncate_test_FILESPACKAGE=	${PACKAGE}

CLEANFILES=	truncate_test.root_owned
# The dd status=none option is non-standard. Only use it when this test succeeds
# rather than require dd to be a bootstrap tool.
DD_NOSTATUS!=(dd status=none count=0 2> /dev/null && echo status=none) || true
DD=dd ${DD_NOSTATUS}
truncate_test.root_owned:
	${DD} if=/dev/null bs=1 count=1 of=${.TARGET}

.include <bsd.test.mk>
