# $FreeBSD$

TESTSDIR=	${TESTSBASE}/sbin/pfctl/files
BINDIR=		${TESTSDIR}

# We use ${.CURDIR} as workaround so that the glob patterns work.
FILES=		${.CURDIR}/pf????.in
FILES+=		${.CURDIR}/pf????.include
FILES+=		${.CURDIR}/pf????.ok
FILES+=		${.CURDIR}/pfctl_test_descr.sh

.include <bsd.progs.mk>
