# $NetBSD: Makefile,v 1.44 2013/02/25 00:33:19 jmmv Exp $

.include <bsd.own.mk>

.if ${MKATF} != "no"

TESTSDIR=	${TESTSBASE}

TESTS_SUBDIRS=		bin dev games include kernel lib libexec net
TESTS_SUBDIRS+=		sbin sys usr.bin usr.sbin

. if (${MKRUMP} != "no")
TESTS_SUBDIRS+=		fs rump

. if ${MKKMOD} != "no"
TESTS_SUBDIRS+=		modules
. endif
. endif

. if ${MKCRYPTO} != "no"
TESTS_SUBDIRS+=		crypto
. endif

. if ${MKIPFILTER} != "no"
TESTS_SUBDIRS+=		ipf
. endif

. if ${MKSHARE} != "no"
TESTS_SUBDIRS+=		share
. endif

. if ${MKATF} != "no"
ATFFILE_EXTRA_SUBDIRS+=	atf
. endif

. if ${MKKYUA} != "no"
ATFFILE_EXTRA_SUBDIRS+=	kyua-atf-compat kyua-cli kyua-testers
. endif

.include <bsd.test.mk>

.else

.include <bsd.subdir.mk>
.endif
