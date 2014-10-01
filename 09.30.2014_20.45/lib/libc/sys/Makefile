# $NetBSD: Makefile,v 1.36 2014/07/18 12:34:52 christos Exp $

MKMAN=	no

.include <bsd.own.mk>

.include "../arch/Makefile.exec_prot"

TESTSDIR=		${TESTSBASE}/lib/libc/sys

TESTS_C+=		t_access
TESTS_C+=		t_chroot
TESTS_C+=		t_clock_gettime
TESTS_C+=		t_clone
TESTS_C+=		t_connect
TESTS_C+=		t_dup
TESTS_C+=		t_fsync
TESTS_C+=		t_getcontext
TESTS_C+=		t_getgroups
TESTS_C+=		t_getitimer
TESTS_C+=		t_getlogin
TESTS_C+=		t_getpid
TESTS_C+=		t_getrusage
TESTS_C+=		t_getsid
TESTS_C+=		t_gettimeofday
TESTS_C+=		t_issetugid
TESTS_C+=		t_kevent
TESTS_C+=		t_kill
TESTS_C+=		t_link
TESTS_C+=		t_listen
TESTS_C+=		t_lwp_ctl
TESTS_C+=		t_lwp_create
TESTS_C+=		t_minherit
TESTS_C+=		t_mincore
TESTS_C+=		t_mkdir
TESTS_C+=		t_mkfifo
TESTS_C+=		t_mknod
TESTS_C+=		t_mlock
TESTS_C+=		t_mmap
TESTS_C+=		t_mprotect
TESTS_C+=		t_msgctl
TESTS_C+=		t_msgget
TESTS_C+=		t_msgrcv
TESTS_C+=		t_msgsnd
TESTS_C+=		t_msync
TESTS_C+=		t_nanosleep
TESTS_C+=		t_pipe
TESTS_C+=		t_pipe2
TESTS_C+=		t_poll
TESTS_C+=		t_recvmmsg
TESTS_C+=		t_revoke
TESTS_C+=		t_select
TESTS_C+=		t_setrlimit
TESTS_C+=		t_setuid
TESTS_C+=		t_sigaction
TESTS_C+=		t_sigqueue
TESTS_C+=		t_sigtimedwait
TESTS_C+=		t_socketpair
TESTS_C+=		t_swapcontext
TESTS_C+=		t_stat
TESTS_C+=		t_timer_create
TESTS_C+=		t_truncate
TESTS_C+=		t_ucontext
TESTS_C+=		t_umask
TESTS_C+=		t_unlink
TESTS_C+=		t_write

SRCS.t_mprotect=	t_mprotect.c ${SRCS_EXEC_PROT}

LDADD.t_getpid+=        -lpthread

.if (${MKRUMP} != "no")
TESTS_C+=		t_posix_fadvise
LDADD.t_posix_fadvise+= -lrumpvfs -lrump -lrumpuser -lrump -lpthread
.endif

WARNS=			4

.include <bsd.test.mk>

