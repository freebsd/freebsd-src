# From:	@(#)Makefile	8.3 (Berkeley) 4/2/94
# $FreeBSD$

# NOPAM is used by PicoBSD

.if defined(NOPAM)

PROG=	passwd
SRCS=	local_passwd.c passwd.c pw_copy.c pw_util.c 

GENSRCS=yp.h yp_clnt.c yppasswd.h yppasswd_clnt.c \
	yppasswd_private.h yppasswd_private_clnt.c yppasswd_private_xdr.c
CFLAGS+=-Wall

DPADD=	${LIBCRYPT} ${LIBUTIL}
LDADD=	-lcrypt -lutil
.PATH:  ${.CURDIR}/../../usr.bin/chpass ${.CURDIR}/../../usr.sbin/vipw \
	${.CURDIR}/../rlogin 

CFLAGS+= -DLOGIN_CAP -DCRYPT -I. -I${.CURDIR} \
	-I${.CURDIR}/../../usr.sbin/vipw \
	-I${.CURDIR}/../../usr.bin/chpass \
	-Dyp_error=warnx -DLOGGING

.else

PROG=	passwd
SRCS=	local_passwd.c passwd.c pw_copy.c pw_util.c pw_yp.c \
	yp_passwd.c ypxfr_misc.c ${GENSRCS}
GENSRCS=yp.h yp_clnt.c yppasswd.h yppasswd_clnt.c \
	yppasswd_private.h yppasswd_private_clnt.c yppasswd_private_xdr.c
CFLAGS+=-Wall

DPADD=	${LIBCRYPT} ${LIBRPCSVC} ${LIBUTIL}
LDADD=	-lcrypt -lrpcsvc -lutil
.PATH:  ${.CURDIR}/../../usr.bin/chpass ${.CURDIR}/../../usr.sbin/vipw \
	${.CURDIR}/../rlogin ${.CURDIR}/../../libexec/ypxfr \
	${.CURDIR}/../../usr.sbin/rpc.yppasswdd

CFLAGS+= -DLOGIN_CAP -DCRYPT -DYP -I. -I${.CURDIR} \
	-I${.CURDIR}/../../usr.sbin/vipw \
	-I${.CURDIR}/../../usr.bin/chpass \
	-I${.CURDIR}/../../libexec/ypxfr \
	-I${.CURDIR}/../../usr.sbin/rpc.yppasswdd \
	-Dyp_error=warnx -DLOGGING

.endif

CLEANFILES= ${GENSRCS}

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

yppasswd_clnt.c: ${RPCSRC_PW}
	${RPCGEN} -l -o ${.TARGET} ${RPCSRC_PW}

yppasswd_private.h: ${RPCSRC_PRIV}
	${RPCGEN} -h -o ${.TARGET} ${RPCSRC_PRIV}

yppasswd_private_xdr.c: ${RPCSRC_PRIV}
	${RPCGEN} -c -o ${.TARGET} ${RPCSRC_PRIV}

yppasswd_private_clnt.c: ${RPCSRC_PRIV}
	${RPCGEN} -l -o ${.TARGET} ${RPCSRC_PRIV}

BINMODE=4555
LINKS=${BINDIR}/passwd ${BINDIR}/yppasswd
MLINKS=passwd.1 yppasswd.1

.if exists(${DESTDIR}${LIBDIR}/libkrb.a) && !defined(NOCRYPT) && !defined(NO_OPENSSL) && defined(MAKE_KERBEROS4)
SRCS+=	kpasswd_standalone.c
.PATH:	${.CURDIR}/../../usr.bin/chpass ${.CURDIR}/../../usr.sbin/vipw \
	${.CURDIR}/../../usr.bin/rlogin ${.CURDIR}/../../usr.bin/passwd \
	${.CURDIR}/../../crypto/kerberosIV/kadmin

CFLAGS+=	-DKERBEROS
# XXX not defined: ${LIBKADM}, ${LIBCOM_ERR}
DPADD=	${LIBKADM} ${LIBKRB} ${LIBCRYPTO} ${LIBCRYPT} ${LIBRPCSVC} ${LIBCOM_ERR} ${LIBUTIL}
LDADD=	-lkadm -lkrb -lcrypto -lcrypt -lrpcsvc -lcom_err -lutil
DISTRIBUTION=	krb4
.endif

beforeinstall:
.for i in passwd yppasswd
	[ ! -e ${DESTDIR}${BINDIR}/$i ] || \
		chflags noschg ${DESTDIR}${BINDIR}/$i || true
.endfor

afterinstall:
	-chflags schg ${DESTDIR}${BINDIR}/passwd

.include <bsd.prog.mk>
