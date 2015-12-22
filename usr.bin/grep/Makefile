#	$NetBSD: Makefile,v 1.4 2011/02/16 01:31:33 joerg Exp $
#	$FreeBSD$
#	$OpenBSD: Makefile,v 1.6 2003/06/25 15:00:04 millert Exp $

.include <bsd.own.mk>

.if ${MK_BSD_GREP} == "yes"
PROG=	grep
.else
PROG=	bsdgrep
CLEANFILES+= bsdgrep.1

bsdgrep.1: grep.1
	cp ${.ALLSRC} ${.TARGET}
.endif
SRCS=	file.c grep.c queue.c util.c

# Extra files ported backported form some regex improvements
.PATH: ${.CURDIR}/regex
SRCS+=	fastmatch.c hashtable.c tre-compile.c tre-fastmatch.c xmalloc.c
CFLAGS+=-I${.CURDIR}/regex

.if ${MK_BSD_GREP} == "yes"
LINKS=	${BINDIR}/grep ${BINDIR}/egrep \
	${BINDIR}/grep ${BINDIR}/fgrep \
	${BINDIR}/grep ${BINDIR}/zgrep \
	${BINDIR}/grep ${BINDIR}/zegrep \
	${BINDIR}/grep ${BINDIR}/zfgrep

MLINKS= grep.1 egrep.1 \
	grep.1 fgrep.1 \
	grep.1 zgrep.1 \
	grep.1 zegrep.1 \
	grep.1 zfgrep.1
.endif

LDADD=	-lz
DPADD=	${LIBZ}

.if !defined(WITHOUT_LZMA_SUPPORT)
LDADD+=	-llzma -lpthread
DPADD+=	${LIBLZMA} ${LIBPTHREAD}

LINKS+=	${BINDIR}/${PROG} ${BINDIR}/xzgrep \
	${BINDIR}/${PROG} ${BINDIR}/xzegrep \
	${BINDIR}/${PROG} ${BINDIR}/xzfgrep \
	${BINDIR}/${PROG} ${BINDIR}/lzgrep \
	${BINDIR}/${PROG} ${BINDIR}/lzegrep \
	${BINDIR}/${PROG} ${BINDIR}/lzfgrep

MLINKS+= grep.1 xzgrep.1 \
	 grep.1 xzegrep.1 \
	 grep.1 xzfgrep.1 \
	 grep.1 lzgrep.1 \
	 grep.1 lzegrep.1 \
	 grep.1 lzfgrep.1
.else
CFLAGS+= -DWITHOUT_LZMA
.endif

.if !defined(WITHOUT_BZIP2_SUPPORT)
LDADD+=	-lbz2
DPADD+=	${LIBBZ2}

.if ${MK_BSD_GREP} == "yes"
LINKS+= ${BINDIR}/grep ${BINDIR}/bzgrep \
	${BINDIR}/grep ${BINDIR}/bzegrep \
	${BINDIR}/grep ${BINDIR}/bzfgrep
MLINKS+= grep.1 bzgrep.1 \
	 grep.1 bzegrep.1 \
	 grep.1 bzfgrep.1
.endif
.else
CFLAGS+= -DWITHOUT_BZIP2
.endif

.if !defined(WITHOUT_GNU_COMPAT)
CFLAGS+= -I${DESTDIR}/usr/include/gnu
LDADD+=	-lgnuregex
DPADD+=	${LIBGNUREGEX}
.endif

.if !defined(WITHOUT_NLS)
.include "${.CURDIR}/nls/Makefile.inc"
.else
CFLAGS+= -DWITHOUT_NLS
.endif

.if ${MK_TESTS} != "no"
SUBDIR+=	tests
.endif

.include <bsd.prog.mk>
