#	@(#)Makefile	5.11 (Berkeley) 9/6/90
#	$Id$

.PATH: ${.CURDIR}/../libc/rpc ${.CURDIR}/.

SRCS+=	auth_des.c auth_none.c auth_time.c auth_unix.c \
	authdes_prot.c authunix_prot.c bindresvport.c \
	clnt_generic.c clnt_perror.c clnt_raw.c clnt_simple.c clnt_tcp.c \
	clnt_udp.c clnt_unix.c crypt_client.c des_crypt.c des_soft.c \
	get_myaddress.c getpublickey.c getrpcent.c getrpcport.c \
	key_call.c key_prot_xdr.c netname.c netnamer.c \
	pmap_clnt.c pmap_getmaps.c pmap_getport.c pmap_prot.c \
	pmap_prot2.c pmap_rmt.c rpc_callmsg.c rpc_commondata.c \
	rpc_dtablesize.c rpc_prot.c rpcdname.c rtime.c \
	svc.c svc_auth.c svc_auth_des.c svc_auth_unix.c \
	svc_raw.c svc_run.c svc_simple.c \
	svc_tcp.c svc_udp.c svc_unix.c

# generated sources
SRCS+=	crypt_clnt.c crypt_xdr.c

CFLAGS+= -DBROKEN_DES

CLEANFILES+= crypt_clnt.c crypt_xdr.c crypt.h

RPCDIR= ${DESTDIR}/usr/include/rpcsvc
RPCGEN= rpcgen -C

crypt_clnt.c: ${RPCDIR}/crypt.x crypt.h
	${RPCGEN} -l -o ${.TARGET} ${RPCDIR}/crypt.x

crypt_xdr.c: ${RPCDIR}/crypt.x crypt.h
	${RPCGEN} -c -o ${.TARGET} ${RPCDIR}/crypt.x

crypt.h: ${RPCDIR}/crypt.x
	${RPCGEN} -h -o ${.TARGET} ${RPCDIR}/crypt.x

.if ${LIB} == "c"
#
# XXX -- rstat.1 and rstat_svc.8 shouldn't really be here
#        but there's no rstat command, don't know why, so I'm
#        leaving them here in case they're needed sometime later.
# Paul.
#

# MAN1+=	rstat.1
MAN3+=	bindresvport.3 des_crypt.3 getrpcent.3 getrpcport.3 publickey.3 rpc.3 \
	rpc_secure.3 rtime.3
MAN5+=	publickey.5 rpc.5
MAN8+=	rstat_svc.8

MLINKS+=	getrpcent.3 endrpcent.3 \
		getrpcent.3 getrpcbyname.3 \
		getrpcent.3 getrpcbynumber.3 \
		getrpcent.3 setrpcent.3 \
		rpc.3 auth_destroy.3 \
		rpc.3 authnone_create.3 \
		rpc.3 authunix_create.3 \
		rpc.3 authunix_create_default.3 \
		rpc.3 callrpc.3 \
		rpc.3 clnt_broadcast.3 \
		rpc.3 clnt_call.3 \
		rpc.3 clnt_control.3 \
		rpc.3 clnt_create.3 \
		rpc.3 clnt_destroy.3 \
		rpc.3 clnt_freeres.3 \
		rpc.3 clnt_geterr.3 \
		rpc.3 clnt_pcreateerror.3 \
		rpc.3 clnt_perrno.3 \
		rpc.3 clnt_perror.3 \
		rpc.3 clnt_spcreateerror.3 \
		rpc.3 clnt_sperrno.3 \
		rpc.3 clnt_sperror.3 \
		rpc.3 clntraw_create.3 \
		rpc.3 clnttcp_create.3 \
		rpc.3 clntudp_bufcreate.3 \
		rpc.3 clntudp_create.3 \
		rpc.3 get_myaddress.3 \
		rpc.3 pmap_getmaps.3 \
		rpc.3 pmap_getport.3 \
		rpc.3 pmap_rmtcall.3 \
		rpc.3 pmap_set.3 \
		rpc.3 pmap_unset.3 \
		rpc.3 regsterrpc.3 \
		rpc.3 rpc_createerr.3 \
		rpc.3 svc_destroy.3 \
		rpc.3 svc_fds.3 \
		rpc.3 svc_fdset.3 \
		rpc.3 svc_getargs.3 \
		rpc.3 svc_getcaller.3 \
		rpc.3 svc_getreg.3 \
		rpc.3 svc_getregset.3 \
		rpc.3 svc_register.3 \
		rpc.3 svc_run.3 \
		rpc.3 svc_sendreply.3 \
		rpc.3 svc_unregister.3 \
		rpc.3 svcerr_auth.3 \
		rpc.3 svcerr_decode.3 \
		rpc.3 svcerr_noproc.3 \
		rpc.3 svcerr_noprog.3 \
		rpc.3 svcerr_progvers.3 \
		rpc.3 svcerr_systemerr.3 \
		rpc.3 svcerr_weakauth.3 \
		rpc.3 svcfd_create.3 \
		rpc.3 svcraw_create.3 \
		rpc.3 svctcp_create.3 \
		rpc.3 svcudp_bufcreate.3 \
		rpc.3 xdr_accepted_reply.3 \
		rpc.3 xdr_authunix_parms.3 \
		rpc.3 xdr_callhdr.3 \
		rpc.3 xdr_callmsg.3 \
		rpc.3 xdr_opaque_auth.3 \
		rpc.3 xdr_pmap.3 \
		rpc.3 xdr_pmaplist.3 \
		rpc.3 xdr_rejected_reply.3 \
		rpc.3 xdr_replymsg.3 \
		rpc.3 xprt_register.3 \
		rpc.3 xprt_unregister.3
.endif
