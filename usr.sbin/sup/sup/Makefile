# $Id: Makefile,v 1.4 1996/06/24 04:25:57 jkh Exp $

PROG=	sup
SRCS=	supcmain.c supcvers.c supcparse.c supcname.c supcmisc.c supcmeat.c

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
