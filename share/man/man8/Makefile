#	@(#)Makefile	8.1 (Berkeley) 6/5/93
# $FreeBSD$

.PATH: ${.CURDIR}/../../../release/sysinstall

MAN=	adding_user.8 \
	crash.8 \
	diskless.8 \
	intro.8 \
	picobsd.8 \
	rc.8 \
	sticky.8 \
	yp.8

.if exists(${.CURDIR}/../../../release/sysinstall)
MAN+=	sysinstall.8
.endif

MLINKS=	rc.8 rc.early.8 rc.8 rc.serial.8 rc.8 rc.pccard.8 rc.8 rc.network.8
MLINKS+=rc.8 rc.firewall.8 rc.8 rc.atm.8 rc.8 rc.local.8 rc.8 rc.shutdown.8
MLINKS+=yp.8 YP.8 yp.8 NIS.8 yp.8 nis.8

# XXX NOT IMPORTED:	man8.hp300 man8.tahoe man8.vax
.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "alpha"
MLINKS+=rc.8 ${MACHINE_ARCH}/rc.${MACHINE_ARCH}.8
SUBDIR=	man8.${MACHINE_ARCH}
.endif

.if make(maninstall)
maninstall:: _SUBDIR
.endif

.include <bsd.prog.mk>
