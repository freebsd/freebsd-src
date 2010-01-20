# Makefile for libopie
#
# $FreeBSD$
#
OPIE_DIST?=	${.CURDIR}/../../contrib/opie
DIST_DIR=	${OPIE_DIST}/${.CURDIR:T}
SHLIB_MAJOR=    4

KEYFILE?=	\"/etc/opiekeys\"

.PATH: ${DIST_DIR} ${OPIE_DIST}/libmissing

LIB=	opie
SRCS=	atob8.c btoa8.c btoh.c challenge.c getsequence.c hash.c hashlen.c \
	keycrunch.c lock.c lookup.c newseed.c parsechallenge.c passcheck.c \
	passwd.c randomchallenge.c readpass.c unlock.c verify.c version.c \
	btoe.c accessfile.c generator.c insecure.c getutmpentry.c \
	readrec.c writerec.c open.c \
	getutline.c pututline.c endutent.c setutent.c # from libmissing
SRCS+=	opieextra.c
INCS=	${OPIE_DIST}/opie.h

CFLAGS+=-I${.CURDIR} -I${OPIE_DIST} -I${DIST_DIR} \
	-DKEY_FILE=${KEYFILE}

ACCESSFILE?=	\"/etc/opieaccess\"
CFLAGS+=	-DINSECURE_OVERRIDE -DPATH_ACCESS_FILE=${ACCESSFILE}

DPADD=	${LIBMD}
LDADD=	-lmd

MAN=	${OPIE_DIST}/opie.4 ${OPIE_DIST}/opiekeys.5 ${OPIE_DIST}/opieaccess.5

MLINKS=	opie.4 skey.4

.include <bsd.lib.mk>
