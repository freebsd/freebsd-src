
# $FreeBSD$

all: aicasm

aicasm:	${.CURDIR}/../../../dev/aic7xxx/aicasm/*.[chyl]
	${MAKE} -f ${.CURDIR}/../../../dev/aic7xxx/aicasm/Makefile \
	MAKESRCPATH=${.CURDIR}/../../../dev/aic7xxx/aicasm

depend:
	${MAKE} -f ${.CURDIR}/../../../dev/aic7xxx/aicasm/Makefile \
	MAKESRCPATH=${.CURDIR}/../../../dev/aic7xxx/aicasm depend

obj:
	${MAKE} -f ${.CURDIR}/../../../dev/aic7xxx/aicasm/Makefile \
	MAKESRCPATH=${.CURDIR}/../../../dev/aic7xxx/aicasm obj

clean:
	${MAKE} -f ${.CURDIR}/../../../dev/aic7xxx/aicasm/Makefile \
	MAKESRCPATH=${.CURDIR}/../../../dev/aic7xxx/aicasm clean

cleandir:
	${MAKE} -f ${.CURDIR}/../../../dev/aic7xxx/aicasm/Makefile \
	MAKESRCPATH=${.CURDIR}/../../../dev/aic7xxx/aicasm cleandir

install:
