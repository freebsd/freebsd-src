# $FreeBSD$
#
# Author: Harti Brandt <harti@freebsd.org>
#
LIB=	ngatm
SHLIB_MAJOR= 1
MAN=	libngatm.3 uniaddr.3 unifunc.3 unimsg.3 unistruct.3
WARNS=	6

# source of the library lives in contrib
SDIR=	${.CURDIR}/../../sys
CTRB=	${.CURDIR}/../../contrib/ngatm
LIBBASE= ${SDIR}/contrib/ngatm

CFLAGS+= -I${LIBBASE} -I${.OBJDIR} -I${CTRB}/libngatm
# CFLAGS+= -DSSCOP_DEBUG -DSSCFU_DEBUG -DUNI_DEBUG

.PATH: ${LIBBASE}/netnatm ${LIBBASE}/netnatm/saal ${LIBBASE}/netnatm/misc \
	${LIBBASE}/netnatm/msg
.PATH: ${CTRB}/libngatm ${CTRB}/man

SRCS=	unimsg.c unimsg_common.c straddr.c				\
	traffic.c uni_ie.c uni_msg.c					\
	saal_sscop.c saal_sscfu.c

# Includes
INCSGROUPS= INCSATM INCSSAAL INCSMSG

# common files
INCSATMDIR= $(INCLUDEDIR)/netnatm
INCSATM= unimsg.h addr.h

# signaling AAL
INCSSAALDIR= $(INCLUDEDIR)/netnatm/saal
INCSSAAL= saal/sscfu.h saal/sscfudef.h saal/sscop.h saal/sscopdef.h

# message parsing
INCSMSGDIR= $(INCLUDEDIR)/netnatm/msg
INCSMSG= msg/uni_config.h msg/uni_hdr.h msg/uni_ie.h msg/uni_msg.h	\
	 msg/unimsglib.h msg/uniprint.h msg/unistruct.h

.include <bsd.lib.mk>
