#	$Id$

PROG=	passwd
BINDIR=	/usr/bin

SRCS=	local_passwd.c passwd.c pw_copy.c pw_util.c kpasswd.c
.PATH:	${.CURDIR}/../../usr.bin/chpass ${.CURDIR}/../../usr.sbin/vipw \
	${.CURDIR}/../../usr.bin/rlogin ${.CURDIR}/../../usr.bin/passwd
CFLAGS+=	-DKERBEROS -DPOSIX \
		-I${.CURDIR} -I${.CURDIR}/../../usr.sbin/vipw \
		-I${.CURDIR}/../../usr.bin/chpass \
		-I${.CURDIR}/../../usr.bin/passwd \
		-I${.CURDIR}/../include \
		-I${.CURDIR}/../libkadm
LDADD=	-L${KADMOBJDIR} -lkadm -L${KRBOBJDIR} -lkrb -L${DESOBJDIR} -ldes \
	-lcrypt -lcom_err

BINOWN=	root
BINMODE=4555
INSTALLFLAGS=	-fschg
NOMAN=		#man page installed by regular passwd

.include <bsd.prog.mk>
