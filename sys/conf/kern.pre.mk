# $FreeBSD$

# Part of a unified Makefile for building kernels.  This part contains all
# of the definitions that need to be before %BEFORE_DEPEND.

.include <bsd.own.mk>

# backwards compat option for older systems.
MACHINE_CPUARCH?=${MACHINE_ARCH:C/mipse[lb]/mips/:C/armeb/arm/:C/powerpc64/powerpc/}

# Can be overridden by makeoptions or /etc/make.conf
KERNEL_KO?=	kernel
KERNEL?=	kernel
KODIR?=		/boot/${KERNEL}
LDSCRIPT_NAME?=	ldscript.$M
LDSCRIPT?=	$S/conf/${LDSCRIPT_NAME}

M=	${MACHINE_CPUARCH}

AWK?=		awk
LINT?=		lint
NM?=		nm
OBJCOPY?=	objcopy
SIZE?=		size

.if ${CC:T:Micc} == "icc"
COPTFLAGS?=	-O
.else
. if defined(DEBUG)
_MINUS_O=	-O
CTFFLAGS+=	-g
. else
_MINUS_O=	-O2
. endif
. if ${MACHINE_CPUARCH} == "amd64"
COPTFLAGS?=-O2 -frename-registers -pipe
. else
COPTFLAGS?=${_MINUS_O} -pipe
. endif
. if !empty(COPTFLAGS:M-O[23s]) && empty(COPTFLAGS:M-fno-strict-aliasing)
COPTFLAGS+= -fno-strict-aliasing
. endif
.endif
.if !defined(NO_CPU_COPTFLAGS)
. if ${CC:T:Micc} == "icc"
COPTFLAGS+= ${_ICC_CPUCFLAGS:C/(-x[^M^K^W]+)[MKW]+|-x[MKW]+/\1/}
. else
COPTFLAGS+= ${_CPUCFLAGS}
. endif
.endif
.if ${CC:T:Micc} == "icc"
C_DIALECT=
NOSTDINC= -X
.else
C_DIALECT= -std=c99
NOSTDINC= -nostdinc
.endif

INCLUDES= ${NOSTDINC} ${INCLMAGIC} -I. -I$S

# This hack lets us use the OpenBSD altq code without spamming a new
# include path into contrib'ed source files.
INCLUDES+= -I$S/contrib/altq

.if make(depend) || make(kernel-depend)

# ... and the same for ipfilter
INCLUDES+= -I$S/contrib/ipfilter

# ... and the same for pf
INCLUDES+= -I$S/contrib/pf

# ... and the same for ath
INCLUDES+= -I$S/dev/ath -I$S/dev/ath/ath_hal

# ... and the same for the NgATM stuff
INCLUDES+= -I$S/contrib/ngatm

# .. and the same for twa
INCLUDES+= -I$S/dev/twa

# ...  and XFS
INCLUDES+= -I$S/gnu/fs/xfs/FreeBSD -I$S/gnu/fs/xfs/FreeBSD/support -I$S/gnu/fs/xfs

# ...  and OpenSolaris
INCLUDES+= -I$S/contrib/opensolaris/compat

# ... and the same for cxgb
INCLUDES+= -I$S/dev/cxgb

.endif

CFLAGS=	${COPTFLAGS} ${C_DIALECT} ${DEBUG} ${CWARNFLAGS}
CFLAGS+= ${INCLUDES} -D_KERNEL -DHAVE_KERNEL_OPTION_HEADERS -include opt_global.h
.if ${CC:T:Micc} != "icc"
.if ${CC:T:Mclang} != "clang"
CFLAGS+= -fno-common -finline-limit=${INLINE_LIMIT}
.if ${MACHINE_CPUARCH} != "mips"
CFLAGS+= --param inline-unit-growth=100
CFLAGS+= --param large-function-growth=1000
.else
# XXX Actually a gross hack just for Octeon because of the Simple Executive.
CFLAGS+= --param inline-unit-growth=10000
CFLAGS+= --param large-function-growth=100000
CFLAGS+= --param max-inline-insns-single=10000
.endif
.endif
WERROR?= -Werror
.endif

# XXX LOCORE means "don't declare C stuff" not "for locore.s".
ASM_CFLAGS= -x assembler-with-cpp -DLOCORE ${CFLAGS}

.if defined(PROFLEVEL) && ${PROFLEVEL} >= 1
.if ${CC:T:Micc} == "icc" || ${CC:T:Mclang} == "clang"
.error "Profiling doesn't work with icc or clang yet"
.endif
CFLAGS+=	-DGPROF -falign-functions=16
.if ${PROFLEVEL} >= 2
CFLAGS+=	-DGPROF4 -DGUPROF
PROF=	-pg -mprofiler-epilogue
.else
PROF=	-pg
.endif
.endif
DEFINED_PROF=	${PROF}

# Put configuration-specific C flags last (except for ${PROF}) so that they
# can override the others.
CFLAGS+=	${CONF_CFLAGS}

# Optional linting. This can be overridden in /etc/make.conf.
LINTFLAGS=	${LINTOBJKERNFLAGS}

NORMAL_C= ${CC} -c ${CFLAGS} ${WERROR} ${PROF} ${.IMPSRC}
NORMAL_S= ${CC} -c ${ASM_CFLAGS} ${WERROR} ${.IMPSRC}
PROFILE_C= ${CC} -c ${CFLAGS} ${WERROR} ${.IMPSRC}
NORMAL_C_NOWERROR= ${CC} -c ${CFLAGS} ${PROF} ${.IMPSRC}

NORMAL_M= ${AWK} -f $S/tools/makeobjops.awk ${.IMPSRC} -c ; \
	  ${CC} -c ${CFLAGS} ${WERROR} ${PROF} ${.PREFIX}.c

NORMAL_CTFCONVERT= [ -z "${CTFCONVERT}" -o -n "${NO_CTF}" ] || \
		   ${CTFCONVERT} ${CTFFLAGS} ${.TARGET}

NORMAL_LINT=	${LINT} ${LINTFLAGS} ${CFLAGS:M-[DIU]*} ${.IMPSRC}

GEN_CFILES= $S/$M/$M/genassym.c ${MFILES:T:S/.m$/.c/}
SYSTEM_CFILES= config.c env.c hints.c vnode_if.c
SYSTEM_DEP= Makefile ${SYSTEM_OBJS}
SYSTEM_OBJS= locore.o ${MDOBJS} ${OBJS}
SYSTEM_OBJS+= ${SYSTEM_CFILES:.c=.o}
SYSTEM_CTFMERGE= [ -z "${CTFMERGE}" -o -n "${NO_CTF}" ] || ${CTFMERGE} ${CTFFLAGS} -o ${.TARGET} ${SYSTEM_OBJS} vers.o
.if ${MACHINE_ARCH} == XXX_ALTIX_TODO
SYSTEM_LDFLAGS= -r
.else
SYSTEM_OBJS+= hack.So
SYSTEM_LDFLAGS= -Bdynamic -T ${LDSCRIPT} -export-dynamic \
	-dynamic-linker /red/herring
.endif
SYSTEM_LD= @${LD} ${SYSTEM_LDFLAGS} -warn-common \
	-o ${.TARGET} -X ${SYSTEM_OBJS} vers.o
SYSTEM_LD_TAIL= @${OBJCOPY} --strip-symbol gcc2_compiled. ${.TARGET} ; \
	${SIZE} ${.TARGET} ; chmod 755 ${.TARGET}
SYSTEM_DEP+= ${LDSCRIPT}

# MKMODULESENV is set here so that port makefiles can augment
# them.

MKMODULESENV=	MAKEOBJDIRPREFIX=${.OBJDIR}/modules KMODDIR=${KODIR}
MKMODULESENV+=	MACHINE_CPUARCH=${MACHINE_CPUARCH}
.if (${KERN_IDENT} == LINT)
MKMODULESENV+=	ALL_MODULES=LINT
.endif
.if defined(MODULES_OVERRIDE)
MKMODULESENV+=	MODULES_OVERRIDE="${MODULES_OVERRIDE}"
.endif
.if defined(DEBUG)
MKMODULESENV+=	DEBUG_FLAGS="${DEBUG}"
.endif
