#	$Id: Makefile,v 1.4 1994/08/28 17:21:36 bde Exp $
#
# Doing a make install builds /usr/share/examples

DIRS=etc FreeBSD_version lkm sunrpc
NOOBJ=	noobj

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

.include <bsd.prog.mk>
