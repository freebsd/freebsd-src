#       @(#)Makefile    5.3 (Berkeley) 5/12/90
#	$Id: Makefile,v 1.18 1997/12/26 01:53:58 wosch Exp $

PROG=	gzip
SRCS=	gzip.c zip.c deflate.c trees.c bits.c unzip.c inflate.c util.c \
	crypt.c lzw.c unlzw.c unlzh.c unpack.c getopt.c
MAN1=	gzexe.1 gzip.1 zdiff.1 zforce.1 zmore.1 znew.1
CFLAGS+=-DSTDC_HEADERS=1 -DHAVE_UNISTD_H=1 -DDIRENT=1
GREP_LIBZ?=	YES

.if ${MACHINE_ARCH} == "i386"
SRCS+=	match.S
CFLAGS+=-DASMV
.endif

MLINKS= gzip.1 gunzip.1  gzip.1 zcat.1  gzip.1 gzcat.1
MLINKS+= zdiff.1 zcmp.1

LINKS+= ${BINDIR}/gzip ${BINDIR}/gunzip
LINKS+= ${BINDIR}/gzip ${BINDIR}/gzcat
LINKS+= ${BINDIR}/gzip ${BINDIR}/zcat
LINKS+= ${BINDIR}/zdiff ${BINDIR}/zcmp
.if defined(GREP_LIBZ) && empty(GREP_LIBZ)
LINKS+= ${BINDIR}/zgrep ${BINDIR}/zegrep
LINKS+= ${BINDIR}/zgrep ${BINDIR}/zfgrep
MLINKS+= zgrep.1 zfgrep.1 zgrep.1 zegrep.1
MAN1+=	zgrep.1
.endif

NOSHARED?=yes
ZGREP=zgrep.getopt

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	   ${.CURDIR}/zforce ${.CURDIR}/gzexe ${.CURDIR}/znew \
	   ${.CURDIR}/zmore ${.CURDIR}/zdiff \
	   ${DESTDIR}${BINDIR}
.if defined(GREP_LIBZ) && empty(GREP_LIBZ)
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	   ${.CURDIR}/${ZGREP} ${DESTDIR}${BINDIR}/zgrep
.endif

match.o: ${.CURDIR}/match.S
	$(CPP) ${.CURDIR}/match.S >_match.s
	$(CC) -c _match.s
	mv -f _match.o match.o
	rm -f _match.s

.include <bsd.prog.mk>
