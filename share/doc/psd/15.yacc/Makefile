#	@(#)Makefile	8.1 (Berkeley) 8/14/93
# $FreeBSD$

DIR=	psd/15.yacc
SRCS=	ss.. ss0 ss1 ss2 ss3 ss4 ss5 ss6 ss7 ss8 ss9 ssA ssB ssa ssb ssc ssd
MACROS=	-msU
REFER=	refer -e -p /usr/old/dict/papers/Ind

paper.ps: ${SRCS}
	${REFER} ${SRCS} | ${ROFF} > ${.TARGET}

.include <bsd.doc.mk>
