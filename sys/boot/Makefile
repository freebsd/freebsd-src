# $FreeBSD: src/sys/boot/Makefile,v 1.6 1999/11/15 08:31:14 marcel Exp $

# Build the add-in FORTH interpreter
SUBDIR+=		ficl

# Pick the machine-dependant subdir based on the target architecture.
# XXX pc98 is basicly a special i386 with MACHINE_ARCH=i386 and MACHINE=pc98.
.if ${MACHINE_ARCH} == "i386" && ${MACHINE} == "pc98"
SUBDIR+=		${MACHINE}
.else
SUBDIR+=		${MACHINE_ARCH}
.endif

# Build ARC / AlphaBIOS executable on the Alpha
.if ${MACHINE_ARCH} == "alpha"
SUBDIR+=		arc
.endif

.include <bsd.subdir.mk>
