# $FreeBSD$

GREP_LIBZ=YES

PROG=	grep
SRCS=	dfa.c getopt.c getopt1.c grep.c kwset.c obstack.c savedir.c search.c \
	stpcpy.c

CFLAGS+=-I${.CURDIR} -DHAVE_CONFIG_H

LINKS+=	${BINDIR}/grep ${BINDIR}/egrep \
	${BINDIR}/grep ${BINDIR}/fgrep
MLINKS=	grep.1 egrep.1 grep.1 fgrep.1

DPADD+=	${LIBGNUREGEX}
LDADD+=	-lgnuregex

.if defined(GREP_LIBZ) && !empty(GREP_LIBZ)
LDADD+=	-lz
DPADD+=	${LIBZ}
CFLAGS+=-DHAVE_LIBZ=1
LINKS+=	${BINDIR}/grep ${BINDIR}/zgrep \
	${BINDIR}/grep ${BINDIR}/zegrep \
	${BINDIR}/grep ${BINDIR}/zfgrep
MLINKS+=grep.1 zgrep.1 grep.1 zegrep.1 grep.1 zfgrep.1
.endif

SUBDIR+=doc

check:	all
	@failed=0; total=0; \
	for tst in ${TESTS}; do \
		total=$$(($$total+1)); \
		if GREP=${.OBJDIR}/${PROG} srcdir=${.CURDIR}/tests \
		   ${.CURDIR}/tests/$$tst; then \
			echo "PASS: $$tst"; \
		else \
			failed=$$(($$failed+1)); \
			echo "FAIL: $$tst"; \
		fi; \
	done; \
	if [ "$$failed" -eq 0 ]; then \
		echo "All $$total tests passed"; \
	else \
		echo "$$failed of $$total tests failed"; \
	fi

TESTS=	warning.sh khadafy.sh spencer1.sh bre.sh ere.sh status.sh empty.sh \
	options.sh

.include <bsd.prog.mk>
