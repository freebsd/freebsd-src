# This subdirectory contains Unix specific support functions.
# $FreeBSD$

LIB=		unix
SRCS = access.c addbas.c app3.c app4.c basnam.c bytfre.c corrup.c \
	chmod.c cohtty.c cusub.c cwd.c detach.c efopen.c epopen.c \
	exists.c failed.c filnam.c fsusg.c indir.c init.c isdir.c \
	isfork.c iswait.c jobid.c lcksys.c link.c locfil.c lock.c \
	loctim.c mail.c mkdirs.c mode.c move.c opensr.c pause.c \
	picksb.c pipe.c portnm.c priv.c proctm.c recep.c run.c seq.c \
	serial.c signal.c sindir.c size.c sleep.c spawn.c splcmd.c \
	splnam.c spool.c srmdir.c statsb.c status.c sync.c tcp.c \
	time.c tli.c tmpfil.c trunc.c uacces.c ufopen.c uid.c ultspl.c \
	unknwn.c uuto.c walk.c wldcrd.c work.c xqtfil.c xqtsub.c  ftw.c
CFLAGS+=	-I$(.CURDIR)/../common_sources \
		-DOWNER=\"$(owner)\" -DSBINDIR=\"$(libxdir)\"

NOMAN=		noman
NOPROFILE=	noprofile

install:

.include <bsd.lib.mk>
