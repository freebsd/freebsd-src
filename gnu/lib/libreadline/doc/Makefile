#       $Id: Makefile,v 1.8 1997/01/05 16:19:35 ache Exp $

SRCDIR= ${.CURDIR}/../../../../contrib/libreadline/doc

INFO= history readline

CLEANFILES= hist.info rlman.info

hist.info: hstech.texinfo hsuser.texinfo
rlman.info: rltech.texinfo rluser.texinfo

history.info: hist.info
	ln -f $> $@

readline.info: rlman.info
	ln -f $> $@

.include <bsd.info.mk>
