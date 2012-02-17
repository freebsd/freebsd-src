# $FreeBSD$

.include	"../../Makefile.inc"

LIB=		ctf
SHLIB_MAJOR=	1

SRCS=		ctf_create.c \
		ctf_decl.c \
		ctf_error.c \
		ctf_hash.c \
		ctf_labels.c \
		ctf_lib.c \
		ctf_lookup.c \
		ctf_open.c \
		ctf_subr.c \
		ctf_types.c \
		ctf_util.c

.PATH:		${OPENSOLARIS_USR_DISTDIR}/common/ctf
.PATH:		${OPENSOLARIS_USR_DISTDIR}/lib/libctf/common
.PATH:		${OPENSOLARIS_SYS_DISTDIR}/common/ctf

CFLAGS+=	-DCTF_OLD_VERSIONS

CFLAGS+=	-I${.CURDIR}/../../../sys/cddl/compat/opensolaris \
		-I${.CURDIR}/../../../cddl/compat/opensolaris/include \
		-I${OPENSOLARIS_USR_DISTDIR}/head \
		-I${OPENSOLARIS_USR_DISTDIR}/common/ctf \
		-I${OPENSOLARIS_USR_DISTDIR}/lib/libctf/common \
		-I${OPENSOLARIS_SYS_DISTDIR}/uts/common

.include <bsd.lib.mk>

