#	$Id: Makefile,v 1.11.4.6 1996/06/05 02:52:27 jkh Exp $

LIB=	disk
SRCS=	blocks.c disklabel.c dkcksum.c chunk.c disk.c change.c \
	create_chunk.c rules.c write_disk.c data.c

CFLAGS+= 	-Wall
CLEANFILES+=	tmp.c tst01 tst01.o data.c
VPATH=		${.CURDIR}/../../sbin/disklabel
NOPROFILE=	yes
NOSHARED=	yes
NOPIC=		yes

MAN3=		libdisk.3
MLINKS+=	libdisk.3 Open_Disk.3 \
		libdisk.3 Clone_Disk.3 \
		libdisk.3 Free_Disk.3 \
		libdisk.3 Debug_Disk.3 \
		libdisk.3 Set_Bios_Geom.3 \
		libdisk.3 Delete_Chunk.3 \
		libdisk.3 Collapse_Disk.3 \
		libdisk.3 Collapse_Chunk.3 \
		libdisk.3 Create_Chunk.3 \
		libdisk.3 All_FreeBSD.3 \
		libdisk.3 CheckRules.3 \
		libdisk.3 Disk_Names.3 \
		libdisk.3 Set_Boot_Mgr.3 \
		libdisk.3 Set_Boot_Blocks.3 \
		libdisk.3 Write_Disk.3 \
		libdisk.3 Cyl_Aligned.3 \
		libdisk.3 Next_Cyl_Aligned.3 \
		libdisk.3 Prev_Cyl_Aligned.3 \
		libdisk.3 Track_Aligned.3 \
		libdisk.3 Next_Track_Aligned.3 \
		libdisk.3 Prev_Track_Aligned.3 \
		libdisk.3 Create_Chunk_DWIM.3 \
		libdisk.3 MakeDev.3 \
		libdisk.3 MakeDevDisk.3 \
		libdisk.3 ShowChunkFlags.3 \
		libdisk.3 ChunkCanBeRoot.3

.include <bsd.lib.mk>

BOOTS=/usr/mdec

data.c:	${.CURDIR}/libdisk.h ${BOOTS}/boot1 ${BOOTS}/boot2
	file2c 'const unsigned char boot1[] = {' '};' \
                < ${BOOTS}/boot1 > tmp.c
	file2c 'const unsigned char boot2[] = {' '};' \
                < ${BOOTS}/boot2 >> tmp.c
	mv -f tmp.c data.c

beforeinstall:
	@${ECHO} installing libdisk.h
	@cd ${.CURDIR}; \
	cmp -s libdisk.h ${DESTDIR}/usr/include/libdisk.h || \
	    ${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 libdisk.h \
	    ${DESTDIR}/usr/include/libdisk.h


tst01: tst01.o libdisk.a
	cc ${CFLAGS} -static tst01.o -o tst01 libdisk.a
