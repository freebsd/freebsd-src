
# Common rules for building iwlwifi firmware.

EXT?=		ucode
_NAME?=		iwlwifi-${NAME}-${FWVERSION}.${EXT}

IMG=		${_NAME}
KMOD=		${_NAME}

CLEANFILES+=	${IMG}

FIRMWS=		${IMG}:${IMG}:${FWVERSION}

# License ack is not needed for iwlwifi (same as iwn/iwm).
#FIRMWARE_LICENSE=

${IMG}: ${SRCTOP}/sys/contrib/dev/iwlwififw/${IMG}
	cp ${.ALLSRC} ${.TARGET}

