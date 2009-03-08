# $FreeBSD$

.PATH: ${.CURDIR}/${MACHINE_ARCH}/string ${.CURDIR}/string

CFLAGS += -I${.CURDIR}/locale

# machine-independent string sources
MISRCS += \
	bcmp.c \
	bcopy.c \
	bzero.c \
	ffs.c \
	ffsl.c \
	ffsll.c \
	fls.c \
	flsl.c \
	flsll.c \
	index.c \
	memccpy.c \
	memchr.c \
	memcmp.c \
	memcpy.c \
	memmem.c \
	memmove.c \
	memrchr.c \
	memset.c \
	rindex.c \
	stpcpy.c \
	stpncpy.c \
	strcasecmp.c \
	strcasestr.c \
	strcat.c \
	strchr.c \
	strcmp.c \
	strcoll.c \
	strcpy.c \
	strcspn.c \
	strdup.c \
	strerror.c \
	strlcat.c \
	strlcpy.c \
	strlen.c \
	strmode.c \
	strncat.c \
	strncmp.c \
	strncpy.c \
	strndup.c \
	strnlen.c \
	strnstr.c \
	strpbrk.c \
	strrchr.c \
	strsep.c \
	strsignal.c \
	strspn.c \
	strstr.c \
	strtok.c \
	strxfrm.c \
	swab.c \
	wcscasecmp.c \
	wcscat.c \
	wcschr.c \
	wcscmp.c \
	wcscoll.c \
	wcscpy.c \
	wcscspn.c \
	wcsdup.c \
	wcslcat.c \
	wcslcpy.c \
	wcslen.c \
	wcsncasecmp.c \
	wcsncat.c \
	wcsncmp.c \
	wcsncpy.c \
	wcsnlen.c \
	wcspbrk.c \
	wcsrchr.c \
	wcsspn.c \
	wcsstr.c \
	wcstok.c \
	wcswidth.c \
	wcsxfrm.c \
	wmemchr.c \
	wmemcmp.c \
	wmemcpy.c \
	wmemmove.c \
	wmemset.c

SYM_MAPS += ${.CURDIR}/string/Symbol.map

# machine-dependent string sources
.if exists(${.CURDIR}/${MACHINE_ARCH}/string/Buildfile.inc)
.include "${.CURDIR}/${MACHINE_ARCH}/string/Buildfile.inc"
.endif

