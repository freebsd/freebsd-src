#	@(#)Makefile	8.1 (Berkeley) 6/5/93
# $FreeBSD$

# Do not include `info' in the SUBDIR list, it is handled separately.

SUBDIR=	${_colldef} \
	${_dict} \
	${_doc} \
	${_examples} \
	${_isdn} \
	${_man} \
	${_me} \
	misc \
	mk \
	${_mklocale} \
	${_monetdef} \
	${_msgdef} \
	${_numericdef} \
	${_sendmail} \
	skel \
	snmp \
	${_syscons} \
	tabset \
	termcap \
	${_timedef} \
	zoneinfo

.if !defined(NO_LOCALES)
_colldef =	colldef
_mklocale =	mklocale
_monetdef =	monetdef
_msgdef =	msgdef
_numericdef =	numericdef
_timedef =	timedef
.endif

.if !defined(NO_SYSCONS)
_syscons=	syscons
.endif

.if !defined(NO_MAN)
_man=		man
.endif

.if !defined(NO_GROFF)
_me=		me
.endif

.if !defined(NO_EXAMPLES)
_examples=	examples
.endif

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
