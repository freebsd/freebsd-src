# $FreeBSD$
#
# Doing a make install builds /usr/share/sendmail/

MAINTAINER=	gshapiro@FreeBSD.org

SENDMAIL_DIR=${.CURDIR}/../../contrib/sendmail
CFDIR=	cf
CFDIRS!=	(cd ${SENDMAIL_DIR}; find -L ${CFDIR} \( -name CVS -prune \) -o -type d -print)
CFFILES!=	(cd ${SENDMAIL_DIR}; find -L ${CFDIR} \( -name CVS -prune \) -o -type f -print)

DDIR=	${DESTDIR}/usr/share/sendmail

NOOBJ=	noobj

# Define SHARED to indicate whether you want symbolic links to the system
# source (``symlinks''), or a separate copy (``copies'')
SHARED?=	copies

all clean cleandir depend lint tags:

beforeinstall: ${SHARED}

copies::
.for dir in ${CFDIRS}
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m 755 -d ${DDIR}/${dir}
.endfor
.for file in ${CFFILES}
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 444 ${SENDMAIL_DIR}/${file} ${DDIR}/${file}
.endfor

symlinks::
	rm -rf ${DDIR}/${CFDIR}; ln -s ${SENDMAIL_DIR}/${CFDIR} ${DDIR}/${CFDIR}

.include <bsd.prog.mk>
