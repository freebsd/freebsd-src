# $Id: Makefile,v 1.1 1995/12/26 05:15:20 peter Exp $

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

DPADD+=	$(LIBCRYPT) $(LIBOBJ)/libsup.a
LDADD+= -L${LIBOBJ} -lsup -lcrypt

.include <bsd.prog.mk>
