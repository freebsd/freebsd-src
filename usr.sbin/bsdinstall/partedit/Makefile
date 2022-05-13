# $FreeBSD$

BINDIR= ${LIBEXECDIR}/bsdinstall
PROG=	partedit
LINKS= ${BINDIR}/partedit ${BINDIR}/autopart \
       ${BINDIR}/partedit ${BINDIR}/scriptedpart
SYMLINKS= ../libexec/bsdinstall/partedit /usr/sbin/sade
CFLAGS+=	-I${SRCTOP}/contrib/bsddialog/lib -I${.OBJDIR}/..
LIBADD+=	geom util bsddialog

PARTEDIT_ARCH= ${MACHINE}
.if ${MACHINE} == "i386" || ${MACHINE} == "amd64"
PARTEDIT_ARCH= x86
.elif ${MACHINE} == "arm64" || ${MACHINE} == "riscv"
PARTEDIT_ARCH= efi
.endif
.if !exists(partedit_${PARTEDIT_ARCH}.c)
PARTEDIT_ARCH= generic
.endif

SRCS=	diskmenu.c partedit.c gpart_ops.c partedit_${PARTEDIT_ARCH}.c \
	part_wizard.c scripted.c

WARNS?=	3
MAN= sade.8

.include <bsd.prog.mk>
