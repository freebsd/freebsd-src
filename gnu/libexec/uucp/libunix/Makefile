# This subdirectory contains Unix specific support functions.
# $FreeBSD$

LIB=		unix
SRCS=	basnam.c cusub.c efopen.c epopen.c init.c isdir.c isfork.c \
	iswait.c lock.c loctim.c mail.c mkdirs.c pause.c pipe.c \
	portnm.c proctm.c serial.c signal.c sindir.c sleep.c spawn.c \
	sync.c tcp.c time.c ufopen.c uid.c
CFLAGS+=	-I$(.CURDIR)/../common_sources \
		-DOWNER=\"$(owner)\" -DSBINDIR=\"$(libxdir)\"

NOMAN=		noman
NOPROFILE=	noprofile

install:

.include <bsd.lib.mk>
