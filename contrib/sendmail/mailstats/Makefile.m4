include(confBUILDTOOLSDIR`/M4/switch.m4')

define(`confREQUIRE_LIBSM', `true')
# sendmail dir
SMSRCDIR=	ifdef(`confSMSRCDIR', `confSMSRCDIR', `${SRCDIR}/sendmail')
PREPENDDEF(`confENVDEF', `confMAPDEF')
PREPENDDEF(`confINCDIRS', `-I${SMSRCDIR} ')

bldPRODUCT_START(`executable', `mailstats')
define(`bldINSTALL_DIR', `S')
define(`bldSOURCES', `mailstats.c ')
bldPUSH_SMLIB(`sm')
bldPUSH_SMLIB(`smutil')
APPENDDEF(`confENVDEF', `-DNOT_SENDMAIL')
bldPRODUCT_END

bldPRODUCT_START(`manpage', `mailstats')
define(`bldSOURCES', `mailstats.8')
bldPRODUCT_END

bldFINISH

