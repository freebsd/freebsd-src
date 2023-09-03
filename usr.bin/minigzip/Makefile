
SRCDIR=	${SRCTOP}/sys/contrib/zlib/test
.PATH:	${SRCDIR}

PROG=	minigzip

WARNS?=	2
CFLAGS+=-DUSE_MMAP
LIBADD=	z

.include <bsd.prog.mk>
