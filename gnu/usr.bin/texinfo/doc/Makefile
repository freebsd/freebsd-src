#	$Id$

INFO=	info-stnd info texinfo

MAKEINFO= ../makeinfo/makeinfo

CLEANFILES+=    ${.OBJDIR}/texinfo.texi

texinfo.texi: texinfo.txi
	@ln -sf ${.OODATE} ${.TARGET}

.include <bsd.info.mk>

.PATH: ${TXIDIR}/doc
