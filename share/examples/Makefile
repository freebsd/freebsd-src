#	$Id: Makefile,v 1.16 1995/09/14 23:49:21 ache Exp $
#
# Doing a make install builds /usr/share/examples

DIRS=etc find_interface FreeBSD_version ibcs2 lkm startslip sunrpc sup

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
	find ${DIRS} \( -name CVS -prune \) -o -print | cpio -dumpv ${DDIR}

symlinks:
	@${ECHO} installing symlinks in ${DDIR}
	@-for a in ${DIRS}; do \
		rm -rf ${DDIR}/$$a; \
		ln -s ${.CURDIR}/$$a ${DDIR}; \
	done

.include <bsd.prog.mk>
