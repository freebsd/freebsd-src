# $FreeBSD$

# Set default CPU compile flags and baseline CPUTYPE for each arch.  The
# compile flags must support the minimum CPU type for each architecture but
# may tune support for more advanced processors.

.if !defined(CPUTYPE)
.if ${MACHINE_ARCH} == "i386"
_CPUCFLAGS = -mcpu=pentiumpro
CPUTYPE = i386
.elif ${MACHINE_ARCH} == "alpha"
_CPUCFLAGS = -mcpu=ev4 -mtune=ev5
CPUTYPE = ev4
.elif ${MACHINE_ARCH} == "ia64"
_CPUCFLAGS =
CPUTYPE = itanium
.elif ${MACHINE_ARCH} == "sparc64"
_CPUCFLAGS =
CPUTYPE = ultrasparc
.endif
.else

# Handle aliases (not documented in make.conf to avoid user confusion
# between e.g. i586 and pentium)

.if ${MACHINE_ARCH} == "i386"
. if ${CPUTYPE} == "pentiumpro"
CPUTYPE = i686
. elif ${CPUTYPE} == "pentium"
CPUTYPE = i586
. elif ${CPUTYPE} == "athlon"
CPUTYPE = k7
. endif
.endif

# Logic to set up correct gcc optimization flag.  This must be included
# after /etc/make.conf so it can react to the local value of CPUTYPE
# defined therein.  Consult:
#	http://gcc.gnu.org/onlinedocs/gcc/i386-and-x86-64-Options.html
#	http://gcc.gnu.org/onlinedocs/gcc/DEC-Alpha-Options.html
#	http://gcc.gnu.org/onlinedocs/gcc/SPARC-Options.html
#	http://gcc.gnu.org/onlinedocs/gcc/RS-6000-and-PowerPC-Options.html

.if !defined(NO_CPU_CFLAGS) || !defined(NO_CPU_COPTFLAGS)
. if ${MACHINE_ARCH} == "i386"
.  if ${CPUTYPE} == "k7"
.   if defined(BOOTSTRAPPING)
_CPUCFLAGS = -march=k6		# gcc 2.95.x didn't support athlon
.   else
_CPUCFLAGS = -march=athlon
.   endif
.  elif ${CPUTYPE} == "k6-2"
_CPUCFLAGS = -march=k6
.  elif ${CPUTYPE} == "k6"
_CPUCFLAGS = -march=k6
.  elif ${CPUTYPE} == "k5"
_CPUCFLAGS = -march=pentium
.  elif ${CPUTYPE} == "p4"
_CPUCFLAGS = -march=pentiumpro
.  elif ${CPUTYPE} == "p3"
_CPUCFLAGS = -march=pentiumpro
.  elif ${CPUTYPE} == "p2"
_CPUCFLAGS = -march=pentiumpro
.  elif ${CPUTYPE} == "i686"
_CPUCFLAGS = -march=pentiumpro
.  elif ${CPUTYPE} == "i586/mmx"
_CPUCFLAGS = -march=pentium-mmx
.  elif ${CPUTYPE} == "i586"
_CPUCFLAGS = -march=pentium
.  elif ${CPUTYPE} == "i486"
_CPUCFLAGS = -march=i486
.  endif
. elif ${MACHINE_ARCH} == "alpha"
.  if ${CPUTYPE} == "ev6"
_CPUCFLAGS = -mcpu=ev6
.  elif ${CPUTYPE} == "pca56"
_CPUCFLAGS = -mcpu=pca56
.  elif ${CPUTYPE} == "ev56"
_CPUCFLAGS = -mcpu=ev56
.  elif ${CPUTYPE} == "ev5"
_CPUCFLAGS = -mcpu=ev5
.  elif ${CPUTYPE} == "ev45"
_CPUCFLAGS = -mcpu=ev4		# No -mcpu=ev45 for gcc
.  elif ${CPUTYPE} == "ev4"
_CPUCFLAGS = -mcpu=ev4
.  endif
. endif
.endif
.endif

# NB: COPTFLAGS is handled in /usr/src/sys/conf/Makefile.<arch>

.if !defined(NO_CPU_CFLAGS)
CFLAGS += ${_CPUCFLAGS}
.endif

# Set up the list of CPU features based on the CPU type.  This is an
# unordered list to make it easy for client makefiles to test for the
# presence of a CPU feature.

.if ${MACHINE_ARCH} == "i386"
. if ${CPUTYPE} == "k7"
MACHINE_CPU = k7 3dnow mmx k6 k5 i586 i486 i386
. elif ${CPUTYPE} == "k6-2"
MACHINE_CPU = 3dnow mmx k6 k5 i586 i486 i386
. elif ${CPUTYPE} == "k6"
MACHINE_CPU = mmx k6 k5 i586 i486 i386
. elif ${CPUTYPE} == "k5"
MACHINE_CPU = k5 i586 i486 i386
. elif ${CPUTYPE} == "p4"
MACHINE_CPU = sse i686 mmx i586 i486 i386
. elif ${CPUTYPE} == "p3"
MACHINE_CPU = sse i686 mmx i586 i486 i386
. elif ${CPUTYPE} == "p2"
MACHINE_CPU = i686 mmx i586 i486 i386
. elif ${CPUTYPE} == "i686"
MACHINE_CPU = i686 i586 i486 i386
. elif ${CPUTYPE} == "i586/mmx"
MACHINE_CPU = mmx i586 i486 i386
. elif ${CPUTYPE} == "i586"
MACHINE_CPU = i586 i486 i386
. elif ${CPUTYPE} == "i486"
MACHINE_CPU = i486 i386
. elif ${CPUTYPE} == "i386"
MACHINE_CPU = i386
. endif
.elif ${MACHINE_ARCH} == "alpha"
. if ${CPUTYPE} == "ev6"
MACHINE_CPU = ev6 ev56 pca56 ev5 ev45 ev4
. elif ${CPUTYPE} == "pca56"
MACHINE_CPU = pca56 ev56 ev5 ev45 ev4
. elif ${CPUTYPE} == "ev56"
MACHINE_CPU = ev56 ev5 ev45 ev4
. elif ${CPUTYPE} == "ev5"
MACHINE_CPU = ev5 ev45 ev4
. elif ${CPUTYPE} == "ev45"
MACHINE_CPU = ev45 ev4
. elif ${CPUTYPE} == "ev4"
MACHINE_CPU = ev4
. endif
.elif ${MACHINE_ARCH} == "ia64"
. if ${CPUTYPE} == "itanium"
MACHINE_CPU = itanium
. endif
.endif
