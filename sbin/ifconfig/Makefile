#	From: @(#)Makefile	8.1 (Berkeley) 6/5/93
#	$FreeBSD$

PROG=	ifconfig
MAN8=	ifconfig.8
DPADD=	${LIBIPX}
LDADD=	-lipx
COPTS=	-DNS -Wall -Wmissing-prototypes -Wcast-qual -Wwrite-strings \
	-Wnested-externs

.include <bsd.prog.mk>
