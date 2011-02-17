#	$FreeBSD$
#	$OpenBSD: Makefile,v 1.6 2003/06/25 15:00:04 millert Exp $

PROG=	grep
SRCS=	fastgrep.c file.c grep.c queue.c util.c
LINKS=	${BINDIR}/grep ${BINDIR}/egrep \
	${BINDIR}/grep ${BINDIR}/fgrep \
	${BINDIR}/grep ${BINDIR}/zgrep \
	${BINDIR}/grep ${BINDIR}/zegrep \
	${BINDIR}/grep ${BINDIR}/zfgrep \

MLINKS= grep.1 egrep.1 \
	grep.1 fgrep.1 \
	grep.1 zgrep.1 \
	grep.1 zegrep.1 \
	grep.1 zfgrep.1

WARNS?=	6

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
