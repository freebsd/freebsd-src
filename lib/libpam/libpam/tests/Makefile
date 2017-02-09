# $FreeBSD$

OPENPAM=	${SRCTOP}/contrib/openpam
.PATH: ${OPENPAM}/t

TESTSDIR = ${TESTSBASE}/lib/libpam

COMMONSRC = t_file.c t_main.c
.for test in t_openpam_ctype t_openpam_readlinev t_openpam_readword
TAP_TESTS_C += ${test}
SRCS.${test} = ${test}.c ${COMMONSRC}
.endfor
CFLAGS +=-I${OPENPAM}/include -I${OPENPAM}/lib/libpam -I${OPENPAM}/t
WARNS ?= 6

DPADD = ${LIBPAM}
LDADD = ${MINUSLPAM}

.include <bsd.test.mk>
