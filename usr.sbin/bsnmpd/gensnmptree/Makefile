# $FreeBSD: src/usr.sbin/bsnmpd/gensnmptree/Makefile,v 1.4.10.1.8.1 2012/03/03 06:15:13 kensmith Exp $
#
# Author: Harti Brandt <harti@freebsd.org>

CONTRIB=${.CURDIR}/../../../contrib/bsnmp
.PATH: ${CONTRIB}/gensnmptree

PROG=	gensnmptree
CFLAGS+= -I${CONTRIB}/lib
CFLAGS+= -DQUADFMT='"llu"' -DQUADXFMT='"llx"' -DHAVE_STDINT_H
CFLAGS+= -DHAVE_INTTYPES_H

.include <bsd.prog.mk>
