#
# $FreeBSD: src/gnu/usr.bin/perl/x2p/Makefile.inc,v 1.5.2.1 2000/10/05 09:54:56 markm Exp $
#

PERL5SRC=	${.CURDIR}/../../../../../contrib/perl5
PERL5LIBSRC=	${.CURDIR}/../../libperl
MINIPERLOPT?=	-I${.OBJDIR}/../../perl/lib -I${.OBJDIR}/../../perl
MINIPERL=	${.OBJDIR}/../../miniperl/miniperl

LDFLAGS+=	-L${.OBJDIR}/../../libperl

STRIP=

.include "../Makefile.inc"
