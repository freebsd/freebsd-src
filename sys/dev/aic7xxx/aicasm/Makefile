# $Id$
#
# $FreeBSD: src/sys/dev/aic7xxx/aicasm/Makefile,v 1.8.2.2 2001/01/07 22:52:35 gibbs Exp $

PROG=	aicasm

CSRCS=	aicasm.c aicasm_symbol.c
GENSRCS= aicasm_gram.c aicasm_scan.c

GENHDRS=	y.tab.h

SRCS=	${GENSRCS} ${CSRCS}
CLEANFILES+= ${GENSRCS} ${GENHDRS} y.output
DPADD+=	${LIBL}
LDADD+=	-ll

# Correct path for kernel builds
# Don't rely on the kernel's .depend file
.ifdef MAKESRCPATH
.PATH: ${MAKESRCPATH}
DEPENDFILE=
.endif

CFLAGS+= -I/usr/include -I.
NOMAN=	noman

.ifdef DEBUG
CFLAGS+= -DDEBUG -g
YFLAGS+= -t -v
LFLAGS+= -d
.endif

.include <bsd.prog.mk>
