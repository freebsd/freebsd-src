# $FreeBSD$

.include <bsd.own.mk>

.if !defined(NO_PIC)
SUBDIR+=	dso
.endif

# TODO: doesn't link properly (for some odd reason it's trying to link in
#       libatf.so)
#NETBSD_ATF_TESTS_C=	tls_static_test
.if !defined(NO_PIC)
NETBSD_ATF_TESTS_C+=	tls_dlopen_test
NETBSD_ATF_TESTS_C+=	tls_dynamic_test
.endif

.include "../Makefile.netbsd-tests"

DSODIR=		${.OBJDIR}/../tls_dso

DPADD.tls_static_test+=		${LIBPTHREAD}
LDADD.tls_static_test+=		-lpthread
LDFLAGS.tls_static_test+=	-static
SRCS.tls_static_test=		t_tls_static.c t_tls_static_helper.c

DPADD.tls_dynamic_test+=	${LIBPTHREAD} ${DSODIR}/libh_tls_dynamic.so
LDADD.tls_dynamic_test+=	-lpthread -lh_tls_dynamic
LDFLAGS.tls_dynamic_test+=	-Wl,-rpath,${TESTSDIR} -L${DSODIR}

DPADD.tls_dlopen_test+=		${LIBPTHREAD}
LDADD.tls_dlopen_test+=		-lpthread
LDFLAGS.tls_dlopen_test+=	-Wl,-rpath,${TESTSDIR} -Wl,-export-dynamic

.include <bsd.test.mk>
