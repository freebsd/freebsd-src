#	$NetBSD: Makefile,v 1.8 2016/10/14 16:02:35 christos Exp $
#

.include <bsd.init.mk>

TESTSDIR=	${TESTSBASE}/rump/modautoload

TESTS_C=	t_modautoload
PAXCTL_FLAGS=	+ma

SRCS.t_modautoload+=	t_modautoload.c

# Note: we link the rump kernel into the application to make this work
# on amd64.  This is the reason we keep this test in its own
# subdirectory -- otherwise the LDADD lines would get a little hairy.
LDFLAGS+=	-Wl,-E
LDADD+=		-Wl,--whole-archive	${DESTDIR}/usr/lib/librumpvfs.a	\
					${DESTDIR}/usr/lib/librump.a	\
		-Wl,--no-whole-archive
LDADD+=		-lrumpuser -lpthread
DPADD+=		${LIBRUMPVFS} ${LIBRUMP} ${LIBRUMPUSER}

WARNS=	4

# To include a rump version of sysctlbyname()

.PATH:		${NETBSDSRCDIR}/lib/libc/gen
SRCS.t_modautoload+=		sysctlbyname.c
SRCS.t_modautoload+=		sysctlgetmibinfo.c
CPPFLAGS+=	-DRUMP_ACTION

.include <bsd.test.mk>
