#
# Common rules for building firmware.  Note this gets auto-included
# by the subdir Makefile's as a consequence of included bsd.kmod.mk.

_NAME=		rtw${NAME}_fw.bin

IMG=		${_NAME}
KMOD=		${_NAME}

CLEANFILES+=	${IMG}

FIRMWS=		${IMG}:${IMG}:${VERSION}

${IMG}: ${SRCTOP}/sys/contrib/dev/rtw89fw/${IMG}
	cp ${.ALLSRC} ${.TARGET}

