# $Id: Makefile,v 1.4 1996/06/24 04:25:59 jkh Exp $

PROG=	supfilesrv
SRCS=	supfilesrv.c
MAN8=	supservers.8
MLINKS=	supservers.8 supfilesrv.8 supservers.8 supscan.8

LIBSRC=	${.CURDIR}/../lib
COPTS+=	-DVAR_TMP -DHAS_DAEMON -I${.CURDIR} -I${LIBSRC}
COPTS+=	-DRENAMELOG=\"/var/log/sup.rename\"

.if exists(${.OBJDIR}/../lib)
LIBOBJ=	${.OBJDIR}/../lib
.else
LIBOBJ=	${LIBSRC}
.endif

DPADD=	$(LIBOBJ)/libsup.a $(LIBCRYPT) $(LIBUTIL)
LDADD=	-L${LIBOBJ} -lsup -lcrypt -lutil

.include <bsd.prog.mk>
