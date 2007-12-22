#
# $Id: //depot/aic7xxx/freebsd/dev/aic7xxx/aicasm/Makefile#2 $
#
# $FreeBSD$

PROG=	aicasm

CSRCS=	aicasm.c aicasm_symbol.c
YSRCS=	aicasm_gram.y aicasm_macro_gram.y
LSRCS=	aicasm_scan.l aicasm_macro_scan.l

GENHDRS=	aicasm_gram.h aicasm_macro_gram.h

SRCS=	${GENHDRS} ${CSRCS} ${YSRCS} ${LSRCS}
CLEANFILES+= ${GENHDRS} ${YSRCS:R:C/(.*)/\1.output/g}
DPADD=	${LIBL}
LDADD=	-ll

# Correct path for kernel builds
# Don't rely on the kernel's .depend file
.ifdef MAKESRCPATH
.PATH: ${MAKESRCPATH}
DEPENDFILE=	.depend_aicasm
.endif

.if ${CC} == "icc"
CFLAGS+=	-restrict
NOSTDINC=	-X
.else
NOSTDINC=	-nostdinc
.endif
CFLAGS+= ${NOSTDINC} -I/usr/include -I.
.ifdef MAKESRCPATH
CFLAGS+= -I${MAKESRCPATH}
.endif
NO_MAN=
YFLAGS= -b ${.TARGET:R} ${.TARGET:M*macro*:S/$(.TARGET)/-p mm/} -d
LFLAGS+= ${.TARGET:M*macro*:S/$(.TARGET)/-Pmm/}

.ifdef AICASM_DEBUG
CFLAGS+= -DDEBUG -g
YFLAGS+= -t -v
LFLAGS+= -d
.endif

.include <bsd.prog.mk>
