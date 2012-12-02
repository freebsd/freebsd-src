# $FreeBSD$

.include <bsd.compiler.mk>

SRCS+=	lex.l yacc.y
CFLAGS+= -I${.CURDIR} -I${.CURDIR}/../mkcsmapper \
	 -I${.CURDIR}/../../lib/libc/iconv \
	 -I${.CURDIR}/../../lib/libiconv_modules/mapper_std
.if ${COMPILER_TYPE} == "gcc"
CFLAGS+= --param max-inline-insns-single=64
.endif
