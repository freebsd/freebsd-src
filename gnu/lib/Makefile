#	$Id: Makefile,v 1.15 1997/02/22 15:42:33 peter Exp $

SUBDIR= libdialog libg++

.if ${MACHINE} != "alpha"
SUBDIR+= libgmp libmp
.endif

SUBDIR+= libobjc libregex libreadline libstdc++

.include <bsd.subdir.mk>
