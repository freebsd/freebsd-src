# $FreeBSD$

# Build the add-in FORTH interpreter
SUBDIR+=		ficl

# Pick the machine-dependant subdir based on the target architecture.
SUBDIR+=		${MACHINE}

# Build ARC / AlphaBIOS executable on the Alpha
.if ${MACHINE_ARCH} == "alpha"
SUBDIR+=		arc
.endif

.include <bsd.subdir.mk>
