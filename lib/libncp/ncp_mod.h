/*
 * Describes all ncp_lib kernel functions
 *
 * $FreeBSD: src/lib/libncp/ncp_mod.h,v 1.1 1999/10/12 11:56:38 bp Exp $
 */
#ifndef _NCP_MOD_H_
#define _NCP_MOD_H_

/* order of calls in syscall table relative to offset in system table */
#define	NCP_SE(callno)		(callno+sysentoffset)
#define	NCP_CONNSCAN		NCP_SE(0)
#define	NCP_CONNECT		NCP_SE(1)
#define	NCP_INTFN		NCP_SE(2)
#define	SNCP_REQUEST		NCP_SE(3)

#endif /* !_NCP_MOD_H_ */