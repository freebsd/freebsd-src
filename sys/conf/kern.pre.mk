# kern.pre.mk
#
# Unified Makefile for building kernels.  This includes all the definitions
# that need to be included before %BEFORE_DEPEND
#
# $FreeBSD$
#

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

COPTFLAGS?=-O -pipe
.if !defined(NO_CPU_COPTFLAGS)
COPTFLAGS+= ${_CPUCFLAGS}
.endif
INCLUDES= -nostdinc -I- ${INCLMAGIC} -I. -I$S -I$S/dev

# This hack lets us use the Intel ACPICA code without spamming a new 
# include path into 100+ source files.
INCLUDES+= -I$S/contrib/dev/acpica

# ... and the same for ipfilter
INCLUDES+= -I$S/contrib/ipfilter

# ... and the same for Atheros HAL
INCLUDES+= -I$S/contrib/dev/ath -I$S/contrib/dev/ath/freebsd

COPTS=	${INCLUDES} -D_KERNEL -include opt_global.h
CFLAGS=	${COPTFLAGS} ${CWARNFLAGS} ${DEBUG} ${COPTS} -fno-common

# XXX LOCORE means "don't declare C stuff" not "for locore.s".
ASM_CFLAGS= -x assembler-with-cpp -DLOCORE ${CFLAGS}

.if defined(PROFLEVEL) && ${PROFLEVEL} >= 1
CFLAGS+=	-DGPROF -falign-functions=16
.if ${PROFLEVEL} >= 2
CFLAGS+=	-DGPROF4 -DGUPROF
# XXX -Wno-inline is to break some warnings.
PROF=	-finstrument-functions -Wno-inline
.else
PROF=	-pg
.endif
.endif
DEFINED_PROF=	${PROF}
# WERROR?=	-Werror
INLINE_LIMIT?=	2500
CFLAGS+=	--param max-inline-insns-single=${INLINE_LIMIT}


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

GEN_CFILES= $S/$M/$M/genassym.c
SYSTEM_CFILES= config.c env.c hints.c majors.c vnode_if.c
SYSTEM_DEP= Makefile ${SYSTEM_OBJS}
SYSTEM_OBJS= locore.o ${MDOBJS} ${OBJS}
SYSTEM_OBJS+= ${SYSTEM_CFILES:.c=.o}
SYSTEM_OBJS+= hack.So
SYSTEM_LD= @${LD} ${FMT} -Bdynamic -T $S/conf/ldscript.$M \
	-warn-common -export-dynamic -dynamic-linker /red/herring \
	-o ${.TARGET} -X ${SYSTEM_OBJS} vers.o
SYSTEM_LD_TAIL= @${OBJCOPY} --strip-symbol gcc2_compiled. ${.TARGET} ; \
	${SIZE} ${FMT} ${.TARGET} ; chmod 755 ${.TARGET}
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
MKMODULESENV+=	DEBUG="${DEBUG}" DEBUG_FLAGS="${DEBUG}"
.endif
