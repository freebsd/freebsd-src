# From:	@(#)Makefile	8.3 (Berkeley) 4/2/94
# $FreeBSD$

PROG=	passwd
SRCS=	local_passwd.c yppasswd_private_xdr.c yppasswd_comm.c yp_passwd.c \
	passwd.c pw_copy.c pw_util.c pw_yp.c

DPADD=	${LIBCRYPT} ${LIBRPCSVC}
LDADD=	-lcrypt -lrpcsvc
.PATH:  ${.CURDIR}/../../usr.bin/chpass ${.CURDIR}/../../usr.sbin/vipw \
	${.CURDIR}/../rlogin ${.CURDIR}/../../libexec/ypxfr \
	${.CURDIR}/../../usr.sbin/rpc.yppasswdd

CFLAGS+= -DCRYPT -DYP -I. -I${.CURDIR} -I${.CURDIR}/../../usr.sbin/vipw \
	-I${.CURDIR}/../../usr.bin/chpass -I${.CURDIR}/../../libexec/ypxfr \
	-I${.CURDIR}/../../usr.sbin/rpc.yppasswdd -Dyp_error=warnx -DLOGGING

SRCS+= ypxfr_misc.c yp_clnt.c yppasswd_clnt.c

CLEANFILES= yp.h yp_clnt.c yppasswd.h yppasswd_clnt.c \
	    yppasswd_private.h yppasswd_private_xdr.c

RPCGEN= rpcgen -C
RPCSRC= ${DESTDIR}/usr/include/rpcsvc/yp.x
RPCSRC_PW= ${DESTDIR}/usr/include/rpcsvc/yppasswd.x
RPCSRC_PRIV= ${.CURDIR}/../../usr.sbin/rpc.yppasswdd/yppasswd_private.x

yp.h: ${RPCSRC}
	${RPCGEN} -h -o ${.TARGET} ${RPCSRC}

yp_clnt.c: ${RPCSRC} yp.h
	${RPCGEN} -l -o ${.TARGET} ${RPCSRC}

yppasswd.h: ${RPCSRC_PW}
	${RPCGEN} -h -o ${.TARGET} ${RPCSRC_PW}

yppasswd_clnt.c: ${RPCSRC_PW} yppasswd.h
	${RPCGEN} -l -o ${.TARGET} ${RPCSRC_PW}

yppasswd_private.h: ${RPCSRC_PRIV}
	${RPCGEN} -h -o ${.TARGET} ${RPCSRC_PRIV}

yppasswd_private_xdr.c: ${RPCSRC_PRIV} yppasswd_private.h
	${RPCGEN} -c -o ${.TARGET} ${RPCSRC_PRIV}

BINOWN=	root
BINMODE=4555
MAN1=passwd.1
LINKS=${BINDIR}/passwd ${BINDIR}/yppasswd
MLINKS=passwd.1 yppasswd.1

.if exists(${DESTDIR}/usr/lib/libkrb.a) && (defined(MAKE_EBONES))
SRCS+=	kpasswd.c
.PATH:	${.CURDIR}/../../usr.bin/chpass ${.CURDIR}/../../usr.sbin/vipw \
	${.CURDIR}/../../usr.bin/rlogin ${.CURDIR}/../../usr.bin/passwd \
	${.CURDIR}/../../eBones/usr.bin/passwd

CFLAGS+=	-DKERBEROS \
		-I${.CURDIR}/../../eBones/include \
		-I${.CURDIR}/../../eBones/lib/libkadm
# XXX not defined: ${LIBKADM}, ${LIBCOM_ERR}
DPADD=	${LIBKADM} ${LIBKRB} ${LIBDES} ${LIBCRYPT} ${LIBRPCSVC} ${LIBCOM_ERR}
LDADD=	-lkadm -lkrb -ldes -lcrypt -lrpcsvc -lcom_err
DISTRIBUTION=	krb
.endif

beforeinstall:
	[ ! -e ${DESTDIR}${BINDIR}/passwd ] || \
		chflags noschg ${DESTDIR}${BINDIR}/passwd

afterinstall:
	chflags schg ${DESTDIR}${BINDIR}/passwd

.include <bsd.prog.mk>
