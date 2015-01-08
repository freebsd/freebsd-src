# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/lib/libc/ssp

NO_WERROR=
WARNS?=	2

CFLAGS.h_raw+=	-fstack-protector-all -Wstack-protector
.if ${COMPILER_TYPE} == "clang"
# Disable -fsanitize=bounds until runtime support is done for clang 3.5.0.
.if ${COMPILER_VERSION} < 30500
CFLAGS.h_raw+=	-fsanitize=bounds
.endif
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
PROGS+=		h_raw
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
