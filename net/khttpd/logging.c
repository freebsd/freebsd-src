/*

kHTTPd -- the next generation

logging.c takes care of shutting down a connection.

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
#include <linux/skbuff.h>
#include <linux/smp_lock.h>
#include <net/tcp.h>
#include <asm/uaccess.h>
#include "structure.h"
#include "prototypes.h"

/*

Purpose:

Logging() terminates "finished" connections and will eventually log them to a 
userspace daemon.

Return value:
	The number of requests that changed status, thus the number of connections
	that shut down.
*/


int Logging(const int CPUNR)
{
	struct http_request *CurrentRequest,*Req;
	int count = 0;
	
	EnterFunction("Logging");
	
	CurrentRequest = threadinfo[CPUNR].LoggingQueue;
	
	/* For now, all requests are removed immediatly, but this changes
	   when userspace-logging is added. */
	   
	while (CurrentRequest!=NULL)
	{

		Req = CurrentRequest->Next;

		CleanUpRequest(CurrentRequest);
		
		threadinfo[CPUNR].LoggingQueue = Req;
			
		CurrentRequest = Req;
	
		count++;
		
	}
	
	LeaveFunction("Logging");
	return count;
}



void StopLogging(const int CPUNR)
{
	struct http_request *CurrentRequest,*Next;
	
	EnterFunction("StopLogging");
	CurrentRequest = threadinfo[CPUNR].LoggingQueue;
	
	while (CurrentRequest!=NULL)
	{
		Next=CurrentRequest->Next;
		CleanUpRequest(CurrentRequest);
		CurrentRequest=Next;		
	}
	
	threadinfo[CPUNR].LoggingQueue = NULL;
	LeaveFunction("StopLogging");
}
