#       $Id: Makefile,v 1.23 1996/08/30 18:33:31 ache Exp $

SRCDIR= ${.CURDIR}/../../../../contrib/libreadline/doc

INFO= history readline

CLEANFILES= hist.info rlman.info

hist.info: hstech.texinfo hsuser.texinfo
rlman.info: rltech.texinfo rluser.texinfo

history.info: hist.info
	ln $> $@

readline.info: rlman.info
	ln $> $@

.include <bsd.info.mk>
