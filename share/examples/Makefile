#	$Id: Makefile,v 1.5 1994/09/13 02:21:03 wollman Exp $
#
# Doing a make install builds /usr/share/examples

DIRS=etc FreeBSD_version lkm sunrpc
NOOBJ=	noobj
ETCOWN= root
ETCGRP= wheel
ETCMODE=644

# Define SHARED to indicate whether you want symbolic links to the system
# source (``symlinks''), or a separate copy (``copies''); (latter useful
# in environments where it's not possible to keep /sys publicly readable)
SHARED?=	copies

all clean cleandir depend lint tags:

beforeinstall: ${SHARED}

copies:
	@${ECHO} installing ${DESTDIR}/usr/share/examples
	@-for a in ${DIRS}; do \
		rm -rf ${DESTDIR}/usr/share/examples/$$a; \
		cp =rp $$a ${DESTDIR}/usr/share/examples; \
	done

symlinks:
	@${ECHO} installing symlinks in ${DESTDIR}/usr/share/examples
	@-for a in ${DIRS}; do \
		rm -rf ${DESTDIR}/usr/share/examples/$$a; \
		ln -s ${.CURDIR}/$$a ${DESTDIR}/usr/share/examples; \
	done

install-etc:
	install -c -o ${ETCOWN} -g ${ETCGRP} -m ${ETCMODE} \
		${.CURDIR}/etc/[a-z]* ${DESTDIR}/etc

.include <bsd.prog.mk>
