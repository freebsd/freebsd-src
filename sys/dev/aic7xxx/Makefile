#	$Id: Makefile,v 1.1.2.2 1997/09/03 04:25:04 gibbs Exp $

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

CFLAGS+= -I.
NOMAN=	noman

.ifdef DEBUG
CFLAGS+= -DDEBUG -g
YFLAGS+= -t
LFLAGS+= -d
.endif

.include <bsd.prog.mk>
