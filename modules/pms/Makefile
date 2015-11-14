# $FreeBSD$
# 5/31/2012
#PATH shall include all directories

.PATH: 	${.CURDIR}/../../dev/pms/freebsd/driver/ini/src \
	${.CURDIR}/../../dev/pms/RefTisa/sallsdk/api \
	${.CURDIR}/../../dev/pms/RefTisa/sallsdk/spc \
	${.CURDIR}/../../dev/pms/RefTisa/sallsdk/hda/64k \
	${.CURDIR}/../../dev/pms/RefTisa/discovery/api \
	${.CURDIR}/../../dev/pms/RefTisa/discovery/dm \
	${.CURDIR}/../../dev/pms/RefTisa/sat/api \
	${.CURDIR}/../../dev/pms/RefTisa/sat/src \
	${.CURDIR}/../../dev/pms/RefTisa/tisa/api \
	${.CURDIR}/../../dev/pms/RefTisa/tisa/sassata/common \
	${.CURDIR}/../../dev/pms/RefTisa/tisa/sassata/sas/common \
	${.CURDIR}/../../dev/pms/RefTisa/tisa/sassata/sas/ini \
	${.CURDIR}/../../dev/pms/RefTisa/tisa/sassata/sata/host \
	${.CURDIR}/../../dev/pms/freebsd/driver/common

CFLAGS+=-fno-builtin
CFLAGS+=-fms-extensions

CFLAGS+=-Wredundant-decls
CFLAGS+=-Wunused-variable
CFLAGS+=-Wnested-externs
CFLAGS+=-Woverflow
CFLAGS+=-Wmissing-prototypes
CFLAGS+=-Wparentheses
CFLAGS+=-w

#CFLAGS+=-g


#SUBDIR= tisa tisa/drivers tisa/drivers/FreeBSD tisa/drivers/FreeBSD/ini tisa/drivers/FreeBSD/ini/src
#SUBDIR= Hialeah_driver Hialeah_driver/freebsd Hialeah_driver/freebsd/ini Hialeah_driver/freebsd/ini/src 

KMOD=	pmspcv
#OS files
SRCS=agtiapi.c

#LL files
SRCS+= sadisc.c mpi.c saframe.c sahw.c sainit.c saint.c sampicmd.c sampirsp.c saphy.c saport.c
SRCS+= sasata.c sasmp.c sassp.c satimer.c sautil.c saioctlcmd.c mpidebug.c
#DM files
SRCS+= dminit.c dmsmp.c dmdisc.c dmport.c dmtimer.c dmmisc.c
#SM files
SRCS+= sminit.c smmisc.c smsat.c smsatcb.c smsathw.c smtimer.c
#TD files
SRCS+= tdinit.c tdmisc.c tdesgl.c tdport.c tdint.c tdioctl.c tdhw.c ossacmnapi.c tddmcmnapi.c tdsmcmnapi.c
SRCS+= tdtimers.c itdio.c itdcb.c itdinit.c itddisc.c sat.c ossasat.c sathw.c

#header files
SRCS+=	agtiapi.h agtiproto.h agdef.h 
#
SRCS+=	osenv.h ostypes.h
SRCS+=  opt_scsi.h opt_cam.h
SRCS+=  device_if.h bus_if.h pci_if.h
#INCSRC=	../../common


.include <bsd.kmod.mk>
