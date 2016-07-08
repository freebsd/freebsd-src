# $FreeBSD$

.include <src.opts.mk>

PACKAGE=lib${LIB}
LIB=	compiler_rt
NO_PIC=
WARNS?=	2

CFLAGS+=${PICFLAG} -fvisibility=hidden -DVISIBILITY_HIDDEN
CFLAGS+=-I${.CURDIR}/../../contrib/libcxxrt

.if ${MACHINE_CPUARCH} == "amd64"
CRTARCH=x86_64
.else
CRTARCH=${MACHINE_CPUARCH}
.endif

CRTSRC=${.CURDIR}/../../contrib/compiler-rt/lib/builtins

.PATH: ${CRTSRC}/${CRTARCH} ${CRTSRC}

SRCF=	absvdi2 \
	absvsi2 \
	absvti2 \
	addvdi3 \
	addvsi3 \
	addvti3 \
	apple_versioning \
	ashldi3 \
	ashlti3 \
	ashrdi3 \
	ashrti3 \
	clear_cache \
	clzdi2 \
	clzsi2 \
	clzti2 \
	cmpdi2 \
	cmpti2 \
	ctzdi2 \
	ctzsi2 \
	ctzti2 \
	divdc3 \
	divdi3 \
	divmoddi4 \
	divmodsi4 \
	divsc3 \
	divtc3 \
	divti3 \
	divxc3 \
	enable_execute_stack \
	eprintf \
	extendhfsf2 \
	ffsdi2 \
	ffsti2 \
	fixdfdi \
	fixdfti \
	fixsfdi \
	fixsfti \
	fixunsdfdi \
	fixunsdfsi \
	fixunsdfti \
	fixunssfdi \
	fixunssfsi \
	fixunssfti \
	fixunsxfdi \
	fixunsxfsi \
	fixunsxfti \
	fixxfdi \
	fixxfti \
	floatdidf \
	floatdisf \
	floatditf \
	floatdixf \
	floatsitf \
	floattidf \
	floattisf \
	floattixf \
	floatundidf \
	floatundisf \
	floatunditf \
	floatundixf \
	floatunsidf \
	floatunsisf \
	floatuntidf \
	floatuntisf \
	floatuntixf \
	gcc_personality_v0 \
	int_util \
	lshrdi3 \
	lshrti3 \
	moddi3 \
	modti3 \
	muldc3 \
	muldi3 \
	mulodi4 \
	mulosi4 \
	muloti4 \
	mulsc3 \
	multi3 \
	mulvdi3 \
	mulvsi3 \
	mulvti3 \
	multc3 \
	mulxc3 \
	negdf2 \
	negdi2 \
	negsf2 \
	negti2 \
	negvdi2 \
	negvsi2 \
	negvti2 \
	paritydi2 \
	paritysi2 \
	parityti2 \
	popcountdi2 \
	popcountsi2 \
	popcountti2 \
	powidf2 \
	powisf2 \
	powitf2 \
	powixf2 \
	subvdi3 \
	subvsi3 \
	subvti3 \
	trampoline_setup \
	truncdfhf2 \
	truncsfhf2 \
	ucmpdi2 \
	ucmpti2 \
	udivdi3 \
	udivmoddi4 \
	udivmodsi4 \
	udivmodti4 \
	udivti3 \
	umoddi3 \
	umodti3

# 128-bit quad precision long double support, only used on arm64
.if ${MACHINE_CPUARCH} == "aarch64"
SRCF+=	addtf3 \
	comparetf2 \
	divtf3 \
	extenddftf2 \
	extendsftf2 \
	fixtfdi \
	fixtfsi \
	fixtfti \
	fixunstfdi \
	fixunstfsi \
	fixunstfti \
	floatunsitf \
	multf3 \
	subtf3 \
	trunctfdf2 \
	trunctfsf2
.endif

# These are already shipped by libc.a on arm and mips
.if ${MACHINE_CPUARCH} != "arm" && ${MACHINE_CPUARCH} != "mips"
SRCF+=	adddf3 \
	addsf3 \
	divdf3 \
	divsf3 \
	extendsfdf2 \
	fixdfsi \
	fixsfsi \
	floatsidf \
	floatsisf \
	muldf3 \
	mulsf3 \
	subdf3 \
	subsf3 \
	truncdfsf2
.endif

.if ${MACHINE_CPUARCH} != "arm"
SRCF+=	comparedf2 \
	comparesf2
.endif

.if ${MACHINE_CPUARCH} != "mips"
SRCF+=	divsi3 \
	modsi3 \
	udivsi3 \
	umodsi3
.endif

# FreeBSD-specific atomic intrinsics.
.if ${MACHINE_CPUARCH} == "arm" || ${MACHINE_CPUARCH} == "armv6"
.PATH: ${.CURDIR}/../../sys/arm/arm

SRCF+=	stdatomic
CFLAGS+=	-DEMIT_SYNC_ATOMICS
.elif ${MACHINE_CPUARCH} == "mips"
.PATH: ${.CURDIR}/../../sys/mips/mips

SRCF+=	stdatomic
.endif

.for file in ${SRCF}
.if ${MACHINE_ARCH:Marmv6*} && (!defined(CPUTYPE) || ${CPUTYPE:M*soft*} == "") && \
    exists(${CRTSRC}/${CRTARCH}/${file}vfp.S)
SRCS+= ${file}vfp.S
. elif exists(${CRTSRC}/${CRTARCH}/${file}.S)
SRCS+=	${file}.S
. else
SRCS+=	${file}.c
. endif
.endfor

.if ${MACHINE_CPUARCH} == "arm"
SRCS+=	aeabi_div0.c \
	aeabi_idivmod.S \
	aeabi_ldivmod.S \
	aeabi_memcmp.S \
	aeabi_memcpy.S \
	aeabi_memmove.S \
	aeabi_memset.S \
	aeabi_uidivmod.S \
	aeabi_uldivmod.S \
	bswapdi2.S \
	bswapsi2.S \
	switch16.S \
	switch32.S \
	switch8.S \
	switchu8.S \
	sync_synchronize.S
.endif

.if ${MK_INSTALLLIB} != "no"
SYMLINKS+=libcompiler_rt.a ${LIBDIR}/libgcc.a
.endif
.if ${MK_PROFILE} != "no"
SYMLINKS+=libcompiler_rt_p.a ${LIBDIR}/libgcc_p.a
.endif

.if ${MACHINE_CPUARCH} == "amd64" || ${MACHINE_CPUARCH} == "i386" || \
    ${MACHINE_CPUARCH} == "powerpc" || ${MACHINE_ARCH:Marmv6*}
AFLAGS+=--noexecstack
ACFLAGS+=-Wa,--noexecstack
.endif


.include <bsd.lib.mk>
