# kern.pre.mk
#
# Unified Makefile for building kenrels.  This includes all the definitions
# that need to be included before %BEFORE_DEPEND
#
# $FreeBSD$
#

# Can be overridden by makeoptions or /etc/make.conf
KERNEL_KO?=	kernel
KERNEL?=	kernel
KODIR?=		/boot/${KERNEL}

M=	${MACHINE_ARCH}

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

# This hack is to allow kernel compiles to succeed on machines w/out srcdist
.if exists($S/../include)
INCLUDES+= -I$S/../include
.else
INCLUDES+= -I/usr/include
.endif

COPTS=	${INCLUDES} ${IDENT} -D_KERNEL -ffreestanding -include opt_global.h
CFLAGS=	${COPTFLAGS} ${CWARNFLAGS} ${DEBUG} ${COPTS} -fno-common

# XXX LOCORE means "don't declare C stuff" not "for locore.s".
ASM_CFLAGS= -x assembler-with-cpp -DLOCORE ${CFLAGS}

# Select the correct set of tools. Can't set OBJFORMAT here because it
# doesn't get exported into the environment, and if it were exported
# then it might break building of utilities.
FMT?=		-elf
CFLAGS+=	${FMT}

DEFINED_PROF=	${PROF}
.if defined(PROF)
CFLAGS+=	-malign-functions=4
.if ${PROFLEVEL} >= 2
IDENT+=	-DGPROF4 -DGUPROF
PROF+=	-mprofiler-epilogue
.endif
.endif

# Put configuration-specific C flags last (except for ${PROF}) so that they
# can override the others.
CFLAGS+=	${CONF_CFLAGS}

NORMAL_C= ${CC} -c ${CFLAGS} ${PROF} ${.IMPSRC}
NORMAL_C_C= ${CC} -c ${CFLAGS} ${PROF} ${.IMPSRC}
NORMAL_S= ${CC} -c ${ASM_CFLAGS} ${.IMPSRC}
PROFILE_C= ${CC} -c ${CFLAGS} ${.IMPSRC}

NORMAL_M= perl5 $S/kern/makeobjops.pl -c $<; \
	  ${CC} -c ${CFLAGS} ${PROF} ${.PREFIX}.c

GEN_CFILES= $S/$M/$M/genassym.c
SYSTEM_CFILES= vnode_if.c hints.c env.c config.c
SYSTEM_SFILES= $S/$M/$M/locore.s
SYSTEM_DEP= Makefile ${SYSTEM_OBJS}
SYSTEM_OBJS= locore.o vnode_if.o ${OBJS} hints.o env.o config.o hack.So
SYSTEM_LD= @${LD} ${FMT} -Bdynamic -T $S/conf/ldscript.$M \
	-warn-common -export-dynamic -dynamic-linker /red/herring \
	-o ${.TARGET} -X ${SYSTEM_OBJS} vers.o
SYSTEM_LD_TAIL= @${OBJCOPY} --strip-symbol gcc2_compiled. ${.TARGET} ; \
	${SIZE} ${FMT} ${.TARGET} ; chmod 755 ${.TARGET}
SYSTEM_DEP+= $S/conf/ldscript.$M

# MKMODULESENV is set here so that port makefiles can augment
# them.

MKMODULESENV=	MAKEOBJDIRPREFIX=${.OBJDIR}/modules KMODDIR=${KODIR}
.if defined(MODULES_OVERRIDE)
MKMODULESENV+=	MODULES_OVERRIDE="${MODULES_OVERRIDE}"
.endif
.if defined(DEBUG)
MKMODULESENV+=	DEBUG="${DEBUG}" DEBUG_FLAGS="${DEBUG}"
.endif

all:	${KERNEL_KO}
