#	@(#)Makefile	8.1 (Berkeley) 6/5/93
# $FreeBSD$

.include <bsd.own.mk>

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
	${_zoneinfo}

.if ${MK_LOCALES} != "no"
_colldef =	colldef
_mklocale =	mklocale
_monetdef =	monetdef
_msgdef =	msgdef
_numericdef =	numericdef
_timedef =	timedef
.endif

.if ${MK_SYSCONS} != "no"
_syscons=	syscons
.endif

.if ${MK_MAN} != "no"
_man=		man
.endif

.if ${MK_GROFF} != "no"
_me=		me
.endif

.if ${MK_EXAMPLES} != "no"
_examples=	examples
.endif

.if ${MK_I4B} != "no"
_isdn=		isdn
.endif

.if ${MK_DICT} != "no"
_dict=	 	dict
.endif

.if ${MK_SENDMAIL} != "no"
_sendmail=	 sendmail
.endif

.if ${MK_SHAREDOCS} != "no"
_doc=		doc
.endif

.if ${MK_ZONEINFO} != "no"
_zoneinfo=	zoneinfo
.endif

.include <bsd.subdir.mk>
