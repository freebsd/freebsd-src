#       @(#)Makefile    5.3 (Berkeley) 5/12/90
# $FreeBSD$

PROG=	gzip
SRCS=	gzip.c zip.c deflate.c trees.c bits.c unzip.c inflate.c util.c \
	crypt.c lzw.c unlzw.c unlzh.c unpack.c
CFLAGS+=-DSTDC_HEADERS=1 -DHAVE_UNISTD_H=1 -DDIRENT=1
.if ${MACHINE_ARCH} == "i386"
SRCS+=	match.S
CFLAGS+=-DASMV
.endif
SCRIPTS=gzexe zdiff zforce zmore znew
MAN=	gzexe.1 gzip.1 zdiff.1 zforce.1 zmore.1 znew.1

LINKS+= ${BINDIR}/gzip ${BINDIR}/gunzip
LINKS+= ${BINDIR}/gzip ${BINDIR}/gzcat
LINKS+= ${BINDIR}/gzip ${BINDIR}/zcat
LINKS+= ${BINDIR}/zdiff ${BINDIR}/zcmp
MLINKS= gzip.1 gunzip.1 gzip.1 zcat.1 gzip.1 gzcat.1
MLINKS+= zdiff.1 zcmp.1

GREP_LIBZ?=	YES
.if defined(GREP_LIBZ) && empty(GREP_LIBZ)
SCRIPTS+=zgrep.getopt
MAN+=	zgrep.1
LINKS+= ${BINDIR}/zgrep ${BINDIR}/zegrep
LINKS+= ${BINDIR}/zgrep ${BINDIR}/zfgrep
MLINKS+= zgrep.1 zfgrep.1 zgrep.1 zegrep.1
.endif

match.o: ${.CURDIR}/match.S
	$(CC) -E -DNO_UNDERLINE -traditional ${.CURDIR}/match.S >_match.s
	$(CC) -c _match.s
	mv -f _match.o match.o
	rm -f _match.s

.include <bsd.prog.mk>
