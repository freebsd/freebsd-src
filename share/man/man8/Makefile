#	@(#)Makefile	8.1 (Berkeley) 6/5/93
# $FreeBSD$

MAN=	adding_user.8 crash.8 diskless.8 intro.8 picobsd.8 rc.8 sticky.8

MLINKS=	rc.8 rc.serial.8 rc.8 rc.pccard.8 rc.8 rc.network.8 \
	rc.8 rc.firewall.8 rc.8 rc.atm.8 rc.8 rc.local.8 rc.8 rc.shutdown.8

# XXX NOT IMPORTED:	man8.hp300 man8.tahoe man8.vax
.if ${MACHINE_ARCH} == "i386"
MLINKS+=	rc.8 i386/rc.i386.8
SUBDIR=	man8.i386
.elif ${MACHINE_ARCH} == "alpha"
MLINKS+=	rc.8 alpha/rc.alpha.8
SUBDIR=	man8.alpha
.endif

.if make(maninstall)
maninstall:: _SUBDIR
.endif

.include <bsd.prog.mk>
