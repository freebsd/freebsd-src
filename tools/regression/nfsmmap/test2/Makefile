# $FreeBSD: src/tools/regression/nfsmmap/test2/Makefile,v 1.3 2004/12/21 08:47:22 ru Exp $

PROG= test2
NO_MAN=
NFSSERVER?= herring.nlsystems.com
CLEANFILES= test2.zeros test2.good test2.data test2.scratch

all: test2 test2.zeros test2.good
	@cp ${.OBJDIR}/test2.zeros ${.OBJDIR}/test2.data
	@if [ `hostname` != ${NFSSERVER} ] ; then \
		ssh ${NFSSERVER} touch ${.OBJDIR}/test2.data; \
	fi
	@cd ${.OBJDIR}; ${.OBJDIR}/test2
	@if cmp -s ${.OBJDIR}/test2.data ${.OBJDIR}/test2.good && \
	   cmp -s ${.OBJDIR}/test2.scratch ${.OBJDIR}/test2.good ; then \
		echo passed; \
	else \
		echo failed; \
	fi

test2.zeros: test2.zeros.uu
	uudecode $?

test2.good: test2.good.uu
	uudecode $?

.include <bsd.prog.mk>
