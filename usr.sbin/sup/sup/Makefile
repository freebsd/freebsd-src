# $Id$

PROG=	sup
SRCS=	supcmain.c supcvers.c supcparse.c supcname.c supcmisc.c supcmeat.c

LIBSRC=	${.CURDIR}/../lib
COPTS+=	-DVAR_TMP -DHAS_DAEMON -I${.CURDIR} -I${LIBSRC}
COPTS+=	-DRENAMELOG=\"/var/log/sup.rename\"

.if exists(${LIBSRC}/obj/.)
LIBOBJ=	${LIBSRC}/obj
.else
LIBOBJ=	${LIBSRC}
.endif

DPADD+=	$(LIBCRYPT) $(LIBOBJ)/libsup.a
LDADD+= -L${LIBOBJ} -lsup -lcrypt

.include <bsd.prog.mk>
