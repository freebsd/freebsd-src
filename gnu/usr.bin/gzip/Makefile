#       @(#)Makefile    5.3 (Berkeley) 5/12/90
#	$Id: Makefile,v 1.10 1996/05/07 23:15:47 wosch Exp $

PROG=	gzip
SRCS=	gzip.c zip.c deflate.c trees.c bits.c unzip.c inflate.c util.c \
	crypt.c lzw.c unlzw.c unlzh.c unpack.c getopt.c match.S
MAN1=	gzexe.1 gzip.1 zdiff.1 zforce.1 zmore.1 znew.1 zgrep.1
CFLAGS+=-DASMV -DSTDC_HEADERS=1 -DHAVE_UNISTD_H=1 -DDIRENT=1
MLINKS= gzip.1 gunzip.1  gzip.1 zcat.1  gzip.1 gzcat.1
MLINKS+= zgrep.1 zfgrep.1 zgrep.1 zegrep.1
MLINKS+= zdiff.1 zcmp.1
LINKS+= ${BINDIR}/gzip ${BINDIR}/gunzip
LINKS+= ${BINDIR}/gzip ${BINDIR}/gzcat
LINKS+= ${BINDIR}/gzip ${BINDIR}/zcat
LINKS+= ${BINDIR}/zgrep ${BINDIR}/zegrep
LINKS+= ${BINDIR}/zgrep ${BINDIR}/zfgrep
LINKS+= ${BINDIR}/zdiff ${BINDIR}/zcmp
NOSHARED=yes

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	   ${.CURDIR}/zforce ${.CURDIR}/gzexe ${.CURDIR}/znew \
	   ${.CURDIR}/zmore ${.CURDIR}/zdiff ${.CURDIR}/zgrep \
	   ${DESTDIR}${BINDIR}
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	   ${.CURDIR}/zgrep.getopt ${DESTDIR}${BINDIR}/zgrep

match.o: ${.CURDIR}/match.S
	$(CPP) ${.CURDIR}/match.S >_match.s
	$(CC) -c _match.s
	mv -f _match.o match.o
	rm -f _match.s

.include <bsd.prog.mk>
