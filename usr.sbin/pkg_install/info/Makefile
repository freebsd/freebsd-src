PROG= pkg_info

CFLAGS+=	${DEBUG} -I${.CURDIR}/../lib
LDADD+=		-L${.CURDIR}/../lib -L${.CURDIR}/../lib/obj -linstall


SRCS= main.c perform.c show.c

.include <bsd.prog.mk>
