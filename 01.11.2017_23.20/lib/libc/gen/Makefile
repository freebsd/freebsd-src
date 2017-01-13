# $NetBSD: Makefile,v 1.50 2016/12/09 06:12:02 kamil Exp $

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/lib/libc/gen

TESTS_SUBDIRS=	exect
TESTS_SUBDIRS+=	execve
TESTS_SUBDIRS+=	posix_spawn

TESTS_C+=	t_alarm
TESTS_C+=	t_assert
TESTS_C+=	t_basedirname
TESTS_C+=	t_closefrom
TESTS_C+=	t_cpuset
TESTS_C+=	t_dir
TESTS_C+=	t_floatunditf
TESTS_C+=	t_fmtcheck
TESTS_C+=	t_fnmatch
TESTS_C+=	t_fpclassify
TESTS_C+=	t_fpsetmask
TESTS_C+=	t_fpsetround
TESTS_C+=	t_ftok
TESTS_C+=	t_getcwd
TESTS_C+=	t_getgrent
TESTS_C+=	t_glob
TESTS_C+=	t_humanize_number
TESTS_C+=	t_isnan
TESTS_C+=	t_nice
TESTS_C+=	t_pause
TESTS_C+=	t_raise
TESTS_C+=	t_randomid
TESTS_C+=	t_realpath
TESTS_C+=	t_setdomainname
TESTS_C+=	t_sethostname
TESTS_C+=	t_siginfo
TESTS_C+=	t_sleep
TESTS_C+=	t_syslog
TESTS_C+=	t_time
TESTS_C+=	t_ttyname
TESTS_C+=	t_vis

CPPFLAGS.t_siginfo.c+=-D__TEST_FENV

LDADD.t_siginfo+=	-lm
DPADD.t_siginfo+=	${LIBM}
LDADD.t_fpclassify+=	-lm
DPADD.t_fpclassify+=	${LIBM}
LDADD.t_fpsetround+=	-lm
DPADD.t_fpsetround+=	${LIBM}

LDADD.t_nice+=		-lpthread
DPADD.t_nice+=		${LIBPTHREAD}
LDADD.t_syslog+=	-lpthread
DPADD.t_syslog+=	${LIBPTHREAD}

.include <bsd.test.mk>
