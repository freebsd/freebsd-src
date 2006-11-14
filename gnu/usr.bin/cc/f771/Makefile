# $FreeBSD$

.include "${.CURDIR}/../Makefile.inc"

.PATH: ${GCCDIR}/f ${GCCDIR}

PROG=	f771
SRCS=	bad.c bit.c bld.c com.c data.c equiv.c expr.c global.c implic.c info.c \
	intrin.c lab.c lex.c malloc.c name.c parse.c        src.c st.c sta.c \
	stb.c stc.c std.c ste.c storag.c stp.c str.c sts.c stt.c stu.c stv.c \
	stw.c symbol.c target.c top.c type.c where.c main.c
BINDIR=	/usr/libexec
NO_MAN=

CFLAGS+= -I${GCCDIR}/f -I.

DPADD=	${LIBCC_INT}
LDADD=	${LIBCC_INT}

#-----------------------------------------------------------------------
# str-* gunk

.for i in 1t 2t fo io nq op ot
.ORDER: str-$i.h str-$i.j
str-$i.j str-$i.h: str-$i.fin
	${.OBJDIR}/../cc_tools/fini ${GCCDIR}/f/str-$i.fin str-$i.j str-$i.h

FINIHDRS+=	str-$i.j str-$i.h
.endfor

SRCS+=		${FINIHDRS:M*.h}
CLEANFILES+=	${FINIHDRS}

.include <bsd.prog.mk>
