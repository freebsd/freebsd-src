#	$FreeBSD$

NCPLIB=${.CURDIR}

LIB=		ncp

SHLIB_MAJOR=	1
SHLIB_MINOR=	0

NOMAN=

SRCS=	ncpl_subr.c ncpl_bind.c ncpl_queue.c ncpl_file.c ncpl_misc.c \
	ncpl_net.c ncpl_rcfile.c ncpl_conn.c ncpl_nls.c ncpl_msg.c \
	ncpl_rpc.c ncpl_crypt.c ipx.c sap.c

HEADERS=ncp_cfg.h ncp_lib.h ncp_file.h ncp_rcfile.h

beforeinstall:
.for hdr in ${HEADERS}
	install -c -o ${BINOWN} -g ${BINGRP} -m 0444 \
	     ${.CURDIR}/${hdr} ${DESTDIR}/usr/include/netncp
.endfor

.include <bsd.lib.mk>
