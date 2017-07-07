/* @(#)rpc.h	2.3 88/08/10 4.0 RPCSRC; from 1.9 88/02/08 SMI */
/*
 * Copyright (c) 2010, Oracle America, Inc.
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of the "Oracle America, Inc." nor the names of
 *       its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * rpc.h, Just includes the billions of rpc header files necessary to
 * do remote procedure calling.
 */
#ifndef GSSRPC_RPC_H
#define GSSRPC_RPC_H

#include <gssrpc/types.h>		/* some typedefs */
#include <netinet/in.h>

/* external data representation interfaces */
#include <gssrpc/xdr.h>		/* generic (de)serializer */

/* Client side only authentication */
#include <gssrpc/auth.h>		/* generic authenticator (client side) */

/* Client side (mostly) remote procedure call */
#include <gssrpc/clnt.h>		/* generic rpc stuff */

/* semi-private protocol headers */
#include <gssrpc/rpc_msg.h>	/* protocol for rpc messages */
#include <gssrpc/auth_unix.h>	/* protocol for unix style cred */
#include <gssrpc/auth_gss.h>	/* RPCSEC_GSS */
/*
 *  Uncomment-out the next line if you are building the rpc library with
 *  DES Authentication (see the README file in the secure_rpc/ directory).
 */
#if 0
#include <gssrpc/auth_des.h>	protocol for des style cred
#endif

/* Server side only remote procedure callee */
#include <gssrpc/svc_auth.h>	/* service side authenticator */
#include <gssrpc/svc.h>		/* service manager and multiplexer */

/*
 * Punt the rpc/netdb.h everywhere because it just makes things much more
 * difficult.  We don't use the *rpcent functions anyway.
 */
#if 0
/*
 * COMMENT OUT THE NEXT INCLUDE IF RUNNING ON SUN OS OR ON A VERSION
 * OF UNIX BASED ON NFSSRC.  These systems will already have the structures
 * defined by <rpc/netdb.h> included in <netdb.h>.
 */
/* routines for parsing /etc/rpc */
#if 0 /* netdb.h already included in rpc/types.h */
#include <netdb.h>
#endif

#include <gssrpc/netdb.h>	/* structures and routines to parse /etc/rpc */
#endif

/*
 * get the local host's IP address without consulting
 * name service library functions
 */
GSSRPC__BEGIN_DECLS
extern int get_myaddress(struct sockaddr_in *);
extern int bindresvport(int, struct sockaddr_in *);
extern int bindresvport_sa(int, struct sockaddr *);
extern int callrpc(char *, rpcprog_t, rpcvers_t, rpcproc_t, xdrproc_t,
		   char *, xdrproc_t , char *);
extern int getrpcport(char *, rpcprog_t, rpcvers_t, rpcprot_t);
extern int gssrpc__rpc_dtablesize(void);
GSSRPC__END_DECLS

#endif /* !defined(GSSRPC_RPC_H) */
