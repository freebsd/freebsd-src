#
# $Id: Makefile,v 1.16 1998/03/23 12:23:13 bde Exp $
#

LIB=	gcc

# Install libgcc_pic.a, since ld.so uses it.
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
# We now depend on a bootstrap pass (normally in `make world') to build
# and install the new version of gcc before we get here.  This makes
# finding the new version (XCC) easy but may break finding the old version
# (CC).
#
XCC=	${CC}

# Members of libgcc1.a.
LIB1FUNCS= _mulsi3 _udivsi3 _divsi3 _umodsi3 _modsi3 \
	   _lshrsi3 _ashrsi3 _ashlsi3 \
	   _divdf3 _muldf3 _negdf2 _adddf3 _subdf3 \
	   _fixdfsi _fixsfsi _floatsidf _floatsisf _truncdfsf2 _extendsfdf2 \
	   _addsf3 _negsf2 _subsf3 _mulsf3 _divsf3 \
	   _eqdf2 _nedf2 _gtdf2 _gedf2 _ltdf2 _ledf2 \
	   _eqsf2 _nesf2 _gtsf2 _gesf2 _ltsf2 _lesf2

# Library members defined in libgcc2.c.
LIB2FUNCS= _muldi3 _divdi3 _moddi3 _udivdi3 _umoddi3 _negdi2 \
	   _lshrdi3 _ashldi3 _ashrdi3 _ffsdi2 \
	   _udiv_w_sdiv _udivmoddi4 _cmpdi2 _ucmpdi2 _floatdidf _floatdisf \
	   _fixunsdfsi _fixunssfsi _fixunsdfdi _fixdfdi _fixunssfdi _fixsfdi \
	   _fixxfdi _fixunsxfdi _floatdixf _fixunsxfsi \
	   _fixtfdi _fixunstfdi _floatditf \
	   __gcc_bcmp _varargs _eprintf _op_new _op_vnew _new_handler \
	   _op_delete \
	   _op_vdel _bb _shtab _clear_cache _trampoline __main _exit _ctors \
	   _eh _pure

COMMONHDRS=	config.h tconfig.h tm.h
CFLAGS+=	-I.
CLEANFILES+=	${COMMONHDRS}

LIB1OBJS=	${LIB1FUNCS:S/$/.o/}
LIB2OBJS=	${LIB2FUNCS:S/$/.o/}
LIB1SOBJS=	${LIB1FUNCS:S/$/.so/}
LIB2SOBJS=	${LIB2FUNCS:S/$/.so/}
LIB1POBJS=	${LIB1FUNCS:S/$/.po/}
LIB2POBJS=	${LIB2FUNCS:S/$/.po/}

OBJS=		${LIB1OBJS} ${LIB2OBJS}

config.h tconfig.h:
	echo '#include "${MACHINE_ARCH}/xm-freebsd.h"' > ${.TARGET}

tm.h:
	echo '#include "${MACHINE_ARCH}/freebsd.h"' > ${.TARGET}

${OBJS}: ${COMMONHDRS}

${LIB1OBJS}: libgcc1.c
	${CC} -c ${CFLAGS} -DL${.PREFIX} -o ${.TARGET} ${.ALLSRC:M*.c}
	@${LD} -O ${.TARGET} -x -r ${.TARGET}

${LIB2OBJS}: libgcc2.c
	${XCC} -c ${CFLAGS} -DL${.PREFIX} -o ${.TARGET} ${.ALLSRC:M*.c}
	@${LD} -O ${.TARGET} -x -r ${.TARGET}

.if !defined(NOPIC)
${LIB1SOBJS}: libgcc1.c
	${CC} -c -fpic ${CFLAGS} -DL${.PREFIX} -o ${.TARGET} ${.ALLSRC:M*.c}
	@${LD} -O ${.TARGET} -x -r ${.TARGET}

${LIB2SOBJS}: libgcc2.c
	${XCC} -c -fpic ${CFLAGS} -DL${.PREFIX} -o ${.TARGET} ${.ALLSRC:M*.c}
	@${LD} -O ${.TARGET} -x -r ${.TARGET}
.endif

.if !defined(NOPROFILE)
${LIB1POBJS}: libgcc1.c
	${CC} -c -p ${CFLAGS} -DL${.PREFIX} -o ${.TARGET} ${.ALLSRC:M*.c}
	@${LD} -O ${.TARGET} -X -r ${.TARGET}

${LIB2POBJS}: libgcc2.c
	${XCC} -c -p ${CFLAGS} -DL${.PREFIX} -o ${.TARGET} ${.ALLSRC:M*.c}
	@${LD} -O ${.TARGET} -X -r ${.TARGET}
.endif

.include <bsd.lib.mk>
