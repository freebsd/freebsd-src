#	@(#)Makefile	8.1 (Berkeley) 6/5/93
# $FreeBSD$

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
	zoneinfo

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

.include <bsd.subdir.mk>
