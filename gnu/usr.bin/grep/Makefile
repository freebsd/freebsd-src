#	$Id: Makefile,v 1.11 1997/12/21 19:15:02 wosch Exp $

GREP_LIBZ= YES
GREP_FTS=  YES

PROG=	grep
SRCS=   dfa.c grep.c getopt.c kwset.c obstack.c search.c
CFLAGS+=-DGREP -DHAVE_STRING_H=1 -DHAVE_SYS_PARAM_H=1 -DHAVE_UNISTD_H=1 \
	-DHAVE_GETPAGESIZE=1 -DHAVE_MEMCHR=1 -DHAVE_STRERROR=1 \
	-DHAVE_VALLOC=1

LINKS+= ${BINDIR}/grep ${BINDIR}/egrep \
	${BINDIR}/grep ${BINDIR}/fgrep
MLINKS= grep.1 egrep.1 grep.1 fgrep.1

DPADD+= ${LIBGNUREGEX}
LDADD+= -lgnuregex

.if defined(GREP_LIBZ) && !empty(GREP_LIBZ)
LDADD+= -lz
DPADD+= ${LIBZ}
CFLAGS+= -DHAVE_LIBZ=1
LINKS+= ${BINDIR}/grep ${BINDIR}/zgrep \
	${BINDIR}/grep ${BINDIR}/zegrep \
	${BINDIR}/grep ${BINDIR}/zfgrep 
.endif
.if defined(GREP_FTS) && !empty(GREP_FTS)
CFLAGS+= -DHAVE_FTS=1
.endif

check:	all
	sh ${.CURDIR}/tests/check.sh ${.CURDIR}/tests

.include <bsd.prog.mk>
