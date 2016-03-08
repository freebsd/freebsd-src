# $FreeBSD$
#
# Hand installing our scripts and optionally (based on MK_CDDL) installing
# the DTraceToolkit.
#

.include <src.opts.mk>

SUBDIR= ${_toolkit}

.if ${MK_CDDL} != "no"
_toolkit=	toolkit
.endif

SCRIPTS=	blocking \
		disklatency \
		disklatencycmd \
		hotopen \
		nfsattrstats \
		nfsclienttime \
		siftr \
		tcpconn \
		tcpdebug \
		tcpstate \
		tcptrack \
		udptrack \
		watch_execve \
		watch_kill \
		watch_vop_remove

SCRIPTSDIR= ${SHAREDIR}/dtrace

.include <bsd.prog.mk>
