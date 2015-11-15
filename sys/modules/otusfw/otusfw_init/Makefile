# $FreeBSD$

KMOD=	otusfw_init
FIRMWS=	otusfw_init:otusfw_init:1

CLEANFILES=	otus_init

otusfw_init: ${.CURDIR}/../../../contrib/dev/otus/otus-init
	cp ${.CURDIR}/../../../contrib/dev/otus/otus-init ${.TARGET}

.include <bsd.kmod.mk>
