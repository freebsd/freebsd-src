# $FreeBSD$

DIFFSRC=${.CURDIR}/../../../contrib/diff/src
.PATH:  ${DIFFSRC} \
	${.CURDIR}/../../../contrib/diff/lib \
	${.CURDIR}/../../../contrib/diff/man

PROG=		sdiff
SRCS=           sdiff.c version-etc.c \
		error.c xmalloc.c c-stack.c basename.c strtoumax.c \
		exitfail.c

# Important for ctype macros!
CFLAGS+=-funsigned-char

CFLAGS+=-I${.CURDIR}/../../../contrib/diff
CFLAGS+=-I${.CURDIR}/../../../contrib/diff/src
CFLAGS+=-I${.CURDIR}/../../../contrib/diff/lib
CFLAGS+=-DHAVE_CONFIG_H
CFLAGS+=-DDEFAULT_DIFF_PROGRAM=\"/usr/bin/diff\"

.for f in sdiff.c
${f}: ${DIFFSRC}/${f} ${.CURDIR}/${f}.diff
	patch -s -b .orig -o ${.TARGET} < ${.CURDIR}/${f}.diff ${DIFFSRC}/${f}
CLEANFILES+= ${f}
.endfor

.include <bsd.prog.mk>
