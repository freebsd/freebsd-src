LIB=		ftp
CFLAGS+=	-I${.CURDIR} -Wall

SRCS=	FtpAbort.c              FtpFull.c               FtpPasv.c	\
	FtpArchie.c             FtpGetHost.c            FtpPort.c	\
	FtpBye.c                FtpGood.c               FtpPwd.c	\
	FtpClose.c              FtpIO.c                 FtpRetr.c	\
	FtpCommand.c            FtpInit.c               FtpSize.c	\
	FtpConnect.c            FtpLogin.c              FtpStor.c	\
	FtpCopy.c               FtpMessage.c            \
	FtpData.c               FtpMove.c               FtpType.c	\
	FtpDebug.c              Ftpfopen.c	\
	FtpFilenameChecker.c    FtpOpenDir.c

beforeinstall:
	-cd ${.CURDIR}; cmp -s FtpLibrary.h ${DESTDIR}/usr/include/FtpLibrary.h || \
		install -c -o ${BINOWN} -g ${BINGRP} -m 444 FtpLibrary.h \
		${DESTDIR}/usr/include

.include <bsd.lib.mk>
