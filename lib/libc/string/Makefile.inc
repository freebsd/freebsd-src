#	@(#)Makefile.inc	8.1 (Berkeley) 6/4/93
#	$Id: Makefile.inc,v 1.9 1997/02/22 15:03:21 peter Exp $

.PATH: ${.CURDIR}/../libc/${MACHINE}/string ${.CURDIR}/../libc/string

CFLAGS += -I${.CURDIR}/../libc/locale
# machine-independent string sources
SRCS+=	memccpy.c strcasecmp.c strcoll.c strdup.c strerror.c \
	strmode.c strtok.c strxfrm.c

# machine-dependent string sources
.include "${.CURDIR}/../libc/${MACHINE}/string/Makefile.inc"

# If no machine specific bzero(3), build one out of memset(3).
.if empty(SRCS:Mbzero.S)
OBJS+=	bzero.o
bzero.o: memset.c
	${CC} -DBZERO ${CFLAGS} -c ${.ALLSRC} -o ${.TARGET}
	@${LD} -x -r ${.TARGET}
	@mv -f a.out ${.TARGET}

bzero.po: memset.c
	${CC} -DBZERO ${CFLAGS} -c -p ${.ALLSRC} -o ${.TARGET}
	@${LD} -X -r ${.TARGET}
	@mv -f a.out ${.TARGET}
.endif

# If no machine specific memmove(3), build one out of bcopy(3).
.if empty(SRCS:Mmemmove.S)
OBJS+=	memmove.o
memmove.o: bcopy.c
	${CC} -DMEMMOVE ${CFLAGS} -c ${.ALLSRC} -o ${.TARGET}
	@${LD} -x -r ${.TARGET}
	@mv -f a.out ${.TARGET}

memmove.po: bcopy.c
	${CC} -DMEMMOVE ${CFLAGS} -c -p ${.ALLSRC} -o ${.TARGET}
	@${LD} -X -r ${.TARGET}
	@mv -f a.out ${.TARGET}
.endif

# If no machine specific memcpy(3), build one out of bcopy(3).
.if empty(SRCS:Mmemmove.S)
OBJS+=	memcpy.o
memcpy.o: bcopy.c
	${CC} -DMEMCOPY ${CFLAGS} -c ${.ALLSRC} -o ${.TARGET}
	@${LD} -x -r ${.TARGET}
	@mv -f a.out ${.TARGET}

memcpy.po: bcopy.c
	${CC} -DMEMCOPY ${CFLAGS} -c -p ${.ALLSRC} -o ${.TARGET}
	@${LD} -X -r ${.TARGET}
	@mv -f a.out ${.TARGET}
.endif

# If no machine specific strchr(3), build one out of index(3).
.if empty(SRCS:Mstrchr.S)
OBJS+=	strchr.o
strchr.o: index.c
	${CC} -DSTRCHR ${CFLAGS} -c ${.ALLSRC} -o ${.TARGET}
	@${LD} -x -r ${.TARGET}
	@mv -f a.out ${.TARGET}

strchr.po: index.c
	${CC} -DSTRCHR ${CFLAGS} -c -p ${.ALLSRC} -o ${.TARGET}
	@${LD} -X -r ${.TARGET}
	@mv -f a.out ${.TARGET}
.endif

# If no machine specific strrchr(3), build one out of rindex(3).
.if empty(SRCS:Mstrrchr.S)
OBJS+=	strrchr.o
strrchr.o: rindex.c
	${CC} -DSTRRCHR ${CFLAGS} -c ${.ALLSRC} -o ${.TARGET}
	@${LD} -x -r ${.TARGET}
	@mv -f a.out ${.TARGET}

strrchr.po: rindex.c
	${CC} -DSTRRCHR ${CFLAGS} -c -p ${.ALLSRC} -o ${.TARGET}
	@${LD} -X -r ${.TARGET}
	@mv -f a.out ${.TARGET}
.endif
#
# Only build man pages with libc.
.if ${LIB} == "c"
MAN3+=	string/bcmp.3 string/bcopy.3 string/bstring.3 string/bzero.3 \
	string/ffs.3 string/index.3 string/memccpy.3 string/memchr.3 \
	string/memcmp.3 string/memcpy.3 string/memmove.3 string/memset.3 \
	string/rindex.3 string/strcasecmp.3 string/strcat.3 string/strchr.3 \
	string/strcmp.3 string/strcoll.3 string/strcpy.3 string/strcspn.3 \
	string/string.3 string/strlen.3 string/strmode.3 \
	string/strdup.3 string/strerror.3 string/strpbrk.3 string/strrchr.3 \
	string/strsep.3 string/strspn.3 string/strstr.3 string/strtok.3 \
	string/strxfrm.3 string/swab.3
#
MLINKS+=strcasecmp.3 strncasecmp.3
MLINKS+=strcat.3 strncat.3
MLINKS+=strcmp.3 strncmp.3
MLINKS+=strcpy.3 strncpy.3
MLINKS+=strerror.3 perror.3 strerror.3 sys_errlist.3 strerror.3 sys_nerr.3
.endif
