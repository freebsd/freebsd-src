#
# $Id: Makefile,v 1.9 1996/05/07 23:15:14 wosch Exp $
#

LIB=	gcc
INSTALL_PIC_ARCHIVE=  yes

#
# XXX This is a hack, but it seems to work.  libgcc1.a is supposed to be
# compiled by the native compiler, and libgcc2.a is meant to be compiled
# by *this* version of gcc.
#
# Normally, this does not make any difference, since we only have gcc, but
# when bootstrapping from gcc-2.6.3, we have to use the freshly built 2.7.2
# compiler for some of the libgcc2.c __attribute__ stuff.
#
.if exists(${.OBJDIR}/../cc)
XCC=	${.OBJDIR}/../cc/cc
.else
XCC=	${.CURDIR}/../cc/cc
.endif

.if exists(${.OBJDIR}/../cc1)
XCC+=	-B${.OBJDIR}/../cc1/
.else
XCC+=	-B${.CURDIR}/../cc1/
.endif

.if exists(${.OBJDIR}/../cpp)
XCC+=	-B${.OBJDIR}/../cpp/
.else
XCC+=	-B${.CURDIR}/../cpp/
.endif


LIB1OBJS=       _mulsi3.o _umodsi3.o _modsi3.o _lshrsi3.o _lshlsi3.o \
		_ashrsi3.o _ashlsi3.o _divdf3.o _muldf3.o _negdf2.o _adddf3.o \
		_subdf3.o _fixsfsi.o _floatsidf.o _floatsisf.o _truncdfsf2.o \
		_extendsfdf2.o _addsf3.o _negsf2.o _subsf3.o _mulsf3.o \
		_divsf3.o _eqdf2.o _nedf2.o _gtdf2.o _gedf2.o _ltdf2.o \
		_ledf2.o _eqsf2.o _nesf2.o _gtsf2.o _gesf2.o _ltsf2.o \
		_lesf2.o
LIB2OBJS=	_ffsdi2.o _udiv_w_sdiv.o _udivmoddi4.o _fixunssfsi.o \
		_fixxfdi.o _fixunsxfdi.o _floatdixf.o _fixunsxfsi.o \
		__gcc_bcmp.o _varargs.o _eprintf.o _op_new.o _op_vnew.o \
		_new_handler.o _op_delete.o _op_vdel.o _bb.o _shtab.o \
		_clear_cache.o __main.o _exit.o _ctors.o _eh.o _pure.o

OBJS= ${LIB1OBJS} ${LIB2OBJS}
LIB1SOBJS=${LIB1OBJS:.o=.so}
LIB2SOBJS=${LIB2OBJS:.o=.so}
P1OBJS=${LIB1OBJS:.o=.po}
P2OBJS=${LIB2OBJS:.o=.po}

${LIB1OBJS}: libgcc1.c
	${CC} -c ${CFLAGS} -DL${.PREFIX} -o ${.TARGET} ${GCCDIR}/libgcc1.c
	@${LD} -O ${.TARGET} -x -r ${.TARGET}

${LIB2OBJS}: libgcc2.c
	${XCC} -c ${CFLAGS} -DL${.PREFIX} -o ${.TARGET} ${GCCDIR}/libgcc2.c
	@${LD} -O ${.TARGET} -x -r ${.TARGET}

.if !defined(NOPIC)
${LIB1SOBJS}: libgcc1.c
	${CC} -c -fpic ${CFLAGS} -DL${.PREFIX} -o ${.TARGET} ${GCCDIR}/libgcc1.c
	@${LD} -O ${.TARGET} -x -r ${.TARGET}

${LIB2SOBJS}: libgcc2.c
	${XCC} -c -fpic ${CFLAGS} -DL${.PREFIX} -o ${.TARGET} ${GCCDIR}/libgcc2.c
	@${LD} -O ${.TARGET} -x -r ${.TARGET}
.endif

.if !defined(NOPROFILE)
${P1OBJS}: libgcc1.c
	${CC} -c -p ${CFLAGS} -DL${.PREFIX} -o ${.TARGET} ${GCCDIR}/libgcc1.c
	@${LD} -O ${.TARGET} -X -r ${.TARGET}

${P2OBJS}: libgcc2.c
	${XCC} -c -p ${CFLAGS} -DL${.PREFIX} -o ${.TARGET} ${GCCDIR}/libgcc2.c
	@${LD} -O ${.TARGET} -X -r ${.TARGET}
.endif

.include <bsd.lib.mk>
