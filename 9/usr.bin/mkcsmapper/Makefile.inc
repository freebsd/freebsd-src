# $FreeBSD$

SRCS+=	lex.l yacc.y
CFLAGS+= -I${.CURDIR} -I${.CURDIR}/../mkcsmapper \
	 -I${.CURDIR}/../../lib/libc/iconv \
	 -I${.CURDIR}/../../lib/libiconv_modules/mapper_std \
	 --param max-inline-insns-single=64
