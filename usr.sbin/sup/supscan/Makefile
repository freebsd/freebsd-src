# $Id: Makefile,v 1.3 1996/09/05 17:16:06 bde Exp $

PROG=	supscan
SRCS=	supscan.c
NOMAN=

LIBSRC=	${.CURDIR}/../lib
COPTS+=	-DVAR_TMP -DHAS_DAEMON -I${.CURDIR} -I${LIBSRC}
COPTS+=	-DRENAMELOG=\"/var/log/sup.rename\"

.if exists(${.OBJDIR}/../lib)
LIBOBJ=	${.OBJDIR}/../lib
.else
LIBOBJ=	${LIBSRC}
.endif

DPADD=	${LIBOBJ}/libsup.a ${LIBCRYPT}
LDADD=	-L${LIBOBJ} -lsup -lcrypt

.include <bsd.prog.mk>
