# Makefile for libopie
#
# $Id: Makefile,v 1.3 1997/03/08 19:42:41 pst Exp $
#
OPIE_DIST?=	${.CURDIR}/../../contrib/opie
DIST_DIR=	${OPIE_DIST}/${.CURDIR:T}

KEYFILE?=	\"/etc/opiekeys\"
EXTFILE?=	\"/etc/opiekeys.ext\"

.PATH: ${DIST_DIR} ${OPIE_DIST}/libmissing

LIB=	opie
SRCS=	atob8.c btoa8.c btoh.c challenge.c getsequence.c hash.c hashlen.c \
	keycrunch.c lock.c lookup.c newseed.c parsechallenge.c passcheck.c \
	passwd.c randomchallenge.c readpass.c unlock.c verify.c version.c \
	btoe.c accessfile.c generator.c insecure.c getutmpentry.c \
	readrec.c writerec.c login.c open.c logwtmp.c \
	getutline.c pututline.c endutent.c 	# from libmissing

CFLAGS+=-I${.CURDIR} -I${OPIE_DIST} -I${DIST_DIR} \
	-DSTD_KEY_FILE=${KEYFILE} -DEXT_KEY_FILE=${EXTFILE}

MAN4=	${OPIE_DIST}/opie.4
MAN5=	${OPIE_DIST}/opiekeys.5 ${OPIE_DIST}/opieaccess.5

beforeinstall:
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 ${OPIE_DIST}/opie.h \
			${DESTDIR}/usr/include

.include <bsd.lib.mk>
