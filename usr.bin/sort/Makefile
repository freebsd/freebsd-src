#	$FreeBSD$

SRCDIR=	${.CURDIR}/../../contrib/sort
.PATH: ${SRCDIR}

PROG=	sort
SRCS=	append.c fields.c files.c fsort.c init.c msort.c sort.c tmp.c
MAN=	sort.1
CFLAGS+= -I${SRCDIR} -include ${.CURDIR}/netbsd.h

.include <bsd.prog.mk>
