#
# $FreeBSD: src/gnu/usr.bin/perl/x2p/Makefile.inc,v 1.2.2.1 1999/08/29 14:34:37 peter Exp $
#

PERL5SRC=	${.CURDIR}/../../../../../contrib/perl5
MINIPERLOPT?=	-I${.OBJDIR}/../../perl/lib -I${.OBJDIR}/../../perl

STRIP=

.include "../Makefile.inc"
