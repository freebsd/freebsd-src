include(confBUILDTOOLSDIR`/M4/switch.m4')

# sendmail dir
SMSRCDIR=	ifdef(`confSMSRCDIR', `confSMSRCDIR', `${SRCDIR}/sendmail')
PREPENDDEF(`confENVDEF', `confMAPDEF')
PREPENDDEF(`confINCDIRS', `-I${SMSRCDIR} ')

bldPRODUCT_START(`executable', `smrsh')
define(`bldINSTALL_DIR', `E')
define(`bldSOURCES', `smrsh.c ')
bldPUSH_SMLIB(`smutil')
bldPRODUCT_END

bldPRODUCT_START(`manpage', `smrsh')
define(`bldSOURCES', `smrsh.8')
bldPRODUCT_END

bldFINISH
