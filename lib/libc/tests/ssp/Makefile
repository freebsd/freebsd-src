# $FreeBSD$

.include <bsd.own.mk>

NO_WERROR=
WARNS?=	2

CFLAGS.h_raw+=	-fstack-protector-all -Wstack-protector
.if ${COMPILER_TYPE} == "clang"
CFLAGS.h_raw+=	-fsanitize=bounds
.elif ${COMPILER_TYPE} == "gcc"
CFLAGS.h_raw+=	--param ssp-buffer-size=1
LDADD+=	-lssp
.endif

NETBSD_ATF_TESTS_SH=	ssp_test

BINDIR=		${TESTSDIR}

PROGS=		h_fgets
PROGS+=		h_gets
PROGS+=		h_getcwd
PROGS+=		h_memcpy
PROGS+=		h_memmove
PROGS+=		h_memset
# This testcase doesn't run properly when not compiled with -fsantize=bounds
# with clang, which is currently contingent on a compiler_rt update
#
# XXX: the h_raw/h_read testcases don't cause a SIGABRT with in-tree gcc right
# now on amd64 when it trips the stack bounds specified in t_ssp.sh . This
# probably needs to be fixed as it's currently hardcoded.
#
# sanitizer is not tested or supported for ARM right now. sbruno
.if ${MACHINE_CPUARCH} == "i386" || ${MACHINE_CPUARCH} == "amd64"
.if ${COMPILER_TYPE} == "clang" && ${MK_TOOLCHAIN} == "yes"
.if ${COMPILER_VERSION} < 30500 || 30700 <= ${COMPILER_VERSION}
PROGS+=		h_raw
.endif
.endif
.endif
PROGS+=		h_read
PROGS+=		h_readlink
PROGS+=		h_snprintf
PROGS+=		h_sprintf
PROGS+=		h_stpcpy
PROGS+=		h_stpncpy
PROGS+=		h_strcat
PROGS+=		h_strcpy
PROGS+=		h_strncat
PROGS+=		h_strncpy
PROGS+=		h_vsnprintf
PROGS+=		h_vsprintf

.include "../Makefile.netbsd-tests"

.include <bsd.test.mk>
