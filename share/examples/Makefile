#	$Id: Makefile,v 1.1 1994/08/09 18:25:36 wollman Exp $
#
# Doing a make install builds /usr/share/examples
#
all clean cleandir depend lint tags:

DIRS=sunrpc etc FreeBSD_version

# Define SHARED to indicate whether you want symbolic links to the system
# source (``symlinks''), or a separate copy (``copies''); (latter useful
# in environments where it's not possible to keep /sys publicly readable)
SHARED?=	copies

beforeinstall: ${SHARED}

copies:
	@echo installing ${DESTDIR}/usr/share/examples
	@rm -rf ${DESTDIR}/usr/share/examples
	@-mkdir ${DESTDIR}/usr/share/examples
	@tar cf - ${DIRS} | \
		(cd ${DESTDIR}/usr/share/examples; tar xpfB -);
	@chown -R ${BINOWN}.${BINGRP} ${DESTDIR}/usr/share/examples
	@chmod -R a-w ${DESTDIR}/usr/share/examples

symlinks:
	@echo installing symlinks in ${DESTDIR}/usr/share/examples
	@rm -rf ${DESTDIR}/usr/share/examples
	@-mkdir ${DESTDIR}/usr/share/examples
	@-for a in ${DIRS}; do \
		ln -s ${.CURDIR}/$$a ${DESTDIR}/usr/share/examples; \
	done
	@chown ${BINOWN}.${BINGRP} ${DESTDIR}/usr/share/examples
	@chmod -R a-w ${DESTDIR}/usr/share/examples

.include <bsd.prog.mk>

		
