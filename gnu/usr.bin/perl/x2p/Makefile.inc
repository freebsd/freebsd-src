#
# $FreeBSD: src/gnu/usr.bin/perl/x2p/Makefile.inc,v 1.5 1999/09/25 06:59:10 markm Exp $
#

PERL5SRC=	${.CURDIR}/../../../../../contrib/perl5
PERL5LIBSRC=	${.CURDIR}/../../libperl
MINIPERLOPT?=	-I${.OBJDIR}/../../perl/lib -I${.OBJDIR}/../../perl

STRIP=

.include "../Makefile.inc"
