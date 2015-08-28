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
. elif ${MACHINE_CPUARCH} == "mips"
MACHINE_CPU = mips
. elif ${MACHINE_CPUARCH} == "powerpc"
MACHINE_CPU = aim
. elif ${MACHINE_CPUARCH} == "sparc64"
MACHINE_CPU = ultrasparc
. endif
.else

# Handle aliases (not documented in make.conf to avoid user confusion
# between e.g. i586 and pentium)

. if ${MACHINE_CPUARCH} == "amd64" || ${MACHINE_CPUARCH} == "i386"
.  if ${CPUTYPE} == "barcelona"
CPUTYPE = amdfam10
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
. elif ${MACHINE_ARCH} == "sparc64"
.  if ${CPUTYPE} == "us"
CPUTYPE = ultrasparc
.  elif ${CPUTYPE} == "us3"
CPUTYPE = ultrasparc3
.  endif
. endif

###############################################################################
# Logic to set up correct gcc optimization flag.  This must be included
# after /etc/make.conf so it can react to the local value of CPUTYPE
# defined therein.  Consult:
#	http://gcc.gnu.org/onlinedocs/gcc/ARM-Options.html
#	http://gcc.gnu.org/onlinedocs/gcc/IA_002d64-Options.html
#	http://gcc.gnu.org/onlinedocs/gcc/RS_002f6000-and-PowerPC-Options.html
#	http://gcc.gnu.org/onlinedocs/gcc/MIPS-Options.html
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
. elif ${CPUTYPE} == "armv6"
_CPUCFLAGS = -march=${CPUTYPE} -DARM_ARCH_6=1
. elif ${CPUTYPE} == "cortexa"
_CPUCFLAGS = -march=armv7 -DARM_ARCH_6=1 -mfpu=vfp
.  else
_CPUCFLAGS = -mcpu=${CPUTYPE}
.  endif
. elif ${MACHINE_ARCH} == "powerpc"
.  if ${CPUTYPE} == "e500"
_CPUCFLAGS = -Wa,-me500 -msoft-float
.  else
_CPUCFLAGS = -mcpu=${CPUTYPE} -mno-powerpc64
.  endif
. elif ${MACHINE_ARCH} == "powerpc64"
_CPUCFLAGS = -mcpu=${CPUTYPE}
. elif ${MACHINE_CPUARCH} == "mips"
.  if ${CPUTYPE} == "mips32"
_CPUCFLAGS = -march=mips32
.  elif ${CPUTYPE} == "mips32r2"
_CPUCFLAGS = -march=mips32r2
.  elif ${CPUTYPE} == "mips64"
_CPUCFLAGS = -march=mips64
.  elif ${CPUTYPE} == "mips64r2"
_CPUCFLAGS = -march=mips64r2
.  elif ${CPUTYPE} == "mips4kc"
_CPUCFLAGS = -march=4kc
.  elif ${CPUTYPE} == "mips24kc"
_CPUCFLAGS = -march=24kc
.  endif
. elif ${MACHINE_ARCH} == "sparc64"
.  if ${CPUTYPE} == "v9"
_CPUCFLAGS = -mcpu=v9
.  elif ${CPUTYPE} == "ultrasparc"
_CPUCFLAGS = -mcpu=ultrasparc
.  elif ${CPUTYPE} == "ultrasparc3"
_CPUCFLAGS = -mcpu=ultrasparc3
.  endif
. endif

# Set up the list of CPU features based on the CPU type.  This is an
# unordered list to make it easy for client makefiles to test for the
# presence of a CPU feature.

. if ${MACHINE_CPUARCH} == "i386"
.  if ${CPUTYPE} == "bdver4"
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
.  elif ${CPUTYPE} == "skylake" || ${CPUTYPE} == "knl"
MACHINE_CPU = avx512 avx2 avx sse42 sse41 ssse3 sse3 sse2 sse i686 mmx i586
.  elif ${CPUTYPE} == "broadwell" || ${CPUTYPE} == "haswell"
MACHINE_CPU = avx2 avx sse42 sse41 ssse3 sse3 sse2 sse i686 mmx i586
.  elif ${CPUTYPE} == "ivybridge" || ${CPUTYPE} == "sandybridge"
MACHINE_CPU = avx sse42 sse41 ssse3 sse3 sse2 sse i686 mmx i586
.  elif ${CPUTYPE} == "westmere" || ${CPUTYPE} == "nehalem" || \
    ${CPUTYPE} == "silvermont"
MACHINE_CPU = sse42 sse41 ssse3 sse3 sse2 sse i686 mmx i586
.  elif ${CPUTYPE} == "penryn"
MACHINE_CPU = sse41 ssse3 sse3 sse2 sse i686 mmx i586
.  elif ${CPUTYPE} == "core2" || ${CPUTYPE} == "bonnell"
MACHINE_CPU = ssse3 sse3 sse2 sse i686 mmx i586
.  elif ${CPUTYPE} == "yonah" || ${CPUTYPE} == "prescott"
MACHINE_CPU = sse3 sse2 sse i686 mmx i586
.  elif ${CPUTYPE} == "pentium4" || ${CPUTYPE} == "pentium4m" || \
    ${CPUTYPE} == "pentium-m"
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
. elif ${MACHINE_CPUARCH} == "amd64"
.  if ${CPUTYPE} == "bdver4"
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
.  elif ${CPUTYPE} == "skylake" || ${CPUTYPE} == "knl"
MACHINE_CPU = avx512 avx2 avx sse42 sse41 ssse3 sse3
.  elif ${CPUTYPE} == "broadwell" || ${CPUTYPE} == "haswell"
MACHINE_CPU = avx2 avx sse42 sse41 ssse3 sse3
.  elif ${CPUTYPE} == "ivybridge" || ${CPUTYPE} == "sandybridge"
MACHINE_CPU = avx sse42 sse41 ssse3 sse3
.  elif ${CPUTYPE} == "westmere" || ${CPUTYPE} == "nehalem" || \
    ${CPUTYPE} == "silvermont"
MACHINE_CPU = sse42 sse41 ssse3 sse3
.  elif ${CPUTYPE} == "penryn"
MACHINE_CPU = sse41 ssse3 sse3
.  elif ${CPUTYPE} == "core2" || ${CPUTYPE} == "bonnell"
MACHINE_CPU = ssse3 sse3
.  elif ${CPUTYPE} == "nocona"
MACHINE_CPU = sse3
.  endif
MACHINE_CPU += amd64 sse2 sse mmx
. elif ${MACHINE_ARCH} == "powerpc"
.  if ${CPUTYPE} == "e500"
MACHINE_CPU = booke
.  endif
. elif ${MACHINE_ARCH} == "sparc64"
.  if ${CPUTYPE} == "v9"
MACHINE_CPU = v9
.  elif ${CPUTYPE} == "ultrasparc"
MACHINE_CPU = v9 ultrasparc
.  elif ${CPUTYPE} == "ultrasparc3"
MACHINE_CPU = v9 ultrasparc ultrasparc3
.  endif
. endif
.endif

.if ${MACHINE_CPUARCH} == "mips"
CFLAGS += -G0
.endif

.if ${MACHINE_ARCH} == "armv6"
_CPUCFLAGS += -mfloat-abi=softfp
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
CFLAGS_NO_SIMD.clang= -mno-avx
CFLAGS_NO_SIMD= -mno-mmx -mno-sse
.endif
CFLAGS_NO_SIMD += ${CFLAGS_NO_SIMD.${COMPILER_TYPE}}

# Add in any architecture-specific CFLAGS.  
# These come from make.conf or the command line or the environment.
CFLAGS += ${CFLAGS.${MACHINE_ARCH}}
CXXFLAGS += ${CXXFLAGS.${MACHINE_ARCH}}
