PROG= pkg_delete

CFLAGS+=	${DEBUG} -I${.CURDIR}/../lib
LDADD+=		-L${.CURDIR}/../lib -L${.CURDIR}/../lib/obj -linstall


SRCS= main.c perform.c

.include <bsd.prog.mk>
