#
# $Id$
#

PERL5SRC=	${.CURDIR}/../../../../../contrib/perl5
MINIPERLOPT?=	-I${.OBJDIR}/../../perl/lib -I${.OBJDIR}/../../perl

STRIP=

.include "../Makefile.inc"
