#	$FreeBSD$
#	$OpenBSD: Makefile,v 1.2 2006/11/26 11:31:09 deraadt Exp $

PROG=	dc
SRCS=	dc.c bcode.c inout.c mem.c stack.c
LDADD=	-lcrypto
DPADD=	${LIBCRYPTO}

SUBDIR+=	USD.doc

.include <bsd.prog.mk>
