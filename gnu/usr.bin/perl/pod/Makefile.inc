#
# $FreeBSD: src/gnu/usr.bin/perl/pod/Makefile.inc,v 1.1.2.1 1999/08/29 14:34:17 peter Exp $
#

PERL5SRC?=	${.CURDIR}/../../../../../contrib/perl5
MINIPERLOPT?=	-I${.OBJDIR}/../../perl/lib -I${.OBJDIR}/../../perl

CLEANFILES+=	${PROG}.1 ${PROG}.PL

.ORDER: ${PROG}.PL ${PROG} ${PROG}.1 ${PROG}.1.gz

STRIP=

.include "../Makefile.inc"
