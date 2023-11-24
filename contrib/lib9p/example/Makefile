PROG=	server
SRCS=	server.c
MAN=

CFLAGS=	-pthread -g -O0

LDFLAGS=-L..
LDADD=	-lsbuf -l9p -lcasper -lcap_pwd -lcap_grp

.include <bsd.prog.mk>
