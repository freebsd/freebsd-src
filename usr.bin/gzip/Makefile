#	$NetBSD: Makefile,v 1.18 2013/11/13 11:12:24 pettai Exp $
# $FreeBSD$

.include <bsd.own.mk>

PROG=		gzip
MAN=		gzip.1 gzexe.1 zdiff.1 zforce.1 zmore.1 znew.1

DPADD=		${LIBZ} ${LIBLZMA} ${LIBPTHREAD}
LDADD=		-lz -llzma -lpthread

.if ${MK_BZIP2_SUPPORT} != "no"
DPADD+=		${LIBBZ2}
LDADD+=		-lbz2
.else
CFLAGS+=	-DNO_BZIP2_SUPPORT
.endif

SCRIPTS=	gzexe zdiff zforce zmore znew

MLINKS+=	gzip.1 gunzip.1 \
		gzip.1 gzcat.1 \
		gzip.1 zcat.1 \
		zdiff.1 zcmp.1 \
		zmore.1 zless.1

LINKS+=		${BINDIR}/gzip ${BINDIR}/gunzip \
		${BINDIR}/gzip ${BINDIR}/gzcat \
		${BINDIR}/gzip ${BINDIR}/zcat \
		${BINDIR}/zdiff ${BINDIR}/zcmp

.if ${MK_TESTS} != "no"
SUBDIR+=	tests
.endif

.include <bsd.prog.mk>
