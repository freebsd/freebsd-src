# $FreeBSD$

KMOD=	otusfw_main
FIRMWS=	otusfw_main:otusfw_main:1

CLEANFILES=	otusfw_main

otusfw_main: ${.CURDIR}/../../../contrib/dev/otus/otus-main
	cp ${.CURDIR}/../../../contrib/dev/otus/otus-main ${.TARGET}

.include <bsd.kmod.mk>
