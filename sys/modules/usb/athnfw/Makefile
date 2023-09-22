IMG=			athn-open-ar9271.bin

KMOD=			${IMG}
CLEANFILES+=	${IMG}
FIRMWS=			${IMG}:${IMG}:141

${IMG}: ${SRCTOP}/sys/contrib/dev/athnfw/${IMG}
	cp ${.ALLSRC} ${.TARGET}

.include <bsd.kmod.mk>