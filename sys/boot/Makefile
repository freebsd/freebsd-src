# $FreeBSD$

.include <src.opts.mk>

.if ${MK_FORTH} != "no"
# Build the add-in FORTH interpreter.
SUBDIR+=		ficl
SUBDIR+=		forth
.endif

SUBDIR+=		common

.include <bsd.arch.inc.mk>

# Pick the machine-dependent subdir based on the target architecture.
ADIR=			${MACHINE:S/powerpc64/powerpc/}
.if exists(${.CURDIR}/${ADIR}/.)
SUBDIR+=		${ADIR}
.endif

.include <bsd.subdir.mk>
