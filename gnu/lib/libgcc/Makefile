# $FreeBSD$

.include "${.CURDIR}/../../usr.bin/cc/Makefile.tgt"

GCCDIR=	${.CURDIR}/../../../contrib/gcc
.PATH: ${GCCDIR}/config/${GCC_CPU} ${GCCDIR}

# allow to be overridden for the a.out case
.if !defined(LIB) || ${LIB} != "gcc_r"
LIB=	gcc
.endif

#SHLIB_MAJOR=	1

# We need to install libgcc_pic.a as well, for use by shared libs.
INSTALL_PIC_ARCHIVE=    yes     

#
# XXX This is a hack, but it seems to work.
# libgcc2.a is meant to be compiled by *this* version of gcc.
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
XCXX=	${CXX}

CFLAGS+=	-fexceptions
CFLAGS+=	-DIN_GCC -DIN_LIBGCC2 -D__GCC_FLOAT_NOT_NEEDED \
		-DFINE_GRAINED_LIBRARIES
CFLAGS+=	-D_PTHREADS -DGTHREAD_USE_WEAK
CFLAGS+=	-I${.CURDIR}/../../usr.bin/cc/cc_tools \
		-I${GCCDIR}/config -I${GCCDIR} -I.
LDFLAGS=	-nostdlib

OBJS=		# added to below in various ways depending on TARGET_ARCH

#---------------------------------------------------------------------------
#
# When upgrading GCC, get the following defintions straight from Makefile.in
#

# Library members defined in libgcc2.c.
# Variable length limited to 255 charactes when passed to a shell script.
LIB2FUNCS_1 = _muldi3 _negdi2 _lshrdi3 _ashldi3 _ashrdi3 _ffsdi2 _clz \
    _cmpdi2 _ucmpdi2 _floatdidf _floatdisf _fixunsdfsi _fixunssfsi \
    _fixunsdfdi _fixdfdi _fixunssfdi _fixsfdi _fixxfdi _fixunsxfdi

LIB2FUNCS_2 = _floatdixf _fixunsxfsi _fixtfdi _fixunstfdi _floatditf \
    _clear_cache _trampoline __main _exit _absvsi2 _absvdi2 _addvsi3 \
    _addvdi3 _subvsi3 _subvdi3 _mulvsi3 _mulvdi3 _negvsi2 _negvdi2 _ctors

# Defined in libgcc2.c, included only in the static library.
LIB2FUNCS_ST = _eprintf _bb __gcc_bcmp

FPBIT_FUNCS = _pack_sf _unpack_sf _addsub_sf _mul_sf _div_sf \
    _fpcmp_parts_sf _compare_sf _eq_sf _ne_sf _gt_sf _ge_sf \
    _lt_sf _le_sf _unord_sf _si_to_sf _sf_to_si _negate_sf _make_sf \
    _sf_to_df _thenan_sf _sf_to_usi _usi_to_sf

DPBIT_FUNCS = _pack_df _unpack_df _addsub_df _mul_df _div_df \
    _fpcmp_parts_df _compare_df _eq_df _ne_df _gt_df _ge_df \
    _lt_df _le_df _unord_df _si_to_df _df_to_si _negate_df _make_df \
    _df_to_sf _thenan_df _df_to_usi _usi_to_df

# These might cause a divide overflow trap and so are compiled with
# unwinder info.
LIB2_DIVMOD_FUNCS = _divdi3 _moddi3 _udivdi3 _umoddi3 _udiv_w_sdiv _udivmoddi4

MACHMODE_H = machmode.h machmode.def

LIB2ADD = $(LIB2FUNCS_EXTRA)
LIB2ADD_ST = $(LIB2FUNCS_STATIC_EXTRA)

# Additional sources to handle exceptions; overridden on ia64.
LIB2ADDEH = unwind-dw2.c unwind-dw2-fde.c unwind-sjlj.c

#-----------------------------------------------------------------------
#
#	Platform specific bits.
#	When upgrading GCC, get the following defintions from config/<cpu>/t-*
#

.if ${TARGET_ARCH} == "alpha"
#	from config/alpha/t-alpha
LIB2FUNCS_EXTRA = qrnnd.asm
.endif

.if ${TARGET_ARCH} == "arm"
#	from config/arm/t-strongarm-elf
LIB1ASMSRC = lib1funcs.asm
LIB1ASMFUNCS = _udivsi3 _divsi3 _umodsi3 _modsi3 _dvmd_tls _bb_init_func
# We want fine grained libraries, so use the new code to build the
# floating point emulation libraries.
XXX fix this: NEED_FP_EMULATION set to "yes" with different rules per platform
.endif

.if ${TARGET_ARCH} == "ia64"
#	from config/ia64/t-ia64
LIB1ASMSRC    = lib1funcs.asm
LIB1ASMFUNCS  = __divtf3 __divdf3 __divsf3 \
	__divdi3 __moddi3 __udivdi3 __umoddi3 \
	__divsi3 __modsi3 __udivsi3 __umodsi3 __save_stack_nonlocal \
	__nonlocal_goto __restore_stack_nonlocal __trampoline
LIB2ADDEH = unwind-ia64.c unwind-sjlj.c
.endif

.if ${TARGET_ARCH} == "powerpc"
#	from config/rs6000/t-ppccomm
LIB2FUNCS_EXTRA = tramp.asm
# This one can't end up in shared libgcc
LIB2FUNCS_STATIC_EXTRA = eabi.asm
# We want fine grained libraries, so use the new code to build the
# floating point emulation libraries.
OBJS+=	dp-bit.o fp-bit.o
.endif

.if ${TARGET_ARCH} == "sparc64"
#	from config/sparc/t-elf
# We want fine grained libraries, so use the new code to build the
# floating point emulation libraries.
LIB1ASMSRC = lb1spc.asm
LIB1ASMFUNCS = _mulsi3 _divsi3 _modsi3
OBJS+=	dp-bit.o fp-bit.o
.endif

dp-bit.o: config/fp-bit.c
	${XCC} -c ${CFLAGS} -o ${.TARGET} ${.ALLSRC:M*fp-bit*}

dp-bit.So: config/fp-bit.c
	${XCC} ${PICFLAG} -c ${CFLAGS} -o ${.TARGET} ${.ALLSRC:M*fp-bit*}

dp-bit.po: config/fp-bit.c
	${XCC} -p -c ${CFLAGS} -o ${.TARGET} ${.ALLSRC:M*fp-bit*}

fp-bit.o: config/fp-bit.c
	${XCC} -DFLOAT=1 -c ${CFLAGS} -o ${.TARGET} ${.ALLSRC:M*fp-bit*}

fp-bit.So: config/fp-bit.c
	${XCC} ${PICFLAG} -DFLOAT=1 -c ${CFLAGS} -o ${.TARGET} ${.ALLSRC:M*fp-bit*}

fp-bit.po: config/fp-bit.c
	${XCC} -p -DFLOAT=1 -c ${CFLAGS} -o ${.TARGET} ${.ALLSRC:M*fp-bit*}

#-----------------------------------------------------------------------

# Remove any objects from LIB2FUNCS and LIB2_DIVMOD_FUNCS that are
# defined as optimized assembly code in LIB1ASMFUNCS.
.if defined(LIB1ASMFUNCS)
.for sym in ${LIB1ASMFUNCS}
LIB2FUNCS_1:=	${LIB2FUNCS_1:S/${sym}//g}
LIB2FUNCS_2:=	${LIB2FUNCS_2:S/${sym}//g}
LIB2_DIVMOD_FUNCS:= ${LIB2_DIVMOD_FUNCS:S/${sym}//g}
.endfor
.endif

ASM_T=		${LIB1ASMFUNCS:S/$/.o/}
ASM_P=		${LIB1ASMFUNCS:S/$/.po/}
ASM_S=		${LIB1ASMFUNCS:S/$/.So/}

SYMS=		${LIB2FUNCS_1}		\
		${LIB2FUNCS_2}		\
		${LIB2_DIVMOD_FUNCS}
.if ${TARGET_ARCH} == "arm" || ${TARGET_ARCH} == "powerpc" || ${TARGET_ARCH} == "sparc64"
SYMS+=		${FPBIT_FUNCS} ${DPBIT_FUNCS}
.endif
SYMS_ST=	${LIB2FUNCS_ST}		\
		${LIB2ADD_ST}
OBJS+=		${SYMS:S/$/.o/} ${LIB1ASMFUNCS:S/$/.o/}
OBJS_T=		${SYMS:S/$/.o/}  ${SYMS_ST:S/$/.o/}
OBJS_P=		${SYMS:S/$/.po/} ${SYMS_ST:S/$/.po/}
OBJS_S=		${SYMS:S/$/.So/}
STATICOBJS=	${SYMS_ST:S/$/.o/}
SRCS=		${LIB2ADD} ${LIB2ADDEH}

COMMONHDRS=	tconfig.h
SRCS+=		${COMMONHDRS}
CLEANFILES+=	${COMMONHDRS}

tconfig.h: ${.CURDIR}/../../usr.bin/cc/cc_tools/Makefile
	${MAKE} -f ${.ALLSRC} MFILE=${.ALLSRC} GCCDIR=${GCCDIR} ${.TARGET}

${OBJS} beforedepend: ${COMMONHDRS}

${OBJS_T}: libgcc2.c
	${XCC} -c ${CFLAGS} -DL${.PREFIX} -o ${.TARGET} ${.ALLSRC:M*.c}
	@${LD} -o ${.TARGET}.tmp -x -r ${.TARGET}
	@mv ${.TARGET}.tmp ${.TARGET}

.if !defined(NOPIC)
${OBJS_S}: libgcc2.c
	${XCC} -c ${PICFLAG} ${CFLAGS} -DL${.PREFIX} -o ${.TARGET} ${.ALLSRC:M*.c}
	@${LD} -o ${.TARGET}.tmp -x -r ${.TARGET}
	@mv ${.TARGET}.tmp ${.TARGET}
.endif

.if !defined(NOPROFILE)
${OBJS_P}: libgcc2.c
	${XCC} -c -p ${CFLAGS} -DL${.PREFIX} -o ${.TARGET} ${.ALLSRC:M*.c}
	@${LD} -o ${.TARGET}.tmp -X -r ${.TARGET}
	@mv ${.TARGET}.tmp ${.TARGET}
.endif

.if defined(LIB1ASMSRC)
.for _lib1asmsrc in ${LIB1ASMSRC}
${ASM_T}: ${_lib1asmsrc}
	${XCC} -x assembler-with-cpp -c ${CFLAGS} -DL${.PREFIX} \
	    -o ${.TARGET} ${.ALLSRC:N*.h}
	@${LD} -o ${.TARGET}.tmp -x -r ${.TARGET}
	@mv ${.TARGET}.tmp ${.TARGET}

.if !defined(NOPIC)
${ASM_S}: ${_lib1asmsrc}
	${XCC} -x assembler-with-cpp -c ${PICFLAG} ${CFLAGS} -DL${.PREFIX} \
	    -o ${.TARGET} ${.ALLSRC:N*.h}
	@${LD} -o ${.TARGET}.tmp -x -r ${.TARGET}
	@mv ${.TARGET}.tmp ${.TARGET}
.endif

.if !defined(NOPROFILE)
${ASM_P}: ${_lib1asmsrc}
	${XCC} -x assembler-with-cpp -p -c ${CFLAGS} -DL${.PREFIX} \
	    -o ${.TARGET} ${.ALLSRC:N*.h}
	@${LD} -o ${.TARGET}.tmp -X -r ${.TARGET}
	@mv ${.TARGET}.tmp ${.TARGET}
.endif
.endfor
.endif

.include <bsd.lib.mk>
