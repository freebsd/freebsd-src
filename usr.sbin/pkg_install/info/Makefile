PROG= pkg_info
CFLAGS+=	${DEBUG} -I${.CURDIR}/../lib

.if exists(${.CURDIR}/../lib/obj)
LDADD+=		-L${.CURDIR}/../lib/obj -linstall
DPADD+=		${.CURDIR}/../lib/obj/libinstall.a
.else
LDADD+=		-L${.CURDIR}/../lib -linstall
DPADD+=		${.CURDIR}/../lib/libinstall.a
.endif

LDADD+=         -lftpio
DPADD+=         ${LIBFTPIO}

SRCS= main.c perform.c show.c

.include <bsd.prog.mk>
