MDSRCS+= \
	amd64_archlevel.c \
	bcmp.S \
	memchr.S \
	memcmp.S \
	memccpy.S \
	memcpy.S \
	memmove.S \
	memrchr.S \
	memset.S \
	stpcpy.S \
	stpncpy.S \
	strcat.S \
	strchrnul.S \
	strcmp.S \
	strcpy.c \
	strcspn.S \
	strlcat.c \
	strlcpy.S \
	strlen.S \
	strncat.c \
	strncmp.S \
	strncpy.c \
	strnlen.c \
	strpbrk.c \
	strrchr.S \
	strsep.c \
	strspn.S \
	timingsafe_bcmp.S \
	timingsafe_memcmp.S

.if ${MK_ASAN} != "no"
# Disable ASAN for amd64_archlevel.c since its code is executed before the
# sanitizer runtime can initialize itself.
CFLAGS.amd64_archlevel.c+=	-fno-sanitize=address
.endif
