# $FreeBSD$

# Set default CPU compile flags and baseline CPUTYPE for each arch.  The
# compile flags must support the minimum CPU type for each architecture but
# may tune support for more advanced processors.

.if !defined(CPUTYPE) || empty(CPUTYPE)
. if ${MACHINE_ARCH} == "i386"
_CPUCFLAGS =
MACHINE_CPU = i486
. elif ${MACHINE_ARCH} == "alpha"
_CPUCFLAGS = -mcpu=ev4 -mtune=ev5
MACHINE_CPU = ev4
. elif ${MACHINE_ARCH} == "amd64"
MACHINE_CPU = amd64 sse2 sse
. elif ${MACHINE_ARCH} == "ia64"
_CPUCFLAGS =
MACHINE_CPU = itanium
. elif ${MACHINE_ARCH} == "sparc64"
_CPUCFLAGS =
. elif ${MACHINE_ARCH} == "arm"
_CPUCFLAGS =
MACHINE_CPU = arm
. endif
.else

# Handle aliases (not documented in make.conf to avoid user confusion
# between e.g. i586 and pentium)

. if ${MACHINE_ARCH} == "i386"
.  if ${CPUTYPE} == "pentium4"
CPUTYPE = p4
.  elif ${CPUTYPE} == "pentium4m"
CPUTYPE = p4m
.  elif ${CPUTYPE} == "pentium3"
CPUTYPE = p3
.  elif ${CPUTYPE} == "pentium3m"
CPUTYPE = p3m
.  elif ${CPUTYPE} == "pentium-m"
CPUTYPE = p-m
.  elif ${CPUTYPE} == "pentiumpro"
CPUTYPE = i686
.  elif ${CPUTYPE} == "pentium"
CPUTYPE = i586
.  elif ${CPUTYPE} == "opteron"
CPUTYPE = athlon-mp
.  elif ${CPUTYPE} == "athlon64"
CPUTYPE = athlon-xp
.  elif ${CPUTYPE} == "k7"
CPUTYPE = athlon
.  endif
. endif

# Logic to set up correct gcc optimization flag.  This must be included
# after /etc/make.conf so it can react to the local value of CPUTYPE
# defined therein.  Consult:
#	http://gcc.gnu.org/onlinedocs/gcc/DEC-Alpha-Options.html
#	http://gcc.gnu.org/onlinedocs/gcc/IA-64-Options.html
#	http://gcc.gnu.org/onlinedocs/gcc/RS-6000-and-PowerPC-Options.html
#	http://gcc.gnu.org/onlinedocs/gcc/SPARC-Options.html
#	http://gcc.gnu.org/onlinedocs/gcc/i386-and-x86-64-Options.html

. if ${MACHINE_ARCH} == "i386"
.  if ${CPUTYPE} == "crusoe"
_CPUCFLAGS = -march=i686 -falign-functions=0 -falign-jumps=0 -falign-loops=0
_ICC_CPUCFLAGS = -tpp6 -xiM
.  elif ${CPUTYPE} == "athlon-mp" || ${CPUTYPE} == "athlon-xp" || \
    ${CPUTYPE} == "athlon-4"
_CPUCFLAGS = -march=${CPUTYPE}
_ICC_CPUCFLAGS = -tpp6 -xiMK
.  elif ${CPUTYPE} == "athlon-tbird" || ${CPUTYPE} == "athlon"
_CPUCFLAGS = -march=${CPUTYPE}
_ICC_CPUCFLAGS = -tpp6 -xiM
.  elif ${CPUTYPE} == "k6-3" || ${CPUTYPE} == "k6-2" || ${CPUTYPE} == "k6"
_CPUCFLAGS = -march=${CPUTYPE}
_ICC_CPUCFLAGS = -tpp6 -xi
.  elif ${CPUTYPE} == "k5"
_CPUCFLAGS = -march=pentium
_ICC_CPUCFLAGS = -tpp5
.  elif ${CPUTYPE} == "p4"
_CPUCFLAGS = -march=pentium4
_ICC_CPUCFLAGS = -tpp7 -xiMKW
.  elif ${CPUTYPE} == "p4m"
_CPUCFLAGS = -march=pentium4m
.  elif ${CPUTYPE} == "p3"
_CPUCFLAGS = -march=pentium3
_ICC_CPUCFLAGS = -tpp6 -xiMK
.  elif ${CPUTYPE} == "p3m"
_CPUCFLAGS = -march=pentium3m
.  elif ${CPUTYPE} == "p-m"
_CPUCFLAGS = -march=pentium-m
.  elif ${CPUTYPE} == "p2"
_CPUCFLAGS = -march=pentium2
_ICC_CPUCFLAGS = -tpp6 -xiM
.  elif ${CPUTYPE} == "i686"
_CPUCFLAGS = -march=pentiumpro
_ICC_CPUCFLAGS = -tpp6 -xiM
.  elif ${CPUTYPE} == "i586/mmx"
_CPUCFLAGS = -march=pentium-mmx
_ICC_CPUCFLAGS = -tpp5 -xM
.  elif ${CPUTYPE} == "i586"
_CPUCFLAGS = -march=pentium
_ICC_CPUCFLAGS = -tpp5
.  elif ${CPUTYPE} == "i486"
_CPUCFLAGS = -march=i486
_ICC_CPUCFLAGS =
.  endif
. elif ${MACHINE_ARCH} == "alpha"
.  if ${CPUTYPE} == "ev67"
_CPUCFLAGS = -mcpu=ev67
.  elif ${CPUTYPE} == "ev6"
_CPUCFLAGS = -mcpu=ev6
.  elif ${CPUTYPE} == "pca56"
_CPUCFLAGS = -mcpu=pca56
.  elif ${CPUTYPE} == "ev56"
_CPUCFLAGS = -mcpu=ev56
.  elif ${CPUTYPE} == "ev5"
_CPUCFLAGS = -mcpu=ev5
.  elif ${CPUTYPE} == "ev45"
_CPUCFLAGS = -mcpu=ev45
.  elif ${CPUTYPE} == "ev4"
_CPUCFLAGS = -mcpu=ev4
.  endif
. elif ${MACHINE_ARCH} == "arm"
.  if ${CPUTYPE} == "strongarm"
_CPUCFLAGS = -mcpu=strongarm
.  elif ${CPUTYPE} == "xscale"
#XXX: gcc doesn't seem to like -mcpu=xscale, and dies while rebuilding itself
#_CPUCFLAGS = -mcpu=xscale
_CPUCFLAGS = -D__XSCALE__
.  endif
. endif

# Set up the list of CPU features based on the CPU type.  This is an
# unordered list to make it easy for client makefiles to test for the
# presence of a CPU feature.

. if ${MACHINE_ARCH} == "i386"
.  if ${CPUTYPE} == "athlon-mp" || ${CPUTYPE} == "athlon-xp" || \
    ${CPUTYPE} == "athlon-4"
MACHINE_CPU = athlon-xp k7 3dnow sse mmx k6 k5 i586 i486 i386
.  elif ${CPUTYPE} == "athlon" || ${CPUTYPE} == "athlon-tbird"
MACHINE_CPU = athlon k7 3dnow mmx k6 k5 i586 i486 i386
.  elif ${CPUTYPE} == "k6-3" || ${CPUTYPE} == "k6-2"
MACHINE_CPU = 3dnow mmx k6 k5 i586 i486 i386
.  elif ${CPUTYPE} == "k6"
MACHINE_CPU = mmx k6 k5 i586 i486 i386
.  elif ${CPUTYPE} == "k5"
MACHINE_CPU = k5 i586 i486 i386
.  elif ${CPUTYPE} == "p4" || ${CPUTYPE} == "p4m" || ${CPUTYPE} == "p-m"
MACHINE_CPU = sse2 sse i686 mmx i586 i486 i386
.  elif ${CPUTYPE} == "p3" || ${CPUTYPE} == "p3m"
MACHINE_CPU = sse i686 mmx i586 i486 i386
.  elif ${CPUTYPE} == "p2"
MACHINE_CPU = i686 mmx i586 i486 i386
.  elif ${CPUTYPE} == "i686"
MACHINE_CPU = i686 i586 i486 i386
.  elif ${CPUTYPE} == "i586/mmx"
MACHINE_CPU = mmx i586 i486 i386
.  elif ${CPUTYPE} == "i586"
MACHINE_CPU = i586 i486 i386
.  elif ${CPUTYPE} == "i486"
MACHINE_CPU = i486 i386
.  elif ${CPUTYPE} == "i386"
MACHINE_CPU = i386
.  endif
. elif ${MACHINE_ARCH} == "alpha"
.  if ${CPUTYPE} == "ev6"
MACHINE_CPU = ev6 ev56 pca56 ev5 ev45 ev4
.  elif ${CPUTYPE} == "pca56"
MACHINE_CPU = pca56 ev56 ev5 ev45 ev4
.  elif ${CPUTYPE} == "ev56"
MACHINE_CPU = ev56 ev5 ev45 ev4
.  elif ${CPUTYPE} == "ev5"
MACHINE_CPU = ev5 ev45 ev4
.  elif ${CPUTYPE} == "ev45"
MACHINE_CPU = ev45 ev4
.  elif ${CPUTYPE} == "ev4"
MACHINE_CPU = ev4
.  endif
. elif ${MACHINE_ARCH} == "amd64"
MACHINE_CPU = amd64 sse2 sse
. elif ${MACHINE_ARCH} == "ia64"
.  if ${CPUTYPE} == "itanium"
MACHINE_CPU = itanium
.  endif
. endif
.endif

.if ${MACHINE_ARCH} == "alpha"
_CPUCFLAGS += -mieee
.endif

# NB: COPTFLAGS is handled in /usr/src/sys/conf/kern.pre.mk

.if !defined(NO_CPU_CFLAGS)
. if ${CC} == "icc"
CFLAGS += ${_ICC_CPUCFLAGS}
. else
CFLAGS += ${_CPUCFLAGS}
. endif
.endif
