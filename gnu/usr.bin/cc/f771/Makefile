# $FreeBSD: src/gnu/usr.bin/cc/f771/Makefile,v 1.5.2.1 2000/07/04 05:39:50 obrien Exp $

.include "${.CURDIR}/../Makefile.inc"

.PATH: ${GCCDIR}/f

PROG=	f771
SRCS=	bad.c bit.c bld.c com.c data.c equiv.c expr.c global.c implic.c info.c \
	intrin.c lab.c lex.c malloc.c name.c parse.c proj.c src.c st.c sta.c \
	stb.c stc.c std.c ste.c storag.c stp.c str.c sts.c stt.c stu.c stv.c \
	stw.c symbol.c target.c top.c type.c version.c where.c
BINDIR=	/usr/libexec
NOMAN=

CFLAGS+= -I${GCCDIR}/f -I.

DPADD=	${LIBCC_INT} 
LDADD=	${LIBCC_INT}

build-tools: fini

#  The use of ``proj+%BT.o'' is to get around bogus dependacy information
#  created for build-tools sources.
fini: fini.o proj+%BT.o
	${CC} -static ${CFLAGS} ${LDFLAGS} -o ${.TARGET} ${.ALLSRC}

proj+%BT.o: proj.c
	${CC} ${CFLAGS} -o ${.TARGET} -c ${.ALLSRC}

CLEANFILES+=	fini fini.o proj+%BT.o

#-----------------------------------------------------------------------
# str-* gunk

.for i in 1t 2t fo io nq op ot
.ORDER: str-$i.h str-$i.j
str-$i.j str-$i.h: str-$i.fin fini
	./fini ${GCCDIR}/f/str-$i.fin str-$i.j str-$i.h

FINIHDRS+=	str-$i.j str-$i.h
.endfor

CLEANFILES+=	${FINIHDRS}

.include <bsd.prog.mk>

#-----------------------------------------------------------------------
# Fixups.

.if !exists(${DEPENDFILE})
${OBJS:Nproj.o} ${DEPENDFILE}: ${FINIHDRS}
.endif
