#	$Id: Makefile,v 1.1 1997/12/02 20:45:07 wollman Exp $

PROG=	chkprintcap
MAN8=	chkprintcap.8
CFLAGS+=-I${.CURDIR}/../common_source -Wall -Werror
.PATH:	${.CURDIR}/../common_source
DPADD=	${.OBJDIR}/../common_source/liblpr.a
LDADD=	-L${.OBJDIR}/../common_source -llpr

.include "../../Makefile.inc"
.include <bsd.prog.mk>
