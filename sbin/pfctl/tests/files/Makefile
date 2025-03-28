PACKAGE=	tests

TESTSDIR=	${TESTSBASE}/sbin/pfctl/files
BINDIR=		${TESTSDIR}

# We use ${.CURDIR} as workaround so that the glob patterns work.
FILES!=		echo ${.CURDIR}/pf????.in ${.CURDIR}/pf????.include ${.CURDIR}/pf????.ok ${.CURDIR}/pf????.fail

.include <bsd.progs.mk>
