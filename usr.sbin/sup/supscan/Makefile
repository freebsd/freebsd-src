# $Id: Makefile,v 1.2 1996/06/24 04:26:03 jkh Exp $

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

DPADD=	$(LIBCRYPT) $(LIBOBJ)/libsup.a ${LIBCRYPT}
LDADD=	-L${LIBOBJ} -lsup -lcrypt

.include <bsd.prog.mk>
