#	$Id: Makefile,v 1.1 1994/11/08 03:58:48 phk Exp $
#
# Doing a make install builds /usr/share/examples

DDIR=	${DESTDIR}/usr/share/FAQ
NOOBJ=	noobj

# Define SHARED to indicate whether you want symbolic links to the system
# source (``symlinks''), or a separate copy (``copies''); (latter useful
# in environments where it's not possible to keep /sys publicly readable)
SHARED?=	copies

all clean cleandir depend lint tags:

beforeinstall: ${SHARED}

copies:
	@${ECHO} installing ${DDIR}
	rm -rf ${DDIR}
	mkdir ${DDIR}
	find . -print | grep -v /CVS | grep -v Makefile | cpio -dumpv ${DDIR}

symlinks:
	@${ECHO} installing symlink to ${DDIR}
	rm -rf ${DDIR}; \
	ln -s ${.CURDIR} ${DDIR}; \

.include <bsd.prog.mk>
