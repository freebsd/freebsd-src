# $FreeBSD$
#
# Hand installing our scripts and optionally (based on MK_CDDL) installing
# the DTraceToolkit.
#

.include <bsd.own.mk>

SUBDIR= ${_toolkit}

.if ${MK_CDDL} != "no"
_toolkit=	toolkit
.endif

SCRIPTS=	nfsclienttime hotopen

SCRIPTSDIR= ${SHAREDIR}/dtrace

NO_OBJ=

.include <bsd.prog.mk>
