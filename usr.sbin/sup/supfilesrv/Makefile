# $Id$

PROG=	supfilesrv
SRCS=	supfilesrv.c
MAN8=	supservers.8
MLINKS=	supservers.8 supfilesrv.8 supservers.8 supscan.8

LIBSRC=	${.CURDIR}/../lib
COPTS+=	-DVAR_TMP -DHAS_DAEMON -I${.CURDIR} -I${LIBSRC}
COPTS+=	-DRENAMELOG=\"/var/log/sup.rename\"

.if exists(${LIBSRC}/obj/.)
LIBOBJ=	${LIBSRC}/obj
.else
LIBOBJ=	${LIBSRC}
.endif

DPADD+=	$(LIBCRYPT) $(LIBOBJ)/libsup.a $(LIBUTIL)
LDADD+= -L${LIBOBJ} -lsup -lcrypt -lutil

.include <bsd.prog.mk>
