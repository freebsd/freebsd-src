#
# $FreeBSD$
#

PERL5SRC=	${.CURDIR}/../../../../../contrib/perl5
PERL5LIBSRC=	${.CURDIR}/../../libperl
MINIPERLOPT?=	-I${.OBJDIR}/../../perl/lib -I${.OBJDIR}/../../perl
MINIPERL=	miniperl

LDFLAGS+=	-L${.OBJDIR}/../../libperl

STRIP=

.include "../Makefile.inc"
