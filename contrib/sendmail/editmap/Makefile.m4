dnl $Id: Makefile.m4,v 1.4.6.1 2002/06/21 21:58:27 ca Exp $
include(confBUILDTOOLSDIR`/M4/switch.m4')

define(`confREQUIRE_LIBSM', `true')
# sendmail dir
SMSRCDIR=	ifdef(`confSMSRCDIR', `confSMSRCDIR', `${SRCDIR}/sendmail')
PREPENDDEF(`confENVDEF', `confMAPDEF')
PREPENDDEF(`confINCDIRS', `-I${SMSRCDIR} ')

bldPRODUCT_START(`executable', `editmap')
define(`bldSOURCES', `editmap.c ')
define(`bldINSTALL_DIR', `S')
bldPUSH_SMLIB(`sm')
bldPUSH_SMLIB(`smutil')
bldPUSH_SMLIB(`smdb')
APPENDDEF(`confENVDEF', `-DNOT_SENDMAIL')
bldPRODUCT_END

bldPRODUCT_START(`manpage', `editmap')
define(`bldSOURCES', `editmap.8')
bldPRODUCT_END

bldFINISH
