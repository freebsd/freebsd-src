/*

kHTTPd -- the next generation

Wait for headers on the accepted connections

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

WaitForHeaders polls all connections in "WaitForHeaderQueue" to see if
headers have arived. If so, the headers are decoded and the request is
moved to either the "SendingDataQueue" or the "UserspaceQueue".

Return value:
	The number of requests that changed status
*/

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/smp_lock.h>
#include <linux/file.h>

#include <asm/uaccess.h>

#include "structure.h"
#include "prototypes.h"

static	char			*Buffer[CONFIG_KHTTPD_NUMCPU];


static int DecodeHeader(const int CPUNR, struct http_request *Request);


int WaitForHeaders(const int CPUNR)
{
	struct http_request *CurrentRequest,**Prev;
	struct sock *sk;
	int count = 0;
	
	EnterFunction("WaitForHeaders");
	
	CurrentRequest = threadinfo[CPUNR].WaitForHeaderQueue;
	
	Prev = &(threadinfo[CPUNR].WaitForHeaderQueue);
	
	while (CurrentRequest!=NULL)
	{
		
		/* If the connection is lost, remove from queue */
		
		if (CurrentRequest->sock->sk->state != TCP_ESTABLISHED
		    && CurrentRequest->sock->sk->state != TCP_CLOSE_WAIT)
		{
			struct http_request *Next;
			
			Next = CurrentRequest->Next;
			
			*Prev = CurrentRequest->Next;
			CurrentRequest->Next = NULL;
			
		
			CleanUpRequest(CurrentRequest);
			CurrentRequest = Next;
			continue;
		}
		
		
		
		/* If data pending, take action */	
		
		sk = CurrentRequest->sock->sk;
		
		if (!skb_queue_empty(&(sk->receive_queue))) /* Do we have data ? */
		{
			struct http_request *Next;
			
			
			
			/* Decode header */
			
			if (DecodeHeader(CPUNR,CurrentRequest)<0)
			{
				CurrentRequest = CurrentRequest->Next;
				continue;
			} 
			
			
			/* Remove from WaitForHeaderQueue */		
			
			Next= CurrentRequest->Next;
		
			*Prev = Next;
			count++;
			
			/* Add to either the UserspaceQueue or the DataSendingQueue */
			
			if (CurrentRequest->IsForUserspace!=0)
			{
				CurrentRequest->Next = threadinfo[CPUNR].UserspaceQueue;
				threadinfo[CPUNR].UserspaceQueue = CurrentRequest;	
			} else
			{
				CurrentRequest->Next = threadinfo[CPUNR].DataSendingQueue;
				threadinfo[CPUNR].DataSendingQueue = CurrentRequest;	
			} 	
			
			CurrentRequest = Next;
			continue;
		
		}	

		
		Prev = &(CurrentRequest->Next);
		CurrentRequest = CurrentRequest->Next;
	}

	LeaveFunction("WaitForHeaders");
	return count;
}

void StopWaitingForHeaders(const int CPUNR)
{
	struct http_request *CurrentRequest,*Next;
	
	EnterFunction("StopWaitingForHeaders");
	CurrentRequest = threadinfo[CPUNR].WaitForHeaderQueue;

	while (CurrentRequest!=NULL)
	{
		Next = CurrentRequest->Next;
		CleanUpRequest(CurrentRequest);
		CurrentRequest=Next;		
	}
	
	threadinfo[CPUNR].WaitForHeaderQueue = NULL; /* The queue is empty now */
	
	free_page((unsigned long)Buffer[CPUNR]);
	Buffer[CPUNR]=NULL;
	
	EnterFunction("StopWaitingForHeaders");
}


/* 

DecodeHeader peeks at the TCP/IP data, determines what the request is, 
fills the request-structure and sends the HTTP-header when apropriate.

*/

static int DecodeHeader(const int CPUNR, struct http_request *Request)
{
	struct msghdr		msg;
	struct iovec		iov;
	int			len;

	mm_segment_t		oldfs;
	
	EnterFunction("DecodeHeader");
	
	if (Buffer[CPUNR] == NULL) {
		/* see comments in main.c regarding buffer managemnet - dank */
		printk(KERN_CRIT "khttpd: lost my buffer");
		BUG();
	}

	/* First, read the data */

	msg.msg_name     = 0;
	msg.msg_namelen  = 0;
	msg.msg_iov	 = &iov;
	msg.msg_iovlen   = 1;
	msg.msg_control  = NULL;
	msg.msg_controllen = 0;
	msg.msg_flags    = 0;
	
	msg.msg_iov->iov_base = &Buffer[CPUNR][0];
	msg.msg_iov->iov_len  = (size_t)4095;
	
	len = 0;
	oldfs = get_fs(); set_fs(KERNEL_DS);
	/* 4095 leaves a "0" to terminate the string */
	
	len = sock_recvmsg(Request->sock,&msg,4095,MSG_PEEK);
	set_fs(oldfs);

	if (len<0) {
		/* WONDERFUL. NO COMMENTS. --ANK */
		Request->IsForUserspace = 1;
		return 0;
	}

	if (len>=4094) /* BIG header, we cannot decode it so leave it to userspace */	
	{
		Request->IsForUserspace = 1;
		return 0;
	}
	
	/* Then, decode the header */
	
	
	ParseHeader(Buffer[CPUNR],len,Request);
	
	Request->filp = OpenFileForSecurity(Request->FileName);
	
	
	Request->MimeType = ResolveMimeType(Request->FileName,&Request->MimeLength);
	
	
	if (Request->MimeType==NULL) /* Unknown mime-type */
	{
		if (Request->filp!=NULL)
		{
			fput(Request->filp);
			Request->filp = NULL;
		}
		Request->IsForUserspace = 1;
		
		return 0;
	}

	if (Request->filp==NULL)
	{
		Request->IsForUserspace = 1;
		return 0;
	}
	else
	{
		Request->FileLength = (int)Request->filp->f_dentry->d_inode->i_size;
		Request->Time       = Request->filp->f_dentry->d_inode->i_mtime;
		Request->IMS_Time   = mimeTime_to_UnixTime(Request->IMS);
		sprintf(Request->LengthS,"%i",Request->FileLength);
		time_Unix2RFC(min_t(unsigned int, Request->Time,CurrentTime_i),Request->TimeS);
   	        /* The min() is required by rfc1945, section 10.10:
   	           It is not allowed to send a filetime in the future */

		if (Request->IMS_Time>Request->Time)
		{	/* Not modified since last time */
			Send304(Request->sock);
			Request->FileLength=0;
		}
		else   /* Normal Case */
		{
			Request->sock->sk->tp_pinfo.af_tcp.nonagle = 2; /* this is TCP_CORK */
			if (Request->HTTPVER!=9)  /* HTTP/0.9 doesn't allow a header */
				SendHTTPHeader(Request);
		}
		
	
	}
	
	LeaveFunction("DecodeHeader");
	return 0;
}


int InitWaitHeaders(int ThreadCount)
{
	int I,I2;

	EnterFunction("InitWaitHeaders");
	I=0;	
	while (I<ThreadCount)
	{
		Buffer[I] = (char*)get_free_page((int)GFP_KERNEL);
		if (Buffer[I] == NULL) 
		{
			printk(KERN_CRIT "kHTTPd: Not enough memory for basic needs\n");
			I2=0;
			while (I2<I-1)
			{
				free_page( (unsigned long)Buffer[I2++]);
			}
			return -1;
		}
		I++;
	}
	
	LeaveFunction("InitWaitHeaders");	
	return 0;

}
