# $FreeBSD$
# ex:ts=8

GCCDIR=	${.CURDIR}/../../../contrib/gcc

.PATH: ${GCCDIR}/cp ${GCCDIR}

# allow to be overridden for the a.out case
.if !defined(LIB) || ${LIB} != "gcc_r"
LIB=	gcc
.endif

.if ${OBJFORMAT} == aout
# Install libgcc_pic.a, since ld.so uses it.
INSTALL_PIC_ARCHIVE=  yes
.endif

CFLAGS+=	-fexceptions
CFLAGS+=	-DIN_GCC -DIN_LIBGCC2 -D__GCC_FLOAT_NOT_NEEDED
.if ${OBJFORMAT} != aout
#CFLAGS+=	-fPIC -D_PTHREADS -DGTHREAD_USE_WEAK
CFLAGS+=	-D_PTHREADS -DGTHREAD_USE_WEAK
.endif

all: lib${LIB}.a

lib${LIB}.a: libgcc_build.a
	cp -p ${.ALLSRC} ${.TARGET}

CLEANFILES+=	lib${LIB}.a

#
#	fake out to semi-follow what is done in GCC's Makefile.in
#
srcdir=		${GCCDIR}
GCC_FOR_TARGET=	${CC}
LIBGCC2_CFLAGS=	${CFLAGS}
INCLUDES=	-I${GCCDIR}/config \
		-I${GCCDIR} \
		-I. \
		-I${.CURDIR}/../../usr.bin/cc/cc_tools

COMMONHDRS=	config.h tconfig.h tm.h
CLEANFILES+=	${COMMONHDRS}
SRCS+=		${COMMONHDRS}

config.h:
	echo '#include "auto-host.h"'				> ${.TARGET}
	echo '#include "gansidecl.h"'				>> ${.TARGET}
	echo '#include <${MACHINE_ARCH}/xm-${MACHINE_ARCH}.h>'	>> ${.TARGET}
	echo '#include <xm-freebsd.h>'				>> ${.TARGET}
	echo '#include "defaults.h"'			>> ${.TARGET}
	echo '#include "hwint.h"'			>> ${.TARGET}
	echo '#ifndef POSIX'				>> ${.TARGET}
	echo '# define POSIX'				>> ${.TARGET}
	echo '#endif'					>> ${.TARGET}


tconfig.h:
	echo '#include "gansidecl.h"'			> ${.TARGET}
	echo '#include "${MACHINE_ARCH}/xm-${MACHINE_ARCH}.h"'	>> ${.TARGET}
	echo '#include "defaults.h"'			>> ${.TARGET}


#	KEEP THIS IN SYNC with src/gcc/usr.bin/cc/cc_tools/Makefile !!
tm.h:
	echo '#include "gansidecl.h"'				> ${.TARGET}
	echo '#include "${MACHINE_ARCH}/${MACHINE_ARCH}.h"'	>> ${.TARGET}
.if ${MACHINE_ARCH} == "i386"
	echo '#include "${MACHINE_ARCH}/att.h"'			>> ${.TARGET}
.endif
	echo '#include <freebsd-native.h>'		>> ${.TARGET}
	echo '#include <freebsd.h>'				>> ${.TARGET}
.if exists(${MACHINE_ARCH}/config/${MACHINE_ARCH}/elf.h)
	echo '#include "${MACHINE_ARCH}/elf.h"'		>> ${.TARGET}
.endif
	echo '#include "${MACHINE_ARCH}/freebsd.h"'		>> ${.TARGET}
.if ${MACHINE_ARCH} == "i386"
	echo '#include "${MACHINE_ARCH}/perform.h"'		>> ${.TARGET}
.endif
	echo '#include "defaults.h"'			>> ${.TARGET}


#
# XXX Make sure changes in GCC's Makefile.in are synced here !!
#

libgcc_build.a: ${COMMONHDRS} libgcc.mk
	$(MAKE) GCC_FOR_TARGET="$(GCC_FOR_TARGET)" \
	  AR_FOR_TARGET="ar" \
	  AR_CREATE_FOR_TARGET="ar rc" \
	  AR_FLAGS_FOR_TARGET="" \
	  OLDCC="${CC}" CCLIBFLAGS="-O" \
	  CFLAGS="$(CFLAGS) $(WARN_CFLAGS)" \
	  RANLIB_FOR_TARGET="ranlib" \
	  RANLIB_TEST_FOR_TARGET="true" \
	  NM_FOR_TARGET="nm" AWK="awk" \
	  LIBGCC2_CFLAGS="$(LIBGCC2_CFLAGS)" \
	  INCLUDES="$(INCLUDES)" MAYBE_USE_COLLECT2="$(MAYBE_USE_COLLECT2)" \
	  CONFIG_H="${COMMONHDRS}" MACHMODE_H="$(MACHMODE_H)" \
	  LIB1ASMSRC='$(LIB1ASMSRC)' \
	  -f libgcc.mk ./libgcc_build.a

#
# XXX Make sure changes in GCC's Makefile.in are synced here !!
#

libgcc.mk: mklibgcc
	objext='.o' \
	OLDCC='${CC}' \
	LIBGCC1='libgcc1.a' \
	LIB1FUNCS='$(LIB1FUNCS)' \
	LIB1ASMFUNCS='$(LIB1ASMFUNCS)' \
	LIB1FUNCS_EXTRA='$(LIB1FUNCS_EXTRA)' \
	LIB2FUNCS='$(LIB2FUNCS)' \
	LIB2FUNCS_ST='$(LIB2FUNCS_ST)' \
	LIB2ADD='$(LIB2ADD)' \
	LIB2ADDEH='$(LIB2ADDEH)' \
	LIB2ADDEHDEP='$(LIB2ADDEHDEP)' \
	LIB2ADD_ST='$(LIB2ADD_ST)' \
	FPBIT='$(FPBIT)' \
	FPBIT_FUNCS='$(FPBIT_FUNCS)' \
	DPBIT='$(DPBIT)' \
	DPBIT_FUNCS='$(DPBIT_FUNCS)' \
	MULTILIBS='.;' \
	EXTRA_MULTILIB_PARTS='$(EXTRA_MULTILIB_PARTS)' \
	SHLIB_LINK='$(SHLIB_LINK)' \
	SHLIB_INSTALL='$(SHLIB_INSTALL)' \
	SHLIB_EXT='$(SHLIB_EXT)' \
	SHLIB_MULTILIB='$(SHLIB_MULTILIB)' \
	SHLIB_MKMAP='$(SHLIB_MKMAP)' \
	SHLIB_MAPFILES='$(SHLIB_MAPFILES)' \
	SHLIB_NM_FLAGS='$(SHLIB_NM_FLAGS)' \
	  $(SHELL) mklibgcc > tmp-libgcc.mk
	echo ".PATH: ${GCCDIR}" > ${.TARGET}
	sed -e 's|libgcc/\./||g' tmp-libgcc.mk >> ${.TARGET}
	rm tmp-libgcc.mk
	touch config.status stmp-int-hdrs

mklibgcc: mklibgcc.in
	sed								\
	    -e 's|libgcc\.a|libgcc_build.a|g'				\
	    -e 's|@srcdir@|${GCCDIR}|g'					\
	    -e 's|@build_canonical@|${MACHINE_ARCH}-deo-freebsd5.0|g'	\
	    -e 's|@target@|${MACHINE_ARCH}-deo-freebsd5.0|g'		\
	    -e 's|@enable_shared@|no|g'					\
	    -e 's|@NO_MINUS_C_MINUS_O@||g'				\
	    ${.ALLSRC} > ${.TARGET}

CLEANFILES+=	mklibgcc libgcc.mk config.status \
		stmp-int-hdrs tmp-libgcc.mk stmp-dirs \
		libgcc_build.a

#
# XXX This is a hack, but it seems to work.
# When upgrading GCC, get the following defintions straight from Makefile.in
#

# Members of libgcc1.a.
LIB1FUNCS = _mulsi3 _udivsi3 _divsi3 _umodsi3 _modsi3 \
   _lshrsi3 _ashrsi3 _ashlsi3 \
   _divdf3 _muldf3 _negdf2 _adddf3 _subdf3 \
   _fixdfsi _fixsfsi _floatsidf _floatsisf _truncdfsf2 _extendsfdf2 \
   _addsf3 _negsf2 _subsf3 _mulsf3 _divsf3 \
   _eqdf2 _nedf2 _gtdf2 _gedf2 _ltdf2 _ledf2 \
   _eqsf2 _nesf2 _gtsf2 _gesf2 _ltsf2 _lesf2

# Library members defined in libgcc2.c.
LIB2FUNCS = _muldi3 _divdi3 _moddi3 _udivdi3 _umoddi3 _negdi2 \
     _lshrdi3 _ashldi3 _ashrdi3 _ffsdi2 _clz \
    _udiv_w_sdiv _udivmoddi4 _cmpdi2 _ucmpdi2 _floatdidf _floatdisf \
    _fixunsdfsi _fixunssfsi _fixunsdfdi _fixdfdi _fixunssfdi _fixsfdi \
    _fixxfdi _fixunsxfdi _floatdixf _fixunsxfsi \
    _fixtfdi _fixunstfdi _floatditf \
    _clear_cache _trampoline __main _exit \
    _absvsi2 _absvdi2 _addvsi3 _addvdi3 _subvsi3 _subvdi3 \
    _mulvsi3 _mulvdi3 _negvsi2 _negvdi2 \
    _ctors

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

# Build libgcc.a.
LIB2ADD = $(LIB2FUNCS_EXTRA)
LIB2ADD_ST = $(LIB2FUNCS_STATIC_EXTRA)

# Additional sources to handle exceptions; overridden on ia64.
LIB2ADDEH = $(srcdir)/unwind-dw2.c $(srcdir)/unwind-dw2-fde.c \
  $(srcdir)/unwind-sjlj.c
LIB2ADDEHDEP = unwind.inc unwind-dw2-fde.h

MACHMODE_H = machmode.h machmode.def

# nm flags to list global symbols in libgcc object files.
SHLIB_NM_FLAGS = -pg

#
#	Bmake'ed tweaks for the GCC Makefile.in barrowed bits.
#

.for objfile in ${LIB1FUNCS} ${LIB2FUNCS} ${LIB2FUNCS_ST} ${FPBIT_FUNCS} \
    ${DPBIT_FUNCS}
CLEANFILES+=	${objfile}.o
.endfor

CLEANFILES+=	${LIB2ADDEH:S/.c/.o/g:S/$(srcdir)\///g}

CLEANDIRS+=	libgcc

#
#	Need to supply an `install' target as we aren't using bsd.lib.mk.
#

install:
	${INSTALL} ${COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${_INSTALLFLAGS} lib${LIB}.a ${DESTDIR}${LIBDIR}

.include <bsd.dep.mk>
.include <bsd.obj.mk>
.include <bsd.sys.mk>
