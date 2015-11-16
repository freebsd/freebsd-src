# $FreeBSD$

TESTSDIR=	${TESTSBASE}/sys/kern/acct

ATF_TESTS_C=	acct_test

CFLAGS+=	-I${.OBJDIR}

CLEANFILES+=	convert.c

DPSRCS.acct_test=	convert.c

convert.c: ${SRCTOP}/sys/kern/kern_acct.c
	sed -n -e 's/log(/syslog(/g' \
	       -e '/FLOAT_CONVERSION_START/,/FLOAT_CONVERSION_END/p' ${.ALLSRC} >>${.TARGET}

.include <bsd.test.mk>
