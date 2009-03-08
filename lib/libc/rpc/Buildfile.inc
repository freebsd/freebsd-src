# $FreeBSD$

.PATH: ${.CURDIR}/rpc

RPCSRCS += \
	auth_none.c \
	auth_unix.c \
	authunix_prot.c \
	bindresvport.c \
	clnt_bcast.c \
	clnt_dg.c \
	clnt_generic.c \
	clnt_perror.c \
	clnt_raw.c \
	clnt_simple.c \
	clnt_vc.c \
	crypt_clnt.c \
	crypt_xdr.c \
	getnetconfig.c \
	getnetpath.c \
	getrpcent.c \
	getrpcport.c \
	mt_misc.c \
	pmap_clnt.c \
	pmap_getmaps.c \
	pmap_getport.c \
	pmap_prot.c \
	pmap_prot2.c \
	pmap_rmt.c \
	rpc_callmsg.c \
	rpc_commondata.c \
	rpc_dtablesize.c \
	rpc_generic.c \
	rpc_prot.c \
	rpc_soc.c \
	rpcb_clnt.c \
	rpcb_prot.c \
	rpcb_st_xdr.c \
	rpcsec_gss_stub.c \
	svc.c \
	svc_auth.c \
	svc_auth_unix.c \
	svc_dg.c \
	svc_generic.c \
	svc_raw.c \
	svc_run.c \
	svc_simple.c \
	svc_vc.c

# Secure-RPC
RPCSRCS += \
	auth_des.c \
	auth_time.c \
	authdes_prot.c \
	crypt_client.c \
	des_crypt.c \
	des_soft.c \
	getpublickey.c \
	key_call.c \
	key_prot_xdr.c \
	svc_auth_des.c

# Resolver stuff
RPCSRCS += \
	netname.c \
	netnamer.c \
	rpcdname.c

# Misc Source
RPCSRCS += rtime.c

SRCS += ${RPCSRCS}

# Force this directory to be in the include path first to override the
# header files that these sources will compile with.
.for s in ${RPCSRCS}
${s:R}.o_CFLAGS = -I${.CURDIR}/rpc -I${.CURDIR}
.endfor

# generated sources
GENSRCS += \
	crypt.h \
	crypt_clnt.c \
	crypt_xdr.c

SYM_MAPS +=${.CURDIR}/rpc/Symbol.map

CFLAGS += -DBROKEN_DES -DPORTMAP -DDES_BUILTIN -I${.CURDIR}/rpc 

.if exists(${COMMONSTAGEDIR}/usr/include/rpcsvc/crypt.x)
CRYPTX = ${COMMONSTAGEDIR}/usr/include/rpcsvc/crypt.x
.else
CRYPTX = ${COMMONSHAREDSTAGEDIR}/usr/include/rpcsvc/crypt.x
.endif

crypt_clnt.c: ${CRYPTX}
	${RPCGEN} -C -l -o ${.TARGET} ${.ALLSRC}

crypt_xdr.c: ${CRYPTX}
	${RPCGEN} -C -c -o ${.TARGET} ${.ALLSRC}

crypt.h: ${CRYPTX}
	${RPCGEN} -C -h -o ${.TARGET} ${.ALLSRC}
