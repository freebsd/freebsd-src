#	@(#)Makefile	8.1 (Berkeley) 6/5/93
# $FreeBSD: src/share/Makefile,v 1.33.2.1 2007/08/26 15:57:06 remko Exp $

# Do not include `info' in the SUBDIR list, it is handled separately.

SUBDIR=	colldef \
	${_dict} \
	${_doc} \
	examples \
	${_isdn} \
	man \
	me \
	misc \
	mk \
	mklocale \
	monetdef \
	msgdef \
	numericdef \
	${_sendmail} \
	skel \
	snmp \
	syscons \
	tabset \
	termcap \
	timedef \
	${_zoneinfo}

.if !defined(NO_I4B)
_isdn=		isdn
.endif

.if !defined(NO_DICT)
_dict=	 	dict
.endif

.if !defined(NO_SENDMAIL)
_sendmail=	 sendmail
.endif

.if !defined(NO_SHAREDOCS)
_doc=		doc
.endif

.if !defined(NO_ZONEINFO)
_zoneinfo=	zoneinfo
.endif

.include <bsd.subdir.mk>
