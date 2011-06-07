#	$NetBSD: Makefile,v 1.4 2011/02/16 01:31:33 joerg Exp $
#	$FreeBSD$
#	$OpenBSD: Makefile,v 1.6 2003/06/25 15:00:04 millert Exp $

.include <bsd.own.mk>

.if ${MK_BSD_GREP} == "yes"
PROG=	grep
.else
PROG=	bsdgrep
.endif
SRCS=	fastgrep.c file.c grep.c queue.c util.c

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

bsdgrep.1: grep.1
	cp ${.ALLSRC} ${.TARGET}

LDADD=	-lz -lbz2
DPADD=	${LIBZ} ${LIBBZ2}

.if !defined(WITHOUT_GNU_COMPAT)
CFLAGS+= -I/usr/include/gnu
LDADD+=	-lgnuregex
DPADD+=	${LIBGNUREGEX}
.endif

.if !defined(WITHOUT_NLS)
.include "${.CURDIR}/nls/Makefile.inc"
.else
CFLAGS+= -DWITHOUT_NLS
.endif

.include <bsd.prog.mk>
