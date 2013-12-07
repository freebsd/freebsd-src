# $FreeBSD$
#
# This file contains common settings used for building FreeBSD
# sources.

# Enable various levels of compiler warning checks.  These may be
# overridden (e.g. if using a non-gcc compiler) by defining NO_WARNS.

# for GCC:   http://gcc.gnu.org/onlinedocs/gcc-4.2.1/gcc/Warning-Options.html

.include <bsd.compiler.mk>

# the default is gnu99 for now
CSTD?=		gnu99

.if ${CSTD} == "k&r"
CFLAGS+=	-traditional
.elif ${CSTD} == "c89" || ${CSTD} == "c90"
CFLAGS+=	-std=iso9899:1990
.elif ${CSTD} == "c94" || ${CSTD} == "c95"
CFLAGS+=	-std=iso9899:199409
.elif ${CSTD} == "c99"
CFLAGS+=	-std=iso9899:1999
.else # CSTD
CFLAGS+=	-std=${CSTD}
.endif # CSTD
.if !defined(NO_WARNS)
# -pedantic is problematic because it also imposes namespace restrictions
#CFLAGS+=	-pedantic
.if defined(WARNS)
.if ${WARNS} >= 1
CWARNFLAGS+=	-Wsystem-headers
.if !defined(NO_WERROR) && (${COMPILER_TYPE} != "clang" \
    || !defined(NO_WERROR.clang))
CWARNFLAGS+=	-Werror
.endif # !NO_WERROR && (!CLANG || !NO_WERROR.clang)
.endif # WARNS >= 1
.if ${WARNS} >= 2
CWARNFLAGS+=	-Wall -Wno-format-y2k
.endif # WARNS >= 2
.if ${WARNS} >= 3
CWARNFLAGS+=	-W -Wno-unused-parameter -Wstrict-prototypes\
		-Wmissing-prototypes -Wpointer-arith
.endif # WARNS >= 3
.if ${WARNS} >= 4
CWARNFLAGS+=	-Wreturn-type -Wcast-qual -Wwrite-strings -Wswitch -Wshadow\
		-Wunused-parameter
.if !defined(NO_WCAST_ALIGN) && (${COMPILER_TYPE} != "clang" \
    || !defined(NO_WCAST_ALIGN.clang))
CWARNFLAGS+=	-Wcast-align
.endif # !NO_WCAST_ALIGN && (!CLANG || !NO_WCAST_ALIGN.clang)
.endif # WARNS >= 4
# BDECFLAGS
.if ${WARNS} >= 6
CWARNFLAGS+=	-Wchar-subscripts -Winline -Wnested-externs -Wredundant-decls\
		-Wold-style-definition
.if ${COMPILER_TYPE} == "clang" && !defined(EARLY_BUILD) && \
    !defined(NO_WMISSING_VARIABLE_DECLARATIONS)
CWARNFLAGS+=	-Wmissing-variable-declarations
.endif
.endif # WARNS >= 6
.if ${WARNS} >= 2 && ${WARNS} <= 4
# XXX Delete -Wuninitialized by default for now -- the compiler doesn't
# XXX always get it right.
CWARNFLAGS+=	-Wno-uninitialized
.endif # WARNS >=2 && WARNS <= 4
CWARNFLAGS+=	-Wno-pointer-sign
# Clang has more warnings enabled by default, and when using -Wall, so if WARNS
# is set to low values, these have to be disabled explicitly.
.if ${COMPILER_TYPE} == "clang" && !defined(EARLY_BUILD)
.if ${WARNS} <= 6
CWARNFLAGS+=	-Wno-empty-body -Wno-string-plus-int
.endif # WARNS <= 6
.if ${WARNS} <= 3
CWARNFLAGS+=	-Wno-tautological-compare -Wno-unused-value\
		-Wno-parentheses-equality -Wno-unused-function -Wno-enum-conversion
.endif # WARNS <= 3
.if ${WARNS} <= 2
CWARNFLAGS+=	-Wno-switch -Wno-switch-enum -Wno-knr-promoted-parameter
.endif # WARNS <= 2
.if ${WARNS} <= 1
CWARNFLAGS+=	-Wno-parentheses
.endif # WARNS <= 1
.if defined(NO_WARRAY_BOUNDS)
CWARNFLAGS+=	-Wno-array-bounds
.endif # NO_WARRAY_BOUNDS
.endif # CLANG
.endif # WARNS

.if defined(FORMAT_AUDIT)
WFORMAT=	1
.endif # FORMAT_AUDIT
.if defined(WFORMAT)
.if ${WFORMAT} > 0
#CWARNFLAGS+=	-Wformat-nonliteral -Wformat-security -Wno-format-extra-args
CWARNFLAGS+=	-Wformat=2 -Wno-format-extra-args
.if ${COMPILER_TYPE} == "clang" && !defined(EARLY_BUILD)
.if ${WARNS} <= 3
CWARNFLAGS+=	-Wno-format-nonliteral
.endif # WARNS <= 3
.endif # CLANG
.if !defined(NO_WERROR) && (${COMPILER_TYPE} != "clang" \
    || !defined(NO_WERROR.clang))
CWARNFLAGS+=	-Werror
.endif # !NO_WERROR && (!CLANG || !NO_WERROR.clang)
.endif # WFORMAT > 0
.endif # WFORMAT
.if defined(NO_WFORMAT) || (${COMPILER_TYPE} == "clang" && defined(NO_WFORMAT.clang))
CWARNFLAGS+=	-Wno-format
.endif # NO_WFORMAT || (CLANG && NO_WFORMAT.clang)
.endif # !NO_WARNS

.if defined(IGNORE_PRAGMA)
CWARNFLAGS+=	-Wno-unknown-pragmas
.endif # IGNORE_PRAGMA

.if ${COMPILER_TYPE} == "clang" && !defined(EARLY_BUILD)
CLANG_NO_IAS=	 -no-integrated-as
CLANG_OPT_SMALL= -mstack-alignment=8 -mllvm -inline-threshold=3\
		 -mllvm -enable-load-pre=false -mllvm -simplifycfg-dup-ret
CFLAGS+=	 -Qunused-arguments
.endif # CLANG

.if ${MK_SSP} != "no" && ${MACHINE_CPUARCH} != "ia64" && \
    ${MACHINE_CPUARCH} != "arm" && ${MACHINE_CPUARCH} != "mips"
# Don't use -Wstack-protector as it breaks world with -Werror.
SSP_CFLAGS?=	-fstack-protector
CFLAGS+=	${SSP_CFLAGS}
.endif # SSP && !IA64 && !ARM && !MIPS

# Allow user-specified additional warning flags
CFLAGS+=	${CWARNFLAGS}


# Tell bmake not to mistake standard targets for things to be searched for
# or expect to ever be up-to-date.
PHONY_NOTMAIN = afterdepend afterinstall all beforedepend beforeinstall \
		beforelinking build build-tools buildfiles buildincludes \
		checkdpadd clean cleandepend cleandir cleanobj configure \
		depend dependall distclean distribute exe extract fetch \
		html includes install installfiles installincludes lint \
		obj objlink objs objwarn patch realall realdepend \
		realinstall regress subdir-all subdir-depend subdir-install \
		tags whereobj

.PHONY: ${PHONY_NOTMAIN}
.NOTMAIN: ${PHONY_NOTMAIN}
