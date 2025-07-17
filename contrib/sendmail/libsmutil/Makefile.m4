dnl $Id: Makefile.m4,v 8.18 2006-06-28 21:02:39 ca Exp $
include(confBUILDTOOLSDIR`/M4/switch.m4')

define(`confREQUIRE_SM_OS_H', `true')
# sendmail dir
SMSRCDIR=	ifdef(`confSMSRCDIR', `confSMSRCDIR', `${SRCDIR}/sendmail')
PREPENDDEF(`confENVDEF', `confMAPDEF')
PREPENDDEF(`confINCDIRS', `-I${SMSRCDIR} ')

bldPRODUCT_START(`library', `libsmutil')
define(`bldSOURCES', `debug.c err.c lockfile.c safefile.c snprintf.c cf.c ')
APPENDDEF(`confENVDEF', `-DNOT_SENDMAIL')
bldPRODUCT_END

srcdir=${SRCDIR}/libsmutil
define(`confCHECK_LIBS',`libsmutil.a ../libsm/libsm.a')dnl
include(confBUILDTOOLSDIR`/M4/'bldM4_TYPE_DIR`/check.m4')
smcheck(`t-lockfile', `compile')
smcheck(`t-lockfile-0.sh', `run')

bldFINISH
