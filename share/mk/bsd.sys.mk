# $FreeBSD$
#
# This file contains common settings used for building FreeBSD
# sources.

# Enable various levels of compiler warning checks.  These may be
# overridden (e.g. if using a non-gcc compiler) by defining NO_WARNS.

# for GCC:  http://gcc.gnu.org/onlinedocs/gcc-3.0.4/gcc_3.html#IDX143

# the default is gnu99 for now
CSTD		?= gnu99

.if ${CC:T:Micc} != "icc"
. if ${CSTD} == "k&r"
CFLAGS		+= -traditional
. elif ${CSTD} == "c89" || ${CSTD} == "c90"
CFLAGS		+= -std=iso9899:1990
. elif ${CSTD} == "c94" || ${CSTD} == "c95"
CFLAGS		+= -std=iso9899:199409
. elif ${CSTD} == "c99"
CFLAGS		+= -std=iso9899:1999
. else
CFLAGS		+= -std=${CSTD}
. endif
.endif
.if !defined(NO_WARNS) && ${CC:T:Micc} != "icc"
# -pedantic is problematic because it also imposes namespace restrictions
#CFLAGS		+= -pedantic
. if defined(WARNS)
.  if ${WARNS} >= 1
CWARNFLAGS	+=	-Wsystem-headers
.   if !defined(NO_WERROR)
CWARNFLAGS	+=	-Werror
.   endif
.  endif
.  if ${WARNS} >= 2
CWARNFLAGS	+=	-Wall -Wno-format-y2k
.  endif
.  if ${WARNS} >= 3
CWARNFLAGS	+=	-W -Wno-unused-parameter -Wstrict-prototypes\
			-Wmissing-prototypes -Wpointer-arith
.  endif
.  if ${WARNS} >= 4
CWARNFLAGS	+=	-Wreturn-type -Wcast-qual -Wwrite-strings -Wswitch\
			-Wshadow -Wunused-parameter
.   if !defined(NO_WCAST_ALIGN)
CWARNFLAGS	+=	-Wcast-align
.   endif
.  endif
# BDECFLAGS
.  if ${WARNS} >= 6
CWARNFLAGS	+=	-Wchar-subscripts -Winline -Wnested-externs\
			-Wredundant-decls -Wold-style-definition
.  endif
.  if ${WARNS} >= 2 && ${WARNS} <= 4
# XXX Delete -Wuninitialized by default for now -- the compiler doesn't
# XXX always get it right.
CWARNFLAGS	+=	-Wno-uninitialized
.  endif
CWARNFLAGS	+=	-Wno-pointer-sign
. endif

. if defined(FORMAT_AUDIT)
WFORMAT		=	1
. endif
. if defined(WFORMAT)
.  if ${WFORMAT} > 0
#CWARNFLAGS	+=	-Wformat-nonliteral -Wformat-security -Wno-format-extra-args
CWARNFLAGS	+=	-Wformat=2 -Wno-format-extra-args
.   if !defined(NO_WERROR)
CWARNFLAGS	+=	-Werror
.   endif
.  endif
. endif
.endif

.if defined(IGNORE_PRAGMA)
CWARNFLAGS	+=	-Wno-unknown-pragmas
.endif

.if ${MK_SSP} != "no" && ${CC:T:Micc} != "icc" && \
	${MACHINE_CPUARCH} != "ia64" && \
	${MACHINE_CPUARCH} != "arm" && ${MACHINE_CPUARCH} != "mips"
# Don't use -Wstack-protector as it breaks world with -Werror.
SSP_CFLAGS	?=	-fstack-protector
CFLAGS		+=	${SSP_CFLAGS}
.endif

# Allow user-specified additional warning flags
CFLAGS		+=	${CWARNFLAGS}
