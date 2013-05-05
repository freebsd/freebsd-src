# $FreeBSD$

.include <bsd.own.mk>

LIB=	compiler_rt
NO_PIC=
WARNS?=	2

CFLAGS+=${PICFLAG} -fvisibility=hidden -DVISIBILITY_HIDDEN

.if ${MACHINE_CPUARCH} == "amd64"
CRTARCH=x86_64
.else
CRTARCH=${MACHINE_CPUARCH}
.endif

CRTSRC=${.CURDIR}/../../contrib/compiler-rt/lib

.PATH: ${CRTSRC}/${CRTARCH} ${CRTSRC}

SRCF=	absvdi2 \
	absvsi2 \
	absvti2 \
	addvdi3 \
	addvsi3 \
	addvti3 \
	ashldi3 \
	ashlti3 \
	ashrdi3 \
	ashrti3 \
	clzdi2 \
	clzsi2 \
	clzti2 \
	cmpdi2 \
	cmpti2 \
	comparedf2 \
	comparesf2 \
	ctzdi2 \
	ctzsi2 \
	ctzti2 \
	divdc3 \
	divdi3 \
	divmoddi4 \
	divmodsi4 \
	divsc3 \
	divti3 \
	divxc3 \
	enable_execute_stack \
	eprintf \
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
	floatdixf \
	floattidf \
	floattisf \
	floattixf \
	floatundidf \
	floatundisf \
	floatundixf \
	floatunsidf \
	floatunsisf \
	floatuntidf \
	floatuntisf \
	floatuntixf \
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
	ucmpdi2 \
	ucmpti2 \
	udivdi3 \
	udivmoddi4 \
	udivmodsi4 \
	udivmodti4 \
	udivti3 \
	umoddi3 \
	umodti3

# Don't build clear_cache on ARM with clang as it is a builtin there.
.if ${MACHINE_CPUARCH} != "arm" || ${COMPILER_TYPE} != "clang"
SRCF+=	clear_cache
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

.if ${MACHINE_CPUARCH} != "mips" && \
    (${MACHINE_CPUARCH} != "arm" || ${MK_ARM_EABI} != "no")
SRCF+=	divsi3 \
	modsi3 \
	udivsi3 \
	umodsi3
.endif

# FreeBSD-specific atomic intrinsics.
.if ${MACHINE_CPUARCH} == "arm" || ${MACHINE_CPUARCH} == "mips"
SRCF+=	__sync_fetch_and_add_4 \
	__sync_fetch_and_and_4 \
	__sync_fetch_and_or_4 \
	__sync_fetch_and_sub_4 \
	__sync_fetch_and_xor_4 \
	__sync_lock_test_and_set_4 \
	__sync_val_compare_and_swap_4
.endif
.if ${MACHINE_ARCH:Mmips64*} != ""
SRCF+=	__sync_fetch_and_add_8 \
	__sync_fetch_and_and_8 \
	__sync_fetch_and_or_8 \
	__sync_fetch_and_sub_8 \
	__sync_fetch_and_xor_8 \
	__sync_lock_test_and_set_8 \
	__sync_val_compare_and_swap_8
.endif

.for file in ${SRCF}
. if ${MACHINE_CPUARCH} != "arm" && exists(${CRTSRC}/${CRTARCH}/${file}.S)
SRCS+=	${file}.S
. else
SRCS+=	${file}.c
. endif
.endfor

.if ${MACHINE_CPUARCH} == "arm" && ${MK_ARM_EABI} != "no"
SRCS+=	aeabi_idivmod.S \
	aeabi_ldivmod.S \
	aeabi_memcmp.S \
	aeabi_memcpy.S \
	aeabi_memmove.S \
	aeabi_memset.S \
	aeabi_uidivmod.S \
	aeabi_uldivmod.S
.endif

.if ${MK_INSTALLLIB} != "no"
SYMLINKS+=libcompiler_rt.a ${LIBDIR}/libgcc.a
.endif
.if ${MK_PROFILE} != "no"
SYMLINKS+=libcompiler_rt_p.a ${LIBDIR}/libgcc_p.a
.endif

.if ${MACHINE_CPUARCH} == "amd64" || ${MACHINE_CPUARCH} == "i386" || \
    ${MACHINE_CPUARCH} == "powerpc"
AFLAGS+=--noexecstack
ACFLAGS+=-Wa,--noexecstack
.endif


.include <bsd.lib.mk>
