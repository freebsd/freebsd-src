# $FreeBSD$

MDSRCS+= \
	ffs.S \
	strlen.S

.if !defined(LIBCHERI)
MDSRCS+= \
	bcmp.S \
	bzero.S \
	strchr.S \
	strcmp.S \
	strrchr.S
.endif

.if ${MK_CHERI} == "no"
MDSRCS+= \
	bcopy.S \
	memcpy.S \
	memmove.S
.else
CHERI_MDSRCS=	\
		memchr_c.c \
		memcmp_c.c \
		memcpy_c.S \
		memcpy_c_tofrom.c \
		memmove_c.c \
		memset_c.c \
		strchr_c.c \
		strcmp_c.c \
		strnlen_c.c \
		strncmp_c.c \
		strncpy_c.c \
		strtol_c.c
CHERI_MISRCS=	\
		bcopy.c \
		memcpy.c \
		memmove.c \

MDSRCS+=	${CHERI_MDSRCS}

.for src in ${CHERI_MDSRCS} ${CHERI_MISRCS}
CC.${src}=	${_CHERI_CC}
.endfor

SYM_MAPS+=	${LIBC_SRCTOP}/mips/string/Symbol.map
.endif
