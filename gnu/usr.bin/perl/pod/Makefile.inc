#
# $FreeBSD: src/gnu/usr.bin/perl/pod/Makefile.inc,v 1.2.2.1 2000/10/05 09:54:48 markm Exp $
#

PERL5SRC?=	${.CURDIR}/../../../../../contrib/perl5
MINIPERLOPT?=	-I${.OBJDIR}/../../perl/lib -I${.OBJDIR}/../../perl
MINIPERL=	${.OBJDIR}/../../miniperl/miniperl

CLEANFILES+=	${PROG}.1 ${PROG}.PL

.ORDER: ${PROG}.PL ${PROG} ${PROG}.1 ${PROG}.1.gz

STRIP=

.include "../Makefile.inc"
