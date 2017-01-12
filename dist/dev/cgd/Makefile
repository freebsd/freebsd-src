#	$NetBSD: Makefile,v 1.7 2016/11/11 07:39:58 alnsn Exp $
#

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/dev/cgd
FILES=		paramsfile
FILESDIR=	${TESTSDIR}

TESTS_SH=	t_cgd

.if ${MKCRYPTO} != "no" && ${MKRUMP} != "no"
TESTS_C=	t_cgd_3des t_cgd_aes t_cgd_blowfish

WARNS=	4
CPPFLAGS+=	-D_KERNTYPES
LDADD+=	-lrumpdev -lrumpdev_disk -lrumpdev_cgd -lrumpkern_crypto -lrumpvfs
LDADD+=	-lrump
LDADD+=	-lrumpuser
LDADD+=	-lrump -lutil
LDADD+=	-lpthread
.endif

.include <bsd.test.mk>
