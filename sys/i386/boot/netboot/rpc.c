/***********************************************************************

Remote Procedure Call Support Routines

Author: Martin Renters
  Date: Oct/1994

***********************************************************************/

#include "netboot.h"

int rpc_id;
extern char packet[];
extern struct nfs_diskless nfsdiskless;
extern int hostnamelen;
/***************************************************************************

RPCLOOKUP:  Lookup RPC Port numbers

***************************************************************************/
rpclookup(addr, prog, ver)
	int addr, prog, ver;
{
	struct rpc_t buf, *rpc;
	char *rpcptr;
	int retries = MAX_RPC_RETRIES;
	rpcptr = sprintf(&buf.u.data,"%L%L%L%L%L%L%L%L%L%L%L%L%L%L",
		rpc_id, MSG_CALL, 2, PROG_PORTMAP, 2, PORTMAP_LOOKUP,
		0, 0, 0, 0, prog, ver, IP_UDP, 0);
	while(retries--) {
		udp_transmit(arptable[addr].ipaddr, RPC_SOCKET,
			SUNRPC, rpcptr - (char *)&buf, &buf);
		if (await_reply(AWAIT_RPC, rpc_id, NULL)) {
			rpc = (struct rpc_t *)&packet[ETHER_HDR_LEN];
			if (rpc->u.reply.rstatus == rpc->u.reply.verifier ==
				rpc->u.reply.astatus == 0)
				return(ntohl(rpc->u.reply.data[0]));
			else {
				rpc_err(rpc);
				return(-1);
			}
		}
	}
	return(-1);
}

/***************************************************************************

NFS_MOUNT:  Mount an NFS Filesystem

***************************************************************************/
nfs_mount(server, port, path, fh)
	int server;
	int port;
	char *path;
	char *fh;
{
	struct	rpc_t buf, *rpc;
	char	*rpcptr;
	int retries = MAX_RPC_RETRIES;
	rpcptr = sprintf(&buf.u.data,"%L%L%L%L%L%L%L%L%L%S%L%L%L%L%L%L%L%S",
		rpc_id, MSG_CALL, 2, PROG_MOUNT, 1, MOUNT_ADDENTRY,
		1, hostnamelen + 28,0,&nfsdiskless.my_hostnam,0,0,2,0,0,0,0,
		path);
	while(retries--) {
		udp_transmit(arptable[server].ipaddr, RPC_SOCKET,
			port, rpcptr - (char *)&buf, &buf);
		if (await_reply(AWAIT_RPC, rpc_id, NULL)) {
			rpc = (struct rpc_t *)&packet[ETHER_HDR_LEN];
			if (rpc->u.reply.rstatus || rpc->u.reply.verifier ||
				rpc->u.reply.astatus || rpc->u.reply.data[0]) {
				rpc_err(rpc);
				return(-(ntohl(rpc->u.reply.data[0])));
			} else {
				bcopy(&rpc->u.reply.data[1],fh, 32);
				return(0);
			}
		}
	}
	return(-1);
}


/***************************************************************************

NFS_LOOKUP:  Lookup Pathname

***************************************************************************/
nfs_lookup(server, port, fh, path, file_fh, sizep)
	int server;
	int port;
	char *fh;
	char *path;
	char *file_fh;
	int *sizep;
{
	struct	rpc_t buf, *rpc;
	char	*rpcptr;
	int retries = MAX_RPC_RETRIES;
	rpcptr = sprintf(&buf.u.data,"%L%L%L%L%L%L%L%L%L%S%L%L%L%L%L%L%L%M%S",
		rpc_id, MSG_CALL, 2, PROG_NFS, 2, NFS_LOOKUP,
		1, hostnamelen + 28,0,&nfsdiskless.my_hostnam,0,0,2,0,0,0,0,
		32, fh, path);
	while(retries--) {
		udp_transmit(arptable[server].ipaddr, RPC_SOCKET,
			port, rpcptr - (char *)&buf, &buf);
		if (await_reply(AWAIT_RPC, rpc_id, NULL)) {
			rpc = (struct rpc_t *)&packet[ETHER_HDR_LEN];
			if (rpc->u.reply.rstatus || rpc->u.reply.verifier ||
				rpc->u.reply.astatus || rpc->u.reply.data[0]) {
				rpc_err(rpc);
				return(-(ntohl(rpc->u.reply.data[0])));
			} else {
				bcopy(&rpc->u.reply.data[1],file_fh, 32);
				if (sizep)
				  *sizep = ntohl(rpc->u.reply.data[14]);
				return(0);
			}
		}
	}
	return(-1);
}

/***************************************************************************

NFS_READ:  Read File

***************************************************************************/
nfs_read(server, port, fh, offset, len, buffer)
	int server;
	int port;
	char *fh;
	int offset, len;
	char *buffer;
{
	struct	rpc_t buf, *rpc;
	char	*rpcptr;
	int	retries = MAX_RPC_RETRIES;
	int	rlen;
	rpcptr = sprintf(&buf.u.data,
		"%L%L%L%L%L%L%L%L%L%S%L%L%L%L%L%L%L%M%L%L%L",
		rpc_id, MSG_CALL, 2, PROG_NFS, 2, NFS_READ,
		1, hostnamelen + 28,0,&nfsdiskless.my_hostnam,0,0,2,0,0,0,0,
		32, fh, offset, len, 0);
	while(retries--) {
		udp_transmit(arptable[server].ipaddr, RPC_SOCKET,
			port, rpcptr - (char *)&buf, &buf);
		if (await_reply(AWAIT_RPC, rpc_id, NULL)) {
			rpc = (struct rpc_t *)&packet[ETHER_HDR_LEN];
			if (rpc->u.reply.rstatus || rpc->u.reply.verifier ||
				rpc->u.reply.astatus || rpc->u.reply.data[0]) {
				rpc_err(rpc);
				return(-(ntohl(rpc->u.reply.data[0])));
			} else {
				rlen = ntohl(rpc->u.reply.data[18]);
				if (len < rlen) rlen = len;
				if (len > rlen) printf("short read\r\n");
				bcopy(&rpc->u.reply.data[19], buffer, rlen);
				return(rlen);
			}
		}
	}
	return(-1);
}

/***************************************************************************

RPC_ERR - Print RPC Errors

***************************************************************************/
rpc_err(rpc)
	struct rpc_t	*rpc;
{
	int err = ntohl(rpc->u.reply.data[0]);
	printf("***RPC Error: (%d,%d,%d):\r\n ",
		ntohl(rpc->u.reply.rstatus),
		ntohl(rpc->u.reply.verifier),
		ntohl(rpc->u.reply.astatus));
}

nfs_err(err)
	int err;
{
	err = -err;
	if (err == NFSERR_PERM)		printf("Not owner");
	else if (err == NFSERR_NOENT) 	printf("No such file or directory");
	else if (err == NFSERR_ACCES)	printf("Permission denied");
	else printf("Error %d",err);
	printf("\r\n");
}
