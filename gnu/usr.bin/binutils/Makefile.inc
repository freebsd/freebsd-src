#
# $Id: Makefile.inc,v 1.1.1.1 1998/03/01 23:40:47 jdp Exp $
#

.if     ${MACHINE_ARCH} == "i386"
BINDIR?=	/usr/libexec/elf
.else
BINDIR?=	/usr/bin
.endif
