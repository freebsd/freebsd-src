
# $FreeBSD$

all: aicasm

aicasm:	${.CURDIR}/../../../dev/aic7xxx/aicasm/*.[chyl]
	${MAKE} -f ${.CURDIR}/../../../dev/aic7xxx/aicasm/Makefile \
	MAKESRCPATH=${.CURDIR}/../../../dev/aic7xxx/aicasm

install:

clean:
	rm -f *.[chylo]
	rm -f aicasm
