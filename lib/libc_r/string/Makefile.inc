#	@(#)Makefile.inc	8.1 (Berkeley) 6/4/93
#	$FreeBSD$

.PATH: ${.CURDIR}/../libc/${MACHINE}/string ${.CURDIR}/../libc/string

CFLAGS += -I${.CURDIR}/../libc/locale
# machine-independent string sources
SRCS+=	memccpy.c strcasecmp.c strcoll.c strdup.c strerror.c \
	strmode.c strtok.c strxfrm.c

# machine-dependent string sources
.include "${.CURDIR}/${MACHINE}/string/Makefile.inc"

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
