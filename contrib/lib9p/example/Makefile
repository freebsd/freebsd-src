PROG=	server
SRCS=	server.c
MAN=

CFLAGS=	-pthread -g -O0

LDFLAGS=-L..
LIBADD+=sbuf 9p casper cap_pwd cap_grp

.include <bsd.prog.mk>
