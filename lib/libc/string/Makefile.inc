#	@(#)Makefile.inc	5.6 (Berkeley) 3/5/91

# string sources
.PATH: ${.CURDIR}/${MACHINE}/string ${.CURDIR}/string

SRCS+=	memccpy.c strcasecmp.c strcoll.c strdup.c \
	strftime.c strmode.c strtok.c strxfrm.c swab.c

.if   (${MACHINE} == "hp300")
SRCS+=	bcmp.s bcopy.s bzero.s ffs.s index.s memchr.c memcmp.c memset.c \
	rindex.s strcat.c strcmp.s strcpy.s strcspn.c strlen.s \
	strncat.c strncmp.s strncpy.s strpbrk.c strsep.c \
	strspn.c strstr.c
.elif (${MACHINE} == "i386")
SRCS+=	bcmp.s bcopy.s bzero.s ffs.s index.s memchr.s memcmp.s memset.s \
	rindex.s strcat.s strcmp.s strcpy.s strcspn.c strlen.s \
	strncat.c strncmp.s strncpy.c strpbrk.c strsep.c \
	strspn.c strstr.c
SRCS+=	memmove.s strchr.s strrchr.s
.elif (${MACHINE} == "tahoe")
SRCS+=	bcmp.s bcopy.s bzero.s ffs.s index.c memchr.c memcmp.s memset.c \
	rindex.c strcat.s strcmp.s strcpy.s strcspn.c strlen.s \
	strncat.s strncmp.s strncpy.s strpbrk.c strsep.c \
	strspn.c strstr.c
SRCS+=	memmove.s
.elif (${MACHINE} == "vax")
SRCS+=	bcmp.s bcopy.s bzero.s ffs.s index.c memchr.s memcmp.s memset.s \
	rindex.s strcat.s strcmp.s strcpy.s strcspn.s strlen.s \
	strncat.s strncmp.s strncpy.s strpbrk.s strsep.s \
	strspn.s strstr.s
SRCS+=	memmove.s strchr.s strrchr.s
.endif

# if no machine specific memmove(3), build one out of bcopy(3).
.if empty(SRCS:Mmemcpy.s)
.if empty(SRCS:Mmemmove.s)
OBJS+=	memmove.o
memmove.o: bcopy.c
	${CC} -DMEMMOVE ${CFLAGS} -c ${.ALLSRC} -o ${.TARGET}
	@${LD} -x -r ${.TARGET}
	@mv a.out ${.TARGET}

memmove.po: bcopy.c
	${CC} -DMEMMOVE ${CFLAGS} -c -p ${.ALLSRC} -o ${.TARGET}
	@${LD} -X -r ${.TARGET}
	@mv a.out ${.TARGET}
.endif
.endif

# if no machine specific strchr(3), build one out of index(3).
.if empty(SRCS:Mstrchr.s)
OBJS+=	strchr.o
strchr.o: index.c
	${CC} -DSTRCHR ${CFLAGS} -c ${.ALLSRC} -o ${.TARGET}
	@${LD} -x -r ${.TARGET}
	@mv a.out ${.TARGET}

strchr.po: index.c
	${CC} -DSTRCHR ${CFLAGS} -c -p ${.ALLSRC} -o ${.TARGET}
	@${LD} -X -r ${.TARGET}
	@mv a.out ${.TARGET}
.endif

# if no machine specific strrchr(3), build one out of rindex(3).
.if empty(SRCS:Mstrrchr.s)
OBJS+=	strrchr.o
strrchr.o: rindex.c
	${CC} -DSTRRCHR ${CFLAGS} -c ${.ALLSRC} -o ${.TARGET}
	@${LD} -x -r ${.TARGET}
	@mv a.out ${.TARGET}

strrchr.po: rindex.c
	${CC} -DSTRRCHR ${CFLAGS} -c -p ${.ALLSRC} -o ${.TARGET}
	@${LD} -X -r ${.TARGET}
	@mv a.out ${.TARGET}
.endif

MAN3+=	string/bcmp.3 string/bcopy.3 string/bstring.3 string/bzero.3 \
	string/ffs.3 string/index.3 string/memccpy.3 string/memchr.3 \
	string/memcmp.3 string/memcpy.3 string/memmove.3 string/memset.3 \
	string/rindex.3 string/strcasecmp.3 string/strcat.3 \
	string/strchr.3 string/strcmp.3 string/strcoll.3 string/strcpy.3 \
	string/strcspn.3 string/strftime.3 string/string.3 \
	string/strlen.3 string/strmode.3 string/strdup.3 string/strpbrk.3 \
	string/strrchr.3 string/strsep.3 string/strspn.3 \
	string/strstr.3 string/strtok.3 string/strxfrm.3 string/swab.3

MLINKS+=strcasecmp.3 strncasecmp.3
MLINKS+=strcat.3 strncat.3
MLINKS+=strcmp.3 strncmp.3
MLINKS+=strcpy.3 strncpy.3
