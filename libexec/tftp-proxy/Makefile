
.PATH:	${SRCTOP}/contrib/pf/tftp-proxy

PROG=	tftp-proxy
SRCS=	tftp-proxy.c filter.c
MAN=	tftp-proxy.8

CFLAGS+= -I${SRCTOP}/lib/libpfctl -I${OBJTOP}/lib/libpfctl
LIBADD= pfctl

WARNS?=	3

.include <bsd.prog.mk>
