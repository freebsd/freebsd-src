# $Id: Makefile,v 1.7 1998/05/04 05:12:49 peter Exp $

USE_LIBBIND=	true
.include "${.CURDIR}/../../usr.sbin/named/Makefile.inc"

.PATH:	${BIND_DIR}/bin/named-xfer
.PATH:	${BIND_DIR}/bin/named
.PATH:	${BIND_DIR}/doc/man

PROG=	named-xfer
SRCS=	pathnames.h \
	named-xfer.c db_glue.c ns_glue.c tmp_version.c 
MAN8=	named-xfer.8

.include <bsd.prog.mk>
