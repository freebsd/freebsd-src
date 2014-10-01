# $NetBSD: Makefile,v 1.23 2013/02/16 21:40:07 jmmv Exp $

.include <bsd.own.mk>

TESTS_SUBDIRS=	csu libbluetooth libc libcrypt libcurses libevent libexecinfo \
		libm libobjc libposix libppath libprop libpthread \
		librt libtre libutil semaphore

.if (${MKRUMP} != "no")
TESTS_SUBDIRS+= librumpclient librumphijack
.endif

.if ${MKCRYPTO} != "no"
TESTS_SUBDIRS+= libdes
.endif

.if ${MKKYUA} != "no"
# These tests come from external/bsd/lutok/tests/lib/liblutok.  Ensure they
# get registered in the Atffile only when installed.
ATFFILE_EXTRA_TPS=	liblutok
.endif

.if (${MKSKEY} != "no")
TESTS_SUBDIRS+= libskey
.endif

.if ${MKSLJIT} != "no"
TESTS_SUBDIRS+=	libsljit
TESTS_SUBDIRS+=	libbpfjit
.endif

TESTSDIR=	${TESTSBASE}/lib

.include <bsd.test.mk>
