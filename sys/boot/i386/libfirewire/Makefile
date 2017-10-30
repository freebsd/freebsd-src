# $FreeBSD$

.include <bsd.init.mk>

LIB=		firewire
INTERNALLIB=

.PATH:		${SYSDIR}/dev/dcons ${SYSDIR}/dev/firewire
SRCS+=		firewire.c fwohci.c dconsole.c
SRCS+=		dcons.c fwcrom.c

CFLAGS+=	-D_BOOT

CFLAGS+=	-I${LDRSRC} -I${SYSDIR} -I.
CFLAGS+=	-I${BTXLIB}
CFLAGS+=	-I${BOOTSRC}/i386/libi386

CFLAGS+=	-Wformat -Wall

.if ${MACHINE_CPUARCH} == "amd64"
CLEANFILES+=    machine
machine: .NOMETA
	ln -sf ${SYSDIR}/i386/include machine
.endif

.include <bsd.lib.mk>

.if ${MACHINE_CPUARCH} == "amd64"
beforedepend ${OBJS}: machine
.endif

