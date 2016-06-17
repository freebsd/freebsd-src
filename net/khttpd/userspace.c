/*

kHTTPd -- the next generation

Pass connections to userspace-daemons

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

Userspace() hands all requests in the queue to the userspace-daemon, if
such beast exists.

Return value:
	The number of requests that changed status
*/
#include <linux/kernel.h>

#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/net.h>
#include <linux/sched.h>
#include <linux/skbuff.h>
#include <linux/smp_lock.h>
#include <linux/un.h>
#include <linux/unistd.h>
#include <linux/wait.h>

#include <net/ip.h>
#include <net/sock.h>
#include <net/tcp.h>

#include <asm/atomic.h>
#include <asm/semaphore.h>
#include <asm/processor.h>
#include <asm/uaccess.h>

#include <linux/file.h>


#include "structure.h"
#include "prototypes.h"
#include "sysctl.h"

/* prototypes of local, static functions */
static int AddSocketToAcceptQueue(struct socket *sock,const int Port);


int Userspace(const int CPUNR)
{
	struct http_request *CurrentRequest,**Prev,*Next;
	
	EnterFunction("Userspace");

	

	
	CurrentRequest = threadinfo[CPUNR].UserspaceQueue;
	Prev = &(threadinfo[CPUNR].UserspaceQueue);
	
	while (CurrentRequest!=NULL)
	{

		/* Clean-up the waitqueue of the socket.. Bad things happen if
		   this is forgotten. */
		if (CurrentRequest->sock!=NULL)
		{
			if ((CurrentRequest->sock!=NULL)&&(CurrentRequest->sock->sk!=NULL))
			{
				remove_wait_queue(CurrentRequest->sock->sk->sleep,&(CurrentRequest->sleep));
			}
		} 
		

		if  (AddSocketToAcceptQueue(CurrentRequest->sock,sysctl_khttpd_clientport)>=0)
		{
			
			(*Prev) = CurrentRequest->Next;
			Next = CurrentRequest->Next;
			
			
			sock_release(CurrentRequest->sock);
			CurrentRequest->sock = NULL;	 /* We no longer own it */
			
			CleanUpRequest(CurrentRequest); 
				
			CurrentRequest = Next;
			continue;
		
		}
		else /* No userspace-daemon present, or other problems with it */
		{
			(*Prev) = CurrentRequest->Next;
			Next = CurrentRequest->Next;
			
			Send403(CurrentRequest->sock); /* Sorry, no go... */
			
			CleanUpRequest(CurrentRequest); 
				
			CurrentRequest = Next;
			continue;
		
		}

		
		Prev = &(CurrentRequest->Next);	
		CurrentRequest = CurrentRequest->Next;
	}
	
	LeaveFunction("Userspace");
	return 0;
}

void StopUserspace(const int CPUNR)
{
	struct http_request *CurrentRequest,*Next;
	
	EnterFunction("StopUserspace");
	CurrentRequest = threadinfo[CPUNR].UserspaceQueue;

	while (CurrentRequest!=NULL)
	{
		Next= CurrentRequest->Next;
		CleanUpRequest(CurrentRequest);
		CurrentRequest=Next;		
	}
	threadinfo[CPUNR].UserspaceQueue = NULL;
	
	LeaveFunction("StopUserspace");
}


/* 
   "FindUserspace" returns the struct sock of the userspace-daemon, so that we can
   "drop" our request in the accept-queue 
*/

static struct sock *FindUserspace(const unsigned short Port)
{
	struct sock *sk;

	EnterFunction("FindUserspace");

	local_bh_disable();
	sk = tcp_v4_lookup_listener(INADDR_ANY,Port,0);
	local_bh_enable();
	return sk;
}

static void dummy_destructor(struct open_request *req)
{
}

static struct or_calltable Dummy = 
{
	0,
 	NULL,
 	NULL,
 	&dummy_destructor,
 	NULL
};

static int AddSocketToAcceptQueue(struct socket *sock,const int Port)
{
	struct open_request *req;
	struct sock *sk, *nsk;
	
	EnterFunction("AddSocketToAcceptQueue");

	
	sk = FindUserspace((unsigned short)Port);	
	
	if (sk==NULL)   /* No userspace-daemon found */
	{
		return -1;
	}
	
	lock_sock(sk);

	if (sk->state != TCP_LISTEN || tcp_acceptq_is_full(sk))
	{
		release_sock(sk);
		sock_put(sk);
		return -1;
	}

	req = tcp_openreq_alloc();
	
	if (req==NULL)
	{	
		release_sock(sk);
		sock_put(sk);
		return -1;
	}
	
	nsk = sock->sk;
	sock->sk = NULL;
	sock->state = SS_UNCONNECTED;

	req->class	= &Dummy;
	write_lock_bh(&nsk->callback_lock);
	nsk->socket = NULL;
        nsk->sleep  = NULL;
	write_unlock_bh(&nsk->callback_lock);

	tcp_acceptq_queue(sk, req, nsk);	

	sk->data_ready(sk, 0);

	release_sock(sk);
	sock_put(sk);

	LeaveFunction("AddSocketToAcceptQueue");
		
	return +1;	
	
	
	
}

void InitUserspace(const int CPUNR)
{
}


