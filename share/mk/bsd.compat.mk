# $FreeBSD$

.if !defined(BURN_BRIDGES)
.for oldnew in \
	NOATM:NO_ATM \
	NODOCCOMPRESS:NO_DOCCOMPRESS \
	NOEXTRADEPEND:NO_EXTRADEPEND \
	NOINFO:NO_INFO \
	NOINFOCOMPRESS:NO_INFOCOMPRESS \
	NOINSTALLLIB:NO_INSTALLLIB \
	NOLIBC_R:NO_LIBC_R \
	NOLIBPTHREAD:NO_LIBPTHREAD \
	NOLIBTHR:NO_LIBTHR \
	NOLINT:NO_LINT \
	NOMAN:NO_MAN \
	NOMANCOMPRESS:NO_MANCOMPRESS \
	NOMLINKS:NO_MLINKS \
	NOOBJ:NO_OBJ \
	NOPIC:NO_PIC \
	NOPROFILE:NO_PROFILE \
	NOTAGS:NO_TAGS
.for old in ${oldnew:C/:.*//}
.for new in ${oldnew:C/.*://}
.if defined(${old}) && !defined(${new})
.warning ${old} is deprecated in favor of ${new}
${new}=	${${old}}
.endif
.endfor
.endfor
.endfor
.endif
