#
# Hand installing our scripts and optionally (based on MK_CDDL) installing
# the DTraceToolkit.
#

.include <src.opts.mk>

PACKAGE=	dtrace
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
		udptrack

SCRIPTSDIR= ${SHAREDIR}/dtrace

DSRCS=		mbuf.d ipfw.d

FILES=		${DSRCS}
FILESDIR=	/usr/lib/dtrace
FILESMODE=	${NOBINMODE}

.include <bsd.prog.mk>
