include(confBUILDTOOLSDIR`/M4/switch.m4')

define(`confMT', `true')

# sendmail dir
SMSRCDIR=	ifdef(`confSMSRCDIR', `confSMSRCDIR', `${SRCDIR}/sendmail')
PREPENDDEF(`confINCDIRS', `-I${SMSRCDIR} ')

bldPRODUCT_START(`library', `libmilter')
define(`bldINSTALLABLE', `true')
define(`bldSOURCES', `main.c engine.c listener.c handler.c comm.c smfi.c signal.c sm_gethost.c ')
bldPUSH_SMLIB(`smutil')
bldPUSH_INSTALL_TARGET(`install-mfapi')
bldPRODUCT_END
APPENDDEF(`confENVDEF', `-DNOT_SENDMAIL')

divert(bldTARGETS_SECTION)
# Install the API header file
MFAPI=	${SRCDIR}/include/libmilter/mfapi.h
install-mfapi: ${MFAPI}
	${INSTALL} ${MFAPI} ${DESTDIR}${INCLUDEDIR}
divert(0)

bldFINISH
