#
# $FreeBSD$
#

PERL5SRC=	${.CURDIR}/../../../../../contrib/perl5
PERL5LIB=	${.CURDIR}/../../libperl
MINIPERLOPT?=	-I${.OBJDIR}/../../perl/lib -I${.OBJDIR}/../../perl

STRIP=

.include "../Makefile.inc"
