/*

kHTTPd -- the next generation

Send actual file-data to the connections

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

/*

Purpose:

DataSending does the actual sending of file-data to the socket.

Note: Since asynchronous reads do not -yet- exists, this might block!

Return value:
	The number of requests that changed status (ie: made some progress)
*/

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/locks.h>
#include <linux/skbuff.h>

#include <net/tcp.h>

#include <asm/uaccess.h>
#include <linux/smp_lock.h>

#include "structure.h"
#include "prototypes.h"

static	char	*Block[CONFIG_KHTTPD_NUMCPU];

/*

This send_actor is for use with do_generic_file_read (ie sendfile())
It sends the data to the socket indicated by desc->buf.

*/
static int sock_send_actor(read_descriptor_t * desc, struct page *page, unsigned long offset, unsigned long size)
{
	int written;
	char *kaddr;
	unsigned long count = desc->count;
	struct socket *sock = (struct socket *) desc->buf;
	mm_segment_t old_fs;

	if (size > count)
		size = count;
	old_fs = get_fs();
	set_fs(KERNEL_DS);

	kaddr = kmap(page);
	written = SendBuffer_async(sock, kaddr + offset, size);
	kunmap(page);
	set_fs(old_fs);
	if (written < 0) {
		desc->error = written;
		written = 0;
	}
	desc->count = count - written;
	desc->written += written;
	return written;
}




int DataSending(const int CPUNR)
{
	struct http_request *CurrentRequest,**Prev;
	int count = 0;
	
	EnterFunction("DataSending");
	
	Prev = &(threadinfo[CPUNR].DataSendingQueue);
	CurrentRequest = threadinfo[CPUNR].DataSendingQueue;
	while (CurrentRequest!=NULL)
	{
		int ReadSize,Space;
		int retval;


		/* First, test if the socket has any buffer-space left.
		   If not, no need to actually try to send something.  */
		  
		
		Space = sock_wspace(CurrentRequest->sock->sk);
		
		ReadSize = min_t(int, 4 * 4096, CurrentRequest->FileLength - CurrentRequest->BytesSent);
		ReadSize = min_t(int, ReadSize, Space);

		if (ReadSize>0)
		{			
			struct inode *inode;
			
			inode = CurrentRequest->filp->f_dentry->d_inode;
			
			if (inode->i_mapping->a_ops->readpage) {
				/* This does the actual transfer using sendfile */		
				read_descriptor_t desc;
				loff_t *ppos;
		
				CurrentRequest->filp->f_pos = CurrentRequest->BytesSent;

				ppos = &CurrentRequest->filp->f_pos;

				desc.written = 0;
				desc.count = ReadSize;
				desc.buf = (char *) CurrentRequest->sock;
				desc.error = 0;
				do_generic_file_read(CurrentRequest->filp, ppos, &desc, sock_send_actor);
				if (desc.written>0)
				{	
					CurrentRequest->BytesSent += desc.written;
					count++;
				}			
			} 
			else  /* FS doesn't support sendfile() */
			{
				mm_segment_t oldfs;
				CurrentRequest->filp->f_pos = CurrentRequest->BytesSent;
				
				oldfs = get_fs(); set_fs(KERNEL_DS);
				retval = CurrentRequest->filp->f_op->read(CurrentRequest->filp, Block[CPUNR], ReadSize, &CurrentRequest->filp->f_pos);
				set_fs(oldfs);
		
				if (retval>0)
				{
					retval = SendBuffer_async(CurrentRequest->sock,Block[CPUNR],(size_t)retval);
					if (retval>0)
					{
						CurrentRequest->BytesSent += retval;
						count++;				
					}
				}
			}
		
		}
		
		/* 
		   If end-of-file or closed connection: Finish this request 
		   by moving it to the "logging" queue. 
		*/
		if ((CurrentRequest->BytesSent>=CurrentRequest->FileLength)||
		    (CurrentRequest->sock->sk->state!=TCP_ESTABLISHED
		     && CurrentRequest->sock->sk->state!=TCP_CLOSE_WAIT))
		{
			struct http_request *Next;
			Next = CurrentRequest->Next;

			lock_sock(CurrentRequest->sock->sk);
			if  (CurrentRequest->sock->sk->state == TCP_ESTABLISHED ||
			     CurrentRequest->sock->sk->state == TCP_CLOSE_WAIT)
			{
				CurrentRequest->sock->sk->tp_pinfo.af_tcp.nonagle = 0;
				tcp_push_pending_frames(CurrentRequest->sock->sk,&(CurrentRequest->sock->sk->tp_pinfo.af_tcp));
			}
			release_sock(CurrentRequest->sock->sk);

			(*Prev) = CurrentRequest->Next;
			
			CurrentRequest->Next = threadinfo[CPUNR].LoggingQueue;
			threadinfo[CPUNR].LoggingQueue = CurrentRequest;	
				
			CurrentRequest = Next;
			continue;
		
		}
		

		Prev = &(CurrentRequest->Next);	
		CurrentRequest = CurrentRequest->Next;
	}
	
	LeaveFunction("DataSending");
	return count;
}

int InitDataSending(int ThreadCount)
{
	int I,I2;
	
	EnterFunction("InitDataSending");
	I=0;
	while (I<ThreadCount)
	{
		Block[I] = (char*)get_free_page((int)GFP_KERNEL);
		if (Block[I] == NULL) 
		{
			I2=0;
			while (I2<I-1)
			{
				free_page((unsigned long)Block[I2++]);
			}
			LeaveFunction("InitDataSending - abort");
			return -1;
		}
		I++;
	}
	LeaveFunction("InitDataSending");
	return 0;		
}

void StopDataSending(const int CPUNR)
{
	struct http_request *CurrentRequest,*Next;
	
	EnterFunction("StopDataSending");
	CurrentRequest = threadinfo[CPUNR].DataSendingQueue;

	while (CurrentRequest!=NULL)
	{	
		Next = CurrentRequest->Next;
		CleanUpRequest(CurrentRequest);
		CurrentRequest=Next;		
	}
	
	threadinfo[CPUNR].DataSendingQueue = NULL;

	free_page( (unsigned long)Block[CPUNR]);
	LeaveFunction("StopDataSending");
}
