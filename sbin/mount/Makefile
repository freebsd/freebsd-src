#	@(#)Makefile	5.5 (Berkeley) 5/11/90

PROG=	mount
CFLAGS+=-DNFS
MAN8=	mount.8
DPADD=	${LIBRPC}
LDADD=	-lrpc
MLINKS=	mount.8 umount.8

.include <bsd.prog.mk>
