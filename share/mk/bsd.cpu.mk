# $FreeBSD$

# Set default CPU compile flags and baseline CPUTYPE for each arch.  The
# compile flags must support the minimum CPU type for each architecture but
# may tune support for more advanced processors.

.if !defined(CPUTYPE) || empty(CPUTYPE)
_CPUCFLAGS =
. if ${MACHINE_CPUARCH} == "aarch64"
MACHINE_CPU = arm64
. elif ${MACHINE_CPUARCH} == "amd64"
MACHINE_CPU = amd64 sse2 sse mmx
. elif ${MACHINE_CPUARCH} == "arm"
MACHINE_CPU = arm
. elif ${MACHINE_CPUARCH} == "i386"
MACHINE_CPU = i486
. elif ${MACHINE_ARCH} == "powerpc"
MACHINE_CPU = aim
. elif ${MACHINE_ARCH} == "powerpc64"
MACHINE_CPU = aim altivec
. elif ${MACHINE_ARCH} == "powerpc64le"
MACHINE_CPU = aim altivec vsx vsx2
. elif ${MACHINE_CPUARCH} == "riscv"
MACHINE_CPU = riscv
. endif
.else

# Handle aliases (not documented in make.conf to avoid user confusion
# between e.g. i586 and pentium)

. if ${MACHINE_CPUARCH} == "amd64" || ${MACHINE_CPUARCH} == "i386"
.  if ${CPUTYPE} == "barcelona"
CPUTYPE = amdfam10
.  elif ${CPUTYPE} == "skx"
CPUTYPE = skylake-avx512
.  elif ${CPUTYPE} == "core-avx2"
CPUTYPE = haswell
.  elif ${CPUTYPE} == "core-avx-i"
CPUTYPE = ivybridge
.  elif ${CPUTYPE} == "corei7-avx"
CPUTYPE = sandybridge
.  elif ${CPUTYPE} == "corei7"
CPUTYPE = nehalem
.  elif ${CPUTYPE} == "slm"
CPUTYPE = silvermont
.  elif ${CPUTYPE} == "atom"
CPUTYPE = bonnell
.  elif ${CPUTYPE} == "core"
CPUTYPE = prescott
.  endif
.  if ${MACHINE_CPUARCH} == "amd64"
.   if ${CPUTYPE} == "prescott"
CPUTYPE = nocona
.   endif
.  else
.   if ${CPUTYPE} == "k7"
CPUTYPE = athlon
.   elif ${CPUTYPE} == "p4"
CPUTYPE = pentium4
.   elif ${CPUTYPE} == "p4m"
CPUTYPE = pentium4m
.   elif ${CPUTYPE} == "p3"
CPUTYPE = pentium3
.   elif ${CPUTYPE} == "p3m"
CPUTYPE = pentium3m
.   elif ${CPUTYPE} == "p-m"
CPUTYPE = pentium-m
.   elif ${CPUTYPE} == "p2"
CPUTYPE = pentium2
.   elif ${CPUTYPE} == "i686"
CPUTYPE = pentiumpro
.   elif ${CPUTYPE} == "i586/mmx"
CPUTYPE = pentium-mmx
.   elif ${CPUTYPE} == "i586"
CPUTYPE = pentium
.   endif
.  endif
. endif

###############################################################################
# Logic to set up correct gcc optimization flag.  This must be included
# after /etc/make.conf so it can react to the local value of CPUTYPE
# defined therein.  Consult:
#	http://gcc.gnu.org/onlinedocs/gcc/ARM-Options.html
#	http://gcc.gnu.org/onlinedocs/gcc/RS-6000-and-PowerPC-Options.html
#	http://gcc.gnu.org/onlinedocs/gcc/SPARC-Options.html
#	http://gcc.gnu.org/onlinedocs/gcc/i386-and-x86_002d64-Options.html

. if ${MACHINE_CPUARCH} == "i386"
.  if ${CPUTYPE} == "crusoe"
_CPUCFLAGS = -march=i686 -falign-functions=0 -falign-jumps=0 -falign-loops=0
.  elif ${CPUTYPE} == "k5"
_CPUCFLAGS = -march=pentium
.  elif ${CPUTYPE} == "c7"
_CPUCFLAGS = -march=c3-2
.  else
_CPUCFLAGS = -march=${CPUTYPE}
.  endif
. elif ${MACHINE_CPUARCH} == "amd64"
_CPUCFLAGS = -march=${CPUTYPE}
. elif ${MACHINE_CPUARCH} == "arm"
.  if ${CPUTYPE} == "xscale"
#XXX: gcc doesn't seem to like -mcpu=xscale, and dies while rebuilding itself
#_CPUCFLAGS = -mcpu=xscale
_CPUCFLAGS = -march=armv5te -D__XSCALE__
.  elif ${CPUTYPE:M*soft*} != ""
_CPUCFLAGS = -mfloat-abi=softfp
.  elif ${CPUTYPE} == "cortexa"
_CPUCFLAGS = -march=armv7 -mfpu=vfp
.  elif ${CPUTYPE:Marmv[67]*} != ""
# Handle all the armvX types that FreeBSD runs:
#	armv6, armv6t2, armv7, armv7-a, armv7ve
# they require -march=. All the others require -mcpu=.
_CPUCFLAGS = -march=${CPUTYPE}
.  else
# Common values for FreeBSD
# arm: (any arm v4 or v5 processor you are targeting)
#	arm920t, arm926ej-s, marvell-pj4, fa526, fa626,
#	fa606te, fa626te, fa726te
# armv6:
# 	arm1176jzf-s
# armv7: generic-armv7-a, cortex-a5, cortex-a7, cortex-a8, cortex-a9,
#       cortex-a12, cortex-a15, cortex-a17
#       cortex-a53, cortex-a57, cortex-a72,
#       exynos-m1
_CPUCFLAGS = -mcpu=${CPUTYPE}
. endif
. elif ${MACHINE_ARCH} == "powerpc"
.  if ${CPUTYPE} == "e500"
_CPUCFLAGS = -Wa,-me500 -msoft-float
.  else
_CPUCFLAGS = -mcpu=${CPUTYPE} -mno-powerpc64
.  endif
. elif ${MACHINE_ARCH:Mpowerpc64*} != ""
_CPUCFLAGS = -mcpu=${CPUTYPE}
. elif ${MACHINE_CPUARCH} == "aarch64"
.  if ${CPUTYPE:Marmv*} != ""
# Use -march when the CPU type is an architecture value, e.g. armv8.1-a
_CPUCFLAGS = -march=${CPUTYPE}
.  else
# Otherwise assume we have a CPU type
_CPUCFLAGS = -mcpu=${CPUTYPE}
.  endif
. endif

# Set up the list of CPU features based on the CPU type.  This is an
# unordered list to make it easy for client makefiles to test for the
# presence of a CPU feature.

########## i386
. if ${MACHINE_CPUARCH} == "i386"
.  if ${CPUTYPE} == "znver3" || ${CPUTYPE} == "znver2" || \
    ${CPUTYPE} == "znver1"
MACHINE_CPU = avx2 avx sse42 sse41 ssse3 sse4a sse3 sse2 sse mmx k6 k5 i586
.  elif ${CPUTYPE} == "bdver4"
MACHINE_CPU = xop avx2 avx sse42 sse41 ssse3 sse4a sse3 sse2 sse mmx k6 k5 i586
.  elif ${CPUTYPE} == "bdver3" || ${CPUTYPE} == "bdver2" || \
    ${CPUTYPE} == "bdver1"
MACHINE_CPU = xop avx sse42 sse41 ssse3 sse4a sse3 sse2 sse mmx k6 k5 i586
.  elif ${CPUTYPE} == "btver2"
MACHINE_CPU = avx sse42 sse41 ssse3 sse4a sse3 sse2 sse mmx k6 k5 i586
.  elif ${CPUTYPE} == "btver1"
MACHINE_CPU = ssse3 sse4a sse3 sse2 sse mmx k6 k5 i586
.  elif ${CPUTYPE} == "amdfam10"
MACHINE_CPU = athlon-xp athlon k7 3dnow sse4a sse3 sse2 sse mmx k6 k5 i586
.  elif ${CPUTYPE} == "opteron-sse3" || ${CPUTYPE} == "athlon64-sse3"
MACHINE_CPU = athlon-xp athlon k7 3dnow sse3 sse2 sse mmx k6 k5 i586
.  elif ${CPUTYPE} == "opteron" || ${CPUTYPE} == "athlon64" || \
    ${CPUTYPE} == "athlon-fx"
MACHINE_CPU = athlon-xp athlon k7 3dnow sse2 sse mmx k6 k5 i586
.  elif ${CPUTYPE} == "athlon-mp" || ${CPUTYPE} == "athlon-xp" || \
    ${CPUTYPE} == "athlon-4"
MACHINE_CPU = athlon-xp athlon k7 3dnow sse mmx k6 k5 i586
.  elif ${CPUTYPE} == "athlon" || ${CPUTYPE} == "athlon-tbird"
MACHINE_CPU = athlon k7 3dnow mmx k6 k5 i586
.  elif ${CPUTYPE} == "k6-3" || ${CPUTYPE} == "k6-2" || ${CPUTYPE} == "geode"
MACHINE_CPU = 3dnow mmx k6 k5 i586
.  elif ${CPUTYPE} == "k6"
MACHINE_CPU = mmx k6 k5 i586
.  elif ${CPUTYPE} == "k5"
MACHINE_CPU = k5 i586
.  elif ${CPUTYPE} == "sapphirerapids" || ${CPUTYPE} == "tigerlake" || \
    ${CPUTYPE} == "cooperlake" || ${CPUTYPE} == "cascadelake" || \
    ${CPUTYPE} == "icelake-server" || ${CPUTYPE} == "icelake-client" || \
    ${CPUTYPE} == "cannonlake" || ${CPUTYPE} == "knm" || \
    ${CPUTYPE} == "skylake-avx512" || ${CPUTYPE} == "knl" || \
    ${CPUTYPE} == "x86-64-v4"
MACHINE_CPU = avx512 avx2 avx sse42 sse41 ssse3 sse3 sse2 sse i686 mmx i586
.  elif ${CPUTYPE} == "alderlake" || ${CPUTYPE} == "skylake" || \
    ${CPUTYPE} == "broadwell" || ${CPUTYPE} == "haswell" || \
    ${CPUTYPE} == "x86-64-v3"
MACHINE_CPU = avx2 avx sse42 sse41 ssse3 sse3 sse2 sse i686 mmx i586
.  elif ${CPUTYPE} == "ivybridge" || ${CPUTYPE} == "sandybridge"
MACHINE_CPU = avx sse42 sse41 ssse3 sse3 sse2 sse i686 mmx i586
.  elif ${CPUTYPE} == "tremont" || ${CPUTYPE} == "goldmont-plus" || \
    ${CPUTYPE} == "goldmont" || ${CPUTYPE} == "westmere" || \
    ${CPUTYPE} == "nehalem" || ${CPUTYPE} == "silvermont" || \
    ${CPUTYPE} == "x86-64-v2"
MACHINE_CPU = sse42 sse41 ssse3 sse3 sse2 sse i686 mmx i586
.  elif ${CPUTYPE} == "penryn"
MACHINE_CPU = sse41 ssse3 sse3 sse2 sse i686 mmx i586
.  elif ${CPUTYPE} == "core2" || ${CPUTYPE} == "bonnell"
MACHINE_CPU = ssse3 sse3 sse2 sse i686 mmx i586
.  elif ${CPUTYPE} == "yonah" || ${CPUTYPE} == "prescott"
MACHINE_CPU = sse3 sse2 sse i686 mmx i586
.  elif ${CPUTYPE} == "pentium4" || ${CPUTYPE} == "pentium4m" || \
    ${CPUTYPE} == "pentium-m" || ${CPUTYPE} == "x86-64"
MACHINE_CPU = sse2 sse i686 mmx i586
.  elif ${CPUTYPE} == "pentium3" || ${CPUTYPE} == "pentium3m"
MACHINE_CPU = sse i686 mmx i586
.  elif ${CPUTYPE} == "pentium2"
MACHINE_CPU = i686 mmx i586
.  elif ${CPUTYPE} == "pentiumpro"
MACHINE_CPU = i686 i586
.  elif ${CPUTYPE} == "pentium-mmx"
MACHINE_CPU = mmx i586
.  elif ${CPUTYPE} == "pentium"
MACHINE_CPU = i586
.  elif ${CPUTYPE} == "c7"
MACHINE_CPU = sse3 sse2 sse i686 mmx i586
.  elif ${CPUTYPE} == "c3-2"
MACHINE_CPU = sse i686 mmx i586
.  elif ${CPUTYPE} == "c3"
MACHINE_CPU = 3dnow mmx i586
.  elif ${CPUTYPE} == "winchip2"
MACHINE_CPU = 3dnow mmx
.  elif ${CPUTYPE} == "winchip-c6"
MACHINE_CPU = mmx
.  endif
MACHINE_CPU += i486
########## amd64
. elif ${MACHINE_CPUARCH} == "amd64"
.  if ${CPUTYPE} == "znver3" || ${CPUTYPE} == "znver2" || \
    ${CPUTYPE} == "znver1"
MACHINE_CPU = avx2 avx sse42 sse41 ssse3 sse4a sse3
.  elif ${CPUTYPE} == "bdver4"
MACHINE_CPU = xop avx2 avx sse42 sse41 ssse3 sse4a sse3
.  elif ${CPUTYPE} == "bdver3" || ${CPUTYPE} == "bdver2" || \
    ${CPUTYPE} == "bdver1"
MACHINE_CPU = xop avx sse42 sse41 ssse3 sse4a sse3
.  elif ${CPUTYPE} == "btver2"
MACHINE_CPU = avx sse42 sse41 ssse3 sse4a sse3
.  elif ${CPUTYPE} == "btver1"
MACHINE_CPU = ssse3 sse4a sse3
.  elif ${CPUTYPE} == "amdfam10"
MACHINE_CPU = k8 3dnow sse4a sse3
.  elif ${CPUTYPE} == "opteron-sse3" || ${CPUTYPE} == "athlon64-sse3" || \
    ${CPUTYPE} == "k8-sse3"
MACHINE_CPU = k8 3dnow sse3
.  elif ${CPUTYPE} == "opteron" || ${CPUTYPE} == "athlon64" || \
    ${CPUTYPE} == "athlon-fx" || ${CPUTYPE} == "k8"
MACHINE_CPU = k8 3dnow
.  elif ${CPUTYPE} == "sapphirerapids" || ${CPUTYPE} == "tigerlake" || \
    ${CPUTYPE} == "cooperlake" || ${CPUTYPE} == "cascadelake" || \
    ${CPUTYPE} == "icelake-server" || ${CPUTYPE} == "icelake-client" || \
    ${CPUTYPE} == "cannonlake" || ${CPUTYPE} == "knm" || \
    ${CPUTYPE} == "skylake-avx512" || ${CPUTYPE} == "knl" || \
    ${CPUTYPE} == "x86-64-v4"
MACHINE_CPU = avx512 avx2 avx sse42 sse41 ssse3 sse3
.  elif ${CPUTYPE} == "alderlake" || ${CPUTYPE} == "skylake" || \
    ${CPUTYPE} == "broadwell" || ${CPUTYPE} == "haswell" || \
    ${CPUTYPE} == "x86-64-v3"
MACHINE_CPU = avx2 avx sse42 sse41 ssse3 sse3
.  elif ${CPUTYPE} == "ivybridge" || ${CPUTYPE} == "sandybridge"
MACHINE_CPU = avx sse42 sse41 ssse3 sse3
.  elif ${CPUTYPE} == "tremont" || ${CPUTYPE} == "goldmont-plus" || \
    ${CPUTYPE} == "goldmont" || ${CPUTYPE} == "westmere" || \
    ${CPUTYPE} == "nehalem" || ${CPUTYPE} == "silvermont" || \
    ${CPUTYPE} == "x86-64-v2"
MACHINE_CPU = sse42 sse41 ssse3 sse3
.  elif ${CPUTYPE} == "penryn"
MACHINE_CPU = sse41 ssse3 sse3
.  elif ${CPUTYPE} == "core2" || ${CPUTYPE} == "bonnell"
MACHINE_CPU = ssse3 sse3
.  elif ${CPUTYPE} == "nocona"
MACHINE_CPU = sse3
.  endif
MACHINE_CPU += amd64 sse2 sse mmx
########## powerpc
. elif ${MACHINE_ARCH} == "powerpc"
.  if ${CPUTYPE} == "e500"
MACHINE_CPU = booke softfp
.  elif ${CPUTYPE} == "g4"
MACHINE_CPU = aim altivec
.  else
MACHINE_CPU= aim
.  endif
. elif ${MACHINE_ARCH} == "powerpc64"
.  if ${CPUTYPE} == "e5500"
MACHINE_CPU = booke
.  elif ${CPUTYPE} == power7
MACHINE_CPU = altivec vsx
.  elif ${CPUTYPE} == power8
MACHINE_CPU = altivec vsx vsx2
.  elif ${CPUTYPE} == power9
MACHINE_CPU = altivec vsx vsx2 vsx3
.  else
MACHINE_CPU = aim altivec
.  endif
. elif ${MACHINE_ARCH} == "powerpc64le"
MACHINE_CPU = aim altivec vsx vsx2
.  if ${CPUTYPE} == power9
MACHINE_CPU += vsx3
.  endif
########## riscv
. elif ${MACHINE_CPUARCH} == "riscv"
MACHINE_CPU = riscv
. endif
.endif

########## arm
.if ${MACHINE_CPUARCH} == "arm"
MACHINE_CPU += arm
. if ${MACHINE_ARCH:Marmv6*} != ""
MACHINE_CPU += armv6
. endif
. if ${MACHINE_ARCH:Marmv7*} != ""
MACHINE_CPU += armv7
. endif
# Normally armv6 and armv7 are hard float ABI from FreeBSD 11 onwards. However
# when CPUTYPE has 'soft' in it, we use the soft-float ABI to allow building of
# soft-float ABI libraries. In this case, we have to add the -mfloat-abi=softfp
# to force that.
. if defined(CPUTYPE) && ${CPUTYPE:M*soft*} != ""
# Needs to be CFLAGS not _CPUCFLAGS because it's needed for the ABI
# not a nice optimization. Please note: softfp ABI uses hardware floating
# instructions, but passes arguments to function calls in integer regsiters.
# -mfloat-abi=soft is full software floating point, but is not currently
# supported. softfp support in FreeBSD may disappear in FreeBSD 13.0 since
# it was a transition tool from FreeBSD 10 to 11 and is a bit of an odd duck.
CFLAGS += -mfloat-abi=softfp
. endif
.endif

.if ${MACHINE_ARCH} == "powerpc" || ${MACHINE_ARCH} == "powerpcspe"
LDFLAGS.bfd+= -Wl,--secure-plt
.endif

.if ${MACHINE_ARCH} == "powerpcspe"
CFLAGS += -mcpu=8548 -mspe
CFLAGS.gcc+= -mabi=spe -mfloat-gprs=double -Wa,-me500
.endif

.if ${MACHINE_CPUARCH} == "riscv"
CFLAGS += -march=rv64imafdc -mabi=lp64d
.endif

# NB: COPTFLAGS is handled in /usr/src/sys/conf/kern.pre.mk

.if !defined(NO_CPU_CFLAGS)
CFLAGS += ${_CPUCFLAGS}
.endif

#
# Prohibit the compiler from emitting SIMD instructions.
# These flags are added to CFLAGS in areas where the extra context-switch
# cost outweighs the advantages of SIMD instructions.
#
# gcc:
# Setting -mno-mmx implies -mno-3dnow
# Setting -mno-sse implies -mno-sse2, -mno-sse3, -mno-ssse3 and -mfpmath=387
#
# clang:
# Setting -mno-mmx implies -mno-3dnow and -mno-3dnowa
# Setting -mno-sse implies -mno-sse2, -mno-sse3, -mno-ssse3, -mno-sse41 and
# -mno-sse42
# (-mfpmath= is not supported)
#
.if ${MACHINE_CPUARCH} == "i386" || ${MACHINE_CPUARCH} == "amd64"
CFLAGS_NO_SIMD.clang= -mno-avx -mno-avx2
CFLAGS_NO_SIMD= -mno-mmx -mno-sse
.endif
CFLAGS_NO_SIMD += ${CFLAGS_NO_SIMD.${COMPILER_TYPE}}

# Add in any architecture-specific CFLAGS.
# These come from make.conf or the command line or the environment.
CFLAGS += ${CFLAGS.${MACHINE_ARCH}}
CXXFLAGS += ${CXXFLAGS.${MACHINE_ARCH}}

#
# MACHINE_ABI is a list of properties about the ABI used for MACHINE_ARCH.
# The following properties are indicated with one of the follow values:
#
# Byte order:			big-endian, little-endian
# Floating point ABI:		soft-float, hard-float
# Size of long (size_t, etc):	long32, long64
# Pointer type:			ptr32, ptr64
# Size of time_t:		time32, time64
#
.if (${MACHINE} == "arm" && (defined(CPUTYPE) && ${CPUTYPE:M*soft*})) || \
    (${MACHINE_ARCH} == "powerpc" && (defined(CPUTYPE) && ${CPUTYPE} == "e500"))
MACHINE_ABI+=	soft-float
.else
MACHINE_ABI+=	hard-float
.endif
# Currently all 64-bit architectures include 64 in their name (see arch(7)).
.if ${MACHINE_ARCH:M*64*}
MACHINE_ABI+=  long64
.else
MACHINE_ABI+=  long32
.endif
.if ${MACHINE_ABI:Mlong64}
MACHINE_ABI+=  ptr64
.else
MACHINE_ABI+=  ptr32
.endif
.if ${MACHINE_ARCH} == "i386"
MACHINE_ABI+=  time32
.else
MACHINE_ABI+=  time64
.endif
.if ${MACHINE_ARCH:Mpowerpc*} && !${MACHINE_ARCH:M*le}
MACHINE_ABI+=	big-endian
.else
MACHINE_ABI+=	little-endian
.endif
