# $FreeBSD$
#
# Author: Harti Brandt <harti@freebsd.org>
#
LIB=	ngatm
SHLIB_MAJOR= 1
MAN=	unimsg.3
WARNS=	6

# source of the library lives in contrib
SDIR=	${.CURDIR}/../../sys
CTRB=	${.CURDIR}/../../contrib/ngatm
LIBBASE= ${SDIR}/contrib/ngatm

CFLAGS+= -I${LIBBASE} -I${.OBJDIR} -I${CTRB}/libngatm
# CFLAGS+= -DSSCOP_DEBUG -DSSCFU_DEBUG -DUNI_DEBUG

.PATH: ${LIBBASE}/netnatm ${LIBBASE}/netnatm/saal ${LIBBASE}/netnatm/misc
.PATH: ${CTRB}/libngatm ${CTRB}/man

SRCS=	unimsg.c unimsg_common.c					\
	saal_sscop.c saal_sscfu.c

# Includes
INCSGROUPS= INCSATM INCSSAAL

# common files
INCSATMDIR= $(INCLUDEDIR)/netnatm
INCSATM= unimsg.h

# signaling AAL
INCSSAALDIR= $(INCLUDEDIR)/netnatm/saal
INCSSAAL= saal/sscfu.h saal/sscfudef.h saal/sscop.h saal/sscopdef.h

.include <bsd.lib.mk>
