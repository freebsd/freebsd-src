
# $FreeBSD: src/sys/modules/aic7xxx/aicasm/Makefile,v 1.4.2.1 2002/09/02 05:47:45 scottl Exp $

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

cleandepend:
	${MAKE} -f ${.CURDIR}/../../../dev/aic7xxx/aicasm/Makefile \
	MAKESRCPATH=${.CURDIR}/../../../dev/aic7xxx/aicasm cleandepend

install:
