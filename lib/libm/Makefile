#	@(#)Makefile	5.11 (Berkeley) 12/2/92
#
# ieee		- for most IEEE machines, we hope.
# mc68881	- the, ahem, mc68881.
# national	- for those ieee machines whose floating point implementation
#		  has similar byte ordering as the NATIONAL 32016 with 32081.
# i386		- i387 NPX, currently the same as "national"
# mips		- for MIPS achitecture machines
# tahoe		- for the tahoe double format.
# vax		- for the vax D_floating format

LIB=	m
CFLAGS+=-I${.CURDIR}/common_source

.if (${MACHINE} == "ieee")

HARDWARE=${MACHINE}
.PATH:	${.CURDIR}/common_source ${.CURDIR}/common ${.CURDIR}/ieee
# common_source
SRCS+=	acosh.c asincos.c asinh.c atan.c atanh.c cosh.c erf.c exp.c \
	exp__E.c expm1.c floor.c fmod.c gamma.c lgamma.c j0.c j1.c \
	jn.c log.c log10.c log1p.c log__L.c pow.c sinh.c tanh.c
# common
SRCS+=	atan2.c sincos.c tan.c
# ieee
SRCS+=	cabs.c cbrt.c support.c

.elif (${MACHINE} == "hp300" || ${MACHINE} == "luna68k")

HARDWARE=mc68881
.PATH:	${.CURDIR}/mc68881 ${.CURDIR}/common_source ${.CURDIR}/ieee
# common_source
SRCS+=	acosh.c asinh.c erf.c exp__E.c fmod.c gamma.c lgamma.c j0.c j1.c log__L.c \
	pow.c
# mc68881
SRCS+=	asincos.s atan.s atan2.c atanh.s cosh.s exp.s expm1.s floor.s \
	log.s log10.s log1p.s sincos.s sinh.s sqrt.s support.s tan.s tanh.s
# ieee
SRCS+=	cabs.c cbrt.c

.elif (${MACHINE} == "i386")

HARDWARE=i387
.PATH:	${.CURDIR}/common_source ${.CURDIR}/common ${.CURDIR}/ieee
CFLAGS+= -Dnational
# common_source
SRCS+=	acosh.c asincos.c asinh.c atan.c atanh.c cosh.c erf.c exp.c \
	exp__E.c expm1.c floor.c fmod.c gamma.c lgamma.c j0.c j1.c jn.c log.c \
	log10.c log1p.c log__L.c pow.c sinh.c tanh.c
# common
SRCS+=	atan2.c sincos.c tan.c
# ieee
SRCS+=	cabs.c cbrt.c support.c

.elif (${MACHINE} == "mips")

HARDWARE=${MACHINE}
.PATH:	${.CURDIR}/common_source ${.CURDIR}/common ${.CURDIR}/ieee
CFLAGS+= -Dnational
# common_source
SRCS+=	acosh.c asincos.c asinh.c atan.c atanh.c cosh.c erf.c exp.c \
	exp__E.c expm1.c floor.c fmod.c gamma.c lgamma.c j0.c j1.c jn.c log.c \
	log10.c log1p.c log__L.c pow.c sinh.c tanh.c
# common
SRCS+=	atan2.c sincos.c tan.c
# ieee
SRCS+=	cabs.c cbrt.c support.c

.elif (${MACHINE} == "national")

HARDWARE=${MACHINE}
.PATH:	${.CURDIR}/common_source ${.CURDIR}/common ${.CURDIR}/national \
.elif (${MACHINE} == "national")

HARDWARE=${MACHINE}
.PATH:	${.CURDIR}/common_source ${.CURDIR}/common ${.CURDIR}/national \
	${.CURDIR}/ieee
# common_source
SRCS+=	acosh.c asincos.c asinh.c atan.c atanh.c cosh.c erf.c exp.c \
	exp__E.c expm1.c floor.c fmod.c gamma.c lgamma.c j0.c j1.c jn.c log.c \
	log10.c log1p.c log__L.c pow.c sinh.c tanh.c
# common
SRCS+=	atan2.c sincos.c tan.c
# national
SRCS+=	sqrt.s support.s
# ieee
SRCS+=	cabs.c cbrt.c

.elif (${MACHINE} == "sparc")

HARDWARE=${MACHINE}
.PATH:  ${.CURDIR}/common_source ${.CURDIR}/common ${.CURDIR}/ieee
# common_source
SRCS+=	acosh.c asincos.c asinh.c atan.c atanh.c cosh.c erf.c exp.c \
	exp__E.c expm1.c floor.c fmod.c gamma.c lgamma.c j0.c j1.c jn.c log.c \
	log10.c log1p.c log__L.c pow.c sinh.c tanh.c
# XXX should do sqrt & support functions in assembly
# common
SRCS+=	atan2.c sincos.c tan.c
# ieee
SRCS+=	cabs.c cbrt.c support.c

.elif (${MACHINE} == "tahoe")

HARDWARE=${MACHINE}
.PATH:	${.CURDIR}/common_source ${.CURDIR}/common ${.CURDIR}/tahoe \
# common_source
SRCS+=	acosh.c asincos.c asinh.c atan.c atanh.c cosh.c erf.c exp.c \
	exp__E.c expm1.c floor.c fmod.c gamma.c lgamma.c j0.c j1.c jn.c log.c \
	log10.c log1p.c log__L.c pow.c sinh.c tanh.c
# common
SRCS+=	atan2.c sincos.c tan.c
# tahoe
SRCS+=	cabs.s cbrt.s sqrt.s support.s infnan.s

.elif (${MACHINE} == "vax")

HARDWARE=${MACHINE}
.PATH:	${.CURDIR}/common_source ${.CURDIR}/vax
# common_source
SRCS+=	acosh.c asincos.c asinh.c atan.c atanh.c cosh.c erf.c exp.c \
	exp__E.c expm1.c floor.c fmod.c gamma.c lgamma.c j0.c j1.c jn.c log.c \
	log10.c log1p.c log__L.c pow.c sinh.c tanh.c
# vax
SRCS+=	atan2.s cabs.s cbrt.s sqrt.s sincos.s tan.s argred.s support.s \
	infnan.s

.endif

MANSRC= ${.CURDIR}/common_source

MAN3+=	acos.3 acosh.3 asin.3 asinh.3 atan.3 atan2.3 atanh.3 ceil.3 \
	cos.3 cosh.3 erf.3 exp.3 fabs.3 floor.3 fmod.3 hypot.3 ieee.3 \
	infnan.3 j0.3 lgamma.3 math.3 rint.3 sin.3 sinh.3 sqrt.3 \
	tan.3 tanh.3

MLINKS+=erf.3 erfc.3
MLINKS+=exp.3 expm1.3 exp.3 log.3 exp.3 log10.3 exp.3 log1p.3 exp.3 pow.3
MLINKS+=hypot.3 cabs.3
MLINKS+=ieee.3 copysign.3 ieee.3 drem.3 ieee.3 finite.3 ieee.3 logb.3 \
	ieee.3 scalb.3
MLINKS+=j0.3 j1.3 j0.3 jn.3 j0.3 y0.3 j0.3 y1.3 j0.3 yn.3
MLINKS+=lgamma.3 gamma.3

# can't use the standard mkdep, because there are some .s files that
# are using '#' as a comment indicator and cpp thinks it's an undefined
# control.

depend: .depend
.depend: ${SRCS}
	mkdep ${CFLAGS:M-[ID]*} ${.ALLSRC:M*.c}

.include <bsd.lib.mk>

.s.o:
	${AS} -o ${.TARGET} ${.IMPSRC}
	@${LD} -x -r ${.TARGET}
	@mv a.out ${.TARGET}

.s.po:
	sed -f ${.CURDIR}/${HARDWARE}/mcount.sed ${.IMPSRC} | \
	    ${AS} -o ${.TARGET}
	@${LD} -X -r ${.TARGET}
	@mv a.out ${.TARGET}
