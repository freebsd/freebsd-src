#
# $FreeBSD$
#

PERL5SRC?=	${.CURDIR}/../../../../../contrib/perl5
MINIPERLOPT?=	-I${.OBJDIR}/../../perl/lib -I${.OBJDIR}/../../perl
MINIPERL=	miniperl

CLEANFILES+=	${PROG}.1 ${PROG}.PL

.ORDER: ${PROG}.PL ${PROG} ${PROG}.1 ${PROG}.1.gz

STRIP=

.include "../Makefile.inc"
