#	$NetBSD: Makefile,v 1.13 2009/04/14 22:15:20 lukem Exp $
# $FreeBSD$

.include <bsd.own.mk>

PROG=		gzip
MAN=		gzip.1 gzexe.1 zdiff.1 zforce.1 zmore.1 znew.1

DPADD=		${LIBZ}
LDADD=		-lz
WARNS?=		6

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
		zdiff.1 zcmp.1

LINKS+=		${BINDIR}/gzip ${BINDIR}/gunzip \
		${BINDIR}/gzip ${BINDIR}/gzcat \
		${BINDIR}/gzip ${BINDIR}/zcat \
		${BINDIR}/zdiff ${BINDIR}/zcmp

.include <bsd.prog.mk>
