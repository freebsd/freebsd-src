# $FreeBSD$

DOC_PREFIX?= ${RELN_ROOT}/../../../doc

# XXX using /release/doc as anchor!
DESTDIR?= ${DOCDIR}/${.CURDIR:C/^.*\/release\/doc//}
