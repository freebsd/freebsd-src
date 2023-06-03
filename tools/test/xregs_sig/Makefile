
.include <src.opts.mk>

PROG=		xregs_sig
SRCS=		xregs_sig.c
MAN=

LIBADD=		pthread

.if ${MACHINE_CPUARCH} == "amd64"
SRCS+=		c2x2c_amd64.S
.endif
.if ${MACHINE_CPUARCH} == "aarch64"
SRCS+=		c2x2c_aarch64.S
.endif

.include <bsd.prog.mk>
