# $FreeBSD$

.PATH:		${.CURDIR}/../../../cddl/contrib/opensolaris/lib/libdtrace/common

SRCS=		drti.c
FILES=		${SRCS:R:S/$/.o/g}
FILESOWN=	${LIBOWN}
FILESGRP=	${LIBGRP}
FILESMODE=	${LIBMODE}
FILESDIR=	${LIBDIR}/dtrace
CLEANFILES=	${FILES} ${FILES}.tmp

CFLAGS+=	-I${.CURDIR}/../../../sys/cddl/compat/opensolaris \
		-I${.CURDIR}/../../../cddl/compat/opensolaris/include \
		-I${OPENSOLARIS_USR_DISTDIR}/head \
		-I${OPENSOLARIS_USR_DISTDIR}/lib/libctf/common \
		-I${OPENSOLARIS_USR_DISTDIR}/lib/libdtrace/common \
		-I${OPENSOLARIS_SYS_DISTDIR}/uts/common \
		-DPIC -fpic

.c.o:
	${CC} ${CFLAGS} -c ${.IMPSRC}
	mv ${FILES} ${FILES}.tmp
	${LD} -o ${FILES} -r ${FILES}.tmp ${LIBELF_PIC}
	rm -f ${FILES}.tmp

.include <bsd.prog.mk>
