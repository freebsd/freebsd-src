#	$Id: Makefile,v 1.3 1998/03/07 09:47:55 bde Exp $

PROG=	chkprintcap
MAN8=	chkprintcap.8
CFLAGS+=-I${.CURDIR}/../common_source -Wall
.PATH:	${.CURDIR}/../common_source
DPADD=	${LIBLPR}
LDADD=	${LIBLPR}

.if ${MACHINE_ARCH} == "i386"
CFLAGS+=	-Werror
.endif

.include <bsd.prog.mk>
