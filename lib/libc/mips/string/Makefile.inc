#	$NetBSD: Makefile.inc,v 1.2 2000/10/10 21:51:54 jeffs Exp $
# $FreeBSD$

SRCS+=	bcmp.S bcopy.S bzero.S ffs.S memchr.c memcmp.c memset.c \
	memcpy.S memmove.S \
	strcat.c strchr.S strcmp.S strcpy.c strcspn.c strlen.S \
	strncat.c strncmp.c strncpy.c strrchr.S strpbrk.c strsep.c \
	strspn.c strstr.c swab.c

.if ${MK_CHERI} != "no"
_CHERI_CC=	${CHERI_CC} -integrated-as --target=cheri-unknown-freebsd \
		-msoft-float
.if defined(SYSROOT)
_CHERI_CC+=	--sysroot=${SYSROOT}
.endif
SRCS+=	memcpy_c.s memcpy_c_tofrom.c memmove_c.c memset_c.c	\
	strnlen_c.c strncpy_c.c
CC.memcpy_c.s=	${_CHERI_CC}
CC.memcpy_c_tofrom.c=	${_CHERI_CC}
CC.memmove_c.c=	${_CHERI_CC}
CC.memset_c.c=	${_CHERI_CC}
CC.strnlen_c.c=	${_CHERI_CC}
CC.strncpy_c.c=	${_CHERI_CC}

SYM_MAPS+=	${LIBC_SRCTOP}/mips/string/Symbol.map
.endif
