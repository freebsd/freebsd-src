PROG = fetch
SRCS = file.c ftp.c http.c main.c util.c uri.c

CFLAGS+=	-Wall -Wwrite-strings -Wmissing-prototypes

DPADD=  ${LIBFTPIO} ${LIBMD}
LDADD=  -lftpio -lmd

.include <bsd.prog.mk>
