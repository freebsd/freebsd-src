#	$Id: Makefile,v 1.16 1998/01/10 22:50:00 jb Exp $

SUBDIR= libdialog libg++ libgmp libmp

.if ${MACHINE_ARCH} == "i386"
SUBDIR+= libstdc++
.endif

SUBDIR+= libobjc libregex libreadline

.include <bsd.subdir.mk>
