# $FreeBSD$

.include <bsd.own.mk>

MK_WERROR=	no
WARNS?=	2

CFLAGS.h_raw+=	-fstack-protector-all -Wstack-protector
.if ${COMPILER_TYPE} == "clang"
# Only use -fsanitize=bounds when using clang. Otherwise we are not able to
# override the sanitizer runtime libraries to be the ones installed on the
# target system.
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
# XXX: the h_raw/h_read testcases don't cause a SIGABRT with in-tree gcc right
# now on amd64 when it trips the stack bounds specified in t_ssp.sh . This
# probably needs to be fixed as it's currently hardcoded.
.if ${COMPILER_TYPE} == "clang" && !defined(_SKIP_BUILD) && \
    (!defined(_RECURSING_PROGS) || ${PROG} == "h_raw")
.include "${SRCTOP}/lib/libclang_rt/compiler-rt-vars.mk"
_libclang_rt_ubsan=	${SYSROOT}${SANITIZER_LIBDIR}/libclang_rt.ubsan_standalone-${CRTARCH}.a
.if exists(${_libclang_rt_ubsan})
PROGS+=		h_raw
LDADD.h_raw+=	${SANITIZER_LDFLAGS}
.else
.if make(all)
.info Could not find runtime library ${_libclang_rt_ubsan}, skipping h_raw
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
