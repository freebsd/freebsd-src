# $FreeBSD: src/gnu/usr.bin/cvs/Makefile.inc,v 1.6.22.1.2.1 2009/10/25 01:10:29 kensmith Exp $

.if !defined(CVSDIR)

CVSDIR=		$(.CURDIR)/../../../../contrib/cvs

LIBCVSDIR=	${.OBJDIR}/../lib
LIBCVS=		${LIBCVSDIR}/libcvs.a

LIBDIFFDIR=	${.OBJDIR}/../libdiff
LIBDIFF=	${LIBDIFFDIR}/libdiff.a

.if exists(${.CURDIR}/../../Makefile.inc)
.include "${.CURDIR}/../../Makefile.inc"
.endif

.endif
