#	$Id: Makefile,v 1.11 1993/12/24 02:35:33 jkh Exp $

LIB=	gcc
NOPROFILE=
INSTALL_PIC_ARCHIVE=
SHLIB_MAJOR= 1
SHLIB_MINOR= 0


CFLAGS+=-I${.CURDIR}/../lib -I${.CURDIR}/../${MACHINE}

LIB1OBJS= _mulsi3.o _udivsi3.o _divsi3.o _umodsi3.o _modsi3.o _lshrsi3.o \
	  _lshlsi3.o _ashrsi3.o _ashlsi3.o _divdf3.o _muldf3.o _negdf2.o \
	  _adddf3.o _subdf3.o _fixdfsi.o _fixsfsi.o _floatsidf.o _floatsisf.o \
	  _truncdfsf2.o _extendsfdf2.o _addsf3.o _negsf2.o _subsf3.o \
	  _mulsf3.o _divsf3.o _eqdf2.o _nedf2.o _gtdf2.o _gedf2.o _ltdf2.o \
	  _ledf2.o _eqsf2.o _nesf2.o _gtsf2.o _gesf2.o _ltsf2.o _lesf2.o

LIB2OBJS= _muldi3.o _divdi3.o _moddi3.o _udivdi3.o _umoddi3.o _negdi2.o \
	  _lshrdi3.o _lshldi3.o _ashldi3.o _ashrdi3 _ffsdi2.o \
	  _udiv_w_sdiv.o _udivmoddi4.o _cmpdi2.o _ucmpdi2.o _floatdidf.o \
	  _floatdisf.o _fixunsdfsi.o _fixunssfsi.o _fixunsdfdi.o _fixdfdi.o \
	  _fixunssfdi.o _fixsfdi.o _fixxfdi.o _fixunsxfdi.o _floatdixf.o \
	  _fixunsxfsi.o __gcc_bcmp.o _varargs.o _eprintf.o _op_new.o \
	  _new_handler.o _op_delete.o _bb.o _shtab.o _clear_cache.o \
	  _trampoline.o __main.o _exit.o _ctors.o

OBJS= ${LIB1OBJS} ${LIB2OBJS}
LIB1SOBJS=${LIB1OBJS:.o=.so}
LIB2SOBJS=${LIB2OBJS:.o=.so}

${LIB1OBJS}: libgcc1.c
	${CC} -c ${CFLAGS} -DL${.PREFIX} -o ${.TARGET} ${.CURDIR}/libgcc1.c
	@${LD} -x -r ${.TARGET}
	@mv a.out ${.TARGET}

${LIB2OBJS}: libgcc2.c
	${CC} -c ${CFLAGS} -DL${.PREFIX} -o ${.TARGET} ${.CURDIR}/libgcc2.c
	@${LD} -x -r ${.TARGET}
	@mv a.out ${.TARGET}

.if !defined(NOPIC)
${LIB1SOBJS}: libgcc1.c
	${CC} -c -fpic ${CFLAGS} -DL${.PREFIX} -o ${.TARGET} ${.CURDIR}/libgcc1.c

${LIB2SOBJS}: libgcc2.c
	${CC} -c -fpic ${CFLAGS} -DL${.PREFIX} -o ${.TARGET} ${.CURDIR}/libgcc2.c
.endif

.include <bsd.lib.mk>
