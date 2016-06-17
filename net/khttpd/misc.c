/*

kHTTPd -- the next generation

General functions

*/
/****************************************************************
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2, or (at your option)
 *	any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 ****************************************************************/

#include <linux/kernel.h>

#include <linux/ctype.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/net.h>
#include <linux/sched.h>
#include <linux/skbuff.h>
#include <linux/unistd.h>
#include <linux/file.h>
#include <linux/smp_lock.h>

#include <net/ip.h>
#include <net/sock.h>

#include <asm/atomic.h>
#include <asm/errno.h>
#include <asm/semaphore.h>
#include <asm/processor.h>
#include <asm/uaccess.h>

#include "structure.h"
#include "prototypes.h"

#ifndef ECONNRESET
#define ECONNRESET 102
#endif


/*

Readrest reads and discards all pending input on a socket. This is required
before closing the socket.

*/
static void ReadRest(struct socket *sock)
{
	struct msghdr		msg;
	struct iovec		iov;
	int			len;

	mm_segment_t		oldfs;
	
	
	EnterFunction("ReadRest");
	
	
	if (sock->sk==NULL)
		return;

	len = 1;
		
	while (len>0)
	{
		static char		Buffer[1024];   /* Never read, so doesn't need to
							   be SMP safe */

		msg.msg_name     = 0;
		msg.msg_namelen  = 0;
		msg.msg_iov	 = &iov;
		msg.msg_iovlen   = 1;
		msg.msg_control  = NULL;
		msg.msg_controllen = 0;
		msg.msg_flags    = MSG_DONTWAIT;
	
		msg.msg_iov->iov_base = &Buffer[0];
		msg.msg_iov->iov_len  = (__kernel_size_t)1024;
	
		len = 0;
		oldfs = get_fs(); set_fs(KERNEL_DS);
		len = sock_recvmsg(sock,&msg,1024,MSG_DONTWAIT);
		set_fs(oldfs);
	}
	LeaveFunction("ReadRest");
}


/*

CleanUpRequest takes care of shutting down the connection, closing the file-pointer
and releasing the memory of the request-structure. Do not try to access it afterwards!

*/
void CleanUpRequest(struct http_request *Req)
{
	EnterFunction("CleanUpRequest");	
	
	/* Close the socket ....*/
	if ((Req->sock!=NULL)&&(Req->sock->sk!=NULL))
	{
		ReadRest(Req->sock);
		remove_wait_queue(Req->sock->sk->sleep,&(Req->sleep));
	    	sock_release(Req->sock);
	}
	
	/* ... and the file-pointer ... */
	if (Req->filp!=NULL)
	{
	    	fput(Req->filp);
	    	Req->filp = NULL;
	}
	
	
	/* ... and release the memory for the structure. */
	kfree(Req);
	
	atomic_dec(&ConnectCount);
	LeaveFunction("CleanUpRequest");
}


/*

SendBuffer and Sendbuffer_async send "Length" bytes from "Buffer" to the "sock"et.
The _async-version is non-blocking.

A positive return-value indicates the number of bytes sent, a negative value indicates
an error-condition.

*/
int SendBuffer(struct socket *sock, const char *Buffer,const size_t Length)
{
	struct msghdr	msg;
	mm_segment_t	oldfs;
	struct iovec	iov;
	int 		len;
	
	EnterFunction("SendBuffer");
	
	msg.msg_name     = 0;
	msg.msg_namelen  = 0;
	msg.msg_iov	 = &iov;
	msg.msg_iovlen   = 1;
	msg.msg_control  = NULL;
	msg.msg_controllen = 0;
	msg.msg_flags    = MSG_NOSIGNAL;    
	msg.msg_iov->iov_len = (__kernel_size_t)Length;
	msg.msg_iov->iov_base = (char*) Buffer;
	
	
	len = 0;
	
	oldfs = get_fs(); set_fs(KERNEL_DS);
	len = sock_sendmsg(sock,&msg,(size_t)(Length-len));
	set_fs(oldfs);
	LeaveFunction("SendBuffer");
	return len;	
}

int SendBuffer_async(struct socket *sock, const char *Buffer,const size_t Length)
{
	struct msghdr	msg;
	mm_segment_t	oldfs;
	struct iovec	iov;
	int 		len;
	
	EnterFunction("SendBuffer_async");
	msg.msg_name     = 0;
	msg.msg_namelen  = 0;
	msg.msg_iov	 = &iov;
	msg.msg_iovlen   = 1;
	msg.msg_control  = NULL;
	msg.msg_controllen = 0;
	msg.msg_flags    = MSG_DONTWAIT|MSG_NOSIGNAL;    
	msg.msg_iov->iov_base = (char*) Buffer;
	msg.msg_iov->iov_len  = (__kernel_size_t)Length;
	

	if (sock->sk)
	{	
		oldfs = get_fs(); set_fs(KERNEL_DS);
		len = sock_sendmsg(sock,&msg,(size_t)(Length));
		set_fs(oldfs);
	} else
	{
		return -ECONNRESET;
	}
	
	LeaveFunction("SendBuffer_async");
	return len;	
}




/* 

HTTP header shortcuts. Hardcoded since these might be called in a low-memory
situation, and they don't change anyhow.

*/

static char NoPerm[] = "HTTP/1.0 403 Forbidden\r\nServer: kHTTPd 0.1.6\r\n\r\n";
static char TryLater[] = "HTTP/1.0 503 Service Unavailable\r\nServer: kHTTPd 0.1.6\r\nContent-Length: 15\r\n\r\nTry again later";
static char NotModified[] = "HTTP/1.0 304 Not Modified\r\nServer: kHTTPd 0.1.6\r\n\r\n";


void Send403(struct socket *sock)
{
	EnterFunction("Send403");
	(void)SendBuffer(sock,NoPerm,strlen(NoPerm));
	LeaveFunction("Send403");
}

void Send304(struct socket *sock)
{
	EnterFunction("Send304");
	(void)SendBuffer(sock,NotModified,strlen(NotModified));
	LeaveFunction("Send304");
}

void Send50x(struct socket *sock)
{
	EnterFunction("Send50x");
	(void)SendBuffer(sock,TryLater,strlen(TryLater));
	LeaveFunction("Send50x");
}

