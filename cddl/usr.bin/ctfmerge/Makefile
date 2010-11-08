# $FreeBSD$

.include <bsd.own.mk>

.include	"../../Makefile.inc"

WARNS=		1

PROG=		ctfmerge
SRCS=		alist.c \
		barrier.c \
		ctf.c \
		ctfmerge.c \
		fifo.c \
		hash.c \
		iidesc.c \
		input.c \
		list.c \
		memory.c \
		merge.c \
		output.c \
		strtab.c \
		symbol.c \
		tdata.c \
		traverse.c \
		util.c

WARNS?=		6

CFLAGS+=	-I${.CURDIR}/../../../sys/cddl/compat/opensolaris \
		-I${.CURDIR}/../../../cddl/compat/opensolaris/include \
		-I${OPENSOLARIS_USR_DISTDIR} \
		-I${OPENSOLARIS_SYS_DISTDIR} \
		-I${OPENSOLARIS_USR_DISTDIR}/head \
		-I${OPENSOLARIS_USR_DISTDIR}/tools/ctf/common \
		-I${OPENSOLARIS_USR_DISTDIR}/tools/ctf/cvt \
		-I${OPENSOLARIS_SYS_DISTDIR}/uts/common

LDADD+=		-lctf -ldwarf -lelf -lz -lthr

.PATH:		${OPENSOLARIS_USR_DISTDIR}/tools/ctf/common
.PATH:		${OPENSOLARIS_USR_DISTDIR}/tools/ctf/cvt

.include <bsd.prog.mk>
