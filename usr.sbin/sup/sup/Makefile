# $Id: Makefile,v 1.1 1995/12/26 05:15:18 peter Exp $

# sup is a user level
BINDIR=	${DESTDIR}/usr/bin

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
