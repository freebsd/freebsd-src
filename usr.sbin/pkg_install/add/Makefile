PROG= pkg_add

CFLAGS+=	${DEBUG} -I${.CURDIR}/../lib
LDADD+=		-L${.CURDIR}/../lib -linstall

SRCS= main.c perform.c futil.c extract.c

.include <bsd.prog.mk>
