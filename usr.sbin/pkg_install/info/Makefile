PROG= pkg_info
CFLAGS+=	${DEBUG} -I${.CURDIR}/../lib

.if exists(${.OBJDIR}/../lib)
LDADD+=		-L${.OBJDIR}/../lib -linstall
DPADD+=		${.OBJDIR}/../lib/libinstall.a
.else
LDADD+=		-L${.CURDIR}/../lib -linstall
DPADD+=		${.CURDIR}/../lib/libinstall.a
.endif

LDADD+=         -lftpio -lmd
DPADD+=         ${LIBFTPIO} ${LIBMD}

SRCS= main.c perform.c show.c

.include <bsd.prog.mk>
