PROG= pkg_create

CFLAGS+=	${DEBUG} -I${.CURDIR}/../lib
LDADD+=		-L${.CURDIR}/../lib -L${.CURDIR}/../lib/obj -linstall


SRCS= main.c perform.c pl.c

.include <bsd.prog.mk>
