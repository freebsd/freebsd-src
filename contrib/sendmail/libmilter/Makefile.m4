include(confBUILDTOOLSDIR`/M4/switch.m4')

define(`confMT', `true')

# sendmail dir
SMSRCDIR=	ifdef(`confSMSRCDIR', `confSMSRCDIR', `${SRCDIR}/sendmail')
PREPENDDEF(`confINCDIRS', `-I${SMSRCDIR} ')

bldPRODUCT_START(`library', `libmilter')
define(`bldSOURCES', `main.c engine.c listener.c handler.c comm.c smfi.c signal.c sm_gethost.c ')
bldPUSH_SMLIB(`smutil')
bldPRODUCT_END
APPENDDEF(`confENVDEF', `-DNOT_SENDMAIL')

bldFINISH
