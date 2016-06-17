/*

kHTTPd -- the next generation

Basic socket functions

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

#include "prototypes.h"
#include <linux/kernel.h>
#include <linux/net.h>
#include <linux/version.h>
#include <linux/smp_lock.h>
#include <net/sock.h>


/*

MainSocket is shared by all threads, therefore it has to be
a global variable.

*/
struct socket *MainSocket=NULL;


int StartListening(const int Port)
{
	struct socket *sock;
	struct sockaddr_in sin;
	int error;
	
	EnterFunction("StartListening");
	
	/* First create a socket */
	
	error = sock_create(PF_INET,SOCK_STREAM,IPPROTO_TCP,&sock);
	if (error<0) 
	     (void)printk(KERN_ERR "Error during creation of socket; terminating\n");



	/* Now bind the socket */
	
	sin.sin_family	     = AF_INET;
	sin.sin_addr.s_addr  = INADDR_ANY;
	sin.sin_port         = htons((unsigned short)Port);
	
	error = sock->ops->bind(sock,(struct sockaddr*)&sin,sizeof(sin));
	if (error<0)
	{
		(void)printk(KERN_ERR "kHTTPd: Error binding socket. This means that some other \n");
		(void)printk(KERN_ERR "        daemon is (or was a short time ago) using port %i.\n",Port);
		return 0;	
	}

	/* Grrr... setsockopt() does this. */
	sock->sk->reuse   = 1;

	/* Now, start listening on the socket */
	
	/* I have no idea what a sane backlog-value is. 48 works so far. */
	
	error=sock->ops->listen(sock,48);	
	if (error!=0)
		(void)printk(KERN_ERR "kHTTPd: Error listening on socket \n");
	
	MainSocket = sock;
	
	LeaveFunction("StartListening");
	return 1; 
}	

void StopListening(void)
{
	struct socket *sock;
	
	EnterFunction("StopListening");
	if (MainSocket==NULL) return;
	
	sock=MainSocket;
	MainSocket = NULL;
	sock_release(sock);

	LeaveFunction("StopListening");
}
