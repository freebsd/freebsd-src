#
# $FreeBSD: src/gnu/usr.bin/perl/x2p/Makefile.inc,v 1.5.2.2 2002/02/19 16:21:35 ru Exp $
#

PERL5SRC=	${.CURDIR}/../../../../../contrib/perl5
PERL5LIBSRC=	${.CURDIR}/../../libperl
MINIPERLOPT?=	-I${.OBJDIR}/../../perl/lib -I${.OBJDIR}/../../perl

LDFLAGS+=	-L${.OBJDIR}/../../libperl

STRIP=

.include "../Makefile.inc"
