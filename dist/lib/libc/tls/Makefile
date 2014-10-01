# $NetBSD: Makefile,v 1.4 2011/11/18 01:32:33 joerg Exp $

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/lib/libc/tls
.if (${MKPIC} == "yes")
SUBDIR+=	dso
.endif

TESTS_C+=	t_tls_static
.if (${MKPIC} == "yes")
TESTS_C+=	t_tls_dynamic t_tls_dlopen
.endif

DSODIR!=	cd ${.CURDIR}/../tls_dso && ${PRINTOBJDIR}

SRCS.t_tls_static=	t_tls_static.c t_tls_static_helper.c
DPADD.t_tls_static+=	${LIBPTHREAD}
LDADD.t_tls_static+=	-lpthread -static
DPADD.t_tls_dynamic+=	${LIBPTHREAD} ${DSODIR}/libh_tls_dynamic.so
LDADD.t_tls_dynamic+=	-lpthread \
			-Wl,-rpath,${TESTSDIR} -L${DSODIR} -lh_tls_dynamic
DPADD.t_tls_dlopen+=	${LIBPTHREAD}
LDADD.t_tls_dlopen+=	-lpthread -Wl,-rpath,${TESTSDIR} -Wl,-export-dynamic

.include <bsd.test.mk>
