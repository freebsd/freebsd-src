# $FreeBSD$

FWDIR=	${.CURDIR}/../../contrib/dev/iwn
.PATH: ${FWDIR}

KMOD=	iwnfw
FIRMWS=	iwlwifi-4965-228.57.2.23.fw:iwnfw-4965:22857223 \
	iwlwifi-5000-5.4.A.11.fw:iwnfw-5000:54A11 \
	iwlwifi-5150-8.24.2.2.fw:iwnfw-5150:82422

CLEANFILES=	iwlwifi-4965-228.57.2.23.fw iwlwifi-5000-5.4.A.11.fw \
		iwlwifi-5150-8.24.2.2.fw

iwlwifi-4965-228.57.2.23.fw: ${FWDIR}/iwlwifi-4965-228.57.2.23.fw.uu
	uudecode -p ${FWDIR}/iwlwifi-4965-228.57.2.23.fw.uu > ${.TARGET}

iwlwifi-5000-5.4.A.11.fw: ${FWDIR}/iwlwifi-5000-5.4.A.11.fw.uu
	uudecode -p ${FWDIR}/iwlwifi-5000-5.4.A.11.fw.uu > ${.TARGET}

iwlwifi-5150-8.24.2.2.fw: ${FWDIR}/iwlwifi-5150-8.24.2.2.fw.uu
	uudecode -p ${FWDIR}/iwlwifi-5150-8.24.2.2.fw.uu > ${.TARGET}

.include <bsd.kmod.mk>
