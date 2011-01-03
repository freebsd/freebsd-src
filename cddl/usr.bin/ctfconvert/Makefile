# $FreeBSD$

.PATH: ${.CURDIR}/../../../cddl/contrib/opensolaris/tools/ctf/common
.PATH: ${.CURDIR}/../../../cddl/contrib/opensolaris/tools/ctf/cvt

DEBUG_FLAGS=	-g

PROG=		ctfconvert
SRCS=		alist.c \
		ctf.c \
		ctfconvert.c \
		dwarf.c \
		fixup_tdescs.c \
		hash.c \
		iidesc.c \
		input.c \
		list.c \
		memory.c \
		merge.c \
		output.c \
		st_parse.c \
		stabs.c \
		stack.c \
		strtab.c \
		symbol.c \
		tdata.c \
		traverse.c \
		util.c

CFLAGS+=	-I${.CURDIR}/../../../sys/cddl/compat/opensolaris \
		-I${.CURDIR}/../../../cddl/compat/opensolaris/include \
		-I${OPENSOLARIS_USR_DISTDIR} \
		-I${OPENSOLARIS_SYS_DISTDIR} \
		-I${OPENSOLARIS_USR_DISTDIR}/head \
		-I${OPENSOLARIS_USR_DISTDIR}/tools/ctf/common \
		-I${OPENSOLARIS_USR_DISTDIR}/tools/ctf/cvt \
		-I${OPENSOLARIS_SYS_DISTDIR}/uts/common

DPADD=		${LIBCTF} ${LIBDWARF} ${LIBELF} ${LIBZ} ${LIBPTHREAD}
LDADD=		-lctf -ldwarf -lelf -lz -lpthread

.include <bsd.prog.mk>
