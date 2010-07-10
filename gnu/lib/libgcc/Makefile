# $FreeBSD$

GCCDIR=	${.CURDIR}/../../../contrib/gcc
GCCLIB=	${.CURDIR}/../../../contrib/gcclibs

LIB=		gcc
SHLIB_NAME=	libgcc_s.so.1
SHLIBDIR?=	/lib

.include <bsd.own.mk>
#
# libgcc is linked in last and thus cannot depend on ssp symbols coming
# from earlier libraries. Disable stack protection for this library.
#
MK_SSP=	no

.include "${.CURDIR}/../../usr.bin/cc/Makefile.tgt"

.PATH: ${GCCDIR}/config/${GCC_CPU} ${GCCDIR}/config ${GCCDIR}

CFLAGS+=	-DIN_GCC -DIN_LIBGCC2 -D__GCC_FLOAT_NOT_NEEDED \
		-DHAVE_GTHR_DEFAULT \
		-I${GCCLIB}/include \
		-I${GCCDIR}/config -I${GCCDIR} -I. \
		-I${.CURDIR}/../../usr.bin/cc/cc_tools

LDFLAGS+=	-nodefaultlibs
LDADD+=		-lc

OBJS=		# added to below in various ways depending on TARGET_ARCH

#---------------------------------------------------------------------------
#
# When upgrading GCC, get the following defintions straight from Makefile.in
#
# Library members defined in libgcc2.c.
LIB2FUNCS= _muldi3 _negdi2 _lshrdi3 _ashldi3 _ashrdi3 \
	_cmpdi2 _ucmpdi2 _clear_cache \
	_enable_execute_stack _trampoline __main _absvsi2 _absvdi2 _addvsi3 \
	_addvdi3 _subvsi3 _subvdi3 _mulvsi3 _mulvdi3 _negvsi2 _negvdi2 _ctors \
	_ffssi2 _ffsdi2 _clz _clzsi2 _clzdi2 _ctzsi2 _ctzdi2 _popcount_tab \
	_popcountsi2 _popcountdi2 _paritysi2 _paritydi2 _powisf2 _powidf2 \
	_powixf2 _powitf2 _mulsc3 _muldc3 _mulxc3 _multc3 _divsc3 _divdc3 \
	_divxc3 _divtc3

# The floating-point conversion routines that involve a single-word integer.
.for mode in sf df xf
LIB2FUNCS+= _fixuns${mode}si
.endfor

# Likewise double-word routines.
.for mode in sf df xf tf
LIB2FUNCS+= _fix${mode}di _fixuns${mode}di
LIB2FUNCS+= _floatdi${mode} _floatundi${mode}
.endfor

LIB2ADD = $(LIB2FUNCS_EXTRA)
LIB2ADD_ST = $(LIB2FUNCS_STATIC_EXTRA)

# Additional sources to handle exceptions; overridden by targets as needed.
LIB2ADDEH = unwind-dw2.c unwind-dw2-fde-glibc.c unwind-sjlj.c gthr-gnat.c \
	unwind-c.c
LIB2ADDEHSTATIC = $(LIB2ADDEH)
LIB2ADDEHSHARED = $(LIB2ADDEH)

# List of extra C and assembler files to add to static and shared libgcc2.
# Assembler files should have names ending in `.asm'.
LIB2FUNCS_EXTRA =

# List of extra C and assembler files to add to static libgcc2.
# Assembler files should have names ending in `.asm'.
LIB2FUNCS_STATIC_EXTRA =

# Defined in libgcc2.c, included only in the static library.
# KAN: Excluded _sf_to_tf and _df_to_tf as TPBIT_FUNCS are not
# built on any of our platforms.
LIB2FUNCS_ST = _eprintf __gcc_bcmp

FPBIT_FUNCS = _pack_sf _unpack_sf _addsub_sf _mul_sf _div_sf \
    _fpcmp_parts_sf _compare_sf _eq_sf _ne_sf _gt_sf _ge_sf \
    _lt_sf _le_sf _unord_sf _si_to_sf _sf_to_si _negate_sf _make_sf \
    _sf_to_df _thenan_sf _sf_to_usi _usi_to_sf

DPBIT_FUNCS = _pack_df _unpack_df _addsub_df _mul_df _div_df \
    _fpcmp_parts_df _compare_df _eq_df _ne_df _gt_df _ge_df \
    _lt_df _le_df _unord_df _si_to_df _df_to_si _negate_df _make_df \
    _df_to_sf _thenan_df _df_to_usi _usi_to_df

TPBIT_FUNCS = _pack_tf _unpack_tf _addsub_tf _mul_tf _div_tf \
    _fpcmp_parts_tf _compare_tf _eq_tf _ne_tf _gt_tf _ge_tf \
    _lt_tf _le_tf _unord_tf _si_to_tf _tf_to_si _negate_tf _make_tf \
    _tf_to_df _tf_to_sf _thenan_tf _tf_to_usi _usi_to_tf

# These might cause a divide overflow trap and so are compiled with
# unwinder info.
LIB2_DIVMOD_FUNCS = _divdi3 _moddi3 _udivdi3 _umoddi3 _udiv_w_sdiv _udivmoddi4

#-----------------------------------------------------------------------
#
#	Platform specific bits.
#	When upgrading GCC, get the following definitions from config/<cpu>/t-*
#
.if ${TARGET_ARCH} == "arm"
#	from config/arm/t-strongarm-elf
CFLAGS+=	-Dinhibit_libc -fno-inline
LIB1ASMSRC =	lib1funcs.asm
LIB1ASMFUNCS =  _dvmd_tls _bb_init_func
LIB2FUNCS_EXTRA = floatunsidf.c floatunsisf.c

# Not now
#LIB1ASMFUNCS =  _udivsi3 _divsi3 _umodsi3 _modsi3 _dvmd_tls _bb_init_func
#LIB1ASMFUNCS+=	_call_via_rX _interwork_call_via_rX \
#	_lshrdi3 _ashrdi3 _ashldi3 \
#	_negdf2 _addsubdf3 _muldivdf3 _cmpdf2 _unorddf2 _fixdfsi _fixunsdfsi \
#	_truncdfsf2 _negsf2 _addsubsf3 _muldivsf3 _cmpsf2 _unordsf2 \
#	_fixsfsi _fixunssfsi _floatdidf _floatdisf
.endif

.if ${TARGET_ARCH} == "mips"
LIB2FUNCS_EXTRA = floatunsidf.c floatunsisf.c
.if defined(TARGET_ABI) && ${TARGET_ABI} != "o32"
LIB2FUNCS_EXTRA+= floatdidf.c fixunsdfsi.c
LIB2FUNCS_EXTRA+= floatdisf.c floatundidf.c
LIB2FUNCS_EXTRA+= fixsfdi.c floatundisf.c
LIB2FUNCS_EXTRA+= fixdfdi.c fixunssfsi.c
.endif
.endif

.if ${TARGET_ARCH} == "ia64"
#	from config/ia64/t-ia64
LIB1ASMSRC   = lib1funcs.asm
LIB1ASMFUNCS = __divxf3 __divdf3 __divsf3 \
	__divdi3 __moddi3 __udivdi3 __umoddi3 \
	__divsi3 __modsi3 __udivsi3 __umodsi3 __save_stack_nonlocal \
	__nonlocal_goto __restore_stack_nonlocal __trampoline \
	_fixtfdi _fixunstfdi _floatditf
LIB2ADDEH = unwind-ia64.c unwind-sjlj.c unwind-c.c
.endif

.if ${TARGET_ARCH} == "powerpc"
#	from config/rs6000/t-ppccomm
LIB2FUNCS_EXTRA = tramp.asm
LIB2FUNCS_STATIC_EXTRA = eabi.asm
.endif

.if ${TARGET_ARCH} == "powerpc64"
#	from config/rs6000/t-ppccomm
LIB2FUNCS_EXTRA = tramp.asm
.endif

.if ${TARGET_ARCH} == "sparc64"
#	from config/sparc/t-elf
LIB1ASMSRC =   lb1spc.asm
LIB1ASMFUNCS = _mulsi3 _divsi3 _modsi3
.endif

#-----------------------------------------------------------------------

# Remove any objects from LIB2FUNCS and LIB2_DIVMOD_FUNCS that are
# defined as optimized assembly code in LIB1ASMFUNCS.
.if defined(LIB1ASMFUNCS)
.for sym in ${LIB1ASMFUNCS}
LIB2FUNCS:=	${LIB2FUNCS:S/${sym}//g}
LIB2_DIVMOD_FUNCS:= ${LIB2_DIVMOD_FUNCS:S/${sym}//g}
.endfor
.endif

COMMONHDRS=	tm.h tconfig.h options.h unwind.h gthr-default.h

#-----------------------------------------------------------------------
#
# Helpful shortcuts for compiler invocations.
#
HIDE =  -fvisibility=hidden -DHIDE_EXPORTS
CC_T =	${CC} -c ${CFLAGS} ${HIDE} -fPIC
CC_P =	${CC} -c ${CFLAGS} ${HIDE} -p -fPIC
CC_S =	${CC} -c ${CFLAGS} ${PICFLAG} -DSHARED

#-----------------------------------------------------------------------
#
# Functions from libgcc2.c
#
STD_CFLAGS =
DIV_CFLAGS =	-fexceptions -fnon-call-exceptions

STD_FUNCS =	${LIB2FUNCS}
DIV_FUNCS =	${LIB2_DIVMOD_FUNCS}

STD_CFILE =	libgcc2.c
DIV_CFILE =	libgcc2.c

OBJ_GRPS =	STD DIV

#-----------------------------------------------------------------------
#
# Floating point emulation functions
#
.if ${TARGET_ARCH} == "armNOT_YET" || \
    ${TARGET_ARCH} == "powerpc" || ${TARGET_ARCH} == "sparc64"

FPBIT_CFLAGS =	-DFINE_GRAINED_LIBRARIES -DFLOAT
DPBIT_CFLAGS =	-DFINE_GRAINED_LIBRARIES

FPBIT_CFILE =	config/fp-bit.c
DPBIT_CFILE =	config/fp-bit.c

OBJ_GRPS +=	FPBIT DPBIT
.endif

#-----------------------------------------------------------------------
#
# Generic build rules for object groups defined above
#
.for T in ${OBJ_GRPS}
${T}_OBJS_T =	${${T}_FUNCS:S/$/.o/}
${T}_OBJS_P =	${${T}_FUNCS:S/$/.po/}
${T}_OBJS_S =	${${T}_FUNCS:S/$/.So/}
OBJS +=		${${T}_FUNCS:S/$/.o/}

${${T}_OBJS_T}: ${${T}_CFILE} ${COMMONHDRS}
	${CC_T} ${${T}_CFLAGS} -DL${.PREFIX} -o ${.TARGET} ${.ALLSRC:M*.c}
${${T}_OBJS_P}: ${${T}_CFILE} ${COMMONHDRS}
	${CC_P} ${${T}_CFLAGS} -DL${.PREFIX} -o ${.TARGET} ${.ALLSRC:M*.c}
${${T}_OBJS_S}: ${${T}_CFILE} ${COMMONHDRS}
	${CC_S} ${${T}_CFLAGS} -DL${.PREFIX} -o ${.TARGET} ${.ALLSRC:M*.c}
.endfor

#-----------------------------------------------------------------------
#
# Extra objects coming from separate files
#
.if !empty(LIB2ADD)
OBJS  +=	${LIB2ADD:R:S/$/.o/}
SOBJS +=	${LIB2ADD:R:S/$/.So/}
POBJS +=	${LIB2ADD:R:S/$/.po/}
.endif

#-----------------------------------------------------------------------
#
# Objects that should be in static library only.
#
SYMS_ST =	${LIB2FUNCS_ST}	${LIB2ADD_ST}
STAT_OBJS_T = 	${SYMS_ST:S/$/.o/}
STAT_OBJS_P = 	${SYMS_ST:S/$/.po/}
STATICOBJS  =	${SYMS_ST:S/$/.o/}

${STAT_OBJS_T}:	${STD_CFILE} ${COMMONHDRS}
	${CC_T} -DL${.PREFIX} -o ${.TARGET} ${.ALLSRC:M*.c}
${STAT_OBJS_P}:	${STD_CFILE} ${COMMONHDRS}
	${CC_P} -DL${.PREFIX} -o ${.TARGET} ${.ALLSRC:M*.c}

#-----------------------------------------------------------------------
#
# Assembler files.
#
.if defined(LIB1ASMSRC)
ASM_T =		${LIB1ASMFUNCS:S/$/.o/}
ASM_P =		${LIB1ASMFUNCS:S/$/.po/}
ASM_S =		${LIB1ASMFUNCS:S/$/.So/}
ASM_V =		${LIB1ASMFUNCS:S/$/.vis/}
OBJS +=		${LIB1ASMFUNCS:S/$/.o/}

${ASM_T}: ${LIB1ASMSRC} ${.PREFIX}.vis
	${CC} -x assembler-with-cpp -c ${CFLAGS} -DL${.PREFIX} \
	    -o ${.TARGET} -include ${.PREFIX}.vis ${.ALLSRC:N*.h:N*.vis}
${ASM_P}: ${LIB1ASMSRC} ${.PREFIX}.vis
	${CC} -x assembler-with-cpp -p -c ${CFLAGS} -DL${.PREFIX} \
	    -o ${.TARGET} -include ${.PREFIX}.vis ${.ALLSRC:N*.h:N*.vis}
${ASM_S}: ${LIB1ASMSRC}
	${CC} -x assembler-with-cpp -c ${PICFLAG} ${CFLAGS} -DL${.PREFIX} \
	    -o ${.TARGET} ${.ALLSRC:N*.h}
${ASM_V}: ${LIB1ASMSRC}
	${CC} -x assembler-with-cpp -c ${CFLAGS} -DL${.PREFIX} \
	    -o ${.PREFIX}.vo ${.ALLSRC:N*.h}
	( nm -pg ${.PREFIX}.vo | \
		awk 'NF == 3 && $$2 !~ /^[UN]$$/ { print "\t.hidden ", $$3 }'\
	) > ${.TARGET}

CLEANFILES += ${ASM_V} ${ASM_V:R:S/$/.vo/}
.endif

#-----------------------------------------------------------------------
#
# Exception handling / unwinding support.
#
EH_OBJS_T = ${LIB2ADDEHSTATIC:R:S/$/.o/}
EH_OBJS_P = ${LIB2ADDEHSTATIC:R:S/$/.po/}
EH_OBJS_S = ${LIB2ADDEHSHARED:R:S/$/.So/}
EH_CFLAGS = -fexceptions -D__GLIBC__=3 -DElfW=__ElfN
SOBJS    += ${EH_OBJS_S}

.for _src in ${LIB2ADDEHSTATIC}
${_src:R:S/$/.o/}: ${_src} ${COMMONHDRS}
	${CC_T} ${EH_CFLAGS} -o ${.TARGET} ${.IMPSRC}
${_src:R:S/$/.po/}: ${_src} ${COMMONHDRS}
	${CC_P} ${EH_CFLAGS} -o ${.TARGET} ${.IMPSRC}
.endfor
.for _src in ${LIB2ADDEHSHARED}
${_src:R:S/$/.So/}: ${_src} ${COMMONHDRS}
	${CC_S} ${EH_CFLAGS} -o ${.TARGET} ${.IMPSRC}
.endfor


#-----------------------------------------------------------------------
#
# Generated headers
#
${COMMONHDRS}: ${.CURDIR}/../../usr.bin/cc/cc_tools/Makefile
	${MAKE} -f ${.ALLSRC} MFILE=${.ALLSRC} GCCDIR=${GCCDIR} ${.TARGET}

CLEANFILES += ${COMMONHDRS}
CLEANFILES += cs-*.h option*

#-----------------------------------------------------------------------
#
# Build symbol version map
#
SHLIB_MKMAP      = ${GCCDIR}/mkmap-symver.awk
SHLIB_MKMAP_OPTS =
SHLIB_MAPFILES   = ${GCCDIR}/libgcc-std.ver
VERSION_MAP      = libgcc.map

libgcc.map: ${SHLIB_MKMAP} ${SHLIB_MAPFILES} ${SOBJS} ${OBJS:R:S/$/.So/}
	(  nm -pg ${SOBJS};echo %% ; \
	  cat ${SHLIB_MAPFILES} \
	    | sed -e '/^[   ]*#/d' \
	          -e 's/^%\(if\|else\|elif\|endif\|define\)/#\1/' \
	    | ${CC} ${CFLAGS} -E -xassembler-with-cpp -; \
	) | awk -f ${SHLIB_MKMAP} ${SHLIB_MKMAP_OPTS} > ${.TARGET}

CLEANFILES +=	libgcc.map

#-----------------------------------------------------------------------
#
# Build additional static libgcc_eh[_p].a libraries.
#
lib${LIB}_eh.a:	${EH_OBJS_T}
	@${ECHO} building static ${LIB}_eh library
	@rm -f ${.TARGET}
	@${AR} cq ${.TARGET} `lorder ${EH_OBJS_T} | tsort -q`
	${RANLIB} ${.TARGET}

all:	lib${LIB}_eh.a

.if ${MK_PROFILE} != "no"
lib${LIB}_eh_p.a:	${EH_OBJS_P}
	@${ECHO} building profiled ${LIB}_eh library
	@rm -f ${.TARGET}
	@${AR} cq ${.TARGET} `lorder ${EH_OBJS_P} | tsort -q`
	${RANLIB} ${.TARGET}
all:	lib${LIB}_eh_p.a
.endif

_libinstall: _lib-eh-install

_lib-eh-install:
.if ${MK_INSTALLLIB} != "no"
	${INSTALL} -C -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
		${_INSTALLFLAGS} lib${LIB}_eh.a ${DESTDIR}${LIBDIR}
.endif
.if ${MK_PROFILE} != "no"
	${INSTALL} -C -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
		${_INSTALLFLAGS} lib${LIB}_eh_p.a ${DESTDIR}${LIBDIR}
.endif

CLEANFILES+=	lib${LIB}_eh.a lib${LIB}_eh_p.a ${EH_OBJS_T} ${EH_OBJS_P}

.include <bsd.lib.mk>

.SUFFIXES: .vis .vo
