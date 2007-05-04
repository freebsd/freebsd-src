# $FreeBSD$

# Part of a unified Makefile for building kernels.  This part contains all
# of the definitions that need to be before %BEFORE_DEPEND.

SRCCONF?=	/etc/src.conf
.if exists(${SRCCONF})
.include "${SRCCONF}"
.endif

# Can be overridden by makeoptions or /etc/make.conf
KERNEL_KO?=	kernel
KERNEL?=	kernel
KODIR?=		/boot/${KERNEL}

M=	${MACHINE_ARCH}

AWK?=		awk
LINT?=		lint
NM?=		nm
OBJCOPY?=	objcopy
SIZE?=		size

.if ${CC} == "icc"
COPTFLAGS?=	-O
.else
. if defined(DEBUG)
_MINUS_O=	-O
. else
_MINUS_O=	-O2
. endif
. if ${MACHINE_ARCH} == "amd64"
COPTFLAGS?=-O2 -frename-registers -pipe
. else
COPTFLAGS?=${_MINUS_O} -pipe
. endif
. if !empty(COPTFLAGS:M-O[23s]) && empty(COPTFLAGS:M-fno-strict-aliasing)
COPTFLAGS+= -fno-strict-aliasing
. endif
.endif
.if !defined(NO_CPU_COPTFLAGS)
. if ${CC} == "icc"
COPTFLAGS+= ${_ICC_CPUCFLAGS:C/(-x[^M^K^W]+)[MKW]+|-x[MKW]+/\1/}
. else
COPTFLAGS+= ${_CPUCFLAGS}
. endif
.endif
.if ${CC} == "icc"
C_DIALECT=
NOSTDINC= -X
.else
C_DIALECT= -std=c99
NOSTDINC= -nostdinc
.endif

INCLUDES= ${NOSTDINC} -I- ${INCLMAGIC} -I. -I$S

# This hack lets us use the OpenBSD altq code without spamming a new
# include path into contrib'ed source files.
INCLUDES+= -I$S/contrib/altq

.if make(depend) || make(kernel-depend)

# ... and the same for ipfilter
INCLUDES+= -I$S/contrib/ipfilter

# ... and the same for pf
INCLUDES+= -I$S/contrib/pf

# ... and the same for Atheros HAL
INCLUDES+= -I$S/dev/ath

# ... and the same for the NgATM stuff
INCLUDES+= -I$S/contrib/ngatm

# .. and the same for twa
INCLUDES+= -I$S/dev/twa

# .. and the same for em
INCLUDES+= -I$S/dev/em

# ...  and XFS
INCLUDES+= -I$S/gnu/fs/xfs/FreeBSD -I$S/gnu/fs/xfs/FreeBSD/support -I$S/gnu/fs/xfs

.endif

CFLAGS=	${COPTFLAGS} ${C_DIALECT} ${DEBUG} ${CWARNFLAGS}
CFLAGS+= ${INCLUDES} -D_KERNEL -DHAVE_KERNEL_OPTION_HEADERS -include opt_global.h
.if ${CC} != "icc"
CFLAGS+= -fno-common -finline-limit=${INLINE_LIMIT}
CFLAGS+= --param inline-unit-growth=100
CFLAGS+= --param large-function-growth=1000
WERROR?= -Werror
.endif

# XXX LOCORE means "don't declare C stuff" not "for locore.s".
ASM_CFLAGS= -x assembler-with-cpp -DLOCORE ${CFLAGS}

.if defined(PROFLEVEL) && ${PROFLEVEL} >= 1
.if ${CC} == "icc"
.error "Profiling doesn't work with icc yet"
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

NORMAL_LINT=	${LINT} ${LINTFLAGS} ${CFLAGS:M-[DIU]*} ${.IMPSRC}

GEN_CFILES= $S/$M/$M/genassym.c ${MFILES:T:S/.m$/.c/}
SYSTEM_CFILES= config.c env.c hints.c vnode_if.c
SYSTEM_DEP= Makefile ${SYSTEM_OBJS}
SYSTEM_OBJS= locore.o ${MDOBJS} ${OBJS}
SYSTEM_OBJS+= ${SYSTEM_CFILES:.c=.o}
SYSTEM_OBJS+= hack.So
SYSTEM_LD= @${LD} -Bdynamic -T $S/conf/ldscript.$M \
	-warn-common -export-dynamic -dynamic-linker /red/herring \
	-o ${.TARGET} -X ${SYSTEM_OBJS} vers.o
SYSTEM_LD_TAIL= @${OBJCOPY} --strip-symbol gcc2_compiled. ${.TARGET} ; \
	${SIZE} ${.TARGET} ; chmod 755 ${.TARGET}
SYSTEM_DEP+= $S/conf/ldscript.$M

# MKMODULESENV is set here so that port makefiles can augment
# them.

MKMODULESENV=	MAKEOBJDIRPREFIX=${.OBJDIR}/modules KMODDIR=${KODIR}
.if (${KERN_IDENT} == LINT)
MKMODULESENV+=	ALL_MODULES=LINT
.endif
.if defined(MODULES_OVERRIDE)
MKMODULESENV+=	MODULES_OVERRIDE="${MODULES_OVERRIDE}"
.endif
.if defined(DEBUG)
MKMODULESENV+=	DEBUG_FLAGS="${DEBUG}"
.endif
