#
# $Id: Makefile.inc,v 1.2 1998/09/10 10:51:10 markm Exp $
#

PERL5SRC=	${.CURDIR}/../../../../../contrib/perl5
MINIPERLOPT?=	-I${.OBJDIR}/../../perl/lib -I${.OBJDIR}/../../perl

STRIP=

.include "../Makefile.inc"
