#	@(#)Makefile	8.2 (Berkeley) 4/2/94

PROG=	chpass
SRCS=	chpass.c edit.c field.c pw_copy.c pw_scan.c pw_util.c table.c util.c
BINOWN=	root
BINMODE=4555
.PATH:	${.CURDIR}/../../usr.sbin/pwd_mkdb ${.CURDIR}/../../usr.sbin/vipw \
	${.CURDIR}/../../libexec/ypxfr \
	${.CURDIR}/../../usr.sbin/rpc.yppasswdd
CFLAGS+=-I${.CURDIR}/../../usr.sbin/pwd_mkdb -I${.CURDIR}/../../usr.sbin/vipw
LINKS=	${BINDIR}/chpass ${BINDIR}/chfn
LINKS+=	${BINDIR}/chpass ${BINDIR}/chsh
LINKS+=	${BINDIR}/chpass ${BINDIR}/ypchpass
LINKS+=	${BINDIR}/chpass ${BINDIR}/ypchfn
LINKS+=	${BINDIR}/chpass ${BINDIR}/ypchsh
MLINKS=	chpass.1 chfn.1 chpass.1 chsh.1
COPTS+=	-DYP -I. -I${.CURDIR}/../../libexec/ypxfr \
	-I${.CURDIR}/../../usr.sbin/rpc.yppasswdd -Dyp_error=warnx

#Some people need this, uncomment to activate
#COPTS+= -DRESTRICT_FULLNAME_CHANGE

SRCS+=	yppasswd_private_xdr.c yppasswd_private_clnt.c yp_clnt.c \
	yppasswd_clnt.c pw_yp.c ypxfr_misc.c
CLEANFILES= yp_clnt.c yp.h yppasswd_clnt.c yppasswd.h \
	    yppasswd_private_xdr.c yppasswd_private_clnt.c yppasswd_private.h

DPADD=	${LIBRPCSVC} ${LIBCRYPT}
LDADD+=	-lrpcsvc -lcrypt

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

yppasswd_private_clnt.c: ${RPCSRC_PRIV} yppasswd_private.h
	${RPCGEN} -l -o ${.TARGET} ${RPCSRC_PRIV}

beforeinstall:
.for i in chpass chfn chsh ypchpass ypchfn ypchsh
	[ ! -e ${DESTDIR}${BINDIR}/$i ] || \
		chflags noschg ${DESTDIR}${BINDIR}/$i
.endfor

afterinstall:
	chflags schg ${DESTDIR}${BINDIR}/chpass

.include <bsd.prog.mk>
