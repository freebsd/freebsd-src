PROG=	grep
SRCS=	dfa.c grep.c getopt.c kwset.c obstack.c regex.c search.c
CFLAGS+=-DGREP -DHAVE_STRING_H=1 -DHAVE_SYS_PARAM_H=1 -DHAVE_UNISTD_H=1 \
	-DHAVE_GETPAGESIZE=1 -DHAVE_MEMCHR=1 -DHAVE_STRERROR=1 \
	-DHAVE_VALLOC=1

LINKS+= ${DESTDIR}${BINDIR}/grep ${DESTDIR}${BINDIR}/egrep \
	${DESTDIR}${BINDIR}/grep ${DESTDIR}${BINDIR}/fgrep
MLINKS= grep.1 egrep.1 grep.1 fgrep.1

check:	all
	sh ${.CURDIR}/tests/check.sh ${.CURDIR}/tests

.include <bsd.prog.mk>
