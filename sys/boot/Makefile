# $FreeBSD: src/sys/boot/Makefile,v 1.6.2.1 2002/07/17 14:23:14 ru Exp $

# Build the add-in FORTH interpreter
SUBDIR+=		ficl

# Pick the machine-dependant subdir based on the target architecture.
SUBDIR+=		${MACHINE}

# Build ARC / AlphaBIOS executable on the Alpha
.if ${MACHINE_ARCH} == "alpha"
SUBDIR+=		arc
.endif

.include <bsd.subdir.mk>
