# $FreeBSD$

.if !defined(BURN_BRIDGES)
.for oldnew in \
	NOATM:NO_ATM \
	NOLIBC_R:NO_LIBC_R \
	NOLIBPTHREAD:NO_LIBPTHREAD \
	NOLIBTHR:NO_LIBTHR \
	NOMAN:NO_MAN \
	NOMANCOMPRESS:NO_MANCOMPRESS \
	NOOBJ:NO_OBJ
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
