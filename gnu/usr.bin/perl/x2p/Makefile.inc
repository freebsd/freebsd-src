#
# $Id$
#

PERL5SRC=	${.CURDIR}/../../../../../contrib/perl5
MINIPERLOPT?=	-I${.OBJDIR}/../../miniperl/lib -I${.OBJDIR}/../../miniperl

STRIP=

.include "../Makefile.inc"
