include(confBUILDTOOLSDIR`/M4/switch.m4')

# sendmail dir
SMSRCDIR=     ifdef(`confSMSRCDIR', `confSMSRCDIR', `${SRCDIR}/sendmail')
PREPENDDEF(`confENVDEF', `confMAPDEF')
PREPENDDEF(`confINCDIRS', `-I${SMSRCDIR} ')

bldPRODUCT_START(`executable', `rmail')
define(`bldNO_INSTALL')
define(`bldSOURCES', `rmail.c ')
bldPUSH_SMLIB(`smutil')
bldPRODUCT_END

bldPRODUCT_START(`manpage', `rmail')
define(`bldSOURCES', `rmail.8')
bldPRODUCT_END

RMAIL=ifdef(`confFORCE_RMAIL', `force-install', `defeat-install')

divert(bldTARGETS_SECTION)
install: ${RMAIL}

defeat-install:
	@echo "NOTE: This version of rmail is not suited for some operating"
	@echo "      systems.  You can force the install using"
	@echo "      'Build force-install'."

force-install: install-rmail ifdef(`confNO_MAN_BUILD',, `install-docs')

install-rmail: rmail
	${INSTALL} -c -o ${UBINOWN} -g ${UBINGRP} -m ${UBINMODE} rmail ${DESTDIR}${UBINDIR}
divert

bldFINISH

