#	$Id: Makefile,v 1.13.4.3 1996/01/06 07:34:36 nate Exp $
#
# Doing a make install builds /usr/share/examples

DIRS=etc FreeBSD_version ibcs2 lkm sliplogin startslip sunrpc sup

DDIR=${DESTDIR}/usr/share/examples

NOOBJ=	noobj

# Define SHARED to indicate whether you want symbolic links to the system
# source (``symlinks''), or a separate copy (``copies''); (latter useful
# in environments where it's not possible to keep /sys publicly readable)
SHARED?=	copies

all clean cleandir depend lint tags:

beforeinstall: ${SHARED}

copies:
	@${ECHO} installing ${DDIR}
	@-for a in ${DIRS}; do \
		rm -rf ${DDIR}/$$a; \
	done
	find ${DIRS} \( -name CVS -prune \) -o -type f -print -exec \
		${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 644 {} ${DDIR}/{} \;

symlinks:
	@${ECHO} installing symlinks in ${DDIR}
	@-for a in ${DIRS}; do \
		rm -rf ${DDIR}/$$a; \
		ln -s ${.CURDIR}/$$a ${DDIR}; \
	done

.include <bsd.prog.mk>
