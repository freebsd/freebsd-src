
.include <src.opts.mk>

PROG=		avx_sig
SRCS=		avx_sig.c
MAN=

LIBADD=		pthread

.if ${MACHINE_CPUARCH} == "amd64"
SRCS+=		c2x2c_amd64.S
.endif
.if ${MACHINE_CPUARCH} == "aarch64"
SRCS+=		c2x2c_aarch64.S
.endif

.include <bsd.prog.mk>
