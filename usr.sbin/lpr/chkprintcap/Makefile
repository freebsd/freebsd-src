#	$Id: Makefile,v 1.4 1998/05/13 06:50:09 jb Exp $

PROG=	chkprintcap
MAN8=	chkprintcap.8
CFLAGS+=-I${.CURDIR}/../common_source -Wall
.PATH:	${.CURDIR}/../common_source
DPADD=	${LIBLPR}
LDADD=	${LIBLPR}

.include <bsd.prog.mk>
