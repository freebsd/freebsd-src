PROG= pkg_info

CFLAGS+=	-I${.CURDIR}/../lib
LDADD+=		-L${.CURDIR}/../lib -linstall

SRCS= main.c perform.c show.c

.include <bsd.prog.mk>
