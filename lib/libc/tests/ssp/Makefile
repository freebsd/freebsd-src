# $FreeBSD$

.include <bsd.own.mk>

NO_WERROR=
WARNS?=	2

CFLAGS.h_raw+=	-fstack-protector-all -Wstack-protector
.if ${COMPILER_TYPE} == "clang"
CFLAGS.h_raw+=	-fsanitize=bounds
.elif ${COMPILER_TYPE} == "gcc"
CFLAGS.h_raw+=	--param ssp-buffer-size=1
DPADD+=		${LIBSSP}
LDADD+=		-lssp
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
.if ${COMPILER_TYPE} == "clang" && ${COMPILER_VERSION} < 30500
PROGS+=		h_raw
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
