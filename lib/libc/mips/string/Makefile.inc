#	$NetBSD: Makefile.inc,v 1.2 2000/10/10 21:51:54 jeffs Exp $
# $FreeBSD$

SRCS+=	ffs.S memchr.c memcmp.c memset.c \
	strcat.c strcpy.c strcspn.c strlen.S \
	strncat.c strncmp.c strncpy.c strpbrk.c strsep.c \
	strspn.c strstr.c swab.c

.if !defined(LIBCHERI)
SRCS+=	bcmp.S \
	bzero.S \
	strchr.S \
	strcmp.S \
	strrchr.S
.else
SRCS+=	bcmp.c \
	bzero.c \
	strchr.c \
	strcmp.c \
	strrchr.c
.endif

.if ${MK_CHERI} == "no"
SRCS+=	bcopy.S memcpy.S memmove.S
.else
CHERI_SRCS=	\
		bcopy.c \
		memchr_c.c \
		memcmp_c.c \
		memcpy.c \
		memcpy_c.S \
		memcpy_c_tofrom.c \
		memmove.c \
		memmove_c.c \
		memset_c.c \
		strchr_c.c \
		strcmp_c.c \
		strnlen_c.c \
		strncmp_c.c \
		strncpy_c.c \
		strtol_c.c

SRCS+=	${CHERI_SRCS}

.for src in ${CHERI_SRCS}
CC.${src}=	${_CHERI_CC}
.endfor

SYM_MAPS+=	${LIBC_SRCTOP}/mips/string/Symbol.map
.endif
