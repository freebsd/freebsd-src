# $FreeBSD$

PROG=	mkuzip
MAN=	mkuzip.8
SRCS=	mkuzip.c mkuz_blockcache.c mkuz_lzma.c mkuz_zlib.c mkuz_conveyor.c \
	    mkuz_blk.c mkuz_fqueue.c mkuz_time.c

DPADD=	${LIBZ} ${LIBMD} ${LIBLZMA} ${LIBPTHREAD}
LDADD=	-lz -lmd -llzma -lpthread

.include <bsd.prog.mk>
